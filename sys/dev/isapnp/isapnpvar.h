/*	$NetBSD: isapnpvar.h,v 1.28 2009/03/15 15:45:48 cegger Exp $	*/

/*-
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas.
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

#ifndef _DEV_ISAPNP_ISAPNPVAR_H_
#define _DEV_ISAPNP_ISAPNPVAR_H_

/*
 * ISA Plug and Play register definitions;
 * From Plug and Play ISA Specification V1.0a, May 5 1994
 */

/*
 * Structures and definitions needed by the machine-dependent header.
 */
struct isapnp_softc;

#include <machine/isapnp_machdep.h>

#ifndef _KERNEL

# include <stdlib.h>
# include <string.h>
# include <unistd.h>

# define ISAPNP_WRITE_ADDR(sc, v) outb(ISAPNP_ADDR, v)
# define ISAPNP_WRITE_DATA(sc, v) outb(ISAPNP_WRDATA, v)
# define ISAPNP_READ_DATA(sc) inb(sc->sc_read_port)

# define DELAY(us) usleep(us)
# define ISAPNP_MALLOC(a) malloc(a)
# define ISAPNP_FREE(a) free(a)

# define panic printf

#else

# define ISAPNP_WRITE_ADDR(sc, v) \
    bus_space_write_1(sc->sc_iot, sc->sc_addr_ioh, 0, v)
# define ISAPNP_WRITE_DATA(sc, v) \
    bus_space_write_1(sc->sc_iot, sc->sc_wrdata_ioh, 0, v)
# define ISAPNP_READ_DATA(sc) \
    bus_space_read_1(sc->sc_iot, sc->sc_read_ioh, 0)

# define ISAPNP_MALLOC(a) malloc(a, M_DEVBUF, M_WAITOK)
# define ISAPNP_FREE(a) free(a, M_DEVBUF)

#endif

#ifdef DEBUG_ISAPNP
# define DPRINTF(a) printf a
#else
# define DPRINTF(a)
#endif

struct isapnp_softc {
	device_t		sc_dev;
	int			sc_read_port;
	bus_space_tag_t		sc_iot;
	bus_space_tag_t		sc_memt;
	isa_chipset_tag_t	sc_ic;
	bus_dma_tag_t		sc_dmat;
	bus_space_handle_t	sc_addr_ioh;
	bus_space_handle_t	sc_wrdata_ioh;
	bus_space_handle_t	sc_read_ioh;
	bus_space_handle_t	sc_memh;
	u_int8_t		sc_ncards;
    	u_int8_t		sc_id[ISAPNP_MAX_CARDS][ISAPNP_SERIAL_SIZE];
};

struct isapnp_region {
	bus_space_handle_t h;
	u_int32_t base;

	u_int32_t minbase;
	u_int32_t maxbase;
	u_int32_t length;
	u_int32_t align;
	u_int8_t  flags;
};

struct isapnp_pin {
	u_int8_t  num;
	u_int8_t  flags:4;
	u_int8_t  type:4;
	u_int16_t bits;
};

struct isapnp_attach_args {
	bus_space_tag_t ipa_iot;	/* isa i/o space tag */
	bus_space_tag_t ipa_memt;	/* isa mem space tag */
	bus_dma_tag_t	ipa_dmat;	/* isa dma tag */

	isa_chipset_tag_t ipa_ic;

	struct isapnp_attach_args *ipa_sibling;
	struct isapnp_attach_args *ipa_child;

	char	ipa_devident[ISAPNP_MAX_IDENT];
	char	ipa_devlogic[ISAPNP_MAX_DEVCLASS];
	char	ipa_devcompat[ISAPNP_MAX_DEVCLASS];
	char	ipa_devclass[ISAPNP_MAX_DEVCLASS];

	u_char	ipa_pref;
	u_char	ipa_devnum;

	u_char	ipa_nio;
	u_char	ipa_nirq;
	u_char	ipa_ndrq;
	u_char	ipa_nmem;
	u_char	ipa_nmem32;

	struct isapnp_region	ipa_io[ISAPNP_NUM_IO];
	struct isapnp_region	ipa_mem[ISAPNP_NUM_MEM];
	struct isapnp_region	ipa_mem32[ISAPNP_NUM_MEM32];
	struct isapnp_pin	ipa_irq[ISAPNP_NUM_IRQ];
	struct isapnp_pin	ipa_drq[ISAPNP_NUM_DRQ];
};

static __inline void isapnp_write_reg(struct isapnp_softc *, int, u_char);
static __inline u_char isapnp_read_reg(struct isapnp_softc *, int);

static __inline void
isapnp_write_reg(struct isapnp_softc *sc, int r, u_char v)
{
	ISAPNP_WRITE_ADDR(sc, r);
	ISAPNP_WRITE_DATA(sc, v);
}

static __inline u_char
isapnp_read_reg(struct isapnp_softc *sc, int r)
{
	ISAPNP_WRITE_ADDR(sc, r);
	return ISAPNP_READ_DATA(sc);
}

struct isapnp_attach_args *
    isapnp_get_resource(struct isapnp_softc *, int);
char *isapnp_id_to_vendor(char *, const u_char *);

int isapnp_config(bus_space_tag_t, bus_space_tag_t,
    struct isapnp_attach_args *);
void isapnp_unconfig(bus_space_tag_t, bus_space_tag_t,
    struct isapnp_attach_args *);
#ifdef _KERNEL
struct isapnp_devinfo;
int isapnp_devmatch(const struct isapnp_attach_args *,
    const struct isapnp_devinfo *, int *);
void isapnp_isa_attach_hook(struct isa_softc *);
#endif

#ifdef DEBUG_ISAPNP
void isapnp_print_mem(const char *, const struct isapnp_region *);
void isapnp_print_io(const char *, const struct isapnp_region *);
void isapnp_print_irq(const char *, const struct isapnp_pin *);
void isapnp_print_drq(const char *, const struct isapnp_pin *);
void isapnp_print_dep_start(const char *, const u_char);
void isapnp_print_attach(const struct isapnp_attach_args *);
void isapnp_get_config(struct isapnp_softc *, struct isapnp_attach_args *);
void isapnp_print_config(const struct isapnp_attach_args *);
#endif

#endif /* ! _DEV_ISAPNP_ISAPNPVAR_H_ */
