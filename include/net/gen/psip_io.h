/*
server/ip/gen/psip_io.h
*/

#ifndef __SERVER__IP__GEN__PSIP_IO_H__
#define __SERVER__IP__GEN__PSIP_IO_H__

typedef struct nwio_psipopt
{
	unsigned long nwpo_flags;
} nwio_psipopt_t;

#define NWPO_PROMISC_MASK	0x0001L
#define		NWPO_EN_PROMISC		0x00000001L
#define		NWUO_DI_PROMISC		0x00010000L

#endif /* __SERVER__IP__GEN__PSIP_IO_H__ */

/*
 * $PchId: psip_io.h,v 1.2 1995/11/17 22:22:16 philip Exp $
 */
