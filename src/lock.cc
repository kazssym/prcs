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
#include <time.h>
#include <fcntl.h>
#include "maketime.h"
}

#include "prcs.h"
#include "misc.h"
#include "utils.h"
#include "lock.h"

#define LOCK_FILE_MODE 0666

/* This file was once implemented using fcntl(), I still have the code,
 * but since NFS sucks, I don't even include it anymore. */

/*
 * The number of seconds the process waits between stat calls to check
 * on the status of the lock.
 */
#define TRIAL_WAIT_PERIOD 1

long get_file_age(const char* file)
{
    FILE* lock_file = fopen(file, "r");
    char line_buf[1024];
    time_t youngest = 0;

    if(!lock_file)
	goto done;

    if(!fgets(line_buf, 1024, lock_file))
	goto done;

    while(fgets(line_buf, 1024, lock_file)) {
	const char* ptr = line_buf;
	int spaces = 0;
	char c;

	if(line_buf[0] == ' ')
	    continue;

	/* this scan depends on the format of the file, it looks for the
	 * 5th space and assumes that is a time. */

	while((c = *ptr++) != 0) {
	    if(c == ' ')
		spaces += 1;

	    if(spaces == 5) {
		time_t this_age = str2time(ptr, 0, 0);

		if(this_age > youngest)
		    youngest = this_age;

		break;
	    }
	}
    }

done:
    if (lock_file)
	fclose (lock_file);

    return time(NULL) - youngest;
}

/*
 * chill until the lock is available.
 */
int chilling_out(const char* waitfile, int wait_type)
{
    long age = get_file_age(waitfile);

    if(age < 0) {
	return LOCK_AVAILABLE;
    } else if(age > STALE_LOCK_TIMEOUT) {
	return LOCK_TRY_STEAL;
    } else {
	if(wait_type == LOCK_WAIT) {
	    struct stat buf;
	    long sleeptime = STALE_LOCK_TIMEOUT - age;
	    while(sleeptime > 0) {
		sleeptime -= TRIAL_WAIT_PERIOD - sleep(TRIAL_WAIT_PERIOD);
		if(stat(waitfile, &buf) < 0)
		    return LOCK_AVAILABLE;
	    }
	    return LOCK_TRY_STEAL;
	} else {
	    return LOCK_TRY_AGAIN;
	}
    }
}

int AdvisoryLock::write_lock(int wait_type)
{
    int chill;
    struct stat buf;

    if(locktype == WRITE_LOCK)
	return LOCK_SUCCESS;

    ASSERT(locktype != READ_LOCK, "unlock first");

    while (true) {
	while((wrfd = open(wrlockname, O_RDWR|O_CREAT|O_EXCL, LOCK_FILE_MODE)) < 0) {
	    if(errno == EEXIST) {
		chill = chilling_out(wrlockname, wait_type);
		if(chill != LOCK_AVAILABLE)
		    return chill;
	    } else {
		prcserror << "Error opening lock file "
			   << squote(wrlockname) << " for write lock" << perror;
		return LOCK_FAIL;
	    }
	}

	if(locktype != TRYING_READ_LOCK && stat(rdlockname, &buf) >= 0) {
	    if(close(wrfd) < 0) {
		prcserror << "Write failed on write lock file" << perror;
		return LOCK_FAIL;
	    }
	    if(unlink(wrlockname) < 0 && errno != ENOENT) {
		prcserror << "Unlink failed on write lock file" << perror;
		return LOCK_FAIL;
	    }
	    chill = chilling_out(rdlockname, wait_type);
	    if(chill != LOCK_AVAILABLE)
		return chill;
	} else {
	    break;
	}
    }

    if(!write_write_owner_notice())
	return LOCK_FAIL;

    if(close(wrfd) < 0) {
	prcserror << "Write failed on read lock file " << squote(wrlockname) << perror;
	return LOCK_FAIL;
    }

    locktype = WRITE_LOCK;

    return LOCK_SUCCESS;
}

int AdvisoryLock::read_lock(int wait_type)
{
    int trywrite;

    if(locktype == READ_LOCK) return LOCK_SUCCESS;

    ASSERT(locktype != WRITE_LOCK, "unlock first");

    locktype = TRYING_READ_LOCK;

    trywrite = write_lock(wait_type);

    if(trywrite != LOCK_SUCCESS) {
	locktype = NO_LOCK;
	return trywrite;
    }

    if((rdfd = open(rdlockname, O_RDWR|O_CREAT|O_EXCL, LOCK_FILE_MODE)) < 0) {
	if(errno != EEXIST) {
	    prcserror << "Error opening lock file " << squote(rdlockname)
		      << " for read lock" << perror;
	    locktype = NO_LOCK;
	    return LOCK_FAIL;
	} else if((rdfd = open(rdlockname, O_RDWR|O_CREAT, LOCK_FILE_MODE)) < 0) {
	    prcserror << "Error opening lock file " << squote(rdlockname)
		      << " for read lock" << perror;
	    locktype = NO_LOCK;
	    return LOCK_FAIL;
	}
    } else /* file was created */ {
	if(write(rdfd, LOCK_HEADER, LOCK_LENGTH) < 0) {
	    prcserror << "Write failed on lock file " << squote(rdlockname) << perror;
	    locktype = NO_LOCK;
	    return LOCK_FAIL;
	}
    }

    if(!write_read_owner_notice()) {
	locktype = NO_LOCK;
	return LOCK_FAIL;
    }

    locktype = READ_LOCK;

    if(unlink(wrlockname) < 0 && errno != ENOENT) {
	prcserror << "Unlink failed on lock file " << squote(wrlockname) << perror;
	locktype = NO_LOCK;
	return LOCK_FAIL;
    }

    if(close(rdfd) < 0) {
	prcserror << "Close failed on lock file " << squote(rdlockname) << perror;
	locktype = NO_LOCK;
	return LOCK_FAIL;
    }

    return LOCK_SUCCESS;
}

bool AdvisoryLock::unlock()
{
    if(locktype == NO_LOCK) {
	return true;
    } else if(locktype == WRITE_LOCK) {
	if(unlink(wrlockname) < 0 && errno != ENOENT) {
	    prcserror << "Unlink failed on lock file " << squote(wrlockname) << perror;
	    return false;
	}
    } else if(locktype == READ_LOCK) {
	char buf[256];
	int trywrite;
	char* p;
	bool lastlock = true;
	int c;

	memset(buf, ' ', rd_lock_banner_len);

	locktype = TRYING_READ_LOCK;

	trywrite = write_lock_nowait();

	while(true) {
	    /* There are two PRCS specific warnings here, cause I'm
	       not sure how to handle trouble obtaining the lock
	       while unlocking a read lock */
	    if(trywrite == LOCK_SUCCESS) {
		break;
	    } else if(trywrite == LOCK_TRY_AGAIN) {
		prcsinfo << "Read lock modifier lock unavailable, waiting to unlock the repository"
			 << dotendl;
	    } else /* if(trywrite == LOCK_FAIL || trywrite == LOCK_TRY_STEAL) */ {
		prcserror << "Unrecoverable error preventing unlocking of the repository, "
		    "you can manually remove locks " << squote(wrlockname) << " and "
			  << squote(rdlockname) << dotendl;
		return false;
	    }
	    trywrite = write_lock_wait();
	}

	locktype = READ_LOCK;

	if((rdfd = open(rdlockname, O_RDWR, LOCK_FILE_MODE)) < 0) {
	    prcserror << "Open failed while unlocking read lock file "
		      << squote(rdlockname) << ", the lock may have been stolen" << perror;
	    return unlink_writelock();
	} else if(lseek(rdfd, rd_lock_banner_offset, SEEK_SET) < 0) {
	    prcserror << "Lseek failed unlocking lock file" << perror;
	    return unlink_writelock();
	} else if(write(rdfd, buf, rd_lock_banner_len - 1) < 0) {
	    prcserror << "Write failed unlocking lock file" << perror;
	    return unlink_writelock();
	} else if(lseek(rdfd, LOCK_LENGTH, SEEK_SET) < 0) {
	    prcserror << "Lseek failed unlocking lock file" << perror;
	    return unlink_writelock();
	}

	while (true) {
	    do {
		c = read(rdfd, buf, 256);
	    } while (c < 0 && errno == EINTR);

	    if (c <= 0) /* error ignored */
		break;

	    p = buf;
	    while(p < buf + c){
		if(!isspace(*p))
		    lastlock = false;
		p += 1;
	    }
	}

	if(close(rdfd) < 0) {
	    prcserror << "Close failed unlocking lock file" << perror;
	    return unlink_writelock();
	}

	if(lastlock && unlink(rdlockname) < 0 && errno != ENOENT) {
	    prcserror << "Failed removing lock fail " << squote(rdlockname) << perror;
	    return unlink_writelock();
	}

	locktype = WRITE_LOCK;

	if(unlink(wrlockname) < 0 && errno != ENOENT) {
	    prcserror << "Failed removing lock file " << squote(wrlockname) << perror;
	    return unlink_writelock();
	}
    }

    locktype = NO_LOCK;
    return true;
}

AdvisoryLock::AdvisoryLock(const char* wrlockname0)
    :wrfd(-1), rdfd(-1), locktype(NO_LOCK)
{
    wrlockname.assign(wrlockname0);
    wrlockname.append(".writers");
    rdlockname.assign(wrlockname0);
    rdlockname.append(".readers");
}

/*
 * common code to both implementations
 */
bool AdvisoryLock::unlink_writelock()
{
    unlink(wrlockname);
    return false;
}

bool AdvisoryLock::write_write_owner_notice()
{
    Dstring pid;

    if(ftruncate(wrfd, 0) < 0) {
	prcserror << "Ftruncate failed while write locking lock file "
		  << squote(wrlockname) << perror;
	return false;
    }

    if(lseek(wrfd, 0, SEEK_SET) < 0) {
	prcserror << "Lseek failed while write locking lock file "
		  << squote(wrlockname) << perror;
	return false;
    }

    if(write(wrfd, LOCK_HEADER, LOCK_LENGTH) < 0) {
	prcserror << "Write failed while write locking lock file "
		  << squote(wrlockname) << perror;
	return false;
    }

    age = time(NULL);

    pid.sprintf("write lock: %s %d %s %s\n", get_login(), getpid(),
		get_host_name(), time_t_to_rfc822(age));

    wr_lock_banner_offset = lseek(wrfd, 0, SEEK_CUR);
    wr_lock_banner_len = pid.length();

    if( write(wrfd, pid.cast(), pid.length()) != pid.length()) {
	prcserror << "Error recording write lock in lock file "
		  << squote(wrlockname) << perror;
	return false;
    }

    return true;
}

bool AdvisoryLock::write_read_owner_notice()
{
    Dstring pid;

    if(lseek(rdfd, 0, SEEK_END) < 0) {
	prcserror << "Lseek failed while read locking lock file "
		  << squote(rdlockname) << perror;
	return false;
    }

    age = time(NULL);

    pid.sprintf("read lock: %s %d %s %s\n", get_login(), getpid(),
		get_host_name(), time_t_to_rfc822(age));

    rd_lock_banner_offset = lseek(rdfd, 0, SEEK_CUR);
    rd_lock_banner_len = pid.length();

    if( write(rdfd, pid.cast(), pid.length()) != pid.length()) {
	prcserror << "Error recording read lock in lock file " << squote(rdlockname) << perror;
	return false;
    }

    return true;
}

bool AdvisoryLock::steal_lock()
{
    if(unlink(rdlockname) < 0 && errno != ENOENT) {
	prcserror << "Unlink failed while stealing lock" << perror;
	return false;
    }
    if(unlink(wrlockname) < 0 && errno != ENOENT) {
	prcserror << "Unlink failed while stealing lock" << perror;
	return false;
    }
    return true;
}

FILE* AdvisoryLock::who_is_locking()
{
    FILE* ret;
    if(fs_file_exists(rdlockname)) {
	ret = fopen(rdlockname, "r");
    } else {
	ret = fopen(wrlockname, "r");
    }

    if(ret) {
	if(fseek(ret, LOCK_LENGTH, 0) < 0) {
	    prcserror << "Seek error in lock file " << squote(wrlockname) << perror;
	    return NULL;
	}
	return ret;
    } else {
	prcserror << "Error opening lock file " << squote(wrlockname)
		  << " to get lockers" << perror;
	return NULL;
    }
}

int AdvisoryLock::touch_lock()
{
    if (locktype == READ_LOCK) {

	if ((rdfd = open (rdlockname, O_RDWR, LOCK_FILE_MODE)) < 0)
	    return LOCK_FAIL;

	if (lseek (rdfd, rd_lock_banner_offset, SEEK_SET) < 0)
	    return LOCK_FAIL;

	Dstring pid;

	age = time(NULL);

	pid.sprintf("read lock: %s %d %s %s\n", get_login(), getpid(),
		    get_host_name(), time_t_to_rfc822(age));

	if( write(rdfd, pid.cast(), pid.length()) != pid.length())
	    return LOCK_FAIL;

	if (close(rdfd) < 0)
	    return LOCK_FAIL;

    } else if (locktype == WRITE_LOCK) {

	if ((wrfd = open (wrlockname, O_RDWR, LOCK_FILE_MODE)) < 0)
	    return LOCK_FAIL;

	if (lseek (wrfd, wr_lock_banner_offset, SEEK_SET) < 0)
	    return LOCK_FAIL;

	Dstring pid;

	age = time(NULL);

	pid.sprintf("write lock: %s %d %s %s\n", get_login(), getpid(),
		    get_host_name(), time_t_to_rfc822(age));

	if( write(wrfd, pid.cast(), pid.length()) != pid.length())
	    return LOCK_FAIL;

	if ( close(wrfd) < 0)
	    return LOCK_FAIL;
    }

    return LOCK_SUCCESS;
}

int AdvisoryLock::write_lock_wait() { return write_lock(LOCK_WAIT); }
int AdvisoryLock::read_lock_wait() { return read_lock(LOCK_WAIT); }
int AdvisoryLock::write_lock_nowait() { return write_lock(LOCK_NOWAIT); }
int AdvisoryLock::read_lock_nowait() { return read_lock(LOCK_NOWAIT); }

#if 0

int main()
{
    AdvisoryLock lock1("lockfile");
    int lock1val;
    bool wait = true;
    bool write = true;
    int pid;
    long foo;

    setup_streams("lock");

    while(true) {
	wait = foo % 2;
	foo += random();
	write = foo % 2;
	foo += random();

	if((pid = fork()) == 0)
	    break;
    }

    pid = getpid();

    prcsout << "pid " << pid << " requests " << (write ? "write" : "read")
	    << " " << (wait ? "wait" : "nowait") << " lock" << dotendl;

    if(wait) {
	if(write)
	    lock1val = lock1.write_lock_wait();
	else
	    lock1val = lock1.read_lock_wait();
    } else {
	if(write)
	    lock1val = lock1.write_lock_nowait();
	else
	    lock1val = lock1.read_lock_nowait();
    }

    if(lock1val == LOCK_SUCCESS) {
	prcserror("got %s lock pid %d\n", (write? "write": "read"), pid);
	sleep(10);
    } else if(lock1val == LOCK_FAIL) {
	prcserror("%s lock failed pid %d\n",(write? "write": "read"), pid);
    } else if(lock1val == LOCK_TRY_STEAL) {
	prcserror("%s lock says try steal %d\n", (write? "write": "read"), pid);
    } else {
	prcserror("%s lock returns try again pid %d\n",(write? "write": "read"), pid);
    }

    lock1.unlock();
    prcserror("exiting %d\n", pid);
}

#endif
