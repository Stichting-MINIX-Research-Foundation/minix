/*	$NetBSD: igmavar.h,v 1.1 2014/01/21 14:52:07 mlelstv Exp $	*/

/*
 * Copyright (c) 2014 Michael van Elst
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef IGMAVAR_H
#define IGMAVAR_H

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>

struct igma_chip;
struct igma_chip_ops {
	void (*barrier)(const struct igma_chip *, int);
	u_int32_t (*read_reg)(const struct igma_chip *, int);
	void (*write_reg)(const struct igma_chip *, int, u_int32_t);
	u_int8_t (*read_vga)(const struct igma_chip *, int);
	void (*write_vga)(const struct igma_chip *, int, u_int8_t);
#if 0
	u_int8_t (*read_crtc)(const struct igma_chip *, int);
	void (*write_crtc)(const struct igma_chip *, int, u_int8_t);
#endif
};

struct igma_chip {
	const struct igma_chip_ops	*ops;

	bus_space_tag_t         gttt;
	bus_space_handle_t      gtth;

	bus_space_tag_t         mmiot;
	bus_space_handle_t      mmioh;

	bus_space_tag_t         vgat;
	bus_space_handle_t      vgah;

	bus_space_tag_t         gmt;
	bus_space_handle_t      gmh;
	bus_addr_t		gmb;

	int num_gmbus;
	int gpio_offset;
	int vga_cntrl;
	int num_pipes;
	int use_pipe;
	u_int32_t pri_cntrl;
	int backlight_cntrl;
	int backlight_cntrl2;
	int backlight_factor;

	unsigned quirks;
};
#define IGMA_PLANESTART_QUIRK   (1L << 0)
#define IGMA_PFITDISABLE_QUIRK  (1L << 1)

struct igma_attach_args {
	const struct igma_chip_ops	*iaa_chip_ops;
	struct igma_chip   	iaa_chip;
	bool				iaa_console;
	char				iaa_name[32];
};

#endif
