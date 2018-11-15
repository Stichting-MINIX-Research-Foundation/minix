# Id

include $(top_srcdir)/Makefile.am.common

AM_CPPFLAGS += $(INCLUDE_libintl) -I$(srcdir)/../lib/krb5

libexec_PROGRAMS = kcm

kcm_SOURCES =		\
	acl.c		\
	acquire.c	\
	cache.c		\
	client.c	\
	config.c	\
	connect.c	\
	events.c	\
	glue.c		\
	headers.h	\
	kcm_locl.h	\
	log.c		\
	main.c		\
	protocol.c	\
	sessions.c	\
	renew.c

noinst_HEADERS = $(srcdir)/kcm-protos.h

$(srcdir)/kcm-protos.h: $(kcm_SOURCES)
	cd $(srcdir); perl ../cf/make-proto.pl -o kcm-protos.h -q -P comment $(kcm_SOURCES) || rm -f kcm-protos.h

$(kcm_OBJECTS): $(srcdir)/kcm-protos.h

man_MANS = kcm.8

LDADD = $(top_builddir)/lib/hdb/libhdb.la \
	$(top_builddir)/lib/krb5/libkrb5.la \
	$(LIB_hcrypto) \
	$(top_builddir)/lib/asn1/libasn1.la \
	$(top_builddir)/lib/ntlm/libheimntlm.la \
	$(top_builddir)/lib/ipc/libheim-ipcs.la \
	$(LIB_roken) \
	$(LIB_door_create) \
	$(LIB_pidfile)

EXTRA_DIST = NTMakefile $(man_MANS)
