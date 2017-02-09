/* $NetBSD: bt459reg.h,v 1.4 2005/12/11 12:21:26 christos Exp $ */

/*
 * Mach Operating System
 * Copyright (c) 1991,1990,1989 Carnegie Mellon University
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */

/*
 * Register definitions for the Brooktree Bt459 135 MHz Monolithic
 * CMOS 256x64 Color Palette RAMDAC.
 */

/*
 * Directly-accessible registers.  Note the address register is
 * auto-incrementing.
 */
#define	BT459_REG_ADDR_LOW		0x00	/* C1,C0 == 0,0 */
#define	BT459_REG_ADDR_HIGH		0x01	/* C1,C0 == 0,1 */
#define	BT459_REG_IREG_DATA		0x02	/* C1,C0 == 1,0 */
#define	BT459_REG_CMAP_DATA		0x03	/* C1,C0 == 1,1 */

#define	BT459_REG_MAX			BT459_REG_CMAP_DATA

/*
 * All internal register access to the Bt459 is done indirectly via the
 * Address Register (mapped into the host bus in a device-specific
 * fashion).  The following register definitions are in terms of
 * their address register address values.
 */

						/* 0000-00ff colormap entries */

#define	BT459_IREG_CCOLOR_1		0x0181	/* Cursor color regs */
#define	BT459_IREG_CCOLOR_2		0x0182
#define	BT459_IREG_CCOLOR_3		0x0183
#define	BT459_IREG_ID			0x0200	/* read-only, gives "4a" */
#define	BT459_IREG_COMMAND_0		0x0201
#define	BT459_IREG_COMMAND_1		0x0202
#define	BT459_IREG_COMMAND_2		0x0203
#define	BT459_IREG_PRM			0x0204
						/* 0205 reserved */
#define	BT459_IREG_PBM			0x0206
						/* 0207 reserved */
#define	BT459_IREG_ORM			0x0208
#define	BT459_IREG_OBM			0x0209
#define	BT459_IREG_ILV			0x020a
#define	BT459_IREG_TEST			0x020b
#define	BT459_IREG_RSIG			0x020c
#define	BT459_IREG_GSIG			0x020d
#define	BT459_IREG_BSIG			0x020e
						/* 020f-02ff reserved */
#define	BT459_IREG_CCR			0x0300
#define	BT459_IREG_CURSOR_X_LOW		0x0301
#define	BT459_IREG_CURSOR_X_HIGH	0x0302
#define	BT459_IREG_CURSOR_Y_LOW		0x0303
#define	BT459_IREG_CURSOR_Y_HIGH	0x0304
#define	BT459_IREG_WXLO			0x0305
#define	BT459_IREG_WXHI			0x0306
#define	BT459_IREG_WYLO			0x0307
#define	BT459_IREG_WYHI			0x0308
#define	BT459_IREG_WWLO			0x0309
#define	BT459_IREG_WWHI			0x030a
#define	BT459_IREG_WHLO			0x030b
#define	BT459_IREG_WHHI			0x030c
						/* 030d-03ff reserved */
#define BT459_IREG_CRAM_BASE		0x0400
