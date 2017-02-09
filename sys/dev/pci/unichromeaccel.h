/* $NetBSD: unichromeaccel.h,v 1.1 2006/08/13 20:26:55 jmcneill Exp $ */

/*
 * Copyright 1998-2006 VIA Technologies, Inc. All Rights Reserved.
 * Copyright 2001-2006 S3 Graphics, Inc. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sub license,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHOR(S) OR COPYRIGHT HOLDER(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef _DEV_PCI_UNICHROMEACCEL_H
#define _DEV_PCI_UNICHROMEACCEL_H

/* To be included in fb.h */
#ifndef FB_ACCEL_VIA_UNICHROME
#define FB_ACCEL_VIA_UNICHROME  50
#endif

/* MMIO Base Address Definition */
#define MMIO_VGABASE                0x8000
#define MMIO_CR_READ                MMIO_VGABASE + 0x3D4
#define MMIO_CR_WRITE               MMIO_VGABASE + 0x3D5
#define MMIO_SR_READ                MMIO_VGABASE + 0x3C4
#define MMIO_SR_WRITE               MMIO_VGABASE + 0x3C5

#define MMIO_OUT8(reg, val)	\
	bus_space_write_1(sc->sc_memt, sc->sc_memh, reg, val)
#define MMIO_OUT16(reg, val)	\
	bus_space_write_2(sc->sc_memt, sc->sc_memh, reg, val)
#define MMIO_OUT32(reg, val)	\
	bus_space_write_4(sc->sc_memt, sc->sc_memh, reg, val)
#define MMIO_IN8(reg)		\
	bus_space_read_1(sc->sc_memt, sc->sc_memh, reg)
#define MMIO_IN16(reg)		\
	bus_space_read_2(sc->sc_memt, sc->sc_memh, reg)
#define MMIO_IN32(reg)		\
	bus_space_read_4(sc->sc_memt, sc->sc_memh, reg)

/* HW Cursor Status Define */
#define HW_Cursor_ON    0
#define HW_Cursor_OFF   1

/* Initial HW cursor flag */
#if 0
static int MAX_CURS = 32;
static int HW_Cursor_Init = 1;
#endif

#define CURSOR_SIZE     (8 * 1024)
#define VQ_SIZE         (256 * 1024)

#define VIA_MMIO_BLTBASE        0x200000
#define VIA_MMIO_BLTSIZE        0x200000

/* Defines for 2D registers */
#define VIA_REG_GECMD           0x000
#define VIA_REG_GEMODE          0x004
#define VIA_REG_SRCPOS          0x008
#define VIA_REG_DSTPOS          0x00C
#define VIA_REG_DIMENSION       0x010       /* width and height */
#define VIA_REG_PATADDR         0x014
#define VIA_REG_FGCOLOR         0x018
#define VIA_REG_BGCOLOR         0x01C
#define VIA_REG_CLIPTL          0x020       /* top and left of clipping */
#define VIA_REG_CLIPBR          0x024       /* bottom and right of clipping */
#define VIA_REG_OFFSET          0x028
#define VIA_REG_KEYCONTROL      0x02C       /* color key control */
#define VIA_REG_SRCBASE         0x030
#define VIA_REG_DSTBASE         0x034
#define VIA_REG_PITCH           0x038       /* pitch of src and dst */
#define VIA_REG_MONOPAT0        0x03C
#define VIA_REG_MONOPAT1        0x040
#define VIA_REG_COLORPAT        0x100       /* from 0x100 to 0x1ff */

/* VIA_REG_PITCH(0x38): Pitch Setting */
#define VIA_PITCH_ENABLE        0x80000000

/* defines for VIA HW cursor registers */
#define VIA_REG_CURSOR_MODE     0x2D0
#define VIA_REG_CURSOR_POS      0x2D4
#define VIA_REG_CURSOR_ORG      0x2D8
#define VIA_REG_CURSOR_BG       0x2DC
#define VIA_REG_CURSOR_FG       0x2E0

/* VIA_REG_GEMODE(0x04): GE mode */
#define VIA_GEM_8bpp            0x00000000
#define VIA_GEM_16bpp           0x00000100
#define VIA_GEM_32bpp           0x00000300

/* defines for VIA 3D registers */
#define VIA_REG_STATUS          0x400
#define VIA_REG_TRANSET         0x43C
#define VIA_REG_TRANSPACE       0x440

/* VIA_REG_STATUS(0x400): Engine Status */
#define VIA_CMD_RGTR_BUSY       0x00000080  /* Command Regulator is busy */
#define VIA_2D_ENG_BUSY         0x00000001  /* 2D Engine is busy */
#define VIA_3D_ENG_BUSY         0x00000002  /* 3D Engine is busy */
#define VIA_VR_QUEUE_BUSY       0x00020000  /* Virtual Queue is busy */

#define MAXLOOP                 0xFFFFFF

#endif /* _DEV_PCI_UNICHROMEACCEL_H */
