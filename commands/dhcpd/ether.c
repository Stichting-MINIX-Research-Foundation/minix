/*	ether.c - Raw Ethernet stuff
 *							Author: Kees J. Bot
 *								16 Dec 2000
 */
#include <sys/types.h>
#include <fcntl.h>
#include <string.h>
#include <sys/asynchio.h>
#include <net/hton.h>
#include <net/gen/in.h>
#include <net/gen/ether.h>
#include <net/gen/eth_hdr.h>
#include <net/gen/ip_hdr.h>
#include <net/gen/icmp.h>
#include <net/gen/icmp_hdr.h>
#include <net/gen/oneCsum.h>
#include <net/gen/udp.h>
#include <net/gen/udp_hdr.h>
#include <net/gen/dhcp.h>
#include "arp.h"
#include "dhcpd.h"

static ether_addr_t BCAST_ETH =	{{ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }};
#define BCAST_IP	HTONL(0xFFFFFFFFUL)
#define LOCALHOST	HTONL(0x7F000001UL)

static u16_t udp_cksum(ipaddr_t src, ipaddr_t dst, udp_hdr_t *udp)
{
    /* Compute the checksum of an UDP packet plus data. */
    struct udp_pseudo {
	ipaddr_t	src, dst;
	u8_t		zero, proto;
	u16_t		length;
    } pseudo;
    size_t len;

    /* Fill in the UDP pseudo header that must be prefixed to the UDP
     * packet to compute the checksum of the whole thing.
     */
    pseudo.src= src;
    pseudo.dst= dst;
    pseudo.zero= 0;
    pseudo.proto= IPPROTO_UDP;
    pseudo.length= udp->uh_length;

    len= ntohs(udp->uh_length);
    if (len & 1) {
	/* Length is odd?  Pad with a zero. */
	B(udp)[len++]= 0;
    }
    return oneC_sum(oneC_sum(0, &pseudo, sizeof(pseudo)), udp, len);
}

void udp2ether(buf_t *bp, network_t *np)
{
    /* Transform a packet in UDP format to raw Ethernet.  Ignore the UDP
     * addresses, always broadcast from 0.0.0.0.
     */
    udp_io_hdr_t udpio;

    /* Save the UDP I/O header. */
    udpio= *bp->udpio;

    /* Fill in the Ethernet, IP and UDP headers. */
    bp->eth->eh_dst= BCAST_ETH;
    bp->eth->eh_src= np->eth;
    bp->eth->eh_proto= HTONS(ETH_IP_PROTO);
    bp->ip->ih_vers_ihl= 0x45;
    bp->ip->ih_tos= 0;
    bp->ip->ih_length= htons(sizeof(ip_hdr_t)
			+ sizeof(udp_hdr_t) + udpio.uih_data_len);
    bp->ip->ih_id= 0;
    bp->ip->ih_flags_fragoff= NTOHS(0x4000);
    bp->ip->ih_ttl= IP_MAX_TTL;
    bp->ip->ih_proto= IPPROTO_UDP;
    bp->ip->ih_hdr_chk= 0;
    bp->ip->ih_src= 0;
    bp->ip->ih_dst= BCAST_IP;
    bp->ip->ih_hdr_chk= ~oneC_sum(0, bp->ip, sizeof(*bp->ip));
    bp->udp->uh_src_port= udpio.uih_src_port;
    bp->udp->uh_dst_port= udpio.uih_dst_port;
    bp->udp->uh_length= htons(sizeof(udp_hdr_t) + udpio.uih_data_len);
    bp->udp->uh_chksum= 0;
    bp->udp->uh_chksum= ~udp_cksum(bp->ip->ih_src, bp->ip->ih_dst, bp->udp);
}

int ether2udp(buf_t *bp)
{
    /* Transform an UDP packet read from raw Ethernet to normal UDP.
     * Return true iff the packet is indeed UDP and has no errors.
     */
    udp_io_hdr_t udpio;

    if (bp->eth->eh_proto != HTONS(ETH_IP_PROTO)
	|| bp->ip->ih_vers_ihl != 0x45
	|| bp->ip->ih_proto != IPPROTO_UDP
	|| oneC_sum(0, bp->ip, 20) != (u16_t) ~0
	|| udp_cksum(bp->ip->ih_src, bp->ip->ih_dst, bp->udp) != (u16_t) ~0
    ) {
	/* Not UDP/IP or checksums bad. */
	return 0;
    }
    udpio.uih_src_addr= bp->ip->ih_src;
    udpio.uih_dst_addr= bp->ip->ih_dst;
    udpio.uih_src_port= bp->udp->uh_src_port;
    udpio.uih_dst_port= bp->udp->uh_dst_port;
    udpio.uih_ip_opt_len= 0;
    udpio.uih_data_len= ntohs(bp->udp->uh_length) - sizeof(udp_hdr_t);
    *bp->udpio= udpio;
    return 1;
}

void make_arp(buf_t *bp, network_t *np)
{
    /* Create an ARP packet to query for my IP address. */
    arp46_t *arp= (arp46_t *) bp->eth;

    memset(arp, 0, sizeof(*arp));
    arp->dstaddr= BCAST_ETH;
    arp->srcaddr= np->eth;
    arp->ethtype= HTONS(ETH_ARP_PROTO);
    arp->hdr= HTONS(ARP_ETHERNET);
    arp->pro= HTONS(ETH_IP_PROTO);
    arp->hln= 6;
    arp->pln= 4;
    arp->op= HTONS(ARP_REQUEST);
    arp->sha= np->eth;
    memcpy(arp->spa, &np->ip, sizeof(np->ip));
    memcpy(arp->tpa, &np->ip, sizeof(np->ip));
}

int is_arp_me(buf_t *bp, network_t *np)
{
    /* True iff an ARP packet is a reply from someone else with an address I
     * thought was mine.  (That's like, bad.)
     */
    arp46_t *arp= (arp46_t *) bp->eth;

    if (arp->ethtype == HTONS(ETH_ARP_PROTO)
	&& arp->hdr == HTONS(ARP_ETHERNET)
	&& arp->pro == HTONS(ETH_IP_PROTO)
	&& arp->op == HTONS(ARP_REPLY)
	&& memcmp(&arp->spa, &np->ip, sizeof(np->ip)) == 0
	&& memcmp(&arp->sha, &np->eth, sizeof(np->eth)) != 0
    ) {
	np->conflict= arp->sha;
	return 1;
    }
    return 0;
}

void icmp_solicit(buf_t *bp)
{
    /* Fill in a router solicitation ICMP packet. */
    icmp_hdr_t *icmp= (icmp_hdr_t *) (bp->ip + 1);

    bp->ip->ih_vers_ihl= 0x45;
    bp->ip->ih_dst= BCAST_IP;

    icmp->ih_type= ICMP_TYPE_ROUTE_SOL;
    icmp->ih_code= 0;
    icmp->ih_hun.ihh_unused= 0;
    icmp->ih_chksum= 0;
    icmp->ih_chksum= ~oneC_sum(0, icmp, 8);
}

void icmp_advert(buf_t *bp, network_t *np)
{
    /* Fill in a router advert to be sent to my own interface. */
    icmp_hdr_t *icmp= (icmp_hdr_t *) (bp->ip + 1);

    bp->ip->ih_vers_ihl= 0x45;
    bp->ip->ih_dst= LOCALHOST;

    icmp->ih_type= ICMP_TYPE_ROUTER_ADVER;
    icmp->ih_code= 0;
    icmp->ih_hun.ihh_ram.iram_na= 1;
    icmp->ih_hun.ihh_ram.iram_aes= 2;
    icmp->ih_hun.ihh_ram.iram_lt= htons(DELTA_ADV);
    ((u32_t *) icmp->ih_dun.uhd_data)[0] = np->gateway;
    ((u32_t *) icmp->ih_dun.uhd_data)[1] = HTONL((u32_t) -9999);
    icmp->ih_chksum= 0;
    icmp->ih_chksum= ~oneC_sum(0, icmp, 16);
}

ipaddr_t icmp_is_advert(buf_t *bp)
{
    /* Check if an IP packet is a router advertisement, and if it's genuine,
     * i.e. the sender is mentioned in the packet.
     */
    icmp_hdr_t *icmp= (icmp_hdr_t *) (bp->ip + 1);
    int i;

    if (icmp->ih_type == ICMP_TYPE_ROUTER_ADVER) {
	for (i= 0; i < icmp->ih_hun.ihh_ram.iram_na; i++) {
	    if (((u32_t *) icmp->ih_dun.uhd_data)[2*i] == bp->ip->ih_src) {
		/* It's a router! */
		return bp->ip->ih_src;
	    }
	}
    }
    return 0;
}
