/*	$NetBSD: tpmvar.h,v 1.3 2012/10/27 17:18:23 chs Exp $	*/
/*
 * Copyright (c) 2008, 2009 Michael Shalayeff
 * Copyright (c) 2009, 2010 Hans-Jörg Höxer
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

struct tpm_softc {
	device_t sc_dev;
	void *sc_ih;

	int	(*sc_init)(struct tpm_softc *, int, const char *);
	int	(*sc_start)(struct tpm_softc *, int);
	int	(*sc_read)(struct tpm_softc *, void *, size_t, size_t *, int);
	int	(*sc_write)(struct tpm_softc *, const void *, size_t);
	int	(*sc_end)(struct tpm_softc *, int, int);

	bus_space_tag_t sc_bt, sc_batm;
	bus_space_handle_t sc_bh, sc_bahm;

	u_int32_t sc_devid;
	u_int32_t sc_rev;
	u_int32_t sc_stat;
	u_int32_t sc_capabilities;

	int sc_flags;
#define	TPM_OPEN	0x0001

	int	 sc_vector;
};

int tpm_intr(void *);

bool tpm_suspend(device_t, const pmf_qual_t *);
bool tpm_resume(device_t, const pmf_qual_t *);

int tpm_tis12_probe(bus_space_tag_t, bus_space_handle_t);
int tpm_tis12_init(struct tpm_softc *, int, const char *);
int tpm_tis12_start(struct tpm_softc *, int);
int tpm_tis12_read(struct tpm_softc *, void *, size_t, size_t *, int);
int tpm_tis12_write(struct tpm_softc *, const void *, size_t);
int tpm_tis12_end(struct tpm_softc *, int, int);

int tpm_legacy_probe(bus_space_tag_t, bus_addr_t);
int tpm_legacy_init(struct tpm_softc *, int, const char *);
int tpm_legacy_start(struct tpm_softc *, int);
int tpm_legacy_read(struct tpm_softc *, void *, size_t, size_t *, int);
int tpm_legacy_write(struct tpm_softc *, const void *, size_t);
int tpm_legacy_end(struct tpm_softc *, int, int);
