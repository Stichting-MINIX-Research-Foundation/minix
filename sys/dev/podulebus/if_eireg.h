/* $NetBSD: if_eireg.h,v 1.4 2005/12/11 12:23:28 christos Exp $ */

/*
 * 2000 Ben Harris
 *
 * This file is in the public domain.
 */

/*
 * if_eireg.h - register definitions etc for the Acorn Ether1 card
 */

#ifndef _IF_EIREG_H_
#define _IF_EIREG_H_

/*
 * The card has three address spaces.  The ROM is mapped into the
 * bottom 32 bytes of SYNC address space, and contains the
 * expansion card ID information and the Ethernet address.  There is a
 * pair of write-only registers at the start of the FAST address
 * space.  One of these performs miscellaneous control functions, and
 * the other acts as a page selector for the board memory.  The board
 * has 64k of RAM, and 4k pages of this can be mapped at offset 0x2000
 * in the FAST space by writing the page number to the page register.
 * The 82586 has access to the whole of this memory and (I believe)
 * sees it as the top 64k of its address space.
 */

/* Registers in the board's control space */
#define EI_PAGE		0
#define EI_CONTROL	1
#define EI_CTL_RST	0x01 /* Reset */
#define EI_CTL_LB	0x02 /* Loop-back */
#define EI_CTL_CA	0x04 /* Channel Attention */
#define EI_CTL_CLI	0x08 /* Clear Interrupt */

/* Offset of base of memory in bus_addr_t units */
#define EI_MEMOFF	0x2000

/*
 * All addresses within board RAM are in bytes of actual RAM.  RAM is
 * 16 bits wide, and can only be accessed by word transfers
 * (bus_space_xxx_2).
 */
#define EI_MEMSIZE	0x10000
#define EI_MEMBASE	(0x1000000 - EI_MEMSIZE)
#define EI_PAGESIZE	0x1000
#define EI_NPAGES	(EI_MEMSIZE / EI_PAGESIZE)
#define ei_atop(a)	(((a) % EI_MEMSIZE) / EI_PAGESIZE)
#define ei_atopo(a)	((a) % EI_PAGESIZE)

#define EI_SCP_ADDR	IE_SCP_ADDR % EI_MEMSIZE

/*
 * The ROM on the Ether1 is a bit oddly wired, in that the interrupt line
 * is wired up as the high-order address line, so as to allow the interrupt
 * status bit the first byte to reflect the actual interrupt status.
 */

#define EI_ROMSIZE	0x20
/* First eight bytes are standard extended podule ID. */
#define EI_ROM_HWREV	0x08
#define EI_ROM_EADDR	0x09
#define EI_ROM_CRC	0x1c

#endif
