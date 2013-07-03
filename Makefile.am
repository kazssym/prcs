## Process this file with automake to produce Makefile.in
## $Id: Makefile.am 1.9.1.29 Sun, 09 May 2004 19:20:56 -0700 jmacd $

ACLOCAL_AMFLAGS = -Im4

SUBDIRS = libgnu src test doc contrib emacs be

EXTRA_DIST = \
	FAQ \
	README.BE \
	config-be \
	README-original \
	m4/gnulib-cache.m4

#if GCC3PATCH
#PATCHFILE = prcs-1.3gcc3.diffs
#BUILT_SOURCES = patch-stamp
#patch-stamp:
#	patch -p1 < ${PATCHFILE} && touch patch-stamp
#clean-local:
#	if test -f patch-stamp; then patch -p1 --reverse < ${PATCHFILE} && rm -f patch-stamp; fi
#endif

## This doesn't work with gmake -j, but I can't figure it out.
all-local:
	@if src/prcs config -f; then :; else \
	  echo ""; \
	  echo "* Warning: prcs config reports a configuration error, probably"; \
	  echo "* indicating that one of the programs prcs uses is the wrong"; \
	  echo "* version.  You may set the RCS_PATH environment variable to"; \
	  echo "* correct this problem."; \
	  echo ""; \
	fi

bindist: all
	strip src/prcs
	tar -cf - INSTALL NEWS README TODO FAQ -C src prcs | \
	  gzip > prcs-@VERSION@-`uname`.tar.gz

## I think this is a neat use of execute.  It shows one reason to make
## prcs operations on project D/P work as if files had D prepended instead
## of changing into D and using the same filenames.

develdist:
	@rm -rf prcs-@VERSION@
	@ln -s . prcs-@VERSION@
	@rm -rf prcs-@VERSION@-devel.tar.gz
	@rm -rf prcs-@VERSION@-devel.tar
	tar -cf prcs-@VERSION@-devel.tar `prcs execute -f --not :.*directory prcs-@VERSION@`
	gzip prcs-@VERSION@-devel.tar
	@rm -rf prcs-@VERSION@
