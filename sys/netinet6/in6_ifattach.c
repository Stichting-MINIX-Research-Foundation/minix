/*	$NetBSD: in6_ifattach.c,v 1.95 2015/02/23 19:15:59 martin Exp $	*/
/*	$KAME: in6_ifattach.c,v 1.124 2001/07/18 08:32:51 jinmei Exp $	*/

/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: in6_ifattach.c,v 1.95 2015/02/23 19:15:59 martin Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kmem.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/kernel.h>
#include <sys/syslog.h>
#include <sys/md5.h>
#include <sys/socketvar.h>
#include <sys/cprng.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_var.h>

#include <netinet/ip6.h>
#include <netinet6/in6_ifattach.h>
#include <netinet6/ip6_var.h>
#include <netinet6/nd6.h>
#include <netinet6/ip6_mroute.h>
#include <netinet6/scope6_var.h>

#include <net/net_osdep.h>

unsigned long in6_maxmtu = 0;

int ip6_auto_linklocal = 1;	/* enable by default */

callout_t in6_tmpaddrtimer_ch;


#if 0
static int get_hostid_ifid(struct ifnet *, struct in6_addr *);
#endif
static int get_rand_ifid(struct ifnet *, struct in6_addr *);
static int generate_tmp_ifid(u_int8_t *, const u_int8_t *, u_int8_t *);
static int get_ifid(struct ifnet *, struct ifnet *, struct in6_addr *);
static int in6_ifattach_linklocal(struct ifnet *, struct ifnet *);
static int in6_ifattach_loopback(struct ifnet *);

#define EUI64_GBIT	0x01
#define EUI64_UBIT	0x02
#define EUI64_TO_IFID(in6)	do {(in6)->s6_addr[8] ^= EUI64_UBIT; } while (/*CONSTCOND*/ 0)
#define EUI64_GROUP(in6)	((in6)->s6_addr[8] & EUI64_GBIT)
#define EUI64_INDIVIDUAL(in6)	(!EUI64_GROUP(in6))
#define EUI64_LOCAL(in6)	((in6)->s6_addr[8] & EUI64_UBIT)
#define EUI64_UNIVERSAL(in6)	(!EUI64_LOCAL(in6))

#define IFID_LOCAL(in6)		(!EUI64_LOCAL(in6))
#define IFID_UNIVERSAL(in6)	(!EUI64_UNIVERSAL(in6))

#define GEN_TEMPID_RETRY_MAX 5

#if 0
/*
 * Generate a last-resort interface identifier from hostid.
 * works only for certain architectures (like sparc).
 * also, using hostid itself may constitute a privacy threat, much worse
 * than MAC addresses (hostids are used for software licensing).
 * maybe we should use MD5(hostid) instead.
 *
 * in6 - upper 64bits are preserved
 */
static int
get_hostid_ifid(struct ifnet *ifp, struct in6_addr *in6)
{
	int off, len;
	static const uint8_t allzero[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
	static const uint8_t allone[8] =
	    { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

	if (!hostid)
		return -1;

	/* get up to 8 bytes from the hostid field - should we get */
	len = (sizeof(hostid) > 8) ? 8 : sizeof(hostid);
	off = sizeof(*in6) - len;
	memcpy(&in6->s6_addr[off], &hostid, len);

	/* make sure we do not return anything bogus */
	if (memcmp(&in6->s6_addr[8], allzero, sizeof(allzero)))
		return -1;
	if (memcmp(&in6->s6_addr[8], allone, sizeof(allone)))
		return -1;

	/* make sure to set "u" bit to local, and "g" bit to individual. */
	in6->s6_addr[8] &= ~EUI64_GBIT;	/* g bit to "individual" */
	in6->s6_addr[8] |= EUI64_UBIT;	/* u bit to "local" */

	/* convert EUI64 into IPv6 interface identifier */
	EUI64_TO_IFID(in6);

	return 0;
}
#endif

/*
 * Generate a last-resort interface identifier, when the machine has no
 * IEEE802/EUI64 address sources.
 * The goal here is to get an interface identifier that is
 * (1) random enough and (2) does not change across reboot.
 * We currently use MD5(hostname) for it.
 */
static int
get_rand_ifid(struct ifnet *ifp,
	struct in6_addr *in6)	/* upper 64bits are preserved */
{
	MD5_CTX ctxt;
	u_int8_t digest[16];

#if 0
	/* we need at least several letters as seed for ifid */
	if (hostnamelen < 3)
		return -1;
#endif

	/* generate 8 bytes of pseudo-random value. */
	memset(&ctxt, 0, sizeof(ctxt));
	MD5Init(&ctxt);
	MD5Update(&ctxt, (u_char *)hostname, hostnamelen);
	MD5Final(digest, &ctxt);

	/* assumes sizeof(digest) > sizeof(ifid) */
	memcpy(&in6->s6_addr[8], digest, 8);

	/* make sure to set "u" bit to local, and "g" bit to individual. */
	in6->s6_addr[8] &= ~EUI64_GBIT;	/* g bit to "individual" */
	in6->s6_addr[8] |= EUI64_UBIT;	/* u bit to "local" */

	/* convert EUI64 into IPv6 interface identifier */
	EUI64_TO_IFID(in6);

	return 0;
}

static int
generate_tmp_ifid(u_int8_t *seed0, const u_int8_t *seed1, u_int8_t *ret)
{
	MD5_CTX ctxt;
	u_int8_t seed[16], digest[16], nullbuf[8];
	/*
	 * interface ID for subnet anycast addresses.
	 * XXX: we assume the unicast address range that requires IDs
	 * in EUI-64 format.
	 */
	static const uint8_t anycast_id[8] = { 0xfd, 0xff, 0xff, 0xff,
	    0xff, 0xff, 0xff, 0x80 };
	static const uint8_t isatap_id[4] = { 0x00, 0x00, 0x5e, 0xfe };
	int badid, retry = 0;

	/* If there's no hisotry, start with a random seed. */
	memset(nullbuf, 0, sizeof(nullbuf));
	if (memcmp(nullbuf, seed0, sizeof(nullbuf)) == 0) {
		cprng_fast(seed, sizeof(seed));
	} else
		memcpy(seed, seed0, 8);

	/* copy the right-most 64-bits of the given address */
	/* XXX assumption on the size of IFID */
	memcpy(&seed[8], seed1, 8);

  again:
	/* for debugging purposes only */
#if 0
	{
		int i;

		printf("generate_tmp_ifid: new randomized ID from: ");
		for (i = 0; i < 16; i++)
			printf("%02x", seed[i]);
		printf(" ");
	}
#endif

	/* generate 16 bytes of pseudo-random value. */
	memset(&ctxt, 0, sizeof(ctxt));
	MD5Init(&ctxt);
	MD5Update(&ctxt, seed, sizeof(seed));
	MD5Final(digest, &ctxt);

	/*
	 * draft-ietf-ipngwg-temp-addresses-v2-00.txt 3.2.1. (3)
	 * Take the left-most 64-bits of the MD5 digest and set bit 6 (the
	 * left-most bit is numbered 0) to zero.
	 */
	memcpy(ret, digest, 8);
	ret[0] &= ~EUI64_UBIT;

	/*
	 * Reject inappropriate identifiers according to
	 * draft-ietf-ipngwg-temp-addresses-v2-00.txt 3.2.1. (4)
	 * At this moment, we reject following cases:
	 * - all 0 identifier
	 * - identifiers that conflict with reserved subnet anycast addresses,
	 *   which are defined in RFC 2526.
	 * - identifiers that conflict with ISATAP addresses
	 * - identifiers used in our own addresses
	 */
	badid = 0;
	if (memcmp(nullbuf, ret, sizeof(nullbuf)) == 0)
		badid = 1;
	else if (memcmp(anycast_id, ret, 7) == 0 &&
	    (anycast_id[7] & ret[7]) == anycast_id[7]) {
		badid = 1;
	} else if (memcmp(isatap_id, ret, sizeof(isatap_id)) == 0)
		badid = 1;
	else {
		struct in6_ifaddr *ia;

		for (ia = in6_ifaddr; ia; ia = ia->ia_next) {
			if (!memcmp(&ia->ia_addr.sin6_addr.s6_addr[8], 
			    ret, 8)) {
				badid = 1;
				break;
			}
		}
	}

	/*
	 * In the event that an unacceptable identifier has been generated,
	 * restart the process, using the right-most 64 bits of the MD5 digest
	 * obtained in place of the history value.
	 */
	if (badid) {
		/* for debugging purposes only */
#if 0
		{
			int i;

			printf("unacceptable random ID: ");
			for (i = 0; i < 16; i++)
				printf("%02x", digest[i]);
			printf("\n");
		}
#endif

		if (++retry < GEN_TEMPID_RETRY_MAX) {
			memcpy(seed, &digest[8], 8);
			goto again;
		} else {
			/*
			 * We're so unlucky.  Give up for now, and return
			 * all 0 IDs to tell the caller not to make a
			 * temporary address.
			 */
			nd6log((LOG_NOTICE,
			    "generate_tmp_ifid: never found a good ID\n"));
			memset(ret, 0, 8);
		}
	}

	/*
	 * draft-ietf-ipngwg-temp-addresses-v2-00.txt 3.2.1. (6)
	 * Take the rightmost 64-bits of the MD5 digest and save them in
	 * stable storage as the history value to be used in the next
	 * iteration of the algorithm.
	 */
	memcpy(seed0, &digest[8], 8);

	/* for debugging purposes only */
#if 0
	{
		int i;

		printf("to: ");
		for (i = 0; i < 16; i++)
			printf("%02x", digest[i]);
		printf("\n");
	}
#endif

	return 0;
}

/*
 * Get interface identifier for the specified interface.
 *
 * in6 - upper 64bits are preserved
 */
int
in6_get_hw_ifid(struct ifnet *ifp, struct in6_addr *in6)
{
	struct ifaddr *ifa;
	const struct sockaddr_dl *sdl = NULL, *tsdl;
	const char *addr;
	size_t addrlen;
	static u_int8_t allzero[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
	static u_int8_t allone[8] =
		{ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

	IFADDR_FOREACH(ifa, ifp) {
		if (ifa->ifa_addr->sa_family != AF_LINK)
			continue;
		tsdl = satocsdl(ifa->ifa_addr);
		if (tsdl == NULL || tsdl->sdl_alen == 0)
			continue;
		if (sdl == NULL || ifa == ifp->if_dl || ifa == ifp->if_hwdl)
			sdl = tsdl;
		if (ifa == ifp->if_hwdl)
			break;
	}

	if (sdl == NULL)
		return -1;

	addr = CLLADDR(sdl);
	addrlen = sdl->sdl_alen;

	switch (ifp->if_type) {
	case IFT_IEEE1394:
	case IFT_IEEE80211:
		/* IEEE1394 uses 16byte length address starting with EUI64 */
		if (addrlen > 8)
			addrlen = 8;
		break;
	default:
		break;
	}

	/* get EUI64 */
	switch (ifp->if_type) {
	/* IEEE802/EUI64 cases - what others? */
	case IFT_ETHER:
	case IFT_FDDI:
	case IFT_ATM:
	case IFT_IEEE1394:
	case IFT_IEEE80211:
		/* look at IEEE802/EUI64 only */
		if (addrlen != 8 && addrlen != 6)
			return -1;

		/*
		 * check for invalid MAC address - on bsdi, we see it a lot
		 * since wildboar configures all-zero MAC on pccard before
		 * card insertion.
		 */
		if (memcmp(addr, allzero, addrlen) == 0)
			return -1;
		if (memcmp(addr, allone, addrlen) == 0)
			return -1;

		/* make EUI64 address */
		if (addrlen == 8)
			memcpy(&in6->s6_addr[8], addr, 8);
		else if (addrlen == 6) {
			in6->s6_addr[8] = addr[0];
			in6->s6_addr[9] = addr[1];
			in6->s6_addr[10] = addr[2];
			in6->s6_addr[11] = 0xff;
			in6->s6_addr[12] = 0xfe;
			in6->s6_addr[13] = addr[3];
			in6->s6_addr[14] = addr[4];
			in6->s6_addr[15] = addr[5];
		}
		break;

	case IFT_ARCNET:
		if (addrlen != 1)
			return -1;
		if (!addr[0])
			return -1;

		memset(&in6->s6_addr[8], 0, 8);
		in6->s6_addr[15] = addr[0];

		/*
		 * due to insufficient bitwidth, we mark it local.
		 */
		in6->s6_addr[8] &= ~EUI64_GBIT;	/* g bit to "individual" */
		in6->s6_addr[8] |= EUI64_UBIT;	/* u bit to "local" */
		break;

	case IFT_GIF:
#ifdef IFT_STF
	case IFT_STF:
#endif
		/*
		 * RFC2893 says: "SHOULD use IPv4 address as ifid source".
		 * however, IPv4 address is not very suitable as unique
		 * identifier source (can be renumbered).
		 * we don't do this.
		 */
		return -1;

	default:
		return -1;
	}

	/* sanity check: g bit must not indicate "group" */
	if (EUI64_GROUP(in6))
		return -1;

	/* convert EUI64 into IPv6 interface identifier */
	EUI64_TO_IFID(in6);

	/*
	 * sanity check: ifid must not be all zero, avoid conflict with
	 * subnet router anycast
	 */
	if ((in6->s6_addr[8] & ~(EUI64_GBIT | EUI64_UBIT)) == 0x00 &&
	    memcmp(&in6->s6_addr[9], allzero, 7) == 0) {
		return -1;
	}

	return 0;
}

/*
 * Get interface identifier for the specified interface.  If it is not
 * available on ifp0, borrow interface identifier from other information
 * sources.
 *
 * altifp - secondary EUI64 source
 */
static int
get_ifid(struct ifnet *ifp0, struct ifnet *altifp, 
	struct in6_addr *in6)
{
	struct ifnet *ifp;

	/* first, try to get it from the interface itself */
	if (in6_get_hw_ifid(ifp0, in6) == 0) {
		nd6log((LOG_DEBUG, "%s: got interface identifier from itself\n",
		    if_name(ifp0)));
		goto success;
	}

	/* try secondary EUI64 source. this basically is for ATM PVC */
	if (altifp && in6_get_hw_ifid(altifp, in6) == 0) {
		nd6log((LOG_DEBUG, "%s: got interface identifier from %s\n",
		    if_name(ifp0), if_name(altifp)));
		goto success;
	}

	/* next, try to get it from some other hardware interface */
	IFNET_FOREACH(ifp) {
		if (ifp == ifp0)
			continue;
		if (in6_get_hw_ifid(ifp, in6) != 0)
			continue;

		/*
		 * to borrow ifid from other interface, ifid needs to be
		 * globally unique
		 */
		if (IFID_UNIVERSAL(in6)) {
			nd6log((LOG_DEBUG,
			    "%s: borrow interface identifier from %s\n",
			    if_name(ifp0), if_name(ifp)));
			goto success;
		}
	}

#if 0
	/* get from hostid - only for certain architectures */
	if (get_hostid_ifid(ifp, in6) == 0) {
		nd6log((LOG_DEBUG,
		    "%s: interface identifier generated by hostid\n",
		    if_name(ifp0)));
		goto success;
	}
#endif

	/* last resort: get from random number source */
	if (get_rand_ifid(ifp, in6) == 0) {
		nd6log((LOG_DEBUG,
		    "%s: interface identifier generated by random number\n",
		    if_name(ifp0)));
		goto success;
	}

	printf("%s: failed to get interface identifier\n", if_name(ifp0));
	return -1;

success:
	nd6log((LOG_INFO, "%s: ifid: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\n",
	    if_name(ifp0), in6->s6_addr[8], in6->s6_addr[9], in6->s6_addr[10],
	    in6->s6_addr[11], in6->s6_addr[12], in6->s6_addr[13],
	    in6->s6_addr[14], in6->s6_addr[15]));
	return 0;
}

/*
 * altifp - secondary EUI64 source
 */

static int
in6_ifattach_linklocal(struct ifnet *ifp, struct ifnet *altifp)
{
	struct in6_ifaddr *ia __diagused;
	struct in6_aliasreq ifra;
	struct nd_prefixctl prc0;
	int i, error;

	/*
	 * configure link-local address.
	 */
	memset(&ifra, 0, sizeof(ifra));

	/*
	 * in6_update_ifa() does not use ifra_name, but we accurately set it
	 * for safety.
	 */
	strncpy(ifra.ifra_name, if_name(ifp), sizeof(ifra.ifra_name));

	ifra.ifra_addr.sin6_family = AF_INET6;
	ifra.ifra_addr.sin6_len = sizeof(struct sockaddr_in6);
	ifra.ifra_addr.sin6_addr.s6_addr32[0] = htonl(0xfe800000);
	ifra.ifra_addr.sin6_addr.s6_addr32[1] = 0;
	if ((ifp->if_flags & IFF_LOOPBACK) != 0) {
		ifra.ifra_addr.sin6_addr.s6_addr32[2] = 0;
		ifra.ifra_addr.sin6_addr.s6_addr32[3] = htonl(1);
	} else {
		if (get_ifid(ifp, altifp, &ifra.ifra_addr.sin6_addr) != 0) {
			nd6log((LOG_ERR,
			    "%s: no ifid available\n", if_name(ifp)));
			return -1;
		}
	}
	if (in6_setscope(&ifra.ifra_addr.sin6_addr, ifp, NULL))
		return -1;

	sockaddr_in6_init(&ifra.ifra_prefixmask, &in6mask64, 0, 0, 0);
	/* link-local addresses should NEVER expire. */
	ifra.ifra_lifetime.ia6t_vltime = ND6_INFINITE_LIFETIME;
	ifra.ifra_lifetime.ia6t_pltime = ND6_INFINITE_LIFETIME;

	/*
	 * Now call in6_update_ifa() to do a bunch of procedures to configure
	 * a link-local address. We can set the 3rd argument to NULL, because
	 * we know there's no other link-local address on the interface
	 * and therefore we are adding one (instead of updating one).
	 */
	if ((error = in6_update_ifa(ifp, &ifra, NULL,
	    IN6_IFAUPDATE_DADDELAY)) != 0) {
		/*
		 * XXX: When the interface does not support IPv6, this call
		 * would fail in the SIOCINITIFADDR ioctl.  I believe the
		 * notification is rather confusing in this case, so just
		 * suppress it.  (jinmei@kame.net 20010130)
		 */
		if (error != EAFNOSUPPORT)
			nd6log((LOG_NOTICE, "in6_ifattach_linklocal: failed to "
			    "configure a link-local address on %s "
			    "(errno=%d)\n",
			    if_name(ifp), error));
		return -1;
	}

	ia = in6ifa_ifpforlinklocal(ifp, 0); /* ia must not be NULL */
	KASSERTMSG(ia, "ia == NULL in in6_ifattach_linklocal");

	/*
	 * Make the link-local prefix (fe80::/64%link) as on-link.
	 * Since we'd like to manage prefixes separately from addresses,
	 * we make an ND6 prefix structure for the link-local prefix,
	 * and add it to the prefix list as a never-expire prefix.
	 * XXX: this change might affect some existing code base...
	 */
	memset(&prc0, 0, sizeof(prc0));
	prc0.ndprc_ifp = ifp;
	/* this should be 64 at this moment. */
	prc0.ndprc_plen = in6_mask2len(&ifra.ifra_prefixmask.sin6_addr, NULL);
	prc0.ndprc_prefix = ifra.ifra_addr;
	/* apply the mask for safety. (nd6_prelist_add will apply it again) */
	for (i = 0; i < 4; i++) {
		prc0.ndprc_prefix.sin6_addr.s6_addr32[i] &=
		    in6mask64.s6_addr32[i];
	}
	/*
	 * Initialize parameters.  The link-local prefix must always be
	 * on-link, and its lifetimes never expire.
	 */
	prc0.ndprc_raf_onlink = 1;
	prc0.ndprc_raf_auto = 1;	/* probably meaningless */
	prc0.ndprc_vltime = ND6_INFINITE_LIFETIME;
	prc0.ndprc_pltime = ND6_INFINITE_LIFETIME;
	/*
	 * Since there is no other link-local addresses, nd6_prefix_lookup()
	 * probably returns NULL.  However, we cannot always expect the result.
	 * For example, if we first remove the (only) existing link-local
	 * address, and then reconfigure another one, the prefix is still
	 * valid with referring to the old link-local address.
	 */
	if (nd6_prefix_lookup(&prc0) == NULL) {
		if ((error = nd6_prelist_add(&prc0, NULL, NULL)) != 0)
			return error;
	}

	return 0;
}

/*
 * ifp - mut be IFT_LOOP
 */

static int
in6_ifattach_loopback(struct ifnet *ifp)
{
	struct in6_aliasreq ifra;
	int error;

	memset(&ifra, 0, sizeof(ifra));

	/*
	 * in6_update_ifa() does not use ifra_name, but we accurately set it
	 * for safety.
	 */
	strncpy(ifra.ifra_name, if_name(ifp), sizeof(ifra.ifra_name));

	sockaddr_in6_init(&ifra.ifra_prefixmask, &in6mask128, 0, 0, 0);

	/*
	 * Always initialize ia_dstaddr (= broadcast address) to loopback
	 * address.  Follows IPv4 practice - see in_ifinit().
	 */
	sockaddr_in6_init(&ifra.ifra_dstaddr, &in6addr_loopback, 0, 0, 0);

	sockaddr_in6_init(&ifra.ifra_addr, &in6addr_loopback, 0, 0, 0);

	/* the loopback  address should NEVER expire. */
	ifra.ifra_lifetime.ia6t_vltime = ND6_INFINITE_LIFETIME;
	ifra.ifra_lifetime.ia6t_pltime = ND6_INFINITE_LIFETIME;

	/* we don't need to perform DAD on loopback interfaces. */
	ifra.ifra_flags |= IN6_IFF_NODAD;

	/*
	 * We are sure that this is a newly assigned address, so we can set
	 * NULL to the 3rd arg.
	 */
	if ((error = in6_update_ifa(ifp, &ifra, NULL, 0)) != 0) {
		nd6log((LOG_ERR, "in6_ifattach_loopback: failed to configure "
		    "the loopback address on %s (errno=%d)\n",
		    if_name(ifp), error));
		return -1;
	}

	return 0;
}

/*
 * compute NI group address, based on the current hostname setting.
 * see draft-ietf-ipngwg-icmp-name-lookup-* (04 and later).
 *
 * when ifp == NULL, the caller is responsible for filling scopeid.
 */
int
in6_nigroup(struct ifnet *ifp, const char *name, int namelen, 
	struct sockaddr_in6 *sa6)
{
	const char *p;
	u_int8_t *q;
	MD5_CTX ctxt;
	u_int8_t digest[16];
	u_int8_t l;
	u_int8_t n[64];	/* a single label must not exceed 63 chars */

	if (!namelen || !name)
		return -1;

	p = name;
	while (p && *p && *p != '.' && p - name < namelen)
		p++;
	if (p - name > sizeof(n) - 1)
		return -1;	/* label too long */
	l = p - name;
	strncpy((char *)n, name, l);
	n[(int)l] = '\0';
	for (q = n; *q; q++) {
		if ('A' <= *q && *q <= 'Z')
			*q = *q - 'A' + 'a';
	}

	/* generate 8 bytes of pseudo-random value. */
	memset(&ctxt, 0, sizeof(ctxt));
	MD5Init(&ctxt);
	MD5Update(&ctxt, &l, sizeof(l));
	MD5Update(&ctxt, n, l);
	MD5Final(digest, &ctxt);

	memset(sa6, 0, sizeof(*sa6));
	sa6->sin6_family = AF_INET6;
	sa6->sin6_len = sizeof(*sa6);
	sa6->sin6_addr.s6_addr16[0] = htons(0xff02);
	sa6->sin6_addr.s6_addr8[11] = 2;
	memcpy(&sa6->sin6_addr.s6_addr32[3], digest,
	    sizeof(sa6->sin6_addr.s6_addr32[3]));
	if (in6_setscope(&sa6->sin6_addr, ifp, NULL))
		return -1; /* XXX: should not fail */

	return 0;
}

/*
 * XXX multiple loopback interface needs more care.  for instance,
 * nodelocal address needs to be configured onto only one of them.
 * XXX multiple link-local address case
 *
 * altifp - secondary EUI64 source
 */
void
in6_ifattach(struct ifnet *ifp, struct ifnet *altifp)
{
	struct in6_ifaddr *ia;
	struct in6_addr in6;

	/* some of the interfaces are inherently not IPv6 capable */
	switch (ifp->if_type) {
	case IFT_BRIDGE:
#ifdef IFT_PFLOG
	case IFT_PFLOG:
#endif
#ifdef IFT_PFSYNC
	case IFT_PFSYNC:
#endif
		ND_IFINFO(ifp)->flags &= ~ND6_IFF_AUTO_LINKLOCAL;
		ND_IFINFO(ifp)->flags |= ND6_IFF_IFDISABLED;
		return;
	}

	/*
	 * if link mtu is too small, don't try to configure IPv6.
	 * remember there could be some link-layer that has special
	 * fragmentation logic.
	 */
	if (ifp->if_mtu < IPV6_MMTU) {
		nd6log((LOG_INFO, "in6_ifattach: "
		    "%s has too small MTU, IPv6 not enabled\n",
		    if_name(ifp)));
		return;
	}

	/* create a multicast kludge storage (if we have not had one) */
	in6_createmkludge(ifp);

	/*
	 * quirks based on interface type
	 */
	switch (ifp->if_type) {
#ifdef IFT_STF
	case IFT_STF:
		/*
		 * 6to4 interface is a very special kind of beast.
		 * no multicast, no linklocal.  RFC2529 specifies how to make
		 * linklocals for 6to4 interface, but there's no use and
		 * it is rather harmful to have one.
		 */
		ND_IFINFO(ifp)->flags &= ~ND6_IFF_AUTO_LINKLOCAL;
		return;
#endif
	case IFT_CARP:
		return;
	default:
		break;
	}

	/*
	 * usually, we require multicast capability to the interface
	 */
	if ((ifp->if_flags & IFF_MULTICAST) == 0) {
		nd6log((LOG_INFO, "in6_ifattach: "
		    "%s is not multicast capable, IPv6 not enabled\n",
		    if_name(ifp)));
		return;
	}

	/*
	 * assign loopback address for loopback interface.
	 * XXX multiple loopback interface case.
	 */
	if ((ifp->if_flags & IFF_LOOPBACK) != 0) {
		in6 = in6addr_loopback;
		if (in6ifa_ifpwithaddr(ifp, &in6) == NULL) {
			if (in6_ifattach_loopback(ifp) != 0)
				return;
		}
	}

	/*
	 * assign a link-local address, if there's none.
	 */
	if (!(ND_IFINFO(ifp)->flags & ND6_IFF_IFDISABLED) &&
	    ND_IFINFO(ifp)->flags & ND6_IFF_AUTO_LINKLOCAL)
	{
		ia = in6ifa_ifpforlinklocal(ifp, 0);
		if (ia == NULL && in6_ifattach_linklocal(ifp, altifp) != 0) {
			printf("%s: cannot assign link-local address\n",
			    ifp->if_xname);
		}
	}
}

/*
 * NOTE: in6_ifdetach() does not support loopback if at this moment.
 * We don't need this function in bsdi, because interfaces are never removed
 * from the ifnet list in bsdi.
 */
void
in6_ifdetach(struct ifnet *ifp)
{
	struct in6_ifaddr *ia, *oia;
	struct ifaddr *ifa, *next;
	struct rtentry *rt;
	short rtflags;
	struct in6_multi_mship *imm;

	/* remove ip6_mrouter stuff */
	ip6_mrouter_detach(ifp);

	/* remove neighbor management table */
	nd6_purge(ifp, NULL);

	/* XXX this code is duplicated in in6_purgeif() --dyoung */
	/* nuke any of IPv6 addresses we have */
	if_purgeaddrs(ifp, AF_INET6, in6_purgeaddr);

	/* XXX isn't this code is redundant, given the above? --dyoung */
	/* XXX doesn't this code replicate code in in6_purgeaddr() ? --dyoung */
	/* undo everything done by in6_ifattach(), just in case */
	for (ifa = IFADDR_FIRST(ifp); ifa != NULL; ifa = next) {
		next = IFADDR_NEXT(ifa);

		if (ifa->ifa_addr->sa_family != AF_INET6
		 || !IN6_IS_ADDR_LINKLOCAL(&satosin6(&ifa->ifa_addr)->sin6_addr)) {
			continue;
		}

		ia = (struct in6_ifaddr *)ifa;

		/*
		 * leave from multicast groups we have joined for the interface
		 */
		while ((imm = LIST_FIRST(&ia->ia6_memberships)) != NULL) {
			LIST_REMOVE(imm, i6mm_chain);
			in6_leavegroup(imm);
		}

		/* remove from the routing table */
		if ((ia->ia_flags & IFA_ROUTE) &&
		    (rt = rtalloc1((struct sockaddr *)&ia->ia_addr, 0))) {
			rtflags = rt->rt_flags;
			rtfree(rt);
			rtrequest(RTM_DELETE, (struct sockaddr *)&ia->ia_addr,
			    (struct sockaddr *)&ia->ia_addr,
			    (struct sockaddr *)&ia->ia_prefixmask,
			    rtflags, NULL);
		}

		/* remove from the linked list */
		ifa_remove(ifp, &ia->ia_ifa);

		/* also remove from the IPv6 address chain(itojun&jinmei) */
		oia = ia;
		if (oia == (ia = in6_ifaddr))
			in6_ifaddr = ia->ia_next;
		else {
			while (ia->ia_next && (ia->ia_next != oia))
				ia = ia->ia_next;
			if (ia->ia_next)
				ia->ia_next = oia->ia_next;
			else {
				nd6log((LOG_ERR,
				    "%s: didn't unlink in6ifaddr from list\n",
				    if_name(ifp)));
			}
		}

		ifafree(&oia->ia_ifa);
	}

	/* cleanup multicast address kludge table, if there is any */
	in6_purgemkludge(ifp);

	/*
	 * remove neighbor management table.  we call it twice just to make
	 * sure we nuke everything.  maybe we need just one call.
	 * XXX: since the first call did not release addresses, some prefixes
	 * might remain.  We should call nd6_purge() again to release the
	 * prefixes after removing all addresses above.
	 * (Or can we just delay calling nd6_purge until at this point?)
	 */
	nd6_purge(ifp, NULL);
}

int
in6_get_tmpifid(struct ifnet *ifp, u_int8_t *retbuf, 
	const u_int8_t *baseid, int generate)
{
	u_int8_t nullbuf[8];
	struct nd_ifinfo *ndi = ND_IFINFO(ifp);

	memset(nullbuf, 0, sizeof(nullbuf));
	if (memcmp(ndi->randomid, nullbuf, sizeof(nullbuf)) == 0) {
		/* we've never created a random ID.  Create a new one. */
		generate = 1;
	}

	if (generate) {
		memcpy(ndi->randomseed1, baseid, sizeof(ndi->randomseed1));

		/* generate_tmp_ifid will update seedn and buf */
		(void)generate_tmp_ifid(ndi->randomseed0, ndi->randomseed1,
		    ndi->randomid);
	}
	memcpy(retbuf, ndi->randomid, 8);
	if (generate && memcmp(retbuf, nullbuf, sizeof(nullbuf)) == 0) {
		/* generate_tmp_ifid could not found a good ID. */
		return -1;
	}

	return 0;
}

void
in6_tmpaddrtimer(void *ignored_arg)
{
	struct nd_ifinfo *ndi;
	u_int8_t nullbuf[8];
	struct ifnet *ifp;

	mutex_enter(softnet_lock);
	KERNEL_LOCK(1, NULL);

	callout_reset(&in6_tmpaddrtimer_ch,
	    (ip6_temp_preferred_lifetime - ip6_desync_factor -
	    ip6_temp_regen_advance) * hz, in6_tmpaddrtimer, NULL);

	memset(nullbuf, 0, sizeof(nullbuf));
	IFNET_FOREACH(ifp) {
		ndi = ND_IFINFO(ifp);
		if (memcmp(ndi->randomid, nullbuf, sizeof(nullbuf)) != 0) {
			/*
			 * We've been generating a random ID on this interface.
			 * Create a new one.
			 */
			(void)generate_tmp_ifid(ndi->randomseed0,
			    ndi->randomseed1, ndi->randomid);
		}
	}

	KERNEL_UNLOCK_ONE(NULL);
	mutex_exit(softnet_lock);
}
