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
 * $Id: checkout.cc 1.16.1.24.1.3.1.8.1.1.1.27 Sat, 02 Feb 2002 13:07:41 -0800 jmacd $
 */

#include "prcs.h"

#include "repository.h"
#include "projdesc.h"
#include "fileent.h"
#include "vc.h"
#include "checkout.h"
#include "checkin.h"
#include "setkeys.h"
#include "misc.h"
#include "system.h"
#include "memseg.h"
#include "rebuild.h"

static PrVoidError checkout_each_file(ProjectDescriptor*);
static PrVoidError make_all_subdirectories(ProjectDescriptor*);
static PrVoidError write_new_prj_file(ProjectDescriptor*);
static PrPrcsExitStatusError create_empty_project();
static PrVoidError checkout_symlink(FileEntry*);
static PrVoidError checkout_keywords(FileEntry*);
static PrVoidError checkout_nokeywords(FileEntry*);
static PrOverwriteStatusError check_overwrite_different_file_type(FileEntry*);

static const char new_project_format[] =
";; -*- Prcs -*-\n"
"(Created-By-Prcs-Version %d %d %d)\n"
"(Project-Description \"\")\n"
"(Project-Version %s %s 0)\n"
"(Parent-Version -*- -*- -*-)\n"
"(Version-Log \"Empty project.\")\n"
"(New-Version-Log \"\")\n"
"(Checkin-Time \"%s\")\n"
"(Checkin-Login %s)\n"
"(Populate-Ignore ())\n"
"(Project-Keywords)\n"
"(Files\n"
";; This is a comment.  Fill in files here.\n"
";; For example:  (prcs/checkout.cc ())\n"
")\n"
"(Merge-Parents)\n"
"(New-Merge-Parents)\n";

PrPrcsExitStatusError checkout_command()
{
    ProjectDescriptor *project;
    ProjectVersionData* project_data;
    RepEntry* rep_entry;
    bool entry_exists;

    Return_if_fail(entry_exists <<
		   Rep_repository_entry_exists(cmd_root_project_name));

    if(!entry_exists || cmd_version_specifier_minor_int == 0) {
	if(!entry_exists)
	    prcsinfo << "Project not found in repository, initial checkout" << dotendl;

        if(cmd_version_specifier_minor_int > 0) {
	    prcsquery << "Minor version " << cmd_version_specifier_minor
		      << " is ignored at initial checkout.  "
		      << force("Continuing")
		      << report("Continue")
		      << optfail('n')
		      << defopt('y', "Create empty project file with minor version 0")
		      << query("Continue");

	    Return_if_fail(prcsquery.result());
        }

        return create_empty_project();
    }

    Return_if_fail(rep_entry << Rep_init_repository_entry(cmd_root_project_name,
							  false, false, true));

    if(rep_entry->version_count() == 0)
	return create_empty_project();

    Return_if_fail(project_data << resolve_version(cmd_version_specifier_major,
						   cmd_version_specifier_minor,
						   cmd_root_project_full_name,
						   cmd_root_project_file_path,
						   NULL,
						   rep_entry));

    if(project_data->prcs_minor_int() == 0)
	return create_empty_project();

    prcsinfo << "Checkout project " << squote(cmd_root_project_name)
	     << " version " << project_data << dotendl;

    Return_if_fail(project << rep_entry->checkout_prj_file(cmd_root_project_full_name,
							   project_data->rcs_version(),
							   KeepNothing));

    project->read_quick_elim();

    eliminate_unnamed_files(project);

    Return_if_fail(warn_unused_files(true));

    Return_if_fail(write_new_prj_file(project));

    Return_if_fail(checkout_each_file(project));

    project->quick_elim_update();

    return ExitSuccess;
}

static PrPrcsExitStatusError create_empty_project()
{
    if (strcmp(cmd_version_specifier_major, "@") == 0 ||
        cmd_version_specifier_major[0] == '\0') {
        cmd_version_specifier_major = "0";
    }

    ProjectDescriptor* new_project;

    /* sortof a kludge, so that write_new_prj_file thinkgs it needs
     * to write the project file. */
    cmd_prj_given_as_file = true;

    Return_if_fail(new_project << checkout_empty_prj_file(cmd_root_project_full_name,
							  cmd_version_specifier_major,
							  KeepNothing));

    Return_if_fail(write_new_prj_file(new_project));

    prcsinfo << "You may now edit the file " << squote(cmd_root_project_file_path) << dotendl;

    return ExitSuccess;
}

PrVoidError check_create_subdir(const char* dir)
{
    struct stat buf;

    if(stat(dir, &buf) >= 0) {

        if (!S_ISDIR(buf.st_mode)) {
	    prcsquery << "Required subdirectory "
		      << squote(dir)
		      << " exists and is not a directory.  "
		      << force ("Deleting")
		      << report ("Delete")
		      << optfail ('n')
		      << defopt ('y', "Delete the file, replacing it with the required directory")
		      << query ("Delete");

	    Return_if_fail (prcsquery.result ());

	    If_fail (fs_nuke_file (dir))
	        pthrow prcserror << "Couldn't remove file " << squote (dir) << perror;
	} else {
	  return NoError;
	}
    }

    If_fail(Err_mkdir(dir, (mode_t)0777)) {
        pthrow prcserror << "Couldn't create required subdirectory " << squote(dir) << perror;
    }

    return NoError;
}

PrVoidError make_subdirectories(const char* name0)
{
    char dir[MAXPATHLEN];
    char* name = dir;

    strncpy(dir, name0, MAXPATHLEN);
    dir[MAXPATHLEN - 1] = 0;

    while(*name != '\0') {

	while(*name != '/' && *name != '\0') {name += 1; }

	if(*name == '\0')
	    break;

	*name = '\0';

	if (dir[0]) /* if not / */
	    Return_if_fail(check_create_subdir(dir));

	*name = '/';

	while(*name == '/') { name += 1; }
    }

    return NoError;
}

static PrVoidError make_all_subdirectories(ProjectDescriptor *project)
{
    if(option_report_actions)
        return NoError;

    foreach_fileent(fe_ptr, project) {
	FileEntry *fe = *fe_ptr;

        if(!fe->on_command_line())
            continue;

	Return_if_fail(make_subdirectories(fe->working_path()));

        if(fe->file_type() == Directory)
	    Return_if_fail(check_create_subdir(fe->working_path()));
    }

    return NoError;
}

static PrVoidError checkout_each_file(ProjectDescriptor* project)
{
    Return_if_fail(make_all_subdirectories(project));

    foreach_fileent(fe_ptr, project) {
	FileEntry *fe = *fe_ptr;

        if(!fe->on_command_line() || fe->file_type() == Directory)
            continue;

        if(fe->file_type() == SymLink) {
	    Return_if_fail(fe->initialize_descriptor(project->repository_entry(),
						     false, false));
            Return_if_fail(checkout_symlink(fe));
	} else if(fe->keyword_sub()) {
	    Return_if_fail(fe->get_repository_info(project->repository_entry()));
            Return_if_fail(checkout_keywords(fe));
	} else
	    Return_if_fail(checkout_nokeywords(fe));

	project->repository_entry()->Rep_clear_compressed_cache();
    }

    return NoError;
}

static PrVoidError checkout_symlink(FileEntry* fe)
{
    const char* filename = fe->working_path();
    OverwriteStatus replace;

    Return_if_fail(replace << check_overwrite_different_file_type(fe));

    if(replace == IgnoreMe)
	return NoError;

    if(replace == SameType) {
        const char* rdlnk;

        Return_if_fail(rdlnk << read_sym_link(filename));

        if(strcmp(rdlnk, fe->link_name()) == 0) {
            if(option_long_format) {
                prcsoutput << "Symlink " << squote(filename)
			   << " unchanged" << dotendl;
	    }

	    DEBUG("Symlink " << squote(filename) << " unchanged");

	    return NoError;
	}

	static BangFlag bang;

	prcsquery << "Symlink " << squote(filename)
		  << " reads " << squote(rdlnk) << ", new text is "
		  << squote(fe->link_name()) << ".  "
		  << force("Updating")
		  << report("Update")
		  << allow_bang (bang)
		  << option('n', "Leave symlink as is")
		  << defopt('y', "Update symlink")
		  << query("Update");

	Return_if_fail(prcsquery.result());

	if(option_report_actions)
	    return NoError;

	If_fail(Err_unlink(filename)) {
            pthrow prcserror << "Can't unlink old symlink "
			     << squote(filename) << perror;
        }
    }

    DEBUG("Create symlink " << squote(filename));

    if(!option_report_actions) {
	If_fail(Err_symlink(fe->link_name(), filename)) {
	    pthrow prcserror << "Failed creating symlink " << squote(filename)
			     << perror;
	}
    }

    if(option_long_format) {
        prcsoutput << "Checkout symlink " << squote(filename)
		   << " to " << squote(fe->link_name()) << dotendl;
    }

    return NoError;
}

static MemorySegment unsetkey_buffer(false);
static MemorySegment setkey_buffer(false);
static MemorySegment file_buffer(false);

static PrVoidError checkout_keywords(FileEntry* fe)
{
    bool setkey_val;
    OverwriteStatus replace;
    FILE* cofile;
    RcsVersionData* version_data;
    bool same_modulo_keywords = false;

    const char* new_setkey_buf = NULL;
    int new_setkey_len = 0;

    const char* new_unsetkey_buf = NULL;
    int new_unsetkey_len = 0;

    const char* filename = fe->working_path();

    Return_if_fail(replace << check_overwrite_different_file_type(fe));

    if(replace == IgnoreMe)
	return NoError;

    Return_if_fail(version_data << fe->project()->repository_entry()->
		   lookup_rcs_file_data(fe->descriptor_name(),
					fe->descriptor_version_number()));

    if(replace == SameType) {
	/* The quick_elim database is not used as it is impractical to
	 * make assumptions about the version of PRCS that last set
	 * keywords in a file.  Knowledge of the keys would be
	 * neccesary for quick_elim to know that the file will remain
	 * unchanged. */

	/* I have optimized this code for low disk IO bandwidth and
	 * not memory use.  For a version of this procedure with lower
	 * memory requirements and higher disk IO bandwidth, see
	 * versions prior to pre-release.16 */

	/* Outline of the following somewhat hairy comparison: First
	 * unsetkey the old file into memory and compare lengths with
	 * the one that will be pulled out of the repository, if the
	 * lengths are the same, compare md5 checksums, if they are
	 * still the same, query the user to continue without replacing
	 * keys.  If setkeys did not modify the file, then the files
	 * are the same and return.
	 *
	 * If the user asks for keywords to be replaced and the md5
	 * checksums compared, use the old file to set keywords and
	 * avoid reading from the repository.
	 *
	 * If the md5 checksums are different, check out the new file
	 * into memory.  Read the old file into memory, setkey the new
	 * file into memory and compare.  If the keyed files are the
	 * same, maybe let the user know and return.  If they are
	 * different, query the user to replace the file. */

	bool old_file_sets_keys;

	Return_if_fail(old_file_sets_keys << setkeys_outbuf(filename, &unsetkey_buffer,
							    fe, Unsetkeys));

	if(unsetkey_buffer.length() == version_data->length()) {
	    char* checksum;

	    checksum = md5_buffer(unsetkey_buffer.segment(), unsetkey_buffer.length());

	    if(memcmp(checksum, version_data->unkeyed_checksum(), 16) == 0) {

		if(old_file_sets_keys) {
		    /* Files are the same modulo keywords */

		    DEBUG("File " << squote(filename) << " is the same modulo keywords");

		    same_modulo_keywords = true;

		    /* We can skip checking the new file out of the
		     * repository and just use the current buffer to
		     * replace keys. */

		    new_unsetkey_buf = unsetkey_buffer.segment();
		    new_unsetkey_len = unsetkey_buffer.length();

		} else {
		    /* No keywords were found, so the files are identicle, we
		     * can just return. */
		    DEBUG("File " << squote(filename) << " is identical");
		    if(option_long_format) {
			prcsoutput << "File " << squote(filename)
				   << " is unmodified" << dotendl;
		    }

		    return NoError;
		}
	    }
	}
    }

    if(!new_unsetkey_buf) {
	/* Now, if the md5 checksum didn't match, check out the new version */

	Return_if_fail(fe->initialize_descriptor(fe->project()->repository_entry(), false, false));

	Return_if_fail(cofile << VC_checkout_stream(fe->descriptor_version_number(),
						    fe->full_descriptor_name()));

	/* version_data->length() is an optimization to avoid resizing the
	 * buffer each time the pipe data doubles in size */
	Dstring ds;
	fe->describe (ds);
	Return_if_fail(unsetkey_buffer.map_file(ds.cast (), fileno(cofile), version_data->length()));

	Return_if_fail(VC_close_checkout_stream(cofile,
						fe->descriptor_version_number(),
						fe->full_descriptor_name()));

	new_unsetkey_buf = unsetkey_buffer.segment();
	new_unsetkey_len = unsetkey_buffer.length();
    }

    /* At this point, new_unsetkey_buf is located in unsetkey_buffer's
     * segment.  Code following cannot trample or use it */

    if(replace == SameType) {
	/* Files may differ, now compare keyword values. */

	Return_if_fail(setkey_val << setkeys_inoutbuf(new_unsetkey_buf,
						      new_unsetkey_len,
						      &setkey_buffer,
						      fe,
						      Setkeys));

	new_setkey_buf = setkey_buffer.segment();
	new_setkey_len = setkey_buffer.length();

	/* Keys are set in the new file, file_buffer is no longer needed,
	 * now map the old file into file_buffer */

	if(new_setkey_len == fe->stat_length()) {
	    /* Length is the same, now try memcmp */

	    Return_if_fail(file_buffer.map_file(filename));

	    if(memcmp(new_setkey_buf, file_buffer.segment(), new_unsetkey_len) == 0) {
		/* No differences. */
		if(option_long_format) {
		    prcsoutput << "File " << squote(filename)
			       << " is unmodified" << dotendl;
		}

		DEBUG("Keywords match");

		return NoError;
	    }

	    DEBUG("Keywords don't match");
	}

	/* Now we're sure the files differ, so query the user */

	if(same_modulo_keywords) {
	    static BangFlag bang;

	    prcsquery << "File " << squote(filename)
		      << " differs only in keywords.  "
		      << report("Replace")
		      << force("Replacing")
		      << allow_bang(bang)
		      << option('n', "Leave file untouched with old keywords")
		      << defopt('y', "Replace with up to date keywords")
		      << query("Replace");

	} else {
	    static BangFlag bang;

	    prcsquery << "File " << squote(filename)
		      << " differs.  "
		      << report("Replace") /* no force */
		      << force("Replacing")
		      << allow_bang(bang)
		      << option('n', "Leave old file and ignore this checkout")
		      << defopt('y', "Replace with new file")
		      << query("Replace");
	}

	char c;

	Return_if_fail(c << prcsquery.result());

	if(c == 'n')
	    return NoError;
    }

    if(new_setkey_buf) {
	if (!option_report_actions) {
	    WriteableFile outfile;

	    Return_if_fail(outfile.open(filename));

	    Return_if_fail(outfile.write(new_setkey_buf, new_setkey_len));

	    Return_if_fail(outfile.close());
	}
    } else {
	if (option_report_actions) {
	    Return_if_fail(setkey_val << setkeys_inputbuf(new_unsetkey_buf,
							  new_unsetkey_len,
							  NULL,
							  fe,
							  Setkeys));
	} else {
	    WriteableFile outfile;

	    Return_if_fail(outfile.open(filename));

	    Return_if_fail(setkey_val << setkeys_inputbuf(new_unsetkey_buf,
							  new_unsetkey_len,
							  &outfile.stream(),
							  fe,
							  Setkeys));

	    Return_if_fail(outfile.close());
	}
    }

    if(!option_report_actions) {

	Return_if_fail(fe->chmod(0));

	Return_if_fail(fe->utime(version_data->date()));

	If_fail(fe->stat())
	    pthrow prcserror << "Stat failed on file " << squote(filename) << perror;

	fe->project()->quick_elim_update_fe(fe);
    }

    if(option_long_format)
        prcsoutput << "Checkout file " << squote(filename)
		   << ", " << (setkey_val ? "" : "no ") << "keywords modified" << dotendl;

    return NoError;
}

static PrVoidError checkout_nokeywords(FileEntry* fe)
{
    OverwriteStatus replace;
    const char* filename = fe->working_path();
    RcsVersionData* version_data;

    Return_if_fail(replace << check_overwrite_different_file_type(fe));

    if(replace == IgnoreMe)
	return NoError;

    Return_if_fail(version_data << fe->project()->repository_entry()->
		   lookup_rcs_file_data(fe->descriptor_name(),
					fe->descriptor_version_number()));

    if(replace == SameType) {

	if(version_data->length() == fe->stat_length()) {

	    Return_if_fail(file_buffer.map_file(filename));

	    char* checksum = md5_buffer(file_buffer.segment(), file_buffer.length());

	    if(memcmp(checksum, version_data->unkeyed_checksum(), 16) == 0) {
		if(option_long_format)
		    prcsoutput << "File " << squote(filename) << " is unmodified" << dotendl;

		return NoError;
	    }
	}

	static BangFlag bang;

	prcsquery << "File " << squote(filename) << " differs.  "
		  << force("Replacing")
		  << report("Replace")
		  << allow_bang (bang)
		  << option('n', "Don't replace this file")
		  << defopt('y', "Replace this file")
		  << query("Replace");

	char c;

	Return_if_fail(c << prcsquery.result());

	if (c == 'n')
	    return NoError;
    }

    if(!option_report_actions) {
	Return_if_fail(fe->initialize_descriptor(fe->project()->repository_entry(), false, false));

	FILE* cofile;

	Return_if_fail(cofile << VC_checkout_stream(fe->descriptor_version_number(),
						    fe->full_descriptor_name()));

	WriteableFile outfile;

	Return_if_fail(outfile.open(filename));

	Return_if_fail(outfile.copy(cofile));

	Return_if_fail(outfile.close());

	Return_if_fail(fe->chmod(0));

	Return_if_fail(fe->utime(version_data->date()));

	If_fail(fe->stat())
	    pthrow prcserror << "Stat failed on file " << squote(filename) << perror;

	fe->project()->quick_elim_update_fe(fe);
    }

    if(option_long_format)
        prcsoutput << "Checkout file " << squote(filename) << dotendl;

    return NoError;
}

/* An OverwriteStatus is one of --
 *
 *      DoesntExist     The file doesn't exist, go ahead.
 *      IgnoreMe        The file exists and the user has asked to ignore it.
 *      SameType        The file exists and a comparison makes sense.
 *
 * This function is used by the three checkout_type() functions to
 * determine whether it should attempt to compare the current file
 * with the one it would like to write out.  */
static PrOverwriteStatusError check_overwrite_different_file_type(FileEntry* fe)
{
    const char* ls = "";

    If_fail(fe->stat())
	/* @@@ There might be a broken symlink to think about. */
        return DoesntExist;

    if((fe->file_type() == SymLink  && S_ISLNK(fe->stat_mode())) ||
       (fe->file_type() == RealFile && S_ISREG(fe->stat_mode())) ||
       (fe->file_type() == Directory && S_ISDIR(fe->stat_mode())))
	return SameType;

    if(!option_force_resolution) {
	If_fail(ls << show_file_info(fe->working_path()))
	    ls = "*** ls failed ***";
    }

    static BangFlag bang;

    prcsquery << ls << prcsendl << "File " << squote(fe->working_path())
	      << " is of different type from the one being checked out("
	      << format_type(fe->file_type()) << ").  "
	      << force("Replacing")
	      << report("Replace")
	      << allow_bang (bang)
	      << option('n', "Don't check out this file")
	      << defopt('y', "Delete this file and replace")
	      << query("Replace");

    char c;

    Return_if_fail(c << prcsquery.result());

    if(c == 'n')
	return IgnoreMe;

    if(!option_report_actions) {
	If_fail(fs_nuke_file(fe->working_path()))
	    pthrow prcserror << "Couldn't remove file " << squote(fe->working_path())
			     << perror;
    }

    return DoesntExist;
}

static PrVoidError write_new_prj_file(ProjectDescriptor* project)
{
    if(option_report_actions || option_exclude_project_file)
        return NoError;

    if(fs_file_exists(cmd_root_project_file_path)) {
        if (!cmd_prj_given_as_file) {
            return NoError;
        }

	prcsquery << "Project file " << squote(cmd_root_project_file_path)
		  << " already exists.  "
		  << force("Replacing")
		  << option('n', "Keep the old project file")
		  << defopt('y', "Replace the project file")
		  << query("Replace");

	char c;

	Return_if_fail(c << prcsquery.result());

	if(c == 'n')
	    return NoError;
    }

    Return_if_fail(project->write_project_file(cmd_root_project_file_path));

    return NoError;
}

PrProjectDescriptorPtrError checkout_empty_prj_file(const char* fullname,
						    const char* maj,
						    ProjectReadData flags)
{
    return checkout_create_empty_prj_file(fullname, temp_file_1, maj, flags);
}

PrProjectDescriptorPtrError checkout_create_empty_prj_file(const char* fullname,
							   const char* name,
							   const char* maj,
							   ProjectReadData flags)
{
    FILE* prj;

    If_fail ( prj << Err_fopen(name, "w") )
        pthrow prcserror << "Cannot create temp file " << squote(name) << perror;

    fprintf (prj, new_project_format,
	     prcs_version_number[0],
	     prcs_version_number[1],
	     prcs_version_number[2],
	     strip_leading_path(fullname),
	     maj,
	     get_utc_time(),
	     get_login());

    If_fail( Err_fclose(prj) )
        pthrow prcserror << "Error writing temp file " << squote(name) << perror;

    return read_project_file(fullname, name, false, flags);
}

PrVoidError WriteableFile::open(const char* filename)
{
    struct stat sbuf;

    if (lstat(filename, &sbuf) >= 0) {

	if (!option_unlink && S_ISLNK(sbuf.st_mode)) {
	    If_fail(Err_stat(filename, &sbuf)) {
		real_name = filename;
	    } else {
		/* This is to make sure there are no cycles, its
		 * not too long, etc. */

	    Return_if_fail(real_name << find_real_filename(filename));

		If_fail(Err_stat(real_name, &sbuf))
		pthrow prcserror << "Stat failed on " << squote(real_name)
				<< perror;
	    }
	} else {
	    real_name = filename;
	}

	if (sbuf.st_nlink > 1)
	    prcswarning << "warning: Breaking " << sbuf.st_nlink - 1
			<< " hard link(s) to " << squote(real_name)
			<< dotendl;

	temp_name.assign(real_name);

	make_temp_file_same_dir (&temp_name);
    } else {
	temp_name.assign(filename);
    }

    os.open(temp_name);

    if (os.bad())
	pthrow prcserror << "Open failed on " << squote(temp_name) << perror;

    return NoError;
}

PrVoidError WriteableFile::write(const char* seg, int len)
{
    os.write(seg, len);

    if (os.bad())
	pthrow prcserror << "Write failed on " << squote(temp_name)
			<< perror;

    return NoError;
}

PrVoidError WriteableFile::close()
{
    os.close();

    if (os.bad())
	pthrow prcserror << "Write failed on " << squote(temp_name)
			<< perror;

    if (real_name) {

	If_fail(Err_rename(temp_name, real_name))
	    pthrow prcserror << "Rename failed on " << squote(real_name)
			    << " to " << squote(temp_name)
			    << perror;

    }

    return NoError;
}

WriteableFile::WriteableFile() :real_name(NULL) { }
ostream& WriteableFile::stream() { return os; }
