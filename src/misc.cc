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
#include <sys/utsname.h>
#include <fcntl.h>
#include <time.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/wait.h>
#include <signal.h>
#include <utime.h>
#include "maketime.h"
}

#include "prcs.h"
#include "docs.h"
#include "prcsdir.h"
#include "misc.h"
#include "vc.h"
#include "syscmd.h"
#include "populate.h"
#include "repository.h"
#include "system.h"
#include "checkout.h"

const char  *cmd_root_project_name = NULL;
const char  *cmd_root_project_full_name;
const char  *cmd_root_project_file_path;
const char  *cmd_root_project_path;
int          cmd_root_project_path_len;

bool         cmd_prj_given_as_file = false;
char       **cmd_filenames_given;
const char **cmd_corrected_filenames_given;
int          cmd_filenames_count;
bool        *cmd_filenames_found;
char       **cmd_diff_options_given;
int          cmd_diff_options_count;
const char  *cmd_version_specifier_major = NULL;
const char  *cmd_version_specifier_minor = NULL;
int          cmd_version_specifier_minor_int = -1;
const char  *cmd_alt_version_specifier_major = NULL;
const char  *cmd_alt_version_specifier_minor = NULL;
const char  *cmd_repository_path = NULL;

int option_immediate_uncompression = 0;
int option_force_resolution = 0;
int option_long_format = 0;
int option_really_long_format = 0;
int option_report_actions = 0;
int option_version_present = 0;
int option_diff_keywords = 0;
int option_diff_new_file = 0;
int option_populate_delete = 0;
int option_package_compress = 0;
int option_be_silent = 0;
int option_preserve_permissions = 0;
int option_exclude_project_file = 0;
int option_unlink = 0;
int option_match_file = 0;
int option_not_match_file = 0;
int option_all_files = 0;
int option_preorder = 0;
int option_pipe = 0;
int option_nokeywords = 0;
int option_version_log = 0;
#ifdef PRCS_DEVEL
int option_n_debug = 1;
int option_tune = 10;
#endif
int option_jobs = 1;
int option_skilled_merge = 0;
int option_plain_format = 0;
int option_sort = 0;

const char *option_match_file_pattern = NULL;
const char *option_not_match_file_pattern = NULL;
const char *option_sort_type = NULL;
const char *option_version_log_string = NULL;

const char* temp_file_1 = NULL;
const char* temp_file_2 = NULL;
const char* temp_file_3 = NULL;
const char* temp_directory = NULL;

PrVoidError bug (void)
{
  static const char maintainer[] = "prcs-bugs@XCF.Berkeley.EDU";

  pthrow prcserror << "Please report this to " << maintainer << dotendl
		  << "When sending bug reports, always include:" << prcsendl
		  << "-- a complete description of the problem encountered" << prcsendl
		  << "-- the output of `prcs config'" << prcsendl
		  << "-- the operating system and version" << prcsendl
		  << "-- the architecture" << dotendl
		  << "If possible, include:" << prcsendl
		  << "-- the file " << squote (bug_name()) << ", if it exists" << prcsendl
		  << "-- the working project file, if one was in use" << prcsendl
		  << "-- the output of `ls -lR' in the current directory" << dotendl
		  << "Disk space permitting, retain the following:" << prcsendl
		  << "-- any relevant working project files" << prcsendl
		  << "-- a repository package created with `prcs package PROJECT'" << dotendl
		  << "Disk space not permitting, retain just:" << prcsendl
		  << "-- the repository file PRCS/PROJECT/PROJECT.prj,v" << prcsendl
		  << "-- the repository log PRCS/PROJECT/prcs_log" << prcsendl
		  << "-- the repository data file PRCS/PROJECT/prcs_data" << prcsendl
		  << "-- the output of `rlog' on each repository file under PRCS/PROJECT" << dotendl
		  << "These steps will help diagnose the problem" << dotendl;
}

const char* bug_name (void)
{
  const char* tmp = get_environ_var ("TMPDIR");

  if (!tmp) tmp = "/tmp";

  static Dstring *it;

  if (!it)
    it = new Dstring;

  it->assign (tmp);
  it->append ("/");
  it->append ("prcs_bug");

  return it->cast();
}

bool is_linked(const char* path)
{
    struct stat buf;

    if (lstat(path, &buf) < 0) {
        return false;
    } else if (S_ISLNK(buf.st_mode) || buf.st_nlink > 1) {
        return true;
    } else {
        return false;
    }
}

/* this unbackslashes all backslashed characters and removes excess
 * slashes from path */
void correct_path(const char *path, Dstring* in)
{
    ASSERT(path != NULL, "can't be null");

    while(*path) {
        if(*path == '\\') {
            path += 1;
        }

        in->append(*path);
        path += 1;
    }
}

/* backslashes all backslashes and quotation marks in a path so that
 * they can be inserted into the project file */
void protect_path(const char* path, Dstring* in)
{
    while(*path) {
        switch (*path) {
	case '\"': case '\\': case ')':  case '(':  case ' ':
	case '\t': case '\n': case '\v': case '\r': case '\f': case ';':
            in->append('\\');
	default:
        in->append(*path);
	}
        path += 1;
    }
}

void print_protected (const char* str, ostream& os)
{
    while(*str) {
	char c = *str++;

        switch (c) {
	case '\"': case '\\': case ')':  case '(':  case ' ':
	case '\t': case '\n': case '\v': case '\r': case '\f': case ';':
            os << '\\';
	default:
	    os << c;
	}
    }
}

void protect_string(const char* path, Dstring* in)
{
    while(*path) {
        switch (*path) {
	case '\"': case '\\':
            in->append('\\');
	default:
        in->append(*path);
	}
        path += 1;
    }
}

bool weird_pathname(const char* N)
{
    /* don't allow anything outside the repository */
    if (strncmp("../", N, 3) == 0)
        return true;
    else if (strncmp("./", N, 2) == 0)
	return true;

    if (N[0] == '/')
	return true;

    /* don't allow any '/../'s in the path */
    while ((N = strchr(N, '/')) != NULL) {
        if (strncmp(N, "/../", 4) == 0)
            return true;
	if (strncmp(N, "/./", 3) == 0)
	    return true;
        N += 1;
    }

    if (N) {
	int len = strlen(N);

	if (len > 0 && N[len - 1] == '/')
	    return true;
    }

    return false;
}

const char* strip_leading_path(const char* P)
{
    char* s = strrchr(P, '/');

    if ( s == '\0' )
        return P;
    else
        return s+1;
}

bool fs_is_symlink (const char* P)
{
    struct stat buf;

    if (lstat(P, &buf) < 0)
	return false;
    if (S_ISLNK(buf.st_mode))
	return true;
    return false;
}

bool fs_is_directory_not_link(const char* P)
{
    struct stat buf;

    if(stat(P, &buf) < 0)
        return false;
    if(!S_ISDIR(buf.st_mode) || S_ISLNK(buf.st_mode))
        return false;
    return true;
}

bool fs_is_directory(const char* P)
{
    struct stat buf;

    if(stat(P, &buf) < 0)
        return false;
    if(!S_ISDIR(buf.st_mode))
        return false;
    return true;
}

bool fs_file_readable(const char* P) { return access(P, F_OK | R_OK) >= 0; }
bool fs_file_wrandex(const char* P) { return access(P, F_OK | W_OK | X_OK) >= 0; }
bool fs_file_writeable(const char* P) { return access(P, F_OK | W_OK ) >= 0; }
bool fs_file_exists(const char* P) { return access(P, F_OK) >= 0; }
bool fs_file_executable(const char* P) { return access(P, F_OK | X_OK) >= 0; }
bool fs_file_rwx(const char* P) { return access(P, F_OK | X_OK | W_OK | R_OK) >= 0; }

PrVoidError directory_recurse(const char* base,
			      const void *data,
			      PrVoidError (*func)(const char* name,
						  const void* data))
  /* This is used for iterating through the repository.
   * It doesn't follow symlinks, as a result. */
{
    Dir current_dir(base);

    foreach (ent_ptr, current_dir, Dir::FullDirIterator) {
	const char *ent = *ent_ptr;

	if (fs_is_directory_not_link(ent))
	    Return_if_fail(directory_recurse(ent, data, func));
	else
	    Return_if_fail(func(ent, data));
    }

    if (! current_dir.OK() ) {
	pthrow prcserror << "Error reading directory "
			<< squote (base) << perror;
    }

    return NoError;
}

PrVoidError directory_recurse_dirs(const char* base,
				   const void *data,
				   PrVoidError (*func)(const char* name,
						       const void* data))
  /* This is used for iterating through the repository.
   * It doesn't follow symlinks, as a result. */
{
    Dir current_dir(base);

    Return_if_fail ((*func) (base, data));

    foreach (ent_ptr, current_dir, Dir::FullDirIterator) {
	const char *ent = *ent_ptr;

	if (fs_is_directory_not_link(ent)) {
	    Return_if_fail ((*func) (ent, data));

	    Return_if_fail(directory_recurse(ent, data, func));
	}
    }

    if (! current_dir.OK() ) {
	pthrow prcserror << "Error reading directory "
			<< squote (base) << perror;
    }

    return NoError;
}

NprVoidError fs_nuke_file(const char* file)
{
    bool ret = true;
    if(fs_is_directory(file)) {
	char old_dir[MAXPATHLEN];
	const char* old_dir_name;

	If_fail(old_dir_name << name_in_cwd(""))
	    ret = false;

	strncpy (old_dir, old_dir_name, MAXPATHLEN-1);

        If_fail(change_cwd(file))
            ret = false;

	if (ret) {
	    {
		Dir current_dir(".");

		foreach(ent_ptr, current_dir, Dir::DirIterator) {
		    If_fail(fs_nuke_file(*ent_ptr)) {
			ret = false;
		    }
		}

		Return_if_fail(change_cwd(old_dir));

		ret &= current_dir.OK();
	    }
            ret &= rmdir(file) >= 0;
        }

    } else {
        ret &= unlink(file) >= 0;
    }

    if(ret)
	return NoError;
    else
	return FatalError;
}

mode_t get_umask()
{
    mode_t mask = umask(0);

    umask(mask);

    return mask;
}

const char* get_login()
{
    static char buf[32]; /* L_cuserid */
    static bool do_once = false;

    if(do_once) return buf;

    struct passwd *user = getpwuid(get_user_id());

    if(!user)
	strcpy(buf, "unknown");
    else
	strcpy(buf, user->pw_name);

    do_once = true;

    return buf;
}

uid_t get_user_id()
{
    static bool do_once = false;
    static uid_t uid;

    if (do_once) return uid;

    do_once = true;

    uid = getuid();

    return uid;
}

/*
 * get_utc_time --
 *
 *     returns a time formatted like so: Sun, 24 Mar 1996 23:20:44 -0800
 *     which is UTC in that the offset from gmt is included, but the hour
 *     minute and date are in local time.  careful--the result is over
 *     written by the next call.
 */
const char* get_utc_time()
{
    static char buf[64];
    static bool do_once = false;
    if(do_once)
	return buf;
    strcpy(buf, time_t_to_rfc822(get_utc_time_t()));
    do_once = true;
    return buf;
}

time_t get_utc_time_t()
{
    static time_t t = 0;

    if(t)
	return t;

    time(&t);

    return t;
}

/*
 * timestr_to_time_t --
 *
 *     this calls the somewhat excessive RCS time conversion routine that
 *     converts a standard RCS time string such as 1996/01/23 07:36:14
 *     into a time_t.  I could write a less bulky conversion routine that
 *     didn't try to convert 10 different formats.
 */
time_t timestr_to_time_t(const char* rcstime)
{
    return str2time(rcstime, 0, 0);
}

/*
 * From RFC#822, Aug 13, 1982
 *
 *    5.1.  SYNTAX
 *
 *    date-time   =  [ day "," ] date time        ; dd mm yy
 *                                                ;  hh:mm:ss zzz
 *    day         =  "Mon"  / "Tue" /  "Wed"  / "Thu"
 *                /  "Fri"  / "Sat" /  "Sun"
 *    date         = 1*2DIGIT month 2*4DIGIT      ; correction in RFC#1123
 *                                                ; day month year
 *                                                ;  e.g. 20 Jun 82
 *    month       =  "Jan"  /  "Feb" /  "Mar"  /  "Apr"
 *                /  "May"  /  "Jun" /  "Jul"  /  "Aug"
 *                /  "Sep"  /  "Oct" /  "Nov"  /  "Dec"
 *    time        =  hour zone                    ; ANSI and Military
 *    hour        =  2DIGIT ":" 2DIGIT [":" 2DIGIT]
 *                                                ; 00:00:00 - 23:59:59
 *    zone        =  "UT"  / "GMT"                ; Universal Time
 *                                                ; North American : UT
 *                /  "EST" / "EDT"                ;  Eastern:  - 5/ - 4
 *                /  "CST" / "CDT"                ;  Central:  - 6/ - 5
 *                /  "MST" / "MDT"                ;  Mountain: - 7/ - 6
 *                /  "PST" / "PDT"                ;  Pacific:  - 8/ - 7
 *                /  1ALPHA                       ; Military: Z = UT;
 *                                                ;  A:-1; (J not used)
 *                                                ;  M:-12; N:+1; Y:+12
 *                / ( ("+" / "-") 4DIGIT )        ; Local differential
 *                                                ;  hours+min. (HHMM)
 */
const char* time_t_to_rfc822(time_t t)
{
    static char timebuf[64];
    static const char day[7][4] =
      { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
    static const char mon[12][4] =
      { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
    struct tm lt = *localtime(&t);
    int utc_offset = difftm(&lt, gmtime(&t));
    char sign = utc_offset < 0 ? '-' : '+';
    int minutes = abs(utc_offset) / 60;
    int hours = minutes / 60;
    sprintf(timebuf, "%s, %02d %s %d %02d:%02d:%02d %c%02d%02d",
	    day[lt.tm_wday], lt.tm_mday, mon[lt.tm_mon], lt.tm_year + 1900,
	    lt.tm_hour, lt.tm_min, lt.tm_sec,
	    sign, hours, minutes % 60);
    return timebuf;
}

/*
 * get_host_name --
 *
 *     This used to be here because I couldn't find a portable method
 *     of finding a hostname, but now I did.  This is used by the
 *     locking mechanism to alert users what machine is holding a lock.
 */
const char* get_host_name()
{
    static char buf[256];
    struct utsname utsbuf;
    if(uname(&utsbuf) < 0)
        strcpy(buf, "** uname() failed -- unknown host **");
    else
        strcpy(buf, utsbuf.nodename);
    return buf;
}

/*
 * name_in_cwd --
 *
 *     I don't like SunOS's getcwd(), it freaked me out when I was
 *     looking at a profiler output.  It calls popen on 'pwd' and fgets
 *     on the output!
 *
 *     returns NULL on failure.
 */
static bool changed = true;
PrConstCharPtrError name_in_cwd(const char* name)
{
    static char buf[MAXPATHLEN];
    static int buflen;

    if(changed) {
        if(
#if defined(sun) && !defined(__SVR4)
           getwd(buf)
#else
           getcwd(buf, MAXPATHLEN)
#endif
           == NULL) {
            pthrow prcserror << "Getcwd failed" << perror;
        } else
            changed = false;
        buflen = strlen(buf);
    }

    if(buflen + strlen(name) + 1 > MAXPATHLEN)
        pthrow prcserror << "Failed building full pathname to " << squote(name)
			<< ", name too long" << dotendl;

    buf[buflen] = '/';
    strcpy(buf + buflen + 1, name);

    return buf;
}

/*
 * change_cwd --
 *
 *     Change the current working directory and tell name_in_cwd its different
 *     now.
 */
NprVoidError change_cwd(const char* path)
{
    changed = true;
    if(chdir(path) == 0)
	return NoError;
    else
	return NonFatalError;
}

/*
 * guess_prj_name --
 *
 *     returns NULL if 0 or more than 1 .prj file are in the current directory.
 *     otherwise returns a heap-allocated Dstring containing the name of the
 *     file, after truncating the ".prj"
 */
PrDstringPtrError guess_prj_name(const char* dir)
{
    bool found = false;
    Dstring buf;
    Dstring *ret;

    Dir current_dir(dir);

    foreach(ent_ptr, current_dir, Dir::DirIterator) {
	const char* ent = *ent_ptr;

        int len = strlen(ent);
        if (len > 4 && strcmp(".prj", ent + len - 4) == 0) {
            if(found) {
                return (Dstring*)0;
            } else {
                buf.assign(ent);
                found = true;
            }
        }
    }

    if (! current_dir.OK() ) {
        pthrow prcserror << "Error reading current directory" << perror;
    }

    if(found) {
        ret = new Dstring(buf);
        ret->truncate(ret->length() - 4);

        return ret;
    } else {
        return (Dstring*)0;
    }
}

PrConstCharPtrError read_sym_link(const char* name)
{
    static char buf[MAXPATHLEN];
    int ret;

    if((ret = readlink(name, buf, MAXPATHLEN)) < 0) {
        pthrow prcserror << "Error reading symlink " << squote(name) << perror;
    } else {
        buf[ret] = '\0'; /* readlink() doesn't zero terminate */
    }

    if (ret == 0)
	pthrow prcserror << "Symlink " << squote(name) << " may not be null" << dotendl;

    return buf;
}

PrConstCharPtrError show_file_info(const char* file)
{
    static Dstring* lsout;
    ArgList *args;
    FILE* out;

    if(lsout == NULL)
        lsout = new Dstring();

    Return_if_fail(args << ls_command.new_arg_list());

    args->append("-ldgF");
    args->append(file);

    Return_if_fail(ls_command.open(true, false));

    out = ls_command.standard_out();

    lsout->truncate(0);

    If_fail(read_string(out, lsout))
	pthrow prcserror << "Read failure from ls output" << perror;

    Return_if_fail_if_ne(ls_command.close(), 0)
        pthrow prcserror << ls_command.path() << " exited abnormally" << dotendl;

    return lsout->cast();
}

PrConstCharPtrError absolute_path(const char* name)
{
    static Dstring *buf = NULL;

    if(!buf)
	buf = new Dstring;

    if(strcmp(name, "-") == 0) {
        return name;
    } else if(name[0] == '/') {
        return name;
    } else {
	const char* cwd;

	Return_if_fail(cwd << name_in_cwd(""));

        buf->assign(cwd);
        buf->append(name);

        return buf->cast();
    }
}

bool pathname_equal(const char* const& a, const char* const& b)
{
    const char *c(a), *d(b);
    for(;;) {

        while(c != a && *c == '/' && *(c - 1) == '/') { c += 1; }
        while(d != b && *d == '/' && *(d - 1) == '/') { d += 1; }

        if(*c != *d)
            return false;

        if(*c == '\0') {
            return true;
        }

        c += 1;
        d += 1;
    }
}

int pathname_hash(const char* const& s, int M)
{
    const char *p;
    unsigned int h(0), g;
    for(p = s; *p != '\0'; p += 1) {
        if(p != s && *p == '/' && *(p-1) == '/')
            continue;
        h = ( h << 4 ) + *p;
        if ( ( g = h & 0xf0000000 ) ) {
            h = h ^ (g >> 24);
            h = h ^ g;
        }
    }

    return h % M;
}

char get_user_char()
    /* 0 is error */
{
    int ans, c;

    If_fail(ans << Err_fgetc(stdin))
	return 0;

    if(ans == EOF)
        return 0;
    else if(ans == '\n')
        return '\n';

    while(true) {
	If_fail(c << Err_fgetc(stdin))
	    return 0;

	if (c == EOF)
	    return 0;
	else if (c == '\n')
	    break;
    }

    return (char)ans;
}

void make_temp_file_same_dir (Dstring* temp_name)
{
    temp_name->append(".prcs_tmp_");

    int len = temp_name->length();
    int tries = 0;

    do {
	temp_name->truncate(len);
	temp_name->append_int(tries++);
    } while (fs_file_exists(*temp_name));
}

const char* make_temp_file(const char* extension)
{
    static int counter = 0;
    ASSERT(temp_directory, "set the temp dir");
    Dstring *t = new Dstring(temp_directory);
    t->sprintfa("/prcs_temp.%d%s", counter, extension);
    counter += 1;
    return *t;
}

const char* format_type(FileType t, bool cap)
{
    switch (t) {
    case SymLink:
	return cap ? "Symlink" : "symlink";
    case Directory:
	return cap ? "Directory" : "directory";
    case RealFile:
	return cap ? "File" : "file";
    }
    return "*** Illegal Type ***";
}

PrVoidError fs_write_filename(FILE *in, const char* filename)
{
    FILE* out;

    If_fail(out << Err_fopen(filename, "w"))
	pthrow prcserror << "Failed opening file " << squote(filename)
		         << " for writing" << perror;

    Return_if_fail(write_file(in, out));

    If_fail(Err_fclose(out))
	pthrow prcserror << "Failed writing to file "
			 << squote(filename) << perror;

    return NoError;
}

PrVoidError fs_move_filename(const char* infile, const char* outfile)
{
    if (rename(infile, outfile) >= 0)
	return NoError;

    if (errno != EXDEV)
	pthrow prcserror << "Rename failed on " << squote(infile) << " to "
			<< squote(outfile) << perror;

    return fs_copy_filename(infile, outfile);
}

PrVoidError fs_copy_filename(const char* infile, const char* outfile)
{
    FILE *in, *out;

    If_fail(in << Err_fopen(infile, "r"))
	pthrow prcserror << "Failed opening file " << squote(infile)
	                << " for reading" << perror;

    If_fail(out << Err_fopen(outfile, "w"))
	pthrow prcserror << "Failed opening file " << squote(outfile)
		        << " for writing" << perror;

    Return_if_fail(write_file(in, out));

    fclose(in);

    If_fail(Err_fclose(out))
	pthrow prcserror << "Failed writing file " << squote(outfile)
			 << perror;

    return NoError;
}

static char *Buffer1 = NULL, *Buffer2 = NULL;
static int Buffer1Len = 0, Buffer2Len = 0;
static int Buffer1Alloc = 0, Buffer2Alloc = 0;

static void opt_buffer(int toread, int* buflen, int* bufalloc, char** buf)
{
    struct stat statbuf;

    *buflen = 1 << 10;  /* default */

    if(toread >= 0 && fstat(toread, &statbuf) >= 0)
        *buflen = statbuf.st_blksize;

    if(*bufalloc < *buflen) {
        if(*bufalloc == 0) {
            *buf = NEWVEC(char, *buflen);
            *bufalloc = *buflen;
        } else {
            *buf = (char*)EXPANDVEC(*buf, *bufalloc, *buflen - *bufalloc);
        }
    }
}

#define opt_buffer_1(fd) opt_buffer(fd, &Buffer1Len, &Buffer1Alloc, &Buffer1)
#define opt_buffer_2(fd) opt_buffer(fd, &Buffer2Len, &Buffer2Alloc, &Buffer2)

PrVoidError write_file(FILE* in, FILE* out)
{
    int c;

    opt_buffer_1(-1);

    for (;;) {
        If_fail (c << Err_fread(Buffer1, Buffer1Len, in))
	  pthrow prcserror << "Read failure" << perror;

	if (c == 0)
	  break;

        If_fail (Err_fwrite(Buffer1, c, out))
	    pthrow prcserror << "Write failure" << perror;
    }

    return NoError;
}

bool empty_file(FILE* empty)
{
    int c;

    opt_buffer_1(-1);

    do {
      If_fail (c << Err_fread(Buffer1, Buffer1Len, empty))
	c = -1;
    } while (c > 0);

    return true;
}

PrVoidError fs_zero_file(const char* name)
{
    /* use open so that if FILE doesn't exist create it. */
    If_fail(Err_open(name, O_TRUNC|O_CREAT, 0644))
        pthrow prcserror << "Failed truncating file "<< squote(name) << perror;

    return NoError;
}

NprVoidError write_fds(int infd, int outfd)
{
    int c;

    opt_buffer_1(infd);

    do {
	If_fail(c << Err_read(infd, Buffer1, Buffer1Len))
	    return ReadFailure;

	If_fail(Err_write(outfd, Buffer1, c))
	    return WriteFailure;

    } while (c);

    return NoError;
}

bool fs_same_file_owner(const char* file)
{
    struct stat buf;

    if(stat(file, &buf) < 0)
        return false;

    return get_user_id() == buf.st_uid;
}

NprBoolError cmp_fds(int one, int two)
{
    int nRead1 = 1, nRead2 = 1, len;

    opt_buffer_1(one);
    opt_buffer_2(two);

    len = Buffer1Len < Buffer2Len ? Buffer1Len : Buffer2Len;

    while(true) {
	If_fail(nRead1 << Err_read(one, Buffer1, len))
	    return ReadFailure;
	If_fail(nRead2 << Err_read(two, Buffer2, len))
	    return ReadFailure;
	if (nRead1 != nRead2)
	  return true;
	if (nRead1 == 0)
	  return false;
        if(memcmp(Buffer1, Buffer2, nRead1) != 0)
            return 1;
    }

    return nRead1 != nRead2;
}

NprVoidError read_string(FILE* f, Dstring* s)
{
    while(true) {
	int c;

	Return_if_fail(c << Err_fgetc(f));

	if (c == EOF)
	  return ReadFailure;

	if (c == '\n')
	    break;

	s->append((char)c);
    }

    return NoError;
}

NprCFilePtrError Err_fopen(const char* file, const char* type)
{
    FILE* f = fopen(file, type);

    if(f)
	return f;
    else
	return NonFatalError;
}

NprVoidError Err_fclose(FILE* f)
{
    if(fclose(f) < 0)
	return NonFatalError;
    else
	return NoError;
}

NprVoidError Err_unlink(const char* a)
{
    if(unlink(a) < 0 && errno != ENOENT)
	return NonFatalError;
    else
	return NoError;
}

NprVoidError Err_rename(const char* a, const char* b)
{
    if(rename(a, b) < 0)
	return NonFatalError;
    else
	return NoError;
}

NprIntError Err_fread(void* buf, size_t nbytes, FILE* f)
{
  int c;

  DEBUG ("Err_fread (" << nbytes << ")");

  do {
    errno = 0;
    clearerr(f);
    c = fread (buf, 1, nbytes, f);

    DEBUG (c << " = fread (" << nbytes << ")");
    if (errno)
      DEBUG ("errno = " << errno);
    if (ferror(f))
      DEBUG ("ferror(f) = " << ferror (f));

  } while (c <= 0 && ferror(f) && errno == EINTR);

  if (c <= 0 && ferror (f))
    return ReadFailure;
  else
    return c;
}

NprVoidError Err_fwrite(const void* buf, size_t nbytes, FILE* out)
{
  int c;

  DEBUG ("Err_fwrite (" << nbytes << ")");

  while (nbytes > 0) {
    do {
      errno = 0;
      clearerr(out);
      c = fwrite (buf, 1, nbytes, out);

      DEBUG (c << " = fwrite (" << nbytes << ")");
      if (errno)
	DEBUG ("errno = " << errno);
      if (ferror(out))
	DEBUG ("ferror(out) = " << ferror (out));

    } while (c <= 0 && ferror(out) && errno == EINTR);

    if (c <= 0 && ferror(out))
      return WriteFailure;

    nbytes -= c;
    buf = ((const char*)buf) + c;
  }

  return NoError;
}

NprIntError Err_read(int fd, void* buf, size_t nbytes)
{
    int c;

    do {
	c = read(fd, buf, nbytes);
    } while (c < 0 && errno == EINTR);

    if (c < 0)
	return ReadFailure;
    else
	return c;
}

NprVoidError Err_read_expect(int fd, void* buf, size_t nbytes)
{
    int c;

    while(nbytes > 0) {
	Return_if_fail(c << Err_read(fd, buf, nbytes));

	nbytes -= c;
    }

    return NoError;
}

NprVoidError Err_write(int fd, const void* buf, size_t nbytes)
{
    int c;

    while (nbytes > 0) {
	do {
	    c = write(fd, buf, nbytes);
	} while (c < 0 && errno == EINTR);

	if (c < 0)
	    return WriteFailure;

	nbytes -= c;
	buf = ((const char*)buf) + c;
    }
    return NoError;
}

NprIntError Err_open(const char* a, int flags, mode_t mode)
{
    int fd;
    fd = open(a, flags, mode);
    if(fd < 0)
	return NonFatalError;
    else
	return fd;
}

NprVoidError Err_close(int fd)
{
    if(close(fd) < 0)
	return NonFatalError;
    else
	return NoError;
}

NprVoidError Err_mkdir(const char* name, mode_t mode)
{
    ASSERT(name[strlen(name) - 1] != '/', "This fails on some operating systems, "
	   "notably old versions of Linux");

    if(mkdir(name, mode) < 0)
	return NonFatalError;
    else
	return NoError;
}

NprVoidError Err_chmod(const char* name, mode_t mode)
{
    if(chmod(name, mode) < 0)
	return NonFatalError;
    else
	return NoError;
}

NprVoidError Err_chown(const char* name, uid_t uid, gid_t gid)
{
    if(chown(name, uid, gid) < 0)
	return NonFatalError;
    else
	return NoError;
}

NprVoidError Err_utime(const char* name, const struct utimbuf *ut)
{
    if(utime(name, ut) < 0)
	return NonFatalError;
    else
	return NoError;
}

NprVoidError Err_fstat(int fd, struct stat* buf)
{
    if(fstat(fd, buf) < 0)
	return NonFatalError;
    else
	return NoError;
}

NprVoidError Err_stat(const char* file, struct stat* buf)
{
    if(stat(file, buf) < 0)
	return NonFatalError;
    else
	return NoError;
}

NprVoidError Err_symlink(const char* a, const char* b)
{
    if(symlink(a, b) < 0)
	return NonFatalError;
    else
	return NoError;
}

NprIntError Err_waitpid(int pid, pid_t* pid_ret, bool nohang)
{
    int status;
    pid_t ret;

    while (true) {
        while ((ret = waitpid(pid, &status, (nohang ? WNOHANG : 0))) < 0 && errno == EINTR) { }

        if (WIFSTOPPED(status))
            kill(SIGCONT, pid);
        else
            break;
    }

    if (pid_ret)
	(*pid_ret) = ret;

    if (ret < 0)
	return FatalError;
    else
	return status;
}

NprIntError Err_waitpid_nostart(int pid, pid_t* pid_ret, bool nohang)
{
    int status;
    pid_t ret;

    while (true) {
        while ((ret = waitpid(pid, &status, (nohang ? WNOHANG : 0))) < 0 && errno == EINTR) { }

        if (WIFSTOPPED(status))
            sleep(1);
        else
            break;
    }

    if (pid_ret)
	(*pid_ret) = ret;

    if (ret < 0)
	return FatalError;
    else
	return status;
}

NprIntError Err_fgetc(FILE* f)
{
    int c;

    while ((c = fgetc(f)) == EOF && errno == EINTR) { }

    if (c == EOF && ferror(f))
	return ReadFailure;
    else
	return c;
}

NprVoidError Err_creat(const char* name, mode_t mode)
{
    int s = open (name, O_CREAT | O_TRUNC, mode);

    if (s < 0)
	return FatalError;

    return NoError;
}

char* p_strdup(const char* name)
{
    char *s = NEWVEC(char, strlen(name)+1);

    strcpy(s, name);

    return s;
}

PrConstCharPtrError find_real_filename(const char* filename)
{
    static char buf[MAXPATHLEN];
    struct stat sbuf;

    if (lstat(filename, &sbuf) < 0)
	pthrow prcserror << "Lstat failed on "
			<< squote(filename)
			<< perror;

    if (S_ISLNK(sbuf.st_mode)) {
	const char* sym;

	Return_if_fail(sym << read_sym_link(filename));

	if (sym[0] == '/') {
	    return find_real_filename(sym);
	} else {
	    Dstring directory;

	    dirname(filename, &directory);

	    directory.append('/');
	    directory.append(sym);

	    return find_real_filename(directory);
	}

    } else {
	strncpy(buf, filename, MAXPATHLEN-1);
	buf[MAXPATHLEN-1] = 0;

	return buf;
    }
}

void dirname(const char* p, Dstring* s)
{
    const char* last_slash = strrchr(p, '/');

    if (!last_slash)
	s->assign("./");
    else
	s->append(p, last_slash - p);
}

PrVoidError WriteableFile::copy(FILE* copy_me)
{
    opt_buffer_1(-1);

    int c;

    for (;;) {
        If_fail (c << Err_fread(Buffer1, Buffer1Len, copy_me))
	  pthrow prcserror << "Read failure" << perror;

	if (c == 0)
	  break;

	os.write (Buffer1, c);

	if (os.bad())
	    pthrow prcserror << "Error writing " << squote(temp_name) << perror;
    }

    return NoError;
}

PrVoidError WriteableFile::copy(const char* copy_me)
{
    FILE* it;

    If_fail(it << Err_fopen(copy_me, "r"))
	pthrow prcserror << "Open failed on " << squote(copy_me) << perror;

    PrVoidError ret = copy(it);

    fclose(it);

    return ret;
}

const char* major_version_of(const char* name)
{
    static Dstring* result = NULL;

    if (!result)
	result = new Dstring;

    const char* last_period = strrchr(name, '.');

    if (!last_period)
	return NULL;

    result->assign (name);
    result->truncate (last_period - name);

    return result->cast();
}

const char* minor_version_of(const char* name)
{
    static Dstring* result = NULL;

    if (!result)
	result = new Dstring;

    const char* last_period = strrchr(name, '.');

    if (!last_period)
	return NULL;

    result->assign(last_period + 1);

    return result->cast();
}

const char* get_environ_var (const char* var)
{
    static KeywordTable env_table;
    static bool once = false;
    const char* env;

    if (!once) {
	once = true;

	for (int i = 0; i < env_names_count; i += 1) {
	    env = getenv (env_names[i].var);

	    if (!env)
		env = env_names[i].defval;

	    if (env && !env[0])
	      env = NULL;

	    env_table.insert (env_names[i].var, env);
	}
    }

    const char** lu = env_table.lookup (var);

    if (lu)
	return *lu;

    env = getenv (var);

    if (env && *env) {
	env_table.insert (var, env);

	return env;
    }

    return NULL;
}

Dir::Dir(const char* path)
{
    finished = false;
    thisdir = opendir(path);
    full_name.assign(path);
    full_name_len = full_name.length();
    Dir_next();
}

Dir::~Dir()
{
    Dir_close();
}

bool Dir::Dir_open(const char* path)
{
    Dir_close();
    thisdir = opendir(path);
    full_name.assign(path);
    full_name_len = full_name.length();
    Dir_next();
    return OK();
}

bool Dir::OK() const
{
    return thisdir != NULL;
}

const char* Dir::Dir_entry() const
{
    return entry->d_name;
}

const char* Dir::Dir_full_entry()
{
    if(!finished) {
	full_name.truncate(full_name_len);
	full_name.append('/');
	full_name.append(entry->d_name);
    }
    return full_name;
}

bool Dir::Dir_next()
{
    const char* name;
    if(thisdir == NULL) {
	finished = true;
	return false;
    }

    if(finished)
	return false;

    if((entry = readdir(thisdir)) == NULL) {
	finished = true;
	return false;
    }

    name = Dir_entry();
    if(name[0] == '.' &&
       (name[1] == '\0' ||
	(name[1] == '.' && name[2] == '\0')))
	return Dir_next();
    return true;
}

bool Dir::Dir_finished() const
{
    return finished;
}

void Dir::Dir_close()
{
    if(thisdir) {
	closedir(thisdir);
	thisdir = NULL;
    }
}

Dir::DirIterator::DirIterator(Dir& d0) { dp = &d0; }
const char* Dir::DirIterator::operator*() const { return dp->Dir_entry(); }
void Dir::DirIterator::next() { dp->Dir_next(); }
bool Dir::DirIterator::finished() const { return dp->Dir_finished(); }

Dir::FullDirIterator::FullDirIterator(Dir& d0) { dp = &d0; }
const char* Dir::FullDirIterator::operator*() const { return dp->Dir_full_entry(); }
void Dir::FullDirIterator::next() { dp->Dir_next(); }
bool Dir::FullDirIterator::finished() const { return dp->Dir_finished(); }
