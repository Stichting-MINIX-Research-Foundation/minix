/*
server/ip/gen/udp_hdr.h
*/

#ifndef __SERVER__IP__GEN__UDP_HDR_H__
#define __SERVER__IP__GEN__UDP_HDR_H__

/*
 * Included for compatibility with programs which assume udp_io_hdr_t to be
 * defined in this header file
 */
#include "udp_io_hdr.h"

typedef struct udp_hdr
{
	udpport_t uh_src_port;
	udpport_t uh_dst_port;
	u16_t uh_length;
	u16_t uh_chksum;
} udp_hdr_t;

#endif /* __SERVER__IP__GEN__UDP_HDR_H__ */
