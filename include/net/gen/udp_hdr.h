/*
server/ip/gen/udp_hdr.h
*/

#ifndef __SERVER__IP__GEN__UDP_HDR_H__
#define __SERVER__IP__GEN__UDP_HDR_H__

typedef struct udp_hdr
{
	udpport_t uh_src_port;
	udpport_t uh_dst_port;
	u16_t uh_length;
	u16_t uh_chksum;
} udp_hdr_t;

typedef struct udp_io_hdr
{
	ipaddr_t uih_src_addr;
	ipaddr_t uih_dst_addr;
	udpport_t uih_src_port;
	udpport_t uih_dst_port;
	u16_t uih_ip_opt_len;
	u16_t uih_data_len;
} udp_io_hdr_t;

#endif /* __SERVER__IP__GEN__UDP_HDR_H__ */
