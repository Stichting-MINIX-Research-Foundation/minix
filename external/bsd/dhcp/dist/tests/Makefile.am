EXTRA_DIST = failover/dhcp-1.cf failover/dhcp-2.cf failover/new-failover \
	     DHCPv6/000-badmsgtype.pl \
	     DHCPv6/010-solicit-noclientid.pl \
	     DHCPv6/011-solicit-serverid.pl \
	     DHCPv6/020-advertise-mcast.pl \
	     DHCPv6/030-request-noclientid.pl \
	     DHCPv6/031-request-noserverid.pl \
	     DHCPv6/032-request-badduid.pl \
	     DHCPv6/110-information-request-ia_na.pl \
	     DHCPv6/111-information-request-ia_ta.pl \
	     DHCPv6/112-badduid.pl \
	     DHCPv6/210-solicit-nohost.pl \
	     DHCPv6/211-solicit-opt-in-na.pl \
	     DHCPv6/212-solicit-opt-in-na-norapidcommit.pl \
	     DHCPv6/280-release-nohost.pl \
	     DHCPv6/281-release-bad-address.pl \
	     DHCPv6/282-release-no-address.pl \
	     DHCPv6/283-release.pl \
	     DHCPv6/290-decline-nohost.pl \
	     DHCPv6/291-decline-bad-address.pl \
	     DHCPv6/292-decline-no-address.pl \
	     DHCPv6/293-decline.pl \
	     DHCPv6/README DHCPv6/dhcp_client.pm \
	     DHCPv6/stubcli-opt-in-na.pl DHCPv6/stubcli.pl \
	     DHCPv6/test-a.conf DHCPv6/test-b.conf \
	     HOWTO-unit-test \
	     unit_test_sample.c

AM_CPPFLAGS = -I..

check_LIBRARIES = libt_api.a
libt_api_a_SOURCES = t_api.c t_api_dhcp.c

