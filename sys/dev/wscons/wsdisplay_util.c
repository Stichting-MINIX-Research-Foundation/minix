/*	$NetBSD: wsdisplay_util.c,v 1.2 2013/01/31 10:57:31 macallan Exp $ */

/*-
 * Copyright (c) 2011 Michael Lorenz
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

/* some utility functions for use with wsdisplay */

#include <sys/param.h>
#include <sys/stdint.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/device.h>
#include <dev/cons.h>

#include <dev/wscons/wsdisplayvar.h>
#include <dev/rasops/rasops.h>
#include <dev/wscons/wsconsio.h>

int
wsdisplayio_get_edid(device_t dev, struct wsdisplayio_edid_info *d)
{
	prop_data_t edid_data;
	int edid_size;

	edid_data = prop_dictionary_get(device_properties(dev), "EDID");
	if (edid_data != NULL) {
		edid_size = prop_data_size(edid_data);
		/* less than 128 bytes is bogus */
		if (edid_size < 128)
			return ENODEV;
		d->data_size = edid_size;
		if (d->buffer_size < edid_size)
			return EAGAIN;
		return copyout(prop_data_data_nocopy(edid_data),
			       d->edid_data, edid_size);
	}
	return ENODEV;
}

/* convenience function to fill in stuff from rasops_info */
int
wsdisplayio_get_fbinfo(struct rasops_info *ri, struct wsdisplayio_fbinfo *fbi)
{
	fbi->fbi_width = ri->ri_width;
	fbi->fbi_height = ri->ri_height;
	fbi->fbi_stride = ri->ri_stride;
	fbi->fbi_bitsperpixel = ri->ri_depth;
	if (ri->ri_depth > 8) {
		fbi->fbi_pixeltype = WSFB_RGB;
		fbi->fbi_subtype.fbi_rgbmasks.red_offset = ri->ri_rpos;
		fbi->fbi_subtype.fbi_rgbmasks.red_size = ri->ri_rnum;
		fbi->fbi_subtype.fbi_rgbmasks.green_offset = ri->ri_gpos;
		fbi->fbi_subtype.fbi_rgbmasks.green_size = ri->ri_gnum;
		fbi->fbi_subtype.fbi_rgbmasks.blue_offset = ri->ri_bpos;
		fbi->fbi_subtype.fbi_rgbmasks.blue_size = ri->ri_bnum;
		fbi->fbi_subtype.fbi_rgbmasks.alpha_offset = 0;
		fbi->fbi_subtype.fbi_rgbmasks.alpha_size = 0;
	} else {
		fbi->fbi_pixeltype = WSFB_CI;
		fbi->fbi_subtype.fbi_cmapinfo.cmap_entries = 1 << ri->ri_depth;
	}
	fbi->fbi_flags = 0;
	fbi->fbi_fbsize = ri->ri_stride * ri->ri_height;
	fbi->fbi_fboffset = 0;
	return 0;
}
