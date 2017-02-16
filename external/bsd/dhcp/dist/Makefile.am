#
# automake adds dependencies that we don't like, so we explicitly remove them
#
Makefile:

#
# We have a lot of files that we want shipped with the distribution.
#
EXTRA_DIST = RELNOTES LICENSE \
	     contrib/3.0b1-lease-convert contrib/dhclient-tz-exithook.sh \
	     contrib/dhcp.spec contrib/sethostname.sh contrib/solaris.init \
	     contrib/ms2isc/Registry.pm contrib/ms2isc/ms2isc.pl \
	     contrib/ms2isc/readme.txt contrib/ldap/dhcpd-conf-to-ldap \
	     contrib/ldap/dhcp.schema contrib/ldap/README.ldap \
             contrib/dhcp-lease-list.pl \
	     doc/IANA-arp-parameters doc/Makefile doc/References.html \
	     doc/References.txt doc/References.xml doc/api+protocol \
	     doc/ja_JP.eucJP/dhclient-script.8 doc/ja_JP.eucJP/dhclient.8 \
	     doc/ja_JP.eucJP/dhclient.conf.5 doc/ja_JP.eucJP/dhclient.leases.5 \
	     doc/ja_JP.eucJP/dhcp-eval.5 doc/ja_JP.eucJP/dhcp-options.5 \
	     doc/examples/dhclient-dhcpv6.conf doc/examples/dhcpd-dhcpv6.conf \
	     doc/devel/arch.dox doc/devel/atf.dox doc/devel/contrib.dox \
	     doc/devel/debug.dox doc/devel/isc-logo.jpg doc/devel/mainpage.dox \
	     doc/devel/omapi.dox doc/devel/qa.dox util/bindvar.sh \
	     bind/Makefile bind/bind.tar.gz bind/version.tmp \
	     common/tests/Atffile server/tests/Atffile

SUBDIRS = bind includes tests common dst omapip client dhcpctl relay server

nobase_include_HEADERS = dhcpctl/dhcpctl.h

