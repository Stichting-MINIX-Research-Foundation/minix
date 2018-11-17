# Id

include $(top_srcdir)/Makefile.am.common

SUBDIRS = bin db gss ldap can java kdc

if ENABLE_SHARED
if HAVE_DLOPEN
SUBDIRS += plugin
endif
endif

EXTRA_DIST = NTMakefile
