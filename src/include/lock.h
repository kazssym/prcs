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
 * $Id: lock.h 1.7.1.7.1.6 Tue, 29 Sep 1998 13:26:59 -0700 jmacd $
 */

#ifndef _LOCK_H_
#define _LOCK_H_

/*
 * USE_FCNTL_ADVISORY_LOCKS determines whether the locking mechanism
 * uses fcntl() to obtain kernel supported advisory locks, which
 * often will not work (but is capable of working) over NFS.  If
 * false.  The fcntl() locking implementation will not allow locks
 * to be stolen, since fcntl() is assumed to work.  The other
 * implementation uses open(lockfile, O_CREAT|O_EXCL...) to
 * exclusively open a lock.  Since this mechanism is not guaranteed
 * to be destroyed when the process dies (the process must unlock
 * the lock before exiting), these locks are allowed to time out.
 * A timeout period is defined with the variable STALE_LOCK_TIMEOUT.
 * Should a lock be requested who's last modification time exceeds
 * this value in seconds, a the lock call will return LOCK_TRY_STEAL,
 * at which point the user may request to steal the lock and continue.
 *
 * The only interface difference between the two is that the fcntl()
 * locks will never return LOCK_TRY_STEAL.
 *
 * A list of all people locking the file is maintained in the lock
 * file itself, so to lock a file you should name the lock differently.
 *
 * A process can request a list of all lockers.  As with fcntl() locks,
 * read locks deny write locks and not read locks.  Write locks deny
 * write and read locks.
 */

#include <fcntl.h>

/*
 * Metrowerks Standard C++ Library has its own idea of READ_LOCK and
 * WRITE_LOCK which collides with the defines in this file.
 * Good news is that the library's defines are not officially exported
 * so they can be disabled here. Bad news is that lock.h must not be
 * used before any of the standard library includes.
 */

#ifdef __MWERKS__
#  ifdef READ_LOCK
#    undef READ_LOCK
#  endif
#  ifdef WRITE_LOCK
#    undef WRITE_LOCK
#  endif
#endif

#define STALE_LOCK_TIMEOUT 600 /* 10 minutes */

		/* these are possible return values */
/* The lock requested was obtained */
#define LOCK_SUCCESS 0
/* A failure of some type arose */
#define LOCK_FAIL 1
/* A nowait lock was requested and the lock is not available */
#define LOCK_TRY_AGAIN 2
/* The lock requested is not available and has timed out */
#define LOCK_TRY_STEAL 3
/* The lock is available */
#define LOCK_AVAILABLE 4

	      /* these are not intended for user use */
/* values of the locktype field */
#define NO_LOCK 0
#define READ_LOCK 1
#define WRITE_LOCK 2
#define TRYING_READ_LOCK 3

/* the fcntl() lock types */
#define LOCK_WAIT F_SETLKW
#define LOCK_NOWAIT F_SETLK

/* the fcntl() lock requests */
#define LOCK_TYPE_READ F_RDLCK
#define LOCK_TYPE_WRITE F_WRLCK
#define LOCK_TYPE_UNLOCK F_UNLCK

/* the header of a lock file */
#define LOCK_HEADER "file is currently locked by:\n"
#define LOCK_LENGTH strlen(LOCK_HEADER)

class AdvisoryLock {
public:
    /*
     * creates a lock using lockname0 as the lock file.  this class
     * is either implemented using fcntl() if USE_FCNTL_ADVISORY_LOCKING
     * is defined true or else using a more naive strategy.
     */
    AdvisoryLock(const char* lockname0);
    /*
     * write_lock_wait --
     *
     *     returns LOCK_SUCCESS if successful, after waiting for a lock.
     *     returns LOCK_FAIL if failure occurs
     *     returns LOCK_TRY_STEAL if fcntl() isn't being used (by compile
     *     time option USE_FCNTL_ADVISORY_LOCKS) and lock age is beyond
     *     STALE_LOCK_TIMEOUT.
     *
     *     this function can block indefinetly if USE_FCNTL_ADVISORY_LOCKS
     *     is defined at compile time.
     *
     *     write locks exclude other write and read locks.
     */
    int write_lock_wait();
    /*
     * read_lock_wait --
     *
     *     same return values as above.  read locks exclude write locks
     *     but not other read locks..
     */
    int read_lock_wait();
    /*
     * write_lock_nowait --
     *
     *     returns LOCK_SUCCESS if successful
     *     returns LOCK_FAIL if failure occurs
     *     returns LOCK_TRY_AGAIN if anyone else is locking the file
     *     or, if fcntl() is not being used, the lock is younger than
     *     STALE_LOCK_TIMEOUT.
     *     returns LOCK_TRY_STEAL if fcntl() isn't being used (by compile
     *     time option USE_FCNTL_ADVISORY_LOCKS) and lock age is beyond
     *     STALE_LOCK_TIMEOUT.
     */
    int write_lock_nowait();
    /*
     * read_lock_wait --
     *
     *     same return values as above.
     */
    int read_lock_nowait();
    /*
     * unlock --
     *
     *     returns true if the lock is unheld or released, otherwise false
     *     will steal a lock in order to do so.  and may wait a while.
     */
    bool unlock();
    /*
     * steal_lock --
     *
     *     if LOCK_TRY_STEAL has been returned, steal the lock.
     *     returns LOCK_FAIL or LOCK_SUCCESS.
     */
    bool steal_lock();
    /*
     * who_is_locking --
     *
     *     returns a stream opened for reading which contains a
     *     description of who is locking the lockfile and what type
     *     of locks.
     *
     *     each line is either blank indicating that a
     *     lock has been released, or a description of a locker.
     */
    FILE* who_is_locking();

    int touch_lock();
    time_t time_remaining();

private:

    AdvisoryLock(const AdvisoryLock&);
    AdvisoryLock& operator=(const AdvisoryLock&);
    bool write_read_owner_notice();
    bool write_write_owner_notice();
    int write_lock(int wait);
    int read_lock(int wait);
    bool unlink_writelock();

    Dstring wrlockname;
    Dstring rdlockname;
    int wrfd;
    int rdfd;
    int locktype;
    off_t rd_lock_banner_offset;
    off_t rd_lock_banner_len;
    off_t wr_lock_banner_offset;
    off_t wr_lock_banner_len;
    time_t age;
};

#endif
