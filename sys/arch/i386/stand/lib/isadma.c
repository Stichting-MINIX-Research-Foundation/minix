/*	$NetBSD: isadma.c,v 1.2 2008/12/14 17:03:43 christos Exp $	*/

/* from: NetBSD:dev/isa/isadma.c */

#include <sys/types.h>
#include <machine/pio.h>

#include <lib/libsa/stand.h>

#include "isadmavar.h"

#define	IO_DMA1		0x000		/* 8237A DMA Controller #1 */
#define	IO_DMA2		0x0C0		/* 8237A DMA Controller #2 */
#define	DMA37MD_CASCADE	0xc0	/* cascade mode */
#define	DMA1_SMSK	(IO_DMA1 + 1*10)	/* single mask register */
#define	DMA1_MODE	(IO_DMA1 + 1*11)	/* mode register */
#define	DMA2_SMSK	(IO_DMA2 + 2*10)	/* single mask register */
#define	DMA2_MODE	(IO_DMA2 + 2*11)	/* mode register */

/*
 * isa_dmacascade(): program 8237 DMA controller channel to accept
 * external dma control by a board.
 */
void
isa_dmacascade(int chan)
{

#ifdef ISADMA_DEBUG
	if (chan < 0 || chan > 7)
		panic("isa_dmacascade: impossible request");
#endif

	/* set dma channel mode, and set dma channel mode */
	if ((chan & 4) == 0) {
		outb(DMA1_MODE, chan | DMA37MD_CASCADE);
		outb(DMA1_SMSK, chan);
	} else {
		chan &= 3;

		outb(DMA2_MODE, chan | DMA37MD_CASCADE);
		outb(DMA2_SMSK, chan);
	}
}
