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


#ifndef _MISC_H_
#define _MISC_H_

#include <unistd.h>

#define STRINGIFY2(X) #X
#define STRINGIFY(X) STRINGIFY2(X)

/* read_sym_link --
 *
 *     returns pointer to the contents of a symbolic link, or NULL if
 *     something goes wrong.  */
extern PrConstCharPtrError read_sym_link(const char* link);
extern PrConstCharPtrError find_real_filename(const char* filename);

/* fs_nuke_file --
 *
 *     recursively deletes files of all types */
extern NprVoidError fs_nuke_file(const char* file);

extern bool fs_file_readable(const char* P);
extern bool fs_file_wrandex(const char* P);
extern bool fs_file_writeable(const char* P);
extern bool fs_file_exists(const char* P);
extern bool fs_file_executable(const char* P);
extern bool fs_file_rwx(const char* P);

extern bool fs_is_directory(const char* P);
extern bool fs_is_directory_not_link(const char* P);
extern bool fs_is_symlink(const char* P);
extern bool fs_same_file_owner(const char* P);

/* weird_pathnameP --
 *
 *     returns true if the pathname contains a "/.." or begins with
 *     "../" */
extern bool weird_pathname(const char* P);

extern const char* strip_leading_path(const char* P);
extern void dirname(const char* P, Dstring* S);
extern const char* get_utc_time();
extern time_t get_utc_time_t();
extern const char* get_host_name();
extern const char* get_login();
extern gid_t get_user_id();
extern mode_t get_umask();
extern PrConstCharPtrError name_in_cwd(const char*);
extern NprVoidError change_cwd(const char*);
extern time_t timestr_to_time_t(const char* rcstime);
extern const char* time_t_to_rfc822(time_t t);

/* guess_prj_name --
 *
 *     returns a newly allocated Dstring corresponding to a unique
 *     file.prj in the current directory, or NULL if none.  */
extern PrDstringPtrError guess_prj_name(const char* dir);

/* correct_path, protect_path --
 *
 *     these possible allocate new storage to protect or correct the
 *     path name.  to protect it backslashes any backslash.  to
 *     correct it removes backslashes.  */
extern void protect_string(const char* path, Dstring* in);
extern void protect_path(const char* path, Dstring*);
extern void correct_path(const char* path, Dstring*);
extern void print_protected(const char* str, ostream& os);

/* show_file_info --
 *
 *     calls the systems 'ls -l' on the argument filename.  */
extern PrConstCharPtrError show_file_info(const char*);

/* absolute_path --
 *
 *     returns file if file begins with a '/' or is "-".  otherwise
 *     calls getcwd() and appends file.  */
extern PrConstCharPtrError absolute_path(const char* file);

/* pathname_hash, pathname_equal --
 *
 *     used by the hash table.  these both ignore duplicated slashes
 *     in a path name.  */
extern int pathname_hash(const char* const&, int);
extern bool pathname_equal(const char* const&, const char* const&);

extern const char* make_temp_file(const char* extension);
extern void make_temp_file_same_dir(Dstring* append_to);

extern const char* major_version_of(const char* maj_dot_min);
extern const char* minor_version_of(const char* maj_dot_min);

extern const char* format_type(FileType, bool cap = false);

extern char get_user_char();

extern bool empty_file(FILE* empty);
extern PrVoidError write_file(FILE* in, FILE* out);
extern NprVoidError write_fds(int infd, int outfd);

extern PrVoidError fs_zero_file(const char* P);
extern PrVoidError fs_copy_filename(const char* infile, const char* outfile);
extern PrVoidError fs_move_filename(const char* infile, const char* outfile);
extern PrVoidError fs_write_filename(FILE *in, const char* filename);

/* cmp_fds --
 *
 *     compares two files, returns 0 if identicle, 1 if they differ,
 *     and 2 if trouble occured.  does NOT close either fd.  */
NprBoolError cmp_fds(int one, int two);

NprVoidError read_string(FILE*, Dstring*);

char* p_strdup(const char* string);

PrVoidError directory_recurse(const char* base,
			      const void* data,
			      PrVoidError (*func)(const char* name,
						  const void* data));

PrVoidError directory_recurse_dirs(const char* base,
				   const void* data,
				   PrVoidError (*func)(const char* name,
						       const void* data));

const char* get_environ_var (const char* var);

#define foreach(P, INIT, TYPE) \
    for (TYPE P(INIT); ! (P).finished (); (P).next ())

PrVoidError bug(void);
const char* bug_name (void);

#endif
