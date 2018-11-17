# Id

include $(top_srcdir)/Makefile.am.common

bin_SCRIPTS = krb5-config

pkgconfigdir = $(libdir)/pkgconfig

pkgconfig_DATA = \
	heimdal-gssapi.pc \
	heimdal-krb5.pc \
	heimdal-kadm-client.pc \
	heimdal-kadm-server.pc \
	kafs.pc \
	kadm-client.pc \
	kadm-server.pc \
	krb5.pc \
	krb5-gssapi.pc

man_MANS = krb5-config.1

if PKINIT
LIB_pkinit = -lhx509
endif

subst = sed	-e "s!@PACKAGE\@!$(PACKAGE)!g" \
		-e "s!@VERSION\@!$(VERSION)!g" \
		-e "s!@CANONICAL_HOST\@!$(CANONICAL_HOST)!g" \
		-e "s!@prefix\@!$(prefix)!g" \
		-e "s!@exec_prefix\@!$(exec_prefix)!g" \
		-e "s!@libdir\@!$(libdir)!g" \
		-e "s!@includedir\@!$(includedir)!g" \
		-e "s!@PTHREAD_LIBADD\@!$(PTHREAD_LIBADD)!g" \
		-e "s!@LIB_crypt\@!$(LIB_crypt)!g" \
		-e "s!@LIB_dbopen\@!$(LIB_dbopen)!g" \
		-e "s!@LIB_hcrypto_appl\@!$(LIB_hcrypto_appl)!g" \
		-e "s!@LIB_dlopen\@!$(LIB_dlopen)!g" \
		-e "s!@LIB_door_create\@!$(LIB_door_create)!g" \
		-e "s!@LIB_pkinit\@!$(LIB_pkinit)!g" \
		-e "s!@LIBS\@!$(LIBS)!g"

krb5-config: krb5-config.in
	$(subst) $(srcdir)/krb5-config.in > $@.new
	mv $@.new $@
	chmod +x $@

.pc.in.pc:
	$(subst) $< > $@.new
	mv $@.new $@

EXTRA_DIST = \
	NTMakefile \
	cov.sh \
	$(man_MANS) \
	krb5-config.in \
	heimdal-gssapi.pc.in \
	kdc-log-analyze.pl \
	$(pkgconfig_DATA:.pc=.pc.in)

CLEANFILES = \
	krb5-config \
	krb5-config.new \
	$(pkgconfig_DATA) \
	$(pkgconfig_DATA:.pc=.pc.new)

