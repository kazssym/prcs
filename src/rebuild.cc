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

extern "C" {
#include "md5.h"
}

#include "prcs.h"

#include "repository.h"
#include "misc.h"
#include "vc.h"
#include "setkeys.h"
#include "checkout.h"
#include "hash.h"
#include "system.h"
#include "memseg.h"
#include "rebuild.h"
#include "prcsdir.h"
#include "projdesc.h"
#include "fileent.h"
#include "syscmd.h"
#include "checkin.h"

struct RebuildCallbackData {
    RepEntry* rep_entry;
    RcsFileTable* file_table;
};

static const char *old_data_file_headers[] =
{
    "\158\233\071\375",
    "\159\234\072\376",
    NULL
};

static const char data_file_header[] = "\156\234\072\376";
static const int data_file_length = 4;
static const char project_summary_header[]  = "@";
static const char rcs_file_summary_header[] = "#";
static const char version_count_header[]    = "$";
const char prcs_data_file_name[] = "prcs_data";

static MemorySegment read_buf(false);

static PrRcsDeltaPtrError rebuild_repository_file(RepEntry* rep_entry, const char* name);

/**********************************************************************/
/*                        Miscellaneous Debris                        */
/**********************************************************************/

/* Simply compute the md5 checksum of a memory segment */
char* md5_buffer(const char* buffer, int buflen)
{
    MD5_CTX context;
    static unsigned char digest[16];

    MD5Init(&context);
    MD5Update(&context, (unsigned char*)buffer, buflen);
    MD5Final(digest, &context);

    return (char*)digest;
}

/* Determines whether a file is garbage by looking at its extension and
 * for the "prcs_" prefix. */
static bool is_garbage_by_name(const char* pathname)
{
    const char* name = strip_leading_path(pathname);
    int len = strlen(name);

    if(strcmp(name + len - 2, ",v") == 0)
	return false;
    else if(strncmp(name, "prcs_", 5) == 0)
	return false;
    else
	return true;
}

/* Determines whether a file is used internally by PRCS, by looking at the
 * prefix. */
static bool is_prcs_file(const char* pathname, const char* project_file_name)
{
    const char* name = strip_leading_path(pathname);
    const char* projname = strip_leading_path(project_file_name);

    if(strncmp(name, "prcs_", 5) == 0)
	return true;
    else if(strcmp(name, projname) == 0)
	return true;
    else
	return false;
}

/* Writes the data file header, consisting of the string "This file contains ..."
 * and the project name */
static void write_data_file_header(ostream& os, const char* /*name*/)
{
    os.write(data_file_header, data_file_length);
}

/* Compute the checksum of a file by checkin it out into memory,
 * and calling md5_buffer. */
static PrVoidError compute_cksum(const char* versionfile, RcsVersionData* ver)
{
    FILE* cofile;

    Return_if_fail(cofile << VC_checkout_stream(ver->rcs_version(), versionfile));

    // Blah
    Dstring ds;
    describe_versionfile (versionfile, ver, ds);
    Return_if_fail(read_buf.map_file(ds.cast(), fileno(cofile)));

    ver->length(read_buf.length());

    Return_if_fail(VC_close_checkout_stream(cofile, ver->rcs_version(), versionfile));

    ver->unkeyed_checksum(md5_buffer(read_buf.segment(), read_buf.length()));

    return NoError;
}

/* Create a data file with no versions in it, for initializing a repository */
PrVoidError create_empty_data(const char* project, const char* filename0)
{
    ofstream new_data;

    new_data.open(filename0);

    write_data_file_header(new_data, project);

    if(new_data.bad()) {
	pthrow prcserror << "Failed creating new repository data file "
			 << squote(filename0) << perror;
    }

    return NoError;
}

/**********************************************************************/
/*                          Write Methods                             */
/**********************************************************************/

/* Write out the ProjectVersionData structure */
static void write_data_file_project_info(ostream& os, ProjectVersionData* ver)
{
    os << ver->date() << '\0';
    os << ver->author() << '\0';
    os << ver->rcs_version() << '\0';

    os << (int)ver->deleted() << '\0';

    os << ver->prcs_major() << '\0'
       << ver->prcs_minor() << '\0';

    os << ver->parent_count() << '\0';

    for (int i = 0; i < ver->parent_count(); i += 1)
	os << ver->parent_index(i) << '\0';
}

/* Write out the RcsVersionData structure */
static void write_data_file_rcs_info(ostream& os, RcsVersionData* ver)
{
    os << ver->date() << '\0';
    os << ver->length() << '\0';
    os << ver->author() << '\0';
    os << ver->rcs_version() << '\0';
    os << ver->plus_lines () << '\0';
    os << ver->minus_lines () << '\0';

    os.write(ver->unkeyed_checksum(), 16);
}

/* Write out a ProjectVersionData for each version in the project */
static void write_data_file_project_summary(ostream& os,
					    ProjectVersionDataPtrArray *project_summary)
{
    os << project_summary_header << project_summary->length() << '\0';

    foreach(proj_ptr2, project_summary, ProjectVersionDataPtrArray::ArrayIterator)
	write_data_file_project_info(os, *proj_ptr2);
}

static void write_data_file_rcs_file_summary(ostream&os, const char* name, RcsDelta* delta)
{
    if (delta->count() == 0) return;
    /* Here because reference_files isn't removing files it deletes
     * from the table and this is easier. */

    os << rcs_file_summary_header << name << '\0';
    os << version_count_header << delta->count() << '\0';

    foreach(ver, delta, RcsDelta::DeltaIterator)
	write_data_file_rcs_info(os, *ver);
}

/**********************************************************************/
/*                       Deletion Methods                             */
/**********************************************************************/

/* Delete a single version from a single file. */
/* Whether RCS really deletes the version or not is ignored */
static PrVoidError delete_version(const char* file_name, const char* version)
{
    if(!option_report_actions) {
	bool deleted;

	Return_if_fail(deleted << VC_delete_version(version, file_name));

	if(!deleted)
	    return NoError;
    }

    if(option_long_format)
	prcsoutput << (option_report_actions ? "Delete" : "Deleted")
		   << " unreferenced version " << version
		   << " from version file " << squote(file_name) << dotendl;

    return NoError;
}

/* Delete all unreferenced versions from an RCS file */
static PrVoidError delete_unreferenced_versions_file(RepEntry* rep_entry,
						     const char* file_name,
						     RcsDelta* delta)
{
    foreach(data_ptr, delta, RcsDelta::DeltaIterator) {
	RcsVersionData* data = *data_ptr;

	if(data->referenced() == RcsVersionData::Referenced)
	    continue;

	rep_entry->Rep_log() << "Deleteing version " << data->rcs_version()
			     << " from file " << file_name << dotendl;

	delta->remove(data->rcs_version());

	Return_if_fail(delete_version(file_name, data->rcs_version()));
    }

    return NoError;
}

static PrVoidError ignoring(ProjectVersionData* project_data)
{
    prcswarning << "Project version " << project_data << "'s project file is invalid "
      "and will be ignored. " << dotendl;

    bug ();

    return NoError;
}

/* Mark all versions referenced by the a single version of a project
 * in the file table. */
static PrVoidError reference_versions(const char* project_file_name,
				      ProjectVersionData* project_data,
				      RcsFileTable* file_table)
{
    FILE* cofile;
    ProjectDescriptor* project;
    Dstring name;

    Return_if_fail(cofile << VC_checkout_stream(project_data->rcs_version(),
						project_file_name));

    name.sprintf("%s:%s", project_file_name, project_data->rcs_version());

    If_fail(project << read_project_file(cmd_root_project_name,
					 strip_leading_path(name),
					 false,
					 cofile,
					 KeepNothing))
	return ignoring(project_data);

    Return_if_fail(VC_close_checkout_stream(cofile,
					    project_data->rcs_version(),
					    project_file_name));

    foreach_fileent (fe_ptr, project) {
	FileEntry *fe = *fe_ptr;

	if (fe->file_type() == RealFile && !fe->descriptor_name())
	    return ignoring(project_data);
    }

    foreach_fileent(fe_ptr, project) {
	RcsDelta **lookup, *delta;
	RcsVersionData* version_data;
	FileEntry *fe = *fe_ptr;

	if(fe->file_type() != RealFile)
	    continue;

	if((lookup = file_table->lookup(fe->descriptor_name())) == NULL) {
	    /*delete project;*/
	    pthrow prcserror << "Version file "
			    << squote(fe->descriptor_name())
			    << " referenced by project version "
                            << project_data
                            << " but not found, cannot clean repository" << dotendl;
	}

	delta = *lookup;

	version_data = delta->lookup(fe->descriptor_version_number());

	if(!version_data) {
	    /*delete project;*/
	    pthrow prcserror << "Version " << fe->descriptor_version_number()
			    << " in repository file "
			    << squote(fe->descriptor_name())
			    << " referenced by project version "
                            << project_data
                            << " but not found, cannot clean repository"
                            << dotendl;
	}

	version_data->reference(RcsVersionData::Referenced);
    }

    delete project;

    return NoError;
}

/* For each file, call delete_unreferenced_versions_file */
static PrVoidError
delete_unreferenced_versions(RepEntry* rep_entry)
{
    ProjectVersionDataPtrArray* project_summary = rep_entry->project_summary();
    RcsFileTable* file_table = rep_entry->rcs_file_summary();

    foreach(project_data_ptr, project_summary,
	    ProjectVersionDataPtrArray::ArrayIterator) {

	if((*project_data_ptr)->deleted())
	    continue;

	Return_if_fail(reference_versions(rep_entry->Rep_name_of_version_file(),
					  *project_data_ptr,
					  file_table));
    }

    foreach(file_pair_ptr, file_table, RcsFileTable::HashIterator) {
	Dstring name(rep_entry->Rep_name_in_entry((*file_pair_ptr).x()));

	name.append (",v");

	Return_if_fail(delete_unreferenced_versions_file(rep_entry, name, (*file_pair_ptr).y()));

	if((*file_pair_ptr).y()->count() == 0) {
	    /* Takes care of deleteing the empty file */
	    rebuild_repository_file(rep_entry, name);
	}
    }

    return NoError;
}


/**********************************************************************/
/*                          Gather Methods                            */
/**********************************************************************/

/* Read one RCS file's rlog, and return an RcsDelta table */
static PrRcsDeltaPtrError rebuild_repository_file(RepEntry* rep_entry,
						  const char* name)
{
    RcsDelta* data;

    Return_if_fail(data << VC_get_version_data(name));

    if(data->count() == 0) {
	if(option_long_format) {
	    prcsoutput << (option_report_actions ? "Delete" : "Deleting")
		       << " empty version file " << squote(strip_leading_path(name))
		       << " from repository" << dotendl;
	}

	rep_entry->Rep_log() << "Deleting empty file " << squote(name) << dotendl;

	if(!option_report_actions) {
	    If_fail(Err_unlink(name))
		pthrow prcserror << "Unlink failed on "
				 << squote(name) << perror;
	}

	return (RcsDelta*)0;
    }

    foreach(rev_ptr, data, RcsDelta::DeltaIterator) {
	Return_if_fail(compute_cksum(name, *rev_ptr));
    }

    return data;
}

static PrVoidError rebuild_file_table_file (const char* name,
					    const void* data)
{
    RepEntry* rep_entry = ((RebuildCallbackData*)data)->rep_entry;
    RcsFileTable* file_table = ((RebuildCallbackData*)data)->file_table;

    if(fs_is_symlink(name) || is_garbage_by_name(name)) {

	rep_entry->Rep_log() << "Deleting garbage " << squote(name) << dotendl;

	/* Delete it! */
	if(!option_report_actions) {
	    If_fail(Err_unlink(name)) {
		prcswarning << "Failed removing garbage file "
			    << squote(name)
			    << " from repository" << dotendl;
		return NoError;
	    }
	}

	if(option_long_format) {
	    prcsoutput << (option_report_actions ? "Delete" : "Deleting")
		       << " garbage file " << squote(name)
		       << " from repository" << dotendl;
	}

    } else if(!is_prcs_file(name, rep_entry->Rep_name_of_version_file())) {

	/* Found an RCS file, run rebuild */
	RcsDelta* delta_ptr;

	Return_if_fail(delta_ptr << rebuild_repository_file(rep_entry, name));

	if(!delta_ptr)
	    return NoError;

	Dstring saved_name (name + strlen(rep_entry->Rep_entry_path()) + 1);

	saved_name.truncate (saved_name.length() - 2);

	file_table->insert(p_strdup(saved_name.cast()), delta_ptr);
    }

    return NoError;
}

static PrRcsFileTablePtrError rebuild_file_table(RepEntry* rep_ptr)
{
    RcsFileTable* file_table = new RcsFileTable;
    RebuildCallbackData data;

    data.rep_entry = rep_ptr;
    data.file_table = file_table;

    Return_if_fail(directory_recurse (rep_ptr->Rep_entry_path(),
				      &data, rebuild_file_table_file));

    return file_table;
}


/* This is the top-level call to rebuilding a repository data file. */
static PrVoidError rebuild_repository(RepEntry *rep_ptr,
				      ProjectVersionDataPtrArray* project_summary,
				      RcsFileTable* file_table)
{
    if(!file_table || !project_summary) {
	Return_if_fail(project_summary <<
		       VC_get_project_version_data(rep_ptr->Rep_name_of_version_file()));

	Return_if_fail(file_table << rebuild_file_table(rep_ptr));
    }

    /* Unlink here in case we're mmaping the file that's about to get truncated. */
    If_fail(Err_unlink(rep_ptr->Rep_name_in_entry(prcs_data_file_name))) {
	pthrow prcserror << "Unlink failed on old repository data file "
			<< squote(rep_ptr->Rep_name_in_entry(prcs_data_file_name))
			<< perror;
    }

    rep_ptr->project_summary(project_summary);
    rep_ptr->rcs_file_summary(file_table);

    If_fail(delete_unreferenced_versions(rep_ptr))
	prcsinfo << "Continuing to build repository data file, "
	"no cleaning was performed" << dotendl;

    if (option_report_actions) {
	prcsinfo << "Rebuild will be likely successful, the repository appears to be well" << dotendl;
	return NoError;
    }

    ofstream new_data(rep_ptr->Rep_name_in_entry(prcs_data_file_name));

    write_data_file_header(new_data, cmd_root_project_name);

    write_data_file_project_summary(new_data, project_summary);

    foreach(file_pair_ptr, file_table, RcsFileTable::HashIterator) {
	write_data_file_rcs_file_summary(new_data,
					 (*file_pair_ptr).x(),
					 (*file_pair_ptr).y());
    }

    new_data.close();

    if(new_data.bad()) {
	pthrow prcserror << "Write failed to reconstructed project data file "
			<< squote(rep_ptr->Rep_name_in_entry(prcs_data_file_name))
			<< perror;
    }

    return NoError;
}

/* Run rebuild with an opened repository */
PrVoidError admin_rebuild_command_no_open(RepEntry* rep, bool valid_rep_data)
{
    Umask mask (rep->Rep_get_umask());

    Return_if_fail(rep->Rep_uncompress_all_files());

    if(valid_rep_data)
	Return_if_fail(rebuild_repository(rep,
					  rep->project_summary(),
					  rep->rcs_file_summary()));
    else
	Return_if_fail(rebuild_repository(rep, NULL, NULL));

    Return_if_fail(rep->Rep_compress_all_files());

    Return_if_fail(rep->Rep_make_default_tag ());

    return NoError;
}

/* This is it. */
PrPrcsExitStatusError admin_rebuild_command()
{
    RepEntry *rep;

    Return_if_fail(rep << Rep_init_repository_entry(cmd_root_project_name,
						    true, false, false));

    rep->Rep_log() << "Rebuilding project" << dotendl;

    prcsinfo << "This command may run for a long time" << dotendl;

    Return_if_fail(admin_rebuild_command_no_open(rep, false));

    rep->Rep_log() << "Suceeded rebuilding project" << dotendl;

    return ExitSuccess;
}

/* Mark a project version as deleted by changing its log information */
static PrVoidError mark_deleted(ProjectVersionData* project_data, RepEntry* rep_entry)
{
    Dstring log;

    project_data->deleted(true);

    format_version_log(project_data, log);

    Return_if_fail(VC_set_log(log.cast(),
			      project_data->rcs_version(),
			      rep_entry->Rep_name_of_version_file()));

    return NoError;
}

/* This is it, too. */
PrPrcsExitStatusError delete_command()
{
    RepEntry *rep_entry;
    ProjectVersionData* project_data;

    Return_if_fail(rep_entry << Rep_init_repository_entry(cmd_root_project_name,
						   true, false, true));

    Return_if_fail(project_data << resolve_version(cmd_version_specifier_major,
						   cmd_version_specifier_minor,
						   cmd_root_project_full_name,
						   cmd_root_project_file_path,
						   NULL,
						   rep_entry));
#if 0
    /*@@@ I have no idea about this*/
    if(project_data->prcs_minor_int() == 0)
	pthrow prcserror << "You may not delete minor version 0" << dotendl;
#endif

    if(project_data->deleted())
	pthrow prcserror << "That version is already deleted" << dotendl;

    prcsquery << "Delete version "
	      << project_data
	      << report("")
	      << defopt('y', "You know what you're doing, go ahead")
	      << optfail('n')
	      << query(".  Are you sure");

    Return_if_fail(prcsquery.result());

    if(option_report_actions)
	return ExitSuccess;

    rep_entry->Rep_log() << "Deleting version " << project_data << dotendl;

    Return_if_fail(mark_deleted(project_data, rep_entry));

    Return_if_fail(admin_rebuild_command_no_open(rep_entry, true));

    rep_entry->Rep_log() << "Suceeded deleting version" << dotendl;

    return ExitSuccess;
}

/* READ */

/* Called when something goes wrong reading a data file */
PrVoidError RebuildFile::bad_data_file()
{
    pthrow prcserror << "Invalid repository data file "
		    << squote(source_name())
		    << ".  Please run " << squote("prcs admin rebuild")
		    << " to rebuild the data file"
		    << dotendl;
}

/* Read the header and assert that the projects match */
PrVoidError RebuildFile::read_header()
{
    const char* s;

    s = get_string(data_file_length);

    if (!s) return bad_data_file();

    for (int i = 0; old_data_file_headers[i]; i += 1) {
	if (strncmp (s, old_data_file_headers[i], data_file_length) == 0) {
	    pthrow prcserror << "The repository data file is outdated, you must run "
			    << squote ("prcs admin rebuild")
			    << " to rebuild the data file" << dotendl;
	}
    }

    if (strncmp (s, data_file_header, data_file_length) != 0)
	return bad_data_file();

    return NoError;
}

/* Read a single project version */
PrProjectVersionDataPtrError RebuildFile::read_project_version(int i)
{
    ProjectVersionData* ver = new ProjectVersionData(i);
    int get;

    if((get = get_size()) < 0)
	return bad_data_file();

    ver->date(get);
    ver->author(get_string());
    ver->rcs_version(get_string());

    if(!(ver->author() && ver->rcs_version()))
	return bad_data_file();

    if((get = get_size()) < 0)
	return bad_data_file();

    ver->deleted(get);
    ver->prcs_major(get_string());
    ver->prcs_minor(get_string());

    if ((get = get_size()) < 0)
	return bad_data_file();

    for (int i = 0, c = get; i < c; i += 1) {

	if ((get = get_size()) < 0)
	    return bad_data_file();

	ver->new_parent(get);
    }

    return ver;
}

/* Read a single RCS version */
PrRcsVersionDataPtrError RebuildFile::read_rcs_version()
{
    RcsVersionData* ver = new RcsVersionData;
    const char* cksum;
    const char *pls, *mls;

    ver->date(get_size());
    ver->length(get_size());

    if(ver->date() < 0 || ver->length() < 0)
	return bad_data_file();

    ver->author(get_string());
    ver->rcs_version(get_string());

    if(!(ver->author() && ver->rcs_version()))
	return bad_data_file();

    if (! (pls = get_string ()) || ! (mls = get_string ()))
	return bad_data_file ();

    ver->set_plus_lines (pls);
    ver->set_minus_lines (mls);

    if(!(cksum = get_string(16)))
	return bad_data_file();

    ver->unkeyed_checksum(cksum);

    return ver;
}

/* Read some number of project versions */
PrVoidError RebuildFile::read_project_summary(int& count)
{
    int version_count;
    ProjectVersionData* ver;

    if((version_count = get_size()) < 0)
	return bad_data_file();

    for(int i = 0; i < version_count; i += 1) {
 	Return_if_fail(ver << read_project_version(count++));
    	project_data->append(ver);
    }

    return NoError;
}

/* Read some number of RCS file summaries */
PrVoidError RebuildFile::read_rcs_file_summary()
{
    const char *read_file_name;
    RcsDelta **lookup, *delta_array;
    int num;

    if((read_file_name = get_string()) == NULL)
	return bad_data_file();

    if((lookup = rcs_file_table->lookup(read_file_name)) == NULL) {
	delta_array = new RcsDelta(NULL);
	rcs_file_table->insert(read_file_name, delta_array);
    } else {
	delta_array = *lookup;
    }

    if(!get_string(version_count_header))
	return bad_data_file();

    if((num = get_size()) < 0)
	return bad_data_file();

    for(int i = 0; i < num; i += 1) {

	RcsVersionData *ver;

	Return_if_fail(ver << read_rcs_version());

	delta_array->insert(ver->rcs_version(), ver);

	ASSERT(delta_array->lookup(ver->rcs_version()) == ver, "hope so");
    }

    return NoError;
}

/* the following RebuildFile methods deal with reading or expecting strings
 * and positive ints from a file. */
PrRebuildFilePtrError read_repository_data(const char* filename0, bool write)
{
    RebuildFile* file = new RebuildFile;

    Return_if_fail(file->init_from_file(filename0, write));

    return file;
}

PrVoidError RebuildFile::init_from_file(const char* filename0, bool write)
{
    filename.assign(filename0);

    seg = new MemorySegment(false, write);

    Return_if_fail(seg->map_file(filename0));

    last = seg->segment() + seg->length();
    offset = 0;

    Return_if_fail(read_header());

    rcs_file_table = new RcsFileTable;

    project_data = new ProjectVersionDataPtrArray;

    const char* s;
    int version_count = 0;

    while((s = get_string(strlen(project_summary_header))) != NULL) {
	if(strncmp(s, project_summary_header, strlen(project_summary_header)) == 0)
	    Return_if_fail(read_project_summary(version_count));
	else if(strncmp(s, rcs_file_summary_header, strlen(rcs_file_summary_header)) == 0)
	    Return_if_fail(read_rcs_file_summary());
	else
	    return bad_data_file();
    }

    if(done())
	return NoError;
    else
	return bad_data_file();
}

const char* RebuildFile::get_string()
{
    const char* val = seg->segment() + offset;

    if(seg->length() < offset)
	return NULL;

    offset += strlen(val) + 1;

    return val;
}

const char* RebuildFile::get_string(int len)
{
    if(last - (seg->segment() + offset) < len || seg->length() < offset)
	return NULL;

    const char* val = seg->segment() + offset;

    offset += len;

    return val;
}

bool RebuildFile::get_string(const char* expected, bool term)
{
    int len = strlen(expected);

    if(last - (seg->segment() + offset) < len || seg->length() < offset)
	return false;

    if(strncmp(expected, seg->segment() + offset, len) != 0)
	return false;

    offset += len;
    if(term)
	offset += 1;

    return true;
}

int RebuildFile::get_size()
{
    const char* p = seg->segment() + offset;
    char c = 0;
    int val = 0;

    while(p < last && (c = *p++) != 0)  {
	if(c < '0' || c > '9')
	    return -1;
	val *= 10;
	val += (c - '0');
    }

    offset = p - seg->segment();

    if(p <= last && c == 0)
	return val;

    return -1;
}

const char* RebuildFile::source_name() { return filename; }

bool RebuildFile::done() { return offset == seg->length(); }

void RebuildFile::init_stream()
{
    if(!buf) {
        buf = new filebuf(fdopen(dup(seg->fd()), "a+"), ios::out);
        buf->pubseekoff(0, ios::end, ios::out);
	os = new ostream(buf);
    }
}

/* The following three methods append new data to the file */
void RebuildFile::add_project_data(ProjectVersionData* data)
{
    init_stream();

    (*os) << project_summary_header << 1 << '\0';

    write_data_file_project_info(*os, data);
}

void RebuildFile::add_rcs_file_data(const char* new_file_name, RcsVersionData* data)
{
    init_stream();

    (*os) << rcs_file_summary_header << new_file_name << '\0';
    (*os) << version_count_header << 1 << '\0';

    write_data_file_rcs_info(*os, data);
}

PrVoidError RebuildFile::update_project_data()
{
    os->flush();

    if(os->fail())
	goto error;

    delete os;
    delete buf;

    os = NULL;
    buf = NULL;

    If_fail(seg->unmap())
	goto error;

    delete seg;
    seg = NULL;

    return NoError;

error:
    pthrow prcserror << "Failed updating repository data file"
		     << squote(source_name()) << perror;
}

RebuildFile::~RebuildFile()
{
    delete seg;

    foreach(data_ptr, project_data, ProjectVersionDataPtrArray::ArrayIterator)
	delete (*data_ptr);

    delete project_data;

    foreach(file_ptr, rcs_file_table, RcsFileTable::HashIterator) {
	delete (*file_ptr).y();
    }

    delete rcs_file_table;
}

ProjectVersionDataPtrArray* RebuildFile::get_project_summary() const
{
    return project_data;
}

RcsFileTable* RebuildFile::get_rcs_file_summary() const
{
    return rcs_file_table;
}

RebuildFile::RebuildFile()
    :buf(NULL), os(NULL) { }
