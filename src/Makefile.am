SUBDIRS = include

AM_CPPFLAGS = -I$(srcdir)/include -I../libgnu

EXTRA_DIST = README.GNU.SOURCES prcserror.tl dynarray.tl hash.tl \
	     prcs-keys.c prcs-keys.gperf prj-names.h prj-names.gperf

bin_PROGRAMS = prcs

BUILT_SOURCES = prcs-keys.c prj-names.h

prcs_LDADD = @LIBOBJS@ ../libgnu/libgnu.a

prcs_DEPENDENCIES = @LIBOBJS@

prcs_SOURCES = prcs.cc changes.cc sexp.cc fileent.cc projdesc.cc checkin.cc \
	checkout.cc repository.cc populate.cc syscmd.cc vc.cc diff.cc \
	info.cc misc.cc package.cc merge.cc lock.cc rebuild.cc \
	prcserror.cc convert.cc memseg.cc prcsver.cc setkeys.cc \
	quick.cc rekey.cc dstring.cc dynarray.cc hash.cc execute.cc \
	docs.cc attrs.cc utils.c getopt.c getopt1.c md5c.c maketime.c \
	partime.c vclex.l prcs-keys.c prjlex.l regex.c

## This appeared to fail for unknown reasons.
#include/docs.h docs.cc.in: ../doc/prcs.texi ../doc/make-docs.el
#	$(EMACS) --batch -l $(srcdir)/../doc/make-docs.el -f make-docs-noargs

misc.cc prcs.cc: include/docs.h
projdesc.cc: prj-names.h

JGPERF=jgperf

prj-names.h: prj-names.gperf
	$(JGPERF) -A -G -C -a -t -T -p \
		-N ProjectDescriptor::prj_lookup_func \
		-H ProjectDescriptor::prj_lookup_hash \
		-W ProjectDescriptor::_pftable \
		< $< > $@

prcs-keys.c: prcs-keys.gperf
	$(JGPERF) -A -C -a -r -k1,9 \
		-N is_builtin_keyword \
		-H is_builtin_keyword_hash \
		< $< > $@

# gperf sucks way too much for me to fix this
#attrs.cc: attrs.gperf
#	$(JGPERF) -A -G -C -a -t -T -p \
#		-N is_file_attribute \
#		-H is_file_attribute_hash \
#		< $< > $@
