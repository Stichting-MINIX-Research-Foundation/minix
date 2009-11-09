/*
minix/swap.h

Defines the super block of swap partitions and some useful constants.

Created:	Aug 2, 1992 by Philip Homburg
*/

#ifndef _MINIX__SWAP_H
#define _MINIX__SWAP_H

/* Two possible layouts for a partition with swapspace:
 *
 *	Sector		Swap partition		FS+swap partition
 *
 *       0 - 1		bootblock		bootblock
 *	     2		swap header		FS header
 *	     3		blank			swap header
 *	 4 - m		swapspace		file system
 *	m+1 - n		-			swapspace
 */
 
#define SWAP_MAGIC0	0x9D
#define SWAP_MAGIC1	0xC3
#define SWAP_MAGIC2	0x01
#define SWAP_MAGIC3	0x82

typedef struct swap_hdr
{
	u8_t sh_magic[4];
	u8_t sh_version;
	u8_t sh_dummy[3];
	u32_t sh_offset;
	u32_t sh_swapsize;
	i32_t sh_priority;
} swap_hdr_t;

#define SWAP_BOOTOFF	 1024
#define SWAP_OFFSET	 2048
#define OPTSWAP_BOOTOFF	(1024+512)
#define SH_VERSION	    1
#define SH_PRIORITY	    0

#endif /* _MINIX__SWAP_H */

/*
 * $PchId: swap.h,v 1.6 1996/04/10 20:25:48 philip Exp $
 */
