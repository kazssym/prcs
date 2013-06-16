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
 * $Id: syscmd.cc 1.5.1.19.1.10.1.18 Wed, 14 Oct 1998 20:50:34 -0700 jmacd $
 */


#include "prcs.h"

extern "C" {
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
/* Everything below this is losing stuff for IRIX, AIX, and HP-UX's select() */
#if HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#if HAVE_BSTRING_H
#include <bstring.h>
#endif
#ifdef HAVE_SYS_STREAM_H
#include <sys/stream.h>
#endif
#ifdef HAVE_VFORK_H
#include <vfork.h>
#endif
#ifdef __BEOS__
/* FD_ZERO and friends are defined here */
#include <socket.h>
#endif
}

#if NO_FD_SET
typedef struct {
  int set[32]; /* sizeof(int)*32*8 files */
} FD_SET_TYPE;
#else
typedef fd_set FD_SET_TYPE;
#endif
/* All the way down to here. */

#include "syscmd.h"
#include "misc.h"
#include "system.h"
#include "memseg.h"

/* Keep this (GRIBBLE=FOO) here, says PNH!  It works around a problem
 * with Solaris 2.6, GNU ld, getenv, getpwuid, static linking, and
 * perhaps a few other bogons.  */

char *const null_environment[] = { "GRIBBLE=FOO", NULL };

SystemCommand* sys_cmd_by_name (const char* name)
{
    static CommandTable cmd_table;

    SystemCommand** lu = cmd_table.lookup (name);

    if (lu)
	return *lu;

    SystemCommand *cmd = new SystemCommand (name, "PATH");

    cmd_table.insert (name, cmd);

    return cmd;
}

SystemCommand::SystemCommand(const char* name0, const char* path0)
    :name(name0), path_env(path0), one_pid(0)
{
    pr.standard_err = NULL;
    pr.standard_out = NULL;
}

PrVoidError SystemCommand::init()
{
    if (fp.length() == 0) {

	const char* user_path0 = get_environ_var (path_env);

	if( user_path0 ) {

	    const char* user_path = user_path0;

	    while(user_path && user_path[0]) {
		const char* first_colon = strchr(user_path, ':');
		const char* this_path;
		int this_path_len;

		if(first_colon) {
		    this_path = user_path;
		    this_path_len = first_colon - this_path;
		    user_path = first_colon + 1;
		} else {
		    this_path = user_path;
		    this_path_len = strlen(user_path);
		    user_path = NULL;
		}

		fp.append(this_path, this_path_len);

		if(this_path[this_path_len - 1] != '/')
		    fp.append('/');

		fp.append(strip_leading_path(name));

		if(access(fp, F_OK | X_OK) >= 0)
		    return NoError;

		if(errno != ENOENT)
		    prcswarning << "Can't execute " << squote(fp) << perror;

		fp.truncate(0);
	    }
	}

	fp.assign(name);

	if(access(name, F_OK | X_OK) >= 0)
	    return NoError;

	if(errno != ENOENT)
	    pthrow prcserror << "Can't execute system command " << squote(name) << perror;
	else if(user_path0)
	    pthrow prcserror << "System command "
			    << squote(strip_leading_path(name)) << " not in $"
			    << path_env << " nor the default location " << squote(name) << dotendl;
	else
	    pthrow prcserror << "System command " << squote(name) << " does not exist" << dotendl;

    }

    return NoError;
}

PrVoidError SystemCommand::open(bool out, bool err)
{
    Return_if_fail(one_pid << make_pipe_out(argl, &pr, out, err));

    return NoError;
}

PrVoidError SystemCommand::open_delayed(DelayedJob::DelayNotifyFunction notf,
					void* data,
					bool out,
					bool err)
{
    int out_fd = -1;
    int err_fd = -1;
    pid_t pid;

    Return_if_fail(DelayedJob::delay_wait());

    Return_if_fail(pid << make_pipe_out_delay(argl, &out_fd, &err_fd, out, err));

    (void)new DelayedJob(notf, pid, data, name, out_fd, err_fd);

    pr.standard_out = NULL;
    pr.standard_err = NULL;

    return NoError;
}

PrArgListPtrError SystemCommand::new_arg_list()
{
    Return_if_fail(init());

    close();

    argl.append(fp);

    return &argl;
}

FILE* SystemCommand::standard_out() const { return pr.standard_out; }
FILE* SystemCommand::standard_err() const { return pr.standard_err; }

PrExitStatusError SystemCommand::open_stdout()
{
    return make_pipe_stdout(argl);
}

PrExitStatusError SystemCommand::open_filein(int fd, bool cl)
{
    return make_pipe_file_in(fd, argl, cl);
}

const char* SystemCommand::path() const { return fp; }

/* returns PIPE_CLOSE_FAILURE if process did not exit properly due to
 * a call to exit(). otherwise it returns the exit value of the
 * process that was running. */
PrExitStatusError SystemCommand::close()
{
    int ret;

    if (one_pid != 0)
	If_fail(ret << close_pipe(&pr, one_pid))
	    pthrow prcserror << "Child process " << squote(name)
		            << " terminated abnormally" << dotendl;

    pr.standard_err = NULL;
    pr.standard_out = NULL;
    argl.truncate(0);
    one_pid = 0;

    return ret;
}

static int stdinpipe[2];

void abort_child(const char* argv0)
{
    FILE* user_tty = fopen(ctermid(NULL), "w");
    if(user_tty)
	fprintf(user_tty, "prcs: Exec failed: %s: %s\n", argv0, strerror(errno));

    _exit(127);
}

PrPidTError make_pipe_out(const char* const* argv, PipeRec* pr,
			  bool pipeout, bool pipeerr)
{
    int pid, stdpipe[2], errpipe[2];

    if(stdinpipe[0] == 0) {
	if(pipe(stdinpipe) < 0)
	    pthrow prcserror << "Pipe failed" << perror;

	close(stdinpipe[PIPE_WRITE_FD]);
    }

    if(pipeout && pipe(stdpipe) < 0)
	pthrow prcserror << "Pipe failed" << perror;

    if(pipeerr && pipe(errpipe) < 0)
	pthrow prcserror << "Pipe failed" << perror;

    pid = vfork();
    if(pid < 0)
	pthrow prcserror << "Fork failed" << perror;

    if (pid == 0) {
	close(STDIN_FILENO);
	// This had to be added because of a bug in FreeBSD/NetBSD's
	// freopen treatment of ferror..., or a GNU diff3 bug where by
	// it freopens stdin without noticing its stdin is closed.
	dup2(stdinpipe[PIPE_READ_FD], STDIN_FILENO);

	if(pipeout) {
	    dup2(stdpipe[PIPE_WRITE_FD], STDOUT_FILENO);
	    close(stdpipe[PIPE_WRITE_FD]);
	    close(stdpipe[PIPE_READ_FD]);
	}

	if(pipeerr) {
	    dup2(errpipe[PIPE_WRITE_FD], STDERR_FILENO);
	    close(errpipe[PIPE_WRITE_FD]);
	    close(errpipe[PIPE_READ_FD]);
	}

	execve(argv[0], (char* const*)argv, null_environment);
	abort_child(argv[0]);
    }

    if(pipeout) {
	close(stdpipe[PIPE_WRITE_FD]);
	pr->standard_out = fdopen(stdpipe[PIPE_READ_FD], "r");
    }

    if(pipeerr) {
	close(errpipe[PIPE_WRITE_FD]);
	pr->standard_err = fdopen(errpipe[PIPE_READ_FD], "r");
    }

    return pid;
}

PrPidTError make_pipe_out_delay(const char* const* argv,
				int* stdout_fd,
				int* stderr_fd,
				bool pipeout, bool pipeerr)
{
    int pid, stdpipe[2], errpipe[2];

    if(pipeout && pipe(stdpipe) < 0)
	pthrow prcserror << "Pipe failed" << perror;

    if(pipeerr && pipe(errpipe) < 0)
	pthrow prcserror << "Pipe failed" << perror;

    pid = vfork();
    if(pid < 0)
	pthrow prcserror << "Fork failed" << perror;

    if (pid == 0) {
	close(STDIN_FILENO);
	// This had to be added because of a bug in FreeBSD/NetBSD's
	// freopen treatment of ferror..., or a GNU diff3 bug where by
	// it freopens stdin without noticing its stdin is closed.
	dup2(stdinpipe[PIPE_READ_FD], STDIN_FILENO);

	if(pipeout) {
	    dup2(stdpipe[PIPE_WRITE_FD], STDOUT_FILENO);
	    close(stdpipe[PIPE_WRITE_FD]);
	    close(stdpipe[PIPE_READ_FD]);
	}

	if(pipeerr) {
	    dup2(errpipe[PIPE_WRITE_FD], STDERR_FILENO);
	    close(errpipe[PIPE_WRITE_FD]);
	    close(errpipe[PIPE_READ_FD]);
	}

	execve(argv[0], (char* const*)argv, null_environment);
	abort_child(argv[0]);
    }

    if(pipeout) {
	close(stdpipe[PIPE_WRITE_FD]);
	*stdout_fd = stdpipe[PIPE_READ_FD];
    }

    if(pipeerr) {
	close(errpipe[PIPE_WRITE_FD]);
	*stderr_fd = errpipe[PIPE_READ_FD];
    }

    return pid;
}

NprExitStatusError close_pipe(PipeRec *pr, pid_t pid)
{
    int status;

    if(pr->standard_out != NULL) {
	empty_file(pr->standard_out);
	fclose(pr->standard_out);
    }

    if(pr->standard_err != NULL) {
	empty_file(pr->standard_err);
	fclose(pr->standard_err);
    }

    If_fail(status << Err_waitpid(pid))
	return FatalError;

    if(!WIFEXITED(status))
	return NonFatalError;
    else
	return WEXITSTATUS(status);
}

PrExitStatusError make_pipe_file_in(int fd, const char* const* argv, bool close_stdout)
{
    int status, pid;

    if((pid = vfork()) < 0)
	pthrow prcserror << "Fork failed" << perror;

    if (pid == 0) {

	if(close_stdout) {
	    close(STDOUT_FILENO);
	    close(STDERR_FILENO);
	}

	dup2(fd, STDIN_FILENO);

	execve(argv[0], (char * const*)argv, null_environment);
	abort_child(argv[0]);
    }

    If_fail(status << Err_waitpid(pid))
	pthrow prcserror << "Waitpid failed on pid " << pid << perror;

    close(fd);

    if(!WIFEXITED(status))
	if (WIFSIGNALED(status)) {
	    pthrow prcserror << "Process " << squote(argv[0]) << " terminated with signal "
			    << WTERMSIG(status) << dotendl;
	} else {
	    pthrow prcserror << "Process " << squote(argv[0]) << " was stopped with signal "
			    << WSTOPSIG(status) << dotendl;
	}
    else
	return WEXITSTATUS(status);
}

PrExitStatusError make_pipe_stdout(const char* const* argv)
{
    int pid, status;

    if((pid = vfork()) < 0) {
	pthrow prcserror << "Fork failed" << perror;
    } else if (pid == 0) {
	execv(argv[0], (char * const*)argv);
	abort_child(argv[0]);
    }

    If_fail(status << Err_waitpid(pid))
	pthrow prcserror << "Waitpid failed on pid " << pid << perror;

    if(!WIFEXITED(status))
	if (WIFSIGNALED(status)) {
	    pthrow prcserror << "Process " << squote(argv[0]) << " terminated with signal "
			    << WTERMSIG(status) << dotendl;
	} else {
	    pthrow prcserror << "Process " << squote(argv[0]) << " was stopped with signal "
			    << WSTOPSIG(status) << dotendl;
	}
    else
	return WEXITSTATUS(status);
}

/* DelayedJob */

MemorySegmentList *DelayedJob::_free_segments = NULL;
DelayedJobList    *DelayedJob::_outstanding_jobs = NULL;
int                DelayedJob::_outstanding_job_count = 0;

DelayedJob::DelayedJob (DelayNotifyFunction on_completion0,
			pid_t pid,
			void* data,
			const char* name,
			int stdout_fd0,
			int stderr_fd0)
    :_on_completion(on_completion0),
     _stdout_fd(stdout_fd0),
     _stderr_fd(stderr_fd0),
     _stdout_file(NULL),
     _stderr_file(NULL),
     _stdout_eof(false),
     _stderr_eof(false),
     _pid(pid),
     _data(data),
     _name(name)
{
    init_segs();
}

DelayedJob::DelayedJob (DelayNotifyFunction on_completion0,
			pid_t pid,
			void* data,
			const char* name,
			const char* stdout_file,
			const char* stderr_file)
    :_on_completion(on_completion0),
     _stdout_fd(-1),
     _stderr_fd(-1),
     _stdout_file(stdout_file),
     _stderr_file(stderr_file),
     _stdout_eof(false),
     _stderr_eof(false),
     _pid(pid),
     _data(data),
     _name(name)
{
    init_segs();
}

void DelayedJob::init_segs()
{
    DEBUG ("starting a job, outstanding count is " << _outstanding_job_count);

    _outstanding_jobs = new DelayedJobList(this, _outstanding_jobs);
    _outstanding_job_count += 1;

    if (_stdout_fd >= 0 || _stdout_file != NULL) {

	if (_free_segments) {
	    MemorySegmentList* tmp = _free_segments;

	    _free_segments = _free_segments->tail();
	    _out_segment = _free_segments->head();

	    delete tmp;
	} else {
	    _out_segment = new MemorySegment(true, false);
	}

	fcntl(_stdout_fd, F_SETFL, O_NONBLOCK);
	_out_segment->clear_segment();

    } else {
	_stdout_eof = true;
    }

    if (_stderr_fd >= 0 || _stderr_file != NULL) {

	if (_free_segments) {
	    MemorySegmentList* tmp = _free_segments;

	    _free_segments = _free_segments->tail();
	    _err_segment = _free_segments->head();

	    delete tmp;
	} else {
	    _err_segment = new MemorySegment(true, false);
	}

	fcntl(_stderr_fd, F_SETFL, O_NONBLOCK);
	_err_segment->clear_segment();

    } else {
	_stderr_eof = true;
    }
}

#define STARTSYS struct timeval sys_tv; gettimeofday (&sys_tv, NULL);
#define ENDSYS struct timeval sys_end_tv; gettimeofday (&sys_end_tv, NULL); printf ("waited %.4f secs\n", (float)(sys_end_tv.tv_sec - sys_tv.tv_sec) + (float)(sys_end_tv.tv_usec - sys_tv.tv_usec)/1000000.0);

PrVoidError DelayedJob::delay_wait(bool force)
{
    while ((force && _outstanding_job_count > 0) ||
	   option_jobs <= _outstanding_job_count) {
        bool x;

        Return_if_fail(x << wait_some());

	if (x)
	  continue;

	Return_if_fail(read_some());
    }

    return NoError;
}

#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>

PrBoolError DelayedJob::wait_some()
{
    int status;
    pid_t pid;

    while (true) {

#if 1
      If_fail(status << Err_waitpid(-1, &pid, true))
	    pthrow prcserror << "Wait failed" << perror;
#else
      struct rusage ru;

      STARTSYS;
      if ((pid = wait3 (&status, WNOHANG, &ru)) < 0)
	  pthrow prcserror << "Wait failed" << perror;
      ENDSYS;

      printf ("Rusage: %.4f %.4f\n", (float)(ru.ru_utime.tv_sec) + (float)ru.ru_utime.tv_usec/1000000.0,
	      (float)(ru.ru_stime.tv_sec) + (float)ru.ru_stime.tv_usec/1000000.0);
#endif

	if (pid == 0)
	    return false;

	if (WIFSTOPPED(status)) {
	    kill (SIGCONT, pid);
	    continue;
	}

	for (DelayedJobList* p = _outstanding_jobs; p; p = p->tail()) {
	    if (pid == p->head()->_pid) {
		Return_if_fail (p->head()->finish_job(status));
		return true;
	    }
	}
    }
}

PrVoidError DelayedJob::read_some()
{
    FD_SET_TYPE read_fds;
    int nfds = -1;
    int sval;

    FD_ZERO(&read_fds);

    for (DelayedJobList *p = _outstanding_jobs; p; p = p->tail()) {

	if (!p->head()->_stdout_eof) {
	    FD_SET(p->head()->_stdout_fd, &read_fds);
	    if (p->head()->_stdout_fd > nfds)
		nfds = p->head()->_stdout_fd;
	}

	if (!p->head()->_stderr_eof) {
	    FD_SET(p->head()->_stderr_fd, &read_fds);
	    if (p->head()->_stderr_fd > nfds)
		nfds = p->head()->_stderr_fd;
	}
    }

    nfds += 1;

    struct timeval tv;

    tv.tv_sec = 0;
    tv.tv_usec = 10000; /* we expect to be interrupted by SIGCHLD
			 * when a child completes. */

#ifdef __BEOS__
    /* BeOS upto and including R3 does not allow select() on files.
     */
    FD_SET_TYPE save_fds = read_fds;
    sval = select(0, NULL, NULL, NULL, &tv);
    read_fds = save_fds;
#else
    sval = select (nfds, (SELECT_TYPE*)(&read_fds), NULL, NULL, &tv);

    if (sval == 0)
      return NoError;
#endif

    if (sval < 0 && errno == EINTR)
	/* probably SIGCHLD, try waiting. */
	return NoError;

    if (sval < 0)
	pthrow prcserror << "Select failed" << perror;

    for (DelayedJobList *p = _outstanding_jobs; p; p = p->tail()) {

	if (!p->head()->_stdout_eof && FD_ISSET(p->head()->_stdout_fd, &read_fds))
	    Return_if_fail(p->head()->read_one_fd (p->head()->_stdout_fd));

	if (!p->head()->_stderr_eof && FD_ISSET(p->head()->_stderr_fd, &read_fds))
	    Return_if_fail(p->head()->read_one_fd (p->head()->_stderr_fd));
    }

    return NoError;
}

PrVoidError DelayedJob::read_one_fd(int fd)
{
    char buf[8124];
    int readval;
    MemorySegment* seg;

    if (fd == _stdout_fd)
	seg = _out_segment;
    else
	seg = _err_segment;

    while (true) {
	readval = read(fd, buf, 8124);

	if (readval < 0 && errno != EAGAIN && errno != EINTR && errno != EWOULDBLOCK)
	    pthrow prcserror << "Read failed from child process " << squote(_name) << perror;

	if (readval < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
	    /* But select said it was ready!? */
	    return NoError;

	if (readval < 0)
	    /* Got interupted. */
	    continue;

	if (readval == 0) {
	    if (fd == _stdout_fd)
		_stdout_eof = true;
	    else
		_stderr_eof = true;

	    close(fd);

	    return NoError;
	}

	seg->append_segment(buf, readval);
    }
}

PrVoidError DelayedJob::finish_job(int status)
{
    if (!_stdout_eof)
        Return_if_fail(read_one_fd(_stdout_fd));

    if (!_stderr_eof)
        Return_if_fail(read_one_fd(_stderr_fd));

    _outstanding_job_count -= 1;

    DelayedJobList* p = NULL;
    DelayedJobList* t = _outstanding_jobs;

    for (; t; p = t, t = t->tail()) {

	if (t->head() == this) {

	    if (p == NULL)
		_outstanding_jobs = _outstanding_jobs->tail();
	    else
		p->tail() = p->tail()->tail();

	    delete t;

	    break;
	}
    }

    if (WIFEXITED(status))
	return (*_on_completion)(_out_segment, _err_segment, WEXITSTATUS(status), _data);

    pthrow prcserror << "Child process " << squote(_name)
		    << " was killed by signal " << WTERMSIG(status) << dotendl;
}
