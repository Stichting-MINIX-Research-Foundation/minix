/* $NetBSD: vbe.c,v 1.8 2013/05/31 15:11:07 tsutsui Exp $ */

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

/*
 * VESA BIOS Extensions routines
 */

#include <lib/libsa/stand.h>
#include <lib/libkern/libkern.h>
#include <machine/bootinfo.h>
#include "libi386.h"
#include "vbe.h"

extern const uint8_t rasops_cmap[];
static uint8_t *vbe_edid = NULL;
static int vbe_edid_valid = 0;

static struct _vbestate {
	int		available;
	int		modenum;
} vbestate;

static int
vbe_mode_is_supported(struct modeinfoblock *mi)
{
	if ((mi->ModeAttributes & 0x01) == 0)
		return 0;	/* mode not supported by hardware */
	if ((mi->ModeAttributes & 0x08) == 0)
		return 0;	/* linear fb not available */
	if ((mi->ModeAttributes & 0x10) == 0)
		return 0;	/* text mode */
	if (mi->NumberOfPlanes != 1)
		return 0;	/* planar mode not supported */
	if (mi->MemoryModel != 0x04 /* Packed pixel */ &&
	    mi->MemoryModel != 0x06 /* Direct Color */)
		return 0;	/* unsupported pixel format */
	return 1;
}

static bool
vbe_check(void)
{
	if (!vbestate.available) {
		printf("VBE not available\n");
		return false;
	}
	return true;
}

void
vbe_init(void)
{
	struct vbeinfoblock vbe;

	memset(&vbe, 0, sizeof(vbe));
	memcpy(vbe.VbeSignature, "VBE2", 4);
	if (biosvbe_info(&vbe) != 0x004f)
		return;
	if (memcmp(vbe.VbeSignature, "VESA", 4) != 0)
		return;

	vbestate.available = 1;
	vbestate.modenum = 0;
}

int
vbe_available(void)
{
	return vbestate.available;
}

int
vbe_set_palette(const uint8_t *cmap, int slot)
{
	struct paletteentry pe;
	int ret;

	if (!vbe_check())
		return 1;

	pe.Blue = cmap[2] >> 2;
	pe.Green = cmap[1] >> 2;
	pe.Red = cmap[0] >> 2;
	pe.Alignment = 0;

	ret = biosvbe_palette_data(0x0600, slot, &pe);

	return ret == 0x004f ? 0 : 1;
}

int
vbe_set_mode(int modenum)
{
	struct modeinfoblock mi;
	struct btinfo_framebuffer fb;
	int ret, i;

	if (!vbe_check())
		return 1;

	ret = biosvbe_get_mode_info(modenum, &mi);
	if (ret != 0x004f) {
		printf("mode 0x%x invalid\n", modenum);
		return 1;
	}

	if (!vbe_mode_is_supported(&mi)) {
		printf("mode 0x%x not supported\n", modenum);
		return 1;
	}

	ret = biosvbe_set_mode(modenum);
	if (ret != 0x004f) {
		printf("mode 0x%x could not be set\n", modenum);
		return 1;
	}

	/* Setup palette for packed pixel mode */
	if (mi.MemoryModel == 0x04)
		for (i = 0; i < 256; i++)
			vbe_set_palette(&rasops_cmap[i * 3], i);

	fb.physaddr = (uint64_t)mi.PhysBasePtr & 0xffffffff;
	fb.width = mi.XResolution;
	fb.height = mi.YResolution;
	fb.stride = mi.BytesPerScanLine;
	fb.depth = mi.BitsPerPixel;
	fb.flags = 0;
	fb.rnum = mi.RedMaskSize;
	fb.rpos = mi.RedFieldPosition;
	fb.gnum = mi.GreenMaskSize;
	fb.gpos = mi.GreenFieldPosition;
	fb.bnum = mi.BlueMaskSize;
	fb.bpos = mi.BlueFieldPosition;
	fb.vbemode = modenum;

	framebuffer_configure(&fb);

	return 0;
}

int
vbe_commit(void)
{
	int ret = 1;

	if (vbestate.modenum > 0) {
		ret = vbe_set_mode(vbestate.modenum);
		if (ret) {
			printf("WARNING: failed to set VBE mode 0x%x\n",
			    vbestate.modenum);
			wait_sec(5);
		}
	}
	return ret;
}

static void *
vbe_farptr(uint32_t farptr)
{
	return VBEPHYPTR((((farptr & 0xffff0000) >> 12) + (farptr & 0xffff)));
}

static int
vbe_parse_mode_str(char *str, int *x, int *y, int *depth)
{
	char *p;

	p = str;
	*x = strtoul(p, NULL, 0);
	if (*x == 0)
		return 0;
	p = strchr(p, 'x');
	if (!p)
		return 0;
	++p;
	*y = strtoul(p, NULL, 0);
	if (*y == 0)
		return 0;
	p = strchr(p, 'x');
	if (!p)
		*depth = 8;
	else {
		++p;
		*depth = strtoul(p, NULL, 0);
		if (*depth == 0)
			return 0;
	}

	return 1;
}

static int
vbe_find_mode_xyd(int x, int y, int depth)
{
	struct vbeinfoblock vbe;
	struct modeinfoblock mi;
	uint32_t farptr;
	uint16_t mode;
	int safety = 0;

	memset(&vbe, 0, sizeof(vbe));
	memcpy(vbe.VbeSignature, "VBE2", 4);
	if (biosvbe_info(&vbe) != 0x004f)
		return 0;
	if (memcmp(vbe.VbeSignature, "VESA", 4) != 0)
		return 0;
	farptr = vbe.VideoModePtr;
	if (farptr == 0)
		return 0;

	while ((mode = *(uint16_t *)vbe_farptr(farptr)) != 0xffff) {
		safety++;
		farptr += 2;
		if (safety == 100)
			return 0;
		if (biosvbe_get_mode_info(mode, &mi) != 0x004f)
			continue;
		/* we only care about linear modes here */
		if (vbe_mode_is_supported(&mi) == 0)
			continue;
		safety = 0;
		if (mi.XResolution == x &&
		    mi.YResolution == y &&
		    mi.BitsPerPixel == depth)
			return mode;
	}

	return 0;
}

static int
vbe_find_mode(char *str)
{
	int x, y, depth;

	if (!vbe_parse_mode_str(str, &x, &y, &depth))
		return 0;

	return vbe_find_mode_xyd(x, y, depth);
}

static void
vbe_dump_mode(int modenum, struct modeinfoblock *mi)
{
	printf("0x%x=%dx%dx%d", modenum,
	    mi->XResolution, mi->YResolution, mi->BitsPerPixel);
}

static int
vbe_get_edid(int *pwidth, int *pheight)
{
	const uint8_t magic[] = EDID_MAGIC;
	int ddc_caps, ret;

	ddc_caps = biosvbe_ddc_caps();
	if (ddc_caps == 0) {
		return 1;
	}

	if (vbe_edid == NULL) {
		vbe_edid = alloc(128);
	}
	if (vbe_edid_valid == 0) {
		ret = biosvbe_ddc_read_edid(0, vbe_edid);
		if (ret != 0x004f)
			return 1;
		if (memcmp(vbe_edid, magic, sizeof(magic)) != 0)
			return 1;
		vbe_edid_valid = 1;
	}

	*pwidth = vbe_edid[EDID_DESC_BLOCK + 2] |
	    (((int)vbe_edid[EDID_DESC_BLOCK + 4] & 0xf0) << 4);
	*pheight = vbe_edid[EDID_DESC_BLOCK + 5] |
	    (((int)vbe_edid[EDID_DESC_BLOCK + 7] & 0xf0) << 4);

	return 0;
}

void
vbe_modelist(void)
{
	struct vbeinfoblock vbe;
	struct modeinfoblock mi;
	uint32_t farptr;
	uint16_t mode;
	int nmodes = 0, safety = 0;
	int ddc_caps, edid_width, edid_height;

	if (!vbe_check())
		return;

	ddc_caps = biosvbe_ddc_caps();
	if (ddc_caps & 3) {
		printf("DDC");
		if (ddc_caps & 1)
			printf(" [DDC1]");
		if (ddc_caps & 2)
			printf(" [DDC2]");

		if (vbe_get_edid(&edid_width, &edid_height) != 0)
			printf(": no EDID information\n");
		else
			printf(": EDID %dx%d\n", edid_width, edid_height);
	}

	printf("Modes: ");
	memset(&vbe, 0, sizeof(vbe));
	memcpy(vbe.VbeSignature, "VBE2", 4);
	if (biosvbe_info(&vbe) != 0x004f)
		goto done;
	if (memcmp(vbe.VbeSignature, "VESA", 4) != 0)
		goto done;
	farptr = vbe.VideoModePtr;
	if (farptr == 0)
		goto done;

	while ((mode = *(uint16_t *)vbe_farptr(farptr)) != 0xffff) {
		safety++;
		farptr += 2;
		if (safety == 100) {
			printf("[?] ");
			break;
		}
		if (biosvbe_get_mode_info(mode, &mi) != 0x004f)
			continue;
		/* we only care about linear modes here */
		if (vbe_mode_is_supported(&mi) == 0)
			continue;
		safety = 0;
		if (nmodes % 4 == 0)
			printf("\n");
		else
			printf("  ");
		vbe_dump_mode(mode, &mi);
		nmodes++;
	}

done:
	if (nmodes == 0)
		printf("none found");
	printf("\n");
}

void
command_vesa(char *cmd)
{
	char arg[20];
	int modenum, edid_width, edid_height;

	if (!vbe_check())
		return;

	strlcpy(arg, cmd, sizeof(arg));

	if (strcmp(arg, "list") == 0) {
		vbe_modelist();
		return;
	}

	if (strcmp(arg, "disabled") == 0 || strcmp(arg, "off") == 0) {
		vbestate.modenum = 0;
		return;
	}

	if (strcmp(arg, "enabled") == 0 || strcmp(arg, "on") == 0) {
		if (vbe_get_edid(&edid_width, &edid_height) != 0) {
			modenum = VBE_DEFAULT_MODE;
		} else {
			modenum = vbe_find_mode_xyd(edid_width, edid_height, 8);
			if (modenum == 0)
				modenum = VBE_DEFAULT_MODE;
		}
	} else if (strncmp(arg, "0x", 2) == 0) {
		modenum = strtoul(arg, NULL, 0);
	} else if (strchr(arg, 'x') != NULL) {
		modenum = vbe_find_mode(arg);
		if (modenum == 0) {
			printf("mode %s not supported by firmware\n", arg);
			return;
		}
	} else {
		modenum = 0;
	}

	if (modenum >= 0x100) {
		vbestate.modenum = modenum;
		return;
	}

	printf("invalid flag, must be 'on', 'off', 'list', "
	    "a display mode, or a VBE mode number\n");
}
