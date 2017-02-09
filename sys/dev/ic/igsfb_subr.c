/*	$NetBSD: igsfb_subr.c,v 1.12 2009/11/18 21:59:38 macallan Exp $ */

/*
 * Copyright (c) 2002 Valeriy E. Ushakov
 *		 2009 Michael Lorenz
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
#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: igsfb_subr.c,v 1.12 2009/11/18 21:59:38 macallan Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>

#include <sys/bus.h>

#include <dev/wscons/wsdisplayvar.h>
#include <dev/wscons/wsconsio.h>
#include <dev/rasops/rasops.h>
#include <dev/wscons/wsdisplay_vconsvar.h>

#include <dev/ic/igsfbreg.h>
#include <dev/ic/igsfbvar.h>

#ifdef IGSFB_DEBUG
#define DPRINTF printf
#else
#define DPRINTF while (0) printf
#endif

static void	igsfb_init_seq(struct igsfb_devconfig *);
static void	igsfb_init_crtc(struct igsfb_devconfig *);
static void	igsfb_init_grfx(struct igsfb_devconfig *);
static void	igsfb_init_attr(struct igsfb_devconfig *);
static void	igsfb_init_ext(struct igsfb_devconfig *);
static void	igsfb_init_dac(struct igsfb_devconfig *);

static void	igsfb_freq_latch(struct igsfb_devconfig *);
static void	igsfb_video_on(struct igsfb_devconfig *);
static void	igsfb_calc_pll(int, int *, int *, int *, int, int, int, int);



/*
 * Enable chip.
 */
int
igsfb_enable(bus_space_tag_t iot, bus_addr_t iobase, int ioflags)
{
	bus_space_handle_t vdoh;
	bus_space_handle_t vseh;
	bus_space_handle_t regh;
	int ret;

	ret = bus_space_map(iot, iobase + IGS_VDO, 1, ioflags, &vdoh);
	if (ret != 0) {
		printf("unable to map VDO register\n");
		goto out0;
	}

	ret = bus_space_map(iot, iobase + IGS_VSE, 1, ioflags, &vseh);
	if (ret != 0) {
		printf("unable to map VSE register\n");
		goto out1;
	}

	ret = bus_space_map(iot, iobase + IGS_REG_BASE, IGS_REG_SIZE, ioflags,
			    &regh);
	if (ret != 0) {
		printf("unable to map I/O registers\n");
		goto out2;
	}

	/*
	 * Start decoding i/o space accesses.
	 */
	bus_space_write_1(iot, vdoh, 0, IGS_VDO_ENABLE | IGS_VDO_SETUP);
	bus_space_write_1(iot, vseh, 0, IGS_VSE_ENABLE);
	bus_space_write_1(iot, vdoh, 0, IGS_VDO_ENABLE);

	/*
	 * Start decoding memory space accesses (XXX: move out of here?
	 * we program this register in igsfb_init_ext).
	 * While here, enable coprocessor and select IGS_COP_BASE_B.
	 */
	igs_ext_write(iot, regh, IGS_EXT_BIU_MISC_CTL,
		      (IGS_EXT_BIU_LINEAREN
		       | IGS_EXT_BIU_COPREN | IGS_EXT_BIU_COPASELB));

	bus_space_unmap(iot, regh, IGS_REG_SIZE);
  out2:	bus_space_unmap(iot, vseh, 1);
  out1:	bus_space_unmap(iot, vdoh, 1);
  out0: return ret;
}


/*
 * Init sequencer.
 * This is common for all video modes.
 */
static void
igsfb_init_seq(struct igsfb_devconfig *dc)
{
	bus_space_tag_t iot = dc->dc_iot;
	bus_space_handle_t ioh = dc->dc_ioh;

	/* start messing with sequencer */
	igs_seq_write(iot, ioh, IGS_SEQ_RESET, 0);

	igs_seq_write(iot, ioh, 1, 0x01); /* 8 dot clock */
	igs_seq_write(iot, ioh, 2, 0x0f); /* enable all maps */
	igs_seq_write(iot, ioh, 3, 0x00); /* character generator */
	igs_seq_write(iot, ioh, 4, 0x0e); /* memory mode */

	/* this selects color mode among other things */
	bus_space_write_1(iot, ioh, IGS_MISC_OUTPUT_W, 0xef);

	/* normal sequencer operation */
	igs_seq_write(iot, ioh, IGS_SEQ_RESET,
		      IGS_SEQ_RESET_SYNC | IGS_SEQ_RESET_ASYNC);
}


/*
 * Init CRTC to 640x480 8bpp at 60Hz
 */
static void
igsfb_init_crtc(struct igsfb_devconfig *dc)
{
	bus_space_tag_t iot = dc->dc_iot;
	bus_space_handle_t ioh = dc->dc_ioh;

	igs_crtc_write(iot, ioh, 0x00, 0x5f);
	igs_crtc_write(iot, ioh, 0x01, 0x4f);
	igs_crtc_write(iot, ioh, 0x02, 0x50);
	igs_crtc_write(iot, ioh, 0x03, 0x80);
	igs_crtc_write(iot, ioh, 0x04, 0x52);
	igs_crtc_write(iot, ioh, 0x05, 0x9d);
	igs_crtc_write(iot, ioh, 0x06, 0x0b);
	igs_crtc_write(iot, ioh, 0x07, 0x3e);

	/* next block is almost constant, only bit 6 in reg 9 differs */
	igs_crtc_write(iot, ioh, 0x08, 0x00);
	igs_crtc_write(iot, ioh, 0x09, 0x40); /* <- either 0x40 or 0x60 */
	igs_crtc_write(iot, ioh, 0x0a, 0x00);
	igs_crtc_write(iot, ioh, 0x0b, 0x00);
	igs_crtc_write(iot, ioh, 0x0c, 0x00);
	igs_crtc_write(iot, ioh, 0x0d, 0x00);
	igs_crtc_write(iot, ioh, 0x0e, 0x00);
	igs_crtc_write(iot, ioh, 0x0f, 0x00);

	igs_crtc_write(iot, ioh, 0x10, 0xe9);
	igs_crtc_write(iot, ioh, 0x11, 0x8b);
	igs_crtc_write(iot, ioh, 0x12, 0xdf);
	igs_crtc_write(iot, ioh, 0x13, 0x50);
	igs_crtc_write(iot, ioh, 0x14, 0x00);
	igs_crtc_write(iot, ioh, 0x15, 0xe6);
	igs_crtc_write(iot, ioh, 0x16, 0x04);
	igs_crtc_write(iot, ioh, 0x17, 0xc3);

	igs_crtc_write(iot, ioh, 0x18, 0xff);
}


/*
 * Init graphics controller.
 * This is common for all video modes.
 */
static void
igsfb_init_grfx(struct igsfb_devconfig *dc)
{
	bus_space_tag_t iot = dc->dc_iot;
	bus_space_handle_t ioh = dc->dc_ioh;

	igs_grfx_write(iot, ioh, 0, 0x00);
	igs_grfx_write(iot, ioh, 1, 0x00);
	igs_grfx_write(iot, ioh, 2, 0x00);
	igs_grfx_write(iot, ioh, 3, 0x00);
	igs_grfx_write(iot, ioh, 4, 0x00);
	igs_grfx_write(iot, ioh, 5, 0x60); /* SRMODE, MODE256 */
	igs_grfx_write(iot, ioh, 6, 0x05); /* 64k @ a0000, GRAPHICS */
	igs_grfx_write(iot, ioh, 7, 0x0f); /* color compare all */
	igs_grfx_write(iot, ioh, 8, 0xff); /* bitmask = all bits mutable */
}


/*
 * Init attribute controller.
 * This is common for all video modes.
 */
static void
igsfb_init_attr(struct igsfb_devconfig *dc)
{
	bus_space_tag_t iot = dc->dc_iot;
	bus_space_handle_t ioh = dc->dc_ioh;
	int i;

	igs_attr_flip_flop(iot, ioh);	/* reset attr flip-flop to address */

	for (i = 0; i < 16; ++i)	/* crt palette */
		igs_attr_write(iot, ioh, i, i);

	igs_attr_write(iot, ioh, 0x10, 0x01); /* select graphic mode */
	igs_attr_write(iot, ioh, 0x11, 0x00); /* crt overscan color */
	igs_attr_write(iot, ioh, 0x12, 0x0f); /* color plane enable */
	igs_attr_write(iot, ioh, 0x13, 0x00);
	igs_attr_write(iot, ioh, 0x14, 0x00);
}


/*
 * When done with ATTR controller, call this to unblank the screen.
 */
static void
igsfb_video_on(struct igsfb_devconfig *dc)
{
	bus_space_tag_t iot = dc->dc_iot;
	bus_space_handle_t ioh = dc->dc_ioh;

	igs_attr_flip_flop(iot, ioh);
	bus_space_write_1(iot, ioh, IGS_ATTR_IDX, 0x20);
	bus_space_write_1(iot, ioh, IGS_ATTR_IDX, 0x20);
}


/*
 * Latch VCLK (b0/b1) and MCLK (b2/b3) values.
 */
static void
igsfb_freq_latch(struct igsfb_devconfig *dc)
{
	bus_space_tag_t iot = dc->dc_iot;
	bus_space_handle_t ioh = dc->dc_ioh;

	bus_space_write_1(iot, ioh, IGS_EXT_IDX, 0xb9);
	bus_space_write_1(iot, ioh, IGS_EXT_PORT, 0x80);
	bus_space_write_1(iot, ioh, IGS_EXT_PORT, 0x00);
}


static void
igsfb_init_ext(struct igsfb_devconfig *dc)
{
	bus_space_tag_t iot = dc->dc_iot;
	bus_space_handle_t ioh = dc->dc_ioh;
	int is_cyberpro = (dc->dc_id >= 0x2000);

	igs_ext_write(iot, ioh, 0x10, 0x10); /* IGS_EXT_START_ADDR enable */
	igs_ext_write(iot, ioh, 0x12, 0x00); /* IGS_EXT_IRQ_CTL disable  */
	igs_ext_write(iot, ioh, 0x13, 0x00); /* MBZ for normal operation */

	igs_ext_write(iot, ioh, 0x31, 0x00); /* segment write ptr */
	igs_ext_write(iot, ioh, 0x32, 0x00); /* segment read ptr */

	/* IGS_EXT_BIU_MISC_CTL: linearen, copren, copaselb, segon */
	igs_ext_write(iot, ioh, 0x33, 0x1d);

	/* sprite location */
	igs_ext_write(iot, ioh, 0x50, 0x00);
	igs_ext_write(iot, ioh, 0x51, 0x00);
	igs_ext_write(iot, ioh, 0x52, 0x00);
	igs_ext_write(iot, ioh, 0x53, 0x00);
	igs_ext_write(iot, ioh, 0x54, 0x00);
	igs_ext_write(iot, ioh, 0x55, 0x00);
	igs_ext_write(iot, ioh, 0x56, 0x00); /* sprite control */

	/* IGS_EXT_GRFX_MODE */
	igs_ext_write(iot, ioh, 0x57, 0x01); /* raster fb */

	/* overscan R/G/B */
	igs_ext_write(iot, ioh, 0x58, 0x00);
	igs_ext_write(iot, ioh, 0x59, 0x00);
	igs_ext_write(iot, ioh, 0x5A, 0x00);

	/*
	 * Video memory size &c.  We rely on firmware to program
	 * BUS_CTL(30), MEM_CTL1(71), MEM_CTL2(72) appropriately.
	 */

	/* ext memory ctl0 */
	igs_ext_write(iot, ioh, 0x70, 0x0B); /* enable fifo, seq */

	/* ext hidden ctl1 */
	igs_ext_write(iot, ioh, 0x73, 0x30); /* XXX: krups: 0x20 */

	/* ext fifo control */
	igs_ext_write(iot, ioh, 0x74, 0x10); /* XXX: krups: 0x1b */
	igs_ext_write(iot, ioh, 0x75, 0x10); /* XXX: krups: 0x1e */

	igs_ext_write(iot, ioh, 0x76, 0x00); /* ext seq. */
	igs_ext_write(iot, ioh, 0x7A, 0xC8); /* ext. hidden ctl */

	/* ext graphics ctl: GCEXTPATH.  krups 1, nettrom 1, docs 3 */
	igs_ext_write(iot, ioh, 0x90, 0x01);

	if (is_cyberpro)	/* select normal vclk/mclk registers */
	    igs_ext_write(iot, ioh, 0xBF, 0x00);

	igs_ext_write(iot, ioh, 0xB0, 0xD2); /* VCLK = 25.175MHz */
	igs_ext_write(iot, ioh, 0xB1, 0xD3);
	igs_ext_write(iot, ioh, 0xB2, 0xDB); /* MCLK = 75MHz*/
	igs_ext_write(iot, ioh, 0xB3, 0x54);
	igsfb_freq_latch(dc);

	if (is_cyberpro)
	    igs_ext_write(iot, ioh, 0xF8, 0x04); /* XXX: ??? */

	/* 640x480 8bpp at 60Hz */
	igs_ext_write(iot, ioh, 0x11, 0x00);
	igs_ext_write(iot, ioh, 0x77, 0x01); /* 8bpp, indexed */
	igs_ext_write(iot, ioh, 0x14, 0x51);
	igs_ext_write(iot, ioh, 0x15, 0x00);
}


static void
igsfb_init_dac(struct igsfb_devconfig *dc)
{
	bus_space_tag_t iot = dc->dc_iot;
	bus_space_handle_t ioh = dc->dc_ioh;
	uint8_t reg;

	/* RAMDAC address 2 select */
	reg = igs_ext_read(iot, ioh, IGS_EXT_SPRITE_CTL);
	igs_ext_write(iot, ioh, IGS_EXT_SPRITE_CTL,
		      reg | IGS_EXT_SPRITE_DAC_PEL);

	/* VREFEN, DAC8 */
	bus_space_write_1(iot, ioh, IGS_DAC_CMD, 0x06);

	/* restore */
	igs_ext_write(iot, ioh, IGS_EXT_SPRITE_CTL, reg);

	bus_space_write_1(iot, ioh, IGS_PEL_MASK, 0xff);
}


void
igsfb_1024x768_8bpp_60Hz(struct igsfb_devconfig *dc)
{
	bus_space_tag_t iot = dc->dc_iot;
	bus_space_handle_t ioh = dc->dc_ioh;

	igs_crtc_write(iot, ioh, 0x11, 0x00); /* write enable CRTC 0..7 */

	igs_crtc_write(iot, ioh, 0x00, 0xa3);
	igs_crtc_write(iot, ioh, 0x01, 0x7f);
	igs_crtc_write(iot, ioh, 0x02, 0x7f); /* krups: 80 */
	igs_crtc_write(iot, ioh, 0x03, 0x85); /* krups: 84 */
	igs_crtc_write(iot, ioh, 0x04, 0x84); /* krups: 88 */
	igs_crtc_write(iot, ioh, 0x05, 0x95); /* krups: 99 */
	igs_crtc_write(iot, ioh, 0x06, 0x24);
	igs_crtc_write(iot, ioh, 0x07, 0xfd);

	/* next block is almost constant, only bit 6 in reg 9 differs */
	igs_crtc_write(iot, ioh, 0x08, 0x00);
	igs_crtc_write(iot, ioh, 0x09, 0x60); /* <- either 0x40 or 0x60 */
	igs_crtc_write(iot, ioh, 0x0a, 0x00);
	igs_crtc_write(iot, ioh, 0x0b, 0x00);
	igs_crtc_write(iot, ioh, 0x0c, 0x00);
	igs_crtc_write(iot, ioh, 0x0d, 0x00);
	igs_crtc_write(iot, ioh, 0x0e, 0x00);
	igs_crtc_write(iot, ioh, 0x0f, 0x00);

	igs_crtc_write(iot, ioh, 0x10, 0x06);
	igs_crtc_write(iot, ioh, 0x11, 0x8c);
	igs_crtc_write(iot, ioh, 0x12, 0xff);
	igs_crtc_write(iot, ioh, 0x13, 0x80); /* depends on BPP */
	igs_crtc_write(iot, ioh, 0x14, 0x0f);
	igs_crtc_write(iot, ioh, 0x15, 0x02);
	igs_crtc_write(iot, ioh, 0x16, 0x21);
	igs_crtc_write(iot, ioh, 0x17, 0xe3);
	igs_crtc_write(iot, ioh, 0x18, 0xff);

	igs_ext_write(iot, ioh, 0xB0, 0xE2); /* VCLK */
	igs_ext_write(iot, ioh, 0xB1, 0x58);
#if 1
	/* XXX: hmm, krups does this */
	igs_ext_write(iot, ioh, 0xB2, 0xE2); /* MCLK */
	igs_ext_write(iot, ioh, 0xB3, 0x58);
#endif
	igsfb_freq_latch(dc);

	igs_ext_write(iot, ioh, 0x11, 0x00);
	igs_ext_write(iot, ioh, 0x77, 0x01); /* 8bpp, indexed */
	igs_ext_write(iot, ioh, 0x14, 0x81);
	igs_ext_write(iot, ioh, 0x15, 0x00);

	dc->dc_width = 1024;
	dc->dc_height = 768;
	dc->dc_depth = 8;
	dc->dc_stride = dc->dc_width;
}


/*
 * igs-video-init from krups prom
 */
void
igsfb_hw_setup(struct igsfb_devconfig *dc)
{
	const struct videomode *mode = NULL;
	int i, size, d;

	igsfb_init_seq(dc);
	igsfb_init_crtc(dc);
	igsfb_init_attr(dc);
	igsfb_init_grfx(dc);
	igsfb_init_ext(dc);
	igsfb_init_dac(dc);

	i = 0;
	while ((strcmp(dc->dc_modestring, videomode_list[i].name) != 0) &&	
	       ( i < videomode_count)) {
		i++;
	}

	if (i < videomode_count) {
		size = videomode_list[i].hdisplay * videomode_list[i].vdisplay;
		/* found a mode, now let's see if we can display it */
		if ((videomode_list[i].dot_clock <= IGS_MAX_CLOCK) &&
		    (videomode_list[i].hdisplay <= 2048) &&
		    (videomode_list[i].hdisplay >= 320) &&
		    (videomode_list[i].vdisplay <= 2048) &&
		    (videomode_list[i].vdisplay >= 200) &&
		    (size <= (dc->dc_memsz - 0x1000))) {
		 	mode = &videomode_list[i];
			/*
			 * now let's see which maximum depth we can support
			 * in that mode
			 */
			d = (dc->dc_vmemsz - 0x1000) / size;
			if (d >= 4) {
				dc->dc_maxdepth = 32;
			} else if (d >= 2) {
				dc->dc_maxdepth = 16;
			} else
				dc->dc_maxdepth = 8;
		}
	}
	dc->dc_mode = mode;

	if (mode != NULL) {
		igsfb_set_mode(dc, mode, 8);
	} else {
		igsfb_1024x768_8bpp_60Hz(dc);
		dc->dc_maxdepth = 8;
	}

	igsfb_video_on(dc);
}

void
igsfb_set_mode(struct igsfb_devconfig *dc, const struct videomode *mode,
    int depth)
{
	bus_space_tag_t iot = dc->dc_iot;
	bus_space_handle_t ioh = dc->dc_ioh;
	int i, m, n, p, hoffset, bytes_per_pixel, memfetch;
	int vsync_start, hsync_start, vsync_end, hsync_end;
	int vblank_start, vblank_end, hblank_start, hblank_end;
	int croffset;
	uint8_t vclk1, vclk2, vclk3, overflow, reg, seq_mode;

	switch (depth) {
		case 8:
			seq_mode = IGS_EXT_SEQ_8BPP;
			break;
		case 15:
			seq_mode = IGS_EXT_SEQ_15BPP; /* 5-5-5 */
			break;
		case 16:
			seq_mode = IGS_EXT_SEQ_16BPP; /* 5-6-5 */
			break;
		case 24:
			seq_mode = IGS_EXT_SEQ_24BPP; /* 8-8-8 */
			break;
		case 32:
			seq_mode = IGS_EXT_SEQ_32BPP;
			break;
		default:
			aprint_error("igsfb: unsupported depth (%d), reverting"
				     " to 8 bit\n", depth);
			depth = 8;
			seq_mode = IGS_EXT_SEQ_8BPP;
	}
	bytes_per_pixel = depth >> 3;

	hoffset = (mode->hdisplay >> 3) * bytes_per_pixel;
	memfetch = hoffset + 1;
	overflow = (((mode->vtotal - 2) & 0x400) >> 10) | 
	    (((mode->vdisplay -1) & 0x400) >> 9) |
	    ((mode->vsync_start & 0x400) >> 8) |
	    ((mode->vsync_start & 0x400) >> 7) |
	    0x10; 

	/* RAMDAC address 2 select */
	reg = igs_ext_read(iot, ioh, IGS_EXT_SPRITE_CTL);
	igs_ext_write(iot, ioh, IGS_EXT_SPRITE_CTL,
		      reg | IGS_EXT_SPRITE_DAC_PEL);

	if (depth == 8) {
		/* palette mode */
		bus_space_write_1(dc->dc_iot, dc->dc_ioh, IGS_DAC_CMD, 0x06);
	} else {
		/* bypass palette */
		bus_space_write_1(dc->dc_iot, dc->dc_ioh, IGS_DAC_CMD, 0x16);
	}
	/* restore */
	igs_ext_write(iot, ioh, IGS_EXT_SPRITE_CTL, reg);

	bus_space_write_1(iot, ioh, IGS_PEL_MASK, 0xff);

	igs_crtc_write(iot, ioh, 0x11, 0x00); /* write enable CRTC 0..7 */

	hsync_start = mode->hsync_start;
	hsync_end = mode->hsync_end;

	hblank_start = min(mode->hsync_start, mode->hdisplay);
	hblank_end = hsync_end;
	if ((hblank_end - hblank_start) >= 63 * 8) {

		/*
		 * H Blanking size must be < 63*8. Same remark as above.
		 */
		hblank_start = hblank_end - 63 * 8;
	}

	vblank_start = min(mode->vsync_start, mode->vdisplay);
	vblank_end = mode->vsync_end;

	vsync_start = mode->vsync_start;
	vsync_end = mode->vsync_end;
	igs_crtc_write(iot, ioh, 0x00, (mode->htotal >> 3) - 5);
	igs_crtc_write(iot, ioh, 0x01, (mode->hdisplay >> 3) - 1);
	igs_crtc_write(iot, ioh, 0x02, (hblank_start >> 3) - 1);
	igs_crtc_write(iot, ioh, 0x03, 0x80 | (((hblank_end >> 3) - 1) & 0x1f));
	igs_crtc_write(iot, ioh, 0x04, hsync_start >> 3);
	igs_crtc_write(iot, ioh, 0x05, ((((hblank_end >> 3) - 1)  & 0x20) << 2) 
	    | ((hsync_end >> 3) & 0x1f));
	igs_crtc_write(iot, ioh, 0x06, (mode->vtotal - 2) & 0xff);
	igs_crtc_write(iot, ioh, 0x07, 
	    ((vsync_start & 0x200) >> 2) |
	    (((mode->vdisplay - 1) & 0x200) >> 3) |
	    (((mode->vtotal - 2) & 0x200) >> 4) |
	    0x10 |
	    (((vblank_start - 1) & 0x100) >> 5) |
	    ((vsync_start  & 0x100) >> 6) |
	    (((mode->vdisplay - 1)  & 0x100) >> 7) |
	    ((mode->vtotal  & 0x100) >> 8));

	igs_crtc_write(iot, ioh, 0x08, 0x00);
	igs_crtc_write(iot, ioh, 0x09, 0x40 | 
	    (((vblank_start - 1) & 0x200) >> 4));
	igs_crtc_write(iot, ioh, 0x0a, 0x00);
	igs_crtc_write(iot, ioh, 0x0b, 0x00);
	igs_crtc_write(iot, ioh, 0x0c, 0x00);
	igs_crtc_write(iot, ioh, 0x0d, 0x00);
	igs_crtc_write(iot, ioh, 0x0e, 0x00);
	igs_crtc_write(iot, ioh, 0x0f, 0x00);

	igs_crtc_write(iot, ioh, 0x10, vsync_start & 0xff);
	igs_crtc_write(iot, ioh, 0x11, (vsync_end & 0x0f) | 0x20);
	igs_crtc_write(iot, ioh, 0x12, (mode->vdisplay - 1) & 0xff);
	igs_crtc_write(iot, ioh, 0x13, hoffset & 0xff);
	igs_crtc_write(iot, ioh, 0x14, 0x0f);
	igs_crtc_write(iot, ioh, 0x15, (vblank_start - 1) & 0xff);
	igs_crtc_write(iot, ioh, 0x16, (vblank_end - 1) & 0xff);
	igs_crtc_write(iot, ioh, 0x17, 0xe3);
	igs_crtc_write(iot, ioh, 0x18, 0xff);

	for (i = 0; i < 0x10; i++)	
		igs_attr_write(iot, ioh, i, i);
	
	igs_attr_write(iot, ioh, 0x10, 0x01);
	igs_attr_write(iot, ioh, 0x11, 0x00);
	igs_attr_write(iot, ioh, 0x12, 0x0f);
	igs_attr_write(iot, ioh, 0x13, 0x00);

	igs_grfx_write(iot, ioh, 0x00, 0x00);
	igs_grfx_write(iot, ioh, 0x01, 0x00);
	igs_grfx_write(iot, ioh, 0x02, 0x00);
	igs_grfx_write(iot, ioh, 0x03, 0x00);
	igs_grfx_write(iot, ioh, 0x04, 0x00);
	igs_grfx_write(iot, ioh, 0x05, 0x60);
	igs_grfx_write(iot, ioh, 0x06, 0x05);
	igs_grfx_write(iot, ioh, 0x07, 0x0f);
	igs_grfx_write(iot, ioh, 0x08, 0xff);

	/* crank up memory clock to 95MHz - needed for higher resolutions */
	igs_ext_write(iot, ioh, IGS_EXT_MCLK0, 0x91);
	igs_ext_write(iot, ioh, IGS_EXT_MCLK1, 0x6a);
	igsfb_freq_latch(dc);

	igs_ext_write(iot, ioh, IGS_EXT_VOVFL, overflow);
	igs_ext_write(iot, ioh, IGS_EXT_SEQ_MISC, seq_mode);
	igs_ext_write(iot, ioh, 0x14, memfetch & 0xff);
	igs_ext_write(iot, ioh, 0x15,
	    ((memfetch & 0x300) >> 8) | ((hoffset & 0x300) >> 4));

	/* finally set the dot clock */
	igsfb_calc_pll(mode->dot_clock, &m, &n, &p, 2047, 255, 7, IGS_MIN_VCO);
	DPRINTF("m: %x, n: %x, p: %x\n", m, n, p);
	vclk1 = m & 0xff;
	vclk2 = (n & 0x1f) | ((p << 6) & 0xc0) |
	    (mode->dot_clock > 180000 ? 0x20 : 0);
	vclk3 = ((m >> 8) & 0x7) | ((n >> 2) & 0x38) | ((p << 4) & 0x40);
	DPRINTF("clk: %02x %02x %02x\n", vclk1, vclk2, vclk3);
	igs_ext_write(iot, ioh, IGS_EXT_VCLK0, vclk1);
	igs_ext_write(iot, ioh, IGS_EXT_VCLK1, vclk2);
	igs_ext_write(iot, ioh, 0xBA, vclk3);
	igsfb_freq_latch(dc);
	DPRINTF("clock: %d\n", IGS_CLOCK(m, n, p));

	if (dc->dc_id > 0x2000) {
		/* we have a blitter, so configure it as well */
		bus_space_write_1(dc->dc_iot, dc->dc_coph, IGS_COP_MAP_FMT_REG,
		    bytes_per_pixel - 1);
		bus_space_write_2(dc->dc_iot, dc->dc_coph,
		    IGS_COP_SRC_MAP_WIDTH_REG, dc->dc_width - 1);
		bus_space_write_2(dc->dc_iot, dc->dc_coph,
		    IGS_COP_DST_MAP_WIDTH_REG, dc->dc_width - 1);
	}

	/* re-init the cursor data address too */
	croffset = dc->dc_vmemsz - IGS_CURSOR_DATA_SIZE;
	croffset >>= 10;	/* bytes -> kilobytes */
	igs_ext_write(dc->dc_iot, dc->dc_ioh,
		      IGS_EXT_SPRITE_DATA_LO, croffset & 0xff);
	igs_ext_write(dc->dc_iot, dc->dc_ioh,
		      IGS_EXT_SPRITE_DATA_HI, (croffset >> 8) & 0xf);

	dc->dc_width = mode->hdisplay;
	dc->dc_height = mode->vdisplay;
	dc->dc_depth = depth;
	dc->dc_stride = dc->dc_width * (depth >> 3);

	igsfb_video_on(dc);
}


static void
igsfb_calc_pll(int target, int *Mp, int *Np, int *Pp, int maxM, int maxN,
    int maxP, int minVco)
{
    int	    M, N, P, bestM = 0, bestN = 0;
    int	    f_vco, f_out;
    int	    err, besterr;

    /*
     * Compute correct P value to keep VCO in range
     */
    for (P = 0; P <= maxP; P++)
    {
	f_vco = target * IGS_SCALE(P);
	if (f_vco >= minVco)
	    break;
    }

    /* M = f_out / f_ref * ((N + 1) * IGS_SCALE(P)); */
    besterr = target;
    for (N = 1; N <= maxN; N++)
    {
	M = ((target * (N + 1) * IGS_SCALE(P) + (IGS_CLOCK_REF/2)) + 
	    IGS_CLOCK_REF/2) / IGS_CLOCK_REF - 1;
	if (0 <= M && M <= maxM)
	{
	    f_out = IGS_CLOCK(M,N,P);
	    err = target - f_out;
	    if (err < 0)
		err = -err;
	    if (err < besterr)
	    {
		besterr = err;
		bestM = M;
		bestN = N;
	    }
	}
    }
    *Mp = bestM;
    *Np = bestN;
    *Pp = P;
}
