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
 * $Id: diff.cc 1.20.1.19.1.3.1.6.1.22 Sun, 17 Sep 2000 17:56:26 -0700 jmacd $
 */

#include "prcs.h"
#include "hash.h"
#include "misc.h"
#include "repository.h"
#include "projdesc.h"
#include "checkin.h"
#include "checkout.h"
#include "vc.h"
#include "fileent.h"
#include "diff.h"
#include "syscmd.h"
#include "setkeys.h"
#include "system.h"
#include "memseg.h"
#include "rebuild.h"


static PrPrcsExitStatusError diff_working_files();
static PrPrcsExitStatusError diff_named_versions();
static PrBoolError diff_similar_files(ProjectDescriptor* toP, ProjectDescriptor* fromP);
static PrBoolError diff_missing_files(ProjectDescriptor* toP, ProjectDescriptor* fromP);
static PrBoolError diff_symlink_pair(FileEntry*, FileEntry*);
static PrBoolError diff_old_new_file(FileEntry*, ProjectDescriptor* fromP, bool new_file);
static PrVoidError prepare_diff_workingfile(FileEntry *fe);
static PrIntError try_optimizations(FileEntry *tofe, FileEntry *fromfe);
static PrVoidError prepare_diff_versionfile(FileEntry *fe, bool use_working);
static PrBoolError diff_project_files(ProjectDescriptor* toP, ProjectDescriptor* fromP);
static PrBoolError diff_two_files(const char* label1, const char* label2,
				  const char* file1, const char* file2,
				  const char* index, const char* from_project,
				  SystemCommand *diff_command);
static PrVoidError get_info(FileEntry* fe, RepEntry* rep_entry);

static bool use_working_to_file = true;
/* Determined by scanning the diff options, given by -q or --brief,
 * and tells PRCS to fake the diff when it can. */
static bool diff_option_brief = false;

#define temp_file_from temp_file_1
#define temp_file_to temp_file_2

PrPrcsExitStatusError diff_command()
{
    kill_prefix(prcsoutput);

    /* This is sort of ugly. */
    for(int i = 0; i < cmd_diff_options_count; i += 1)
	if(strcmp("-q", cmd_diff_options_given[i]) == 0 ||
	   strcmp("--brief", cmd_diff_options_given[i]) == 0) {
	    diff_option_brief = true;
	}

    if(cmd_alt_version_specifier_major == NULL)
	return diff_working_files();
    else
	return diff_named_versions();
}

/* reports the differences from VERSION to WORKING files */
static PrPrcsExitStatusError diff_working_files()
{
    ProjectDescriptor *from, *to;
    ProjectVersionData *from_version;
    bool diffs = false, diff = false;

    use_working_to_file = true;

    if(!fs_file_readable(cmd_root_project_file_path))
	pthrow prcserror << "Can't open file " << squote(cmd_root_project_file_path) << perror;

    /* Set up the project file diff */
    const char* abs_path;

    Return_if_fail(abs_path << absolute_path(cmd_root_project_file_path));

    If_fail(Err_symlink(abs_path, temp_file_to))
	pthrow prcserror << "Failed creating symlink " << squote(temp_file_to) << perror;

    Return_if_fail(to << read_project_file(cmd_root_project_full_name, temp_file_to, true, KeepNothing));

    Return_if_fail(to->init_repository_entry(cmd_root_project_name, false, false));

    eliminate_unnamed_files(to);

    Return_if_fail(from_version << resolve_version(cmd_version_specifier_major,
						   cmd_version_specifier_minor,
						   cmd_root_project_full_name,
						   cmd_root_project_file_path,
						   to,
						   to->repository_entry()));

    if(from_version->prcs_minor_int() == 0) {
	Return_if_fail(from << checkout_create_empty_prj_file(temp_file_from,
							      cmd_root_project_full_name,
							      from_version->prcs_major(),
							      KeepNothing));
    } else {
	Return_if_fail(from << to->repository_entry() ->
		       checkout_create_prj_file(temp_file_from,
						cmd_root_project_full_name,
						from_version->rcs_version(),
						KeepNothing));
    }

    eliminate_unnamed_files(from);

    from->repository_entry(to->repository_entry());

    prcsinfo << "Producing diffs from " << from->full_version() << " to "
	     << to->full_version() << dotendl;

    Return_if_fail(eliminate_working_files(to, NoQueryUserRemoveFromCommandLine));

    to->read_quick_elim();

    to->quick_elim_unmodified();

    if(cmd_prj_given_as_file)
	Return_if_fail(diff << diff_project_files(to, from));

    diffs |= diff;

    Return_if_fail(diff << diff_similar_files(to, from));

    diffs |= diff;

    Return_if_fail(diff << diff_missing_files(to, from));

    diffs |= diff;

    Return_if_fail(warn_unused_files(false));

    if(diffs)
	return ExitDiffs;
    else
	return ExitNoDiffs;
}

/* reports the differences from VERSION1 to VERSION2 */
static PrPrcsExitStatusError diff_named_versions()
{
    ProjectDescriptor *from, *to;
    ProjectVersionData *to_version, *from_version;
    RepEntry *rep_entry;
    bool diffs = false, diff = false;

    use_working_to_file = false;

    Return_if_fail(rep_entry << Rep_init_repository_entry(cmd_root_project_name,
							  false, false, true));

    /* Get the from version */
    Return_if_fail(from_version << resolve_version(cmd_version_specifier_major,
						   cmd_version_specifier_minor,
						   cmd_root_project_full_name,
						   cmd_root_project_file_path,
						   NULL,
						   rep_entry));

    if(from_version->prcs_minor_int() == 0) {
	Return_if_fail(from << checkout_create_empty_prj_file(temp_file_from,
							      cmd_root_project_full_name,
							      from_version->prcs_major(),
							      KeepNothing));
    } else {
	Return_if_fail(from << rep_entry ->
		       checkout_create_prj_file(temp_file_from,
						cmd_root_project_full_name,
						from_version->rcs_version(),
						KeepNothing));
    }

    eliminate_unnamed_files(from);

    /* Get the to version */
    Return_if_fail(to_version << resolve_version(cmd_alt_version_specifier_major,
						 cmd_alt_version_specifier_minor,
						 cmd_root_project_full_name,
						 cmd_root_project_file_path,
						 NULL,
						 rep_entry));

    if(to_version->prcs_minor_int() == 0) {
	Return_if_fail(to << checkout_create_empty_prj_file(temp_file_to,
							    cmd_root_project_full_name,
							    to_version->prcs_major(),
							    KeepNothing));
    } else {
	Return_if_fail(to << rep_entry ->
		       checkout_create_prj_file(temp_file_to,
						cmd_root_project_full_name,
						to_version->rcs_version(),
						KeepNothing));
    }

    eliminate_unnamed_files(to);

    to->repository_entry(rep_entry);
    from->repository_entry(rep_entry);

    /* Diff */

    prcsinfo << "Producing diffs from " << from->full_version() << " to "
	     << to->full_version() << dotendl;

    if(cmd_prj_given_as_file)
	Return_if_fail(diff << diff_project_files(to, from));

    diffs |= diff;

    Return_if_fail(diff << diff_similar_files(to, from));

    diffs |= diff;

    Return_if_fail(diff << diff_missing_files(to, from));

    diffs |= diff;

    Return_if_fail(warn_unused_files(false));

    if(diffs)
	return ExitDiffs;
    else
	return ExitNoDiffs;
}

static PrVoidError get_info(FileEntry* fe, RepEntry* rep_entry)
{
    if(use_working_to_file)
	return fe->get_file_sys_info();
    else
	return fe->get_repository_info(rep_entry);
}

/*
 * prepare_diff_workingfile --
 *
 *     places either a copy of a keywords unset working file or a symlink
 *     to a working file into temp_file_to of the given file entry.
 */
static PrVoidError prepare_diff_workingfile(FileEntry *fe)
{
    If_fail(Err_unlink(temp_file_to)) {
	pthrow prcserror << "Unlink " << squote(temp_file_to) << " failed" << perror;
    }

    if (fe->file_type() == RealFile) {

	Return_if_fail(fe->get_file_sys_info());

	if (fe->keyword_sub() && !option_diff_keywords) {
	    Return_if_fail (setkeys(fe->working_path(),
				    temp_file_to,
				    fe,
				    Unsetkeys));
	} else {
	    const char* abs_path;

	    Return_if_fail(abs_path << absolute_path(fe->working_path()));

	    If_fail (Err_symlink(abs_path, temp_file_to)) {
		pthrow prcserror << "Error creating symbolic link"
				<< squote(temp_file_to) << perror;
	    }
	}
    }

    return NoError;
}

#define DIFFS   3 /* first bit says that optimizations succeeded and to skip this file */
#define NODIFFS 1 /* second bit says whether there were differences */

static PrIntError try_optimizations(FileEntry *tofe, FileEntry *fromfe)
{
    static MemorySegment setkey_buf(false);
    RcsVersionData *to_version_data, *from_version_data;
    bool need_to_replace_to_keywords = option_diff_keywords && tofe->keyword_sub();
    bool need_to_replace_from_keywords = option_diff_keywords && fromfe->keyword_sub();
    bool same_descriptor;

    if(!tofe->descriptor_name()) /* empty descriptor for working file */
	return 0;

    same_descriptor = (strcmp(tofe->descriptor_name(),
			      fromfe->descriptor_name()) == 0 &&
		       strcmp(tofe->descriptor_version_number(),
			      fromfe->descriptor_version_number()) == 0);

    if(!use_working_to_file && !need_to_replace_to_keywords &&
       !need_to_replace_from_keywords && same_descriptor)
	return NODIFFS;

    Return_if_fail(to_version_data << tofe->project()->repository_entry() ->
		   lookup_rcs_file_data(tofe->descriptor_name(),
					tofe->descriptor_version_number()));

    Return_if_fail(from_version_data << fromfe->project()->repository_entry() ->
		   lookup_rcs_file_data(fromfe->descriptor_name(),
					fromfe->descriptor_version_number()));

    /* If brief diff, compare checksums if possible */
    if(diff_option_brief && !use_working_to_file &&
       !need_to_replace_to_keywords && !need_to_replace_from_keywords) {

	if(memcmp(to_version_data->unkeyed_checksum(),
		  from_version_data->unkeyed_checksum(), 16) == 0) {
	    return NODIFFS;
	} else {
	    prcsoutput << "The file " << diff_tuple(fromfe, tofe) << " differs" << prcsendl;
	    return DIFFS;
	}
    }

    if(use_working_to_file && same_descriptor && !need_to_replace_to_keywords) {
	/* If the working file is unmodifed by quick_elim */
	if(tofe->unmodified())
	    return NODIFFS;
    }

    if(use_working_to_file && !need_to_replace_to_keywords &&
       !need_to_replace_from_keywords) {

	if(tofe->keyword_sub()) {
	    Return_if_fail(setkeys_outbuf(tofe->working_path(),
					  &setkey_buf,
					  tofe,
					  Unsetkeys));
	} else {
	    Return_if_fail(setkey_buf.map_file(tofe->working_path()));
	}

	if(setkey_buf.length() == from_version_data->length()) {
	    const char* checksum;

	    checksum = md5_buffer(setkey_buf.segment(), setkey_buf.length());

	    if(memcmp(checksum, from_version_data->unkeyed_checksum(), 16) == 0)
		return NODIFFS;
	}

	/* Checksums differ, if brief diff */
	if(diff_option_brief) {
	    prcsoutput << "The file " << diff_tuple(fromfe, tofe) << " differs" << prcsendl;
	    return DIFFS;
	}
    }

    return 0;
}

/*
 * prepare_diff_versionfile --
 *
 *     writes the file onto either temp_file_from or temp_file_to and
 *     maybe replaces keywords
 */
static PrVoidError prepare_diff_versionfile(FileEntry *fe, bool use_temp_file_from)
{
    const char* filename;

    if(use_temp_file_from)
	filename = temp_file_from;
    else
	filename = temp_file_to;

    Return_if_fail(fe->initialize_descriptor(fe->project()->repository_entry(), false, false));

    Return_if_fail(fe->get_repository_info(fe->project()->repository_entry()));

    if (fe->keyword_sub() && option_diff_keywords) {
	FILE* cofile;

	Return_if_fail(cofile << VC_checkout_stream(fe->descriptor_version_number(),
						    fe->full_descriptor_name()));

	// Blah

	Dstring ds;
	fe->describe (ds);

	RcsVersionData *fe_data;
	Return_if_fail(fe_data << fe->project()->repository_entry() ->
		       lookup_rcs_file_data(fe->descriptor_name(),
					    fe->descriptor_version_number()));

	Return_if_fail(setkeys_infile(ds.cast (), fileno(cofile), fe_data->length (), filename, fe, Setkeys));

	Return_if_fail(VC_close_checkout_stream(cofile,
						fe->descriptor_version_number(),
						fe->full_descriptor_name()));

    } else {
	Return_if_fail(VC_checkout_file(filename,
					fe->descriptor_version_number(),
					fe->full_descriptor_name()));
    }

    return NoError;
}

static PrBoolError diff_similar_files(ProjectDescriptor* toP,
				      ProjectDescriptor* fromP)
{
    FileEntry *tofe, *fromfe;
    bool alldiffs = false, onediff = false;

    foreach_fileent(fe_ptr, toP) {
	tofe = *fe_ptr;

	if(!tofe->on_command_line())
	    continue;

	toP->repository_entry()->Rep_clear_compressed_cache();
	fromP->repository_entry()->Rep_clear_compressed_cache();

	Return_if_fail (fromfe << fromP->match_fileent(tofe));

	if(fromfe) {

	    if(fromfe->file_type() == tofe->file_type()) {
		if(fromfe->file_type() == RealFile) {
		    int no_diffs;

		    /* If they are the same by checksum */
		    Return_if_fail(no_diffs << try_optimizations(tofe, fromfe));

		    if(no_diffs & 1) {
			if(no_diffs & 2) {
			    alldiffs = true;
			    DEBUG("Opt succeeds for " << diff_tuple(tofe, fromfe) << ", diffs");
			} else {
			    DEBUG("Opt succeeds for " << diff_tuple(tofe, fromfe) << ", no diffs");
			}

			continue;
		    }

		    if(use_working_to_file) {
			Return_if_fail(prepare_diff_workingfile(tofe));
		    } else {
			Return_if_fail(prepare_diff_versionfile(tofe, false));
		    }

		    Return_if_fail(prepare_diff_versionfile(fromfe, true));

		    Return_if_fail(onediff << diff_pair(fromfe,
							tofe,
							temp_file_from,
							temp_file_to));

		} else if(fromfe->file_type() == SymLink) {
		    Return_if_fail(tofe->initialize_descriptor(toP->repository_entry(),
							       false, false));

		    Return_if_fail(fromfe->initialize_descriptor(fromP->repository_entry(),
								 false, false));

		    Return_if_fail(onediff << diff_symlink_pair(tofe, fromfe));
		}
		/* directories ignored */
	    } else {
		prcsoutput << "Changes type from type " << format_type(fromfe->file_type())
			   << " to " << format_type(tofe->file_type())
			   << ": " << diff_tuple(fromfe, tofe) << prcsendl;

		onediff = true;
	    }

	    alldiffs |= onediff;
	}
    }

    return alldiffs;
}

static PrBoolError diff_missing_files(ProjectDescriptor* toP,
				      ProjectDescriptor* fromP)
{
    bool alldiffs = false;
    FileEntry *fe;

    foreach_fileent(fe_ptr, toP) {
	fe = *fe_ptr;

	toP->repository_entry()->Rep_clear_compressed_cache();
	fromP->repository_entry()->Rep_clear_compressed_cache();

	if(!fe->real_on_cmd_line())
	    continue;

	FileEntry *bogus;

	Return_if_fail (bogus << fromP->match_fileent(fe));

	if(!bogus) {
	    Return_if_fail(fe->initialize_descriptor(toP->repository_entry(),
						     false, false));

	    Return_if_fail(get_info(fe, toP->repository_entry()));

	    if(option_diff_new_file && fe->file_type() == RealFile && !diff_option_brief) {
		if(use_working_to_file)
		    Return_if_fail(prepare_diff_workingfile(fe));
		else
		    Return_if_fail(prepare_diff_versionfile(fe, false));

		diff_old_new_file (fe, fromP, true);
	    } else {
		prcsoutput << "Only in " << toP->full_version() << ": " << fe->working_path() << prcsendl;
	    }
	    alldiffs = true;
	}
    }

    foreach_fileent(fe_ptr2, fromP) {
	fe = *fe_ptr2;

	if(!fe->real_on_cmd_line())
	    continue;

	FileEntry *bogus;

	Return_if_fail (bogus << toP->match_fileent(fe));

	if(!bogus) {
	    if(option_diff_new_file && fe->file_type() == RealFile && !diff_option_brief) {
		Return_if_fail(prepare_diff_versionfile(fe, false));
		diff_old_new_file (fe, toP, false);
	    } else {
		prcsoutput << "Only in " << fromP->full_version() << ": " << fe->working_path() << prcsendl;
	    }
	    alldiffs = true;
	}
    }

    return alldiffs;
}

static PrBoolError diff_symlink_pair(FileEntry* to, FileEntry* from)
{
    const char *link1, *link2;

    link1 = from->link_name();

    if (use_working_to_file) {
        Return_if_fail (link2 << read_sym_link (from->working_path()));
    } else {
        link2 = to->link_name();
    }

    if(!pathname_equal(link1, link2)) {
	prcsoutput << "Symlink " << diff_tuple(from, to)
		   << " changes from " << squote(link1)
		   << " to " << squote(link2) << prcsendl;
	return true;
    } else {
	return false;
    }
}

static PrBoolError diff_old_new_file(FileEntry *to, ProjectDescriptor* fromP, bool new_file)
{
    Dstring label1, label2;
    Dstring filedes;

    if(!to->descriptor_name()) {
	filedes.assign("()");
    } else {
	filedes.sprintf("(%s/%s %s %03o)",
			to->project()->project_name(),
			to->descriptor_name(),
			to->descriptor_version_number(),
			to->file_mode());
    }

    label1.sprintf("-L%s/%s %s %s ()",
		   fromP->full_version(),
		   to->working_path(),
		   get_utc_time(),
		   get_login());

    label2.sprintf("-L%s/%s %s %s %s",
		   to->project()->full_version(),
		   to->working_path(),
		   to->last_mod_date(),
		   to->last_mod_user(),
		   filedes.cast());

    SystemCommand *cmd = NULL;

    if (to->file_attrs()->diff_tool())
	cmd = sys_cmd_by_name (to->file_attrs()->diff_tool());

    if (new_file)
	return diff_two_files(label1, label2, "/dev/null", temp_file_to, to->working_path(), fromP->full_version(), cmd);
    else
	return diff_two_files(label2, label1, temp_file_to, "/dev/null", to->working_path(), fromP->full_version(), cmd);
}

void format_diff_label(FileEntry* fe, const char* name, Dstring* ds)
{
    Dstring desc;

    const char* date, *user;

    if(fe == NULL) {
	date = get_utc_time();
	user = get_login();
    } else {
	date = fe->last_mod_date();
	user = fe->last_mod_user();
    }

    if(!fe->descriptor_name()) {
	desc.assign("()");
    } else {
	desc.sprintf("(%s/%s %s %03o)",
		     fe->project()->project_name(),
		     fe->descriptor_name(),
		     fe->descriptor_version_number(),
		     fe->file_mode());
    }

    ds->sprintf("-L%s/%s %s %s %s", fe->project()->full_version(), name, date, user, desc.cast());
}

PrBoolError diff_pair(FileEntry* from,
		      FileEntry* to,
		      const char* from_file,
		      const char* to_file)
{
    Dstring label1, label2;

    format_diff_label(from, from->working_path(), &label1);
    format_diff_label(to,   to->working_path(), &label2);

    SystemCommand *cmd = NULL;

    if (to->file_attrs()->diff_tool())
	cmd = sys_cmd_by_name (to->file_attrs()->diff_tool());

    if (!cmd && from->file_attrs()->diff_tool())
	cmd = sys_cmd_by_name (from->file_attrs()->diff_tool());

    return diff_two_files(label1, label2, from_file, to_file, to->working_path(), from->project()->full_version(), cmd);
}

static PrBoolError diff_two_files(const char* label1, const char* label2,
				  const char* file1, const char* file2,
				  const char* index, const char* version,
				  SystemCommand *cmd)
{
    ArgList *args;
    char buf[512];
    FILE* output;
    bool n_index_written = true;
    int c, ret;

    if (cmd)
	Return_if_fail(args << cmd->new_arg_list());
    else
	Return_if_fail(args << gdiff_command.new_arg_list());

    for(int i = 0; i < cmd_diff_options_count; i += 1)
	args->append(cmd_diff_options_given[i]);

   if (!cmd) {
	args->append("-a");
	args->append(label1);
	args->append(file1);
	args->append(label2);
	args->append(file2);

	Return_if_fail(gdiff_command.open(true, false));

	output = gdiff_command.standard_out();
    } else {
	args->append(label1 + 2);
	args->append(file1);
	args->append(label2 + 2);
	args->append(file2);

	Return_if_fail(cmd->open(true, false));

	output = cmd->standard_out();
    }

    for (;;) {
        If_fail (c << Err_fread(buf, 512, output))
	  pthrow prcserror << "Error reading from diff output pipe" << perror;

	if (c == 0)
	  break;

	if(n_index_written) {
	    /* this sorta depends on diffs output, a leading Files
	     * means its in --brief mode, lets filter the filenames */
	    if(strncmp(buf, "Files ", 6) == 0) {
		prcsoutput << "The file " << squote(index) << " differs" << prcsendl;
		break;
	    }
	    prcsoutput << "Index: " << version << "/" << index << prcsendl;
	    n_index_written = false;
	}
	If_fail (Err_fwrite(buf, c, stdout))
	  pthrow prcserror << "Error writing diff output" << perror;
    }

    if (cmd)
	Return_if_fail(ret << cmd->close());
    else
	Return_if_fail(ret << gdiff_command.close());

    if(ret > 1)
	pthrow prcserror << "Diff command exited abnormally on files "
			 << squote(file1) << " and " << squote(file2) << dotendl;

    return (bool)ret;
}

static PrBoolError diff_project_files(ProjectDescriptor* toP, ProjectDescriptor* fromP)
{
    Dstring label1, label2;

    label1.sprintf("-L%s/%s", fromP->full_version(), fromP->project_file_path());
    label2.sprintf("-L%s/%s", toP->full_version(), toP->project_file_path());

    return diff_two_files(label1,
			  label2,
			  temp_file_from,
			  temp_file_to,
			  toP->project_file_path(),
			  fromP->full_version(),
			  NULL /* @@@ */);
}
