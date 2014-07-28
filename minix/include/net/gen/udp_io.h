/*
server/ip/gen/udp_io.h
*/

#ifndef __SERVER__IP__GEN__UDP_IO_H__
#define __SERVER__IP__GEN__UDP_IO_H__

typedef struct nwio_udpopt
{
	unsigned long nwuo_flags;
	udpport_t nwuo_locport;
	udpport_t nwuo_remport;
	ipaddr_t nwuo_locaddr;
	ipaddr_t nwuo_remaddr;
} nwio_udpopt_t;

#define NWUO_NOFLAGS		0x0000L
#define NWUO_ACC_MASK		0x0003L
#define 	NWUO_EXCL		0x00000001L
#define		NWUO_SHARED		0x00000002L
#define		NWUO_COPY		0x00000003L
#define NWUO_LOCPORT_MASK	0x000CL
#define		NWUO_LP_SEL		0x00000004L
#define		NWUO_LP_SET		0x00000008L
#define		NWUO_LP_ANY		0x0000000CL
#define NWUO_LOCADDR_MASK	0x0010L
#define		NWUO_EN_LOC		0x00000010L
#define		NWUO_DI_LOC		0x00100000L
#define NWUO_BROAD_MASK		0x0020L
#define 	NWUO_EN_BROAD		0x00000020L
#define		NWUO_DI_BROAD		0x00200000L
#define NWUO_REMPORT_MASK	0x0100L
#define		NWUO_RP_SET		0x00000100L
#define		NWUO_RP_ANY		0x01000000L
#define NWUO_REMADDR_MASK	0x0200L
#define 	NWUO_RA_SET		0x00000200L
#define		NWUO_RA_ANY		0x02000000L
#define NWUO_RW_MASK		0x1000L
#define		NWUO_RWDATONLY		0x00001000L
#define		NWUO_RWDATALL		0x10000000L
#define NWUO_IPOPT_MASK		0x2000L
#define		NWUO_EN_IPOPT		0x00002000L
#define		NWUO_DI_IPOPT		0x20000000L

#endif /* __SERVER__IP__GEN__UDP_IO_H__ */
