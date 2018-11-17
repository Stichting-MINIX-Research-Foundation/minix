# Id

include $(top_srcdir)/Makefile.am.common

man_MANS = kpasswd.1 kpasswdd.8

bin_PROGRAMS = kpasswd

kpasswd_SOURCES = kpasswd.c kpasswd_locl.h

libexec_PROGRAMS = kpasswdd

noinst_PROGRAMS = kpasswd-generator

kpasswdd_SOURCES = kpasswdd.c kpasswd_locl.h

kpasswdd_LDADD = \
	$(top_builddir)/lib/kadm5/libkadm5srv.la \
	$(top_builddir)/lib/hdb/libhdb.la \
	$(LDADD) \
	$(LIB_pidfile) \
	$(LIB_dlopen) \
	$(DB3LIB) $(DB1LIB) $(LMDBLIB) $(NDBMLIB)

LDADD = $(top_builddir)/lib/krb5/libkrb5.la \
	$(LIB_hcrypto) \
	$(top_builddir)/lib/asn1/libasn1.la \
	$(LIB_roken)

EXTRA_DIST = NTMakefile $(man_MANS)
