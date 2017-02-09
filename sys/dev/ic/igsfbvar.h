/*	$NetBSD: igsfbvar.h,v 1.20 2011/07/26 08:59:37 mrg Exp $ */

/*
 * Copyright (c) 2002, 2003 Valeriy E. Ushakov
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
 */

/*
 * Integraphics Systems IGA 168x and CyberPro series.
 */
#ifndef _DEV_IC_IGSFBVAR_H_
#define _DEV_IC_IGSFBVAR_H_

#include <dev/videomode/videomode.h>

#define	IGS_CMAP_SIZE	256	/* 256 R/G/B entries */
struct igs_hwcmap {
	uint8_t r[IGS_CMAP_SIZE];
	uint8_t g[IGS_CMAP_SIZE];
	uint8_t b[IGS_CMAP_SIZE];
};


#define IGS_CURSOR_MAX_SIZE 64	/* 64x64 sprite */
struct igs_hwcursor {
	struct wsdisplay_curpos cc_pos;
	struct wsdisplay_curpos cc_hot;
	struct wsdisplay_curpos cc_size;
	uint8_t cc_image[512];		/* save copy of image for GCURSOR */
	uint8_t cc_mask[512];		/* save copy of mask for GCURSOR */
	uint16_t cc_sprite[512];	/* sprite in device 2bpp format */
	uint8_t cc_color[6];		/* 2 colors, 3 rgb components */
};


struct igsfb_devconfig {
	/* io space, may be memory mapped */
	bus_space_tag_t dc_iot;
	bus_addr_t dc_iobase;
	int dc_ioflags;

	/* io registers */
	bus_space_handle_t dc_ioh;

	/* graphic coprocessor */
	bus_space_handle_t dc_coph;

	/* linear memory */
	bus_space_tag_t dc_memt;
	bus_addr_t dc_memaddr;
	bus_size_t dc_memsz; /* size of linear address space including mmio */
	int dc_memflags;

	/* video memory size */
	bus_size_t dc_vmemsz;

	/* resolution */
	int dc_width, dc_height, dc_depth, dc_stride;
	int dc_maxdepth;
	const struct videomode *dc_mode;

	char dc_modestring[128];

	/* part of video memory mapped for wsscreen */
	bus_space_handle_t dc_fbh;
	bus_size_t dc_fbsz;

	/* 1KB of cursor sprite data */
	bus_space_handle_t dc_crh;

	/* product id: IGA 168x, CyberPro 2k &c */
	int dc_id;

	/* flags that control driver operation */
	int dc_hwflags;
#define IGSFB_HW_BSWAP			0x1 /* endianness mismatch */
#define IGSFB_HW_BE_SELECT		0x2 /* big endian magic (cyberpro) */
#define IGSFB_HW_TEXT_CURSOR		0x4 /* do text cursor in hardware */

/* do we need to do bswap in software? */
#define IGSFB_HW_SOFT_BSWAP(dc)						\
	((((dc)->dc_hwflags) & (IGSFB_HW_BSWAP | IGSFB_HW_BE_SELECT))	\
		== IGSFB_HW_BSWAP)

	int dc_blanked;			/* screen is currently blanked */
	int dc_curenb;			/* cursor sprite enabled */
	int dc_mapped;			/* currently in mapped mode */

	/* saved dc_ri.ri_ops.putchar */
	void (*dc_ri_putchar)(void *, int, int, u_int, long);

	/* optional MD mmap() method */
	paddr_t (*dc_mmap)(void *, void *, off_t, int);

	struct igs_hwcmap dc_cmap;	/* software copy of colormap */
	struct igs_hwcursor dc_cursor;	/* software copy of cursor sprite */

	/* precomputed bit table for cursor sprite 1bpp -> 2bpp conversion */
	uint16_t dc_bexpand[256];

	/* virtual console support */
	struct vcons_data dc_vd;
	struct vcons_screen dc_console;
};


struct igsfb_softc {
	device_t sc_dev;
	struct igsfb_devconfig *sc_dc;
};



/*
 * Access sugar for indexed registers
 */

static __inline uint8_t
igs_idx_read(bus_space_tag_t, bus_space_handle_t, u_int, uint8_t);
static __inline void
igs_idx_write(bus_space_tag_t, bus_space_handle_t, u_int, uint8_t, uint8_t);

static __inline uint8_t
igs_idx_read(bus_space_tag_t t, bus_space_handle_t h,
	     u_int idxport, uint8_t idx)
{
	bus_space_write_1(t, h, idxport, idx);
	return (bus_space_read_1(t, h, idxport + 1));
}

static __inline void
igs_idx_write(bus_space_tag_t t, bus_space_handle_t h,
	      u_int idxport, uint8_t idx, uint8_t val)
{
	bus_space_write_1(t, h, idxport, idx);
	bus_space_write_1(t, h, idxport + 1, val);
}


/* sugar for sequencer controller */
#define igs_seq_read(t,h,x)	\
	(igs_idx_read((t),(h),IGS_SEQ_IDX,(x)))
#define igs_seq_write(t,h,x,v)	\
	(igs_idx_write((t),(h),IGS_SEQ_IDX,(x),(v)))


/* sugar for CRT controller */
#define igs_crtc_read(t,h,x)	\
	(igs_idx_read((t),(h),IGS_CRTC_IDX,(x)))
#define igs_crtc_write(t,h,x,v)	\
	(igs_idx_write((t),(h),IGS_CRTC_IDX,(x),(v)))


/* sugar for attribute controller */
#define igs_attr_flip_flop(t,h)	\
	((void)bus_space_read_1((t),(h),IGS_INPUT_STATUS1));
#define igs_attr_read(t,h,x)	\
	(igs_idx_read((t),(h),IGS_ATTR_IDX,(x)))

static __inline void
igs_attr_write(bus_space_tag_t, bus_space_handle_t, uint8_t, uint8_t);

static __inline void
igs_attr_write(bus_space_tag_t t, bus_space_handle_t h,
	       uint8_t idx, uint8_t val)
{
	bus_space_write_1(t, h, IGS_ATTR_IDX, idx);
	bus_space_write_1(t, h, IGS_ATTR_IDX, val); /* sic, same register */
}


/* sugar for graphics controller registers */
#define igs_grfx_read(t,h,x)	(igs_idx_read((t),(h),IGS_GRFX_IDX,(x)))
#define igs_grfx_write(t,h,x,v)	(igs_idx_write((t),(h),IGS_GRFX_IDX,(x),(v)))


/* sugar for extended registers */
#define igs_ext_read(t,h,x)	(igs_idx_read((t),(h),IGS_EXT_IDX,(x)))
#define igs_ext_write(t,h,x,v)	(igs_idx_write((t),(h),IGS_EXT_IDX,(x),(v)))


/* igsfb_subr.c */
int	igsfb_enable(bus_space_tag_t, bus_addr_t, int);
void	igsfb_hw_setup(struct igsfb_devconfig *);
void	igsfb_1024x768_8bpp_60Hz(struct igsfb_devconfig *);
void	igsfb_set_mode(struct igsfb_devconfig *, const struct videomode *, int);

/* igsfb.c */
int	igsfb_cnattach_subr(struct igsfb_devconfig *);
void	igsfb_attach_subr(struct igsfb_softc *, int);


extern struct igsfb_devconfig igsfb_console_dc;

#endif /* _DEV_IC_IGSFBVAR_H_ */
