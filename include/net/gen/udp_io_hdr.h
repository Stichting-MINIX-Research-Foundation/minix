#ifndef __SERVER__IP__GEN__UDP_IO_HDR_H__
#define __SERVER__IP__GEN__UDP_IO_HDR_H__


typedef struct udp_io_hdr
{
	ipaddr_t uih_src_addr;
	ipaddr_t uih_dst_addr;
	udpport_t uih_src_port;
	udpport_t uih_dst_port;
	u16_t uih_ip_opt_len;
	u16_t uih_data_len;
} udp_io_hdr_t;


#endif /* __SERVER__IP__GEN__UDP_IO_HDR_H__ */
