/*	$NetBSD: i8237reg.h,v 1.8 2005/12/11 12:21:26 christos Exp $	*/

/*
 * Intel 8237 DMA Controller
 */

#define	DMA37MD_WRITE	0x04	/* read the device, write memory operation */
#define	DMA37MD_READ	0x08	/* write the device, read memory operation */
#define	DMA37MD_LOOP	0x10	/* auto-initialize mode */
#define	DMA37MD_DEMAND	0x00	/* demand mode */
#define	DMA37MD_SINGLE	0x40	/* single mode */
#define	DMA37MD_BLOCK	0x80	/* block mode */
#define	DMA37MD_CASCADE	0xc0	/* cascade mode */

#define	DMA37SM_CLEAR	0x00	/* clear mask bit */
#define	DMA37SM_SET	0x04	/* set mask bit */
