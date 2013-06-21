dnl -*-Mode: C; comment-start: "dnl "-*-
dnl Process this file with autoconf to produce a configure script.
AC_PREREQ([2.69])
AC_INIT([PRCS], [1.3.4k1],, [prcs])
AC_CONFIG_SRCDIR([src/prcs.cc])
AC_CONFIG_HEADERS([config.h])
AC_CANONICAL_HOST

dnl $Format: "PRCS_MAJOR_VERSION_NUMBER=$ReleaseMajorVersion$" $
PRCS_MAJOR_VERSION_NUMBER=1
dnl $Format: "PRCS_MINOR_VERSION_NUMBER=$ReleaseMinorVersion$" $
PRCS_MINOR_VERSION_NUMBER=3
dnl $Format: "PRCS_MICRO_VERSION_NUMBER=$ReleaseMicroVersion$" $
PRCS_MICRO_VERSION_NUMBER=4
AC_DEFINE_UNQUOTED([PACKAGE_VERSION_MAJOR], [$PRCS_MAJOR_VERSION_NUMBER],
[Define to the major version number.])
AC_DEFINE_UNQUOTED([PACKAGE_VERSION_MINOR], [$PRCS_MINOR_VERSION_NUMBER],
[Define to the minor version number.])
AC_DEFINE_UNQUOTED([PACKAGE_VERSION_MICRO], [$PRCS_MICRO_VERSION_NUMBER],
[Define to the micro version number.])

AM_INIT_AUTOMAKE

AC_SUBST(PRCS_MAJOR_VERSION_NUMBER)
AC_SUBST(PRCS_MINOR_VERSION_NUMBER)
AC_SUBST(PRCS_MICRO_VERSION_NUMBER)

DEBUGFLAG=

AC_ARG_ENABLE(debug, [  --enable-debug          turn on debugging],
if eval "test x$enable_debug = xyes"; then
  AC_DEFINE([PRCS_DEVEL], 1, [Define for developer features.])
  DEBUGFLAG=-g
fi)

AC_ARG_ENABLE(environment, [  --disable-environment   disable default build-time environment variable values],
if eval "test x$enable_environment = xyes"; then
  ENVVARS=yes
else
  ENVVARS=no
fi
,
ENVVARS=yes)

AC_ARG_ENABLE(lex-check, [  --disable-lex-check     disable check for lex.  Lex (or flex) is needed for
                          building .c files from .l files, and only
                          maintainers need this.],
if eval "test x$enable_lex_check = xyes"; then
  DO_LEX_CHECK=yes
else
  DO_LEX_CHECK=no
fi
,
DO_LEX_CHECK=yes)

if test -z "$CFLAGS";   then test_CFLAGS=set;   fi
if test -z "$CPPFLAGS"; then test_CPPFLAGS=set; fi
if test -z "$CXXFLAGS"; then test_CXXFLAGS=set; fi

AC_PROG_CC
AC_PROG_CXX
AC_PROG_CPP
gl_EARLY

if test "$test_CFLAGS" = set; then
  CFLAGS=
fi

if test "$test_CPPFLAGS" = set; then
  CPPFLAGS=
fi

if test "$test_CXXFLAGS" = set; then
  CXXFLAGS=
fi

if test -n "$DEBUGFLAG"; then
  CFLAGS="$DEBUGFLAG $CFLAGS"
  CXXFLAGS="$DEBUGFLAG $CXXFLAGS"
else
  if echo "$CFLAGS" | grep "\-O" > /dev/null 2> /dev/null; then
    true
  else
    CFLAGS="$CFLAGS -O2"
  fi
  if echo "$CXXFLAGS" | grep "\-O" > /dev/null 2> /dev/null; then
    true
  else
    CXXFLAGS="$CXXFLAGS -O2"
  fi
fi

if eval "test x$GXX = xyes"; then
  if echo "$CXXFLAGS" | grep "\-Wall" > /dev/null 2> /dev/null; then
    true
  else
    CXXFLAGS="$CXXFLAGS -Wall"
  fi
fi

if eval "test x$LIBGPLUSPLUS != x -a x$GCC = xyes"; then
  CXXFLAGS="-nostdinc++ -I$LIBGPLUSPLUS/g++-include $CXXFLAGS"
  LDFLAGS="-L$LIBGPLUSPLUS $LDFLAGS"
fi
	  
dnl Checks for header files.
AC_HEADER_DIRENT
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS(limits.h sys/ioctl.h unistd.h \
		 bstring.h sys/select.h sys/stream.h)
gl_INIT

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_UID_T
AC_TYPE_MODE_T
AC_TYPE_OFF_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_STRUCT_ST_BLKSIZE
AC_STRUCT_TM
AC_FUNC_ALLOCA
AC_FUNC_VFORK
AC_PROG_GCC_TRADITIONAL
AC_FUNC_MEMCMP

AC_CHECK_SIZEOF(long, 4)
AC_CHECK_SIZEOF(short, 4)
AC_CHECK_SIZEOF(int, 4)

AC_FUNC_MMAP

AC_TYPE_SIGNAL
AC_CHECK_FUNCS(getcwd gethostname getwd mkdir fork rmdir strerror strtol uname)

# BeOS hides the fd_set in <socket.h>, but it's there.
case $host in
*beos*)
    ac_cv_no_fd_set=no ;;
esac

AC_CACHE_CHECK([whether fd_set type is missing], ac_cv_no_fd_set, [
AC_TRY_COMPILE([
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#if HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#if HAVE_BSTRING_H
#include <bstring.h>
#endif
],
[fd_set read_set;],
ac_cv_no_fd_set=no,
ac_cv_no_fd_set=yes)])

if test $ac_cv_no_fd_set = yes; then
  AC_DEFINE([NO_FD_SET], 1, [Define if headers lack FDSET.])
fi

AC_LANG_CPLUSPLUS

AC_CHECK_HEADERS(std.h)

dnl
dnl Checking for stupidity (HPUX, etc.).
dnl

ORIG_CXXFLAGS=$CXXFLAGS
CXXFLAGS="$CXXFLAGS -Werror"

AC_CACHE_CHECK([select fd_set type], ac_cv_select_type, [
AC_TRY_RUN([
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <stdlib.h>
#include <unistd.h>
#if HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#if HAVE_BSTRING_H
#include <bstring.h>
#endif
#ifdef HAVE_SYS_STREAM_H
#include <sys/stream.h>
#endif
#if NO_FD_SET
typedef struct {
  int set[32]; /* sizeof(int)*32*8 files */
} FD_SET_TYPE;
#else
typedef fd_set FD_SET_TYPE;
#endif

int
main ()
{
  struct timeval tv; tv.tv_sec = 0; tv.tv_usec = 0;
  FD_SET_TYPE read_fds;
  FD_ZERO(&read_fds);
  exit (select (3, &read_fds, NULL, NULL, &tv));
}],
ac_cv_select_type=FD_SET_TYPE,
[AC_TRY_RUN([
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <stdlib.h>
#include <unistd.h>
#if HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#if HAVE_BSTRING_H
#include <bstring.h>
#endif
#ifdef HAVE_SYS_STREAM_H
#include <sys/stream.h>
#endif
#if NO_FD_SET
typedef struct {
  int set[32]; /* sizeof(int)*32*8 files */
} FD_SET_TYPE;
#else
typedef fd_set FD_SET_TYPE;
#endif

int
main ()
{
  struct timeval tv; tv.tv_sec = 0; tv.tv_usec = 0;
  FD_SET_TYPE read_fds;
  FD_ZERO(&read_fds);
  exit (select (3, (int*)(&read_fds), NULL, NULL, &tv));
}],
ac_cv_select_type=int,
[AC_TRY_RUN([
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <stdlib.h>
#include <unistd.h>
#if HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#if HAVE_BSTRING_H
#include <bstring.h>
#endif
#ifdef HAVE_SYS_STREAM_H
#include <sys/stream.h>
#endif
#if NO_FD_SET
typedef struct {
  int set[32]; /* sizeof(int)*32*8 files */
} FD_SET_TYPE;
#else
typedef fd_set FD_SET_TYPE;
#endif

int
main ()
{
  struct timeval tv; tv.tv_sec = 0; tv.tv_usec = 0;
  FD_SET_TYPE read_fds;
  FD_ZERO(&read_fds);
  exit (select (3, (void*)(&read_fds), NULL, NULL, &tv));
}],
ac_cv_select_type=void,
ac_cv_select_type=FD_SET_TYPE)
])
],
ac_cv_select_type=FD_SET_TYPE)])

AC_DEFINE_UNQUOTED(SELECT_TYPE, $ac_cv_select_type, [Defines the FDSET type.])

CXXFLAGS=$ORIG_CXXFLAGS

dnl end select check.

AC_DEFINE(EXPLICIT_TEMPLATES, 1, [Used with -fno-implicit-templates.])

dnl
dnl Test working fnmatch.  Solaris 5.4's doesn't work.
dnl

AC_CACHE_CHECK([for working fnmatch function], ac_cv_sys_working_fnmatch,
[AC_TRY_RUN([
#include <stdlib.h>
#include <fnmatch.h>
int
main ()
{
  exit ((fnmatch ("a", "a", FNM_PATHNAME) == 0
	 && fnmatch ("a", "b", FNM_PATHNAME) == FNM_NOMATCH
	 && fnmatch ("*", "Solaris Sucks", FNM_NOESCAPE) == 0)
	? 0 : 1);
}],
ac_cv_sys_working_fnmatch=yes,
ac_cv_sys_working_fnmatch=no,
ac_cv_sys_working_fnmatch=no)])

if test $ac_cv_sys_working_fnmatch = no; then
  AC_LIBOBJ([fnmatch])
  AC_DEFINE(USE_SYS_FNMATCH, 1, [System has fnmatch.])
fi

dnl
dnl Need to know for c++ the arg type to signal handlers.
dnl

# Metrowerks/BeOS compiles '...' as signal arg type without complains,
# but the generated code messes up the stack on return. So we force
# to be 'int'.
case $host in
*beos*)
    ac_cv_signal_arg_type=int ;;
esac

AC_CACHE_CHECK([for signal handler argument type], ac_cv_signal_arg_type,
[AC_TRY_RUN([
#include <stdlib.h>
#include <signal.h>

RETSIGTYPE
handler(...) { }

int
main ()
{
   signal(SIGPIPE, handler);
   exit(0);
}],
ac_cv_signal_arg_type=...,
AC_TRY_RUN([
#include <stdlib.h>
#include <signal.h>

RETSIGTYPE
handler(void) { }

int
main ()
{
   signal(SIGPIPE, handler);
   exit(0);
}],
ac_cv_signal_arg_type=void,
ac_cv_signal_arg_type=int,
ac_cv_signal_arg_type=int),
ac_cv_signal_arg_type=int)])

AC_DEFINE_UNQUOTED(SIGNAL_ARG_TYPE, $ac_cv_signal_arg_type, [Argument type for signal().])

dnl
dnl End of my tests, back to C.
dnl

AC_LANG_C

AC_PROG_RANLIB

if eval "test x$DO_LEX_CHECK = xyes"; then
AM_PROG_LEX
fi

OLDPATH=$PATH
dnl Add /usr/ucb for whoami on Solaris
PATH=$PATH:/usr/ucb
dnl For josh to build binaries with.
if test `whoami` = jmacd; then
  PATH=/usr/sww/bin/GNU:/usr/local/bin:$PATH
fi

dnl Use gdiff, gdiff3, and gtar, if available
dnl Require 9 command paths for compiling in.
if test -z "$SYS_RCS_COMMAND_PATH"; then
  AC_PATH_PROG(SYS_RCS_COMMAND_PATH, rcs, /usr/local/bin/rcs)
fi
if test -z "$SYS_CI_COMMAND_PATH"; then
  AC_PATH_PROG(SYS_CI_COMMAND_PATH, ci, /usr/local/bin/ci)
fi
if test -z "$SYS_CO_COMMAND_PATH"; then
  AC_PATH_PROG(SYS_CO_COMMAND_PATH, co, /usr/local/bin/co)
fi
if test -z "$SYS_RLOG_COMMAND_PATH"; then
  AC_PATH_PROG(SYS_RLOG_COMMAND_PATH, rlog, /usr/local/bin/rlog)
fi

if test -z "$SYS_GDIFF_COMMAND_PATH"; then
  AC_PATH_PROG(SYS_GDIFF_COMMAND_PATH, diff, /usr/local/bin/diff)
fi
if test -z "$SYS_GDIFF3_COMMAND_PATH"; then
  AC_PATH_PROG(SYS_GDIFF3_COMMAND_PATH, diff3, /usr/local/bin/diff3)
fi
if test -z "$SYS_TAR_COMMAND_PATH"; then
  AC_PATH_PROG(SYS_TAR_COMMAND_PATH, tar, /bin/tar)
fi
if test -z "$SYS_LS_COMMAND_PATH"; then
  AC_PATH_PROG(SYS_LS_COMMAND_PATH, ls, /bin/ls)
fi
if test -z "$SYS_GZIP_COMMAND_PATH"; then
  AC_PATH_PROG(SYS_GZIP_COMMAND_PATH, gzip, /usr/local/bin/gzip)
fi

WARNED=no

case "`$SYS_RCS_COMMAND_PATH -V | head -1`" in
# These patterns should match 5.8 and later.
[*GNU\ RCS*5.[89]] | [*GNU\ RCS*5.[89].*] | [*GNU\ RCS*5.[1-9][0-9]*]) ;;
*)
echo ""
echo "* Warning: The RCS command $SYS_RCS_COMMAND_PATH is not version 5.8+,"
echo "* you can to remove the file config.cache, install the correct"
echo "* version of RCS, and reconfigure with the new version first in your"
echo "* path.  Alternatively, you can proceed and use the RCS_PATH"
echo "* environment variable to set the path for RCS commands."
echo ""
WARNED=yes ;;
esac

gdiff_ver="`$SYS_GDIFF_COMMAND_PATH -v | grep diffutils`"
if test "$gdiff_verX" == "X"; then
echo ""
echo "* Warning: The DIFF command $SYS_GDIFF_COMMAND_PATH is not version 2.7,"
echo "* you can to remove the file config.cache, install the correct"
echo "* version of diff, and reconfigure with the new version first in your"
echo "* path.  Alternatively, you can proceed and use the GDIFF_PATH"
echo "* environment variable to set the path for DIFF commands."
echo ""
WARNED=yes
fi

PATH=$OLDPATH

dnl For installing prcs.el; also substs EMACS for doc/Makefile.am
AM_PATH_LISPDIR

SYS_RCS_COMMAND_PATH=\"$SYS_RCS_COMMAND_PATH\"
SYS_CI_COMMAND_PATH=\"$SYS_CI_COMMAND_PATH\"
SYS_CO_COMMAND_PATH=\"$SYS_CO_COMMAND_PATH\"
SYS_RLOG_COMMAND_PATH=\"$SYS_RLOG_COMMAND_PATH\"
SYS_GDIFF_COMMAND_PATH=\"$SYS_GDIFF_COMMAND_PATH\"
SYS_GDIFF3_COMMAND_PATH=\"$SYS_GDIFF3_COMMAND_PATH\"
SYS_TAR_COMMAND_PATH=\"$SYS_TAR_COMMAND_PATH\"
SYS_LS_COMMAND_PATH=\"$SYS_LS_COMMAND_PATH\"
SYS_GZIP_COMMAND_PATH=\"$SYS_GZIP_COMMAND_PATH\"

AC_SUBST(SYS_RCS_COMMAND_PATH)
AC_SUBST(SYS_CI_COMMAND_PATH)
AC_SUBST(SYS_CO_COMMAND_PATH)
AC_SUBST(SYS_RLOG_COMMAND_PATH)
AC_SUBST(SYS_GDIFF_COMMAND_PATH)
AC_SUBST(SYS_GDIFF3_COMMAND_PATH)
AC_SUBST(SYS_TAR_COMMAND_PATH)
AC_SUBST(SYS_LS_COMMAND_PATH)
AC_SUBST(SYS_GZIP_COMMAND_PATH)

PRCS_DEFAULT_ENV_VARS

AC_OUTPUT(Makefile libgnu/Makefile src/Makefile doc/Makefile src/include/Makefile contrib/Makefile emacs/Makefile be/Makefile src/docs.cc)

if test $WARNED = yes; then
echo ""
echo "* Warnings were generated during configure.  You may proceed building"
echo "* PRCS, but you probably want to fix the problem before running it."
echo "* If you think configure found the wrong paths for the system binaries,"
echo "* you may with to edit the file 'config.h' before running make."
echo "* After building, be sure and run the command 'prcs config' to"
echo "* see if the problem persists. "
echo ""
fi
