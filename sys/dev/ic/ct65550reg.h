/*	$NetBSD: ct65550reg.h,v 1.2 2011/03/23 04:02:43 macallan Exp $	*/

/*
 * Copyright 2006 by Michael Lorenz.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
*/

#ifndef CHIPSFB_H
#define CHIPSFB_H

/* VGA */
#define CRTC_INDEX	0x3d4
#define CRTC_DATA	0x3d5
#define SEQ_INDEX	0x3c4
#define SEQ_DATA	0x3c5
#define MISC_W		0x3c2
#define GRA_INDEX	0x3ce
#define GRA_DATA	0x3cf
#define ATT_IW		0x3c0

/* palette */
#define CT_DACMASK	0x3c6
#define CT_DACSTATE	0x3c7	/* read only */
#define CT_READINDEX	0x3c7	/* write only */
#define CT_WRITEINDEX	0x3c8
#define CT_DACDATA	0x3c9

/* extended VGA */
#define CT_FP_INDEX	0x3d0
#define CT_FP_DATA	0x3d1
#define CT_MM_INDEX	0x3d2
#define CT_MM_DATA	0x3d3
#define CT_CONF_INDEX	0x3d6
#define CT_CONF_DATA	0x3d7

/* offsets in aperture */
#define CT_OFF_FB	0x00000000
#define CT_OFF_BITBLT	0x00400000
#define CT_OFF_DRAW	0x00400040
#define CT_OFF_DATA	0x00410000

#define CT_OFF_BE	0x00800000

/* blitter registers */
#define CT_BLT_STRIDE	0x00000000
	/* 
	 * upper 16 bit are destination stride in bytes
	 * lower 16 bit are source stride in bytes
	 */
	 
#define CT_BLT_BG	0x04
#define CT_BLT_FG	0x08
#define CT_BLT_EXPCTL	0x0c	/* expansion control */
	#define LEFT_CLIPPING_MSK	0x0000003f
	#define MONO_RIGHT_CLIPPING_MSK	0x00003f00
	#define MONO_INITIAL_DISCARD	0x003f0000
	#define MONO_SRC_ALIGN_MASK	0x07000000
	#define MONO_SRC_ALIGN_BIT	0x01000000
	#define MONO_SRC_ALIGN_BYTE	0x02000000
	#define MONO_SRC_ALIGN_WORD	0x03000000
	#define MONO_SRC_ALIGN_LONG	0x04000000
	#define MONO_SRC_ALIGN_LONGLONG	0x05000000
	#define MONO_SELECT_ALT_FG_BG	0x08000000 /* use CT_SRC_EXP_* */
	
#define CT_BLT_CONTROL	0x10
	#define BLT_ROP_MASK		0x000000ff
	#define BLT_START_RIGHT		0x00000100 /* 0 for start left */
	#define BLT_START_BOTTOM	0x00000200 /* 0 for start top */
	#define BLT_SRC_IS_CPU		0x00000400 /* 0 for vram source */
	#define BLT_SRC_IS_MONO		0x00001000
	#define BLT_MONO_TRANSPARENCY	0x00002000
	#define BLT_COLOR_COMPARE_MASK	0x0001c000 /* 0 for no color keying */
	#define BLT_PAT_TRANSPARENCY	0x00020000 /* pattern is transparent */
	#define BLT_PAT_IS_MONO		0x00040000
	#define BLT_PAT_IS_SOLID	0x00080000 /* ignore pattern */
	#define BLT_PAT_VERT_ALIGN_MASK	0x00700000
	#define BLT_IS_BUSY		0x80000000

#define ROP_COPY	0xcc
#define ROP_NOT_SRC	0x33
#define ROP_NOT_DST	0x55
#define ROP_PAT		0xf0

#define CT_BLT_PATTERN	0x14 /* address in vram */
#define CT_BLT_SRCADDR	0x18
#define CT_BLT_DSTADDR	0x1c

#define CT_BLT_SIZE	0x20	/* width and height */
/*
 * upper 16 bit are destination height
 * lower 16 bit are destination width in bytes
 */

#define CT_SRC_EXP_BG	0x24
#define CT_SRC_EXP_FG	0x28

/* extension registers ( via CT_CONF */
#define XR_VENDOR_LO		0x00
#define XR_VENDOR_HI		0x01
#define XR_DEVICE_LO		0x02
#define XR_DEVICE_HI		0x03
#define XR_REVISION		0x04
#define XR_LINEAR_BASE_LO	0x05
#define XR_LINEAR_BASE_HI	0x06

#define XR_CONFIGURATION	0x08
	#define BUS_PCI		0x01
	#define BUS_VL		0x00
	#define ENABLE_PCI	0x02

#define XR_IO_CONTROL		0x09
	#define ENABLE_CRTC_EXT	0x01
	#define ENABLE_ATTR_EXT	0x02
	
#define XR_ADDR_MAPPING		0x0a
	#define ENABLE_MAPPING	0x01	/* in VGA window */
	#define ENABLE_LINEAR	0x02
	#define ENABLE_PACKED	0x04
	#define FB_SWAP_NONE	0x00
	#define FB_SWAP_16	0x10
	#define FB_SWAP_32	0x20

#define XR_BURST_WRITE_MODE	0x0b

#define XR_PAGE_SELECT		0x0e

#define XR_BITBLT_CONTROL0	0x20
	#define BLITTER_BUSY	0x01
	#define BLITTER_RESET	0x02
	#define BLITTER_8BIT	0x00
	#define BLITTER_16BIT	0x10
	#define BLITTER_24BIT	0x20
	#define BLITTER_32BIT	0x30	/* reserved */
	
#define XR_DRAM_ACCESS_CONTROL	0x40
	#define	ENABLE_64BIT	0x01
	#define DISABLE_WRAP	0x02	/* otherwise only 256kB */
	#define EXTENDED_TEXT	0x10
	
#define XR_DRAM_TYPE		0x41
	#define DRAM_FASTPAGE	0x00
	#define DRAM_EDO	0x01
	
#define XR_DRAM_CONFIG		0x42
	#define DRAM_8BIT_COL	0x00
	#define DRAM_9BIT_COL	0x01
	
#define XR_DRAM_INTERFACE	0x43
#define XR_DRAM_TIMING		0x44

#define XR_VIDEO_PIN_CONTROL	0x60
#define XR_DDC_SYNC_SELECT	0x61
	#define DDC_HSYNC_DATA	0x01
	#define DDC_HSYNC_OUT	0x02	/* hsync is controlled by above */
	#define DDC_VSYNC_DATA	0x04
	#define DDC_VSYNC_OUT	0x08	/* vsync is controlled by above */
	#define DDC_HV_POWERDOWN 0x10
	#define DDC_ENABLE_HSYNC 0x20
	#define DDC_ENABLE_VSYNC 0x40
	
/*
 * upper 6 bit define if corresponding bits in DATA are input or output 
 * 1 selects output
 */
#define XR_GPIO_CONTROL		0x62	
#define XR_GPIO_DATA		0x63

#define XR_PIN_TRISTATE_CONTROL	0x67

#define XR_CONFIG_PINS_0	0x70
#define XR_CONFIG_PINS_1	0x71

#define XR_PIXEL_PIPELINE_CTL_0	0x80
	#define ENABLE_EXTENDED_PALETTE	0x01
	#define ENABLE_CRT_OVERSCAN	0x02
	#define ENABLE_PANEL_OVERSCAN	0x04
	#define ENABLE_EXTENDED_STATUS	0x08
	#define ENABLE_CURSOR_1		0x10
	#define ENABLE_PIXEL_AVERAGING	0x20
	#define SELECT_PIXEL_STREAM	0x40	/* 1 for P1 */
	#define ENABLE_8BIT_DAC		0x80	/* 6 bit otherwise */
	
#define XR_PIXEL_PIPELINE_CTL_1	0x81
	#define COLOR_VGA		0x00
	#define COLOR_8BIT_EXTENDED	0x02
	#define COLOR_15BIT		0x04
	#define COLOR_16BIT		0x05
	#define COLOR_24BIT		0x06
	#define COLOR_32BIT		0x07
	
#define XR_PIXEL_PIPELINE_CTL_2	0x82
	#define ENABLE_BLANK_PEDESTAL	0x01
	#define ENABLE_SYNC_ON_GREEN	0x02
	#define ENABLE_VIDEO_GAMMA	0x04
	#define ENABLE_GRAPHICS_GAMMA	0x08
	
#define XR_CURSOR_1_CTL		0xa0
#define XR_CURSOR_1_VERT_EXT	0xa1
#define XR_CURSOR_1_BASEADDR_LO	0xa2
#define XR_CURSOR_1_BASEADDR_HI	0xa3
#define XR_CURSOR_1_X_LO	0xa4
#define XR_CURSOR_1_X_HI	0xa5
#define XR_CURSOR_1_Y_LO	0xa6
#define XR_CURSOR_1_Y_HI	0xa7

#define XR_CURSOR_2_CTL		0xa8
#define XR_CURSOR_2_VERT_EXT	0xa9
#define XR_CURSOR_2_BASEADDR_LO	0xaa
#define XR_CURSOR_2_BASEADDR_HI	0xab
#define XR_CURSOR_2_X_LO	0xac
#define XR_CURSOR_2_X_HI	0xad
#define XR_CURSOR_2_Y_LO	0xae
#define XR_CURSOR_2_Y_HI	0xaf

#define XR_VCLOCK_0_M		0xc0
#define XR_VCLOCK_0_N		0xc1
#define XR_VCLOCK_0_MN_MSBS	0xc2
#define XR_VCLOCK_0_DIV_SELECT	0xc3

#define XR_VCLOCK_1_M		0xc4
#define XR_VCLOCK_1_N		0xc5
#define XR_VCLOCK_1_MN_MSBS	0xc6
#define XR_VCLOCK_1_DIV_SELECT	0xc7

#define XR_VCLOCK_2_M		0xc8
#define XR_VCLOCK_2_N		0xc9
#define XR_VCLOCK_2_MN_MSBS	0xca
#define XR_VCLOCK_2_DIV_SELECT	0xcb

#define XR_MEMCLOCK_M		0xcc
#define XR_MEMCLOCK_N		0xcd
#define XR_MEMCLOCK_DIV_SELECT	0xce
#define XR_CLOCK_CONFIG		0xcf

#define XR_MODULE_POWER_DOWN	0xd0
#define XR_DOWN_COUNTER		0xd2

#define XR_SOFTWARE_FLAG_0	0xe0
#define XR_SOFTWARE_FLAG_1	0xe1
#define XR_SOFTWARE_FLAG_2	0xe2
#define XR_SOFTWARE_FLAG_3	0xe3
#define XR_SOFTWARE_FLAG_4	0xe4
#define XR_SOFTWARE_FLAG_5	0xe5
#define XR_SOFTWARE_FLAG_6	0xe6
#define XR_SOFTWARE_FLAG_7	0xe7

#define XR_TEST_BLOCK_SELECT	0xf8
#define XR_TEST_CONTROL_PORT	0xf9
#define XR_TEST_DATA_PORT	0xfa
#define XR_SCAN_TEST_CONTROL_0	0xfb
#define XR_SCAN_TEST_CONTROL_1	0xfc

/* flat panel control registers, via CT_FP_* */
#define FP_FEATURE		0x00
	#define PANEL_EXISTS	0x01
	#define POPUP_EXISTS	0x04
	
#define FP_CRT_FP_CONTROL	0x01
	#define ENABLE_CRT	0x01
	#define ENABLE_PANEL	0x02
	
#define FP_MODE_CONTROL		0x02
#define FP_DOT_CLOCK_SOURCE	0x03
	#define FP_CLOCK_0	0x00
	#define FP_CLOCK_1	0x04
	#define FP_CLOCK_2	0x08
	#define USE_VIDEO_CLOCK	0x00
	#define USE_MEM_CLOCK	0x10
	
#define FP_POWER_SEQ_DELAY	0x04
/*
 * upper 4 bits select power up delay in 3.4ms increments
 * lower 4 bits select power down delay in 29ms increments
 */

#define FP_POWER_DOWN_CTL_1	0x05
/* the lower 3 bits select how many refresh cycles per scanline are preformed */
	#define PANEL_POWER_OFF	0x08
	#define HOST_STANDBY	0x10
	#define PANEL_TRISTATE	0x20
	#define NO_SEFL_REFRESH	0x40
	#define PANEL_INACTIVE	0x80

/* these bits are effective when the panel is powered down */
#define FP_POWER_DOWN_CTL_0	0x06
	#define FP_VGA_PALETTE_POWERDOWN	0x01
	#define FP_VGA_PALETTE_ENABLE		0x02
	#define FP_ENABLE_SYNC			0x04
	
#define FP_PIN_POLARITY		0x08
	#define FP_DISPLAY_NEGATIVE	0x02
	#define FP_HSYNC_NEGATIVE	0x04
	#define FP_VSYNC_NEGATIVE	0x08
	#define FP_TEXT_VIDEO_INVERT	0x10
	#define FP_GRAPHICS_INVERT	0x20
	#define CRT_HSYNC_NEGATIVE	0x40
	#define CRT_VSYNC_NEGATIVE	0x80
	
#define FP_OUTPUT_DRIVE		0x0a
	#define VL_THRESHOLD_5V		0x02	/* 3.3v otherwise */
	#define FP_DRIVE_HIGH		0x04	/* req. with 3.3v */
	#define BUS_INTERFACE_LOW	0x08	/* req. with 3.3v */
	#define MEM_DRIVE_HIGHER	0x10
	#define MEM_C_DRIVE_HIGHER	0x20
	#define SYNC_DRIVE_HIGHER	0x40
	
#define FP_PIN_CONTROL_1	0x0b
	#define DISPLAY_ENABLE_ON_69	0x01	/* M signal otherwise */
	#define DISPLAY_ENABLE_ON_68	0x02	/* FP Hsync otherwise */
	#define COMPOSITE_SYNC_ON_65	0x04	/* separate otherwise */
	#define BACKLIGHT_ON_61		0x08	/* on 54 otherwise */
	#define GPIO_ON_154		0x10
	#define SIMPLE_COMPOSITE_SYNC	0x20
	#define MEM_C_TRISTATE		0x80
	
#define FP_PIN_CONTROL_2	0x0c
	#define ACTI_ON_53		0x00
	#define COMPOSITE_SYNC_ON_53	0x08
	#define GPIO_IN_ON_53		0x10
	#define GPIO_OUT_ON_53		0x18
	#define ENABKL_ON_54		0x00
	#define COMPOSITE_SYNC_ON_54	0x40
	#define GPIO_IN_ON_54		0x80
	#define GPIO_OUT_ON_54		0xc0
	
#define FP_ACTIVITY_CONTROL	0x0f
/* the lower 5 bits select a timeout in 28.1s increments */
	#define PANEL_OFF_ON_TIMEOUT	0x40 /* backlight off otherwise */
	#define ENABLE_ACTIVITY_TIMER	0x80
	
#define FP_PANEL_FORMAT_0	0x10
	#define SINGLE_PANEL_SINGLE_DRIVE	0x00
	#define DUAL_PANEL_DUAL_DRIVE		0x03
	#define MONO_NTSC			0x00
	#define MONO_EQUIV_WEIGHT		0x04
	#define MONO_GREEN_ONLY			0x08
	#define COLOUR_PANEL			0x0c
	#define SHIFT_CLOCK_DIVIDER_MASK	0x70
	
#define FP_PANEL_FORMAT_1	0x11
	
#define FP_PANEL_FORMAT_2	0x12
#define FP_PANEL_FORMAT_3	0x13

#define FP_FRC_OPTION_SELECT	0x16
#define FP_POLYNOMIAL_FRC_CTL	0x17

#define FP_TEXTMODE_CONTROL	0x18
#define FP_BLINK_RATE_CONTROL	0x19
#define FP_FB_CONTROL		0x1a

#define FP_ACDCLK_CONTROL	0x1e
#define FP_DIAGNOSTIC		0x1f

#define FP_HSIZE_LSB		0x20 /* panel size - 1 */
#define FP_HSYNC_START		0x21 /* value - 1 */
#define FP_HSYNC_END		0x22
#define FP_HTOTAL_LSB		0x23 /* value - 5 */
#define FP_HSYNC_DELAY_LSB	0x24
#define FP_HORZ_OVERFLOW_1	0x25
/*
 * upper 4 bits are upper 4 bits of FP_HSYNC_START
 * lower 4 bits are upper 4 bits of FP_HSIZE_LSB
 */
 
#define FP_HORZ_OVERFLOW_2	0x26
/*
 * upper 4 bits are upper 4 bits of FP_HSYNC_DELAY_LSB
 * lower 4 bits are upper 4 bits of FP_HTOTAL_LSB
 */
 
#define FP_HSYNC_WIDTH_DISABLE	0x27
/* lower 7 bits are HSYNC width - 1 */
	#define DELAY_DISABLE	0x80
	
#define FP_VSIZE_LSB		0x30 /* panel size - 1 */
#define FP_VSYNC_START		0x31 /* value - 1 */
#define FP_VSYNC_END		0x32 /* value - 1 */
#define FP_VTOTAL_LSB		0x33 /* value - 2 */
#define FP_VSYNC_DELAY_LSB	0x34 /* value - 1 */
#define FP_VERT_OVERFLOW_1	0x35
/*
 * upper 4 bits are upper 4 bits of FP_VSYNC_START
 * lower 4 bits are upper 4 bits of FP_VSIZE_LSB
 */
 
#define FP_VERT_OVERFLOW_2	0x36
/*
 * upper 4 bits are upper 4 bits of FP_VSYNC_DELAY_LSB
 * lower 4 bits are upper 4 bits of FP_VTOTAL_LSB
 */

#define FP_VSYNC_DISABLE	0x37
	#define FP_VSYNC_WIDTH_MASK	0x38 /* value - 1 */
	#define FP_VSYNC_IS_CRT_VSYNC	0x40
	#define FP_VSYNC_DELAY_DISABLE	0x80
	
#define FP_HORZ_COMPENSATION	0x40
#define FP_VERT_COMPENSATION	0x41
#define FP_VERT_COMPENSATION2	0x48

#define FP_TEXT_VSTRETCH_0_MSB	0x49
#define FP_TEXT_VSTRETCH_0_LSB	0x4a
#define FP_TEXT_VSTRETCH_1_MSB	0x4b
#define FP_TEXT_VSTRETCH_1_LSB	0x4c
#define FP_TEXT_LINE_REPL	0x4d
#define FP_SEL_VSTRETCH_DISABLE	0x4e



#endif
