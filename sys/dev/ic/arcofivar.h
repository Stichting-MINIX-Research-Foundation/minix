/*	$NetBSD: arcofivar.h,v 1.1 2014/08/24 08:17:44 tsutsui Exp $	*/
/*	$OpenBSD: arcofivar.h,v 1.2 2011/12/25 00:07:27 miod Exp $	*/

/*
 * Copyright (c) 2011 Miodrag Vallat.
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

#define	ARCOFI_NREGS		6

struct arcofi_softc {
	device_t		sc_dev;
	bus_addr_t		sc_reg[ARCOFI_NREGS];
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;

	struct audio_device	sc_audio_device;
	void			*sc_sih;

	int			sc_open;
	int			sc_mode;

	struct {
		uint8_t	cr3, cr4;
		uint	gr_idx, gx_idx;
		int	output_mute;
	}			sc_active,
				sc_shadow;

	struct {
		uint8_t	*buf;
		uint8_t	*past;
		void	(*cb)(void *);
		void	*cbarg;
	}			sc_recv,
				sc_xmit;
	kmutex_t		sc_lock;
	kmutex_t		sc_intr_lock;
	kcondvar_t		sc_cv;
	struct audio_encoding_set *sc_encodings;
};

void	arcofi_attach(struct arcofi_softc *, const char *);
int	arcofi_hwintr(void *);
void	arcofi_swintr(void *);
