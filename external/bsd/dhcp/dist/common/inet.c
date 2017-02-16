/*	$NetBSD: inet.c,v 1.1.1.2 2014/07/12 11:57:44 spz Exp $	*/
/* inet.c

   Subroutines to manipulate internet addresses and ports in a safely portable
   way... */

/*
 * Copyright (c) 2011,2013,2014 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 2007-2009 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 2004,2005 by Internet Systems Consortium, Inc. ("ISC")
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
__RCSID("$NetBSD: inet.c,v 1.1.1.2 2014/07/12 11:57:44 spz Exp $");

#include "dhcpd.h"

/* Return just the network number of an internet address... */

struct iaddr subnet_number (addr, mask)
	struct iaddr addr;
	struct iaddr mask;
{
	int i;
	struct iaddr rv;

	if (addr.len > sizeof(addr.iabuf))
		log_fatal("subnet_number():%s:%d: Invalid addr length.", MDL);
	if (addr.len != mask.len)
		log_fatal("subnet_number():%s:%d: Addr/mask length mismatch.",
			  MDL);

	rv.len = 0;

	/* Both addresses must have the same length... */
	if (addr.len != mask.len)
		return rv;

	rv.len = addr.len;
	for (i = 0; i < rv.len; i++)
		rv.iabuf [i] = addr.iabuf [i] & mask.iabuf [i];
	return rv;
}

/* Combine a network number and a integer to produce an internet address.
   This won't work for subnets with more than 32 bits of host address, but
   maybe this isn't a problem. */

struct iaddr ip_addr (subnet, mask, host_address)
	struct iaddr subnet;
	struct iaddr mask;
	u_int32_t host_address;
{
	int i, j, k;
	u_int32_t swaddr;
	struct iaddr rv;
	unsigned char habuf [sizeof swaddr];

	if (subnet.len > sizeof(subnet.iabuf))
		log_fatal("ip_addr():%s:%d: Invalid addr length.", MDL);
	if (subnet.len != mask.len)
		log_fatal("ip_addr():%s:%d: Addr/mask length mismatch.",
			  MDL);

	swaddr = htonl (host_address);
	memcpy (habuf, &swaddr, sizeof swaddr);

	/* Combine the subnet address and the host address.   If
	   the host address is bigger than can fit in the subnet,
	   return a zero-length iaddr structure. */
	rv = subnet;
	j = rv.len - sizeof habuf;
	for (i = sizeof habuf - 1; i >= 0; i--) {
		if (mask.iabuf [i + j]) {
			if (habuf [i] > (mask.iabuf [i + j] ^ 0xFF)) {
				rv.len = 0;
				return rv;
			}
			for (k = i - 1; k >= 0; k--) {
				if (habuf [k]) {
					rv.len = 0;
					return rv;
				}
			}
			rv.iabuf [i + j] |= habuf [i];
			break;
		} else
			rv.iabuf [i + j] = habuf [i];
	}
		
	return rv;
}

/* Given a subnet number and netmask, return the address on that subnet
   for which the host portion of the address is all ones (the standard
   broadcast address). */

struct iaddr broadcast_addr (subnet, mask)
	struct iaddr subnet;
	struct iaddr mask;
{
	int i;
	struct iaddr rv;

	if (subnet.len > sizeof(subnet.iabuf))
		log_fatal("broadcast_addr():%s:%d: Invalid addr length.", MDL);
	if (subnet.len != mask.len)
		log_fatal("broadcast_addr():%s:%d: Addr/mask length mismatch.",
			  MDL);

	if (subnet.len != mask.len) {
		rv.len = 0;
		return rv;
	}

	for (i = 0; i < subnet.len; i++) {
		rv.iabuf [i] = subnet.iabuf [i] | (~mask.iabuf [i] & 255);
	}
	rv.len = subnet.len;

	return rv;
}

u_int32_t host_addr (addr, mask)
	struct iaddr addr;
	struct iaddr mask;
{
	int i;
	u_int32_t swaddr;
	struct iaddr rv;

	if (addr.len > sizeof(addr.iabuf))
		log_fatal("host_addr():%s:%d: Invalid addr length.", MDL);
	if (addr.len != mask.len)
		log_fatal("host_addr():%s:%d: Addr/mask length mismatch.",
			  MDL);

	rv.len = 0;

	/* Mask out the network bits... */
	rv.len = addr.len;
	for (i = 0; i < rv.len; i++)
		rv.iabuf [i] = addr.iabuf [i] & ~mask.iabuf [i];

	/* Copy out up to 32 bits... */
	memcpy (&swaddr, &rv.iabuf [rv.len - sizeof swaddr], sizeof swaddr);

	/* Swap it and return it. */
	return ntohl (swaddr);
}

int addr_eq (addr1, addr2)
	struct iaddr addr1, addr2;
{
	if (addr1.len > sizeof(addr1.iabuf))
		log_fatal("addr_eq():%s:%d: Invalid addr length.", MDL);

	if (addr1.len != addr2.len)
		return 0;
	return memcmp (addr1.iabuf, addr2.iabuf, addr1.len) == 0;
}

/* addr_match 
 *
 * compares an IP address against a network/mask combination
 * by ANDing the IP with the mask and seeing whether the result
 * matches the masked network value.
 */
int
addr_match(addr, match)
	struct iaddr *addr;
	struct iaddrmatch *match;
{
        int i;

	if (addr->len != match->addr.len)
		return 0;
	
	for (i = 0 ; i < addr->len ; i++) {
		if ((addr->iabuf[i] & match->mask.iabuf[i]) !=
							match->addr.iabuf[i])
			return 0;
	}
	return 1;
}

/* 
 * Compares the addresses a1 and a2.
 *
 * If a1 < a2, returns -1.
 * If a1 == a2, returns 0.
 * If a1 > a2, returns 1.
 *
 * WARNING: if a1 and a2 differ in length, returns 0.
 */
int
addr_cmp(const struct iaddr *a1, const struct iaddr *a2) {
	int i;

	if (a1->len != a2->len) {
		return 0;
	}

	for (i=0; i<a1->len; i++) {
		if (a1->iabuf[i] < a2->iabuf[i]) {
			return -1;
		}
		if (a1->iabuf[i] > a2->iabuf[i]) {
			return 1;
		}
	}

	return 0;
}

/*
 * Performs a bitwise-OR of two addresses.
 *
 * Returns 1 if the result is non-zero, or 0 otherwise.
 *
 * WARNING: if a1 and a2 differ in length, returns 0.
 */
int 
addr_or(struct iaddr *result, const struct iaddr *a1, const struct iaddr *a2) {
	int i;
	int all_zero;

	if (a1->len != a2->len) {
		return 0;
	}

	all_zero = 1;

	result->len = a1->len;
	for (i=0; i<a1->len; i++) {
		result->iabuf[i] = a1->iabuf[i] | a2->iabuf[i];
		if (result->iabuf[i] != 0) {
			all_zero = 0;
		}
	}

	return !all_zero;
}

/*
 * Performs a bitwise-AND of two addresses.
 *
 * Returns 1 if the result is non-zero, or 0 otherwise.
 *
 * WARNING: if a1 and a2 differ in length, returns 0.
 */
int 
addr_and(struct iaddr *result, const struct iaddr *a1, const struct iaddr *a2) {
	int i;
	int all_zero;

	if (a1->len != a2->len) {
		return 0;
	}

	all_zero = 1;

	result->len = a1->len;
	for (i=0; i<a1->len; i++) {
		result->iabuf[i] = a1->iabuf[i] & a2->iabuf[i];
		if (result->iabuf[i] != 0) {
			all_zero = 0;
		}
	}

	return !all_zero;
}

/*
 * Check if a bitmask of the given length is valid for the address.
 * This is not the case if any bits longer than the bitmask are 1.
 *
 * So, this is valid:
 *
 * 127.0.0.0/8
 *
 * But this is not:
 *
 * 127.0.0.1/8
 *
 * Because the final ".1" would get masked out by the /8.
 */
isc_boolean_t
is_cidr_mask_valid(const struct iaddr *addr, int bits) {
	int zero_bits;
	int zero_bytes;
	int i;
	char byte;
	int shift_bits;

	/*
	 * Check our bit boundaries.
	 */
	if (bits < 0) {
		return ISC_FALSE;
	}
	if (bits > (addr->len * 8)) {
		return ISC_FALSE;
	}

	/*
	 * Figure out how many low-order bits need to be zero.
	 */
	zero_bits = (addr->len * 8) - bits;
	zero_bytes = zero_bits / 8;

	/* 
	 * Check to make sure the low-order bytes are zero.
	 */
	for (i=1; i<=zero_bytes; i++) {
		if (addr->iabuf[addr->len-i] != 0) {
			return ISC_FALSE;
		}
	}

	/* 
	 * Look to see if any bits not in right-hand bytes are 
	 * non-zero, by making a byte that has these bits set to zero 
	 * comparing to the original byte. If these two values are 
	 * equal, then the right-hand bits are zero, and we are 
	 * happy.
	 */
	shift_bits = zero_bits % 8;
	if (shift_bits == 0) return ISC_TRUE;
	byte = addr->iabuf[addr->len-zero_bytes-1];
	return (((byte >> shift_bits) << shift_bits) == byte);
}

/*
 * range2cidr
 *
 * Converts a range of IP addresses to a set of CIDR networks.
 *
 * Examples: 
 *  192.168.0.0 - 192.168.0.255 = 192.168.0.0/24
 *  10.0.0.0 - 10.0.1.127 = 10.0.0.0/24, 10.0.1.0/25
 *  255.255.255.32 - 255.255.255.255 = 255.255.255.32/27, 255.255.255.64/26,
 *  				       255.255.255.128/25
 */
isc_result_t 
range2cidr(struct iaddrcidrnetlist **result, 
	   const struct iaddr *lo, const struct iaddr *hi) {
	struct iaddr addr;
	struct iaddr mask;
	int bit;
	struct iaddr end_addr;
	struct iaddr dummy;
	int ofs, val;
	struct iaddrcidrnetlist *net;
	int tmp;

	if (result == NULL) {
		return DHCP_R_INVALIDARG;
	}
	if (*result != NULL) {
		return DHCP_R_INVALIDARG;
	}
	if ((lo == NULL) || (hi == NULL) || (lo->len != hi->len)) {
		return DHCP_R_INVALIDARG;
	}

	/*
	 * Put our start and end in the right order, if reversed.
	 */
	if (addr_cmp(lo, hi) > 0) {
		const struct iaddr *tmp;
		tmp = lo;
		lo = hi;
		hi = tmp;
	}

	/*
	 * Theory of operation:
	 *
	 * -------------------
	 * Start at the low end, and keep trying larger networks
	 * until we get one that is too big (explained below).
	 *
	 * We keep a "mask", which is the ones-complement of a 
	 * normal netmask. So, a /23 has a netmask of 255.255.254.0,
	 * and a mask of 0.0.1.255.
	 *
	 * We know when a network is too big when we bitwise-AND the 
	 * mask with the starting address and we get a non-zero 
	 * result, like this:
	 *
	 *    addr: 192.168.1.0, mask: 0.0.1.255
	 *    bitwise-AND: 0.0.1.0
	 * 
	 * A network is also too big if the bitwise-OR of the mask
	 * with the starting address is larger than the end address,
	 * like this:
	 *
	 *    start: 192.168.1.0, mask: 0.0.1.255, end: 192.168.0.255
	 *    bitwise-OR: 192.168.1.255 
	 *
	 * -------------------
	 * Once we have found a network that is too big, we add the 
	 * appropriate CIDR network to our list of found networks.
	 *
	 * We then use the next IP address as our low address, and
	 * begin the process of searching for a network that is 
	 * too big again, starting with an empty mask.
	 */
	addr = *lo;
	bit = 0;
	memset(&mask, 0, sizeof(mask));
	mask.len = addr.len;
	while (addr_cmp(&addr, hi) <= 0) {
		/*
		 * Bitwise-OR mask with (1 << bit)
		 */
		ofs = addr.len - (bit / 8) - 1;
		val = 1 << (bit % 8);
		if (ofs >= 0) {
			mask.iabuf[ofs] |= val;
		}

		/* 
		 * See if we're too big, and save this network if so.
		 */
		addr_or(&end_addr, &addr, &mask);
		if ((ofs < 0) ||
		    (addr_cmp(&end_addr, hi) > 0) || 
		    addr_and(&dummy, &addr, &mask)) {
		    	/*
			 * Add a new prefix to our list.
			 */
			net = dmalloc(sizeof(*net), MDL);
			if (net == NULL) {
				while (*result != NULL) {
					net = (*result)->next;
					dfree(*result, MDL);
					*result = net;
				}
				return ISC_R_NOMEMORY;
			}
			net->cidrnet.lo_addr = addr;
			net->cidrnet.bits = (addr.len * 8) - bit;
			net->next = *result;
			*result = net;

		    	/* 
			 * Figure out our new starting address, 
			 * by adding (1 << bit) to our previous
			 * starting address.
			 */
			tmp = addr.iabuf[ofs] + val;
			while ((ofs >= 0) && (tmp > 255)) {
				addr.iabuf[ofs] = tmp - 256;
				ofs--;
				tmp = addr.iabuf[ofs] + 1;
			}
			if (ofs < 0) {
				/* Gone past last address, we're done. */
				break;
			}
			addr.iabuf[ofs] = tmp;

			/*
			 * Reset our bit and mask.
			 */
		    	bit = 0;
			memset(mask.iabuf, 0, sizeof(mask.iabuf));
			memset(end_addr.iabuf, 0, sizeof(end_addr.iabuf));
		} else {
			/*
			 * If we're not too big, increase our network size.
			 */
			bit++;
		}
	}

	/*
	 * We're done.
	 */
	return ISC_R_SUCCESS;
}

/*
 * Free a list of CIDR networks, such as returned from range2cidr().
 */
isc_result_t
free_iaddrcidrnetlist(struct iaddrcidrnetlist **result) {
	struct iaddrcidrnetlist *p;

	if (result == NULL) {
		return DHCP_R_INVALIDARG;
	}
	if (*result == NULL) {
		return DHCP_R_INVALIDARG;
	}

	while (*result != NULL) {
		p = *result;
		*result = p->next;
		dfree(p, MDL);
	}

	return ISC_R_SUCCESS;
}

/* piaddr() turns an iaddr structure into a printable address. */
/* XXX: should use a const pointer rather than passing the structure */
const char *
piaddr(const struct iaddr addr) {
	static char
		pbuf[sizeof("ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255")];
			 /* "255.255.255.255" */

	/* INSIST((addr.len == 0) || (addr.len == 4) || (addr.len == 16)); */

	if (addr.len == 0) {
		return "<null address>";
	}
	if (addr.len == 4) {
		return inet_ntop(AF_INET, addr.iabuf, pbuf, sizeof(pbuf));
	} 
	if (addr.len == 16) {
		return inet_ntop(AF_INET6, addr.iabuf, pbuf, sizeof(pbuf));
	}

	log_fatal("piaddr():%s:%d: Invalid address length %d.", MDL,
		  addr.len);
	/* quell compiler warnings */
	return NULL;
}

/* piaddrmask takes an iaddr structure mask, determines the bitlength of
 * the mask, and then returns the printable CIDR notation of the two.
 */
char *
piaddrmask(struct iaddr *addr, struct iaddr *mask) {
	int mw;
	unsigned int oct, bit;

	if ((addr->len != 4) && (addr->len != 16))
		log_fatal("piaddrmask():%s:%d: Address length %d invalid",
			  MDL, addr->len);
	if (addr->len != mask->len)
		log_fatal("piaddrmask():%s:%d: Address and mask size mismatch",
			  MDL);

	/* Determine netmask width in bits. */
	for (mw = (mask->len * 8) ; mw > 0 ; ) {
		oct = (mw - 1) / 8;
		bit = 0x80 >> ((mw - 1) % 8);
		if (!mask->iabuf[oct])
			mw -= 8;
		else if (mask->iabuf[oct] & bit)
			break;
		else
			mw--;
	}

	if (mw < 0)
		log_fatal("Impossible condition at %s:%d.", MDL);

	return piaddrcidr(addr, mw);
}

/* Format an address and mask-length into printable CIDR notation. */
char *
piaddrcidr(const struct iaddr *addr, unsigned int bits) {
	static char
	    ret[sizeof("ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255/128")];
		    /* "255.255.255.255/32" */

	/* INSIST(addr != NULL); */
	/* INSIST((addr->len == 4) || (addr->len == 16)); */
	/* INSIST(bits <= (addr->len * 8)); */

	if (bits > (addr->len * 8))
		return NULL;

	sprintf(ret, "%s/%d", piaddr(*addr), bits);

	return ret;
}

/* Validate that the string represents a valid port number and
 * return it in network byte order
 */

u_int16_t
validate_port(char *port) {
	long local_port = 0;
	long lower = 1;
	long upper = 65535;
	char *endptr;

	errno = 0;
	local_port = strtol(port, &endptr, 10);
	
	if ((*endptr != '\0') || (errno == ERANGE) || (errno == EINVAL))
		log_fatal ("Invalid port number specification: %s", port);

	if (local_port < lower || local_port > upper)
		log_fatal("Port number specified is out of range (%ld-%ld).",
			  lower, upper);

	return htons((u_int16_t)local_port);
}
