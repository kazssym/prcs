/* -*-Mode: C++;-*-
 * PRCS - The Project Revision Control System
 * Copyright (C) 1997  Josh MacDonald
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * $Id$
 */

#include "prcs.h"

#include "setkeys.h"
#include "projdesc.h"
#include "fileent.h"
#include "repository.h"
#include "vc.h"
#include "checkout.h"
#include "checkin.h"
#include "misc.h"
#include "memseg.h"
#include "rebuild.h"
#include "system.h"
#include "quick.h"
#include "syscmd.h"
#include "diff.h"

static const char rlog_header[] = "checked in by PRCS version %d %d %d\n";

static PrVoidError write_new_prjfile(ProjectDescriptor* project, ProjectVersionData *parent_data);
static PrVoidError checkin_each_file(ProjectDescriptor*);
static PrVoidError maybe_query_user(ProjectDescriptor*);
static PrVoidError slow_eliminate_prepare(ProjectDescriptor*);
static PrVoidError checkin_cleanup_handler(void* data, bool);
static PrVoidError compute_modified(ProjectDescriptor  *project,
				    ProjectDescriptor  *pred_project);

PrPrcsExitStatusError checkin_command()
{
    ProjectVersionData *new_project_data;         /* New version data. */
    ProjectDescriptor  *project;                  /* The working project. */
    ProjectVersionData *pred_project_data = NULL; /* The predecessor project, if different */
    ProjectDescriptor  *pred_project = NULL;      /* from working Project-Version. */

    Return_if_fail(project << read_project_file(cmd_root_project_full_name,
						cmd_root_project_file_path,
						true,
						(ProjectReadData)(KeepMergeParents)));

    install_cleanup_handler(checkin_cleanup_handler, project);

    Return_if_fail(project->init_repository_entry(cmd_root_project_name, true, true));

    /* remove files which were not named on the command line */
    eliminate_unnamed_files(project);

    /* warn the user about filenames on the command line which didn't match anything */
    Return_if_fail(warn_unused_files(true));

    /* select the new version and possibly warn the user of unwise checkins */
    Return_if_fail(new_project_data << project->resolve_checkin_version(cmd_version_specifier_major,
									&pred_project_data));

    prcsinfo << "Checking in project " << squote(cmd_root_project_name)
	     << " version " << new_project_data << dotendl;

    /* check that no unselected files have empty descriptors and that all selected
     * files exist */
    Return_if_fail(eliminate_working_files(project, QueryUserRemoveFromCommandLine));
    Return_if_fail(check_project(project));

    /* if the auxiliary database is present, read it and eliminate files which
     * have not been touched since the last checkout or rekey */
    project->read_quick_elim();
    project->quick_elim_unmodified();

    if (pred_project_data)
	Return_if_fail (pred_project << project->repository_entry()->
			                   checkout_prj_file (project->project_full_name(),
							      pred_project_data->rcs_version(),
							      KeepNothing));

    /* now eliminate files which have not changed by reading and unkeying files,
     * comparing the unkeyed length, then computing their checksum and comparing
     * the unkeyed checksum.  with the unkeyed file in memory, prepare the repository
     * file (possibly uncompress), and write it out into the temporary location
     * in the repository. */

    Return_if_fail(slow_eliminate_prepare(project));

    Return_if_fail(compute_modified(project, pred_project));

    if(option_report_actions)
	return ExitSuccess;

    /* check if the user wants to enter a log. */
    Return_if_fail(maybe_query_user(project));

    project->repository_entry()->Rep_log() << "Checking in project version " << new_project_data << dotendl;

    Return_if_fail(checkin_each_file(project));

    foreach_fileent(fe_ptr, project) {
	FileEntry* fe = *fe_ptr;

	if(fe->real_on_cmd_line())
	    project->quick_elim_update_fe(fe);
    }

    project->quick_elim_update();

    Return_if_fail(project->update_attributes(new_project_data));

    Return_if_fail(write_new_prjfile(project, new_project_data));

    project->repository_entry()->Rep_log() << "Finished checking in" << dotendl;

    return ExitSuccess;
}

static PrVoidError compute_modified(ProjectDescriptor *project,
				    ProjectDescriptor *pred_project)
{
    /* This shares some structure with changes_command, not all of it though */
    bool project_modified = false;

    foreach_fileent(fe_ptr, project) {
	FileEntry *fe = *fe_ptr;

	const char *name = fe->working_path();
	const char *lname;
	bool        modified = false;
	bool        new_file = false;
	FileEntry  *pred_fe = NULL;

	if (pred_project)
	    Return_if_fail (pred_fe << pred_project->match_fileent (fe));

	if (pred_fe) {
	    pred_fe->set_on_command_line (false);
	} else {
	    project_modified = true;
	    new_file = true;

	    if (option_long_format)
		prcsoutput << format_type (fe->file_type(), true)
			   << " " << squote (name) << " was added" << dotendl;
	}

	if (pred_fe && fe->file_type() != pred_fe->file_type()) {
	    project_modified = true;

	    if (option_long_format)
		prcsoutput << "File " << squote (name) << " changes from type "
			   << format_type (pred_fe->file_type()) << " to "
			   << format_type (fe->file_type()) << dotendl;
	}

	if (pred_fe)
	  {
	    const char* a = pred_fe->working_path(), *b = fe->working_path();
	    if (!pathname_equal (a, b))
	      {
		project_modified = true;

		if (option_long_format)
		  prcsoutput << "File " << squote (name) << " is renamed from "
			     << squote (pred_fe->working_path()) << dotendl;
	      }
	  }

	if (!fe->on_command_line())
	    continue;

	switch (fe->file_type()) {
	case Directory:
	    break;
	case SymLink:
	    Return_if_fail (lname << read_sym_link (name));

	    fe->set_link_name (lname);

	    if (!pred_fe ||
		!pred_fe->link_name() ||
		strcmp (lname, pred_fe->link_name()) != 0)
		modified = true;

	    if (!new_file) {
		if (!modified && option_really_long_format)
		    prcsoutput << "Symlink " << squote(name) << " is unmodified" << dotendl;
		else if (modified && option_long_format)
		    prcsoutput << "Symlink " << squote(name) << " is modified" << dotendl;
	    }

	    break;
	case RealFile:
	    if (!fe->unmodified()) {
		modified = true;

		if (option_long_format && !new_file)
		    prcsoutput << "New version of file " << squote(name) << dotendl;
	    }

	    if (pred_fe &&
		fe->descriptor_name() &&
		fe->file_type() == pred_fe->file_type())
	      {
		if (strcmp (fe->descriptor_name(), pred_fe->descriptor_name()) != 0 ||
		    strcmp (fe->descriptor_version_number(),
			    pred_fe->descriptor_version_number()) != 0)
		  {
		    modified = true;
		    if (option_long_format)
		      prcsoutput << "File " << squote (name) << " changes versions" << dotendl;
		  }
	      }

	    if (fe->file_mode() != fe->stat_permissions()) {
		if (option_long_format && !new_file)
		    prcsoutput << "File " << squote(name) << " has new mode" << dotendl;

		modified = true;

		fe->set_file_mode (fe->stat_permissions());
	    }

	    if (!new_file && !modified && option_really_long_format && option_long_format)
		prcsoutput << "File " << squote(name) << " is unmodified" << dotendl;

	    break;
	}

	project_modified |= modified;
    }

    if (pred_project) {
	foreach_fileent (fe_ptr, pred_project) {
	    FileEntry *fe = *fe_ptr;

	    if (fe->on_command_line()) {
		project_modified = true;

		if (option_long_format) {
		    prcsoutput << "File " << squote(fe->working_path()) << " was deleted" << dotendl;
		}
	    }
	}
    }

    if(!project_modified) {
	prcsquery << "No modifications were found.  "
		  << force("Aborting checkin")
		  << report("Abort checkin")
		  << option('n', "Continue, with no modified files in this version")
		  << deffail('y')
		  << query("Abort");

	Return_if_fail(prcsquery.result());
    }

    return NoError;
}

PrVoidError ProjectDescriptor::update_attributes(ProjectVersionData *new_project_data)
{
    /* The Versions. */
    if (strcmp (*project_version_minor(), "0") != 0) {
	parent_version_major()->assign(*project_version_major());
	parent_version_minor()->assign(*project_version_minor());
    }
    project_version_major()->assign(new_project_data->prcs_major());
    project_version_minor()->assign(new_project_data->prcs_minor());

    /* the Checkin-Time attribute */
    checkin_time()->sprintf("%s", get_utc_time());

    /* the Logs */
    if (option_version_log) {
	if (new_version_log()->length() != 0) {
	    prcsquery << "You have supplied a version log on the command line, but the project file contains a new version log already.  "
		      << force("Overriding")
		      << report("Override")
		      << optfail('n')
		      << defopt('y', "Override the project file")
		      << query("Override");

	    Return_if_fail(prcsquery.result());
	}
	version_log()->assign(option_version_log_string);
    } else {
	version_log()->assign(*new_version_log());
    }

    new_version_log()->truncate(0);

    /* the Checkin-Login attribute */
    checkin_login()->assign(get_login());

    set_full_version (true);

    return NoError;
}

PrVoidError check_project(ProjectDescriptor *project)
{
    const char* name;

    foreach_fileent(fe_ptr, project) {
	FileEntry *fe = *fe_ptr;

        name = fe->working_path();

        if(!fe->on_command_line() && fe->empty_descriptor()) {
	    /* if the user is doing a partial operation, the file needs
	     * to have a non-empty internal descriptor, the descriptor
	     * is currently, empty, and init has asked to register new
	     * files for empty descriptors, complain. */

	    If_fail (fe->stat()) {
		pthrow prcserror << "The file " << squote(name)
				<< " has an empty descriptor and does not exist.  You may not "
		    "do a partial checkin excluding files with empty descriptors" << dotendl;
	    } else {
		static BangFlag bang;

		prcsquery << "The file " << squote(name)
			  << " has an empty descriptor and was excluded on the command line.  Partial checkins may not ignore files with empty descriptors.  "
			  << force("Continuing")
			  << report("Continue")
			  << allow_bang(bang)
			  << optfail('n')
			  << defopt('y', "Select this file, as if it were on the command line")
			  << query("Select this file");

		Return_if_fail(prcsquery.result());

		fe->set_on_command_line(true);
	    }
        } else if(fe->file_type() == RealFile && fe->descriptor_name()) {
	    RcsVersionData* data;

	    /* Check to see that the version exists, even if not on the
	     * command line */

	    If_fail(data << project->repository_entry()->
		    lookup_rcs_file_data(fe->descriptor_name(),
					 fe->descriptor_version_number()))
		pthrow prcserror << "The missing file may have been in a project version "
		           "which was deleted" << dotendl;

	}
    }

    return NoError;
}

#ifndef NDEBUG
static PrVoidError invalid()
{
    fs_copy_filename(temp_file_1, bug_name());

    prcserror << "Generated an invalid project file, aborting" << dotendl;

    return bug ();
}
#endif

static PrVoidError write_new_prjfile(ProjectDescriptor *P, ProjectVersionData *parent)
{
#ifndef NDEBUG
    ProjectDescriptor* copy;

    Return_if_fail(P->write_project_file(temp_file_1));

    If_fail(copy << read_project_file(P->project_full_name(), temp_file_1, false, KeepNothing))
	return invalid();

    foreach_fileent(fe_ptr, copy) {
	FileEntry* fe = *fe_ptr;

	if ( fe->empty_descriptor() )
	    /* Hopefully this will catch Gene's problem. */
	    return invalid();
    }

    /*delete copy;*/
#endif

    Return_if_fail(P->write_project_file(cmd_root_project_file_path));

    Return_if_fail(P->checkin_project_file(parent));

    return NoError;
}

static void checkin_hook (void* v_fe, const char* new_version)
{
    FileEntry *fe = (FileEntry*) v_fe;

    fe->set_version_number (new_version);
}

static PrVoidError checkin_each_file(ProjectDescriptor *project)
{
    /* inconsistencies have been checked the Files attribute has been
     * modified already, excepting the new RCS version numbers */

    foreach_fileent(fe_ptr, project) {
	FileEntry *fe = *fe_ptr;

	if(!fe->real_on_cmd_line() || fe->unmodified())
	    continue;

	Return_if_fail(VC_checkin(fe->temp_file_path(),
				  fe->descriptor_version_number(),
				  fe->full_descriptor_name(),
				  NULL,
				  fe,
				  checkin_hook));
    }

    Dstring rlog;

    rlog.sprintf (rlog_header, prcs_version_number[0],
		  prcs_version_number[1],
		  prcs_version_number[2]);

    Return_if_fail(VC_checkin(NULL, NULL, NULL, rlog, NULL, checkin_hook));

    /* @@@ Really should do some verification here.  It seems that the
     * above chmod can reach an NFS server before the new RCS versions
     * do... Dan Bonachea's problem. */

    foreach_fileent(fe_ptr, project) {
	FileEntry *fe = *fe_ptr;

	if(!fe->real_on_cmd_line() || fe->unmodified())
	    continue;

	RcsVersionData ver;

	Return_if_fail (VC_get_one_version_data (fe->full_descriptor_name (),
						 fe->descriptor_version_number (),
						 & ver));

	fe->set_lines (ver.plus_lines (), ver.minus_lines ());

	/* RCS 5.7 does not preserve the mode of an empty version file
	 * created by rcs -i, so we ignore the umask when registering
	 * new files and just chmod the first version. */
	if(strcmp(fe->descriptor_version_number(), "1.1") == 0) {
	    Return_if_fail (project->repository_entry()->
			    Rep_chown_file(fe->descriptor_name()));
	}
    }

    return NoError;
}

/*
 * eliminate_unnamed_files --
 *
 *     this marks files that were not given on the command line so
 *     that they can be ignored in certain stages of the checkin (and
 *     other) commands.  if no files were given, it leaves them all
 *     marked, otherwise, it unmarks them all and marks all files (and
 *     recursively marks directories).  */
void eliminate_unnamed_files(ProjectDescriptor* project)
{
    if(cmd_filenames_count < 1) {
        if (! option_exclude_project_file)
	    cmd_prj_given_as_file = true;
        return;
    }

    for(int i = 0; i < cmd_filenames_count; i += 1) {
        /* special case, the project file */
        if(strcmp(cmd_root_project_file_path, cmd_corrected_filenames_given[i]) == 0) {
            cmd_filenames_found[i] = true;
            cmd_prj_given_as_file = true;
        }
    }

    foreach_fileent(fe_ptr, project) {
	FileEntry *fe = *fe_ptr;

        fe->set_on_command_line(false);

        for(int j = 0; j < cmd_filenames_count; j += 1) {
            const char *f, *w;

            w = fe->working_path();
            f = cmd_corrected_filenames_given[j];

            while(*f && *w && *f == *w) {
                while(f[1] == '/') { f += 1; }
                while(w[1] == '/') { w += 1; }
                f += 1;
                w += 1;
            }

            if (*f == 0 && (w[-1] == '/' || *w == 0 || f[-1] == '/')) {
                cmd_filenames_found[j] = true;
                fe->set_on_command_line(true);
            }
        }
    }
}


/*
 * warn_unused_files --
 *
 *     this optionally prompts the user to continue and warns if any files
 *     were not matched by a call to eliminate_unnamed_files.
 *
 *     if prompt_abort is true, returns an error if the user does not
 *     confirm ignoring unmatched filenames.
 *
 *     if prompt_abort is false, it will return an error if files are unmatched.
 */
PrVoidError warn_unused_files(bool prompt_abort)
{
    bool any = false, ret = true, need_prompt = false;

    if(cmd_filenames_count < 1)
        return NoError;

    for(int j = 0; j < cmd_filenames_count; j += 1) {
        any |= cmd_filenames_found[j];
        if(!cmd_filenames_found[j]) {
            if(prompt_abort) {
		need_prompt = true;
                prcswarning << "File or directory " << squote(cmd_filenames_given[j])
			    << " on command line does not match any working files" << dotendl;
            } else {
                ret = false;
                prcswarning << "File or directory " << squote(cmd_filenames_given[j])
			    << " on command line did not match any working files" << dotendl;
            }
        }
    }

    if(!any)
	pthrow prcserror << "No files on command line match working files" << dotendl;

    if(need_prompt) {
	prcsquery << "Files on the command line did not match files in the project.  "
		  << force("Continuing")
		  << report("Continue")
		  << optfail('n')
		  << defopt('y', "Continue, ignoring these files")
		  << query("Continue");

	Return_if_fail(prcsquery.result());
    }

    if(ret)
	return NoError;
    else
	return PrVoidError(NonFatalError);
}

static PrVoidError maybe_query_user(ProjectDescriptor* P)
{
    int tty, c, lc, llc;

    if(P->new_version_log()->length() > 0 ||
       !get_environ_var("PRCS_LOGQUERY") || /* either it has been edited or PRCS_LOGQUERY is not set */
       option_report_actions ||
       option_force_resolution ||
       option_version_log)
        return NoError;

    tty = isatty(STDIN_FILENO);

    if(tty) {
	prcsquery << "Empty New-Version-Log.  "
		  << option('e', "Enter version log now")
		  << defopt('p', "Proceed with an empty version log")
		  << query("Enter a log, or proceed");

	char c;

	Return_if_fail(c << prcsquery.result());

	if(c == 'p')
	    return NoError;

        prcsoutput << "Enter description terminated by a single '.' or EOF" << dotendl;

	fprintf(stdout, ">> ");
    }

    re_query_message = ">> ";
    re_query_len = 3;

    lc = '\n';
    llc = '\n';

    while(true) {
	If_fail(c << Err_fgetc(stdin))
	    pthrow prcserror << "Read failure on stdin" << perror;

	if (c == EOF) {
	    fprintf (stdout, "\n");
	    break;
	}

        if(tty && c == '\n') {
            if(lc == '.' && llc == '\n') {
                P->new_version_log()->truncate(P->new_version_log()->length() - 1);
                break;
            }
            fprintf(stdout, ">> ");
        }

        llc = lc;
        lc = c;

        P->new_version_log()->append(c);
    }

    if(tty) fprintf(stdout, "Done.\n");

    /* I don't think its possible that PRCS will ever need to query the
     * user after this, but I might as well reopen it. */
    if(freopen(ctermid(NULL), "r", stdin) == NULL)
	pthrow prcserror << "Couldn't reopen the standard input" << perror;

    return NoError;
}

PrProjectVersionDataPtrError
ProjectDescriptor::resolve_checkin_version(const char          *maj,
					   ProjectVersionData **pred_project_data)
{
    ProjectVersionData *cur_project_data = new ProjectVersionData(-1);
    Dstring *new_major = new Dstring; /* Most of this stuff leaks. */
    Dstring *new_minor = new Dstring;
    int prev_highest_minor;

    /* Move New-Merge-Parents to Merge-Parents. */
    Return_if_fail(adjust_merge_parents());

    /* Find first parent version, if any. */
    if(strcmp(*project_version_minor(), "0") != 0) {
	/* Don't beleive what's in the project file, make sure its in the
	 * repository. */
	ProjectVersionData *first_parent_project_data;

	first_parent_project_data = repository_entry()->lookup_version(this);

	if(!first_parent_project_data)
	    pthrow prcserror << "Illegal version, " << full_version()
			    << ", in project file, it may have been deleted" << dotendl;

	cur_project_data->new_parent(first_parent_project_data->version_index());

	if (_merge_parents->length() > 0) {

	    /* See if the previous merge was complete. */
	    if (_merge_parents->last_index()->state & MergeStateIncomplete)
		/* Last merge is incomplete, ask to abort this one and finish last one. */
		Return_if_fail (merge_help_query_incomplete(_merge_parents->last_index()));

	    /* Add in any merge parents. */
	    foreach (ent_ptr, _merge_parents, MergeParentEntryPtrArray::ArrayIterator) {
		MergeParentEntry *ent = *ent_ptr;

		if (ent->state & MergeStateParent)
		    cur_project_data->new_parent(ent->project_data->version_index());
	    }
	}
    } else {
	/* We assume that the first parent is a the Parent-Version field, so we can't
	 * add Merge-Parents if we didn't add a Parent-Version. */
	if (_merge_parents->length() > 0)
	    pthrow prcserror << "Kind of strange to have merge parents when you're an empty "
		"version, isn't it?" << prcsendl;
    }

    /* Compute the new major version. */
    if(!maj[0]) {
	new_major->assign(*project_version_major());
    } else if(strcmp(maj, "@") == 0) {
	int highest_major;

	if(repository_entry()->version_count() == 0)
	    pthrow prcserror << "No versions in the repository, can't resolve major version '@'" << dotendl;

	highest_major = repository_entry()->highest_major_version();

	if(highest_major >= 0) {
	    new_major->assign_int(highest_major);
	} else
	    pthrow prcserror << "No numeric major versions in the repository, "
		"can't resolve major version '@'" << dotendl;
    } else {
	new_major->assign(maj);
    }

    /* Compute the new minor version. */
    prev_highest_minor = repository_entry()->highest_minor_version(*new_major);
    new_minor->assign_int(prev_highest_minor + 1);

    cur_project_data->prcs_major (*new_major);
    cur_project_data->prcs_minor (*new_minor);

    /* Warn if the minor version specified on cmd line differs. */
    if (cmd_version_specifier_minor &&
	strcmp (cmd_version_specifier_minor, "@") != 0 &&
	strcmp (cmd_version_specifier_minor, *new_minor) != 0) {

	prcsquery << "Minor version "
		  << squote(cmd_version_specifier_minor)
		  << " will be ignored, new minor is "
		  << new_minor << ".  "
		  << force("Continuing")
		  << report("Continue")
		  << optfail('n')
		  << defopt('y', "Continue and ignore supplied minor")
		  << query("Continue");

	Return_if_fail(prcsquery.result());
    }

    if(!repository_entry()->major_version_exists(*new_major)) {
	/* If the branch is new, no need to check if the checkin is okay. */

	if(repository_entry()->version_count() != 0) {
	    /* Other majors do exist, confirm creation. */
	    prcsquery << "No previous major version named "
		      << squote(new_major) << ".  "
		      << force("Creating")
		      << report("Create")
		      << optfail('n')
		      << defopt('y', "Create the new branch and continue")
		      << query("Create");

	    Return_if_fail(prcsquery.result());
	}

    } else {
	/* We've already got a ProjectVersionData entry for the working
	 * version, its nameless.  Now we check the common ancestor of
	 * it and the previous highest minor version on the branch we're
	 * checking into.  If this common ancestor is equal to the
	 * previous highest, then things are cool.  If they are not, then
	 * we warn the user and ask them what to do. */
	ProjectVersionDataPtrArray *ancestors;
	ProjectVersionData *new_data;

	new_data = repository_entry()->lookup_version (*new_major, prev_highest_minor);

	ASSERT (new_data, "the minor number was already verified.");

	ancestors = repository_entry()-> common_version(cur_project_data, new_data);

	bool ancestor_unique = ancestors->length() == 1;
	bool ancestor_diff = !ancestor_unique || ancestors->index(0) != new_data;

	if (! ancestor_unique) {
	    /* The common ancestor is troublesome.  Query now. */
	    if (ancestors->length() == 0) {
		prcsquery << "No common ancestor between new version and working version.  ";
	    } else {
		prcsquery << "Ambiguous common ancestor between new version and working version.  ";
	    }
	} else if (ancestor_diff) {
	    /* The common ancestor is unique, but there are versions
	     * in the way, so query. */

	    prcsquery << "Intervening versions have been checked in between the target version, "
		      << cur_project_data << ", and the working version's common ancestor, "
		      << ancestors->index(0) << ".  ";
	}

	if (!ancestor_unique || ancestor_diff) {
	    prcsquery << report ("Query Abort/Force Continue")
		      << force ("Continuing", 'n')
		      << option ('n', "Continue, ignoring warning")
		      << deffail ('y')
		      << query ("Abort");

	    Return_if_fail (prcsquery.result());
	}

	if (!new_data->deleted())
	    *pred_project_data = new_data;

	delete ancestors;
    }

    return cur_project_data;
}

void checkin_prj_hook (void* v_new_rcs_version, const char* new_version)
{
    ((Dstring*)v_new_rcs_version)->assign(new_version);
}

PrVoidError ProjectDescriptor::checkin_project_file(ProjectVersionData *new_project_data)
{
    struct stat buf;
    Dstring version_log, new_rcs_version;

    foreach_fileent(fe_ptr, this)
        (*fe_ptr)->update_repository(repository_entry());

    If_fail(Err_stat(project_file_path(), &buf))
	pthrow prcserror << "Stat failed on newly written project file " << project_file_path() << perror;

    new_project_data->date(get_utc_time_t());
    new_project_data->author(get_login());
    new_project_data->length(buf.st_size);

    format_version_log(new_project_data, version_log);

    Dstring tmp_name (repository_entry()->Rep_name_of_version_file());
    const char* abs_path;

    Return_if_fail (abs_path << absolute_path (project_file_path()));

    tmp_name.truncate (tmp_name.length() - 2);

    unlink (tmp_name);

    If_fail (Err_symlink (abs_path, tmp_name))
      pthrow prcserror << "Error creating symbolic link "
		      << squote(tmp_name) << perror;

    If_fail(VC_checkin(tmp_name,
		       "",
		       repository_entry()->Rep_name_of_version_file(),
		       NULL,
		       &new_rcs_version,
		       checkin_prj_hook))
	pthrow prcserror << "Failure checking in project file" << dotendl;

    If_fail(VC_checkin(NULL, NULL, NULL, version_log, NULL, checkin_prj_hook))
	pthrow prcserror << "Failure checking in project file" << dotendl;

    unlink (tmp_name);

    // @@@ Really should do some verification here to work around NFS bugs

    if (strcmp (new_rcs_version, "1.1") == 0) {
        Dstring desc_name (project_name());
	desc_name.append (".prj");

	/* RCS 5.7 does not preserve the mode of an empty version file
	 * created by rcs -i, so we ignore the umask when registering
	 * new files and just chmod the first version. */
	Return_if_fail(repository_entry()->Rep_chown_file(desc_name));
    }

    new_project_data->rcs_version(new_rcs_version);

    repository_entry()->add_project_data(new_project_data);

    Return_if_fail(repository_entry()->close_repository());

    return NoError;
}

void format_version_log(ProjectVersionData* project_data, Dstring& log)
{
    static const char prcs_version_format[] =
	"PRCS major version: %s\n"
	"PRCS minor version: %s\n";

    log.sprintf(rlog_header, prcs_version_number[0],
		prcs_version_number[1],
		prcs_version_number[2]);
    log.sprintfa(prcs_version_format,
		 project_data->prcs_major(),
		 project_data->prcs_minor());

    if (project_data->parent_count() > 0) {
	log.sprintfa("PRCS parent indices: %d", project_data->parent_index(0));

	if (project_data->parent_count() > 1) {
	    for (int i = 1; i < project_data->parent_count(); i += 1)
		log.sprintfa(":%d", project_data->parent_index(i));
	}

	log.sprintfa("\n");
    }

    if(project_data->deleted())
	log.append("PRCS version deleted\n");
}

void ProjectDescriptor::quick_elim_unmodified()
{
    foreach_fileent(fe_ptr, this) {
	FileEntry* fe = *fe_ptr;

	if(!fe->real_on_cmd_line() || !fe->descriptor_name())
	    continue;

	if(_quick_elim && _quick_elim->unchanged(fe)) {
	    DEBUG("Quick elim succeeds for file " << squote(fe->working_path()));
	    fe->set_unmodified(true);
	}
    }
}

static PrVoidError slow_eliminate_prepare(ProjectDescriptor *project)
{
    static MemorySegment file_buffer(false);
    static MemorySegment setkey_buffer(false);

    foreach_fileent(fe_ptr, project) {
	FileEntry      *fe = *fe_ptr;
	const char     *output_buffer;
	int             output_len;
	bool            setkeys;
	char*           checksum;

	if(!fe->real_on_cmd_line() || fe->unmodified())
	    continue;

	Return_if_fail(file_buffer.map_file(fe->working_path()));

	if(fe->keyword_sub()) {
	    If_fail( setkeys << setkeys_inoutbuf(file_buffer.segment(),
						 file_buffer.length(),
						 &setkey_buffer,
						 fe,
						 Unsetkeys))
		pthrow prcserror << "Keyword replacement failed on file "
			        << squote(fe->working_path()) << perror;

	    output_buffer = setkey_buffer.segment();
	    output_len = setkey_buffer.length();
	} else {
	    output_buffer = file_buffer.segment();
	    output_len = file_buffer.length();
	}

	checksum = md5_buffer(output_buffer, output_len);

	fe->set_checksum(checksum);
	fe->set_unkeyed_length(output_len);

	if(fe->descriptor_name()) {
	    /* If it doesn't have an empty descriptor. */
	    RcsVersionData *old_data;

	    Return_if_fail(old_data << project->repository_entry() ->
			   lookup_rcs_file_data(fe->descriptor_name(),
						fe->descriptor_version_number()));

	    if (output_len == old_data->length() &&
	        memcmp(checksum, old_data->unkeyed_checksum(), 16) == 0) {

		DEBUG("Slow elim succeeds for file " << squote(fe->working_path()));

		fe->set_unmodified (true);

		continue;
	    }
	}

	Return_if_fail(fe->initialize_descriptor(project->repository_entry(),
						 !option_report_actions,
						 true));
	/* Can't free the rep_comp_cache for these initializations, because
	 * of the batch nature of this checkin. If you ever change that, be
	 * sure to clean up. */

	if(!option_report_actions) {
	    unlink(fe->temp_file_path());

	    const char* fullpath;

	    if(fe->keyword_sub() && setkeys) {
		FILE* temp_out;

		fullpath = make_temp_file("");

		If_fail(temp_out << Err_fopen(fullpath, "w"))
		    pthrow prcserror << "Failed opening temp file for writing "
				    << squote(fullpath) << perror;

		If_fail (Err_fwrite(output_buffer, output_len, temp_out))
		    pthrow prcserror << "Failed writing temp file "
				    << squote(fullpath) << perror;

		If_fail (Err_fclose(temp_out))
		    pthrow prcserror << "Failed writing temp file "
				    << squote(fullpath) << perror;
	    } else {
		Return_if_fail(fullpath << name_in_cwd(fe->working_path()));
	    }

	    If_fail (Err_symlink(fullpath, fe->temp_file_path())) {
		pthrow prcserror << "Error creating symbolic link "
				<< squote(fe->temp_file_path()) << perror;
	    }

#if defined (PRCS_DEVEL) && ! defined(NDEBUG)
	    if (option_debug) {
		struct stat sbuf;
		/* The link broke once on scheme.xcf, while running as
		 * root with -j32 on a project consisting of the gcc-2.7.2.2
		 * source tree with all files zeroed. */

		If_fail (Err_stat (fe->temp_file_path(), &sbuf)) {
		    prcserror << "Newly created symlink " << squote(fe->temp_file_path())
			      << " doesn't stat" << dotendl;
		    abort();
		}
	    }
#endif
	}
    }

    if(!option_report_actions)
	Return_if_fail(VC_register(NULL));

    return NoError;
}

void FileEntry::update_repository(RepEntry* rep_entry) const
{
    if(real_on_cmd_line() && !unmodified()) {
	RcsVersionData ver;
	ver.date             (get_utc_time_t());
	ver.author           (get_login());
	ver.length           (unkeyed_length());
	ver.unkeyed_checksum (checksum());
	ver.rcs_version      (descriptor_version_number());
	ver.set_plus_lines   (plus_lines ());
	ver.set_minus_lines  (minus_lines ());

	rep_entry->add_rcs_file_data(descriptor_name(), &ver);
    }
}

static PrVoidError checkin_cleanup_handler(void* data, bool sig)
{
    if (!sig)
        return NoError;

    ProjectDescriptor* project = (ProjectDescriptor* ) data;

    foreach_fileent(fe_ptr, project) {
	FileEntry* fe = *fe_ptr;

	if(!fe->on_command_line())
	    continue;

	if(fe->temp_file_path() != NULL)
	    unlink(fe->temp_file_path());
    }

    return NoError;
}

static void kill_one_subdir(const char* string, Dstring& fill)
{
    int len = strip_leading_path(string) - string;

    if(len > 0)
	len -= 1;

    fill.assign(string, len);
}

static PrBoolError try_elim_subdir(FileEntry* fe, MissingFileAction action)
    /* returns whether to skip this file.  It is given that stat() or
     * lstat() failed on this file */
{
    static DstringPtrArray* eliminated_subdirs = NULL;
    struct stat buf;
    const char* name = fe->working_path();

    if(!eliminated_subdirs)
	eliminated_subdirs = new DstringPtrArray;

    if(!strchr(name, '/'))
	return false;

    Dstring this_subdir;
    Dstring last_subdir;

    kill_one_subdir(name, this_subdir);

    foreach(elim_ptr, eliminated_subdirs, DstringPtrArray::ArrayIterator) {
	Dstring* elim = *elim_ptr;

	if(strncmp(elim->cast(), name, elim->length()) == 0) {
	    switch (action) {
	    case QueryUserRemoveFromCommandLine:
		fe->set_on_command_line(false);
		break;
	    case NoQueryUserRemoveFromCommandLine:
		fe->set_on_command_line(false);
		break;
	    case SetNotPresentWarnUser:
		fe->set_present(false);
		break;
	    }

	    return true;
	}
    }

    if(stat(this_subdir, &buf) >= 0 || errno != ENOENT)
	return false;

    while(this_subdir.length() > 0) {
	last_subdir.assign(this_subdir);

	kill_one_subdir(this_subdir, this_subdir);

	const char* s = this_subdir;

	if(stat(s[0] == 0 ? "." : s, &buf) >= 0) {
	    static BangFlag bang;

	    switch (action) {
	    case QueryUserRemoveFromCommandLine:
		prcsquery << "The directory " << squote(last_subdir)
			  << " does not exist.  "
			  << force("Ignoring all entries")
			  << report("Ignore all entries")
			  << optfail('n')
			  << defopt('y', "Ignore all entries of this directory")
			  << allow_bang (bang)
			  << query("Ignore it and descendants");

		Return_if_fail(prcsquery.result());

		fe->set_on_command_line(false);
		break;
	    case NoQueryUserRemoveFromCommandLine:
		prcswarning << "The directory " << squote(last_subdir)
			    << " does not exist.  Ignoring all entries" << dotendl;

		fe->set_on_command_line(false);
		break;
	    case SetNotPresentWarnUser:
		prcswarning << "The directory " << squote(last_subdir)
			    << " does not exist.  Assuming all entries are unmodified" << dotendl;

		fe->set_present(false);
		break;
	    }

	    eliminated_subdirs->append(new Dstring(last_subdir));

	    return true;

	} else if(errno != ENOENT) {
	    return false;
	}
    }

    return false;
}

PrVoidError eliminate_working_files(ProjectDescriptor* project, MissingFileAction action)
{
    foreach_fileent(fe_ptr, project) {
	FileEntry* fe = *fe_ptr;

        if(fe->on_command_line()) {
	    static BangFlag bang;
	    bool file_present;

	    Return_if_fail(file_present << fe->check_working_file());

	    if(file_present)
		continue;

	    bool ignore_subdir;

	    Return_if_fail(ignore_subdir << try_elim_subdir(fe, action));

	    if(ignore_subdir)
		continue;

	    switch (action) {
	    case QueryUserRemoveFromCommandLine:
		prcsquery << "File " << squote(fe->working_path()) << " is unavailable.  "
			  << force("Continuing")
			  << report("Continue")
			  << allow_bang(bang)
			  << defopt('y', "Ignore this file, as if it were not on the command line")
			  << optfail('n')
			  << query("Ignore this file");

		Return_if_fail(prcsquery.result());
		fe->set_on_command_line(false);
		break;
 	    case NoQueryUserRemoveFromCommandLine:
		prcswarning << "File " << squote(fe->working_path())
			    << " is unavailable.  Continuing" << dotendl;
		fe->set_on_command_line(false);
		break;
	    case SetNotPresentWarnUser:
		prcswarning << "File " << squote(fe->working_path())
			    << " is unavailable.  Assuming unmodified" << dotendl;
		fe->set_present(false);
		break;
	    }
        }
    }

    return NoError;
}
