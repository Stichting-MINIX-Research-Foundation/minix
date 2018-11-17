# Id

include $(top_srcdir)/Makefile.am.common

AM_CPPFLAGS += $(INCLUDE_libintl) $(INCLUDE_openssl_crypto) -I$(srcdir)/../lib/krb5

lib_LTLIBRARIES = libkdc.la

bin_PROGRAMS = string2key

sbin_PROGRAMS = kstash

libexec_PROGRAMS = hprop hpropd kdc digest-service

noinst_PROGRAMS = kdc-replay kdc-tester

man_MANS = kdc.8 kstash.8 hprop.8 hpropd.8 string2key.8

hprop_SOURCES = hprop.c mit_dump.c hprop.h
hpropd_SOURCES = hpropd.c hprop.h

kstash_SOURCES = kstash.c headers.h

string2key_SOURCES = string2key.c headers.h

digest_service_SOURCES = \
	digest-service.c

kdc_SOURCES = connect.c	\
	config.c	\
	announce.c	\
	main.c

kdc_tester_SOURCES = \
	config.c	\
	kdc-tester.c

libkdc_la_SOURCES = 		\
	default_config.c 	\
	set_dbinfo.c	 	\
	digest.c		\
	fast.c			\
	kdc_locl.h		\
	kerberos5.c		\
	krb5tgs.c		\
	pkinit.c		\
	pkinit-ec.c		\
	log.c			\
	misc.c			\
	kx509.c			\
	process.c		\
	windc.c			\
	rx.h

KDC_PROTOS = $(srcdir)/kdc-protos.h $(srcdir)/kdc-private.h

ALL_OBJECTS  = $(kdc_OBJECTS)
ALL_OBJECTS += $(kdc_replay_OBJECTS)
ALL_OBJECTS += $(kdc_tester_OBJECTS)
ALL_OBJECTS += $(libkdc_la_OBJECTS)
ALL_OBJECTS += $(string2key_OBJECTS)
ALL_OBJECTS += $(kstash_OBJECTS)
ALL_OBJECTS += $(hprop_OBJECTS)
ALL_OBJECTS += $(hpropd_OBJECTS)
ALL_OBJECTS += $(digest_service_OBJECTS)

$(ALL_OBJECTS): $(KDC_PROTOS)

libkdc_la_LDFLAGS = -version-info 2:0:0

if versionscript
libkdc_la_LDFLAGS += $(LDFLAGS_VERSION_SCRIPT)$(srcdir)/version-script.map
endif
$(libkdc_la_OBJECTS): $(srcdir)/version-script.map

$(srcdir)/kdc-protos.h: $(libkdc_la_SOURCES)
	cd $(srcdir) && perl ../cf/make-proto.pl -q -P comment -o kdc-protos.h $(libkdc_la_SOURCES) || rm -f kdc-protos.h

$(srcdir)/kdc-private.h: $(libkdc_la_SOURCES)
	cd $(srcdir) && perl ../cf/make-proto.pl -q -P comment -p kdc-private.h $(libkdc_la_SOURCES) || rm -f kdc-private.h


hprop_LDADD = \
	$(top_builddir)/lib/hdb/libhdb.la \
	$(top_builddir)/lib/krb5/libkrb5.la \
	$(LIB_kdb)  \
	$(LIB_hcrypto) \
	$(top_builddir)/lib/asn1/libasn1.la \
	$(LIB_roken) \
	$(DB3LIB) $(DB1LIB) $(LMDBLIB) $(NDBMLIB)

hpropd_LDADD = \
	$(top_builddir)/lib/hdb/libhdb.la \
	$(top_builddir)/lib/krb5/libkrb5.la \
	$(LIB_kdb)  \
	$(LIB_hcrypto) \
	$(top_builddir)/lib/asn1/libasn1.la \
	$(LIB_roken) \
	$(DB3LIB) $(DB1LIB) $(LMDBLIB) $(NDBMLIB)

if PKINIT
LIB_pkinit = $(top_builddir)/lib/hx509/libhx509.la
endif

libkdc_la_LIBADD = \
	$(LIB_pkinit) \
	$(top_builddir)/lib/hdb/libhdb.la \
	$(top_builddir)/lib/krb5/libkrb5.la \
	$(LIB_kdb)  \
	$(top_builddir)/lib/ntlm/libheimntlm.la \
	$(LIB_hcrypto) \
	$(LIB_openssl_crypto) \
	$(top_builddir)/lib/asn1/libasn1.la \
	$(LIB_roken) \
	$(DB3LIB) $(DB1LIB) $(LMDBLIB) $(NDBMLIB)

LDADD = $(top_builddir)/lib/hdb/libhdb.la \
	$(top_builddir)/lib/krb5/libkrb5.la \
	$(LIB_hcrypto) \
	$(top_builddir)/lib/asn1/libasn1.la \
	$(LIB_roken) \
	$(DB3LIB) $(DB1LIB) $(LMDBLIB) $(NDBMLIB)

kdc_LDADD = libkdc.la $(LDADD) $(LIB_pidfile) $(CAPNG_LIBS)

if FRAMEWORK_SECURITY
kdc_LDFLAGS = -framework SystemConfiguration -framework CoreFoundation
endif
kdc_CFLAGS = $(CAPNG_CFLAGS)

digest_service_LDADD = \
	libkdc.la \
	$(top_builddir)/lib/ntlm/libheimntlm.la \
	$(top_builddir)/lib/ipc/libheim-ipcs.la \
	$(LDADD) $(LIB_pidfile)
kdc_replay_LDADD = libkdc.la $(LDADD) $(LIB_pidfile)
kdc_tester_LDADD = libkdc.la $(LDADD) $(LIB_pidfile) $(LIB_heimbase)

include_HEADERS = kdc.h $(srcdir)/kdc-protos.h

noinst_HEADERS = $(srcdir)/kdc-private.h

krb5dir = $(includedir)/krb5
krb5_HEADERS = windc_plugin.h

build_HEADERZ = $(krb5_HEADERS) # XXX

EXTRA_DIST = \
	hprop-version.rc \
	hpropd-version.rc \
	kdc-version.rc \
	kstash-version.rc \
	libkdc-version.rc \
	string2key-version.rc \
	libkdc-exports.def \
	NTMakefile $(man_MANS) version-script.map
