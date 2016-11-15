/*	$NetBSD: r128fbreg.h,v 1.5 2012/01/06 13:59:50 macallan Exp $	*/

/*
 * Copyright 1999, 2000 ATI Technologies Inc., Markham, Ontario,
 *                      Precision Insight, Inc., Cedar Park, Texas, and
 *                      VA Linux Systems Inc., Fremont, California.
 *
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation on the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON-INFRINGEMENT.  IN NO EVENT SHALL ATI, PRECISION INSIGHT, VA LINUX
 * SYSTEMS AND/OR THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

/*
 * Authors:
 *   Rickard E. Faith <faith@valinux.com>
 *   Kevin E. Martin <martin@valinux.com>
 *   Gareth Hughes <gareth@valinux.com>
 *
 * References:
 *
 *   RAGE 128 VR/ RAGE 128 GL Register Reference Manual (Technical
 *   Reference Manual P/N RRG-G04100-C Rev. 0.04), ATI Technologies: April
 *   1999.
 *
 *   RAGE 128 Software Development Manual (Technical Reference Manual P/N
 *   SDK-G04000 Rev. 0.01), ATI Technologies: June 1999.
 *
 */

/*
 * register definitions for ATI Rage 128 graphics controllers
 * mostly from XFree86's ati driver
 */
 

#ifndef R128FB_REG_H
#define R128FB_REG_H

/* RAMDAC */
#define R128_PALETTE_DATA		0x00b4
#define R128_PALETTE_INDEX		0x00b0

/* flat panel registers */
#define R128_FP_PANEL_CNTL		0x0288
	#define FPCNT_DIGON		  0x00000001	/* FP dig. voltage */
	#define FPCNT_BACKLIGHT_ON	  0x00000002
	#define FPCNT_BL_MODULATION_ON	  0x00000004
	#define FPCNT_BL_CLK_SEL	  0x00000008	/* 1 - divide by 3 */
	#define FPCNT_MONID_EN		  0x00000010	/* use MONID pins for
							   backlight control */
	#define FPCNT_FPENABLE_POL	  0x00000020	/* 1 - active low */
	#define FPCNT_LEVEL_MASK	  0x0000ff00
	#define FPCNT_LEVEL_SHIFT	  8

#define R128_LVDS_GEN_CNTL                0x02d0
#       define R128_LVDS_ON               (1   <<  0)
#       define R128_LVDS_DISPLAY_DIS      (1   <<  1)
#       define R128_LVDS_EN               (1   <<  7)
#       define R128_LVDS_DIGON            (1   << 18)
#       define R128_LVDS_BLON             (1   << 19)
#       define R128_LVDS_SEL_CRTC2        (1   << 23)
#       define R128_HSYNC_DELAY_SHIFT     28
#       define R128_HSYNC_DELAY_MASK      (0xf << 28)
#	define R128_LEVEL_MASK	          0x0000ff00
#	define R128_LEVEL_SHIFT	          8

/* drawing engine */
#define R128_PC_NGUI_CTLSTAT              0x0184
#       define R128_PC_FLUSH_GUI          (3 << 0)
#       define R128_PC_RI_GUI             (1 << 2)
#       define R128_PC_FLUSH_ALL          0x00ff
#       define R128_PC_BUSY               (1 << 31)

#define R128_CRTC_OFFSET                  0x0224

#define R128_DST_OFFSET                   0x1404
#define R128_DST_PITCH                    0x1408

#define R128_DP_GUI_MASTER_CNTL           0x146c
#       define R128_GMC_SRC_PITCH_OFFSET_CNTL (1    <<  0)
#       define R128_GMC_DST_PITCH_OFFSET_CNTL (1    <<  1)
#       define R128_GMC_SRC_CLIPPING          (1    <<  2)
#       define R128_GMC_DST_CLIPPING          (1    <<  3)
#       define R128_GMC_BRUSH_DATATYPE_MASK   (0x0f <<  4)
#       define R128_GMC_BRUSH_8X8_MONO_FG_BG  (0    <<  4)
#       define R128_GMC_BRUSH_8X8_MONO_FG_LA  (1    <<  4)
#       define R128_GMC_BRUSH_1X8_MONO_FG_BG  (4    <<  4)
#       define R128_GMC_BRUSH_1X8_MONO_FG_LA  (5    <<  4)
#       define R128_GMC_BRUSH_32x1_MONO_FG_BG (6    <<  4)
#       define R128_GMC_BRUSH_32x1_MONO_FG_LA (7    <<  4)
#       define R128_GMC_BRUSH_32x32_MONO_FG_BG (8    <<  4)
#       define R128_GMC_BRUSH_32x32_MONO_FG_LA (9    <<  4)
#       define R128_GMC_BRUSH_8x8_COLOR       (10   <<  4)
#       define R128_GMC_BRUSH_1X8_COLOR       (12   <<  4)
#       define R128_GMC_BRUSH_SOLID_COLOR     (13   <<  4)
#       define R128_GMC_BRUSH_NONE            (15   <<  4)
#       define R128_GMC_DST_8BPP_CI           (2    <<  8)
#       define R128_GMC_DST_15BPP             (3    <<  8)
#       define R128_GMC_DST_16BPP             (4    <<  8)
#       define R128_GMC_DST_24BPP             (5    <<  8)
#       define R128_GMC_DST_32BPP             (6    <<  8)
#       define R128_GMC_DST_8BPP_RGB          (7    <<  8)
#       define R128_GMC_DST_Y8                (8    <<  8)
#       define R128_GMC_DST_RGB8              (9    <<  8)
#       define R128_GMC_DST_VYUY              (11   <<  8)
#       define R128_GMC_DST_YVYU              (12   <<  8)
#       define R128_GMC_DST_AYUV444           (14   <<  8)
#       define R128_GMC_DST_ARGB4444          (15   <<  8)
#       define R128_GMC_DST_DATATYPE_MASK     (0x0f <<  8)
#       define R128_GMC_DST_DATATYPE_SHIFT    8
#       define R128_GMC_SRC_DATATYPE_MASK       (3    << 12)
#       define R128_GMC_SRC_DATATYPE_MONO_FG_BG (0    << 12)
#       define R128_GMC_SRC_DATATYPE_MONO_FG_LA (1    << 12)
#       define R128_GMC_SRC_DATATYPE_COLOR      (3    << 12)
#       define R128_GMC_BYTE_PIX_ORDER        (1    << 14)
#       define R128_GMC_BYTE_MSB_TO_LSB       (0    << 14)
#       define R128_GMC_BYTE_LSB_TO_MSB       (1    << 14)
#       define R128_GMC_CONVERSION_TEMP       (1    << 15)
#       define R128_GMC_CONVERSION_TEMP_6500  (0    << 15)
#       define R128_GMC_CONVERSION_TEMP_9300  (1    << 15)
#       define R128_GMC_ROP3_MASK             (0xff << 16)
#       define R128_DP_SRC_SOURCE_MASK        (7    << 24)
#       define R128_DP_SRC_SOURCE_MEMORY      (2    << 24)
#       define R128_DP_SRC_SOURCE_HOST_DATA   (3    << 24)
#       define R128_DP_SRC_SOURCE_HOST_ALIGN  (4    << 24)
#       define R128_GMC_3D_FCN_EN             (1    << 27)
#       define R128_GMC_CLR_CMP_CNTL_DIS      (1    << 28)
#       define R128_GMC_AUX_CLIP_DIS          (1    << 29)
#       define R128_GMC_WR_MSK_DIS            (1    << 30)
#       define R128_GMC_LD_BRUSH_Y_X          (1    << 31)
#       define R128_ROP3_ZERO             0x00000000
#       define R128_ROP3_DSa              0x00880000
#       define R128_ROP3_SDna             0x00440000
#       define R128_ROP3_S                0x00cc0000
#       define R128_ROP3_DSna             0x00220000
#       define R128_ROP3_D                0x00aa0000
#       define R128_ROP3_DSx              0x00660000
#       define R128_ROP3_DSo              0x00ee0000
#       define R128_ROP3_DSon             0x00110000
#       define R128_ROP3_DSxn             0x00990000
#       define R128_ROP3_Dn               0x00550000
#       define R128_ROP3_SDno             0x00dd0000
#       define R128_ROP3_Sn               0x00330000
#       define R128_ROP3_DSno             0x00bb0000
#       define R128_ROP3_DSan             0x00770000
#       define R128_ROP3_ONE              0x00ff0000
#       define R128_ROP3_DPa              0x00a00000
#       define R128_ROP3_PDna             0x00500000
#       define R128_ROP3_P                0x00f00000
#       define R128_ROP3_DPna             0x000a0000
#       define R128_ROP3_D                0x00aa0000
#       define R128_ROP3_DPx              0x005a0000
#       define R128_ROP3_DPo              0x00fa0000
#       define R128_ROP3_DPon             0x00050000
#       define R128_ROP3_PDxn             0x00a50000
#       define R128_ROP3_PDno             0x00f50000
#       define R128_ROP3_Pn               0x000f0000
#       define R128_ROP3_DPno             0x00af0000
#       define R128_ROP3_DPan             0x005f0000

#define R128_DP_BRUSH_BKGD_CLR            0x1478
#define R128_DP_BRUSH_FRGD_CLR            0x147c
#define R128_SRC_X_Y                      0x1590
#define R128_DST_X_Y                      0x1594
#define R128_DST_WIDTH_HEIGHT             0x1598

#define R128_SRC_OFFSET                   0x15ac
#define R128_SRC_PITCH                    0x15b0

#define R128_AUX_SC_CNTL                  0x1660
#       define R128_AUX1_SC_EN            (1 << 0)
#       define R128_AUX1_SC_MODE_OR       (0 << 1)
#       define R128_AUX1_SC_MODE_NAND     (1 << 1)
#       define R128_AUX2_SC_EN            (1 << 2)
#       define R128_AUX2_SC_MODE_OR       (0 << 3)
#       define R128_AUX2_SC_MODE_NAND     (1 << 3)
#       define R128_AUX3_SC_EN            (1 << 4)
#       define R128_AUX3_SC_MODE_OR       (0 << 5)
#       define R128_AUX3_SC_MODE_NAND     (1 << 5)

#define R128_DP_CNTL                      0x16c0
#       define R128_DST_X_LEFT_TO_RIGHT   (1 <<  0)
#       define R128_DST_Y_TOP_TO_BOTTOM   (1 <<  1)

#define R128_DP_DATATYPE                  0x16c4
#       define R128_HOST_BIG_ENDIAN_EN    (1 << 29)

#define R128_DP_MIX                       0x16c8
#	define R128_MIX_SRC_VRAM		  (2 << 8)
#	define R128_MIX_SRC_HOSTDATA		  (3 << 8)
#	define R128_MIX_SRC_HOST_BYTEALIGN	  (4 << 8)
#	define R128_MIX_SRC_ROP3_MASK		  (0xff << 16)

#define R128_DP_WRITE_MASK                0x16cc
#define R128_DP_SRC_BKGD_CLR              0x15dc
#define R128_DP_SRC_FRGD_CLR              0x15d8

#define R128_DP_CNTL_XDIR_YDIR_YMAJOR     0x16d0
#       define R128_DST_Y_MAJOR             (1 <<  2)
#       define R128_DST_Y_DIR_TOP_TO_BOTTOM (1 << 15)
#       define R128_DST_X_DIR_LEFT_TO_RIGHT (1 << 31)

#define R128_DEFAULT_OFFSET               0x16e0
#define R128_DEFAULT_PITCH                0x16e4
#define R128_DEFAULT_SC_BOTTOM_RIGHT      0x16e8
#       define R128_DEFAULT_SC_RIGHT_MAX  (0x1fff <<  0)
#       define R128_DEFAULT_SC_BOTTOM_MAX (0x1fff << 16)

/* scissor registers */
#define R128_SC_BOTTOM                    0x164c
#define R128_SC_BOTTOM_RIGHT              0x16f0
#define R128_SC_BOTTOM_RIGHT_C            0x1c8c
#define R128_SC_LEFT                      0x1640
#define R128_SC_RIGHT                     0x1644
#define R128_SC_TOP                       0x1648
#define R128_SC_TOP_LEFT                  0x16ec
#define R128_SC_TOP_LEFT_C                0x1c88

#define R128_GUI_STAT                     0x1740
#       define R128_GUI_FIFOCNT_MASK      0x0fff
#       define R128_GUI_ACTIVE            (1 << 31)

#define R128_HOST_DATA0                   0x17c0
#define R128_HOST_DATA1                   0x17c4
#define R128_HOST_DATA2                   0x17c8
#define R128_HOST_DATA3                   0x17cc
#define R128_HOST_DATA4                   0x17d0
#define R128_HOST_DATA5                   0x17d4
#define R128_HOST_DATA6                   0x17d8
#define R128_HOST_DATA7                   0x17dc

/* Information the firmware is supposed to leave for us */
#define R128_BIOS_5_SCRATCH               0x0024
#       define R128_BIOS_DISPLAY_FP       (1 << 0)
#       define R128_BIOS_DISPLAY_CRT      (2 << 0)
#       define R128_BIOS_DISPLAY_FP_CRT   (3 << 0)

/* Clock stuff */
#define R128_CLOCK_CNTL_INDEX             0x0008
#       define R128_PLL_WR_EN             (1 << 7)
#       define R128_PLL_DIV_SEL           (3 << 8)
#       define R128_PLL2_DIV_SEL_MASK     ~(3 << 8)
#define R128_CLOCK_CNTL_DATA              0x000c

#define R128_CLK_PIN_CNTL                 0x0001 /* PLL */
#define R128_PPLL_CNTL                    0x0002 /* PLL */
#       define R128_PPLL_RESET                (1 <<  0)
#       define R128_PPLL_SLEEP                (1 <<  1)
#       define R128_PPLL_ATOMIC_UPDATE_EN     (1 << 16)
#       define R128_PPLL_VGA_ATOMIC_UPDATE_EN (1 << 17)
#define R128_PPLL_REF_DIV                 0x0003 /* PLL */
#       define R128_PPLL_REF_DIV_MASK     0x03ff
#       define R128_PPLL_ATOMIC_UPDATE_R  (1 << 15) /* same as _W */
#       define R128_PPLL_ATOMIC_UPDATE_W  (1 << 15) /* same as _R */
#define R128_PPLL_DIV_0                   0x0004 /* PLL */
#define R128_PPLL_DIV_1                   0x0005 /* PLL */
#define R128_PPLL_DIV_2                   0x0006 /* PLL */
#define R128_PPLL_DIV_3                   0x0007 /* PLL */
#       define R128_PPLL_FB3_DIV_MASK     0x07ff
#       define R128_PPLL_POST3_DIV_MASK   0x00070000
#define R128_VCLK_ECP_CNTL                0x0008 /* PLL */
#       define R128_VCLK_SRC_SEL_MASK     0x03
#       define R128_VCLK_SRC_SEL_CPUCLK   0x00
#       define R128_VCLK_SRC_SEL_PPLLCLK  0x03
#       define R128_ECP_DIV_MASK          (3 << 8)
#define R128_HTOTAL_CNTL                  0x0009 /* PLL */
#define R128_X_MPLL_REF_FB_DIV            0x000a /* PLL */
#define R128_XPLL_CNTL                    0x000b /* PLL */
#define R128_XDLL_CNTL                    0x000c /* PLL */
#define R128_XCLK_CNTL                    0x000d /* PLL */
#define R128_FCP_CNTL                     0x0012 /* PLL */

#define R128_P2PLL_CNTL                    0x002a /* P2PLL */
#       define R128_P2PLL_RESET               (1 <<  0)
#       define R128_P2PLL_SLEEP               (1 <<  1)
#       define R128_P2PLL_ATOMIC_UPDATE_EN    (1 << 16)
#       define R128_P2PLL_VGA_ATOMIC_UPDATE_EN (1 << 17)
#       define R128_P2PLL_ATOMIC_UPDATE_VSYNC  (1 << 18)
#define R128_P2PLL_REF_DIV                 0x002B /* PLL */
#       define R128_P2PLL_REF_DIV_MASK     0x03ff
#       define R128_P2PLL_ATOMIC_UPDATE_R  (1 << 15) /* same as _W */
#       define R128_P2PLL_ATOMIC_UPDATE_W  (1 << 15) /* same as _R */
#define R128_P2PLL_DIV_0                   0x002c
#       define R128_P2PLL_FB0_DIV_MASK     0x07ff
#       define R128_P2PLL_POST0_DIV_MASK   0x00070000
#define R128_V2CLK_VCLKTV_CNTL            0x002d /* PLL */
#       define R128_V2CLK_SRC_SEL_MASK    0x03
#       define R128_V2CLK_SRC_SEL_CPUCLK  0x00
#       define R128_V2CLK_SRC_SEL_P2PLLCLK 0x03
#define R128_HTOTAL2_CNTL                 0x002e /* PLL */

/* CTRCs */
#define R128_CRTC_GEN_CNTL                0x0050
#       define R128_CRTC_DBL_SCAN_EN      (1 <<  0)
#       define R128_CRTC_INTERLACE_EN     (1 <<  1)
#       define R128_CRTC_CSYNC_EN         (1 <<  4)
#	define R128_CRTC_PIX_WIDTH	  (7 <<  8)
#	define R128_CRTC_COLOR_8BIT	  (2 <<  8)
#	define R128_CRTC_COLOR_15BIT	  (3 <<  8)
#	define R128_CRTC_COLOR_16BIT	  (4 <<  8)
#	define R128_CRTC_COLOR_24BIT	  (5 <<  8)
#	define R128_CRTC_COLOR_32BIT	  (6 <<  8)
#       define R128_CRTC_CUR_EN           (1 << 16)
#       define R128_CRTC_CUR_MODE_MASK    (7 << 17)
#       define R128_CRTC_ICON_EN          (1 << 20)
#       define R128_CRTC_EXT_DISP_EN      (1 << 24)
#       define R128_CRTC_EN               (1 << 25)
#       define R128_CRTC_DISP_REQ_EN_B    (1 << 26)
#define R128_CRTC_EXT_CNTL                0x0054
#       define R128_CRTC_VGA_XOVERSCAN    (1 <<  0)
#       define R128_VGA_ATI_LINEAR        (1 <<  3)
#       define R128_XCRT_CNT_EN           (1 <<  6)
#       define R128_CRTC_HSYNC_DIS        (1 <<  8)
#       define R128_CRTC_VSYNC_DIS        (1 <<  9)
#       define R128_CRTC_DISPLAY_DIS      (1 << 10)
#       define R128_CRTC_CRT_ON           (1 << 15)
#       define R128_FP_OUT_EN             (1 << 22)
#       define R128_FP_ACTIVE             (1 << 23)
#define R128_CRTC_EXT_CNTL_DPMS_BYTE      0x0055
#       define R128_CRTC_HSYNC_DIS_BYTE   (1 <<  0)
#       define R128_CRTC_VSYNC_DIS_BYTE   (1 <<  1)
#       define R128_CRTC_DISPLAY_DIS_BYTE (1 <<  2)
#define R128_CRTC_STATUS                  0x005c
#       define R128_CRTC_VBLANK_SAVE      (1 <<  1)

#define R128_CRTC_H_TOTAL_DISP            0x0200
#       define R128_CRTC_H_TOTAL          (0x01ff << 0)
#       define R128_CRTC_H_TOTAL_SHIFT    0
#       define R128_CRTC_H_DISP           (0x00ff << 16)
#       define R128_CRTC_H_DISP_SHIFT     16
#define R128_CRTC_H_SYNC_STRT_WID         0x0204
#       define R128_CRTC_H_SYNC_STRT_PIX        (0x07  <<  0)
#       define R128_CRTC_H_SYNC_STRT_CHAR       (0x1ff <<  3)
#       define R128_CRTC_H_SYNC_STRT_CHAR_SHIFT 3
#       define R128_CRTC_H_SYNC_WID             (0x3f  << 16)
#       define R128_CRTC_H_SYNC_WID_SHIFT       16
#       define R128_CRTC_H_SYNC_POL             (1     << 23)
#define R128_CRTC_V_TOTAL_DISP            0x0208
#       define R128_CRTC_V_TOTAL          (0x07ff << 0)
#       define R128_CRTC_V_TOTAL_SHIFT    0
#       define R128_CRTC_V_DISP           (0x07ff << 16)
#       define R128_CRTC_V_DISP_SHIFT     16
#define R128_CRTC_V_SYNC_STRT_WID         0x020c
#       define R128_CRTC_V_SYNC_STRT       (0x7ff <<  0)
#       define R128_CRTC_V_SYNC_STRT_SHIFT 0
#       define R128_CRTC_V_SYNC_WID        (0x1f  << 16)
#       define R128_CRTC_V_SYNC_WID_SHIFT  16
#       define R128_CRTC_V_SYNC_POL        (1     << 23)
#define R128_CRTC_VLINE_CRNT_VLINE        0x0210
#       define R128_CRTC_CRNT_VLINE_MASK  (0x7ff << 16)
#define R128_CRTC_CRNT_FRAME              0x0214
#define R128_CRTC_GUI_TRIG_VLINE          0x0218
#define R128_CRTC_DEBUG                   0x021c
#define R128_CRTC_OFFSET                  0x0224
#define R128_CRTC_OFFSET_CNTL             0x0228
#define R128_CRTC_PITCH                   0x022c

#define R128_CRTC2_H_TOTAL_DISP           0x0300
#       define R128_CRTC2_H_TOTAL          (0x01ff << 0)
#       define R128_CRTC2_H_TOTAL_SHIFT    0
#       define R128_CRTC2_H_DISP           (0x00ff << 16)
#       define R128_CRTC2_H_DISP_SHIFT     16
#define R128_CRTC2_H_SYNC_STRT_WID        0x0304
#       define R128_CRTC2_H_SYNC_STRT_PIX        (0x07  <<  0)
#       define R128_CRTC2_H_SYNC_STRT_CHAR       (0x1ff <<  3)
#       define R128_CRTC2_H_SYNC_STRT_CHAR_SHIFT 3
#       define R128_CRTC2_H_SYNC_WID             (0x3f  << 16)
#       define R128_CRTC2_H_SYNC_WID_SHIFT       16
#       define R128_CRTC2_H_SYNC_POL             (1     << 23)
#define R128_CRTC2_V_TOTAL_DISP           0x0308
#       define R128_CRTC2_V_TOTAL          (0x07ff << 0)
#       define R128_CRTC2_V_TOTAL_SHIFT    0
#       define R128_CRTC2_V_DISP           (0x07ff << 16)
#       define R128_CRTC2_V_DISP_SHIFT     16
#define R128_CRTC2_V_SYNC_STRT_WID        0x030c
#       define R128_CRTC2_V_SYNC_STRT       (0x7ff <<  0)
#       define R128_CRTC2_V_SYNC_STRT_SHIFT 0
#       define R128_CRTC2_V_SYNC_WID        (0x1f  << 16)
#       define R128_CRTC2_V_SYNC_WID_SHIFT  16
#       define R128_CRTC2_V_SYNC_POL        (1     << 23)
#define R128_CRTC2_VLINE_CRNT_VLINE       0x0310
#define R128_CRTC2_CRNT_FRAME             0x0314
#define R128_CRTC2_GUI_TRIG_VLINE         0x0318
#define R128_CRTC2_DEBUG                  0x031c
#define R128_CRTC2_OFFSET                 0x0324
#define R128_CRTC2_OFFSET_CNTL            0x0328
#	define R128_CRTC2_TILE_EN         (1 << 15)
#define R128_CRTC2_PITCH                  0x032c
#define R128_CRTC2_GEN_CNTL               0x03f8
#       define R128_CRTC2_DBL_SCAN_EN      (1 <<  0)
#       define R128_CRTC2_CUR_EN           (1 << 16)
#       define R128_CRTC2_ICON_EN          (1 << 20)
#       define R128_CRTC2_DISP_DIS         (1 << 23)
#       define R128_CRTC2_EN               (1 << 25)
#       define R128_CRTC2_DISP_REQ_EN_B    (1 << 26)
#define R128_CRTC2_STATUS                 0x03fc

#endif /* R128FB_REG_H */
