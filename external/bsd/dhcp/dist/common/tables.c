/*	$NetBSD: tables.c,v 1.1.1.3 2014/07/12 11:57:47 spz Exp $	*/
/* tables.c

   Tables of information... */

/*
 * Copyright (c) 2011-2014 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 2004-2009 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1995-2003 by Internet Software Consortium
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 *   Internet Systems Consortium, Inc.
 *   950 Charter Street
 *   Redwood City, CA 94063
 *   <info@isc.org>
 *   https://www.isc.org/
 *
 */

#include <sys/cdefs.h>
__RCSID("$NetBSD: tables.c,v 1.1.1.3 2014/07/12 11:57:47 spz Exp $");

#include "dhcpd.h"

/* XXXDPN: Moved here from hash.c, when it moved to libomapi.  Not sure
   where these really belong. */
HASH_FUNCTIONS (group, const char *, struct group_object, group_hash_t,
		group_reference, group_dereference, do_string_hash)
HASH_FUNCTIONS (universe, const char *, struct universe, universe_hash_t, 0, 0,
		do_case_hash)
HASH_FUNCTIONS (option_name, const char *, struct option, option_name_hash_t,
		option_reference, option_dereference, do_case_hash)
HASH_FUNCTIONS (option_code, const unsigned *, struct option,
		option_code_hash_t, option_reference, option_dereference,
		do_number_hash)

/* DHCP Option names, formats and codes, from RFC1533.

   Format codes:

   I - IPv4 address
   6 - IPv6 address
   l - 32-bit signed integer
   L - 32-bit unsigned integer
   s - 16-bit signed integer
   S - 16-bit unsigned integer
   b - 8-bit signed integer
   B - 8-bit unsigned integer
   t - ASCII text
   T - Lease Time, 32-bit unsigned integer implying a number of seconds from
       some event.  The special all-ones value means 'infinite'.  May either
       be printed as a decimal, eg, "3600", or as this name, eg, "infinite".
   f - flag (true or false)
   A - array of all that precedes (e.g., fIA means array of records of
       a flag and an IP address)
   a - array of the preceding character (e.g., fIa means a single flag
       followed by an array of IP addresses)
   U - name of an option space (universe)
   F - implicit flag - the presence of the option indicates that the
       flag is true.
   o - the preceding value is optional.
   E - encapsulation, string or colon-separated hex list (the latter
       two for parsing).   E is followed by a text string containing
       the name of the option space to encapsulate, followed by a '.'.
       If the E is immediately followed by '.', the applicable vendor
       option space is used if one is defined.
   e - If an encapsulation directive is not the first thing in the string,
       the option scanner requires an efficient way to find the encapsulation.
       This is done by placing a 'e' at the beginning of the option.   The
       'e' has no other purpose, and is not required if 'E' is the first
       thing in the option.
   X - either an ASCII string or binary data.   On output, the string is
       scanned to see if it's printable ASCII and, if so, output as a
       quoted string.   If not, it's output as colon-separated hex.   On
       input, the option can be specified either as a quoted string or as
       a colon-separated hex list.
   N - enumeration.   N is followed by a text string containing
       the name of the set of enumeration values to parse or emit,
       followed by a '.'.   The width of the data is specified in the
       named enumeration.   Named enumerations are tracked in parse.c.
   d - Domain name (i.e., FOO or FOO.BAR).
   D - Domain list (i.e., example.com eng.example.com)
   c - When following a 'D' atom, enables compression pointers.
   Z - Zero-length option
*/

struct universe dhcp_universe;
static struct option dhcp_options[] = {
	{ "subnet-mask", "I",			&dhcp_universe,   1, 1 },
	{ "time-offset", "l",			&dhcp_universe,   2, 1 },
	{ "routers", "IA",			&dhcp_universe,   3, 1 },
	{ "time-servers", "IA",			&dhcp_universe,   4, 1 },
	{ "ien116-name-servers", "IA",		&dhcp_universe,   5, 1 },
	{ "domain-name-servers", "IA",		&dhcp_universe,   6, 1 },
	{ "log-servers", "IA",			&dhcp_universe,   7, 1 },
	{ "cookie-servers", "IA",		&dhcp_universe,   8, 1 },
	{ "lpr-servers", "IA",			&dhcp_universe,   9, 1 },
	{ "impress-servers", "IA",		&dhcp_universe,  10, 1 },
	{ "resource-location-servers", "IA",	&dhcp_universe,  11, 1 },
	{ "host-name", "t",			&dhcp_universe,  12, 1 },
	{ "boot-size", "S",			&dhcp_universe,  13, 1 },
	{ "merit-dump", "t",			&dhcp_universe,  14, 1 },
	{ "domain-name", "t",			&dhcp_universe,  15, 1 },
	{ "swap-server", "I",			&dhcp_universe,  16, 1 },
	{ "root-path", "t",			&dhcp_universe,  17, 1 },
	{ "extensions-path", "t",		&dhcp_universe,  18, 1 },
	{ "ip-forwarding", "f",			&dhcp_universe,  19, 1 },
	{ "non-local-source-routing", "f",	&dhcp_universe,  20, 1 },
	{ "policy-filter", "IIA",		&dhcp_universe,  21, 1 },
	{ "max-dgram-reassembly", "S",		&dhcp_universe,  22, 1 },
	{ "default-ip-ttl", "B",		&dhcp_universe,  23, 1 },
	{ "path-mtu-aging-timeout", "L",	&dhcp_universe,  24, 1 },
	{ "path-mtu-plateau-table", "SA",	&dhcp_universe,  25, 1 },
	{ "interface-mtu", "S",			&dhcp_universe,  26, 1 },
	{ "all-subnets-local", "f",		&dhcp_universe,  27, 1 },
	{ "broadcast-address", "I",		&dhcp_universe,  28, 1 },
	{ "perform-mask-discovery", "f",	&dhcp_universe,  29, 1 },
	{ "mask-supplier", "f",			&dhcp_universe,  30, 1 },
	{ "router-discovery", "f",		&dhcp_universe,  31, 1 },
	{ "router-solicitation-address", "I",	&dhcp_universe,  32, 1 },
	{ "static-routes", "IIA",		&dhcp_universe,  33, 1 },
	{ "trailer-encapsulation", "f",		&dhcp_universe,  34, 1 },
	{ "arp-cache-timeout", "L",		&dhcp_universe,  35, 1 },
	{ "ieee802-3-encapsulation", "f",	&dhcp_universe,  36, 1 },
	{ "default-tcp-ttl", "B",		&dhcp_universe,  37, 1 },
	{ "tcp-keepalive-interval", "L",	&dhcp_universe,  38, 1 },
	{ "tcp-keepalive-garbage", "f",		&dhcp_universe,  39, 1 },
	{ "nis-domain", "t",			&dhcp_universe,  40, 1 },
	{ "nis-servers", "IA",			&dhcp_universe,  41, 1 },
	{ "ntp-servers", "IA",			&dhcp_universe,  42, 1 },
	{ "vendor-encapsulated-options", "E.",	&dhcp_universe,  43, 1 },
	{ "netbios-name-servers", "IA",		&dhcp_universe,  44, 1 },
	{ "netbios-dd-server", "IA",		&dhcp_universe,  45, 1 },
	{ "netbios-node-type", "B",		&dhcp_universe,  46, 1 },
	{ "netbios-scope", "t",			&dhcp_universe,  47, 1 },
	{ "font-servers", "IA",			&dhcp_universe,  48, 1 },
	{ "x-display-manager", "IA",		&dhcp_universe,  49, 1 },
	{ "dhcp-requested-address", "I",	&dhcp_universe,  50, 1 },
	{ "dhcp-lease-time", "L",		&dhcp_universe,  51, 1 },
	{ "dhcp-option-overload", "B",		&dhcp_universe,  52, 1 },
	{ "dhcp-message-type", "B",		&dhcp_universe,  53, 1 },
	{ "dhcp-server-identifier", "I",	&dhcp_universe,  54, 1 },
	{ "dhcp-parameter-request-list", "BA",	&dhcp_universe,  55, 1 },
	{ "dhcp-message", "t",			&dhcp_universe,  56, 1 },
	{ "dhcp-max-message-size", "S",		&dhcp_universe,  57, 1 },
	{ "dhcp-renewal-time", "L",		&dhcp_universe,  58, 1 },
	{ "dhcp-rebinding-time", "L",		&dhcp_universe,  59, 1 },
	{ "vendor-class-identifier", "X",	&dhcp_universe,  60, 1 },
	{ "dhcp-client-identifier", "X",	&dhcp_universe,  61, 1 },
	{ "nwip-domain", "t",			&dhcp_universe,  62, 1 },
	{ "nwip-suboptions", "Enwip.",		&dhcp_universe,  63, 1 },
	{ "nisplus-domain", "t",		&dhcp_universe,  64, 1 },
	{ "nisplus-servers", "IA",		&dhcp_universe,  65, 1 },
	{ "tftp-server-name", "t",		&dhcp_universe,  66, 1 },
	{ "bootfile-name", "t",			&dhcp_universe,  67, 1 },
	{ "mobile-ip-home-agent", "IA",		&dhcp_universe,  68, 1 },
	{ "smtp-server", "IA",			&dhcp_universe,  69, 1 },
	{ "pop-server", "IA",			&dhcp_universe,  70, 1 },
	{ "nntp-server", "IA",			&dhcp_universe,  71, 1 },
	{ "www-server", "IA",			&dhcp_universe,  72, 1 },
	{ "finger-server", "IA",		&dhcp_universe,  73, 1 },
	{ "irc-server", "IA",			&dhcp_universe,  74, 1 },
	{ "streettalk-server", "IA",		&dhcp_universe,  75, 1 },
	{ "streettalk-directory-assistance-server", "IA",
						&dhcp_universe,  76, 1 },
	{ "user-class", "t",			&dhcp_universe,  77, 1 },
	{ "slp-directory-agent", "fIa",		&dhcp_universe,  78, 1 },
	{ "slp-service-scope", "fto",		&dhcp_universe,  79, 1 },
	/* 80 is the zero-length rapid-commit (RFC 4039) */
	{ "fqdn", "Efqdn.",			&dhcp_universe,  81, 1 },
	{ "relay-agent-information", "Eagent.",	&dhcp_universe,  82, 1 },
	/* 83 is iSNS (RFC 4174) */
	/* 84 is unassigned */
	{ "nds-servers", "IA",			&dhcp_universe,  85, 1 },
	{ "nds-tree-name", "t",			&dhcp_universe,  86, 1 },
	{ "nds-context", "t",			&dhcp_universe,  87, 1 },

	/* Note: RFC4280 fails to identify if the DHCPv4 option is to use
	 * compression pointers or not.  Assume not.
	 */
	{ "bcms-controller-names", "D",		&dhcp_universe,  88, 1 },
	{ "bcms-controller-address", "Ia",	&dhcp_universe,  89, 1 },

	/* 90 is the authentication option (RFC 3118) */

	{ "client-last-transaction-time", "L",  &dhcp_universe,  91, 1 },
	{ "associated-ip", "Ia",                &dhcp_universe,  92, 1 },
#if 0
	/* Defined by RFC 4578 */
	{ "pxe-system-type", "S",		&dhcp_universe,  93, 1 },
	{ "pxe-interface-id", "BBB",		&dhcp_universe,  94, 1 },
	{ "pxe-client-id", "BX",		&dhcp_universe,  97, 1 },
#endif
	{ "uap-servers", "t",			&dhcp_universe,  98, 1 },
#if defined(RFC4776_OPTIONS)
        { "geoconf-civic", "X",                 &dhcp_universe, 99, 1 },
#endif
#if defined(RFC4833_OPTIONS)
	{ "pcode", "t",				&dhcp_universe, 100, 1 },
	{ "tcode", "t",				&dhcp_universe, 101, 1 },
#endif
	{ "netinfo-server-address", "Ia",	&dhcp_universe, 112, 1 },
	{ "netinfo-server-tag", "t",		&dhcp_universe, 113, 1 },
	{ "default-url", "t",			&dhcp_universe, 114, 1 },
#if defined(RFC2937_OPTIONS)
	{ "name-service-search", "Sa",		&dhcp_universe, 117, 1 },
#endif
	{ "subnet-selection", "I",		&dhcp_universe, 118, 1 },
	{ "domain-search", "Dc",		&dhcp_universe, 119, 1 },
	{ "vivco", "Evendor-class.",		&dhcp_universe, 124, 1 },
	{ "vivso", "Evendor.",			&dhcp_universe, 125, 1 },
#if 0
	/* Referenced by RFC 4578.
	 * DO NOT UNCOMMENT THESE DEFINITIONS: these names are placeholders
	 * and will not be used in future versions of the software.
	 */
	{ "pxe-undefined-1", "X",		&dhcp_universe, 128, 1 },
	{ "pxe-undefined-2", "X",		&dhcp_universe, 129, 1 },
	{ "pxe-undefined-3", "X",		&dhcp_universe, 130, 1 },
	{ "pxe-undefined-4", "X",		&dhcp_universe, 131, 1 },
	{ "pxe-undefined-5", "X",		&dhcp_universe, 132, 1 },
	{ "pxe-undefined-6", "X",		&dhcp_universe, 133, 1 },
	{ "pxe-undefined-7", "X",		&dhcp_universe, 134, 1 },
	{ "pxe-undefined-8", "X",		&dhcp_universe, 135, 1 },
#endif
#if defined(RFC5192_OPTIONS)
	{"pana-agent", "Ia",			&dhcp_universe, 136, 1 },
#endif
#if defined(RFC5223_OPTIONS)
	{"v4-lost", "d",			&dhcp_universe, 137, 1 },
#endif
#if defined(RFC5417_OPTIONS)
	{"capwap-ac-v4", "Ia",			&dhcp_universe, 138, 1 },
#endif
#if defined(RFC6731_OPTIONS)
        { "rdnss-selection", "BIID",		&dhcp_universe, 146, 1 },
#endif
#if 0
	/* Not defined by RFC yet */
	{ "tftp-server-address", "Ia",		&dhcp_universe, 150, 1 },
#endif
#if 0
	/* PXELINUX options: defined by RFC 5071 */
	{ "pxelinux-magic", "BBBB",		&dhcp_universe, 208, 1 },
	{ "loader-configfile", "t",		&dhcp_universe, 209, 1 },
	{ "loader-pathprefix", "t",		&dhcp_universe, 210, 1 },
	{ "loader-reboottime", "L",		&dhcp_universe, 211, 1 },
#endif
#if defined(RFC5969_OPTIONS)
        { "option-6rd", "BB6Ia",		&dhcp_universe, 212, 1 },
#endif
#if defined(RFC5986_OPTIONS)
	{"v4-access-domain", "d",		&dhcp_universe, 213, 1 },
#endif
	{ NULL, NULL, NULL, 0, 0 }
};

struct universe nwip_universe;
static struct option nwip_options[] = {
	{ "illegal-1", "",			&nwip_universe,   1, 1 },
	{ "illegal-2", "",			&nwip_universe,   2, 1 },
	{ "illegal-3", "",			&nwip_universe,   3, 1 },
	{ "illegal-4", "",			&nwip_universe,   4, 1 },
	{ "nsq-broadcast", "f",			&nwip_universe,   5, 1 },
	{ "preferred-dss", "IA",		&nwip_universe,   6, 1 },
	{ "nearest-nwip-server", "IA",		&nwip_universe,   7, 1 },
	{ "autoretries", "B",			&nwip_universe,   8, 1 },
	{ "autoretry-secs", "B",		&nwip_universe,   9, 1 },
	{ "nwip-1-1", "f",			&nwip_universe,  10, 1 },
	{ "primary-dss", "I",			&nwip_universe,  11, 1 },
	{ NULL, NULL, NULL, 0, 0 }
};

/* Note that the "FQDN suboption space" does not reflect the FQDN option
 * format - rather, this is a handy "virtualization" of a flat option
 * which makes manual configuration and presentation of some of its
 * contents easier (each of these suboptions is a fixed-space field within
 * the fqdn contents - domain and host names are derived from a common field,
 * and differ in the left and right hand side of the leftmost dot, fqdn is
 * the combination of the two).
 *
 * Note further that the DHCPv6 and DHCPv4 'fqdn' options use the same
 * virtualized option space to store their work.
 */

struct universe fqdn_universe;
struct universe fqdn6_universe;
static struct option fqdn_options[] = {
	{ "no-client-update", "f",		&fqdn_universe,   1, 1 },
	{ "server-update", "f",			&fqdn_universe,   2, 1 },
	{ "encoded", "f",			&fqdn_universe,   3, 1 },
	{ "rcode1", "B",			&fqdn_universe,   4, 1 },
	{ "rcode2", "B",			&fqdn_universe,   5, 1 },
	{ "hostname", "t",			&fqdn_universe,   6, 1 },
	{ "domainname", "t",			&fqdn_universe,   7, 1 },
	{ "fqdn", "t",				&fqdn_universe,   8, 1 },
	{ NULL, NULL, NULL, 0, 0 }
};

struct universe vendor_class_universe;
static struct option vendor_class_options[] =  {
	{ "isc", "X",			&vendor_class_universe,      2495, 1 },
	{ NULL, NULL, NULL, 0, 0 }
};

struct universe vendor_universe;
static struct option vendor_options[] = {
	{ "isc", "Eisc.",		&vendor_universe,            2495, 1 },
	{ NULL, NULL, NULL, 0, 0 }
};

struct universe isc_universe;
static struct option isc_options [] = {
	{ "media", "t",				&isc_universe,   1, 1 },
	{ "update-assist", "X",			&isc_universe,   2, 1 },
	{ NULL,	NULL, NULL, 0, 0 }
};

struct universe dhcpv6_universe;
static struct option dhcpv6_options[] = {

				/* RFC3315 OPTIONS */

	/* Client and server DUIDs are opaque fields, but marking them
	 * up somewhat makes configuration easier.
	 */
	{ "client-id", "X",			&dhcpv6_universe,  1, 1 },
	{ "server-id", "X",			&dhcpv6_universe,  2, 1 },

	/* ia-* options actually have at their ends a space for options
	 * that are specific to this instance of the option.  We can not
	 * handle this yet at this stage of development, so the encoding
	 * of these options is unspecified ("X").
	 */
	{ "ia-na", "X",				&dhcpv6_universe,  3, 1 },
	{ "ia-ta", "X",				&dhcpv6_universe,  4, 1 },
	{ "ia-addr", "X",			&dhcpv6_universe,  5, 1 },

	/* "oro" is DHCPv6 speak for "parameter-request-list" */
	{ "oro", "SA",				&dhcpv6_universe,  6, 1 },

	{ "preference", "B",			&dhcpv6_universe,  7, 1 },
	{ "elapsed-time", "S",			&dhcpv6_universe,  8, 1 },
	{ "relay-msg", "X",			&dhcpv6_universe,  9, 1 },

	/* Option code 10 is curiously unassigned. */
	/* 
	 * In draft-ietf-dhc-dhcpv6-25 there were two OPTION_CLIENT_MSG and
	 * OPTION_SERVER_MSG options. They were eventually unified as
	 * OPTION_RELAY_MSG, hence no option with value of 10. 
	 */
#if 0
	/* XXX: missing suitable atoms for the auth option.  We may want
	 * to 'virtually encapsulate' this option a la the fqdn option
	 * seeing as it is processed explicitly by the server and unlikely
	 * to be configured by hand by users as such.
	 */
	{ "auth", "Nauth-protocol.Nauth-algorithm.Nrdm-type.LLX",
						&dhcpv6_universe, 11, 1 },
#endif
	{ "unicast", "6",			&dhcpv6_universe, 12, 1 },
	{ "status-code", "Nstatus-codes.to",	&dhcpv6_universe, 13, 1 },
	{ "rapid-commit", "Z",			&dhcpv6_universe, 14, 1 },
#if 0
	/* XXX: user-class contents are of the form "StA" where the
	 * integer describes the length of the text field.  We don't have
	 * an atom for pre-determined-length octet strings yet, so we
	 * can't quite do these two.
	 */
	{ "user-class", "X",			&dhcpv6_universe, 15, 1 },
	{ "vendor-class", "X",			&dhcpv6_universe, 16, 1 },
#endif
	{ "vendor-opts", "Evsio.",		&dhcpv6_universe, 17, 1 },
	{ "interface-id", "X",			&dhcpv6_universe, 18, 1 },
	{ "reconf-msg", "Ndhcpv6-messages.",	&dhcpv6_universe, 19, 1 },
	{ "reconf-accept", "Z",			&dhcpv6_universe, 20, 1 },

				/* RFC3319 OPTIONS */

	/* Of course: we would HAVE to have a different atom for
	 * domain names without compression.  Typical.
	 */
	{ "sip-servers-names", "D",		&dhcpv6_universe, 21, 1 },
	{ "sip-servers-addresses", "6A",	&dhcpv6_universe, 22, 1 },

				/* RFC3646 OPTIONS */

	{ "name-servers", "6A",			&dhcpv6_universe, 23, 1 },
	{ "domain-search", "D",			&dhcpv6_universe, 24, 1 },

				/* RFC3633 OPTIONS */

	{ "ia-pd", "X",				&dhcpv6_universe, 25, 1 },
	{ "ia-prefix", "X",			&dhcpv6_universe, 26, 1 },

				/* RFC3898 OPTIONS */

	{ "nis-servers", "6A", 			&dhcpv6_universe, 27, 1 },
	{ "nisp-servers", "6A",			&dhcpv6_universe, 28, 1 },
	{ "nis-domain-name", "D",		&dhcpv6_universe, 29, 1 },
	{ "nisp-domain-name", "D",		&dhcpv6_universe, 30, 1 },

				/* RFC4075 OPTIONS */
	{ "sntp-servers", "6A",			&dhcpv6_universe, 31, 1 },

				/* RFC4242 OPTIONS */

	{ "info-refresh-time", "T",		&dhcpv6_universe, 32, 1 },

				/* RFC4280 OPTIONS */

	{ "bcms-server-d", "D",			&dhcpv6_universe, 33, 1 },
	{ "bcms-server-a", "6A",		&dhcpv6_universe, 34, 1 },

	/* Note that 35 is not assigned. */

#if defined(RFC4776_OPTIONS)
			/* RFC4776 OPTIONS */

	{ "geoconf-civic", "X",			&dhcpv6_universe, 36, 1 },
#endif

				/* RFC4649 OPTIONS */

	/* The remote-id option looks like the VSIO option, but for all
	 * intents and purposes we only need to treat the entire field
	 * like a globally unique identifier (and if we create such an
	 * option, ensure the first 4 bytes are our enterprise-id followed
	 * by a globally unique ID so long as you're within that enterprise
	 * id).  So we'll use "X" for now unless someone grumbles.
	 */
	{ "remote-id", "X",			&dhcpv6_universe, 37, 1 },

				/* RFC4580 OPTIONS */

	{ "subscriber-id", "X",			&dhcpv6_universe, 38, 1 },

				/* RFC4704 OPTIONS */

	/* The DHCPv6 FQDN option is...weird.
	 *
	 * We use the same "virtual" encapsulated space as DHCPv4's FQDN
	 * option, so it can all be configured in one place.  Since the
	 * options system does not support multiple inheritance, we use
	 * a 'shill' layer to perform the different protocol conversions,
	 * and to redirect any queries in the DHCPv4 FQDN's space.
	 */
	{ "fqdn", "Efqdn6-if-you-see-me-its-a-bug-bug-bug.",
						&dhcpv6_universe, 39, 1 },


			/* RFC5192 */
#if defined(RFC5192_OPTIONS)
	{ "pana-agent", "6A",			&dhcpv6_universe, 40, 1 },
#endif

			/* RFC4833 OPTIONS */
#if defined(RFC4833_OPTIONS)
	{ "new-posix-timezone", "t",		&dhcpv6_universe, 41, 1 },
	{ "new-tzdb-timezone", "t",		&dhcpv6_universe, 42, 1 },
#endif

			/* RFC4994 OPTIONS */
#if defined(RFC4994_OPTIONS)
	{ "ero", "SA",				&dhcpv6_universe, 43, 1 },
#endif

			/* RFC5007 OPTIONS */

	{ "lq-query", "X",			&dhcpv6_universe, 44, 1 },
	{ "client-data", "X",			&dhcpv6_universe, 45, 1 },
	{ "clt-time", "L",			&dhcpv6_universe, 46, 1 },
	{ "lq-relay-data", "6X",		&dhcpv6_universe, 47, 1 },
	{ "lq-client-link", "6A",		&dhcpv6_universe, 48, 1 },

			/* RFC5223 OPTIONS */
#if defined(RFC5223_OPTIONS)
	{ "v6-lost", "d",			&dhcpv6_universe, 51, 1 },
#endif

			/* RFC5417 OPTIONS */
#if defined(RFC5417_OPTIONS)
	{ "capwap-ac-v6", "6a",			&dhcpv6_universe, 52, 1 },
#endif

			/* RFC5460 OPTIONS */
#if defined(RFC5460_OPTIONS)
	{ "relay-id", "X",			&dhcpv6_universe, 53, 1 },
#endif

			/* RFC5986 OPTIONS */
#if defined(RFC5986_OPTIONS)
	{ "v6-access-domain", "d",		&dhcpv6_universe, 57, 1 },
#endif

			/* RFC6011 OPTIONS */
#if defined(RFC6011_OPTIONS)
	{ "sip-ua-cs-list", "D",		&dhcpv6_universe, 58, 1 },
#endif

			/* RFC5970 OPTIONS */
#if defined(RFC5970_OPTIONS)
	{ "bootfile-url", "t",			&dhcpv6_universe, 59, 1 },
	{ "bootfile-param", "X",		&dhcpv6_universe, 60, 1 },
	{ "client-arch-type", "SA",		&dhcpv6_universe, 61, 1 },
	{ "nii", "BBB",				&dhcpv6_universe, 62, 1 },
#endif

			/* RFC6334 OPTIONS */
#if defined(RFC6334_OPTIONS)
	{ "aftr-name", "d",			&dhcpv6_universe, 64, 1 },
#endif

			/* RFC6440 OPTIONS */
#if defined(RFC6440_OPTIONS)
	{ "erp-local-domain-name", "d",		&dhcpv6_universe, 65, 1 },
#endif

			/* RFC6731 OPTIONS */
#if defined(RFC6731_OPTIONS)
	{ "rdnss-selection", "6BD",		&dhcpv6_universe, 74, 1 },
#endif

			/* RFC6939 OPTIONS */
#if defined(RFC6939_OPTIONS)
	{ "client-linklayer-addr", "X",		&dhcpv6_universe, 79, 1 },
#endif

			/* RFC6977 OPTIONS */
#if defined(RFC6977_OPTIONS)
	{ "link-address", "6",			&dhcpv6_universe, 80, 1 },
#endif

			/* RFC7083 OPTIONS */
#if defined(RFC7083_OPTIONS)
	{ "solmax-rt", "L",			&dhcpv6_universe, 82, 1 },
	{ "inf-max-rt", "L",			&dhcpv6_universe, 83, 1 },
#endif

	{ NULL, NULL, NULL, 0, 0 }
};

struct enumeration_value dhcpv6_duid_type_values[] = {
	{ "duid-llt",	DUID_LLT }, /* Link-Local Plus Time */
	{ "duid-en",	DUID_EN },  /* DUID based upon enterprise-ID. */
	{ "duid-ll",	DUID_LL },  /* DUID from Link Local address only. */
	{ NULL, 0 }
};

struct enumeration dhcpv6_duid_types = {
	NULL,
	"duid-types", 2,
	dhcpv6_duid_type_values
};

struct enumeration_value dhcpv6_status_code_values[] = {
	{ "success",	  0 }, /* Success				*/
	{ "UnspecFail",	  1 }, /* Failure, for unspecified reasons.	*/
	{ "NoAddrsAvail", 2 }, /* Server has no addresses to assign.	*/
	{ "NoBinding",	  3 }, /* Client record (binding) unavailable.	*/
	{ "NotOnLink",	  4 }, /* Bad prefix for the link.		*/
	{ "UseMulticast", 5 }, /* Not just good advice.  It's the law.	*/
	{ "NoPrefixAvail", 6 }, /* Server has no prefixes to assign.	*/
	{ "UnknownQueryType", 7 }, /* Query-type unknown/unsupported.	*/
	{ "MalformedQuery", 8 }, /* Leasequery not valid.		*/
	{ "NotConfigured", 9 }, /* The target address is not in config.	*/
	{ "NotAllowed",  10 }, /* Server doesn't allow the leasequery.	*/
	{ NULL, 0 }
};

struct enumeration dhcpv6_status_codes = {
	NULL,
	"status-codes", 2,
	dhcpv6_status_code_values
};

struct enumeration_value lq6_query_type_values[] = {
	{ "query-by-address", 1 },
	{ "query-by-clientid", 2 },
	{ NULL, 0 }
};

struct enumeration lq6_query_types = {
	NULL,
	"query-types", 2,
	lq6_query_type_values
};

struct enumeration_value dhcpv6_message_values[] = {
	{ "SOLICIT", 1 },
	{ "ADVERTISE", 2 },
	{ "REQUEST", 3 },
	{ "CONFIRM", 4 },
	{ "RENEW", 5 },
	{ "REBIND", 6 },
	{ "REPLY", 7 },
	{ "RELEASE", 8 },
	{ "DECLINE", 9 },
	{ "RECONFIGURE", 10 },
	{ "INFORMATION-REQUEST", 11 },
	{ "RELAY-FORW", 12 },
	{ "RELAY-REPL", 13 },
	{ "LEASEQUERY", 14 },
	{ "LEASEQUERY-REPLY", 15 },
	{ NULL, 0 }
};

/* Some code refers to a different table. */
const char *dhcpv6_type_names[] = {
	NULL,
	"Solicit",
	"Advertise",
	"Request",
	"Confirm",
	"Renew",
	"Rebind",
	"Reply",
	"Release",
	"Decline",
	"Reconfigure",
	"Information-request",
	"Relay-forward",
	"Relay-reply",
	"Leasequery",
	"Leasequery-reply"
};
const int dhcpv6_type_name_max =
	(sizeof(dhcpv6_type_names) / sizeof(dhcpv6_type_names[0]));

struct enumeration dhcpv6_messages = {
	NULL,
	"dhcpv6-messages", 1,
	dhcpv6_message_values
};

struct universe vsio_universe;
static struct option vsio_options[] = {
	{ "isc", "Eisc6.",		&vsio_universe,		     2495, 1 },
	{ NULL, NULL, NULL, 0, 0 }
};

struct universe isc6_universe;
static struct option isc6_options[] = {
	{ "media", "t",				&isc6_universe,     1, 1 },
	{ "update-assist", "X",			&isc6_universe,	    2, 1 },
	{ NULL, NULL, NULL, 0, 0 }
};

const char *hardware_types [] = {
	"unknown-0",
	"ethernet",
	"unknown-2",
	"unknown-3",
	"unknown-4",
	"unknown-5",
	"token-ring",
	"unknown-7",
	"fddi",
	"unknown-9",
	"unknown-10",
	"unknown-11",
	"unknown-12",
	"unknown-13",
	"unknown-14",
	"unknown-15",
	"unknown-16",
	"unknown-17",
	"unknown-18",
	"unknown-19",
	"unknown-20",
	"unknown-21",
	"unknown-22",
	"unknown-23",
	"unknown-24",
	"unknown-25",
	"unknown-26",
	"unknown-27",
	"unknown-28",
	"unknown-29",
	"unknown-30",
	"unknown-31",
	"infiniband",
	"unknown-33",
	"unknown-34",
	"unknown-35",
	"unknown-36",
	"unknown-37",
	"unknown-38",
	"unknown-39",
	"unknown-40",
	"unknown-41",
	"unknown-42",
	"unknown-43",
	"unknown-44",
	"unknown-45",
	"unknown-46",
	"unknown-47",
	"unknown-48",
	"unknown-49",
	"unknown-50",
	"unknown-51",
	"unknown-52",
	"unknown-53",
	"unknown-54",
	"unknown-55",
	"unknown-56",
	"unknown-57",
	"unknown-58",
	"unknown-59",
	"unknown-60",
	"unknown-61",
	"unknown-62",
	"unknown-63",
	"unknown-64",
	"unknown-65",
	"unknown-66",
	"unknown-67",
	"unknown-68",
	"unknown-69",
	"unknown-70",
	"unknown-71",
	"unknown-72",
	"unknown-73",
	"unknown-74",
	"unknown-75",
	"unknown-76",
	"unknown-77",
	"unknown-78",
	"unknown-79",
	"unknown-80",
	"unknown-81",
	"unknown-82",
	"unknown-83",
	"unknown-84",
	"unknown-85",
	"unknown-86",
	"unknown-87",
	"unknown-88",
	"unknown-89",
	"unknown-90",
	"unknown-91",
	"unknown-92",
	"unknown-93",
	"unknown-94",
	"unknown-95",
	"unknown-96",
	"unknown-97",
	"unknown-98",
	"unknown-99",
	"unknown-100",
	"unknown-101",
	"unknown-102",
	"unknown-103",
	"unknown-104",
	"unknown-105",
	"unknown-106",
	"unknown-107",
	"unknown-108",
	"unknown-109",
	"unknown-110",
	"unknown-111",
	"unknown-112",
	"unknown-113",
	"unknown-114",
	"unknown-115",
	"unknown-116",
	"unknown-117",
	"unknown-118",
	"unknown-119",
	"unknown-120",
	"unknown-121",
	"unknown-122",
	"unknown-123",
	"unknown-124",
	"unknown-125",
	"unknown-126",
	"unknown-127",
	"unknown-128",
	"unknown-129",
	"unknown-130",
	"unknown-131",
	"unknown-132",
	"unknown-133",
	"unknown-134",
	"unknown-135",
	"unknown-136",
	"unknown-137",
	"unknown-138",
	"unknown-139",
	"unknown-140",
	"unknown-141",
	"unknown-142",
	"unknown-143",
	"unknown-144",
	"unknown-145",
	"unknown-146",
	"unknown-147",
	"unknown-148",
	"unknown-149",
	"unknown-150",
	"unknown-151",
	"unknown-152",
	"unknown-153",
	"unknown-154",
	"unknown-155",
	"unknown-156",
	"unknown-157",
	"unknown-158",
	"unknown-159",
	"unknown-160",
	"unknown-161",
	"unknown-162",
	"unknown-163",
	"unknown-164",
	"unknown-165",
	"unknown-166",
	"unknown-167",
	"unknown-168",
	"unknown-169",
	"unknown-170",
	"unknown-171",
	"unknown-172",
	"unknown-173",
	"unknown-174",
	"unknown-175",
	"unknown-176",
	"unknown-177",
	"unknown-178",
	"unknown-179",
	"unknown-180",
	"unknown-181",
	"unknown-182",
	"unknown-183",
	"unknown-184",
	"unknown-185",
	"unknown-186",
	"unknown-187",
	"unknown-188",
	"unknown-189",
	"unknown-190",
	"unknown-191",
	"unknown-192",
	"unknown-193",
	"unknown-194",
	"unknown-195",
	"unknown-196",
	"unknown-197",
	"unknown-198",
	"unknown-199",
	"unknown-200",
	"unknown-201",
	"unknown-202",
	"unknown-203",
	"unknown-204",
	"unknown-205",
	"unknown-206",
	"unknown-207",
	"unknown-208",
	"unknown-209",
	"unknown-210",
	"unknown-211",
	"unknown-212",
	"unknown-213",
	"unknown-214",
	"unknown-215",
	"unknown-216",
	"unknown-217",
	"unknown-218",
	"unknown-219",
	"unknown-220",
	"unknown-221",
	"unknown-222",
	"unknown-223",
	"unknown-224",
	"unknown-225",
	"unknown-226",
	"unknown-227",
	"unknown-228",
	"unknown-229",
	"unknown-230",
	"unknown-231",
	"unknown-232",
	"unknown-233",
	"unknown-234",
	"unknown-235",
	"unknown-236",
	"unknown-237",
	"unknown-238",
	"unknown-239",
	"unknown-240",
	"unknown-241",
	"unknown-242",
	"unknown-243",
	"unknown-244",
	"unknown-245",
	"unknown-246",
	"unknown-247",
	"unknown-248",
	"unknown-249",
	"unknown-250",
	"unknown-251",
	"unknown-252",
	"unknown-253",
	"unknown-254",
	"unknown-255" };

universe_hash_t *universe_hash;
struct universe **universes;
int universe_count, universe_max;

/* Universe containing names of configuration options, which, rather than
   writing "option universe-name.option-name ...;", can be set by writing
   "option-name ...;". */

struct universe *config_universe;

/* XXX: omapi must die...all the below keeps us from having to make the
 * option structures omapi typed objects, which is a bigger headache.
 */

char *default_option_format = (char *) "X";

/* Must match hash_reference/dereference types in omapip/hash.h. */
int
option_reference(struct option **dest, struct option *src,
	         const char * file, int line)
{
	if (!dest || !src)
	        return DHCP_R_INVALIDARG;

	if (*dest) {
#if defined(POINTER_DEBUG)
	        log_fatal("%s(%d): reference store into non-null pointer!",
	                  file, line);
#else
	        return DHCP_R_INVALIDARG;
#endif
	}

	*dest = src;
	src->refcnt++;
	rc_register(file, line, dest, src, src->refcnt, 0, RC_MISC);
	return(ISC_R_SUCCESS);
}

int
option_dereference(struct option **dest, const char *file, int line)
{
	if (!dest)
	        return DHCP_R_INVALIDARG;

	if (!*dest) {
#if defined (POINTER_DEBUG)
	        log_fatal("%s(%d): dereference of null pointer!", file, line);
#else
	        return DHCP_R_INVALIDARG;
#endif
	}

	if ((*dest)->refcnt <= 0) {
#if defined (POINTER_DEBUG)
	        log_fatal("%s(%d): dereference of <= 0 refcnt!", file, line);
#else
	        return DHCP_R_INVALIDARG;
#endif
	}

	(*dest)->refcnt--;

	rc_register(file, line, dest, (*dest), (*dest)->refcnt, 1, RC_MISC);

	if ((*dest)->refcnt == 0) {
		/* The option name may be packed in the same alloc as the
		 * option structure.
		 */
	        if ((char *) (*dest)->name != (char *) ((*dest) + 1))
	                dfree((char *) (*dest)->name, file, line);

		/* It's either a user-configured format (allocated), or the
		 * default static format.
		 */
		if (((*dest)->format != NULL) &&
		    ((*dest)->format != default_option_format)) {
			dfree((char *) (*dest)->format, file, line);
		}

	        dfree(*dest, file, line);
	}

	*dest = NULL;
	return ISC_R_SUCCESS;
}

void initialize_common_option_spaces()
{
	unsigned code;
	int i;

	/* The 'universes' table is dynamically grown to contain
	 * universe as they're configured - except during startup.
	 * Since we know how many we put down in .c files, we can
	 * allocate a more-than-right-sized buffer now, leaving some
	 * space for user-configured option spaces.
	 *
	 * 1: dhcp_universe (dhcpv4 options)
	 * 2: nwip_universe (dhcpv4 NWIP option)
	 * 3: fqdn_universe (dhcpv4 fqdn option - reusable for v6)
	 * 4: vendor_class_universe (VIVCO)
	 * 5: vendor_universe (VIVSO)
	 * 6: isc_universe (dhcpv4 isc config space)
	 * 7: dhcpv6_universe (dhcpv6 options)
	 * 8: vsio_universe (DHCPv6 Vendor-Identified space)
	 * 9: isc6_universe (ISC's Vendor universe in DHCPv6 VSIO)
	 * 10: fqdn6_universe (dhcpv6 fqdn option shill to v4)
	 * 11: agent_universe (dhcpv4 relay agent - see server/stables.c)
	 * 12: server_universe (server's config, see server/stables.c)
	 * 13: user-config
	 * 14: more user-config
	 * 15: more user-config
	 * 16: more user-config
	 */
	universe_max = 16;
	i = universe_max * sizeof(struct universe *);
	if (i <= 0)
		log_fatal("Ludicrous initial size option space table.");
	universes = dmalloc(i, MDL);
	if (universes == NULL)
		log_fatal("Can't allocate option space table.");
	memset(universes, 0, i);

	/* Set up the DHCP option universe... */
	dhcp_universe.name = "dhcp";
	dhcp_universe.concat_duplicates = 1;
	dhcp_universe.lookup_func = lookup_hashed_option;
	dhcp_universe.option_state_dereference =
		hashed_option_state_dereference;
	dhcp_universe.save_func = save_hashed_option;
	dhcp_universe.delete_func = delete_hashed_option;
	dhcp_universe.encapsulate = hashed_option_space_encapsulate;
	dhcp_universe.foreach = hashed_option_space_foreach;
	dhcp_universe.decode = parse_option_buffer;
	dhcp_universe.length_size = 1;
	dhcp_universe.tag_size = 1;
	dhcp_universe.get_tag = getUChar;
	dhcp_universe.store_tag = putUChar;
	dhcp_universe.get_length = getUChar;
	dhcp_universe.store_length = putUChar;
	dhcp_universe.site_code_min = 0;
	dhcp_universe.end = DHO_END;
	dhcp_universe.index = universe_count++;
	universes [dhcp_universe.index] = &dhcp_universe;
	if (!option_name_new_hash(&dhcp_universe.name_hash,
				  BYTE_NAME_HASH_SIZE, MDL) ||
	    !option_code_new_hash(&dhcp_universe.code_hash,
				  BYTE_CODE_HASH_SIZE, MDL))
		log_fatal ("Can't allocate dhcp option hash table.");
	for (i = 0 ; dhcp_options[i].name ; i++) {
		option_code_hash_add(dhcp_universe.code_hash,
				     &dhcp_options[i].code, 0,
				     &dhcp_options[i], MDL);
		option_name_hash_add(dhcp_universe.name_hash,
				     dhcp_options [i].name, 0,
				     &dhcp_options [i], MDL);
	}
#if defined(REPORT_HASH_PERFORMANCE)
	log_info("DHCP name hash: %s",
		 option_name_hash_report(dhcp_universe.name_hash));
	log_info("DHCP code hash: %s",
		 option_code_hash_report(dhcp_universe.code_hash));
#endif

	/* Set up the Novell option universe (for option 63)... */
	nwip_universe.name = "nwip";
	nwip_universe.concat_duplicates = 0; /* XXX: reference? */
	nwip_universe.lookup_func = lookup_linked_option;
	nwip_universe.option_state_dereference =
		linked_option_state_dereference;
	nwip_universe.save_func = save_linked_option;
	nwip_universe.delete_func = delete_linked_option;
	nwip_universe.encapsulate = nwip_option_space_encapsulate;
	nwip_universe.foreach = linked_option_space_foreach;
	nwip_universe.decode = parse_option_buffer;
	nwip_universe.length_size = 1;
	nwip_universe.tag_size = 1;
	nwip_universe.get_tag = getUChar;
	nwip_universe.store_tag = putUChar;
	nwip_universe.get_length = getUChar;
	nwip_universe.store_length = putUChar;
	nwip_universe.site_code_min = 0;
	nwip_universe.end = 0;
	code = DHO_NWIP_SUBOPTIONS;
	nwip_universe.enc_opt = NULL;
	if (!option_code_hash_lookup(&nwip_universe.enc_opt,
				     dhcp_universe.code_hash, &code, 0, MDL))
		log_fatal("Unable to find NWIP parent option (%s:%d).", MDL);
	nwip_universe.index = universe_count++;
	universes [nwip_universe.index] = &nwip_universe;
	if (!option_name_new_hash(&nwip_universe.name_hash,
				  NWIP_HASH_SIZE, MDL) ||
	    !option_code_new_hash(&nwip_universe.code_hash,
				  NWIP_HASH_SIZE, MDL))
		log_fatal ("Can't allocate nwip option hash table.");
	for (i = 0 ; nwip_options[i].name ; i++) {
		option_code_hash_add(nwip_universe.code_hash,
				     &nwip_options[i].code, 0,
				     &nwip_options[i], MDL);
		option_name_hash_add(nwip_universe.name_hash,
				     nwip_options[i].name, 0,
				     &nwip_options[i], MDL);
	}
#if defined(REPORT_HASH_PERFORMANCE)
	log_info("NWIP name hash: %s",
		 option_name_hash_report(nwip_universe.name_hash));
	log_info("NWIP code hash: %s",
		 option_code_hash_report(nwip_universe.code_hash));
#endif

	/* Set up the FQDN option universe... */
	fqdn_universe.name = "fqdn";
	fqdn_universe.concat_duplicates = 0;
	fqdn_universe.lookup_func = lookup_linked_option;
	fqdn_universe.option_state_dereference =
		linked_option_state_dereference;
	fqdn_universe.save_func = save_linked_option;
	fqdn_universe.delete_func = delete_linked_option;
	fqdn_universe.encapsulate = fqdn_option_space_encapsulate;
	fqdn_universe.foreach = linked_option_space_foreach;
	fqdn_universe.decode = fqdn_universe_decode;
	fqdn_universe.length_size = 1;
	fqdn_universe.tag_size = 1;
	fqdn_universe.get_tag = getUChar;
	fqdn_universe.store_tag = putUChar;
	fqdn_universe.get_length = getUChar;
	fqdn_universe.store_length = putUChar;
	fqdn_universe.site_code_min = 0;
	fqdn_universe.end = 0;
	fqdn_universe.index = universe_count++;
	code = DHO_FQDN;
	fqdn_universe.enc_opt = NULL;
	if (!option_code_hash_lookup(&fqdn_universe.enc_opt,
				     dhcp_universe.code_hash, &code, 0, MDL))
		log_fatal("Unable to find FQDN parent option (%s:%d).", MDL);
	universes [fqdn_universe.index] = &fqdn_universe;
	if (!option_name_new_hash(&fqdn_universe.name_hash,
				  FQDN_HASH_SIZE, MDL) ||
	    !option_code_new_hash(&fqdn_universe.code_hash,
				  FQDN_HASH_SIZE, MDL))
		log_fatal ("Can't allocate fqdn option hash table.");
	for (i = 0 ; fqdn_options[i].name ; i++) {
		option_code_hash_add(fqdn_universe.code_hash,
				     &fqdn_options[i].code, 0,
				     &fqdn_options[i], MDL);
		option_name_hash_add(fqdn_universe.name_hash,
				     fqdn_options[i].name, 0,
				     &fqdn_options[i], MDL);
	}
#if defined(REPORT_HASH_PERFORMANCE)
	log_info("FQDN name hash: %s",
		 option_name_hash_report(fqdn_universe.name_hash));
	log_info("FQDN code hash: %s",
		 option_code_hash_report(fqdn_universe.code_hash));
#endif

        /* Set up the Vendor Identified Vendor Class options (for option
	 * 125)...
	 */
        vendor_class_universe.name = "vendor-class";
	vendor_class_universe.concat_duplicates = 0; /* XXX: reference? */
        vendor_class_universe.lookup_func = lookup_hashed_option;
        vendor_class_universe.option_state_dereference =
                hashed_option_state_dereference;
        vendor_class_universe.save_func = save_hashed_option;
        vendor_class_universe.delete_func = delete_hashed_option;
        vendor_class_universe.encapsulate = hashed_option_space_encapsulate;
        vendor_class_universe.foreach = hashed_option_space_foreach;
        vendor_class_universe.decode = parse_option_buffer;
        vendor_class_universe.length_size = 1;
        vendor_class_universe.tag_size = 4;
	vendor_class_universe.get_tag = getULong;
        vendor_class_universe.store_tag = putULong;
	vendor_class_universe.get_length = getUChar;
        vendor_class_universe.store_length = putUChar;
	vendor_class_universe.site_code_min = 0;
	vendor_class_universe.end = 0;
	code = DHO_VIVCO_SUBOPTIONS;
	vendor_class_universe.enc_opt = NULL;
	if (!option_code_hash_lookup(&vendor_class_universe.enc_opt,
				     dhcp_universe.code_hash, &code, 0, MDL))
		log_fatal("Unable to find VIVCO parent option (%s:%d).", MDL);
        vendor_class_universe.index = universe_count++;
        universes[vendor_class_universe.index] = &vendor_class_universe;
        if (!option_name_new_hash(&vendor_class_universe.name_hash,
				  VIVCO_HASH_SIZE, MDL) ||
	    !option_code_new_hash(&vendor_class_universe.code_hash,
				  VIVCO_HASH_SIZE, MDL))
                log_fatal("Can't allocate Vendor Identified Vendor Class "
			  "option hash table.");
        for (i = 0 ; vendor_class_options[i].name ; i++) {
		option_code_hash_add(vendor_class_universe.code_hash,
				     &vendor_class_options[i].code, 0,
				     &vendor_class_options[i], MDL);
                option_name_hash_add(vendor_class_universe.name_hash,
                                     vendor_class_options[i].name, 0,
                                     &vendor_class_options[i], MDL);
        }
#if defined(REPORT_HASH_PERFORMANCE)
	log_info("VIVCO name hash: %s",
		 option_name_hash_report(vendor_class_universe.name_hash));
	log_info("VIVCO code hash: %s",
		 option_code_hash_report(vendor_class_universe.code_hash));
#endif

        /* Set up the Vendor Identified Vendor Sub-options (option 126)... */
        vendor_universe.name = "vendor";
	vendor_universe.concat_duplicates = 0; /* XXX: reference? */
        vendor_universe.lookup_func = lookup_hashed_option;
        vendor_universe.option_state_dereference =
                hashed_option_state_dereference;
        vendor_universe.save_func = save_hashed_option;
        vendor_universe.delete_func = delete_hashed_option;
        vendor_universe.encapsulate = hashed_option_space_encapsulate;
        vendor_universe.foreach = hashed_option_space_foreach;
        vendor_universe.decode = parse_option_buffer;
        vendor_universe.length_size = 1;
        vendor_universe.tag_size = 4;
	vendor_universe.get_tag = getULong;
        vendor_universe.store_tag = putULong;
	vendor_universe.get_length = getUChar;
        vendor_universe.store_length = putUChar;
	vendor_universe.site_code_min = 0;
	vendor_universe.end = 0;
	code = DHO_VIVSO_SUBOPTIONS;
	vendor_universe.enc_opt = NULL;
	if (!option_code_hash_lookup(&vendor_universe.enc_opt,
				     dhcp_universe.code_hash, &code, 0, MDL))
		log_fatal("Unable to find VIVSO parent option (%s:%d).", MDL);
        vendor_universe.index = universe_count++;
        universes[vendor_universe.index] = &vendor_universe;
        if (!option_name_new_hash(&vendor_universe.name_hash,
				  VIVSO_HASH_SIZE, MDL) ||
	    !option_code_new_hash(&vendor_universe.code_hash,
				  VIVSO_HASH_SIZE, MDL))
                log_fatal("Can't allocate Vendor Identified Vendor Sub-"
			  "options hash table.");
        for (i = 0 ; vendor_options[i].name ; i++) {
                option_code_hash_add(vendor_universe.code_hash,
				     &vendor_options[i].code, 0,
				     &vendor_options[i], MDL);
                option_name_hash_add(vendor_universe.name_hash,
				     vendor_options[i].name, 0,
				     &vendor_options[i], MDL);
        }
#if defined(REPORT_HASH_PERFORMANCE)
	log_info("VIVSO name hash: %s",
		 option_name_hash_report(vendor_universe.name_hash));
	log_info("VIVSO code hash: %s",
		 option_code_hash_report(vendor_universe.code_hash));
#endif

        /* Set up the ISC Vendor-option universe (for option 125.2495)... */
        isc_universe.name = "isc";
	isc_universe.concat_duplicates = 0; /* XXX: check VIVSO ref */
        isc_universe.lookup_func = lookup_linked_option;
        isc_universe.option_state_dereference =
                linked_option_state_dereference;
        isc_universe.save_func = save_linked_option;
        isc_universe.delete_func = delete_linked_option;
        isc_universe.encapsulate = linked_option_space_encapsulate;
        isc_universe.foreach = linked_option_space_foreach;
        isc_universe.decode = parse_option_buffer;
        isc_universe.length_size = 2;
        isc_universe.tag_size = 2;
	isc_universe.get_tag = getUShort;
        isc_universe.store_tag = putUShort;
	isc_universe.get_length = getUShort;
        isc_universe.store_length = putUShort;
	isc_universe.site_code_min = 0;
	isc_universe.end = 0;
	code = VENDOR_ISC_SUBOPTIONS;
	isc_universe.enc_opt = NULL;
	if (!option_code_hash_lookup(&isc_universe.enc_opt,
				     vendor_universe.code_hash, &code, 0, MDL))
		log_fatal("Unable to find ISC parent option (%s:%d).", MDL);
        isc_universe.index = universe_count++;
        universes[isc_universe.index] = &isc_universe;
        if (!option_name_new_hash(&isc_universe.name_hash,
				  VIV_ISC_HASH_SIZE, MDL) ||
	    !option_code_new_hash(&isc_universe.code_hash,
				  VIV_ISC_HASH_SIZE, MDL))
                log_fatal("Can't allocate ISC Vendor options hash table.");
        for (i = 0 ; isc_options[i].name ; i++) {
		option_code_hash_add(isc_universe.code_hash,
				     &isc_options[i].code, 0,
				     &isc_options[i], MDL);
                option_name_hash_add(isc_universe.name_hash,
                                     isc_options[i].name, 0,
                                     &isc_options[i], MDL);
        }
#if defined(REPORT_HASH_PERFORMANCE)
	log_info("ISC name hash: %s",
		 option_name_hash_report(isc_universe.name_hash));
	log_info("ISC code hash: %s",
		 option_code_hash_report(isc_universe.code_hash));
#endif

	/* Set up the DHCPv6 root universe. */
	dhcpv6_universe.name = "dhcp6";
	dhcpv6_universe.concat_duplicates = 0;
	dhcpv6_universe.lookup_func = lookup_hashed_option;
	dhcpv6_universe.option_state_dereference =
		hashed_option_state_dereference;
	dhcpv6_universe.save_func = save_hashed_option;
	dhcpv6_universe.delete_func = delete_hashed_option;
	dhcpv6_universe.encapsulate = hashed_option_space_encapsulate;
	dhcpv6_universe.foreach = hashed_option_space_foreach;
	dhcpv6_universe.decode = parse_option_buffer;
	dhcpv6_universe.length_size = 2;
	dhcpv6_universe.tag_size = 2;
	dhcpv6_universe.get_tag = getUShort;
	dhcpv6_universe.store_tag = putUShort;
	dhcpv6_universe.get_length = getUShort;
	dhcpv6_universe.store_length = putUShort;
	dhcpv6_universe.site_code_min = 0;
	/* DHCPv6 has no END option. */
	dhcpv6_universe.end = 0x00;
	dhcpv6_universe.index = universe_count++;
	universes[dhcpv6_universe.index] = &dhcpv6_universe;
	if (!option_name_new_hash(&dhcpv6_universe.name_hash,
				  WORD_NAME_HASH_SIZE, MDL) ||
	    !option_code_new_hash(&dhcpv6_universe.code_hash,
				  WORD_CODE_HASH_SIZE, MDL))
		log_fatal("Can't allocate dhcpv6 option hash tables.");
	for (i = 0 ; dhcpv6_options[i].name ; i++) {
		option_code_hash_add(dhcpv6_universe.code_hash,
				     &dhcpv6_options[i].code, 0,
				     &dhcpv6_options[i], MDL);
		option_name_hash_add(dhcpv6_universe.name_hash,
				     dhcpv6_options[i].name, 0,
				     &dhcpv6_options[i], MDL);
	}

	/* Add DHCPv6 protocol enumeration sets. */
	add_enumeration(&dhcpv6_duid_types);
	add_enumeration(&dhcpv6_status_codes);
	add_enumeration(&dhcpv6_messages);

	/* Set up DHCPv6 VSIO universe. */
	vsio_universe.name = "vsio";
	vsio_universe.concat_duplicates = 0;
	vsio_universe.lookup_func = lookup_hashed_option;
	vsio_universe.option_state_dereference =
		hashed_option_state_dereference;
	vsio_universe.save_func = save_hashed_option;
	vsio_universe.delete_func = delete_hashed_option;
	vsio_universe.encapsulate = hashed_option_space_encapsulate;
	vsio_universe.foreach = hashed_option_space_foreach;
	vsio_universe.decode = parse_option_buffer;
	vsio_universe.length_size = 0;
	vsio_universe.tag_size = 4;
	vsio_universe.get_tag = getULong;
	vsio_universe.store_tag = putULong;
	vsio_universe.get_length = NULL;
	vsio_universe.store_length = NULL;
	vsio_universe.site_code_min = 0;
	/* No END option. */
	vsio_universe.end = 0x00;
	code = D6O_VENDOR_OPTS;
	if (!option_code_hash_lookup(&vsio_universe.enc_opt,
				     dhcpv6_universe.code_hash, &code, 0, MDL))
		log_fatal("Unable to find VSIO parent option (%s:%d).", MDL);
	vsio_universe.index = universe_count++;
	universes[vsio_universe.index] = &vsio_universe;
	if (!option_name_new_hash(&vsio_universe.name_hash,
				  VSIO_HASH_SIZE, MDL) ||
	    !option_code_new_hash(&vsio_universe.code_hash,
				  VSIO_HASH_SIZE, MDL))
		log_fatal("Can't allocate Vendor Specific Information "
			  "Options space.");
	for (i = 0 ; vsio_options[i].name != NULL ; i++) {
		option_code_hash_add(vsio_universe.code_hash,
				     &vsio_options[i].code, 0,
				     &vsio_options[i], MDL);
		option_name_hash_add(vsio_universe.name_hash,
				     vsio_options[i].name, 0,
				     &vsio_options[i], MDL);
	}

	/* Add ISC VSIO sub-sub-option space. */
	isc6_universe.name = "isc6";
	isc6_universe.concat_duplicates = 0;
	isc6_universe.lookup_func = lookup_hashed_option;
	isc6_universe.option_state_dereference =
		hashed_option_state_dereference;
	isc6_universe.save_func = save_hashed_option;
	isc6_universe.delete_func = delete_hashed_option;
	isc6_universe.encapsulate = hashed_option_space_encapsulate;
	isc6_universe.foreach = hashed_option_space_foreach;
	isc6_universe.decode = parse_option_buffer;
	isc6_universe.length_size = 0;
	isc6_universe.tag_size = 4;
	isc6_universe.get_tag = getULong;
	isc6_universe.store_tag = putULong;
	isc6_universe.get_length = NULL;
	isc6_universe.store_length = NULL;
	isc6_universe.site_code_min = 0;
	/* No END option. */
	isc6_universe.end = 0x00;
	code = 2495;
	if (!option_code_hash_lookup(&isc6_universe.enc_opt,
				     vsio_universe.code_hash, &code, 0, MDL))
		log_fatal("Unable to find ISC parent option (%s:%d).", MDL);
	isc6_universe.index = universe_count++;
	universes[isc6_universe.index] = &isc6_universe;
	if (!option_name_new_hash(&isc6_universe.name_hash,
				  VIV_ISC_HASH_SIZE, MDL) ||
	    !option_code_new_hash(&isc6_universe.code_hash,
				  VIV_ISC_HASH_SIZE, MDL))
		log_fatal("Can't allocate Vendor Specific Information "
			  "Options space.");
	for (i = 0 ; isc6_options[i].name != NULL ; i++) {
		option_code_hash_add(isc6_universe.code_hash,
				     &isc6_options[i].code, 0,
				     &isc6_options[i], MDL);
		option_name_hash_add(isc6_universe.name_hash,
				     isc6_options[i].name, 0,
				     &isc6_options[i], MDL);
	}

	/* The fqdn6 option space is a protocol-wrapper shill for the
	 * old DHCPv4 space.
	 */
	fqdn6_universe.name = "fqdn6-if-you-see-me-its-a-bug-bug-bug";
	fqdn6_universe.lookup_func = lookup_fqdn6_option;
	fqdn6_universe.option_state_dereference = NULL; /* Covered by v4. */
	fqdn6_universe.save_func = save_fqdn6_option;
	fqdn6_universe.delete_func = delete_fqdn6_option;
	fqdn6_universe.encapsulate = fqdn6_option_space_encapsulate;
	fqdn6_universe.foreach = fqdn6_option_space_foreach;
	fqdn6_universe.decode = fqdn6_universe_decode;
	/* This is not a 'normal' encapsulated space, so these values are
	 * meaningless.
	 */
	fqdn6_universe.length_size = 0;
	fqdn6_universe.tag_size = 0;
	fqdn6_universe.get_tag = NULL;
	fqdn6_universe.store_tag = NULL;
	fqdn6_universe.get_length = NULL;
	fqdn6_universe.store_length = NULL;
	fqdn6_universe.site_code_min = 0;
	fqdn6_universe.end = 0;
	fqdn6_universe.index = universe_count++;
	code = D6O_CLIENT_FQDN;
	fqdn6_universe.enc_opt = NULL;
	if (!option_code_hash_lookup(&fqdn6_universe.enc_opt,
				     dhcpv6_universe.code_hash, &code, 0, MDL))
		log_fatal("Unable to find FQDN v6 parent option. (%s:%d).",
			  MDL);
	universes[fqdn6_universe.index] = &fqdn6_universe;
	/* The fqdn6 space shares the same option space as the v4 space.
	 * So there are no name or code hashes on the v6 side.
	 */
	fqdn6_universe.name_hash = NULL;
	fqdn6_universe.code_hash = NULL;


	/* Set up the hash of DHCPv4 universes. */
	universe_new_hash(&universe_hash, UNIVERSE_HASH_SIZE, MDL);
	universe_hash_add(universe_hash, dhcp_universe.name, 0,
			  &dhcp_universe, MDL);
	universe_hash_add(universe_hash, nwip_universe.name, 0,
			  &nwip_universe, MDL);
	universe_hash_add(universe_hash, fqdn_universe.name, 0,
			  &fqdn_universe, MDL);
	universe_hash_add(universe_hash, vendor_class_universe.name, 0,
			  &vendor_class_universe, MDL);
	universe_hash_add(universe_hash, vendor_universe.name, 0,
			  &vendor_universe, MDL);
	universe_hash_add(universe_hash, isc_universe.name, 0,
			  &isc_universe, MDL);

	/* Set up hashes for DHCPv6 universes. */
	universe_hash_add(universe_hash, dhcpv6_universe.name, 0,
			  &dhcpv6_universe, MDL);
	universe_hash_add(universe_hash, vsio_universe.name, 0,
			  &vsio_universe, MDL);
	universe_hash_add(universe_hash, isc6_universe.name, 0,
			  &isc6_universe, MDL);
/* This should not be necessary.  Listing here just for consistency.
 *	universe_hash_add(universe_hash, fqdn6_universe.name, 0,
 *			  &fqdn6_universe, MDL);
 */
}
