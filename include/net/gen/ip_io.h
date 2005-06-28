/*
server/ip/gen/ip_io.h
*/

#ifndef __SERVER__IP__GEN__IP_IO_H__
#define __SERVER__IP__GEN__IP_IO_H__

typedef struct nwio_ipconf2
{
	u32_t	nwic_flags;
	ipaddr_t nwic_ipaddr;
	ipaddr_t nwic_netmask;
} nwio_ipconf2_t;

typedef struct nwio_ipconf
{
	u32_t	nwic_flags;
	ipaddr_t nwic_ipaddr;
	ipaddr_t nwic_netmask;
	u16_t nwic_mtu;
} nwio_ipconf_t;

#define NWIC_NOFLAGS		0x0
#define NWIC_FLAGS		0x7
#	define NWIC_IPADDR_SET		0x1
#	define NWIC_NETMASK_SET		0x2
#	define NWIC_MTU_SET		0x4

typedef struct nwio_ipopt
{
	u32_t nwio_flags;
	ipaddr_t nwio_rem;
	ip_hdropt_t nwio_hdropt;
	u8_t nwio_tos;
	u8_t nwio_ttl;
	u8_t nwio_df;
	ipproto_t nwio_proto;
} nwio_ipopt_t;

#define NWIO_NOFLAGS	0x0000l
#define NWIO_ACC_MASK	0x0003l
#	define NWIO_EXCL	0x00000001l
#	define NWIO_SHARED	0x00000002l
#	define NWIO_COPY	0x00000003l
#define NWIO_LOC_MASK	0x0010l
#	define NWIO_EN_LOC	0x00000010l
#	define NWIO_DI_LOC	0x00100000l
#define NWIO_BROAD_MASK	0x0020l
#	define NWIO_EN_BROAD	0x00000020l
#	define NWIO_DI_BROAD	0x00200000l
#define NWIO_REM_MASK	0x0100l
#	define NWIO_REMSPEC	0x00000100l
#	define NWIO_REMANY	0x01000000l
#define NWIO_PROTO_MASK	0x0200l
#	define NWIO_PROTOSPEC	0x00000200l
#	define NWIO_PROTOANY	0x02000000l
#define NWIO_HDR_O_MASK	0x0400l
#	define NWIO_HDR_O_SPEC	0x00000400l
#	define NWIO_HDR_O_ANY	0x04000000l
#define NWIO_RW_MASK	0x1000l
#	define NWIO_RWDATONLY	0x00001000l
#	define NWIO_RWDATALL	0x10000000l

#endif /* __SERVER__IP__GEN__IP_IO_H__ */

/*
 * $PchId: ip_io.h,v 1.5 2001/03/12 22:17:25 philip Exp $
 */
