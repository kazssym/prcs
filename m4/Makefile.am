M4S = prcsenv.m4 prcsbuild.m4 jmmmap.m4

EXTRA_DIST = $(M4S)

m4: $(M4S)
	cat $^ > ../acinclude.m4

prcsenv.m4: ../doc/make-docs.el ../doc/prcs.texi
	(cd ../src; $(MAKE) docs.cc.in)
