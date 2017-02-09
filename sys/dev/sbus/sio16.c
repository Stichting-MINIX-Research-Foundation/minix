/*	$NetBSD: sio16.c,v 1.24 2011/07/18 00:58:52 mrg Exp $	*/

/*
 * Copyright (c) 1998, 2001 Matthew R. Green
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * aurora technologies nova16 driver.  this board is an sbus card with
 * an external 16 port serial box.  there are two cirrus logic cd180
 * 8 port serial chips used in the implementation.
 *
 * thanks go to Oliver Aldulea <oli@morcov.bv.ro> for writing the
 * linux driver of this that helped clear up a couple of issues.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: sio16.c,v 1.24 2011/07/18 00:58:52 mrg Exp $");

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/systm.h>

#include <machine/autoconf.h>

#include <dev/ic/cd18xxvar.h>
#include <dev/ic/cd18xxreg.h>

#include "ioconf.h"
#include "locators.h"

/* 1600se configuration register bits */
#define SIO16_CONFIGREG_ENABLE_IO    8
#define SIO16_CONFIGREG_ENABLE_IRQ   4

/* 1600se interrupt ackknowledge register bytes */
#define	SIO16_MINT_ACK		1	/* modem interrupt acknowledge */
#define	SIO16_TINT_ACK		2	/* tx interrupt acknowledge */
#define	SIO16_RINT_ACK		3	/* rx interrupt acknowledge */

/*
 * device cfattach and cfdriver definitions, plus the routine we pass
 * to the cd18xx code or interrupt acknowledgement.
 */
static int	sio16_match(device_t, cfdata_t, void *);
static void	sio16_attach(device_t, device_t, void *);
static u_char	sio16_ackfunc(void *, int who);

/*
 * define the sio16 per-device softc.
 */
struct sio16_softc {
	device_t sc_dev;

	/* sbus information */
	bus_space_tag_t	sc_tag;			/* bus tag for below */
	bus_space_handle_t sc_configreg;	/* configuration register */
	bus_space_handle_t sc_reg[2];		/* cd180 register sets */
	bus_space_handle_t sc_ack;		/* interrupt acknowledgement */
#define	SIO16_1600SE	0x00000001

	u_int           sc_clk;

	/* cd180 information */
	int		sc_ncd180;

};

CFATTACH_DECL_NEW(siosixteen, sizeof(struct sio16_softc),
    sio16_match, sio16_attach, NULL, NULL);

struct sio16_attach_args {
	bus_space_tag_t		cd_tag;
	bus_space_handle_t	cd_handle;
	u_char			(*cd_ackfunc)(void *, int);
	void			*cd_ackfunc_arg;
	u_int			cd_osc;
};

/*
 * device match routine:  is there an sio16 present?
 *
 * note that we can not put "sio16" in the cfdriver, as this is an
 * illegal name, so we have to hard code it here.
 */
#define	SIO16_ROM_NAME	"sio16"
int
sio16_match(device_t parent, cfdata_t cf, void *aux)
{
	struct sbus_attach_args *sa = aux;

	/* does the prom think i'm an sio16? */
	if (strcmp(SIO16_ROM_NAME, sa->sa_name) != 0)
		return (0);

	return (1);
}

/*
 * device attach routine:  go attach all sub devices.
 */
void
sio16_attach(device_t parent, device_t self, void *aux)
{
	struct sbus_attach_args *sa = aux;
	struct sio16_softc *sc = device_private(self);
	bus_space_handle_t h;
	char *mode, *model;
	int i;

	sc->sc_dev = self;

	if (sa->sa_nreg != 4)
		panic("sio16_attach: got %d registers intead of 4",
		    sa->sa_nreg);

	/* copy our bus tag, we will need it */
	sc->sc_tag = sa->sa_bustag;

	/*
	 * sio16 has 4 register mappings.  a single byte configuration
	 * register, 2 128 byte regions for the cd180 registers, and
	 * a 4 byte region for interrupt acknowledgement.
	 */
	if (sbus_bus_map(sa->sa_bustag,
			 sa->sa_reg[0].oa_space,
			 sa->sa_reg[0].oa_base,
			 sa->sa_reg[0].oa_size,
			 0, &h) != 0) {
		printf("%s at sbus: can not map registers 0\n",
		    device_xname(self));
		return;
	}
	sc->sc_configreg = h;
	if (sbus_bus_map(sa->sa_bustag,
			 sa->sa_reg[1].sbr_slot,
			 sa->sa_reg[1].sbr_offset,
			 sa->sa_reg[1].sbr_size,
			 0, &h) != 0) {
		printf("%s at sbus: can not map registers 1\n",
		    device_xname(self));
		return;
	}
	sc->sc_reg[0] = h;
	if (sbus_bus_map(sa->sa_bustag,
			 sa->sa_reg[2].sbr_slot,
			 sa->sa_reg[2].sbr_offset,
			 sa->sa_reg[2].sbr_size,
			 0, &h) != 0) {
		printf("%s at sbus: can not map registers 2\n",
		    device_xname(self));
		return;
	}
	sc->sc_reg[1] = h;
	if (sbus_bus_map(sa->sa_bustag,
			 sa->sa_reg[3].sbr_slot,
			 sa->sa_reg[3].sbr_offset,
			 sa->sa_reg[3].sbr_size,
			 0, &h) != 0) {
		printf("%s at sbus: can not map registers 3\n",
		    device_xname(self));
		return;
	}
	sc->sc_ack = h;

	mode = prom_getpropstring(sa->sa_node, "mode");
	if (mode)
		printf(", %s mode", mode);

	/* get the clock frequency */
	sc->sc_clk = prom_getpropint(sa->sa_node, "clk", 24000);

	model = prom_getpropstring(sa->sa_node, "model");
	if (model == 0) {
		printf(", no model property, bailing\n");
		return;
	}

	/* are we an 1600se? */
	if (strcmp(model, "1600se") == 0) {
		printf(", 16 channels");
		sc->sc_ncd180 = 2;
	} else {
		printf(", don't know model %s, bailing\n", model);
		return;
	}

	/* establish interrupt channel */
	(void)bus_intr_establish(sa->sa_bustag, sa->sa_pri, IPL_TTY,
	    cd18xx_hardintr, sc);

	/* reset the board, and turn on interrupts and I/O */
	bus_space_write_1(sa->sa_bustag, sc->sc_configreg, 0, 0);
	delay(100);
	bus_space_write_1(sa->sa_bustag, sc->sc_configreg, 0,
	    SIO16_CONFIGREG_ENABLE_IO | SIO16_CONFIGREG_ENABLE_IRQ |
	    (((sa->sa_pri) & 0x0f) >> 2));
	delay(10000);

	printf("\n");

	/* finally, configure the clcd's underneath */
	for (i = 0; i < sc->sc_ncd180; i++) {
		struct sio16_attach_args cd;

		cd.cd_tag = sa->sa_bustag;
		cd.cd_osc = sc->sc_clk * 100;
		cd.cd_handle = (bus_space_handle_t)sc->sc_reg[i];
		cd.cd_ackfunc = sio16_ackfunc;
		cd.cd_ackfunc_arg = sc;
		(void)config_found(self, (void *)&cd, NULL);
	}
}

/*
 * note that the addresses used in this function match those assigned
 * in clcd_attach() below, or the various service match routines.
 */
u_char
sio16_ackfunc(void *v, int who)
{
	struct sio16_softc *sc = v;
	bus_size_t addr;

	switch (who) {
	case CD18xx_INTRACK_RxINT:
	case CD18xx_INTRACK_REINT:
		addr = SIO16_RINT_ACK;
		break;
	case CD18xx_INTRACK_TxINT:
		addr = SIO16_TINT_ACK;
		break;
	case CD18xx_INTRACK_MxINT:
		addr = SIO16_MINT_ACK;
		break;
	default:
		panic("%s: sio16_ackfunc: unknown ackfunc %d",
		    device_xname(sc->sc_dev), who);
	}
	return (bus_space_read_1(sc->sc_tag, sc->sc_ack, addr));
}

/*
 * we attach two `clcd' instances per 1600se, that each call the
 * backend cd18xx driver for help.
 */
static int	clcd_match(device_t, cfdata_t, void *);
static void	clcd_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(clcd, sizeof(struct cd18xx_softc),
    clcd_match, clcd_attach, NULL, NULL);

static int
clcd_match(device_t parent, cfdata_t cf, void *aux)
{

	/* XXX */
	return 1;
}

static void
clcd_attach(device_t parent, device_t self, void *aux)
{
	struct cd18xx_softc *sc = device_private(self);
	struct sio16_attach_args *args = aux;

	sc->sc_tag = args->cd_tag;
	sc->sc_handle = args->cd_handle;
	sc->sc_osc = args->cd_osc;
	sc->sc_ackfunc = args->cd_ackfunc;
	sc->sc_ackfunc_arg = args->cd_ackfunc_arg;
	sc->sc_msmr = SIO16_MINT_ACK;
	sc->sc_tsmr = SIO16_TINT_ACK;
	sc->sc_rsmr = SIO16_RINT_ACK;

	/* call the common code */
	cd18xx_attach(sc);
}
