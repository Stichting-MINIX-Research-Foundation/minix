AM_CPPFLAGS = -DLOCALSTATEDIR='"@localstatedir@"'

sbin_PROGRAMS = dhcrelay
dhcrelay_SOURCES = dhcrelay.c
dhcrelay_LDADD = ../common/libdhcp.a ../omapip/libomapi.a \
		 ../bind/lib/libirs.a ../bind/lib/libdns.a \
		 ../bind/lib/libisccfg.a ../bind/lib/libisc.a
man_MANS = dhcrelay.8
EXTRA_DIST = $(man_MANS)

