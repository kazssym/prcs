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
 * $Id: populate.cc 1.11.1.6.1.12.1.3.1.8.1.22 Wed, 24 Oct 2001 03:23:43 -0700 jmacd $
 */

#include "prcs.h"
#include "prcsdir.h"
#include "projdesc.h"
#include "hash.h"
#include "repository.h"
#include "misc.h"
#include "fileent.h"
#include "system.h"
#include "checkin.h"
#include "populate.h"

static int added = 0;
static int deleted = 0;

static void append_new_files(ProjectDescriptor* P,
			     FileRecordList *record_list,
			     bool delete_files);
static int hash_dir(const char*, InoTable*, PathTable*);
static int hash_file_or_dir(const char*, InoTable*, PathTable*);
static PrVoidError check_project_file_populate(ProjectDescriptor *,
					       PathTable *,
					       InoTable *);
static PrVoidError init_ignore(ProjectDescriptor* project);
static bool not_ignored(const char* name);

PrPrcsExitStatusError populate_command_filename(const char* filename, bool verbose);

static FileRecordList *new_records = NULL;
static FileRecordList *deleted_records = NULL;
static VoidPtrList *ignore = NULL;

PrPrcsExitStatusError populate_command()
{
    return populate_command_filename(cmd_root_project_file_path, true);
}

PrPrcsExitStatusError populate_command_filename(const char* filename, bool verbose)
{
    ProjectDescriptor *P;
    PathTable table(pathname_hash, pathname_equal),
	      descr(pathname_hash, pathname_equal);
    InoTable inodes;

    Return_if_fail(P << read_project_file(cmd_root_project_full_name,
					  cmd_root_project_file_path,
					  true,
					  KeepNothing));

    Return_if_fail(check_project_file_populate(P, &table, &inodes));

    Return_if_fail(init_ignore(P));

    if(cmd_filenames_count < 1)
	hash_dir(cmd_root_project_path[0] ? cmd_root_project_path : ".", &inodes, &table);
    else {
	/* the functionality here is the same as eliminate_unnamed_files
	 * from checkin.cc, but its not really eliminating, rather its
	 * marking in this case. */
	for(int i = 0; i < cmd_filenames_count; i += 1) {
	    if (weird_pathname(cmd_corrected_filenames_given[i] + cmd_root_project_path_len))
		pthrow prcserror << "Illegal file name "
				<< squote(cmd_corrected_filenames_given[i])
				<< "names may not contain "
				<< squote("../") << " or " << squote("./")
				<< "or end or begin with " << squote("/")
				<< dotendl;
	    else
		hash_file_or_dir(cmd_corrected_filenames_given[i], &inodes, &table);
	}
    }

    if(added == 0 && deleted == 0) {
	prcsinfo << "No new files" << dotendl;
	return ExitSuccess;
    }

    if(verbose && option_long_format)
	prcsinfo << "New files are: " << prcsendl;

    if(added > 0) {
	P->append_files_data ("\n;; Files added by populate at ");
	P->append_files_data (get_utc_time());
	P->append_files_data (",\n;; to version ");
	P->append_files_data (P->full_version());
	P->append_files_data (", by ");
	P->append_files_data (get_login());
	P->append_files_data (":\n");
	append_new_files(P, new_records, false);
	P->append_files_data ("\n");
    }

    if(deleted > 0) {
	P->append_files_data ("\n;; Files deleted by populate at ");
	P->append_files_data (get_utc_time());
	P->append_files_data (",\n;; from version ");
	P->append_files_data (P->full_version());
	P->append_files_data (", by ");
	P->append_files_data (get_login());
	P->append_files_data (":\n");
	append_new_files(P, deleted_records, true);
	P->append_files_data ("\n");
    }

    if(verbose) {
	if(added == 1) {
	    prcsinfo << "One file was added" << dotendl;
	} else if(added == 0) {
	    prcsinfo << "No new files" << dotendl;
	} else {
	    prcsinfo << added << " files were added" << dotendl;
	}
	if(option_populate_delete) {
	    if(deleted == 1) {
		prcsinfo << "One file was deleted" << dotendl;
	    } else if(deleted == 0) {
		prcsinfo << "No files deleted" << dotendl;
	    } else {
		prcsinfo << deleted << " files were deleted" << dotendl;
	    }
	}
    }

    if(option_report_actions)
	return ExitSuccess;

    Return_if_fail(P->write_project_file(filename));

    return ExitSuccess;
}

static int hash_file_or_dir(const char* name, InoTable* T, PathTable* P)
{
    struct stat statbuf;
    Dstring *n;
    FileRecord r;

    n = new Dstring(name);
    r.name = n;

    if ( lstat(name, &statbuf) < 0 ) {
	delete n;
	return 0;
    } else if ( S_ISREG(statbuf.st_mode) ) {
	/* first look in the table for this inode */
	FileType *lu(T->lookup(statbuf.st_ino));
	if ( lu == NULL && not_ignored(*n) ) { /* if not found then its new */
	    r.type = RealFile; /* set its type */
	    T->insert(statbuf.st_ino, RealFile); /* insert it */
	    P->insert(*n, RealFile);
	    new_records = new FileRecordList(r, new_records);
	    added += 1;
	}
    } else if ( S_ISLNK(statbuf.st_mode) ) {
	FileType *lu(P->lookup(*n));
	if ( lu == NULL && not_ignored(*n) ) {
	    r.type = SymLink;
	    P->insert(*n, SymLink);
	    new_records = new FileRecordList(r, new_records);
	    added += 1;
	}
    } else if ( S_ISDIR(statbuf.st_mode) ) {
	Dstring tmp(name);
	tmp.append('/');
	int inthisdir = hash_dir(tmp, T, P);
	FileType *lu(T->lookup(statbuf.st_ino));
	if ( inthisdir == 0 &&  lu == NULL && not_ignored(*n)) {
	    r.type = Directory;
	    T->insert(statbuf.st_ino, Directory);
	    P->insert(*n, Directory);
	    new_records = new FileRecordList(r, new_records);
	    added += 1;
	}
    } else {
	prcswarning << "Ignoring special file: " << squote(name)
		    << ", continuing" << dotendl;

	delete n;

	return 0;
    }

    return 1;
}

static int hash_dir(const char* dir, InoTable* T, PathTable *P)
{
    Dstring d = dir;
    int len, total = 0;

    if(strcmp(d, ".") == 0)
	d.truncate(0);

    len = d.length();

    Dir current_dir(dir);

    foreach(ent_ptr, current_dir, Dir::DirIterator) {
	d.truncate(len);
	d.append(*ent_ptr);
	total += hash_file_or_dir(d, T, P);
    }

    if (!current_dir.OK())
	prcserror << "Error reading directory " << squote(dir) << perror;

    return total;
}

static bool heuristic_keyword_guess(const char* path)
{
    if (option_nokeywords) {
	return true;
    }

    FILE* file;
    char buffer[1024];
    int nread = 0;

    If_fail(file << Err_fopen(path, "r"))
	return false;

    If_fail (nread << Err_fread (buffer, 1024, file))
        nread = -1;

    fclose(file);

    if (nread < 0)
	return false;
    else
	return memchr(buffer, 0, nread) != NULL;
}

static void append_new_files(ProjectDescriptor* P,
			     FileRecordList *record_list,
			     bool delete_files)
{
    const char *name;
    FileType ft;
    FileEntry* fe;

    for (; record_list; record_list = record_list->tail()) {

	ft = record_list->head().type;
	name = *record_list->head().name + cmd_root_project_path_len;
	fe = record_list->head().fe;

	if(option_long_format) {
	    if(delete_files) {
		prcsoutput << "Deleted " << squote(name) << " of type "
			   << format_type(ft) << prcsendl;
	    } else {
		prcsoutput << "Added " << squote(name) << " of type "
			   << format_type(ft) << prcsendl;
	    }
	}

	if(delete_files) {
	    P->append_file_deletion (fe);
	} else if (ft == SymLink) {
	    const char* ln;

	    If_fail (ln << read_sym_link (name))
		ln = "";

	    P->append_link (name, ln);
	} else if (ft == Directory) {
	    P->append_directory (name);
	} else if (ft == RealFile) {
	    P->append_file (name, heuristic_keyword_guess(name));
	}
    }
}

PrVoidError check_project_file_populate(ProjectDescriptor *P,
					PathTable *table,
					InoTable *inodes)
{
    FileType type;
    const char* name;

    foreach_fileent(fe_ptr, P) {
	FileEntry *fe = *fe_ptr;
	type = fe->file_type();
	name = fe->working_path();
	bool file_present;

	if(Failure(file_present << fe->check_working_file()) || !file_present) {
	    if(option_populate_delete && fe->on_command_line()) {
		char c;
		static BangFlag bang;

		prcsquery << "File " << squote(name) << " is unavailable.  "
			  << force("Deleting")
			  << report("Delete")
			  << allow_bang(bang)
			  << option('n', "Don't delete this file")
			  << defopt('y', "Delete from project file")
			  << query("Delete");

		Return_if_fail(c << prcsquery.result());

		if(c == 'y') {
		    deleted += 1;
		    deleted_records = new FileRecordList(
			FileRecord(fe->file_type(),
				   new Dstring(fe->working_path()),
				   fe),
			deleted_records);
		    P->delete_file(fe);
		}
	    }

	    table->insert(name, type);
	    continue;
	}

	table->insert(name, type);
	inodes->insert(fe->stat_inode(), type);
    }

    struct stat buf;

    If_fail (Err_stat (P->project_file_path(), &buf))
	pthrow prcserror << "Stat failed on file " << squote (P->project_file_path()) << perror;

    inodes->insert (buf.st_ino, RealFile);

    If_fail (Err_stat (P->project_aux_path(), &buf)) {

    } else {
	inodes->insert (buf.st_ino, RealFile);
    }

    return NoError;
}

static PrVoidError init_ignore(ProjectDescriptor* project)
{
    OrderedStringTable *ignore_array = project->populate_ignore();

    foreach (ds_ptr, ignore_array->key_array(), OrderedStringTable::KeyArray::ArrayIterator) {
      const char* ds = (*ds_ptr);

      reg2ex2_t *r = new reg2ex2_t;

      Return_if_fail (prcs_compile_regex (ds, r));

      ignore = new VoidPtrList (r, ignore);
    }

    return NoError;
}

static bool not_ignored(const char* name)
{
    VoidPtrList *i = ignore;

    for (; i; i = i->tail()) {

	bool matches = prcs_regex_matches(name, (reg2ex2_t*) i->head());

	if(matches) {
	    if (option_long_format)
		prcsoutput << "Ignoring file " << squote(name) << dotendl;

	    return false;
	}
    }

    return true;
}

PrVoidError prcs_compile_regex(const char* pat, reg2ex2_t *r)
{
    if(reg2comp (r, pat, REG2_NOSUB) != 0)
	pthrow prcserror << "Regular expression compilation failed on "
			<< pat << dotendl;
    /* Why's the error interface have to be so difficult? */

    return NoError;
}

bool prcs_regex_matches(const char* name, reg2ex2_t* r)
{
    return reg2ex2ec(r, name, 0, 0, 0) == 0;
}

/**********************************************************************/
/*                             Depopulate                             */
/**********************************************************************/

PrPrcsExitStatusError depopulate_command()
{
    ProjectDescriptor *project;
    int files = 0;
    bool once = true;

    Return_if_fail(project << read_project_file(cmd_root_project_full_name,
						cmd_root_project_file_path,
						true,
						KeepNothing));

    eliminate_unnamed_files(project);

    if (cmd_filenames_count == 0) {
	/* Do they really want to do this? */

	prcsquery << "You have requested to delete every file in the working project.  "
		  << report ("Continue")
		  << force ("Continue")
		  << defopt ('y', "Continue")
		  << optfail ('n')
		  << query ("Are you sure");

	Return_if_fail (prcsquery.result());
    }

    foreach_fileent (fe_ptr, project) {
	FileEntry *fe = *fe_ptr;

	if (fe->on_command_line()) {

	    if (option_long_format)
		prcsoutput << "Removed file " << squote (fe->working_path()) << dotendl;

	    if (once) {
		once = false;

		project->append_files_data ("\n;; Files deleted by depopulate at ");
		project->append_files_data (get_utc_time());
		project->append_files_data (",\n;; from version ");
		project->append_files_data (project->full_version());
		project->append_files_data (", by ");
		project->append_files_data (get_login());
		project->append_files_data (":\n");
	    }

	    project->delete_file (fe);
	    project->append_file_deletion (fe);
	    files += 1;
	}
    }

    if (!once)
	project->append_files_data ("\n");

    if (files == 0)
	prcsoutput << "Removed no files" << dotendl;
    else if (files == 1)
	prcsoutput << "Removed 1 file" << dotendl;
    else
	prcsoutput << "Removed " << files << " files" << dotendl;

    if(option_report_actions)
	return ExitSuccess;

    Return_if_fail(project->write_project_file(project->project_file_path()));

    return ExitSuccess;
}
