/*	$NetBSD: site.h,v 1.1.1.3 2014/07/12 11:57:56 spz Exp $	*/
/* Site-specific definitions.

   For supported systems, you shouldn't need to make any changes here.
   However, you may want to, in order to deal with site-specific
   differences. */

/* Add any site-specific definitions and inclusions here... */

/* #include <site-foo-bar.h> */
/* #define SITE_FOOBAR */

/* Define this if you don't want dhcpd to run as a daemon and do want
   to see all its output printed to stdout instead of being logged via
   syslog().   This also makes dhcpd use the dhcpd.conf in its working
   directory and write the dhcpd.leases file there. */

/* #define DEBUG */

/* Define this to see what the parser is parsing.   You probably don't
   want to see this. */

/* #define DEBUG_TOKENS */

/* Define this to see dumps of incoming and outgoing packets.    This
   slows things down quite a bit... */

/* #define DEBUG_PACKET */

/* Define this if you want to see dumps of expression evaluation. */

/* #define DEBUG_EXPRESSIONS */

/* Define this if you want to see dumps of find_lease() in action. */

/* #define DEBUG_FIND_LEASE */

/* Define this if you want to see dumps of parsed expressions. */

/* #define DEBUG_EXPRESSION_PARSE */

/* Define this if you want to watch the class matching process. */

/* #define DEBUG_CLASS_MATCHING */

/* Define this if you want to track memory usage for the purpose of
   noticing memory leaks quickly. */

/* #define DEBUG_MEMORY_LEAKAGE */
/* #define DEBUG_MEMORY_LEAKAGE_ON_EXIT */

/* Define this if you want exhaustive (and very slow) checking of the
   malloc pool for corruption. */

/* #define DEBUG_MALLOC_POOL */

/* Define this if you want to see a message every time a lease's state
   changes. */
/* #define DEBUG_LEASE_STATE_TRANSITIONS */

/* Define this if you want to maintain a history of the last N operations
   that changed reference counts on objects.   This can be used to debug
   cases where an object is dereferenced too often, or not often enough. */

/* #define DEBUG_RC_HISTORY */

/* Define this if you want to see the history every cycle. */

/* #define DEBUG_RC_HISTORY_EXHAUSTIVELY */

/* This is the number of history entries to maintain - by default, 256. */

/* #define RC_HISTORY_MAX 10240 */

/* Define this if you want dhcpd to dump core when a non-fatal memory
   allocation error is detected (i.e., something that would cause a
   memory leak rather than a memory smash). */

/* #define POINTER_DEBUG */

/* Define this if you want debugging output for DHCP failover protocol
   messages. */

/* #define DEBUG_FAILOVER_MESSAGES */

/* Define this to include contact messages in failover message debugging.
   The contact messages are sent once per second, so this can generate a
   lot of log entries. */

/* #define DEBUG_FAILOVER_CONTACT_MESSAGES */

/* Define this if you want debugging output for DHCP failover protocol
   event timeout timing. */

/* #define DEBUG_FAILOVER_TIMING */

/* Define this if you want to include contact message timing, which is
   performed once per second and can generate a lot of log entries. */

/* #define DEBUG_FAILOVER_CONTACT_TIMING */

/* Define this if you want all leases written to the lease file, even if
   they are free leases that have never been used. */

/* #define DEBUG_DUMP_ALL_LEASES */

/* Define this if you want to see the requests and replies between the
   DHCP code and the DNS library code. */

/* #define DEBUG_DNS_UPDATES */

/* Define this if you want to debug the host part of the inform processing */
/* #define DEBUG_INFORM_HOST */

/* Define this if you want DHCP failover protocol support in the DHCP
   server. */

/* #define FAILOVER_PROTOCOL */

/* Define this if you want DNS update functionality to be available. */

#define NSUPDATE

/* Define this if you want to enable the DHCP server attempting to
   find a nameserver to use for DDNS updates. */
#define DNS_ZONE_LOOKUP

/* Define this if you want the dhcpd.pid file to go somewhere other than
   the default (which varies from system to system, but is usually either
   /etc or /var/run. */

/* #define _PATH_DHCPD_PID	"/var/run/dhcpd.pid" */

/* Define this if you want the dhcpd.leases file (the dynamic lease database)
   to go somewhere other than the default location, which is normally
   /etc/dhcpd.leases. */

/* #define _PATH_DHCPD_DB	"/etc/dhcpd.leases" */

/* Define this if you want the dhcpd.conf file to go somewhere other than
   the default location.   By default, it goes in /etc/dhcpd.conf. */

/* #define _PATH_DHCPD_CONF	"/etc/dhcpd.conf" */

/* Network API definitions.   You do not need to choose one of these - if
   you don't choose, one will be chosen for you in your system's config
   header.    DON'T MESS WITH THIS UNLESS YOU KNOW WHAT YOU'RE DOING!!! */

/* Define USE_SOCKETS to use the standard BSD socket API.

   On many systems, the BSD socket API does not provide the ability to
   send packets to the 255.255.255.255 broadcast address, which can
   prevent some clients (e.g., Win95) from seeing replies.   This is
   not a problem on Solaris.

   In addition, the BSD socket API will not work when more than one
   network interface is configured on the server.

   However, the BSD socket API is about as efficient as you can get, so if
   the aforementioned problems do not matter to you, or if no other
   API is supported for your system, you may want to go with it. */

/* #define USE_SOCKETS */

/* Define this to use the Sun Streams NIT API.

   The Sun Streams NIT API is only supported on SunOS 4.x releases. */

/* #define USE_NIT */

/* Define this to use the Berkeley Packet Filter API.

   The BPF API is available on all 4.4-BSD derivatives, including
   NetBSD, FreeBSD and BSDI's BSD/OS.   It's also available on
   DEC Alpha OSF/1 in a compatibility mode supported by the Alpha OSF/1
   packetfilter interface. */

/* #define USE_BPF */

/* Define this to use the raw socket API.

   The raw socket API is provided on many BSD derivatives, and provides
   a way to send out raw IP packets.   It is only supported for sending
   packets - packets must be received with the regular socket API.
   This code is experimental - I've never gotten it to actually transmit
   a packet to the 255.255.255.255 broadcast address - so use it at your
   own risk. */

/* #define USE_RAW_SOCKETS */

/* Define this to change the logging facility used by dhcpd. */

/* #define DHCPD_LOG_FACILITY LOG_DAEMON */


/* Define this if you want to be able to execute external commands
   during conditional evaluation. */

/* #define ENABLE_EXECUTE */

/* Define this if you aren't debugging and you want to save memory
   (potentially a _lot_ of memory) by allocating leases in chunks rather
   than one at a time. */

#define COMPACT_LEASES

/* Define this if you want to be able to save and playback server operational
   traces. */

/* #define TRACING */

/* Define this if you want the server to use the previous behavior
   when determining the DDNS TTL.  If the user has specified a ddns-ttl
   option that is used to detemine the ttl.  (If the user specifies
   an option that references the lease structure it is only usable
   for v4.  In that case v6 will use the default.) Otherwise when
   defined the defaults are: v4 - 1/2 the lease time,
   v6 - DEFAULT_DDNS_TTL.  When undefined the defaults are 1/2 the
   (preferred) lease time for both but with a cap on the maximum. */

/* #define USE_OLD_DDNS_TTL */

/* Define this if you want a DHCPv6 server to send replies to the
   source port of the message it received.  This is useful for testing
   but is only included for backwards compatibility. */
/* #define REPLY_TO_SOURCE_PORT */

/* Define this if you want to enable strict checks in DNS Updates mechanism.
   Do not enable this unless are DHCP developer. */
/* #define DNS_UPDATES_MEMORY_CHECKS */

/* Define this if you want to allow domain list in domain-name option.
   RFC2132 does not allow that behavior, but it is somewhat used due
   to historic reasons. Note that it may be removed some time in the
   future. */

#define ACCEPT_LIST_IN_DOMAIN_NAME

/* In RFC3315 section 17.2.2 stated that if the server was not going
   to be able to assign any addresses to any IAs in a subsequent Request
   from a client that the server should not include any IAs.  This
   requirement was removed in an errata from August 2010.  Define the
   following if you want the pre-errata version.  
   You should only enable this option if you have clients that
   require the original functionality. */

/* #define RFC3315_PRE_ERRATA_2010_08 */

/* In previous versions of the code when the server generates a NAK
   it doesn't attempt to determine if the configuration included a
   server ID for that client.  Defining this option causes the server
   to make a modest effort to determine the server id when building
   a NAK as a response.  This effort will only check the first subnet
   and pool associated with a shared subnet and will not check for
   host declarations.  With some configurations the server id
   computed for a NAK may not match that computed for an ACK. */

#define SERVER_ID_FOR_NAK

/* When processing a request do a simple check to compare the
   server id the client sent with the one the server would send.
   In order to minimize the complexity of the code the server
   only checks for a server id option in the global and subnet
   scopes.  Complicated configurations may result in differnet
   server ids for this check and when the server id for a reply
   packet is determined, which would prohibit the server from
   responding.

   The primary use for this option is when a client broadcasts
   a request but requires the response to come from one of the
   failover peers.  An example of this would be when a client
   reboots while its lease is still active - in this case both
   servers will normally respond.  Most of the time the client
   won't check the server id and can use either of the responses.
   However if the client does check the server id it may reject
   the response if it came from the wrong peer.  If the timing
   is such that the "wrong" peer responds first most of the time
   the client may not get an address for some time.

   Currently this option is only available when failover is in
   use.

   Care should be taken before enabling this option. */

/* #define SERVER_ID_CHECK */

/* Include code to do a slow transition of DDNS records
   from the interim to the standard version, or backwards.
   The normal code will handle removing an old style record
   when the name on a lease is being changed.  This adds code
   to handle the case where the name isn't being changed but
   the old record should be removed to allow a new record to
   be added.  This is the slow transition as leases are only
   updated as a client touches them.  A fast transition would
   entail updating all the records at once, probably at start
   up. */
#define DDNS_UPDATE_SLOW_TRANSITION
   
/* Include definitions for various options.  In general these
   should be left as is, but if you have already defined one
   of these and prefer your definition you can comment the 
   RFC define out to avoid conflicts */
#define RFC2937_OPTIONS
#define RFC4776_OPTIONS
#define RFC4833_OPTIONS
#define RFC4994_OPTIONS
#define RFC5192_OPTIONS
#define RFC5223_OPTIONS
#define RFC5417_OPTIONS
#define RFC5460_OPTIONS
#define RFC5969_OPTIONS
#define RFC5970_OPTIONS
#define RFC5986_OPTIONS
#define RFC6011_OPTIONS
#define RFC6334_OPTIONS
#define RFC6440_OPTIONS
#define RFC6731_OPTIONS
#define RFC6939_OPTIONS
#define RFC6977_OPTIONS
#define RFC7083_OPTIONS

