/*	$NetBSD: cgsixreg.h,v 1.10 2013/05/28 15:25:37 macallan Exp $ */

/*
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)cgsixreg.h	8.4 (Berkeley) 1/21/94
 */

/*
 * CG6 display registers.  (Note, I got tired of writing `cgsix' about
 * halfway through and changed everything to cg6, but I probably missed
 * some.  Unfortunately, the way config works, we need to spell out `six'
 * in some places anyway.)
 *
 * The cg6 is a complicated beastie.  We have been unable to extract any
 * documentation and most of the following are guesses based on a limited
 * amount of reverse engineering.
 *
 * A cg6 is composed of numerous groups of control registers, all with TLAs:
 *	FBC - frame buffer control?
 *	FHC - fbc hardware configuration / control? register (32 bits)
 *	DHC - ???
 *	TEC - transform engine control?
 *	THC - TEC Hardware Configuration
 *	ROM - a 64Kbyte ROM with who knows what in it.
 *	colormap - see below
 *	frame buffer memory (video RAM)
 *	possible other stuff
 *
 * Like the cg3, the cg6 uses a Brooktree Video DAC (see btreg.h).
 *
 * Various revisions of the cgsix have various hardware bugs.  So far,
 * we have only seen rev 1 & 2.
 */

/* Control register banks offsets */
#define CGSIX_ROM_OFFSET	0x000000
#define CGSIX_BT_OFFSET		0x200000
#define CGSIX_DHC_OFFSET	0x240000
#define CGSIX_ALT_OFFSET	0x280000
#define CGSIX_FHC_OFFSET	0x300000
#define CGSIX_THC_OFFSET	0x301000
#define CGSIX_FBC_OFFSET	0x700000
#define CGSIX_TEC_OFFSET	0x701000
#define CGSIX_RAM_OFFSET	0x800000

/* bits in FHC register */
#define	FHC_FBID_MASK	0xff000000	/* bits 24..31 are frame buffer ID */
#define	FHC_FBID_SHIFT	24
#define	FHC_REV_MASK	0x00f00000	/* bits 20..23 are revision */
#define	FHC_REV_SHIFT	20
#define	FHC_FROP_DISABLE 0x00080000	/* disable fast rasterops */
#define	FHC_ROW_DISABLE	0x00040000	/* disable row cache */
#define	FHC_SRC_DISABLE	0x00020000	/* disable source cache */
#define	FHC_DST_DISABLE	0x00010000	/* disable destination cache */
#define	FHC_RESET	0x00008000	/* reset FBC */
#define	FHC_XXX0	0x00004000	/* unused */
#define	FHC_LEBO	0x00002000	/* set little endian byte order */
#define	FHC_RES_MASK	0x00001800	/* bits 11&12 are resolution */
#define	FHC_RES_1024	 0x00000000		/* res = 1024x768 */
#define	FHC_RES_1152	 0x00000800		/* res = 1152x900 */
#define	FHC_RES_1280	 0x00001000		/* res = 1280x1024 */
#define	FHC_RES_1600	 0x00001800		/* res = 1600x1200 */
#define	FHC_CPU_MASK	0x00000600	/* bits 9&10 are cpu type */
#define	FHC_CPU_SPARC	 0x00000000		/* cpu = sparc */
#define	FHC_CPU_68020	 0x00000200		/* cpu = 68020 */
#define	FHC_CPU_386	 0x00000400		/* cpu = 80386 */
#define	FHC_CPU_XXX	 0x00000600		/* unused */
#define	FHC_TEST	0x00000100	/* modify TESTX and TESTY */
#define	FHC_TESTX_MASK	0x000000f0	/* bits 4..7 are test window X */
#define	FHC_TESTX_SHIFT	4
#define	FHC_TESTY_MASK	0x0000000f	/* bits 0..3 are test window Y */
#define	FHC_TESTY_SHIFT	0

/*
 * The layout of the THC.
 */
struct cg6_thc {
	u_int32_t	thc_xxx0[512];	/* ??? */
	u_int32_t	thc_hsync1;	/* horizontal sync timing */
	u_int32_t	thc_hsync2;	/* more hsync timing */
	u_int32_t	thc_hsync3;	/* yet more hsync timing */
	u_int32_t	thc_vsync1;	/* vertical sync timing */
	u_int32_t	thc_vsync2;	/* only two of these */
	u_int32_t	thc_refresh;	/* refresh counter */
	u_int32_t	thc_misc;	/* miscellaneous control & status */
	u_int32_t	thc_xxx1[56];	/* ??? */
	u_int32_t	thc_cursxy;	/* cursor x,y position (16 bits each) */
	u_int32_t	thc_cursmask[32];/* cursor mask bits */
	u_int32_t	thc_cursbits[32];/* what to show where mask enabled */
};

/* bits in thc_misc */
#define	THC_MISC_XXX0		0xfff00000	/* unused */
#define	THC_MISC_REVMASK	0x000f0000	/* cg6 revision? */
#define	THC_MISC_REVSHIFT	16
#define	THC_MISC_XXX1		0x0000e000	/* unused */
#define	THC_MISC_RESET		0x00001000	/* ??? */
#define	THC_MISC_XXX2		0x00000800	/* unused */
#define	THC_MISC_VIDEN		0x00000400	/* video enable */
#define	THC_MISC_SYNC		0x00000200	/* sync status */
#define	THC_MISC_VSYNC		0x00000100	/* vsync status */
#define	THC_MISC_SYNCEN		0x00000080	/* sync enable */
#define	THC_MISC_CURSRES	0x00000040	/* cursor resolution */
#define	THC_MISC_INTEN		0x00000020	/* v.retrace intr enable */
#define	THC_MISC_INTR		0x00000010	/* intr pending / ack bit */
#define	THC_MISC_XXX		0x0000000f	/* ??? */

/* cursor x / y position value for `off' */
#define	THC_CURSOFF	(65536-32)	/* i.e., USHRT_MAX+1-32 */

/*
 * Partial description of TEC (needed to get around FHC rev 1 bugs).
 */
struct cg6_tec_xxx {
	u_int32_t	tec_mv;		/* matrix stuff */
	u_int32_t	tec_clip;	/* clipping stuff */
	u_int32_t	tec_vdc;	/* ??? */
};

/*
 * Partial description of FBC
 *
 * Most of this we don't care about; here are only the portions
 * we need, most notably the blitter.  Comments are merely my
 * best guesses as to register functions, based largely on the
 * X11R6.4 sunGX code.  Some of these are here only so we can
 * stuff canned values in them (eg, offx).
 */
struct cg6_fbc {
	u_int32_t fbc_config;		/* r/o CONFIG register */
	volatile u_int32_t fbc_mode;	/* mode setting */
	u_int32_t fbc_clip;		/* TEC clip check */
	u_int32_t fbc_pad2[1];
	u_int32_t fbc_s;		/* global status */
	u_int32_t fbc_draw;		/* drawing pipeline status */
	u_int32_t fbc_blit;		/* blitter status */
	u_int32_t fbc_font;		/* pixel transfer register */
	u_int32_t fbc_pad3[24];
	u_int32_t fbc_x0;		/* blitter, src llx */
	u_int32_t fbc_y0;		/* blitter, src lly */
	u_int32_t fbc_pad4[2];
	u_int32_t fbc_x1;		/* blitter, src urx */
	u_int32_t fbc_y1;		/* blitter, src ury */
	u_int32_t fbc_pad5[2];
	u_int32_t fbc_x2;		/* blitter, dst llx */
	u_int32_t fbc_y2;		/* blitter, dst lly */
	u_int32_t fbc_pad6[2];
	u_int32_t fbc_x3;		/* blitter, dst urx */
	u_int32_t fbc_y3;		/* blitter, dst ury */
	u_int32_t fbc_pad7[2];
	u_int32_t fbc_offx;		/* x offset for drawing */
	u_int32_t fbc_offy;		/* y offset for drawing */
	u_int32_t fbc_pad8[2];
	u_int32_t fbc_incx;		/* x offset for drawing */
	u_int32_t fbc_incy;		/* y offset for drawing */
	u_int32_t fbc_pad81[2];
	u_int32_t fbc_clipminx;		/* clip rectangle llx */
	u_int32_t fbc_clipminy;		/* clip rectangle lly */
	u_int32_t fbc_pad9[2];
	u_int32_t fbc_clipmaxx;		/* clip rectangle urx */
	u_int32_t fbc_clipmaxy;		/* clip rectangle ury */
	u_int32_t fbc_pad10[2];
	u_int32_t fbc_fg;		/* fg value for rop */
	u_int32_t fbc_bg;
	u_int32_t fbc_alu;		/* operation to be performed */
	u_int32_t fbc_pad12[509];
	u_int32_t fbc_arectx;		/* rectangle drawing, x coord */
	u_int32_t fbc_arecty;		/* rectangle drawing, y coord */
	/* actually much more, but nothing more we need */
};

/* FBC mode definitions (from XFree86) */
#define CG6_FBC_BLIT_IGNORE		0x00000000
#define CG6_FBC_BLIT_NOSRC		0x00100000
#define CG6_FBC_BLIT_SRC		0x00200000
#define CG6_FBC_BLIT_ILLEGAL		0x00300000
#define CG6_FBC_BLIT_MASK		0x00300000

#define CG6_FBC_VBLANK			0x00080000

#define CG6_FBC_MODE_IGNORE		0x00000000
#define CG6_FBC_MODE_COLOR8		0x00020000
#define CG6_FBC_MODE_COLOR1		0x00040000
#define CG6_FBC_MODE_HRMONO		0x00060000
#define CG6_FBC_MODE_MASK		0x00060000

#define CG6_FBC_DRAW_IGNORE		0x00000000
#define CG6_FBC_DRAW_RENDER		0x00008000
#define CG6_FBC_DRAW_PICK		0x00010000
#define CG6_FBC_DRAW_ILLEGAL		0x00018000
#define CG6_FBC_DRAW_MASK		0x00018000

#define CG6_FBC_BWRITE0_IGNORE		0x00000000
#define CG6_FBC_BWRITE0_ENABLE		0x00002000
#define CG6_FBC_BWRITE0_DISABLE		0x00004000
#define CG6_FBC_BWRITE0_ILLEGAL		0x00006000
#define CG6_FBC_BWRITE0_MASK		0x00006000

#define CG6_FBC_BWRITE1_IGNORE		0x00000000
#define CG6_FBC_BWRITE1_ENABLE		0x00000800
#define CG6_FBC_BWRITE1_DISABLE		0x00001000
#define CG6_FBC_BWRITE1_ILLEGAL		0x00001800
#define CG6_FBC_BWRITE1_MASK		0x00001800

#define CG6_FBC_BREAD_IGNORE		0x00000000
#define CG6_FBC_BREAD_0			0x00000200
#define CG6_FBC_BREAD_1			0x00000400
#define CG6_FBC_BREAD_ILLEGAL		0x00000600
#define CG6_FBC_BREAD_MASK		0x00000600

#define CG6_FBC_BDISP_IGNORE		0x00000000
#define CG6_FBC_BDISP_0			0x00000080
#define CG6_FBC_BDISP_1			0x00000100
#define CG6_FBC_BDISP_ILLEGAL		0x00000180
#define CG6_FBC_BDISP_MASK		0x00000180

#define CG6_FBC_INDEX_MOD		0x00000040
#define CG6_FBC_INDEX_MASK		0x00000030

/* rasterops */
#define GX_ROP_CLEAR        0x0
#define GX_ROP_INVERT       0x1
#define GX_ROP_NOOP         0x2
#define GX_ROP_SET          0x3

#define GX_ROP_00_0(rop)    ((rop) << 0)
#define GX_ROP_00_1(rop)    ((rop) << 2)
#define GX_ROP_01_0(rop)    ((rop) << 4)
#define GX_ROP_01_1(rop)    ((rop) << 6)
#define GX_ROP_10_0(rop)    ((rop) << 8)
#define GX_ROP_10_1(rop)    ((rop) << 10)
#define GX_ROP_11_0(rop)    ((rop) << 12)
#define GX_ROP_11_1(rop)    ((rop) << 14)
#define GX_PLOT_PLOT        0x00000000
#define GX_PLOT_UNPLOT      0x00020000
#define GX_RAST_BOOL        0x00000000
#define GX_RAST_LINEAR      0x00040000
#define GX_ATTR_UNSUPP      0x00400000
#define GX_ATTR_SUPP        0x00800000
#define GX_POLYG_OVERLAP    0x01000000
#define GX_POLYG_NONOVERLAP 0x02000000
#define GX_PATTERN_ZEROS    0x04000000
#define GX_PATTERN_ONES     0x08000000
#define GX_PATTERN_MASK     0x0c000000
#define GX_PIXEL_ZEROS      0x10000000
#define GX_PIXEL_ONES       0x20000000
#define GX_PIXEL_MASK       0x30000000
#define GX_PLANE_ZEROS      0x40000000
#define GX_PLANE_ONES       0x80000000
#define GX_PLANE_MASK       0xc0000000
/* rops for bit blit / copy area
   with:
       Plane Mask - use plane mask reg.
       Pixel Mask - use all ones.
       Patt  Mask - use all ones.
*/

#define POLY_O          GX_POLYG_OVERLAP
#define POLY_N          GX_POLYG_NONOVERLAP

#define ROP_STANDARD    (GX_PLANE_MASK |\
                        GX_PIXEL_ONES |\
                        GX_ATTR_SUPP |\
                        GX_RAST_BOOL |\
                        GX_PLOT_PLOT)

/* fg = don't care  bg = don't care */

#define ROP_BLIT(O,I)   (ROP_STANDARD | \
                        GX_PATTERN_ONES |\
                        GX_ROP_11_1(I) |\
                        GX_ROP_11_0(O) |\
                        GX_ROP_10_1(I) |\
                        GX_ROP_10_0(O) |\
                        GX_ROP_01_1(I) |\
                        GX_ROP_01_0(O) |\
                        GX_ROP_00_1(I) |\
                        GX_ROP_00_0(O))

/* fg = fgPixel     bg = don't care */

#define ROP_FILL(O,I)   (ROP_STANDARD | \
                        GX_PATTERN_ONES |\
                        GX_ROP_11_1(I) |\
                        GX_ROP_11_0(I) |\
                        GX_ROP_10_1(I) |\
                        GX_ROP_10_0(I) | \
                        GX_ROP_01_1(O) |\
                        GX_ROP_01_0(O) |\
                        GX_ROP_00_1(O) |\
                        GX_ROP_00_0(O))

/* fg = fgPixel     bg = don't care */
 
#define ROP_STIP(O,I)   (ROP_STANDARD |\
                        GX_ROP_11_1(I) |\
                        GX_ROP_11_0(GX_ROP_NOOP) |\
                        GX_ROP_10_1(I) |\
                        GX_ROP_10_0(GX_ROP_NOOP) | \
                        GX_ROP_01_1(O) |\
                        GX_ROP_01_0(GX_ROP_NOOP) |\
                        GX_ROP_00_1(O) |\
                        GX_ROP_00_0(GX_ROP_NOOP))

/* fg = fgPixel     bg = bgPixel */
                            
#define ROP_OSTP(O,I)   (ROP_STANDARD |\
                        GX_ROP_11_1(I) |\
                        GX_ROP_11_0(I) |\
                        GX_ROP_10_1(I) |\
                        GX_ROP_10_0(O) |\
                        GX_ROP_01_1(O) |\
                        GX_ROP_01_0(I) |\
                        GX_ROP_00_1(O) |\
                        GX_ROP_00_0(O))

#define GX_ROP_USE_PIXELMASK    0x30000000

#define GX_BLT_INPROGRESS       0x20000000

/* status register(s) */
#define GX_EXCEPTION		0x80000000
#define GX_TEC_EXCEPTION	0x40000000
#define GX_FULL                 0x20000000
#define GX_INPROGRESS           0x10000000
#define GX_UNSUPPORTED_ATTR	0x02000000
#define GX_HRMONO		0x01000000
#define GX_OVERFLOW		0x00200000
#define GX_PICK			0x00100000
#define GX_TEC_HIDDEN		0x00040000
#define GX_TEC_INTERSECT	0x00020000
#define GX_TEC_VISIBLE		0x00010000
#define GX_BLIT_HARDWARE	0x00008000	/* hardware can blit this */
#define GX_BLIT_SOFTWARE	0x00004000	/* software must blit this */
#define GX_BLIT_SRC_HIDDEN	0x00002000
#define GX_BLIT_SRC_INTERSECT	0x00001000
#define GX_BLIT_SRC_VISIBLE	0x00000800
#define GX_BLIT_DST_HIDDEN	0x00000400
#define GX_BLIT_DST_INTERSECT	0x00000200
#define GX_BLIT_DST_VISIBLE	0x00000100
#define GX_DRAW_HARDWARE	0x00000010	/* hardware can draw this */
#define GX_DRAW_SOFTAWRE	0x00000008	/* software must draw this */
#define GX_DRAW_HIDDEN		0x00000004
#define GX_DRAW_INTERSECT	0x00000002
#define GX_DRAW_VISIBLE		0x00000001

/* MISC register */
#define GX_INDEX(n)         ((n) << 4)
#define GX_INDEX_ALL        0x00000030
#define GX_INDEX_MOD        0x00000040
#define GX_BDISP_0          0x00000080
#define GX_BDISP_1          0x00000100
#define GX_BDISP_ALL        0x00000180
#define GX_BREAD_0          0x00000200
#define GX_BREAD_1          0x00000400
#define GX_BREAD_ALL        0x00000600
#define GX_BWRITE1_ENABLE   0x00000800
#define GX_BWRITE1_DISABLE  0x00001000
#define GX_BWRITE1_ALL      0x00001800
#define GX_BWRITE0_ENABLE   0x00002000
#define GX_BWRITE0_DISABLE  0x00004000
#define GX_BWRITE0_ALL      0x00006000
#define GX_DRAW_RENDER      0x00008000
#define GX_DRAW_PICK        0x00010000
#define GX_DRAW_ALL         0x00018000
#define GX_MODE_COLOR8      0x00020000
#define GX_MODE_COLOR1      0x00040000
#define GX_MODE_HRMONO      0x00060000
#define GX_MODE_ALL         0x00060000
#define GX_VBLANK           0x00080000
#define GX_BLIT_NOSRC       0x00100000
#define GX_BLIT_SRC         0x00200000
#define GX_BLIT_ALL         0x00300000

#if _CG6_LAYOUT_NOT_USED_ANYMORE
/*
 * This structure exists only to compute the layout of the CG6
 * hardware.  Each of the individual substructures lives on a
 * separate `page' (where a `page' is at least 4K), and many are
 * very far apart.  We avoid large offsets (which make for lousy
 * code) by using pointers to the individual interesting pieces,
 * and map them in independently (to avoid using up PTEs unnecessarily).
 */
struct cg6_layout {
	/* ROM at 0 */
	union {
		int un_id;		/* ID = ?? */
		char un_rom[65536];	/* 64K rom */
		char un_pad[0x200000];
	} cg6_rom_un;

	/* Brooktree DAC at 0x200000 */
	union {
		struct bt_regs un_btregs;
		char un_pad[0x040000];
	} cg6_bt_un;

	/* DHC, whatever that is, at 0x240000 */
	union {
		char un_pad[0x40000];
	} cg6_dhc_un;

	/* ALT, whatever that is, at 0x280000 */
	union {
		char un_pad[0x80000];
	} cg6_alt_un;

	/* FHC register at 0x300000 */
	union {
		int un_fhc;
		char un_pad[0x1000];
	} cg6_fhc_un;

	/* THC at 0x301000 */
	union {
		struct cg6_thc un_thc;
		char un_pad[0x400000 - 0x1000];
	} cg6_thc_un;

	/* FBC at 0x700000 */
	union {
		char un_pad[0x1000];
	} cg6_fbc_un;

	/* TEC at 0x701000 */
	union {
		char un_pad[0x100000 - 0x1000];
		struct cg6_tec_xxx un_tec;
	} cg6_tec_un;

	/* Video RAM at 0x800000 */
	char	cg6_ram[1024 * 1024];	/* approx.? */
};
#endif
