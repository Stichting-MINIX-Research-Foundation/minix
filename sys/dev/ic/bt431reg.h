/* $NetBSD: bt431reg.h,v 1.3 2005/12/11 12:21:26 christos Exp $ */

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
 * Register definitions for the Brooktree Bt431 Monolithic CMOS
 * 64x64 Pixel Cursor Generator.
 */

#define	BT431_REG_COMMAND	0x000
#define	BT431_REG_CURSOR_X_LOW	0x001
#define	BT431_REG_CURSOR_X_HIGH	0x002
#define	BT431_REG_CURSOR_Y_LOW	0x003
#define	BT431_REG_CURSOR_Y_HIGH	0x004
#define	BT431_REG_WXLO		0x005
#define	BT431_REG_WXHI		0x006
#define	BT431_REG_WYLO		0x007
#define	BT431_REG_WYHI		0x008
#define	BT431_REG_WWLO		0x009
#define	BT431_REG_WWHI		0x00a
#define	BT431_REG_WHLO		0x00b
#define	BT431_REG_WHHI		0x00c

#define BT431_REG_CRAM_BASE	0x000
#define BT431_REG_CRAM_END	0x1ff

#define BT431_CMD_CURS_ENABLE	0x40
#define BT431_CMD_XHAIR_ENABLE	0x20
#define BT431_CMD_OR_CURSORS	0x10
#define BT431_CMD_AND_CURSORS	0x00
#define BT431_CMD_1_1_MUX	0x00
#define BT431_CMD_4_1_MUX	0x04
#define BT431_CMD_5_1_MUX	0x08
#define BT431_CMD_xxx_MUX	0x0c
#define BT431_CMD_THICK_1	0x00
#define BT431_CMD_THICK_3	0x01
#define BT431_CMD_THICK_5	0x02
#define BT431_CMD_THICK_7	0x03
