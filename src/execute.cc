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
 * $Id: execute.cc 1.10.1.26 Sat, 30 Oct 1999 18:47:14 -0700 jmacd $
 */

extern "C" {
#include <fcntl.h>
}

#include "prcs.h"

extern "C" {
#include <sys/wait.h>
#ifdef HAVE_VFORK_H
#include <vfork.h>
#endif
}

#include "projdesc.h"
#include "checkin.h"
#include "fileent.h"
#include "misc.h"
#include "repository.h"
#include "checkout.h"
#include "vc.h"
#include "system.h"
#include "syscmd.h"

static bool execute_matched(FileEntry*);
static bool execute_matched_match(FileEntry* fe, reg2ex2_t *re);
static void add_fake(ProjectDescriptor *project,
		     const char* name,
		     const char* tag,
		     FileTable *table,
		     FileEntryPtrArray* add_to);
static FileEntryPtrArray* eliminate_unmatched_files(ProjectDescriptor* project);
static void maybe_add_subdirs_file(FileEntry* fe, FileTable* table, FileEntryPtrArray* add_to);
static PrVoidError init_regex();
static int compare_fileent(const void* a, const void* b);
static PrVoidError do_execute_all(FileEntryPtrArray*, ProjectDescriptor*);
static PrVoidError do_execute(FileEntryPtrArray*, ProjectDescriptor*);
static PrVoidError do_execute_file(FileEntry* fe, ProjectDescriptor*);
static PrBoolError prepare_arg(FileEntryPtrArray* fe_ptrs,
			       const char* arg,
			       ProjectDescriptor* project,
			       ArgList* args);
static PrProjectDescriptorPtrError get_execute_project();
static PrConstCharPtrError make_execute_temp_file(FileEntry* fe, ProjectDescriptor* project);
static PrIntError exec_command(ArgList* args);
static PrVoidError open_exec_file(FileEntry* fe, ProjectDescriptor* project);
static PrVoidError close_exec_file(FileEntry* fe);

static reg2ex2_t *match_regex;
static reg2ex2_t *not_match_regex;

static bool any_failures;
static int current_file_descriptor;
static FILE* current_co_stream;

PrPrcsExitStatusError execute_command()
{
    any_failures = false;

    Return_if_fail(init_regex());

    ProjectDescriptor *project;

    Return_if_fail(project << get_execute_project());

    eliminate_unnamed_files(project);

    Return_if_fail(warn_unused_files(false));

    FileEntryPtrArray *all_files = eliminate_unmatched_files(project);

    if (cmd_diff_options_count < 1) {
	/* @@@ Undocumented, no command, just print all names. */
	kill_prefix(prcsoutput);

	foreach(fe_ptr, all_files, FileEntryPtrArray::ArrayIterator)
	    prcsoutput << (*fe_ptr)->working_path() << prcsendl;
    } else if (option_all_files) {
	Return_if_fail(do_execute_all(all_files, project));
    } else {
	Return_if_fail(do_execute(all_files, project));
    }

    return ExitSuccess;
}

static PrProjectDescriptorPtrError get_execute_project()
{
    ProjectDescriptor *project;

    if (!option_version_present) {
	Return_if_fail(project << read_project_file(cmd_root_project_full_name,
						    cmd_root_project_file_path,
						    true,
						    KeepNothing));

	prcsinfo << "Executing command for working project" << dotendl;
    } else {
	RepEntry* rep_entry;
	ProjectVersionData* version_data;

	Return_if_fail(rep_entry << Rep_init_repository_entry(cmd_root_project_name,
							      false, false, true));

	Return_if_fail(version_data << resolve_version(cmd_version_specifier_major,
						       cmd_version_specifier_minor,
						       cmd_root_project_full_name,
						       cmd_root_project_file_path,
						       NULL,
						       rep_entry));

	if (version_data->prcs_minor_int() == 0) {
	    Return_if_fail(project << checkout_create_empty_prj_file(temp_file_1,
								     cmd_root_project_full_name,
								     version_data->prcs_major(),
								     KeepNothing));
	} else {
	    Return_if_fail(project << rep_entry ->
			   checkout_create_prj_file(temp_file_1,
						    cmd_root_project_full_name,
						    version_data->rcs_version(),
						    KeepNothing));
	}

	project->repository_entry(rep_entry);

	prcsinfo << "Executing command for version "
		 << version_data << dotendl;
    }

    return project;
}


static PrVoidError init_regex()
{
    if (option_match_file) {
	match_regex = new reg2ex2_t;
	Return_if_fail(prcs_compile_regex(option_match_file_pattern,
					  match_regex));
    }

    if (option_not_match_file) {
	not_match_regex = new reg2ex2_t;
	Return_if_fail(prcs_compile_regex(option_not_match_file_pattern,
					  not_match_regex));
    }

    return NoError;
}

static bool execute_matched_match(FileEntry* fe, reg2ex2_t *re)
{
    if (prcs_regex_matches(fe->working_path(), re))
	return true;

    return fe->file_attrs()->regex_matches(re);
}

static bool execute_matched(FileEntry* fe)
{
    if ( option_match_file && !execute_matched_match(fe, match_regex)) {

	if (option_long_format)
	    prcsinfo << "Match failed for " << squote(fe->working_path()) << dotendl;

	return false;
    }

    if (option_not_match_file && execute_matched_match(fe, not_match_regex)) {

	if (option_long_format)
	    prcsinfo << "Match failed for " << squote(fe->working_path()) << dotendl;

	return false;
    }

    return true;
}

static FileEntryPtrArray* eliminate_unmatched_files(ProjectDescriptor* project)
{
    FileTable table(pathname_hash, pathname_equal);
    FileEntryPtrArray* all_files = new FileEntryPtrArray;

    if (cmd_prj_given_as_file)
      add_fake (project,
		project->project_file_path(),
		":project-file",
		&table,
		all_files);

    foreach_fileent(fe_ptr, project) {
	FileEntry* fe = *fe_ptr;

	/* This implies that :implicit-directory files exist even when
	 * the files in them don't.  Otherwise, there is no other way
	 * to get only the directories (because the real files in them
	 * won't match. */
	table.insert (fe->working_path(), fe /*bogus but not used*/);

	if (!fe->on_command_line() || !execute_matched(fe))
	    continue;

	all_files->append (fe);

	maybe_add_subdirs_file(*fe_ptr, &table, all_files);
    }

    all_files->sort(compare_fileent);

    return all_files;
}

static void maybe_add_subdirs_file(FileEntry* fe, FileTable* table, FileEntryPtrArray* add_to)
{
    char dir[MAXPATHLEN];
    char* name = dir;

    strncpy(dir, fe->working_path(), MAXPATHLEN);
    dir[MAXPATHLEN - 1] = 0;

    while(*name != '\0') {

	while(*name != '/' && *name != '\0') { name += 1; }

	if(*name == '\0')
	    break;

	*name = '\0';

	if (dir[0] && !table->isdefined(dir)) {
	    add_fake(fe->project(), dir, ":implicit-directory", table, add_to);
	}

	*name = '/';

	while(*name == '/') { name += 1; }
    }
}

static void add_fake(ProjectDescriptor *project,
		     const char* name,
		     const char* tag,
		     FileTable *table,
		     FileEntryPtrArray* add_to)
{
    Dstring *wp = new Dstring (name);
    Dstring *nametag = new Dstring (tag); /* leak */
    DstringPtrArray tags;
    tags.append (nametag);

    const PrcsAttrs *attrs;

    ListMarker ent_marker;

    If_fail(attrs << project->intern_attrs (&tags, 0, name, false))
      ASSERT (false, "unless this attr is bogus...");

    FileEntry* fe = new FileEntry (wp, ent_marker, attrs, NULL, NULL, NULL, 0666);

    if (!execute_matched (fe)) {
	delete fe;
	return;
    }

    table->insert (*wp, fe);
    add_to->append (fe);
}

static int compare_fileent(const void* a, const void* b)
{
    const FileEntry* fe1 = *((FileEntry**)a);
    const FileEntry* fe2 = *((FileEntry**)b);

    const char* path1 = fe1->working_path();
    const char* path2 = fe2->working_path();

    while (*path1 && *path2 && *path1 == *path2) { path1 += 1; path2 += 1; }

    if (!*path1 && *path2 == '/')
	return 2 * !option_preorder - 1;

    if (!*path2 && *path1 == '/')
	return 2 * option_preorder - 1;

    bool isdir1 = fe1->file_type() == Directory || strchr(path1, '/');
    bool isdir2 = fe2->file_type() == Directory || strchr(path2, '/');

    if (isdir1 == isdir2)
	return strcmp(path1, path2);

    return 2 * (option_preorder ? isdir1 : isdir2) - 1;
}

static PrVoidError do_execute_all(FileEntryPtrArray* all_files,
				  ProjectDescriptor* project)
{
    ArgList* args = new ArgList;

    for(int i = 0; i < cmd_diff_options_count; i += 1) {
	bool skip;

	Return_if_fail(skip << prepare_arg(all_files, cmd_diff_options_given[i], project, args));

	if (skip)
	    pthrow prcserror << "No command could be built" << dotendl;
    }

    if (option_long_format) {
	prcsinfo << "Execute command: ";

	foreach(arg_ptr, args, ArgList::ArrayIterator)
	    prcsinfo << (*arg_ptr) << ' ';

	prcsinfo << prcsendl;
    }

    if (!option_report_actions) {
	int status;

	Return_if_fail(status << exec_command(args));

	if (option_long_format) {
	    if (WIFEXITED(status))
		prcsinfo << "Command exited with value " << WEXITSTATUS(status) << prcsendl;
	    else
		prcsinfo << "Command terminated with signal " << WTERMSIG(status) << prcsendl;
	}

	if (WIFSIGNALED(status))
	    any_failures = true;
    }

    foreach (arg_ptr, args, ArgList::ArrayIterator)
	delete *arg_ptr;

    delete args;

    return NoError;
}

static PrVoidError do_execute(FileEntryPtrArray* files, ProjectDescriptor* project)
{
    foreach(fe_ptr, files, FileEntryPtrArray::ArrayIterator) {
	Return_if_fail(do_execute_file(*fe_ptr, project));

	if (project->repository_entry())
	  project->repository_entry()->Rep_clear_compressed_cache();
    }

    return NoError;
}

static PrVoidError do_execute_file(FileEntry* fe, ProjectDescriptor* project)
{
    ArgList* args = new ArgList;

    current_file_descriptor = -1;
    current_co_stream = NULL;

    if (option_pipe && fe->file_type() != RealFile) {
	if (option_long_format)
	    prcsinfo << "Skipping file " << squote(fe->working_path()) << dotendl;

	return NoError;
    }

    If_fail(open_exec_file(fe, project)) {
	if (option_long_format)
	    prcsinfo << "Skipping file " << squote(fe->working_path()) << dotendl;
	return NoError;
    }

    FileEntryPtrArray fake_array;

    for(int i = 0; i < cmd_diff_options_count; i += 1) {
	bool skip;
	fake_array.truncate(0);
	fake_array.append(fe);

	Return_if_fail(skip << prepare_arg(&fake_array, cmd_diff_options_given[i], project, args));

	if (skip) {
	    if (option_long_format)
		prcsinfo << "Could not build command for file "
			 << squote(fe->working_path()) << dotendl;

	    goto outahere;
	}
    }

    if (option_long_format) {
	prcsinfo << "Execute command: ";

	foreach(arg_ptr, args, ArgList::ArrayIterator)
	    prcsinfo << (*arg_ptr) << ' ';

	prcsinfo << prcsendl;
    }

    if (!option_report_actions) {
	int status;

	Return_if_fail(status << exec_command(args));

	if (option_long_format) {
	    if (WIFEXITED(status))
		prcsinfo << "Command exited with value " << WEXITSTATUS(status) << prcsendl;
	    else
		prcsinfo << "Command terminated with signal " << WTERMSIG(status) << prcsendl;
	}

	if (WIFSIGNALED(status))
	    any_failures = true;
    }

    Return_if_fail(close_exec_file(fe));

outahere:

    foreach (arg_ptr, args, ArgList::ArrayIterator)
	delete *arg_ptr;

    delete args;

    return NoError;
}

static void
append_name (Dstring* n, FileEntry* fe, const char* arg)
{
  const char* path = fe->working_path();

  if (strncmp(arg, "}", 1) == 0)
    n->append (path);
  else if (strncmp(arg, "base}", 5) == 0)
    n->append (strip_leading_path (path));
  else if (strncmp(arg, "dir}", 4) == 0)
    {
      const char* last_slash = strrchr (path, '/');

      if (last_slash)
	n->append (path, last_slash - path);
      else
	n->append (".");
    }
  else
    abort ();
}

static PrBoolError
prepare_arg(FileEntryPtrArray* fe_ptrs,
	    const char* arg,
	    ProjectDescriptor* project,
	    ArgList* args)
{
  char c;

  Dstring this_arg;

  while ((c = *arg++) != 0)
    {
      if (c == '{')
	{
	  if (strncmp(arg, "}", 1) == 0 ||
	      strncmp(arg, "base}", 5) == 0 ||
	      strncmp(arg, "dir}", 4) == 0)
	    {
	      if (fe_ptrs->length() == 0)
		return true;

	      append_name (& this_arg, fe_ptrs->index (0), arg);

	      for (int i = 1; i < fe_ptrs->length(); i += 1)
		{
		  args->append(p_strdup(this_arg));
		  this_arg.truncate(0);
		  append_name (& this_arg, fe_ptrs->index (i), arg);
		}

	      arg += strchr (arg, '}') - arg + 1;
	    }
	  else if (strncmp(arg, "file}", 5) == 0)
	    {
	      const char* name;
	      bool breakit = false;
	      bool any = false;
	      arg += 5;

	      if (fe_ptrs->length() == 0)
		return true;

	      for(int i = 0; i < fe_ptrs->length(); i += 1)
		{

		  if (fe_ptrs->index(i)->file_type() != RealFile)
		    continue;

		  any = true;

		  Return_if_fail(name << make_execute_temp_file(fe_ptrs->index(i), project));

		  if (breakit)
		    {
		      args->append(p_strdup(this_arg));
		      this_arg.truncate(0);
		    }

		  this_arg.append(name);

		  breakit = true;
		}

	      if (!any) /* skip it */
		return true;

	    }
	  else if (strncmp(arg, "options}", 8) == 0)
	    {
	      arg += 8;

	      if (fe_ptrs->length() == 0)
		return true;

	      fe_ptrs->index(0)->file_attrs()->print_to_string(&this_arg, false);

	      for(int i = 1; i < fe_ptrs->length(); i += 1)
		{
		  args->append(p_strdup(this_arg));
		  this_arg.truncate(0);
		  fe_ptrs->index(i)->file_attrs()->print_to_string(&this_arg, false);
		}

	    }
	  else
	    {
	      this_arg.append (c);
	    }
	}
      else
	{
	  this_arg.append(c);
	}
    }

  args->append(p_strdup(this_arg));

  return false;
}

static PrIntError exec_command(ArgList* args)
{
    int status, pid;

    if((pid = vfork()) < 0)
	pthrow prcserror << "Fork failed" << perror;

    if (pid == 0) {
	const char *const* argv = args->cast();

	if (current_file_descriptor >= 0)
	    dup2(current_file_descriptor, STDIN_FILENO);

	execvp(argv[0], (char * const*)argv);
	abort_child(argv[0]);
    }

    If_fail(status << Err_waitpid_nostart(pid))
	pthrow prcserror << "Waitpid failed on pid " << pid << perror;

    return status;
}

static PrVoidError open_exec_file(FileEntry* fe, ProjectDescriptor* project)
{
    if (!option_pipe)
	/* nothing */;
    else if (fe->file_type() == RealFile && !fe->descriptor_name ()) {
        /* special case for project file */
        If_fail(current_file_descriptor << Err_open(temp_file_1, O_RDONLY))
	    pthrow prcserror << "Open failed on " << squote(temp_file_1) << perror;
    } else if (option_version_present) {
        Return_if_fail(fe->initialize_descriptor(project->repository_entry(),
						 false, false));

	FILE* cofile;

	Return_if_fail(cofile << VC_checkout_stream(fe->descriptor_version_number(),
						    fe->full_descriptor_name()));

	current_co_stream = cofile;
        current_file_descriptor = fileno(cofile);
    } else {
        If_fail(current_file_descriptor << Err_open(fe->working_path(), O_RDONLY))
	    pthrow prcserror << "Open failed on " << squote(fe->working_path()) << perror;
    }

    return NoError;
}

static PrVoidError close_exec_file(FileEntry* fe)
{
    if (!option_pipe)
	/* nothing */;
    else if (fe->file_type() == RealFile && !fe->descriptor_name ()) {
        /* special case for project file */
	close(current_file_descriptor);
    } else if (option_version_present)
	Return_if_fail(VC_close_checkout_stream(current_co_stream,
						fe->descriptor_version_number(),
	 					fe->full_descriptor_name()));
    else
	close(current_file_descriptor);

    return NoError;
}

static PrConstCharPtrError make_execute_temp_file(FileEntry* fe, ProjectDescriptor* project)
{
    const char* lastdot = strrchr(fe->working_path(), '.');
    const char* newname = make_temp_file((lastdot && !strchr(lastdot, '/')) ? lastdot : "");

    if (option_version_present) {
        if (fe->file_type() == RealFile && !fe->descriptor_name ()) {
	    /* special case for project file */
	  return temp_file_1;
	}

	Return_if_fail(fe->initialize_descriptor(project->repository_entry(),
						 false,
						 false));

	Return_if_fail(VC_checkout_file(newname,
					fe->descriptor_version_number(),
					fe->full_descriptor_name()));
    } else {
	Return_if_fail(fs_copy_filename(fe->working_path(), newname));
    }

    return newname;
}
