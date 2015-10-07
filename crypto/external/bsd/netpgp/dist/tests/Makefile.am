## $NetBSD: Makefile.am,v 1.3 2009/05/28 00:27:03 lukem Exp $

TESTSUITE	= testsuite
AUTOTEST	= $(AUTOM4TE) --language=autotest

EXTRA_DIST	= \
	package.m4 \
	testsuite

TESTSUITE_DEPS	= \
	init.at \
	cat.at \
	crypt.at \
	detached.at \
	verify.at \
	package.m4 \
	testsuite.at

DISTCLEANFILES	= \
	atconfig \
	package.m4 \
	testsuite

package.m4: $(top_srcdir)/configure.ac
	( \
	echo '# Signature of the current package.'; \
	echo 'm4_define([AT_PACKAGE_NAME],	[@PACKAGE_NAME@])'; \
	echo 'm4_define([AT_PACKAGE_TARNAME],	[@PACKAGE_TARNAME@])'; \
	echo 'm4_define([AT_PACKAGE_VERSION],	[@PACKAGE_VERSION@])'; \
	echo 'm4_define([AT_PACKAGE_STRING],	[@PACKAGE_STRING@])'; \
	echo 'm4_define([AT_PACKAGE_BUGREPORT],	[@PACKAGE_BUGREPORT@])'; \
	) > $@

clean-local:
	test ! -f $(TESTSUITE) || $(SHELL) $(TESTSUITE) --clean

check-local: atconfig atlocal $(TESTSUITE)
	$(SHELL) '$(TESTSUITE)'

$(TESTSUITE): $(TESTSUITE_DEPS)
	@echo "Generating $@"
	$(AUTOTEST) -I '$(srcdir)' -o $@.tmp $@.at
	mv $@.tmp $@
