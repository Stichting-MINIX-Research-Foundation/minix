/*	$NetBSD: packet.c,v 1.1.1.4 2014/07/12 11:57:46 spz Exp $	*/
/* packet.c

   Packet assembly code, originally contributed by Archie Cobbs. */

/*
 * Copyright (c) 2009,2012 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 2004,2005,2007 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1996-2003 by Internet Software Consortium
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
 * This code was originally contributed by Archie Cobbs, and is still
 * very similar to that contribution, although the packet checksum code
 * has been hacked significantly with the help of quite a few ISC DHCP
 * users, without whose gracious and thorough help the checksum code would
 * still be disabled.
 */

#include <sys/cdefs.h>
__RCSID("$NetBSD: packet.c,v 1.1.1.4 2014/07/12 11:57:46 spz Exp $");

#include "dhcpd.h"

#if defined (PACKET_ASSEMBLY) || defined (PACKET_DECODING)
#include "includes/netinet/ip.h"
#include "includes/netinet/udp.h"
#include "includes/netinet/if_ether.h"
#endif /* PACKET_ASSEMBLY || PACKET_DECODING */

/* Compute the easy part of the checksum on a range of bytes. */

u_int32_t checksum (buf, nbytes, sum)
	unsigned char *buf;
	unsigned nbytes;
	u_int32_t sum;
{
	unsigned i;

#ifdef DEBUG_CHECKSUM
	log_debug ("checksum (%x %d %x)", buf, nbytes, sum);
#endif

	/* Checksum all the pairs of bytes first... */
	for (i = 0; i < (nbytes & ~1U); i += 2) {
#ifdef DEBUG_CHECKSUM_VERBOSE
		log_debug ("sum = %x", sum);
#endif
		sum += (u_int16_t) ntohs(*((u_int16_t *)(buf + i)));
		/* Add carry. */
		if (sum > 0xFFFF)
			sum -= 0xFFFF;
	}	

	/* If there's a single byte left over, checksum it, too.   Network
	   byte order is big-endian, so the remaining byte is the high byte. */
	if (i < nbytes) {
#ifdef DEBUG_CHECKSUM_VERBOSE
		log_debug ("sum = %x", sum);
#endif
		sum += buf [i] << 8;
		/* Add carry. */
		if (sum > 0xFFFF)
			sum -= 0xFFFF;
	}
	
	return sum;
}

/* Finish computing the checksum, and then put it into network byte order. */

u_int32_t wrapsum (sum)
	u_int32_t sum;
{
#ifdef DEBUG_CHECKSUM
	log_debug ("wrapsum (%x)", sum);
#endif

	sum = ~sum & 0xFFFF;
#ifdef DEBUG_CHECKSUM_VERBOSE
	log_debug ("sum = %x", sum);
#endif
	
#ifdef DEBUG_CHECKSUM
	log_debug ("wrapsum returns %x", htons (sum));
#endif
	return htons(sum);
}

#ifdef PACKET_ASSEMBLY
void assemble_hw_header (interface, buf, bufix, to)
	struct interface_info *interface;
	unsigned char *buf;
	unsigned *bufix;
	struct hardware *to;
{
	switch (interface->hw_address.hbuf[0]) {
#if defined(HAVE_TR_SUPPORT)
	case HTYPE_IEEE802:
		assemble_tr_header(interface, buf, bufix, to);
		break;
#endif
#if defined (DEC_FDDI)
	case HTYPE_FDDI:
		assemble_fddi_header(interface, buf, bufix, to);
		break;
#endif
	case HTYPE_INFINIBAND:
		log_error("Attempt to assemble hw header for infiniband");
		break;
	case HTYPE_ETHER:
	default:
		assemble_ethernet_header(interface, buf, bufix, to);
		break;
	}
}

/* UDP header and IP header assembled together for convenience. */

void assemble_udp_ip_header (interface, buf, bufix,
			     from, to, port, data, len)
	struct interface_info *interface;
	unsigned char *buf;
	unsigned *bufix;
	u_int32_t from;
	u_int32_t to;
	u_int32_t port;
	unsigned char *data;
	unsigned len;
{
	struct ip ip;
	struct udphdr udp;

	memset (&ip, 0, sizeof ip);

	/* Fill out the IP header */
	IP_V_SET (&ip, 4);
	IP_HL_SET (&ip, 20);
	ip.ip_tos = IPTOS_LOWDELAY;
	ip.ip_len = htons(sizeof(ip) + sizeof(udp) + len);
	ip.ip_id = 0;
	ip.ip_off = 0;
	ip.ip_ttl = 128;
	ip.ip_p = IPPROTO_UDP;
	ip.ip_sum = 0;
	ip.ip_src.s_addr = from;
	ip.ip_dst.s_addr = to;
	
	/* Checksum the IP header... */
	ip.ip_sum = wrapsum (checksum ((unsigned char *)&ip, sizeof ip, 0));
	
	/* Copy the ip header into the buffer... */
	memcpy (&buf [*bufix], &ip, sizeof ip);
	*bufix += sizeof ip;

	/* Fill out the UDP header */
	udp.uh_sport = local_port;		/* XXX */
	udp.uh_dport = port;			/* XXX */
	udp.uh_ulen = htons(sizeof(udp) + len);
	memset (&udp.uh_sum, 0, sizeof udp.uh_sum);

	/* Compute UDP checksums, including the ``pseudo-header'', the UDP
	   header and the data. */

	udp.uh_sum =
		wrapsum (checksum ((unsigned char *)&udp, sizeof udp,
				   checksum (data, len, 
					     checksum ((unsigned char *)
						       &ip.ip_src,
						       2 * sizeof ip.ip_src,
						       IPPROTO_UDP +
						       (u_int32_t)
						       ntohs (udp.uh_ulen)))));

	/* Copy the udp header into the buffer... */
	memcpy (&buf [*bufix], &udp, sizeof udp);
	*bufix += sizeof udp;
}
#endif /* PACKET_ASSEMBLY */

#ifdef PACKET_DECODING
/* Decode a hardware header... */
/* Support for ethernet, TR and FDDI
 * Doesn't support infiniband yet as the supported oses shouldn't get here
 */

ssize_t decode_hw_header (interface, buf, bufix, from)
     struct interface_info *interface;
     unsigned char *buf;
     unsigned bufix;
     struct hardware *from;
{
	switch(interface->hw_address.hbuf[0]) {
#if defined (HAVE_TR_SUPPORT)
	case HTYPE_IEEE802:
		return (decode_tr_header(interface, buf, bufix, from));
#endif
#if defined (DEC_FDDI)
	case HTYPE_FDDI:
		return (decode_fddi_header(interface, buf, bufix, from));
#endif
	case HTYPE_INFINIBAND:
		log_error("Attempt to decode hw header for infiniband");
		return (0);
	case HTYPE_ETHER:
	default:
		return (decode_ethernet_header(interface, buf, bufix, from));
	}
}

/* UDP header and IP header decoded together for convenience. */

ssize_t
decode_udp_ip_header(struct interface_info *interface,
		     unsigned char *buf, unsigned bufix,
		     struct sockaddr_in *from, unsigned buflen,
		     unsigned *rbuflen)
{
  unsigned char *data;
  struct ip ip;
  struct udphdr udp;
  unsigned char *upp, *endbuf;
  u_int32_t ip_len, ulen, pkt_len;
  u_int32_t sum, usum;
  static int ip_packets_seen;
  static int ip_packets_bad_checksum;
  static int udp_packets_seen;
  static int udp_packets_bad_checksum;
  static int udp_packets_length_checked;
  static int udp_packets_length_overflow;
  unsigned len;

  /* Designate the end of the input buffer for bounds checks. */
  endbuf = buf + bufix + buflen;

  /* Assure there is at least an IP header there. */
  if ((buf + bufix + sizeof(ip)) > endbuf)
	  return -1;

  /* Copy the IP header into a stack aligned structure for inspection.
   * There may be bits in the IP header that we're not decoding, so we
   * copy out the bits we grok and skip ahead by ip.ip_hl * 4.
   */
  upp = buf + bufix;
  memcpy(&ip, upp, sizeof(ip));
  ip_len = (*upp & 0x0f) << 2;
  upp += ip_len;

  /* Check the IP packet length. */
  pkt_len = ntohs(ip.ip_len);
  if (pkt_len > buflen)
	return -1;

  /* Assure after ip_len bytes that there is enough room for a UDP header. */
  if ((upp + sizeof(udp)) > endbuf)
	  return -1;

  /* Copy the UDP header into a stack aligned structure for inspection. */
  memcpy(&udp, upp, sizeof(udp));

#ifdef USERLAND_FILTER
  /* Is it a UDP packet? */
  if (ip.ip_p != IPPROTO_UDP)
	  return -1;

  /* Is it to the port we're serving? */
  if (udp.uh_dport != local_port)
	  return -1;
#endif /* USERLAND_FILTER */

  ulen = ntohs(udp.uh_ulen);
  if (ulen < sizeof(udp))
	return -1;

  udp_packets_length_checked++;
  if ((upp + ulen) > endbuf) {
	udp_packets_length_overflow++;
	if ((udp_packets_length_checked > 4) &&
	    ((udp_packets_length_checked /
	      udp_packets_length_overflow) < 2)) {
		log_info("%d udp packets in %d too long - dropped",
			 udp_packets_length_overflow,
			 udp_packets_length_checked);
		udp_packets_length_overflow = 0;
		udp_packets_length_checked = 0;
	}
	return -1;
  }

  if ((ulen < sizeof(udp)) || ((upp + ulen) > endbuf))
	return -1;

  /* Check the IP header checksum - it should be zero. */
  ++ip_packets_seen;
  if (wrapsum (checksum (buf + bufix, ip_len, 0))) {
	  ++ip_packets_bad_checksum;
	  if (ip_packets_seen > 4 &&
	      (ip_packets_seen / ip_packets_bad_checksum) < 2) {
		  log_info ("%d bad IP checksums seen in %d packets",
			    ip_packets_bad_checksum, ip_packets_seen);
		  ip_packets_seen = ip_packets_bad_checksum = 0;
	  }
	  return -1;
  }

  /* Copy out the IP source address... */
  memcpy(&from->sin_addr, &ip.ip_src, 4);

  /* Compute UDP checksums, including the ``pseudo-header'', the UDP
     header and the data.   If the UDP checksum field is zero, we're
     not supposed to do a checksum. */

  data = upp + sizeof(udp);
  len = ulen - sizeof(udp);

  usum = udp.uh_sum;
  udp.uh_sum = 0;

  /* XXX: We have to pass &udp, because we have to zero the checksum
   * field before calculating the sum...'upp' isn't zeroed.
   */
  sum = wrapsum(checksum((unsigned char *)&udp, sizeof(udp),
			 checksum(data, len,
				  checksum((unsigned char *)&ip.ip_src,
					   8, IPPROTO_UDP + ulen))));

  udp_packets_seen++;
  if (usum && usum != sum) {
	  udp_packets_bad_checksum++;
	  if (udp_packets_seen > 4 &&
	      (udp_packets_seen / udp_packets_bad_checksum) < 2) {
		  log_info ("%d bad udp checksums in %d packets",
			    udp_packets_bad_checksum, udp_packets_seen);
		  udp_packets_seen = udp_packets_bad_checksum = 0;
	  }
	  return -1;
  }

  /* Copy out the port... */
  memcpy (&from -> sin_port, &udp.uh_sport, sizeof udp.uh_sport);

  /* Save the length of the UDP payload. */
  if (rbuflen != NULL)
	*rbuflen = len;

  /* Return the index to the UDP payload. */
  return ip_len + sizeof udp;
}
#endif /* PACKET_DECODING */
