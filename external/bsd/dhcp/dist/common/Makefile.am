AM_CPPFLAGS = -I.. -DLOCALSTATEDIR='"@localstatedir@"'
AM_CFLAGS = $(LDAP_CFLAGS)

noinst_LIBRARIES = libdhcp.a
libdhcp_a_SOURCES = alloc.c bpf.c comapi.c conflex.c ctrace.c discover.c \
		    dispatch.c dlpi.c dns.c ethernet.c execute.c fddi.c \
		    icmp.c inet.c lpf.c memory.c nit.c ns_name.c options.c \
		    packet.c parse.c print.c raw.c resolv.c socket.c \
		    tables.c tr.c tree.c upf.c
man_MANS = dhcp-eval.5 dhcp-options.5
EXTRA_DIST = $(man_MANS)

# We want to build this directory first, before descending into tests subdir.
# The reason is that ideally the tests should link existing objects from this
# directory. That eliminates any discrepancies between tested code and
# production code. Sadly, we are not there yet.
SUBDIRS = . tests
