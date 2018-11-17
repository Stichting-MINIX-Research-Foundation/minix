# Id

include $(top_srcdir)/Makefile.am.common

if !NO_AFS
afs_lib = $(LIB_kafs)
endif

AM_CPPFLAGS += -I$(srcdir)/../lib/krb5 \
	$(INCLUDE_libintl) \
	-DHEIMDAL_LOCALEDIR='"$(localedir)"'

man_MANS = \
	kinit.1 \
	klist.1 \
	kdestroy.1 \
	kswitch.1 \
	kdigest.8 \
	kgetcred.1 \
	kimpersonate.8

bin_PROGRAMS = kinit kdestroy kgetcred heimtools
libexec_PROGRAMS = kdigest kimpersonate

noinst_PROGRAMS = kverify kdecode_ticket generate-requests

kinit_LDADD = \
	$(afs_lib) \
	$(top_builddir)/lib/krb5/libkrb5.la \
	$(top_builddir)/lib/ntlm/libheimntlm.la \
	$(LIB_hcrypto) \
	$(top_builddir)/lib/asn1/libasn1.la \
	$(LIB_libintl) \
	$(LIB_roken)

kdestroy_LDADD	= $(kinit_LDADD)

kimpersonate_LDADD = $(kinit_LDADD)

heimtools_LDADD = \
	$(top_builddir)/lib/sl/libsl.la \
	$(kinit_LDADD) \
	$(LIB_readline)

dist_heimtools_SOURCES = heimtools.c klist.c kswitch.c copy_cred_cache.c
nodist_heimtools_SOURCES = heimtools-commands.c

$(heimtools_OBJECTS): heimtools-commands.h

dist_kdigest_SOURCES = kdigest.c
nodist_kdigest_SOURCES = kdigest-commands.c

kdigest_LDADD = \
	$(top_builddir)/lib/ntlm/libheimntlm.la \
	$(top_builddir)/lib/krb5/libkrb5.la \
	$(LIB_hcrypto) \
	$(top_builddir)/lib/asn1/libasn1.la \
	$(top_builddir)/lib/sl/libsl.la \
	$(LIB_roken)

$(kdigest_OBJECTS): kdigest-commands.h

CLEANFILES = \
	kdigest-commands.h kdigest-commands.c \
	heimtools-commands.h heimtools-commands.c

kdigest-commands.c kdigest-commands.h: kdigest-commands.in
	$(SLC) $(srcdir)/kdigest-commands.in

heimtools-commands.c heimtools-commands.h: heimtools-commands.in
	$(SLC) $(srcdir)/heimtools-commands.in

LDADD = \
	$(top_builddir)/lib/krb5/libkrb5.la \
	$(LIB_hcrypto) \
	$(top_builddir)/lib/asn1/libasn1.la \
	$(LIB_roken)

EXTRA_DIST = NTMakefile $(man_MANS) \
	heimtools-version.rc \
	kcpytkt.c \
	kdeltkt.c \
	kvno.c \
	kdestroy-version.rc \
	kdigest-version.rc \
	kgetcred-version.rc \
	kimpersonate-version.rc \
	kinit-version.rc \
	kuser_locl.h heimtools-commands.in kdigest-commands.in copy_cred_cache.1

# make sure install-exec-hook doesn't have any commands in Makefile.am.common
install-exec-hook:
	(cd $(DESTDIR)$(bindir) && rm -f klist && $(LN_S) heimtools klist)
	(cd $(DESTDIR)$(bindir) && rm -f kswitch && $(LN_S) heimtools kswitch)

