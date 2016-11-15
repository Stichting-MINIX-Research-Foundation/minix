/*	$NetBSD: zxreg.h,v 1.7 2009/04/23 20:46:49 macallan Exp $	*/

/*
 *  Copyright (c) 2002 The NetBSD Foundation, Inc.
 *  All rights reserved.
 *
 *  This code is derived from software contributed to The NetBSD Foundation
 *  by Andrew Doran.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 *  ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 *  TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 *  PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 *  BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (C) 1999, 2000 Jakub Jelinek (jakub@redhat.com)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * JAKUB JELINEK BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#ifndef _DEV_SBUS_ZXREG_H_
#define _DEV_SBUS_ZXREG_H_

/* Hardware offsets. */
#define ZX_OFF_UNK2		0x00000000
#define ZX_OFF_LC_SS0_KRN	0x00200000
#define ZX_OFF_LC_SS0_USR	0x00201000
#define ZX_OFF_LD_SS0		0x00400000
#define ZX_OFF_LD_GBL		0x00401000
#define ZX_OFF_LX_CROSS	0x00600000
#define ZX_OFF_LX_CURSOR	0x00601000
#define ZX_OFF_UNK		0x00602000
#define ZX_OFF_SS0		0x00800000
#define ZX_OFF_LC_SS1_KRN	0x01200000
#define ZX_OFF_LC_SS1_USR	0x01201000
#define ZX_OFF_LD_SS1		0x01400000
#define ZX_OFF_SS1		0x01800000

/* offsets relative to ZX_OFF_LC_SS0_KRN */
/* Leo clock domain */
#define ZX_LC_SS0_LEO_INT_ENABLE	0x00000000
#define ZX_LC_SS0_CLR_BLIT_DONE		0x00000004
#define ZX_LC_SS0_CLR_DEODRAW_SEM	0x00000008

/* SBus clock domain */
#define ZX_LC_SS0_CHIP_CODE		0x00000800
#define ZX_LC_SS0_SBUS_STATUS		0x00000804
#define ZX_LC_SS0_SBUS_INT_ENABLE	0x00000808
#define ZX_LC_SS0_FIRST_TIMEOUT_CNTR	0x0000080c
#define ZX_LC_SS0_RERUN_CNTR		0x00000810
#define ZX_LC_SS0_CLR_READ_DMA_DONE	0x00000820
#define ZX_LC_SS0_CLR_WRITE_DMA_DONE	0x00000824
#define ZX_LC_SS0_CLR_INVALID_PTE	0x00000828
#define ZX_LC_SS0_CLR_DMA_ERROR_ACK	0x0000082c
#define ZX_LC_SS0_CLR_SLAVE_ILL_ADDR	0x00000830
#define ZX_LC_SS0_CLR_SLAVE_RERUN_TOUT	0x00000834
#define ZX_LC_SS0_LEO_RESET		0x00000840
#define ZX_LC_SS0_CLR_LEO_RESET		0x00000844
#define ZX_LC_SS0_DMA_READ_PAUSE	0x00000848

/* Leo clock domain */
#define ZX_LC_SS0_LEO_SYSTEM_STATUS	0x00001000
#define ZX_LC_SS0_FB_ADDRESS_SPACE	0x00001004
#define ZX_LC_SS0_STENCIL_MASK		0x00001008
#define ZX_LC_SS0_STENCIL_TRANSPARENT	0x0000100c
#define ZX_LC_SS0_DIRECTION_SIZE	0x00001010
#define ZX_LC_SS0_SOURCE_ADDR		0x00001014
#define ZX_LC_SS0_DEST_COPY_NOSTART	0x00001018
#define ZX_LC_SS0_DEST_COPY_START	0x0000101c
#define ZX_LC_SS0_DEST_FILL_START	0x00001020

/* ROP register */
#define ZX_ATTR_PICK_DISABLE	0x00000000
#define ZX_ATTR_PICK_2D	0x80000000
#define ZX_ATTR_PICK_3D	0xa0000000
#define ZX_ATTR_PICK_2D_REND	0xc0000000
#define ZX_ATTR_PICK_3D_REND	0xe0000000

#define ZX_ATTR_DCE_DISABLE	0x00000000
#define ZX_ATTR_DCE_ENABLE	0x10000000

#define ZX_ATTR_APE_DISABLE	0x00000000
#define ZX_ATTR_APE_ENABLE	0x08000000

#define ZX_ATTR_COLOR_VAR	0x00000000
#define ZX_ATTR_COLOR_CONST	0x04000000

#define ZX_ATTR_AA_DISABLE	0x02000000
#define ZX_ATTR_AA_ENABLE	0x01000000

#define ZX_ATTR_ABE_BG		0x00000000	/* dst + alpha * (src - bg) */
#define ZX_ATTR_ABE_FB		0x00800000	/* dst + alpha * (src - dst) */

#define ZX_ATTR_ABE_DISABLE	0x00000000
#define ZX_ATTR_ABE_ENABLE	0x00400000

#define ZX_ATTR_BLTSRC_A	0x00000000
#define ZX_ATTR_BLTSRC_B	0x00200000

#define ZX_ROP_ZERO		(0x0 << 18)
#define ZX_ROP_NEW_AND_OLD	(0x8 << 18)
#define ZX_ROP_NEW_AND_NOLD	(0x4 << 18)
#define ZX_ROP_NEW		(0xc << 18)
#define ZX_ROP_NNEW_AND_OLD	(0x2 << 18)
#define ZX_ROP_OLD		(0xa << 18)
#define ZX_ROP_NEW_XOR_OLD	(0x6 << 18)
#define ZX_ROP_NEW_OR_OLD	(0xe << 18)
#define ZX_ROP_NNEW_AND_NOLD	(0x1 << 18)
#define ZX_ROP_NNEW_XOR_NOLD	(0x9 << 18)
#define ZX_ROP_NOLD		(0x5 << 18)
#define ZX_ROP_NEW_OR_NOLD	(0xd << 18)
#define ZX_ROP_NNEW		(0x3 << 18)
#define ZX_ROP_NNEW_OR_OLD	(0xb << 18)
#define ZX_ROP_NNEW_OR_NOLD	(0x7 << 18)
#define ZX_ROP_ONES		(0xf << 18)

#define ZX_ATTR_HSR_DISABLE	0x00000000
#define ZX_ATTR_HSR_ENABLE	0x00020000

#define ZX_ATTR_WRITEZ_DISABLE	0x00000000
#define ZX_ATTR_WRITEZ_ENABLE	0x00010000

#define ZX_ATTR_Z_VAR		0x00000000
#define ZX_ATTR_Z_CONST	0x00008000

#define ZX_ATTR_WCLIP_DISABLE	0x00000000
#define ZX_ATTR_WCLIP_ENABLE	0x00004000

#define ZX_ATTR_MONO		0x00000000
#define ZX_ATTR_STEREO_LEFT	0x00001000
#define ZX_ATTR_STEREO_RIGHT	0x00003000

#define ZX_ATTR_WE_DISABLE	0x00000000
#define ZX_ATTR_WE_ENABLE	0x00000800

#define ZX_ATTR_FCE_DISABLE	0x00000000
#define ZX_ATTR_FCE_ENABLE	0x00000400

#define ZX_ATTR_RE_DISABLE	0x00000000
#define ZX_ATTR_RE_ENABLE	0x00000200

#define ZX_ATTR_GE_DISABLE	0x00000000
#define ZX_ATTR_GE_ENABLE	0x00000100

#define ZX_ATTR_BE_DISABLE	0x00000000
#define ZX_ATTR_BE_ENABLE	0x00000080

#define ZX_ATTR_RGBE_DISABLE	0x00000000
#define ZX_ATTR_RGBE_ENABLE	0x00000380

#define ZX_ATTR_OE_DISABLE	0x00000000
#define ZX_ATTR_OE_ENABLE	0x00000040

#define ZX_ATTR_ZE_DISABLE	0x00000000
#define ZX_ATTR_ZE_ENABLE	0x00000020

#define ZX_ATTR_FORCE_WID	0x00000010

#define ZX_ATTR_FC_PLANE_MASK	0x0000000e

#define ZX_ATTR_BUFFER_A	0x00000000
#define ZX_ATTR_BUFFER_B	0x00000001

/* CSR */
#define ZX_CSR_BLT_BUSY	0x20000000

/* draw ss0 ss1 */
#define zd_csr		0x0e00
#define zd_wid		0x0e04
#define zd_wmask	0x0e08
#define zd_widclip	0x0e0c
#define zd_vclipmin	0x0e10
#define zd_vclipmax	0x0e14
#define zd_pickmin	0x0e18		/* SS1 only */
#define zd_pickmax	0x0e1c		/* SS1 only */
#define zd_fg		0x0e20
#define zd_bg		0x0e24
#define zd_src		0x0e28		/* Copy/Scroll (SS0 only) */
#define zd_dst		0x0e2c		/* Copy/Scroll/Fill (SS0 only) */
#define zd_extent	0x0e30		/* Copy/Scroll/Fill size (SS0 only) */

#define zd_setsem	0x0e40		/* SS1 only */
#define zd_clrsem	0x0e44		/* SS1 only */
#define zd_clrpick	0x0e48		/* SS1 only */
#define zd_clrdat	0x0e4c		/* SS1 only */
#define zd_alpha	0x0e50		/* SS1 only */

#define zd_winbg	0x0e80
#define zd_planemask	0x0e84
#define zd_rop		0x0e88
#define zd_z		0x0e8c
#define zd_dczf		0x0e90		/* SS1 only */
#define zd_dczb		0x0e94		/* SS1 only */
#define zd_dcs		0x0e98		/* SS1 only */
#define zd_dczs		0x0e9c		/* SS1 only */
#define zd_pickfb	0x0ea0		/* SS1 only */
#define zd_pickbb	0x0ea4		/* SS1 only */
#define zd_dcfc		0x0ea8		/* SS1 only */
#define zd_forcecol	0x0eac		/* SS1 only */
#define zd_door0	0x0eb0		/* SS1 only */
#define zd_door1	0x0eb4		/* SS1 only */
#define zd_door2	0x0eb8		/* SS1 only */
#define zd_door3	0x0ebc		/* SS1 only */
#define zd_door4	0x0ec0		/* SS1 only */
#define zd_door5	0x0ec4		/* SS1 only */
#define zd_door6	0x0ec8		/* SS1 only */
#define zd_door7	0x0ecc		/* SS1 only */
#define zd_pick0	0x0ed0		/* SS1 only */
#define zd_pick1	0x0ed4		/* SS1 only */
#define zd_pick2	0x0ed8		/* SS1 only */
#define zd_pick3	0x0edc		/* SS1 only */
#define zd_pick4	0x0ee0		/* SS1 only */

#define zd_misc		0x0ef4		/* SS1 only */

#define	ZX_SS1_MISC_ENABLE	0x00000001
#define	ZX_SS1_MISC_STEREO	0x00000002

#define ZX_ADDRSPC_OBGR		0x00
#define ZX_ADDRSPC_Z		0x01
#define ZX_ADDRSPC_W		0x02
#define ZX_ADDRSPC_FONT_OBGR	0x04
#define ZX_ADDRSPC_FONT_Z	0x05
#define ZX_ADDRSPC_FONT_W	0x06
#define ZX_ADDRSPC_O		0x08
#define ZX_ADDRSPC_B		0x09
#define ZX_ADDRSPC_G		0x0a
#define ZX_ADDRSPC_R		0x0b

/* command */
#define zc_csr		0x00
#define zc_addrspace	0x04
#define zc_fontmsk	0x08
#define zc_fontt	0x0c
#define zc_extent	0x10
#define zc_src		0x14
#define zc_dst		0x18
#define zc_copy		0x1c
#define zc_fill		0x20

#define ZX_CROSS_TYPE_CLUT0	0x00001000
#define ZX_CROSS_TYPE_CLUT1	0x00001001
#define ZX_CROSS_TYPE_CLUT2	0x00001002
#define ZX_CROSS_TYPE_WID	0x00001003
#define ZX_CROSS_TYPE_UNK	0x00001006
#define ZX_CROSS_TYPE_VIDEO	0x00002003
#define ZX_CROSS_TYPE_CLUTDATA	0x00004000

#define ZX_CROSS_CSR_ENABLE	0x00000008
#define ZX_CROSS_CSR_PROGRESS	0x00000004
#define ZX_CROSS_CSR_UNK	0x00000002
#define ZX_CROSS_CSR_UNK2	0x00000001

/* cross */
#define zx_type		0x00
#define zx_csr		0x04
#define zx_value	0x08

/* cursor */
#define zcu_type	0x10
#define zcu_misc	0x14
#define zcu_sxy		0x18
#define zcu_data	0x1c

#endif	/* !_DEV_SBUS_ZXREG_H_ */
