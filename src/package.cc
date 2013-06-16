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
 * $Id: package.cc 1.15.1.2.1.8.1.8.1.7.1.12 Wed, 06 Feb 2002 20:57:16 -0800 jmacd $
 */

extern "C" {
#include <sys/wait.h>
#include <signal.h>
}

#include "prcs.h"
#include "prcsdir.h"
#include "syscmd.h"
#include "misc.h"
#include "repository.h"
#include "projdesc.h"
#include "system.h"
#include "vc.h"
#include "lock.h"

const char gzip_magic_1        = '\037';
const char gzip_magic_2        = '\213';
const char magic_number_1      = '\200'; /* tar file contains PROJECT/files */
const char magic_number_2      = '\352'; /* versions 1.0.x and earlier. */
const char new_magic_number_1  = '\201'; /* tar file contains files, allows easy renaming */
const char new_magic_number_2  = '\353'; /* versions 1.1.0 and later. */
const int header_length        = 256;

static PrVoidError package_project(const char* project_name,
				   const char* package_file,
				   bool        obtain_lock,
				   bool        compress_it);

static PrVoidError package_cleanup_handler(void* data, bool /*signaled*/)
{
    ((AdvisoryLock*)data)->unlock();

    return NoError;
}

/* This class makes it so that package, unpackage, etc, work on broken
 * repositories, in case of trouble. */
class PseudoRepEntry {
public:
    PseudoRepEntry (const char* name);

    PrVoidError lock ();
    PrVoidError exists ();
    PrVoidError remove (bool replaced);

    Dstring rep_path;
    Dstring ent_path;

private:
    Dstring _name;
    AdvisoryLock *_lock;
};

PseudoRepEntry::PseudoRepEntry (const char* name)
    :_name(name), _lock (NULL)
{
    const char* path;

    path << Rep_guess_repository_path();
    /* Can't fail after call to Rep_init_repository() */

    rep_path.assign (path);

    ent_path.assign (rep_path);
    ent_path.append ('/');
    ent_path.append (name);
}

PrVoidError PseudoRepEntry::lock()
{
    Dstring lock_path (rep_path);

    lock_path.append (prcs_lock_dir);
    lock_path.append ('/');
    lock_path.append (_name);

    _lock = new AdvisoryLock (lock_path);

    Return_if_fail (obtain_lock(_lock, true));

    install_cleanup_handler(package_cleanup_handler, _lock);

    return NoError;
}

PrVoidError PseudoRepEntry::exists()
{
    if(!fs_is_directory(ent_path)) {
	pthrow prcserror << "Repository entry " << squote(_name) << " does not exist" << dotendl;
    }

    return NoError;
}

PrVoidError PseudoRepEntry::remove(bool being_replaced)
{
    if(fs_is_directory(ent_path)) {
	if (being_replaced) {
	    prcsquery << "Project " << squote(_name) << " already exists.  ";
	} else {
	    prcsquery << "Remove project " << squote(_name) << ".  ";
	}

	prcsquery << force("Deleting")
		  << report("Delete")
		  << optfail('n')
		  << defopt('y', "Delete project from repository")
		  << query("Are you sure");

	Return_if_fail(prcsquery.result());

	if(option_report_actions)
	    return NoError;

	If_fail(fs_nuke_file(ent_path)) {
	    pthrow prcserror << "Failed removing old repository entry "
			     << squote(ent_path) << dotendl;
	}
    } else if (! being_replaced) {
	prcswarning << "Project does not exist: " << squote(_name) << dotendl;
    }

    return NoError;
}

PrPrcsExitStatusError package_command()
{
    Return_if_fail(package_project(cmd_root_project_name,
				   cmd_filenames_given[0],
				   true,
				   option_package_compress));

    return ExitSuccess;
}

static PrVoidError package_project(const char* project_name,
				   const char* package_file,
				   bool obtain_lock,
				   bool compress)
{
    ArgList *args;
    int outfd = STDOUT_FILENO;
    char buf[header_length];
#ifdef HAVE_FORK
    int pipefd[2];
#endif
    int pid = -1, status;
    PseudoRepEntry *prep_entry;

    Return_if_fail(Rep_init_repository());

    prep_entry = new PseudoRepEntry (project_name);

    Return_if_fail (prep_entry->exists());

    if (obtain_lock)
	Return_if_fail (prep_entry->lock());

    Return_if_fail(args << tar_command.new_arg_list());

    if(strcmp(package_file, "-") != 0) {
	If_fail(outfd << Err_open(package_file, O_WRONLY|O_CREAT|O_TRUNC, 0666))
	    pthrow prcserror << "Can't open " << squote(package_file)
		            << " for writing" << perror;
    }

    args->append("cf");
    args->append("-");
    args->append("-C");
    args->append(prep_entry->ent_path);
    args->append(".");

    Return_if_fail(tar_command.open(true, false));

    memset(buf, 0, header_length);
    buf[0] = new_magic_number_1;
    buf[1] = new_magic_number_2;
    sprintf(buf + 2, "%s", project_name);

    if(compress) {
#ifdef HAVE_FORK
	Return_if_fail(gzip_command.init());

	if(pipe(pipefd) != 0) {
	    pthrow prcserror << "Pipe failed, can't compress data" << perror;
	} else if((pid = fork()) < 0) {
	    pthrow prcserror << "Fork failed, can't compress data" << perror;
	} else if (pid == 0) {
	    dup2(pipefd[PIPE_READ_FD], STDIN_FILENO);
	    dup2(outfd, STDOUT_FILENO);
	    close(pipefd[PIPE_READ_FD]);
	    close(pipefd[PIPE_WRITE_FD]);

	    execl(gzip_command.path(), "-", "-f", NULL);
	    abort_child(gzip_command.path());
	} else {
	    dup2(pipefd[PIPE_WRITE_FD], outfd);
	    close(pipefd[PIPE_READ_FD]);
	    close(pipefd[PIPE_WRITE_FD]);
	}
#else
	pthrow prcserror << "Not compiled with fork(), cannot compress package data" << dotendl;
#endif
    }

    If_fail(Err_write(outfd, buf, header_length))
	pthrow prcserror << "Write failed writing package file" << perror;

    If_fail(write_fds(fileno(tar_command.standard_out()), outfd))
	pthrow prcserror << "Write failed writing package file" << perror;

    If_fail(Err_close(outfd))
	pthrow prcserror << "Write failed writing package file" << perror;

    Return_if_fail_if_ne(tar_command.close(), 0) {
	unlink(package_file);
	pthrow prcserror << "System command tar failed" << dotendl;
    }

    if(compress) {
	If_fail(status << Err_waitpid(pid))
	    pthrow prcserror << "Waitpid failed on pid " << pid << perror;

	if(!WIFEXITED(status) || WEXITSTATUS(status) != 0)
	    pthrow prcserror << "System command gzip failed" << dotendl;
    }

    return NoError;
}

static PrVoidError invalid()
{
    pthrow prcserror << squote(cmd_root_project_name)
		    << " is not a valid package file" << dotendl;
}

#ifdef HAVE_FORK
static PrVoidError feed_pipe(const char* header_buf, int from_fd, int to_fd)
{
    If_fail(Err_write(to_fd, header_buf, header_length))
	pthrow prcserror << "Write failed writing to gzip pipe" << perror;

    If_fail(write_fds(from_fd, to_fd))
	pthrow prcserror << "Write failed writing to gzip pipe" << perror;

    return NoError;
}
#endif

PrPrcsExitStatusError unpackage_command()
{
    ArgList *args;
#ifdef HAVE_FORK
    int zipoutpipefd[2], zipinpipefd[2];
#endif
    int fd, zippid = 0, writepid = 0;
    Dstring projname;
    char buf[header_length];
    bool new_format = false;
    bool do_rename = false;
    bool uncompress;

    if(strcmp(cmd_root_project_name, "-") != 0) {
	If_fail(fd << Err_open(cmd_root_project_full_name, O_RDONLY))
	    pthrow prcserror << "Open failed on package file" << perror;
    } else {
	fd = STDIN_FILENO;
    }

    if (cmd_filenames_count > 1)
	pthrow prcserror << "Too many file-or-dir options" << dotendl;

    If_fail(Err_read_expect(fd, buf, header_length)) {
	if (errno)
	    pthrow prcserror << "Read error on package file" << perror;
	else
	    return invalid();
    }

    if(buf[0] == magic_number_1 && buf[1] == magic_number_2) {
	uncompress = false;
    } else if(buf[0] == new_magic_number_1 && buf[1] == new_magic_number_2) {
	uncompress = false;
	new_format = true;
    } else if(buf[0] == gzip_magic_1 && buf[1] == gzip_magic_2) {
	uncompress = true;
    } else {
	return invalid();
    }

    if(uncompress) {
#ifdef HAVE_FORK
	Return_if_fail(gzip_command.init());

	if(pipe(zipoutpipefd) != 0 || pipe(zipinpipefd) != 0) {
	    pthrow prcserror << "Pipe failed, can't compress data" << perror;
	} else if((zippid = fork()) < 0) {
	    pthrow prcserror << "Fork failed, can't compress data" << perror;
	} else if (zippid == 0) {
	    dup2(zipoutpipefd[PIPE_WRITE_FD], STDOUT_FILENO);
	    dup2(zipinpipefd[PIPE_READ_FD], STDIN_FILENO);

	    close(zipoutpipefd[PIPE_READ_FD]);
	    close(zipoutpipefd[PIPE_WRITE_FD]);
	    close(zipinpipefd[PIPE_READ_FD]);
	    close(zipinpipefd[PIPE_WRITE_FD]);

	    execl(gzip_command.path(), "-", "-d","-f", NULL);
	    abort_child(gzip_command.path());
	} else if((writepid = fork()) < 0) {
	    pthrow prcserror << "Fork failed, can't compress data" << perror;
	} else if(writepid == 0) {
	    int exit_val = 0;

            close(zipinpipefd[PIPE_READ_FD]);

	    If_fail(feed_pipe(buf, fd, zipinpipefd[PIPE_WRITE_FD]))
		exit_val = 1;

	    close(zipinpipefd[PIPE_WRITE_FD]);

	    _exit(exit_val);
	} else {

	    dup2(zipoutpipefd[PIPE_READ_FD], fd);
	    close(zipoutpipefd[PIPE_READ_FD]);
	    close(zipoutpipefd[PIPE_WRITE_FD]);
	    close(zipinpipefd[PIPE_READ_FD]);
	    close(zipinpipefd[PIPE_WRITE_FD]);

	    If_fail(Err_read_expect(fd, buf, header_length))
		return invalid();

	    if (buf[0] == magic_number_1 || buf[1] == magic_number_2) {

	    } else if (buf[0] == new_magic_number_1 || buf[1] == new_magic_number_2) {
		new_format = true;
	    } else {
		return invalid();
	    }
	}
#else
	pthrow prcserror << "Not compiled with fork(), cannot uncompress package data" << dotendl;
#endif
    }

    if (cmd_filenames_count == 1) {
	if (!new_format)
	    pthrow prcserror << "Cannot rename project in package file created by PRCS versions "
		"older than 1.1.0" << dotendl;

	if (strcmp (projname, buf + 2) != 0)
	    do_rename = true;

	projname.assign (cmd_filenames_given[0]);

	prcsinfo << "Unpackage project " << squote(projname)
		 << ", formerly named " << squote(buf + 2) << dotendl;
    } else {
	projname.assign (buf + 2);

	prcsinfo << "Unpackage project " << squote(projname) << dotendl;
    }

    if (VC_check_token_match(projname, "label") <= 0)
	pthrow prcserror << "Illegal project name "
			<< squote(projname) << dotendl;

    Return_if_fail(Rep_init_repository());

    PseudoRepEntry *prep_entry = new PseudoRepEntry (projname);

    Return_if_fail (prep_entry->lock());

    Return_if_fail (prep_entry->remove(true));

    Return_if_fail(args << tar_command.new_arg_list());

    if(option_report_actions)
	return ExitSuccess;

    const char* change_to_dir = new_format ? prep_entry->ent_path : prep_entry->rep_path;

    if (new_format) {
	If_fail (Err_mkdir (change_to_dir, 0777))
	    pthrow prcserror << "Mkdir " << squote(change_to_dir) << " failed" << perror;
    }

    If_fail(change_cwd(change_to_dir))
	/* Sun's tar doesn't accept -C for 'x' operations, pretty lame huh. */
	pthrow prcserror << "Chdir " << squote(change_to_dir) << " failed" << perror;

    args->append("-xpf");
    args->append("-");

    Return_if_fail_if_ne(tar_command.open_filein(fd, false), 0)
	pthrow prcserror << "System command tar failed" << dotendl;

    int status;

    if(uncompress) {
	If_fail(status << Err_waitpid(writepid))
	    pthrow prcserror << "Waitpid failed on pid " << writepid << perror;

	if(!WIFEXITED(status) || WEXITSTATUS(status) != 0)
	    pthrow prcserror << "Pipe reader exited with non-zero status" << dotendl;

	If_fail(status << Err_waitpid(zippid))
	    pthrow prcserror << "Waitpid failed on pid " << zippid << perror;

	if(WIFSIGNALED(status) && WTERMSIG(status) == SIGPIPE)
	    pthrow prcserror << "Child gzip process was terminated with SIGPIPE" << dotendl;

	if(!WIFEXITED(status) || WEXITSTATUS(status) != 0)
	    pthrow prcserror << "Child gzip process returns non-zero status" << dotendl;
    }

    if (do_rename) {
	Dstring old_name (buf+2);

	old_name.append (".prj,v");
	projname.append (".prj,v");

	If_fail (Err_rename (old_name, projname))
	    pthrow prcserror << "Rename " << squote(old_name) << " to " << squote(projname)
			    << " failed" << perror;
    }

    return ExitSuccess;
}

PrVoidError save_package()
{
    Dstring new_package;

    new_package.append(cmd_root_project_name);
    new_package.append(".pkg");

    prcsquery << "Enter a package name"
	      << definput(new_package)
	      << string_query("");

    const char* ans;

    Return_if_fail(ans << prcsquery.string_result());

    prcsquery << "Packaging repository.  "
	      << force("Compressing")
	      << report("Compress")
	      << option('n', "Don't compress the package")
	      << defopt('y', "Compress the package")
	      << query("Compress");

    char c;

    Return_if_fail(c << prcsquery.result());

    Return_if_fail(package_project(cmd_root_project_name, ans, false, c == 'y'));

    return NoError;
}

PrPrcsExitStatusError admin_pdelete_command()
{
    Return_if_fail(Rep_init_repository());

    PseudoRepEntry *prep_entry = new PseudoRepEntry (cmd_root_project_name);

    Return_if_fail (prep_entry->lock());

    Return_if_fail (prep_entry->remove (false));

    return ExitSuccess;
}

PrPrcsExitStatusError admin_pinfo_command()
{
    Return_if_fail(Rep_init_repository());

    const char* path;

    path << Rep_guess_repository_path ();
    /* Can't fail after Rep_init_repository */

    Dir dir (path);

    prcsinfo << "Repository contains:" << prcsendl;

    kill_prefix(prcsoutput);

    foreach (ent_ptr, dir, Dir::FullDirIterator)
      {
	const char* fent = *ent_ptr;

	if (! fs_is_directory (fent))
	  continue;

	const char* ent = strip_leading_path (fent);

	if (strcmp (ent, ".locks") == 0)
	  continue;

	prcsoutput << ent << prcsendl;
      }

    if (!dir.OK())
      pthrow prcserror << "Error reading repository directory " << squote (path) << perror;

    return ExitSuccess;
}

PrPrcsExitStatusError admin_prename_command()
{
    Return_if_fail(Rep_init_repository());

    PseudoRepEntry *old_entry = new PseudoRepEntry (cmd_root_project_name);
    PseudoRepEntry *new_entry = new PseudoRepEntry (cmd_filenames_given[0]);

    Return_if_fail (old_entry->exists());
    Return_if_fail (old_entry->lock());

    Return_if_fail (new_entry->remove(true));
    /* locking here doesn't really work.... */

    /* All it takes is two rename() calls */
    Dstring old_prj_name (old_entry->ent_path);
    Dstring new_prj_name (old_entry->ent_path);

    old_prj_name.append ('/');
    old_prj_name.append (cmd_root_project_name);
    old_prj_name.append (".prj,v");

    new_prj_name.append ('/');
    new_prj_name.append (cmd_filenames_given[0]);
    new_prj_name.append (".prj,v");

    if (VC_check_token_match(cmd_filenames_given[0], "label") <= 0)
	pthrow prcserror << "Illegal project name "
			<< squote(cmd_filenames_given[0]) << dotendl;

    prcsinfo << "Renaming project " << cmd_root_project_name
	     << " to " << cmd_filenames_given[0] << dotendl;

    if (option_report_actions)
	return ExitSuccess;

    If_fail (Err_rename (old_prj_name, new_prj_name))
	pthrow prcserror << "Rename " << squote(old_prj_name)
			<< " to " << squote(new_prj_name) << " failed" << perror;

    If_fail (Err_rename (old_entry->ent_path, new_entry->ent_path))
	pthrow prcserror << "Rename " << squote(old_entry->ent_path)
			<< " to " << squote(new_entry->ent_path) << " failed" << perror;

    return ExitSuccess;
}
