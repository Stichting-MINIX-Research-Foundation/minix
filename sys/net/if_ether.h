/*	$NetBSD: if_ether.h,v 1.61 2012/10/31 10:17:34 msaitoh Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)if_ether.h	8.1 (Berkeley) 6/10/93
 */

#ifndef _NET_IF_ETHER_H_
#define _NET_IF_ETHER_H_

#ifdef _KERNEL
#ifdef _KERNEL_OPT
#include "opt_mbuftrace.h"
#endif
#include <sys/mbuf.h>
#endif

#ifndef _STANDALONE
#include <net/if.h>
#endif

/*
 * Some basic Ethernet constants.
 */
#define	ETHER_ADDR_LEN	6	/* length of an Ethernet address */
#define	ETHER_TYPE_LEN	2	/* length of the Ethernet type field */
#define	ETHER_CRC_LEN	4	/* length of the Ethernet CRC */
#define	ETHER_HDR_LEN	((ETHER_ADDR_LEN * 2) + ETHER_TYPE_LEN)
#define	ETHER_MIN_LEN	64	/* minimum frame length, including CRC */
#define	ETHER_MAX_LEN	1518	/* maximum frame length, including CRC */
#define	ETHER_MAX_LEN_JUMBO 9018 /* maximum jumbo frame len, including CRC */

/*
 * Some Ethernet extensions.
 */
#define	ETHER_VLAN_ENCAP_LEN 4	/* length of 802.1Q VLAN encapsulation */
#define	ETHER_PPPOE_ENCAP_LEN 8	/* length of PPPoE encapsulation */

/*
 * Ethernet address - 6 octets
 * this is only used by the ethers(3) functions.
 */
struct ether_addr {
	uint8_t ether_addr_octet[ETHER_ADDR_LEN];
} __packed;

#if defined(__minix)
#define ea_addr ether_addr_octet
typedef struct ether_addr ether_addr_t;
#endif /* defined(__minix) */

/*
 * Structure of a 10Mb/s Ethernet header.
 */
struct ether_header {
	uint8_t  ether_dhost[ETHER_ADDR_LEN];
	uint8_t  ether_shost[ETHER_ADDR_LEN];
	uint16_t ether_type;
} __packed;

#include <net/ethertypes.h>

#define	ETHER_IS_MULTICAST(addr) (*(addr) & 0x01) /* is address mcast/bcast? */
#define	ETHER_IS_LOCAL(addr) (*(addr) & 0x02) /* is address local? */

#define	ETHERMTU_JUMBO	(ETHER_MAX_LEN_JUMBO - ETHER_HDR_LEN - ETHER_CRC_LEN)
#define	ETHERMTU	(ETHER_MAX_LEN - ETHER_HDR_LEN - ETHER_CRC_LEN)
#define	ETHERMIN	(ETHER_MIN_LEN - ETHER_HDR_LEN - ETHER_CRC_LEN)

/*
 * Compute the maximum frame size based on ethertype (i.e. possible
 * encapsulation) and whether or not an FCS is present.
 */
#define	ETHER_MAX_FRAME(ifp, etype, hasfcs)				\
	((ifp)->if_mtu + ETHER_HDR_LEN +				\
	 ((hasfcs) ? ETHER_CRC_LEN : 0) +				\
	 (((etype) == ETHERTYPE_VLAN) ? ETHER_VLAN_ENCAP_LEN : 0) +	\
	 (((etype) == ETHERTYPE_PPPOE) ? ETHER_PPPOE_ENCAP_LEN : 0))

/*
 * Ethernet CRC32 polynomials (big- and little-endian verions).
 */
#define	ETHER_CRC_POLY_LE	0xedb88320
#define	ETHER_CRC_POLY_BE	0x04c11db6

#ifndef _STANDALONE

/*
 * Ethernet-specific mbuf flags.
 */
#define	M_HASFCS	M_LINK0	/* FCS included at end of frame */
#define	M_PROMISC	M_LINK1	/* this packet is not for us */

#ifdef _KERNEL
/*
 * Macro to map an IP multicast address to an Ethernet multicast address.
 * The high-order 25 bits of the Ethernet address are statically assigned,
 * and the low-order 23 bits are taken from the low end of the IP address.
 */
#define ETHER_MAP_IP_MULTICAST(ipaddr, enaddr)				\
	/* const struct in_addr *ipaddr; */				\
	/* uint8_t enaddr[ETHER_ADDR_LEN]; */				\
do {									\
	(enaddr)[0] = 0x01;						\
	(enaddr)[1] = 0x00;						\
	(enaddr)[2] = 0x5e;						\
	(enaddr)[3] = ((const uint8_t *)ipaddr)[1] & 0x7f;		\
	(enaddr)[4] = ((const uint8_t *)ipaddr)[2];			\
	(enaddr)[5] = ((const uint8_t *)ipaddr)[3];			\
} while (/*CONSTCOND*/0)
/*
 * Macro to map an IP6 multicast address to an Ethernet multicast address.
 * The high-order 16 bits of the Ethernet address are statically assigned,
 * and the low-order 32 bits are taken from the low end of the IP6 address.
 */
#define ETHER_MAP_IPV6_MULTICAST(ip6addr, enaddr)			\
	/* struct in6_addr *ip6addr; */					\
	/* uint8_t enaddr[ETHER_ADDR_LEN]; */				\
{                                                                       \
	(enaddr)[0] = 0x33;						\
	(enaddr)[1] = 0x33;						\
	(enaddr)[2] = ((const uint8_t *)ip6addr)[12];			\
	(enaddr)[3] = ((const uint8_t *)ip6addr)[13];			\
	(enaddr)[4] = ((const uint8_t *)ip6addr)[14];			\
	(enaddr)[5] = ((const uint8_t *)ip6addr)[15];			\
}
#endif

struct mii_data;

struct ethercom;

typedef int (*ether_cb_t)(struct ethercom *);

#if !defined(__minix)
/*
 * Structure shared between the ethernet driver modules and
 * the multicast list code.  For example, each ec_softc or il_softc
 * begins with this structure.
 */
struct ethercom {
	struct	ifnet ec_if;			/* network-visible interface */
	LIST_HEAD(, ether_multi) ec_multiaddrs;	/* list of ether multicast
						   addrs */
	int	ec_multicnt;			/* length of ec_multiaddrs
						   list */
	int	ec_capabilities;		/* capabilities, provided by
						   driver */
	int	ec_capenable;			/* tells hardware which
						   capabilities to enable */

	int	ec_nvlans;			/* # VLANs on this interface */
	/* The device handle for the MII bus child device. */
	struct mii_data				*ec_mii;
	/* Called after a change to ec_if.if_flags.  Returns
	 * ENETRESET if the device should be reinitialized with
	 * ec_if.if_init, 0 on success, not 0 on failure.
	 */
	ether_cb_t				ec_ifflags_cb;
#ifdef MBUFTRACE
	struct	mowner ec_rx_mowner;		/* mbufs received */
	struct	mowner ec_tx_mowner;		/* mbufs transmitted */
#endif
};
#endif /* !defined(__minix) */

#define	ETHERCAP_VLAN_MTU	0x00000001	/* VLAN-compatible MTU */
#define	ETHERCAP_VLAN_HWTAGGING	0x00000002	/* hardware VLAN tag support */
#define	ETHERCAP_JUMBO_MTU	0x00000004	/* 9000 byte MTU supported */

#if !defined(__minix)
#define	ECCAPBITS		\
	"\020"			\
	"\1VLAN_MTU"		\
	"\2VLAN_HWTAGGING"	\
	"\3JUMBO_MTU"

/* ioctl() for Ethernet capabilities */
struct eccapreq {
	char		eccr_name[IFNAMSIZ];	/* if name, e.g. "en0" */
	int		eccr_capabilities;	/* supported capabiliites */
	int		eccr_capenable;		/* capabilities enabled */
};
#endif /* !defined(__minix) */

#ifdef	_KERNEL
extern const uint8_t etherbroadcastaddr[ETHER_ADDR_LEN];
extern const uint8_t ethermulticastaddr_slowprotocols[ETHER_ADDR_LEN];
extern const uint8_t ether_ipmulticast_min[ETHER_ADDR_LEN];
extern const uint8_t ether_ipmulticast_max[ETHER_ADDR_LEN];

void	ether_set_ifflags_cb(struct ethercom *, ether_cb_t);
int	ether_ioctl(struct ifnet *, u_long, void *);
int	ether_addmulti(const struct sockaddr *, struct ethercom *);
int	ether_delmulti(const struct sockaddr *, struct ethercom *);
int	ether_multiaddr(const struct sockaddr *, uint8_t[], uint8_t[]);
void    ether_input(struct ifnet *, struct mbuf *);
#endif /* _KERNEL */

#if !defined(__minix)
/*
 * Ethernet multicast address structure.  There is one of these for each
 * multicast address or range of multicast addresses that we are supposed
 * to listen to on a particular interface.  They are kept in a linked list,
 * rooted in the interface's ethercom structure.
 */
struct ether_multi {
	uint8_t enm_addrlo[ETHER_ADDR_LEN]; /* low  or only address of range */
	uint8_t enm_addrhi[ETHER_ADDR_LEN]; /* high or only address of range */
	u_int	 enm_refcount;		/* no. claims to this addr/range */
	LIST_ENTRY(ether_multi) enm_list;
};
#endif /* !defined(__minix) */

/*
 * Structure used by macros below to remember position when stepping through
 * all of the ether_multi records.
 */
struct ether_multistep {
	struct ether_multi  *e_enm;
};

/*
 * Macro for looking up the ether_multi record for a given range of Ethernet
 * multicast addresses connected to a given ethercom structure.  If no matching
 * record is found, "enm" returns NULL.
 */
#define ETHER_LOOKUP_MULTI(addrlo, addrhi, ec, enm)			\
	/* uint8_t addrlo[ETHER_ADDR_LEN]; */				\
	/* uint8_t addrhi[ETHER_ADDR_LEN]; */				\
	/* struct ethercom *ec; */					\
	/* struct ether_multi *enm; */					\
{									\
	for ((enm) = LIST_FIRST(&(ec)->ec_multiaddrs);			\
	    (enm) != NULL &&						\
	    (memcmp((enm)->enm_addrlo, (addrlo), ETHER_ADDR_LEN) != 0 ||	\
	     memcmp((enm)->enm_addrhi, (addrhi), ETHER_ADDR_LEN) != 0);	\
		(enm) = LIST_NEXT((enm), enm_list));			\
}

/*
 * Macro to step through all of the ether_multi records, one at a time.
 * The current position is remembered in "step", which the caller must
 * provide.  ETHER_FIRST_MULTI(), below, must be called to initialize "step"
 * and get the first record.  Both macros return a NULL "enm" when there
 * are no remaining records.
 */
#define ETHER_NEXT_MULTI(step, enm) \
	/* struct ether_multistep step; */  \
	/* struct ether_multi *enm; */  \
{ \
	if (((enm) = (step).e_enm) != NULL) \
		(step).e_enm = LIST_NEXT((enm), enm_list); \
}

#define ETHER_FIRST_MULTI(step, ec, enm) \
	/* struct ether_multistep step; */ \
	/* struct ethercom *ec; */ \
	/* struct ether_multi *enm; */ \
{ \
	(step).e_enm = LIST_FIRST(&(ec)->ec_multiaddrs); \
	ETHER_NEXT_MULTI((step), (enm)); \
}

#ifdef _KERNEL

/*
 * Ethernet 802.1Q VLAN structures.
 */

/* add VLAN tag to input/received packet */
static inline int vlan_input_tag(struct ifnet *, struct mbuf *, u_int);
static inline int
vlan_input_tag(struct ifnet *ifp, struct mbuf *m, u_int vlanid)
{
	struct m_tag *mtag;
	mtag = m_tag_get(PACKET_TAG_VLAN, sizeof(u_int), M_NOWAIT);
	if (mtag == NULL) {
		ifp->if_ierrors++;
		printf("%s: unable to allocate VLAN tag\n", ifp->if_xname);
		m_freem(m);
		return 1;
	}
	*(u_int *)(mtag + 1) = vlanid;
	m_tag_prepend(m, mtag);
	return 0;
}

#define VLAN_INPUT_TAG(ifp, m, vlanid, _errcase)		\
    if (vlan_input_tag(ifp, m, vlanid) != 0) {	 		\
	_errcase;						\
    }

/* extract VLAN tag from output/trasmit packet */
#define VLAN_OUTPUT_TAG(ec, m0)			\
	(VLAN_ATTACHED(ec) ? m_tag_find((m0), PACKET_TAG_VLAN, NULL) : NULL)

/* extract VLAN ID value from a VLAN tag */
#define VLAN_TAG_VALUE(mtag)	\
	((*(u_int *)(mtag + 1)) & 4095)

/* test if any VLAN is configured for this interface */
#define VLAN_ATTACHED(ec)	((ec)->ec_nvlans > 0)

void	ether_ifattach(struct ifnet *, const uint8_t *);
void	ether_ifdetach(struct ifnet *);
int	ether_mediachange(struct ifnet *);
void	ether_mediastatus(struct ifnet *, struct ifmediareq *);

char	*ether_sprintf(const uint8_t *);
char	*ether_snprintf(char *, size_t, const uint8_t *);

uint32_t ether_crc32_le(const uint8_t *, size_t);
uint32_t ether_crc32_be(const uint8_t *, size_t);

int	ether_aton_r(u_char *, size_t, const char *);
#else
/*
 * Prototype ethers(3) functions.
 */
#include <sys/cdefs.h>
__BEGIN_DECLS
char *	ether_ntoa(const struct ether_addr *);
struct ether_addr *
	ether_aton(const char *);
int	ether_ntohost(char *, const struct ether_addr *);
int	ether_hostton(const char *, struct ether_addr *);
int	ether_line(const char *, struct ether_addr *, char *);
__END_DECLS
#endif

#endif /* _STANDALONE */

#endif /* !_NET_IF_ETHER_H_ */
