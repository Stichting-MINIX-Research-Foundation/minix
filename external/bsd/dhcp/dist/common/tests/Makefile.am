SUBDIRS = .

AM_CPPFLAGS = $(ATF_CFLAGS) -I$(top_srcdir)/includes

EXTRA_DIST = Atffile

ATF_TESTS =

if HAVE_ATF

ATF_TESTS += alloc_unittest dns_unittest

alloc_unittest_SOURCES = test_alloc.c $(top_srcdir)/tests/t_api_dhcp.c
alloc_unittest_LDADD = $(ATF_LDFLAGS)
alloc_unittest_LDADD += ../libdhcp.a  \
	../../omapip/libomapi.a ../../bind/lib/libirs.a \
	../../bind/lib/libdns.a ../../bind/lib/libisccfg.a  ../../bind/lib/libisc.a

dns_unittest_SOURCES = dns_unittest.c $(top_srcdir)/tests/t_api_dhcp.c
dns_unittest_LDADD = $(ATF_LDFLAGS)
dns_unittest_LDADD += ../libdhcp.a  \
	../../omapip/libomapi.a ../../bind/lib/libirs.a \
	../../bind/lib/libdns.a ../../bind/lib/libisccfg.a  ../../bind/lib/libisc.a

check: $(ATF_TESTS)
	atf-run | atf-report

endif

check_PROGRAMS = $(ATF_TESTS)
