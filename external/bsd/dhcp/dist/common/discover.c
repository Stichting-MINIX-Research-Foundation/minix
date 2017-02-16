/*	$NetBSD: discover.c,v 1.4 2014/07/12 12:09:37 spz Exp $	*/
/* discover.c

   Find and identify the network interfaces. */

/*
 * Copyright (c) 2013-2014 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 2004-2009,2011 by Internet Systems Consortium, Inc. ("ISC")
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
__RCSID("$NetBSD: discover.c,v 1.4 2014/07/12 12:09:37 spz Exp $");

#include "dhcpd.h"

#define BSD_COMP		/* needed on Solaris for SIOCGLIFNUM */
#include <sys/ioctl.h>
#include <errno.h>

#ifdef HAVE_NET_IF6_H
# include <net/if6.h>
#endif

struct interface_info *interfaces, *dummy_interfaces, *fallback_interface;
int interfaces_invalidated;
int quiet_interface_discovery;
u_int16_t local_port;
u_int16_t remote_port;
int (*dhcp_interface_setup_hook) (struct interface_info *, struct iaddr *);
int (*dhcp_interface_discovery_hook) (struct interface_info *);
isc_result_t (*dhcp_interface_startup_hook) (struct interface_info *);
int (*dhcp_interface_shutdown_hook) (struct interface_info *);

struct in_addr limited_broadcast;

int local_family = AF_INET;
struct in_addr local_address;

void (*bootp_packet_handler) (struct interface_info *,
			      struct dhcp_packet *, unsigned,
			      unsigned int,
			      struct iaddr, struct hardware *);

#ifdef DHCPv6
void (*dhcpv6_packet_handler)(struct interface_info *,
			      const char *, int,
			      int, const struct iaddr *,
			      isc_boolean_t);
#endif /* DHCPv6 */


omapi_object_type_t *dhcp_type_interface;
#if defined (TRACING)
trace_type_t *interface_trace;
trace_type_t *inpacket_trace;
trace_type_t *outpacket_trace;
#endif
struct interface_info **interface_vector;
int interface_count;
int interface_max;

OMAPI_OBJECT_ALLOC (interface, struct interface_info, dhcp_type_interface)

isc_result_t interface_setup ()
{
	isc_result_t status;
	status = omapi_object_type_register (&dhcp_type_interface,
					     "interface",
					     dhcp_interface_set_value,
					     dhcp_interface_get_value,
					     dhcp_interface_destroy,
					     dhcp_interface_signal_handler,
					     dhcp_interface_stuff_values,
					     dhcp_interface_lookup, 
					     dhcp_interface_create,
					     dhcp_interface_remove,
					     0, 0, 0,
					     sizeof (struct interface_info),
					     interface_initialize, RC_MISC);
	if (status != ISC_R_SUCCESS)
		log_fatal ("Can't register interface object type: %s",
			   isc_result_totext (status));

	return status;
}

#if defined (TRACING)
void interface_trace_setup ()
{
	interface_trace = trace_type_register ("interface", (void *)0,
					       trace_interface_input,
					       trace_interface_stop, MDL);
	inpacket_trace = trace_type_register ("inpacket", (void *)0,
					       trace_inpacket_input,
					       trace_inpacket_stop, MDL);
	outpacket_trace = trace_type_register ("outpacket", (void *)0,
					       trace_outpacket_input,
					       trace_outpacket_stop, MDL);
}
#endif

isc_result_t interface_initialize (omapi_object_t *ipo,
				   const char *file, int line)
{
	struct interface_info *ip = (struct interface_info *)ipo;
	ip -> rfdesc = ip -> wfdesc = -1;
	return ISC_R_SUCCESS;
}


/* 
 * Scanning for Interfaces
 * -----------------------
 *
 * To find interfaces, we create an iterator that abstracts out most 
 * of the platform specifics. Use is fairly straightforward:
 *
 * - begin_iface_scan() starts the process.
 * - Use next_iface() until it returns 0.
 * - end_iface_scan() performs any necessary cleanup.
 *
 * We check for errors on each call to next_iface(), which returns a
 * description of the error as a string if any occurs.
 *
 * We currently have code for Solaris and Linux. Other systems need
 * to have code written.
 *
 * NOTE: the long-term goal is to use the interface code from BIND 9.
 */

#if defined(SIOCGLIFCONF) && defined(SIOCGLIFNUM) && defined(SIOCGLIFFLAGS)

/* HP/UX doesn't define struct lifconf, instead they define struct
 * if_laddrconf.  Similarly, 'struct lifreq' and 'struct lifaddrreq'.
 */
#ifdef ISC_PLATFORM_HAVEIF_LADDRCONF
# define lifc_len iflc_len
# define lifc_buf iflc_buf
# define lifc_req iflc_req
# define LIFCONF if_laddrconf
#else
# define ISC_HAVE_LIFC_FAMILY 1
# define ISC_HAVE_LIFC_FLAGS 1
# define LIFCONF lifconf
#endif

#ifdef ISC_PLATFORM_HAVEIF_LADDRREQ
# define lifr_addr iflr_addr
# define lifr_name iflr_name
# define lifr_dstaddr iflr_dstaddr
# define lifr_flags iflr_flags
# define sockaddr_storage sockaddr_ext
# define ss_family sa_family
# define LIFREQ if_laddrreq
#else
# define LIFREQ lifreq
#endif

#ifndef IF_NAMESIZE
# if defined(LIFNAMSIZ)
#  define IF_NAMESIZE	LIFNAMSIZ
# elif defined(IFNAMSIZ)
#  define IF_NAMESIZE	IFNAMSIZ
# else
#  define IF_NAMESIZE	16
# endif
#endif
#elif !defined(__linux) && !defined(HAVE_IFADDRS_H)
# define SIOCGLIFCONF SIOCGIFCONF
# define SIOCGLIFFLAGS SIOCGIFFLAGS
# define LIFREQ ifreq
# define LIFCONF ifconf
# define lifr_name ifr_name
# define lifr_addr ifr_addr
# define lifr_flags ifr_flags
# define lifc_len ifc_len
# define lifc_buf ifc_buf
# define lifc_req ifc_req
#ifdef _AIX
# define ss_family __ss_family
#endif
#endif

#if defined(SIOCGLIFCONF) && defined(SIOCGLIFFLAGS)
/* 
 * Solaris support
 * ---------------
 *
 * The SIOCGLIFCONF ioctl() are the extension that you need to use
 * on Solaris to get information about IPv6 addresses.
 *
 * Solaris' extended interface is documented in the if_tcp man page.
 */

/* 
 * Structure holding state about the scan.
 */
struct iface_conf_list {
	int sock;		/* file descriptor used to get information */
	int num;		/* total number of interfaces */
	struct LIFCONF conf;	/* structure used to get information */
	int next;		/* next interface to retrieve when iterating */
};

/* 
 * Structure used to return information about a specific interface.
 */
struct iface_info {
	char name[IF_NAMESIZE+1];	/* name of the interface, e.g. "bge0" */
	struct sockaddr_storage addr;	/* address information */
	isc_uint64_t flags;		/* interface flags, e.g. IFF_LOOPBACK */
};

/* 
 * Start a scan of interfaces.
 *
 * The iface_conf_list structure maintains state for this process.
 */
static int 
begin_iface_scan(struct iface_conf_list *ifaces) {
#ifdef ISC_PLATFORM_HAVELIFNUM
	struct lifnum lifnum;
#else
	int lifnum;
#endif

	ifaces->sock = socket(local_family, SOCK_DGRAM, IPPROTO_UDP);
	if (ifaces->sock < 0) {
		log_error("Error creating socket to list interfaces; %m");
		return 0;
	}

	memset(&lifnum, 0, sizeof(lifnum));
#ifdef ISC_PLATFORM_HAVELIFNUM
	lifnum.lifn_family = AF_UNSPEC;
#endif
#ifdef SIOCGLIFNUM
	if (ioctl(ifaces->sock, SIOCGLIFNUM, &lifnum) < 0) {
		log_error("Error finding total number of interfaces; %m");
		close(ifaces->sock);
		ifaces->sock = -1;
		return 0;
	}

#ifdef ISC_PLATFORM_HAVELIFNUM
	ifaces->num = lifnum.lifn_count;
#else
	ifaces->num = lifnum;
#endif
#else
	ifaces->num = 64;
#endif /* SIOCGLIFNUM */

	memset(&ifaces->conf, 0, sizeof(ifaces->conf));
#ifdef ISC_HAVE_LIFC_FAMILY
	ifaces->conf.lifc_family = AF_UNSPEC;
#endif
	ifaces->conf.lifc_len = ifaces->num * sizeof(struct LIFREQ);
	ifaces->conf.lifc_buf = dmalloc(ifaces->conf.lifc_len, MDL);
	if (ifaces->conf.lifc_buf == NULL) {
		log_fatal("Out of memory getting interface list.");
	}

	if (ioctl(ifaces->sock, SIOCGLIFCONF, &ifaces->conf) < 0) {
		log_error("Error getting interfaces configuration list; %m");
		dfree(ifaces->conf.lifc_buf, MDL);
		close(ifaces->sock);
		ifaces->sock = -1;
		return 0;
	}

	ifaces->next = 0;

	return 1;
}

/*
 * Retrieve the next interface.
 *
 * Returns information in the info structure. 
 * Sets err to 1 if there is an error, otherwise 0.
 */
static int
next_iface(struct iface_info *info, int *err, struct iface_conf_list *ifaces) {
	struct LIFREQ *p;
	struct LIFREQ tmp;
	isc_boolean_t foundif;
#if defined(sun) || defined(__linux)
	/* Pointer used to remove interface aliases. */
	char *s;
#endif

	do {
		foundif = ISC_FALSE;

		if (ifaces->next >= ifaces->num) {
			*err = 0;
			return 0;
		}

		p = ifaces->conf.lifc_req;
		p += ifaces->next;

		if (strlen(p->lifr_name) >= sizeof(info->name)) {
			*err = 1;
			log_error("Interface name '%s' too long", p->lifr_name);
			return 0;
		}

		/* Reject if interface address family does not match */
		if (p->lifr_addr.ss_family != local_family) {
			ifaces->next++;
			continue;
		}

		strcpy(info->name, p->lifr_name);
		memset(&info->addr, 0, sizeof(info->addr));
		memcpy(&info->addr, &p->lifr_addr, sizeof(p->lifr_addr));

#if defined(sun) || defined(__linux)
		/* interface aliases look like "eth0:1" or "wlan1:3" */
		s = strchr(info->name, ':');
		if (s != NULL) {
			*s = '\0';
		}
#endif /* defined(sun) || defined(__linux) */

		foundif = ISC_TRUE;
	} while ((foundif == ISC_FALSE) ||
		 (strncmp(info->name, "dummy", 5) == 0));
	
	memset(&tmp, 0, sizeof(tmp));
	strcpy(tmp.lifr_name, info->name);
	if (ioctl(ifaces->sock, SIOCGLIFFLAGS, &tmp) < 0) {
		log_error("Error getting interface flags for '%s'; %m", 
			  p->lifr_name);
		*err = 1;
		return 0;
	}
	info->flags = tmp.lifr_flags;

	ifaces->next++;
	*err = 0;
	return 1;
}

/*
 * End scan of interfaces.
 */
static void
end_iface_scan(struct iface_conf_list *ifaces) {
	dfree(ifaces->conf.lifc_buf, MDL);
	close(ifaces->sock);
	ifaces->sock = -1;
}

#elif __linux /* !HAVE_SIOCGLIFCONF */
/* 
 * Linux support
 * -------------
 *
 * In Linux, we use the /proc pseudo-filesystem to get information
 * about interfaces, along with selected ioctl() calls.
 *
 * Linux low level access is documented in the netdevice man page.
 */

/* 
 * Structure holding state about the scan.
 */
struct iface_conf_list {
	int sock;	/* file descriptor used to get information */
	FILE *fp;	/* input from /proc/net/dev */
#ifdef DHCPv6
	FILE *fp6;	/* input from /proc/net/if_inet6 */
#endif
};

/* 
 * Structure used to return information about a specific interface.
 */
struct iface_info {
	char name[IFNAMSIZ];		/* name of the interface, e.g. "eth0" */
	struct sockaddr_storage addr;	/* address information */
	isc_uint64_t flags;		/* interface flags, e.g. IFF_LOOPBACK */
};

/* 
 * Start a scan of interfaces.
 *
 * The iface_conf_list structure maintains state for this process.
 */
static int 
begin_iface_scan(struct iface_conf_list *ifaces) {
	char buf[256];
	int len;
	int i;

	ifaces->fp = fopen("/proc/net/dev", "r");
	if (ifaces->fp == NULL) {
		log_error("Error opening '/proc/net/dev' to list interfaces");
		return 0;
	}

	/*
	 * The first 2 lines are header information, so read and ignore them.
	 */
	for (i=0; i<2; i++) {
		if (fgets(buf, sizeof(buf), ifaces->fp) == NULL) {
			log_error("Error reading headers from '/proc/net/dev'");
			fclose(ifaces->fp);
			ifaces->fp = NULL;
			return 0;
		}
		len = strlen(buf);
		if ((len <= 0) || (buf[len-1] != '\n')) { 
			log_error("Bad header line in '/proc/net/dev'");
			fclose(ifaces->fp);
			ifaces->fp = NULL;
			return 0;
		}
	}

	ifaces->sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (ifaces->sock < 0) {
		log_error("Error creating socket to list interfaces; %m");
		fclose(ifaces->fp);
		ifaces->fp = NULL;
		return 0;
	}

#ifdef DHCPv6
	if (local_family == AF_INET6) {
		ifaces->fp6 = fopen("/proc/net/if_inet6", "r");
		if (ifaces->fp6 == NULL) {
			log_error("Error opening '/proc/net/if_inet6' to "
				  "list IPv6 interfaces; %m");
			close(ifaces->sock);
			ifaces->sock = -1;
			fclose(ifaces->fp);
			ifaces->fp = NULL;
			return 0;
		}
	}
#endif

	return 1;
}

/*
 * Read our IPv4 interfaces from /proc/net/dev.
 *
 * The file looks something like this:
 *
 * Inter-|   Receive ...
 *  face |bytes    packets errs drop fifo frame ...
 *     lo: 1580562    4207    0    0    0     0 ...
 *   eth0:       0       0    0    0    0     0 ...
 *   eth1:1801552440   37895    0   14    0     ...
 *
 * We only care about the interface name, which is at the start of 
 * each line.
 *
 * We use an ioctl() to get the address and flags for each interface.
 */
static int
next_iface4(struct iface_info *info, int *err, struct iface_conf_list *ifaces) {
	char buf[256];
	int len;
	char *p;
	char *name;
	struct ifreq tmp;

	/*
	 * Loop exits when we find an interface that has an address, or 
	 * when we run out of interfaces.
	 */
	for (;;) {
		do {
			/*
	 		 *  Read the next line in the file.
	 		 */
			if (fgets(buf, sizeof(buf), ifaces->fp) == NULL) {
				if (ferror(ifaces->fp)) {
					*err = 1;
					log_error("Error reading interface "
					  	"information");
				} else {
					*err = 0;
				}
				return 0;
			}

			/*
	 		 * Make sure the line is a nice, 
			 * newline-terminated line.
	 		 */
			len = strlen(buf);
			if ((len <= 0) || (buf[len-1] != '\n')) { 
				log_error("Bad line reading interface "
					  "information");
				*err = 1;
				return 0;
			}

			/*
	 		 * Figure out our name.
	 		 */
			p = strrchr(buf, ':');
			if (p == NULL) {
				log_error("Bad line reading interface "
					  "information (no colon)");
				*err = 1;
				return 0;
			}
			*p = '\0';
			name = buf;
			while (isspace(*name)) {
				name++;
			}

			/* 
		 	 * Copy our name into our interface structure.
		 	 */
			len = p - name;
			if (len >= sizeof(info->name)) {
				*err = 1;
				log_error("Interface name '%s' too long", name);
				return 0;
			}
			strcpy(info->name, name);

#ifdef ALIAS_NAMED_PERMUTED
			/* interface aliases look like "eth0:1" or "wlan1:3" */
			s = strchr(info->name, ':');
			if (s != NULL) {
				*s = '\0';
			}
#endif

#ifdef SKIP_DUMMY_INTERFACES
		} while (strncmp(info->name, "dummy", 5) == 0);
#else
		} while (0);
#endif

		memset(&tmp, 0, sizeof(tmp));
		strcpy(tmp.ifr_name, name);
		if (ioctl(ifaces->sock, SIOCGIFADDR, &tmp) < 0) {
			if (errno == EADDRNOTAVAIL) {
				continue;
			}
			log_error("Error getting interface address "
				  "for '%s'; %m", name);
			*err = 1;
			return 0;
		}
		memcpy(&info->addr, &tmp.ifr_addr, sizeof(tmp.ifr_addr));

		memset(&tmp, 0, sizeof(tmp));
		strcpy(tmp.ifr_name, name);
		if (ioctl(ifaces->sock, SIOCGIFFLAGS, &tmp) < 0) {
			log_error("Error getting interface flags for '%s'; %m", 
			  	name);
			*err = 1;
			return 0;
		}
		info->flags = tmp.ifr_flags;

		*err = 0;
		return 1;
	}
}

#ifdef DHCPv6
/*
 * Read our IPv6 interfaces from /proc/net/if_inet6.
 *
 * The file looks something like this:
 *
 * fe80000000000000025056fffec00008 05 40 20 80   vmnet8
 * 00000000000000000000000000000001 01 80 10 80       lo
 * fe80000000000000025056fffec00001 06 40 20 80   vmnet1
 * 200108881936000202166ffffe497d9b 03 40 00 00     eth1
 * fe8000000000000002166ffffe497d9b 03 40 20 80     eth1
 *
 * We get IPv6 address from the start, the interface name from the end, 
 * and ioctl() to get flags.
 */
static int
next_iface6(struct iface_info *info, int *err, struct iface_conf_list *ifaces) {
	char buf[256];
	int len;
	char *p;
	char *name;
	int i;
	struct sockaddr_in6 addr;
	struct ifreq tmp;

	do {
		/*
		 *  Read the next line in the file.
		 */
		if (fgets(buf, sizeof(buf), ifaces->fp6) == NULL) {
			if (ferror(ifaces->fp6)) {
				*err = 1;
				log_error("Error reading IPv6 "
					  "interface information");
			} else {
				*err = 0;
			}
			return 0;
		}

		/*
		 * Make sure the line is a nice, newline-terminated line.
		 */
		len = strlen(buf);
		if ((len <= 0) || (buf[len-1] != '\n')) { 
			log_error("Bad line reading IPv6 "
				  "interface information");
			*err = 1;
			return 0;
		}

		/*
 		 * Figure out our name.
 		 */
		buf[--len] = '\0';
		p = strrchr(buf, ' ');
		if (p == NULL) {
			log_error("Bad line reading IPv6 interface "
			          "information (no space)");
			*err = 1;
			return 0;
		}
		name = p+1;

		/* 
 		 * Copy our name into our interface structure.
 		 */
		len = strlen(name);
		if (len >= sizeof(info->name)) {
			*err = 1;
			log_error("IPv6 interface name '%s' too long", name);
			return 0;
		}
		strcpy(info->name, name);

#ifdef SKIP_DUMMY_INTERFACES
	} while (strncmp(info->name, "dummy", 5) == 0);
#else
	} while (0);
#endif

	/*
	 * Double-check we start with the IPv6 address.
	 */
	for (i=0; i<32; i++) {
		if (!isxdigit(buf[i]) || isupper(buf[i])) {
			*err = 1;
			log_error("Bad line reading IPv6 interface address "
				  "for '%s'", name);
			return 0;
		}
	}

	/* 
	 * Load our socket structure.
	 */
	memset(&addr, 0, sizeof(addr));
	addr.sin6_family = AF_INET6;
	for (i=0; i<16; i++) {
		unsigned char byte;
                static const char hex[] = "0123456789abcdef";
                byte = ((index(hex, buf[i * 2]) - hex) << 4) |
			(index(hex, buf[i * 2 + 1]) - hex);
		addr.sin6_addr.s6_addr[i] = byte;
	}
	memcpy(&info->addr, &addr, sizeof(addr));

	/*
	 * Get our flags.
	 */
	memset(&tmp, 0, sizeof(tmp));
	strcpy(tmp.ifr_name, name);
	if (ioctl(ifaces->sock, SIOCGIFFLAGS, &tmp) < 0) {
		log_error("Error getting interface flags for '%s'; %m", name);
		*err = 1;
		return 0;
	}
	info->flags = tmp.ifr_flags;

	*err = 0;
	return 1;
}
#endif /* DHCPv6 */

/*
 * Retrieve the next interface.
 *
 * Returns information in the info structure. 
 * Sets err to 1 if there is an error, otherwise 0.
 */
static int
next_iface(struct iface_info *info, int *err, struct iface_conf_list *ifaces) {
	if (next_iface4(info, err, ifaces)) {
		return 1;
	}
#ifdef DHCPv6
	if (!(*err)) {
		if (local_family == AF_INET6)
			return next_iface6(info, err, ifaces);
	}
#endif
	return 0;
}

/*
 * End scan of interfaces.
 */
static void
end_iface_scan(struct iface_conf_list *ifaces) {
	fclose(ifaces->fp);
	ifaces->fp = NULL;
	close(ifaces->sock);
	ifaces->sock = -1;
#ifdef DHCPv6
	if (local_family == AF_INET6) {
		fclose(ifaces->fp6);
		ifaces->fp6 = NULL;
	}
#endif
}
#else

/* 
 * BSD support
 * -----------
 *
 * FreeBSD, NetBSD, OpenBSD, and OS X all have the getifaddrs() 
 * function.
 *
 * The getifaddrs() man page describes the use.
 */

#include <ifaddrs.h>

/* 
 * Structure holding state about the scan.
 */
struct iface_conf_list {
	struct ifaddrs *head;	/* beginning of the list */
	struct ifaddrs *next;	/* current position in the list */
};

/* 
 * Structure used to return information about a specific interface.
 */
struct iface_info {
	char name[IFNAMSIZ];		/* name of the interface, e.g. "bge0" */
	struct sockaddr_storage addr;	/* address information */
	isc_uint64_t flags;		/* interface flags, e.g. IFF_LOOPBACK */
};

/* 
 * Start a scan of interfaces.
 *
 * The iface_conf_list structure maintains state for this process.
 */
static int 
begin_iface_scan(struct iface_conf_list *ifaces) {
	if (getifaddrs(&ifaces->head) != 0) {
		log_error("Error getting interfaces; %m");
		return 0;
	}
	ifaces->next = ifaces->head;
	return 1;
}

/*
 * Retrieve the next interface.
 *
 * Returns information in the info structure. 
 * Sets err to 1 if there is an error, otherwise 0.
 */
static int
next_iface(struct iface_info *info, int *err, struct iface_conf_list *ifaces) {
	if (ifaces->next == NULL) {
		*err = 0;
		return 0;
	}
	if (strlen(ifaces->next->ifa_name) >= sizeof(info->name)) {
		log_error("Interface name '%s' too long", 
			  ifaces->next->ifa_name);
		*err = 1;
		return 0;
	}
	strcpy(info->name, ifaces->next->ifa_name);
	memcpy(&info->addr, ifaces->next->ifa_addr, 
	       ifaces->next->ifa_addr->sa_len);
	info->flags = ifaces->next->ifa_flags;
	ifaces->next = ifaces->next->ifa_next;
	*err = 0;
	return 1;
}

/*
 * End scan of interfaces.
 */
static void
end_iface_scan(struct iface_conf_list *ifaces) {
	freeifaddrs(ifaces->head);
	ifaces->head = NULL;
	ifaces->next = NULL;
}
#endif 

/* XXX: perhaps create drealloc() rather than do it manually */
static void
add_ipv4_addr_to_interface(struct interface_info *iface, 
			   const struct in_addr *addr) {
	/*
	 * We don't expect a lot of addresses per IPv4 interface, so
	 * we use 4, as our "chunk size" for collecting addresses.
	 */
	if (iface->addresses == NULL) {
		iface->addresses = dmalloc(4 * sizeof(struct in_addr), MDL);
		if (iface->addresses == NULL) {
			log_fatal("Out of memory saving IPv4 address "
			          "on interface.");
		}
		iface->address_count = 0;
		iface->address_max = 4;
	} else if (iface->address_count >= iface->address_max) {
		struct in_addr *tmp;
		int new_max;

		new_max = iface->address_max + 4;
		tmp = dmalloc(new_max * sizeof(struct in_addr), MDL);
		if (tmp == NULL) {
			log_fatal("Out of memory saving IPv4 address "
			          "on interface.");
		}
		memcpy(tmp, 
		       iface->addresses, 
		       iface->address_max * sizeof(struct in_addr));
		dfree(iface->addresses, MDL);
		iface->addresses = tmp;
		iface->address_max = new_max;
	}
	iface->addresses[iface->address_count++] = *addr;
}

#ifdef DHCPv6
/* XXX: perhaps create drealloc() rather than do it manually */
static void
add_ipv6_addr_to_interface(struct interface_info *iface, 
			   const struct in6_addr *addr) {
	/*
	 * Each IPv6 interface will have at least two IPv6 addresses,
	 * and likely quite a few more. So we use 8, as our "chunk size" for
	 * collecting addresses.
	 */
	if (iface->v6addresses == NULL) {
		iface->v6addresses = dmalloc(8 * sizeof(struct in6_addr), MDL);
		if (iface->v6addresses == NULL) {
			log_fatal("Out of memory saving IPv6 address "
				  "on interface.");
		}
		iface->v6address_count = 0;
		iface->v6address_max = 8;
	} else if (iface->v6address_count >= iface->v6address_max) {
		struct in6_addr *tmp;
		int new_max;

		new_max = iface->v6address_max + 8;
		tmp = dmalloc(new_max * sizeof(struct in6_addr), MDL);
		if (tmp == NULL) {
			log_fatal("Out of memory saving IPv6 address "
				  "on interface.");
		}
		memcpy(tmp, 
		       iface->v6addresses, 
		       iface->v6address_max * sizeof(struct in6_addr));
		dfree(iface->v6addresses, MDL);
		iface->v6addresses = tmp;
		iface->v6address_max = new_max;
	}
	iface->v6addresses[iface->v6address_count++] = *addr;
}
#endif /* DHCPv6 */

/* Use the SIOCGIFCONF ioctl to get a list of all the attached interfaces.
   For each interface that's of type INET and not the loopback interface,
   register that interface with the network I/O software, figure out what
   subnet it's on, and add it to the list of interfaces. */

void 
discover_interfaces(int state) {
	struct iface_conf_list ifaces;
	struct iface_info info;
	int err;

	struct interface_info *tmp;
	struct interface_info *last, *next;

#ifdef DHCPv6
        char abuf[sizeof("ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255")];
#endif /* DHCPv6 */


	struct subnet *subnet;
	int ir;
	isc_result_t status;
	int wifcount = 0;

	static int setup_fallback = 0;

	if (!begin_iface_scan(&ifaces)) {
		log_fatal("Can't get list of interfaces.");
	}

	/* If we already have a list of interfaces, and we're running as
	   a DHCP server, the interfaces were requested. */
	if (interfaces && (state == DISCOVER_SERVER ||
			   state == DISCOVER_RELAY ||
			   state == DISCOVER_REQUESTED))
		ir = 0;
	else if (state == DISCOVER_UNCONFIGURED)
		ir = INTERFACE_REQUESTED | INTERFACE_AUTOMATIC;
	else
		ir = INTERFACE_REQUESTED;

	/* Cycle through the list of interfaces looking for IP addresses. */
	while (next_iface(&info, &err, &ifaces)) {

		/* See if we've seen an interface that matches this one. */
		for (tmp = interfaces; tmp; tmp = tmp->next) {
			if (!strcmp(tmp->name, info.name))
				break;
		}

		/* Skip non broadcast interfaces (plus loopback and
		   point-to-point in case an OS incorrectly marks them
		   as broadcast). Also skip down interfaces unless we're
		   trying to get a list of configurable interfaces. */
		if ((((local_family == AF_INET &&
		    !(info.flags & IFF_BROADCAST)) ||
#ifdef DHCPv6
		    (local_family == AF_INET6 &&
		    !(info.flags & IFF_MULTICAST)) ||
#endif
		      info.flags & IFF_LOOPBACK ||
		      info.flags & IFF_POINTOPOINT) && !tmp) ||
		    (!(info.flags & IFF_UP) &&
		     state != DISCOVER_UNCONFIGURED))
			continue;
		
		/* If there isn't already an interface by this name,
		   allocate one. */
		if (tmp == NULL) {
			status = interface_allocate(&tmp, MDL);
			if (status != ISC_R_SUCCESS) {
				log_fatal("Error allocating interface %s: %s",
					  info.name, isc_result_totext(status));
			}
			strcpy(tmp->name, info.name);
			interface_snorf(tmp, ir);
			interface_dereference(&tmp, MDL);
			tmp = interfaces; /* XXX */
		}

		if (dhcp_interface_discovery_hook) {
			(*dhcp_interface_discovery_hook)(tmp);
		}

		if ((info.addr.ss_family == AF_INET) && 
		    (local_family == AF_INET)) {
			struct sockaddr_in *a = (struct sockaddr_in*)&info.addr;
			struct iaddr addr;

			/* We don't want the loopback interface. */
			if (a->sin_addr.s_addr == htonl(INADDR_LOOPBACK) &&
			    ((tmp->flags & INTERFACE_AUTOMATIC) &&
			     state == DISCOVER_SERVER))
				continue;

			/* If the only address we have is 0.0.0.0, we
			   shouldn't consider the interface configured. */
			if (a->sin_addr.s_addr != htonl(INADDR_ANY))
				tmp->configured = 1;

			add_ipv4_addr_to_interface(tmp, &a->sin_addr);

			/* invoke the setup hook */
			addr.len = 4;
			memcpy(addr.iabuf, &a->sin_addr.s_addr, addr.len);
			if (dhcp_interface_setup_hook) {
				(*dhcp_interface_setup_hook)(tmp, &addr);
			}
		}
#ifdef DHCPv6
		else if ((info.addr.ss_family == AF_INET6) && 
			 (local_family == AF_INET6)) {
			struct sockaddr_in6 *a = 
					(struct sockaddr_in6*)&info.addr;
			struct iaddr addr;

			/* We don't want the loopback interface. */
			if (IN6_IS_ADDR_LOOPBACK(&a->sin6_addr) && 
			    ((tmp->flags & INTERFACE_AUTOMATIC) &&
			     state == DISCOVER_SERVER))
			    continue;

			/* If the only address we have is 0.0.0.0, we
			   shouldn't consider the interface configured. */
			if (IN6_IS_ADDR_UNSPECIFIED(&a->sin6_addr))
				tmp->configured = 1;

			add_ipv6_addr_to_interface(tmp, &a->sin6_addr);

			/* invoke the setup hook */
			addr.len = 16;
			memcpy(addr.iabuf, &a->sin6_addr, addr.len);
			if (dhcp_interface_setup_hook) {
				(*dhcp_interface_setup_hook)(tmp, &addr);
			}
		}
#endif /* DHCPv6 */
	}

	if (err) {
		log_fatal("Error getting interface information.");
	}

	end_iface_scan(&ifaces);


	/* Mock-up an 'ifp' structure which is no longer used in the
	 * new interface-sensing code, but is used in higher layers
	 * (for example to sense fallback interfaces).
	 */
	for (tmp = interfaces ; tmp != NULL ; tmp = tmp->next) {
		if (tmp->ifp == NULL) {
			struct ifreq *tif;

			tif = (struct ifreq *)dmalloc(sizeof(struct ifreq),
						      MDL);
			if (tif == NULL)
				log_fatal("no space for ifp mockup.");
			strcpy(tif->ifr_name, tmp->name);
			tmp->ifp = tif;
		}
	}


	/* If we're just trying to get a list of interfaces that we might
	   be able to configure, we can quit now. */
	if (state == DISCOVER_UNCONFIGURED) {
		return;
	}

	/* Weed out the interfaces that did not have IP addresses. */
	tmp = last = next = NULL;
	if (interfaces)
		interface_reference (&tmp, interfaces, MDL);
	while (tmp) {
		if (next)
			interface_dereference (&next, MDL);
		if (tmp -> next)
			interface_reference (&next, tmp -> next, MDL);
		/* skip interfaces that are running already */
		if (tmp -> flags & INTERFACE_RUNNING) {
			interface_dereference(&tmp, MDL);
			if(next)
				interface_reference(&tmp, next, MDL);
			continue;
		}
		if ((tmp -> flags & INTERFACE_AUTOMATIC) &&
		    state == DISCOVER_REQUESTED)
			tmp -> flags &= ~(INTERFACE_AUTOMATIC |
					  INTERFACE_REQUESTED);

#ifdef DHCPv6
		if (!(tmp->flags & INTERFACE_REQUESTED)) {
#else
		if (!tmp -> ifp || !(tmp -> flags & INTERFACE_REQUESTED)) {
#endif /* DHCPv6 */
			if ((tmp -> flags & INTERFACE_REQUESTED) != ir)
				log_fatal ("%s: not found", tmp -> name);
			if (!last) {
				if (interfaces)
					interface_dereference (&interfaces,
							       MDL);
				if (next)
				interface_reference (&interfaces, next, MDL);
			} else {
				interface_dereference (&last -> next, MDL);
				if (next)
					interface_reference (&last -> next,
							     next, MDL);
			}
			if (tmp -> next)
				interface_dereference (&tmp -> next, MDL);

			/* Remember the interface in case we need to know
			   about it later. */
			if (dummy_interfaces) {
				interface_reference (&tmp -> next,
						     dummy_interfaces, MDL);
				interface_dereference (&dummy_interfaces, MDL);
			}
			interface_reference (&dummy_interfaces, tmp, MDL);
			interface_dereference (&tmp, MDL);
			if (next)
				interface_reference (&tmp, next, MDL);
			continue;
		}
		last = tmp;

		/* We must have a subnet declaration for each interface. */
		if (!tmp->shared_network && (state == DISCOVER_SERVER)) {
			log_error("%s", "");
			if (local_family == AF_INET) {
				log_error("No subnet declaration for %s (%s).",
					  tmp->name, 
					  (tmp->addresses == NULL) ?
					   "no IPv4 addresses" :
					   inet_ntoa(tmp->addresses[0]));
#ifdef DHCPv6
			} else {
				if (tmp->v6addresses != NULL) {
					inet_ntop(AF_INET6, 
						  &tmp->v6addresses[0],
						  abuf,
						  sizeof(abuf));
				} else {
					strcpy(abuf, "no IPv6 addresses");
				}
				log_error("No subnet6 declaration for %s (%s).",
					  tmp->name,
					  abuf);
#endif /* DHCPv6 */
			}
			if (supports_multiple_interfaces(tmp)) {
				log_error ("** Ignoring requests on %s.  %s",
					   tmp -> name, "If this is not what");
				log_error ("   you want, please write %s",
#ifdef DHCPv6
				           (local_family != AF_INET) ?
					   "a subnet6 declaration" :
#endif
					   "a subnet declaration");
				log_error ("   in your dhcpd.conf file %s",
					   "for the network segment");
				log_error ("   to %s %s %s",
					   "which interface",
					   tmp -> name, "is attached. **");
				log_error ("%s", "");
				goto next;
			} else {
				log_error ("You must write a %s",
#ifdef DHCPv6
				           (local_family != AF_INET) ?
					   "subnet6 declaration for this" :
#endif
					   "subnet declaration for this");
				log_error ("subnet.   You cannot prevent %s",
					   "the DHCP server");
				log_error ("from listening on this subnet %s",
					   "because your");
				log_fatal ("operating system does not %s.",
					   "support this capability");
			}
		}

		/* Find subnets that don't have valid interface
		   addresses... */
		for (subnet = (tmp -> shared_network
			       ? tmp -> shared_network -> subnets
			       : (struct subnet *)0);
		     subnet; subnet = subnet -> next_sibling) {
			/* Set the interface address for this subnet
			   to the first address we found. */
		     	if (subnet->interface_address.len == 0) {
				if (tmp->address_count > 0) {
					subnet->interface_address.len = 4;
					memcpy(subnet->interface_address.iabuf,
					       &tmp->addresses[0].s_addr, 4);
				} else if (tmp->v6address_count > 0) {
					subnet->interface_address.len = 16;
					memcpy(subnet->interface_address.iabuf,
					       &tmp->v6addresses[0].s6_addr, 
					       16);
				} else {
					/* XXX: should be one */
					log_error("%s missing an interface "
						  "address", tmp->name);
					continue;
				}
			}
		}

		/* Flag the index as not having been set, so that the
		   interface registerer can set it or not as it chooses. */
		tmp -> index = -1;

		/* Register the interface... */
		if (local_family == AF_INET) {
			if_register_receive(tmp);
			if_register_send(tmp);
#ifdef DHCPv6
		} else {
			if ((state == DISCOVER_SERVER) ||
			    (state == DISCOVER_RELAY)) {
				if_register6(tmp, 1);
			} else {
				if_register_linklocal6(tmp);
			}
#endif /* DHCPv6 */
		}

		interface_stash (tmp);
		wifcount++;
#if defined (F_SETFD)
		if (fcntl (tmp -> rfdesc, F_SETFD, 1) < 0)
			log_error ("Can't set close-on-exec on %s: %m",
				   tmp -> name);
		if (tmp -> rfdesc != tmp -> wfdesc) {
			if (fcntl (tmp -> wfdesc, F_SETFD, 1) < 0)
				log_error ("Can't set close-on-exec on %s: %m",
					   tmp -> name);
		}
#endif
	      next:
		interface_dereference (&tmp, MDL);
		if (next)
			interface_reference (&tmp, next, MDL);
	}

	/*
	 * Now register all the remaining interfaces as protocols.
	 * We register with omapi to allow for control of the interface,
	 * we've already registered the fd or socket with the socket
	 * manager as part of if_register_receive().
	 */
	for (tmp = interfaces; tmp; tmp = tmp -> next) {
		/* not if it's been registered before */
		if (tmp -> flags & INTERFACE_RUNNING)
			continue;
		if (tmp -> rfdesc == -1)
			continue;
		switch (local_family) {
#ifdef DHCPv6 
		case AF_INET6:
			status = omapi_register_io_object((omapi_object_t *)tmp,
							  if_readsocket, 
							  0, got_one_v6, 0, 0);
			break;
#endif /* DHCPv6 */
		case AF_INET:
		default:
			status = omapi_register_io_object((omapi_object_t *)tmp,
							  if_readsocket, 
							  0, got_one, 0, 0);
			break;
		}

		if (status != ISC_R_SUCCESS)
			log_fatal ("Can't register I/O handle for %s: %s",
				   tmp -> name, isc_result_totext (status));

#if defined(DHCPv6)
		/* Only register the first interface for V6, since
		 * servers and relays all use the same socket.
		 * XXX: This has some messy side effects if we start
		 * dynamically adding and removing interfaces, but
		 * we're well beyond that point in terms of mess.
		 */
		if (((state == DISCOVER_SERVER) || (state == DISCOVER_RELAY)) &&
		    (local_family == AF_INET6))
			break;
#endif
	} /* for (tmp = interfaces; ... */

	if (state == DISCOVER_SERVER && wifcount == 0) {
		log_info ("%s", "");
		log_fatal ("Not configured to listen on any interfaces!");
	}

	if ((local_family == AF_INET) && !setup_fallback) {
		setup_fallback = 1;
		maybe_setup_fallback();
	}

#if defined (F_SETFD)
	if (fallback_interface) {
	    if (fcntl (fallback_interface -> rfdesc, F_SETFD, 1) < 0)
		log_error ("Can't set close-on-exec on fallback: %m");
	    if (fallback_interface -> rfdesc != fallback_interface -> wfdesc) {
		if (fcntl (fallback_interface -> wfdesc, F_SETFD, 1) < 0)
		    log_error ("Can't set close-on-exec on fallback: %m");
	    }
	}
#endif /* F_SETFD */
}

int if_readsocket (h)
	omapi_object_t *h;
{
	struct interface_info *ip;

	if (h -> type != dhcp_type_interface)
		return -1;
	ip = (struct interface_info *)h;
	return ip -> rfdesc;
}

int setup_fallback (struct interface_info **fp, const char *file, int line)
{
	isc_result_t status;

	status = interface_allocate (&fallback_interface, file, line);
	if (status != ISC_R_SUCCESS)
		log_fatal ("Error allocating fallback interface: %s",
			   isc_result_totext (status));
	strcpy (fallback_interface -> name, "fallback");
	if (dhcp_interface_setup_hook)
		(*dhcp_interface_setup_hook) (fallback_interface,
					      (struct iaddr *)0);
	status = interface_reference (fp, fallback_interface, file, line);

	fallback_interface -> index = -1;
	interface_stash (fallback_interface);
	return status == ISC_R_SUCCESS;
}

void reinitialize_interfaces ()
{
	struct interface_info *ip;

	for (ip = interfaces; ip; ip = ip -> next) {
		if_reinitialize_receive (ip);
		if_reinitialize_send (ip);
	}

	if (fallback_interface)
		if_reinitialize_send (fallback_interface);

	interfaces_invalidated = 1;
}

isc_result_t got_one (h)
	omapi_object_t *h;
{
	struct sockaddr_in from;
	struct hardware hfrom;
	struct iaddr ifrom;
	int result;
	union {
		unsigned char packbuf [4095]; /* Packet input buffer.
					 	 Must be as large as largest
						 possible MTU. */
		struct dhcp_packet packet;
	} u;
	struct interface_info *ip;

	if (h -> type != dhcp_type_interface)
		return DHCP_R_INVALIDARG;
	ip = (struct interface_info *)h;

      again:
	if ((result =
	     receive_packet (ip, u.packbuf, sizeof u, &from, &hfrom)) < 0) {
		log_error ("receive_packet failed on %s: %m", ip -> name);
		return ISC_R_UNEXPECTED;
	}
	if (result == 0)
		return ISC_R_UNEXPECTED;

	/*
	 * If we didn't at least get the fixed portion of the BOOTP
	 * packet, drop the packet.
	 * Previously we allowed packets with no sname or filename
	 * as we were aware of at least one client that did.  But
	 * a bug caused short packets to not work and nobody has
	 * complained, it seems rational to tighten up that
	 * restriction.
	 */
	if (result < DHCP_FIXED_NON_UDP)
		return ISC_R_UNEXPECTED;

#if defined(IP_PKTINFO) && defined(IP_RECVPKTINFO) && defined(USE_V4_PKTINFO)
	{
		/* We retrieve the ifindex from the unused hfrom variable */
		unsigned int ifindex;

		memcpy(&ifindex, hfrom.hbuf, sizeof (ifindex));

		/*
		 * Seek forward from the first interface to find the matching
		 * source interface by interface index.
		 */
		ip = interfaces;
		while ((ip != NULL) && (if_nametoindex(ip->name) != ifindex))
			ip = ip->next;
		if (ip == NULL)
			return ISC_R_NOTFOUND;
	}
#endif

	if (bootp_packet_handler) {
		ifrom.len = 4;
		memcpy (ifrom.iabuf, &from.sin_addr, ifrom.len);

		(*bootp_packet_handler) (ip, &u.packet, (unsigned)result,
					 from.sin_port, ifrom, &hfrom);
	}

	/* If there is buffered data, read again.    This is for, e.g.,
	   bpf, which may return two packets at once. */
	if (ip -> rbuf_offset != ip -> rbuf_len)
		goto again;
	return ISC_R_SUCCESS;
}

#ifdef DHCPv6
isc_result_t
got_one_v6(omapi_object_t *h) {
	struct sockaddr_in6 from;
	struct in6_addr to;
	struct iaddr ifrom;
	int result;
	char buf[65536];	/* maximum size for a UDP packet is 65536 */
	struct interface_info *ip;
	int is_unicast;
	unsigned int if_idx = 0;

	if (h->type != dhcp_type_interface) {
		return DHCP_R_INVALIDARG;
	}
	ip = (struct interface_info *)h;

	result = receive_packet6(ip, (unsigned char *)buf, sizeof(buf),
				 &from, &to, &if_idx);
	if (result < 0) {
		log_error("receive_packet6() failed on %s: %m", ip->name);
		return ISC_R_UNEXPECTED;
	}

	/* 0 is 'any' interface. */
	if (if_idx == 0)
		return ISC_R_NOTFOUND;

	if (dhcpv6_packet_handler != NULL) {
		/*
		 * If a packet is not multicast, we assume it is unicast.
		 */
		if (IN6_IS_ADDR_MULTICAST(&to)) { 
			is_unicast = ISC_FALSE;
		} else {
			is_unicast = ISC_TRUE;
		}

		ifrom.len = 16;
		memcpy(ifrom.iabuf, &from.sin6_addr, ifrom.len);

		/* Seek forward to find the matching source interface. */
		ip = interfaces;
		while ((ip != NULL) && (if_nametoindex(ip->name) != if_idx))
			ip = ip->next;

		if (ip == NULL)
			return ISC_R_NOTFOUND;

		(*dhcpv6_packet_handler)(ip, buf, 
					 result, from.sin6_port, 
					 &ifrom, is_unicast);
	}

	return ISC_R_SUCCESS;
}
#endif /* DHCPv6 */

isc_result_t dhcp_interface_set_value  (omapi_object_t *h,
					omapi_object_t *id,
					omapi_data_string_t *name,
					omapi_typed_data_t *value)
{
	struct interface_info *interface;
	isc_result_t status;

	if (h -> type != dhcp_type_interface)
		return DHCP_R_INVALIDARG;
	interface = (struct interface_info *)h;

	if (!omapi_ds_strcmp (name, "name")) {
		if ((value -> type == omapi_datatype_data ||
		     value -> type == omapi_datatype_string) &&
		    value -> u.buffer.len < sizeof interface -> name) {
			memcpy (interface -> name,
				value -> u.buffer.value,
				value -> u.buffer.len);
			interface -> name [value -> u.buffer.len] = 0;
		} else
			return DHCP_R_INVALIDARG;
		return ISC_R_SUCCESS;
	}

	/* Try to find some inner object that can take the value. */
	if (h -> inner && h -> inner -> type -> set_value) {
		status = ((*(h -> inner -> type -> set_value))
			  (h -> inner, id, name, value));
		if (status == ISC_R_SUCCESS || status == DHCP_R_UNCHANGED)
			return status;
	}
			  
	return ISC_R_NOTFOUND;
}


isc_result_t dhcp_interface_get_value (omapi_object_t *h,
				       omapi_object_t *id,
				       omapi_data_string_t *name,
				       omapi_value_t **value)
{
	return ISC_R_NOTIMPLEMENTED;
}

isc_result_t dhcp_interface_destroy (omapi_object_t *h,
					 const char *file, int line)
{
	struct interface_info *interface;

	if (h -> type != dhcp_type_interface)
		return DHCP_R_INVALIDARG;
	interface = (struct interface_info *)h;

	if (interface -> ifp) {
		dfree (interface -> ifp, file, line);
		interface -> ifp = 0;
	}
	if (interface -> next)
		interface_dereference (&interface -> next, file, line);
	if (interface -> rbuf) {
		dfree (interface -> rbuf, file, line);
		interface -> rbuf = (unsigned char *)0;
	}
	if (interface -> client)
		interface -> client = (struct client_state *)0;

	if (interface -> shared_network)
		omapi_object_dereference ((void *)
					  &interface -> shared_network, MDL);

	return ISC_R_SUCCESS;
}

isc_result_t dhcp_interface_signal_handler (omapi_object_t *h,
					    const char *name, va_list ap)
{
	struct interface_info *ip, *interface;
	isc_result_t status;

	if (h -> type != dhcp_type_interface)
		return DHCP_R_INVALIDARG;
	interface = (struct interface_info *)h;

	/* If it's an update signal, see if the interface is dead right
	   now, or isn't known at all, and if that's the case, revive it. */
	if (!strcmp (name, "update")) {
		for (ip = dummy_interfaces; ip; ip = ip -> next)
			if (ip == interface)
				break;
		if (ip && dhcp_interface_startup_hook)
			return (*dhcp_interface_startup_hook) (ip);

		for (ip = interfaces; ip; ip = ip -> next)
			if (ip == interface)
				break;
		if (!ip && dhcp_interface_startup_hook)
			return (*dhcp_interface_startup_hook) (ip);
	}

	/* Try to find some inner object that can take the value. */
	if (h -> inner && h -> inner -> type -> signal_handler) {
		status = ((*(h -> inner -> type -> signal_handler))
			  (h -> inner, name, ap));
		if (status == ISC_R_SUCCESS)
			return status;
	}
	return ISC_R_NOTFOUND;
}

isc_result_t dhcp_interface_stuff_values (omapi_object_t *c,
					  omapi_object_t *id,
					  omapi_object_t *h)
{
	struct interface_info *interface;
	isc_result_t status;

	if (h -> type != dhcp_type_interface)
		return DHCP_R_INVALIDARG;
	interface = (struct interface_info *)h;

	/* Write out all the values. */

	status = omapi_connection_put_name (c, "state");
	if (status != ISC_R_SUCCESS)
		return status;
	if ((interface->flags & INTERFACE_REQUESTED) != 0)
	    status = omapi_connection_put_string (c, "up");
	else
	    status = omapi_connection_put_string (c, "down");
	if (status != ISC_R_SUCCESS)
		return status;

	/* Write out the inner object, if any. */
	if (h -> inner && h -> inner -> type -> stuff_values) {
		status = ((*(h -> inner -> type -> stuff_values))
			  (c, id, h -> inner));
		if (status == ISC_R_SUCCESS)
			return status;
	}

	return ISC_R_SUCCESS;
}

isc_result_t dhcp_interface_lookup (omapi_object_t **ip,
				    omapi_object_t *id,
				    omapi_object_t *ref)
{
	omapi_value_t *tv = (omapi_value_t *)0;
	isc_result_t status;
	struct interface_info *interface;

	if (!ref)
		return DHCP_R_NOKEYS;

	/* First see if we were sent a handle. */
	status = omapi_get_value_str (ref, id, "handle", &tv);
	if (status == ISC_R_SUCCESS) {
		status = omapi_handle_td_lookup (ip, tv -> value);

		omapi_value_dereference (&tv, MDL);
		if (status != ISC_R_SUCCESS)
			return status;

		/* Don't return the object if the type is wrong. */
		if ((*ip) -> type != dhcp_type_interface) {
			omapi_object_dereference (ip, MDL);
			return DHCP_R_INVALIDARG;
		}
	}

	/* Now look for an interface name. */
	status = omapi_get_value_str (ref, id, "name", &tv);
	if (status == ISC_R_SUCCESS) {
		char *s;
		unsigned len;
		for (interface = interfaces; interface;
		     interface = interface -> next) {
		    s = memchr (interface -> name, 0, IFNAMSIZ);
		    if (s)
			    len = s - &interface -> name [0];
		    else
			    len = IFNAMSIZ;
		    if ((tv -> value -> u.buffer.len == len &&
			 !memcmp (interface -> name,
				  (char *)tv -> value -> u.buffer.value,
				  len)))
			    break;
		}
		if (!interface) {
		    for (interface = dummy_interfaces;
			 interface; interface = interface -> next) {
			    s = memchr (interface -> name, 0, IFNAMSIZ);
			    if (s)
				    len = s - &interface -> name [0];
			    else
				    len = IFNAMSIZ;
			    if ((tv -> value -> u.buffer.len == len &&
				 !memcmp (interface -> name,
					  (char *)
					  tv -> value -> u.buffer.value,
					  len)))
				    break;
		    }
		}

		omapi_value_dereference (&tv, MDL);
		if (*ip && *ip != (omapi_object_t *)interface) {
			omapi_object_dereference (ip, MDL);
			return DHCP_R_KEYCONFLICT;
		} else if (!interface) {
			if (*ip)
				omapi_object_dereference (ip, MDL);
			return ISC_R_NOTFOUND;
		} else if (!*ip)
			omapi_object_reference (ip,
						(omapi_object_t *)interface,
						MDL);
	}

	/* If we get to here without finding an interface, no valid key was
	   specified. */
	if (!*ip)
		return DHCP_R_NOKEYS;
	return ISC_R_SUCCESS;
}

/* actually just go discover the interface */
isc_result_t dhcp_interface_create (omapi_object_t **lp,
				    omapi_object_t *id)
{
 	struct interface_info *hp;
	isc_result_t status;
	
	hp = (struct interface_info *)0;
	status = interface_allocate (&hp, MDL);
 	if (status != ISC_R_SUCCESS)
		return status;
 	hp -> flags = INTERFACE_REQUESTED;
	status = interface_reference ((struct interface_info **)lp, hp, MDL);
	interface_dereference (&hp, MDL);
	return status;
}

isc_result_t dhcp_interface_remove (omapi_object_t *lp,
				    omapi_object_t *id)
{
 	struct interface_info *interface, *ip, *last;

	interface = (struct interface_info *)lp;

	/* remove from interfaces */
	last = 0;
	for (ip = interfaces; ip; ip = ip -> next) {
		if (ip == interface) {
			if (last) {
				interface_dereference (&last -> next, MDL);
				if (ip -> next)
					interface_reference (&last -> next,
							     ip -> next, MDL);
			} else {
				interface_dereference (&interfaces, MDL);
				if (ip -> next)
					interface_reference (&interfaces,
							     ip -> next, MDL);
			}
			if (ip -> next)
				interface_dereference (&ip -> next, MDL);
			break;
		}
		last = ip;
	}
	if (!ip)
		return ISC_R_NOTFOUND;

	/* add the interface to the dummy_interface list */
	if (dummy_interfaces) {
		interface_reference (&interface -> next,
				     dummy_interfaces, MDL);
		interface_dereference (&dummy_interfaces, MDL);
	}
	interface_reference (&dummy_interfaces, interface, MDL);

	/* do a DHCPRELEASE */
	if (dhcp_interface_shutdown_hook)
		(*dhcp_interface_shutdown_hook) (interface);

	/* remove the io object */
	omapi_unregister_io_object ((omapi_object_t *)interface);

	switch(local_family) {
#ifdef DHCPv6
	case AF_INET6:
		if_deregister6(interface);
		break;
#endif /* DHCPv6 */
	case AF_INET:
	default:
		if_deregister_send(interface);
		if_deregister_receive(interface);
		break;
	}

	return ISC_R_SUCCESS;
}

void interface_stash (struct interface_info *tptr)
{
	struct interface_info **vec;
	int delta;

	/* If the registerer didn't assign an index, assign one now. */
	if (tptr -> index == -1) {
		tptr -> index = interface_count++;
		while (tptr -> index < interface_max &&
		       interface_vector [tptr -> index])
			tptr -> index = interface_count++;
	}

	if (interface_max <= tptr -> index) {
		delta = tptr -> index - interface_max + 10;
		vec = dmalloc ((interface_max + delta) *
			       sizeof (struct interface_info *), MDL);
		if (!vec)
			return;
		memset (&vec [interface_max], 0,
			(sizeof (struct interface_info *)) * delta);
		interface_max += delta;
		if (interface_vector) {
		    memcpy (vec, interface_vector,
			    (interface_count *
			     sizeof (struct interface_info *)));
		    dfree (interface_vector, MDL);
		}
		interface_vector = vec;
	}
	interface_reference (&interface_vector [tptr -> index], tptr, MDL);
	if (tptr -> index >= interface_count)
		interface_count = tptr -> index + 1;
#if defined (TRACING)
	trace_interface_register (interface_trace, tptr);
#endif
}

void interface_snorf (struct interface_info *tmp, int ir)
{
	tmp -> circuit_id = (u_int8_t *)tmp -> name;
	tmp -> circuit_id_len = strlen (tmp -> name);
	tmp -> remote_id = 0;
	tmp -> remote_id_len = 0;
	tmp -> flags = ir;
	if (interfaces) {
		interface_reference (&tmp -> next,
				     interfaces, MDL);
		interface_dereference (&interfaces, MDL);
	}
	interface_reference (&interfaces, tmp, MDL);
}
