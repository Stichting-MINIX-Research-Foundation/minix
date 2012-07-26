lib_LIBRARIES = libomapi.a
noinst_PROGRAMS = svtest

libomapi_a_SOURCES = protocol.c buffer.c alloc.c result.c connection.c \
		     errwarn.c listener.c dispatch.c generic.c support.c \
		     handle.c message.c convert.c hash.c auth.c inet_addr.c \
		     array.c trace.c toisc.c iscprint.c isclib.c

man_MANS = omapi.3
EXTRA_DIST = $(man_MANS)

svtest_SOURCES = test.c
svtest_LDADD = libomapi.a ../bind/lib/libdns.a ../bind/lib/libisc.a

