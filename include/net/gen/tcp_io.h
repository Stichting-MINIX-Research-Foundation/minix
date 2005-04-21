/*
server/ip/gen/tcp_io.h
*/

#ifndef __SERVER__IP__GEN__TCP_IO_H__
#define __SERVER__IP__GEN__TCP_IO_H__

typedef struct nwio_tcpconf
{
	u32_t nwtc_flags;
	ipaddr_t nwtc_locaddr;
	ipaddr_t nwtc_remaddr;
	tcpport_t nwtc_locport;
	tcpport_t nwtc_remport;
} nwio_tcpconf_t;

#define NWTC_NOFLAGS	0x0000L
#define NWTC_ACC_MASK	0x0003L
#	define NWTC_EXCL	0x00000001L
#	define NWTC_SHARED	0x00000002L
#	define NWTC_COPY	0x00000003L
#define NWTC_LOCPORT_MASK	0x0030L
#	define NWTC_LP_UNSET	0x00000010L
#	define NWTC_LP_SET	0x00000020L
#	define NWTC_LP_SEL	0x00000030L
#define NWTC_REMADDR_MASK	0x0100L
#	define NWTC_SET_RA	0x00000100L
#	define NWTC_UNSET_RA	0x01000000L
#define NWTC_REMPORT_MASK	0x0200L
#	define NWTC_SET_RP	0x00000200L
#	define NWTC_UNSET_RP	0x02000000L

typedef struct nwio_tcpcl
{
	long nwtcl_flags;
	long nwtcl_ttl;
} nwio_tcpcl_t;

typedef struct nwio_tcpatt
{
	long nwta_flags;
} nwio_tcpatt_t;

typedef struct nwio_tcpopt
{
	u32_t nwto_flags;
} nwio_tcpopt_t;

#define NWTO_NOFLAG		0x0000L
#define NWTO_SND_URG_MASK	0x0001L
#	define NWTO_SND_URG	0x00000001L
#	define NWTO_SND_NOTURG	0x00010000L
#define NWTO_RCV_URG_MASK	0x0002L
#	define NWTO_RCV_URG	0x00000002L
#	define NWTO_RCV_NOTURG	0x00020000L
#define NWTO_BSD_URG_MASK	0x0004L
#	define NWTO_BSD_URG	0x00000004L
#	define NWTO_NOTBSD_URG	0x00040000L
#define NWTO_DEL_RST_MASK	0x0008L
#	define NWTO_DEL_RST	0x00000008L

#endif /* __SERVER__IP__GEN__TCP_IO_H__ */

/*
 * $PchId: tcp_io.h,v 1.4 1995/11/17 22:17:47 philip Exp $
 */
