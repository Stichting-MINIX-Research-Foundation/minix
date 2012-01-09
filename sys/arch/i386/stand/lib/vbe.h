/* $NetBSD: vbe.h,v 1.3 2011/02/09 04:37:54 jmcneill Exp $ */

/*-
 * Copyright (c) 2009 Jared D. McNeill <jmcneill@invisible.ca>
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#define VBE_DEFAULT_MODE	0x101	/* 640x480x8 */

struct vbeinfoblock {
	char VbeSignature[4];
	uint16_t VbeVersion;
	uint32_t OemStringPtr;
	uint32_t Capabilities;
	uint32_t VideoModePtr;
	uint16_t TotalMemory;
	uint16_t OemSoftwareRev;
	uint32_t OemVendorNamePtr, OemProductNamePtr, OemProductRevPtr;
	/* data area, in total max 512 bytes for VBE 2.0 */
	uint8_t Reserved[222];
	uint8_t OemData[256];
} __packed;

struct modeinfoblock {
	/* Mandatory information for all VBE revisions */
	uint16_t ModeAttributes;
	uint8_t WinAAttributes, WinBAttributes;
	uint16_t WinGranularity, WinSize, WinASegment, WinBSegment;
	uint32_t WinFuncPtr;
	uint16_t BytesPerScanLine;
	/* Mandatory information for VBE 1.2 and above */
	uint16_t XResolution, YResolution;
	uint8_t XCharSize, YCharSize, NumberOfPlanes, BitsPerPixel;
	uint8_t NumberOfBanks, MemoryModel, BankSize, NumberOfImagePages;
	uint8_t Reserved1;
	/* Direct Color fields
	   (required for direct/6 and YUV/7 memory models) */
	uint8_t RedMaskSize, RedFieldPosition;
	uint8_t GreenMaskSize, GreenFieldPosition;
	uint8_t BlueMaskSize, BlueFieldPosition;
	uint8_t RsvdMaskSize, RsvdFieldPosition;
	uint8_t DirectColorModeInfo;
	/* Mandatory information for VBE 2.0 and above */
	uint32_t PhysBasePtr;
	uint32_t OffScreenMemOffset;	/* reserved in VBE 3.0 and above */
	uint16_t OffScreenMemSize;	/* reserved in VBE 3.0 and above */

	/* Mandatory information for VBE 3.0 and above */
	uint16_t LinBytesPerScanLine;
	uint8_t BnkNumberOfImagePages;
	uint8_t LinNumberOfImagePages;
	uint8_t LinRedMaskSize, LinRedFieldPosition;
	uint8_t LinGreenMaskSize, LinGreenFieldPosition;
	uint8_t LinBlueMaskSize, LinBlueFieldPosition;
	uint8_t LinRsvdMaskSize, LinRsvdFieldPosition;
	uint32_t MaxPixelClock;
	uint8_t Reserved4[189];
} __packed;

struct paletteentry {
	uint8_t Blue;
	uint8_t Green;
	uint8_t Red;
	uint8_t Alignment;
} __packed;

/* EDID */
#define	EDID_MAGIC	{ 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00 }
#define	EDID_DESC_BLOCK	0x36

/* low-level VBE calls, from biosvbe.S */
int biosvbe_info(struct vbeinfoblock *);
int biosvbe_set_mode(int);
int biosvbe_get_mode_info(int, struct modeinfoblock *);
int biosvbe_palette_format(int); 
int biosvbe_palette_data(int, int, struct paletteentry *);
int biosvbe_ddc_caps(void);
int biosvbe_ddc_read_edid(int, void *);

/* high-level VBE helpers, from vbe.c */
void vbe_init(void);
int vbe_commit(void);
int vbe_available(void);
int vbe_set_mode(int);
int vbe_set_palette(const uint8_t *, int);
void vbe_modelist(void);

void command_vesa(char *);

/* adjust physical address; boot code runs with %ds having a 64k offset */
#define VBEPHYPTR(x)	((uint8_t *)((x) - (64 * 1024)))
