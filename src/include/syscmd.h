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
 * $Id: syscmd.h 1.5.1.1.1.12.1.6 Wed, 14 Oct 1998 17:57:34 -0700 jmacd $
 */


#ifndef _SYSCMD_H_
#define _SYSCMD_H_

class PipeRec {
public:
    FILE* standard_out;
    FILE* standard_err;
};

/* Similar to STDIN_FILENO and STDOUT_FILENO for pipes, I use these to
 * keep from remember which fileno is which. */
#define PIPE_WRITE_FD 1
#define PIPE_READ_FD 0

extern char *const null_environment[];

/* the following command forks a child and executes a command without
 * invoking a shell.  the first assigns the fd as standard input for
 * the command.  It closes fd after waiting for the child. */
extern PrExitStatusError make_pipe_file_in(int fd, const char* const*, bool);
/* This one just executes the command, THIS IS THE ONLY ONE THAT
 * PRESERVES THE ENVIRONMENT. */
extern PrExitStatusError make_pipe_stdout(const char* const*);

/* the following command takes an argv[] returns a pointer to a pipe
 * record which has opened FILE*s to the standard error and standard
 * output of the command which is executed.  to get a return value
 * from the command, close_pipe should be called.  PIPE_OPEN_FAILURE
 * is returned if fork() or pipe() or exec() fails */
extern PrPidTError make_pipe_out(const char* const*, PipeRec*, bool, bool);
extern PrPidTError make_pipe_out_delay(const char* const* argv,
				       int* stdout_fd,
				       int* stderr_fd,
				       bool pipeout, bool pipeerr);

/* takes as argument a PipeRec which must have been created with
 * make_pipe_out(), and flushes both output streams, then waits for
 * the command to terminate, returning PIPE_CLOSE_FAILURE if the pipe
 * terminated due to some signal or the exit value of the last process
 * in the pipe.  */
extern NprExitStatusError close_pipe(PipeRec* pr, pid_t pid);

class DelayedJob {
public:

    typedef PrVoidError (*DelayNotifyFunction)(MemorySegment*, MemorySegment*,
					       int exit_val, void* data);

    static PrVoidError delay_wait(bool force = false);
    static int delayed_jobs();

    friend class SystemCommand;

private:

    DelayedJob (DelayNotifyFunction notf,
		pid_t pid,
		void* data,
		const char* name,
		int stdout_fd,
		int stderr_fd);

    DelayedJob (DelayNotifyFunction notf,
		pid_t pid,
		void* data,
		const char* name,
		const char* stdout_file,
		const char* stderr_file);

    void init_segs();

    static PrVoidError read_some();
    static PrBoolError wait_some();

    PrVoidError read_one_fd(int fd);
    PrVoidError finish_job(int status);

    ~DelayedJob();

    static MemorySegmentList *_free_segments;
    static DelayedJobList *_outstanding_jobs;
    static int _outstanding_job_count; /* length of above. */

    DelayNotifyFunction _on_completion;

    int _stdout_fd;
    int _stderr_fd;

    const char* _stdout_file;
    const char* _stderr_file;

    bool _stdout_eof;
    bool _stderr_eof;

    MemorySegment* _out_segment;
    MemorySegment* _err_segment;

    pid_t _pid;

    void* _data;
    const char* _name;
};

class SystemCommand {
public:
    /* Initializes a system command but does not stat the executable
     * until the first open command is called or until the init()
     * method is called.  See comment above for naming convention of
     * this initializer. */
    SystemCommand(const char* name, const char* path);

    /* Forks and execs the command using fd as the standard input and
     * closing the standard output if close is true */
    PrExitStatusError open_filein(int fd, bool close);
    /* Forks and execs the command leaving the standard output and
     * standard error open. */
    PrExitStatusError open_stdout();
    /* Forks and execs the command returning a PipeRec containing
     * fdopened FILE*s for the standard output and standard error
     * streams. */
    PrVoidError open(bool out, bool err);
    /* Like the above, but returns immediately. */
    PrVoidError open_delayed(DelayedJob::DelayNotifyFunction,
			     void* data,
			     bool out,
			     bool err);

    FILE* standard_out() const;
    FILE* standard_err() const;

    /* Initializes an array with argv[0] of a new argument list.
     * Arguments should be apended to this before an open command is
     * called */
    PrArgListPtrError new_arg_list();

    /* Computes the full path name of the command and then stats it to
     * see that the current use has execute permission.  path_env is the
     * name of an environment variable in which to find the executable,
     * defaults to RCS_PATH. */
    PrVoidError init();
    /* Assumes init() was successful, and returns the full pathname */
    const char* path() const;

    /* Assuming already open(), wait() for the child to exit and
     * return the exit status.  An error can arise if the child does
     * not terminate due to an exit() call, if the child is stoped
     * by a signal, or if not open(). */
    PrExitStatusError close();

private:

    ArgList argl;
    const char *name;
    Dstring fp;
    PipeRec pr;
    const char* path_env;
    pid_t one_pid;
};

SystemCommand* sys_cmd_by_name (const char* name);

extern SystemCommand rcs_command;
extern SystemCommand ci_command;
extern SystemCommand co_command;
extern SystemCommand rlog_command;
extern SystemCommand tar_command;
extern SystemCommand gdiff3_command;
extern SystemCommand gdiff_command;
extern SystemCommand ls_command;
extern SystemCommand gzip_command;

extern void abort_child(const char* argv0);

#endif
