## Process this file with automake to produce Makefile.in.

AUTOMAKE_OPTIONS = parallel-tests

check_DATA = init.sh

TESTS = config.test
DISTCLEANFILES = $(check_DATA)

EXTRA_DIST = $(TESTS) init.sh.in

TEST_LOG_COMPILER = $(SHELL)

init.sh: $(srcdir)/init.sh.in Makefile
	@rm -f $@-t $@
	sed -e 's/[@]VERSION[@]/$(VERSION)/g' $(srcdir)/init.sh.in > $@-t
	mv $@-t $@
