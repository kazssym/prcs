/* -*-Mode: C++;-*-
 * PRCS - The Project Revision Control System
 * Copyright (C) 1997  Josh MacDonald
 * Copyright (C) 1994  P. N. Hilfinger
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

/* This file contains a bunch of stuff, including some really super ugly
 * stuff for making PRCS compile on SunOS 4.1.x.  The problem with that
 * system is that the system header files are brain-damaged. */

#ifndef UTILS_H
#define UTILS_H

#include <sys/types.h>
#include <sys/stat.h>
#include "config.h"

#ifdef __cplusplus
#define EXTERN extern "C"
#else
#define EXTERN extern
#endif

/*
 * ASSERT --
 *
 *     Josh's ASSERT macro, allows a message to be printed, just in
 *     case you want to forget why you put an assertion in in the
 *     first place.  */
#ifndef NDEBUG
EXTERN void abort(void);
#include <stdio.h>
#if defined(__GNUG__) && defined(PRCS_DEVEL)
#define ASSERT(condition, message) \
if (!(condition)) { \
    fprintf(stderr, "Assertion (%s) failed: %s\n" \
            "%s:%d: in function %s.\n", #condition, message, \
            __FILE__, __LINE__, __PRETTY_FUNCTION__); \
    abort(); }
#else
#define ASSERT(condition, message) \
if (!(condition)) { \
    fprintf(stderr, "%s:%d: assertion \"%s\" failed.\n", \
            __FILE__, __LINE__, #condition); \
    abort(); }
#endif
#else
#define ASSERT(condition, mes) ((void) 0)
#endif

#if defined(sun) && !defined(__SVR4) || defined(ultrix)
struct tm;

#ifdef sun
EXTERN int atexit (void (*) (void));
EXTERN long time(time_t* t);
#endif
#include <utime.h>
EXTERN int utime(const char*, struct utimbuf*);

EXTERN char *getwd(char*);
EXTERN int system (const char *);
EXTERN int getopt(int ac, char* const av[], const char *optstring);
EXTERN int unsetenv(const char*);
EXTERN char *optarg;
EXTERN int optind, opterr;
EXTERN pid_t getpid(void);
EXTERN pid_t getppid(void);
EXTERN int readlink(const char*, char*, int);
EXTERN int symlink(const char*, const char*);
EXTERN int lstat(const char*, struct stat*);
EXTERN char* strptime(char*, char*, struct tm*);
EXTERN char* strcat(char*, const char*);

#ifdef FILE
EXTERN int fclose (FILE *);
EXTERN int fflush (FILE *);
EXTERN int fgetc (FILE *);
EXTERN int fgetpos (FILE *, long *);
EXTERN int fprintf (FILE *, const char *, ...);
EXTERN int fputc (int, FILE *);
EXTERN int fputs (const char *, FILE *);
EXTERN long unsigned int fread (void *, long unsigned int , long unsigned int , FILE *);
EXTERN int fscanf (FILE *, const char *, ...);
EXTERN int fseek (FILE *, long int, int);
EXTERN int fsetpos (FILE *, const long *);
EXTERN long unsigned int fwrite (const void *, long unsigned int , long unsigned int , FILE *);
EXTERN void perror (const char *);
EXTERN int printf (const char *, ...);
EXTERN int puts (const char *);
EXTERN int remove (const char *);
EXTERN int rename (const char *, const char *);
EXTERN void rewind (FILE *);
EXTERN int scanf (const char *, ...);
EXTERN void setbuf (FILE *, char *);
EXTERN int setvbuf (FILE *, char *, int, long unsigned int );
EXTERN int sscanf (const char *, const char *, ...);
#ifndef vprintf
EXTERN int vprintf (const char *, char * );
#endif
#ifndef vsprintf
EXTERN int vsprintf (char *, const char *, char * );
#endif
#ifndef vfprintf
EXTERN int vfprintf (FILE *, const char *, char * );
#endif
EXTERN int ungetc (int, FILE *);
EXTERN int _flsbuf (unsigned int, FILE *);
EXTERN int _filbuf (FILE *);
EXTERN int unlink(const char *filename);
#endif /* FILE */

EXTERN int strncasecmp(const char*, const char*, int n);
#ifdef __string_h
EXTERN void * memchr (const void *, int, long unsigned int );
EXTERN int memcmp (const void *, const void *, long unsigned int );
EXTERN void * memcpy (void *, const void *, long unsigned int );
#ifndef memmove
EXTERN void * memmove (void *, const void *, long unsigned int );
#endif
EXTERN void * memset (void *, int, long unsigned int );
EXTERN int strcoll (const char *, const char *);
EXTERN char * strerror (int);
#endif

EXTERN int access(const char *psth, int mode);
EXTERN int mkstemp(char* temp);
EXTERN int dup2(int fd1, int fd2);
EXTERN int close(int fd);
EXTERN int ftruncate(int fd, size_t length);
EXTERN int truncate(const char* name, size_t length);
EXTERN int fchown(int fd, uid_t owner, gid_t group);
EXTERN int fchmod(int fd, mode_t mode);
EXTERN char *mktemp(char *temp);

EXTERN char *gethostname();
EXTERN void bzero(void *b, size_t len);

#endif /* sun */

EXTERN void *_malloc_(size_t size);
EXTERN void *_mrealloc_(void *V, int size, int N, int INCR);
EXTERN void *_malloc0_(size_t size);
EXTERN void *_mrealloc0_(void *V, int size, int N, int INCR);

#define NEWVEC(T,N) ((T *) _malloc_((unsigned int) (N) * sizeof(T)))

#define EXPANDVEC(V, N, INCR) _mrealloc_(V, sizeof(*(V)), (N), (INCR))

#define NEWVEC0(T,N) ((T *) _malloc0_((unsigned int) (N) * sizeof(T)))

#define EXPANDVEC0(V, N, INCR) _mrealloc0_(V, sizeof(*(V)), (N), (INCR))

#endif /* UTILS_H */
