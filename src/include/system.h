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


#ifndef _SYSTEM_H_
#define _SYSTEM_H_

NprVoidError Err_chdir(const char* file);
NprVoidError Err_unlink(const char* file);
NprVoidError Err_rmdir(const char* file);
NprVoidError Err_truncate(const char* file, int length);
NprVoidError Err_lstat(const char* file, struct stat *buf);
NprVoidError Err_stat(const char* file, struct stat *buf);
NprVoidError Err_fstat(int fd, struct stat *buf);
NprVoidError Err_fwrite(const void*, int, int, FILE*);
NprVoidError Err_fclose(FILE*);
NprVoidError Err_mkdir(const char*, mode_t mode);
NprVoidError Err_creat(const char*, mode_t mode);
NprVoidError Err_close(int fd);
NprVoidError Err_rename(const char* file, const char* file2);
NprVoidError Err_chmod(const char* file, mode_t mode);
NprVoidError Err_symlink(const char* linkname, const char* file);
NprVoidError Err_write(int fd, const void* buf, size_t nbytes);
NprVoidError Err_fwrite(const void* buf, size_t nbytes, FILE* out);
NprVoidError Err_read_expect(int fd, void* buf, size_t nbytes);
NprVoidError Err_chown(const char *path, uid_t owner, gid_t group);
NprVoidError Err_utime(const char *file, const struct utimbuf *timep);

NprIntError Err_fgetc(FILE*);
NprCFilePtrError Err_fopen(const char* file, const char* type);
NprBoolError Err_access(const char* file, int mode);
NprIntError Err_waitpid(int pid, pid_t* pid_ret = NULL, bool nohang = false);
NprIntError Err_waitpid_nostart(int pid, pid_t* pid_ret = NULL, bool nohang = false);
NprIntError Err_readlink(const char* file, char* buf, int length);
NprIntError Err_read(int fd, void* buf, size_t nbytes);
NprIntError Err_fread(void* buf, size_t nbytes, FILE* f);
NprIntError Err_open(const char* file, int flags, mode_t mode = 0);

#endif
