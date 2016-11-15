/*	$NetBSD: if_fmv_isa.c,v 1.13 2008/04/12 06:27:01 tsutsui Exp $	*/

/*
 * All Rights Reserved, Copyright (C) Fujitsu Limited 1995
 *
 * This software may be used, modified, copied, distributed, and sold, in
 * both source and binary form provided that the above copyright, these
 * terms and the following disclaimer are retained.  The name of the author
 * and/or the contributor may not be used to endorse or promote products
 * derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND THE CONTRIBUTOR ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR THE CONTRIBUTOR BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION.
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Portions copyright (C) 1993, David Greenman.  This software may be used,
 * modified, copied, distributed, and sold, in both source and binary form
 * provided that the above copyright and these terms are retained.  Under no
 * circumstances is the author responsible for the proper functioning of this
 * software, nor does the author assume any responsibility for damages
 * incurred with its use.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_fmv_isa.c,v 1.13 2008/04/12 06:27:01 tsutsui Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <net/if.h>
#include <net/if_ether.h>
#include <net/if_media.h>

#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/ic/mb86960reg.h>
#include <dev/ic/mb86960var.h>
#include <dev/ic/fmvreg.h>
#include <dev/ic/fmvvar.h>

#include <dev/isa/isavar.h>

int	fmv_isa_match(device_t, cfdata_t, void *);
void	fmv_isa_attach(device_t, device_t, void *);

struct fmv_isa_softc {
	struct	mb86960_softc sc_mb86960;	/* real "mb86960" softc */

	/* ISA-specific goo. */
	void	*sc_ih;				/* interrupt cookie */
};

CFATTACH_DECL_NEW(fmv_isa, sizeof(struct fmv_isa_softc),
    fmv_isa_match, fmv_isa_attach, NULL, NULL);

struct fe_simple_probe_struct {
	uint8_t port;	/* Offset from the base I/O address. */
	uint8_t mask;	/* Bits to be checked. */
	uint8_t bits;	/* Values to be compared against. */
};

static inline int fe_simple_probe(bus_space_tag_t, bus_space_handle_t,
    struct fe_simple_probe_struct const *);
static int fmv_find(bus_space_tag_t, bus_space_handle_t, int *, int *);

static const int fmv_iomap[8] = {
	0x220, 0x240, 0x260, 0x280, 0x2A0, 0x2C0, 0x300, 0x340
};
#define NFMV_IOMAP __arraycount(fmv_iomap)
#define FMV_NPORTS 0x20

#ifdef FMV_DEBUG
#define DPRINTF	printf
#else
#define DPRINTF	while (/* CONSTCOND */0) printf
#endif

/*
 * Hardware probe routines.
 */

/*
 * Determine if the device is present.
 */
int
fmv_isa_match(device_t parent, cfdata_t cf, void *aux)
{
	struct isa_attach_args *ia = aux;
	bus_space_tag_t iot = ia->ia_iot;
	bus_space_handle_t ioh;
	int i, iobase, irq, rv = 0;
	uint8_t myea[ETHER_ADDR_LEN];

	if (ia->ia_nio < 1)
		return 0;
	if (ia->ia_nirq < 1)
		return 0;

	if (ISA_DIRECT_CONFIG(ia))
		return 0;

	/* Disallow wildcarded values. */
	if (ia->ia_io[0].ir_addr == ISA_UNKNOWN_PORT)
		return 0;

	/*
	 * See if the sepcified address is valid for FMV-180 series.
	 */
	for (i = 0; i < NFMV_IOMAP; i++)
		if (fmv_iomap[i] == ia->ia_io[0].ir_addr)
			break;
	if (i == NFMV_IOMAP) {
		DPRINTF("%s: unknown iobase 0x%x\n",
		    __func__, ia->ia_io[0].ir_addr);
		return 0;
	}

	/* Map i/o space. */
	if (bus_space_map(iot, ia->ia_io[0].ir_addr, FMV_NPORTS, 0, &ioh)) {
		DPRINTF("%s: couldn't map iospace 0x%x\n",
		    __func__, ia->ia_io[0].ir_addr);
		return 0;
	}

	if (fmv_find(iot, ioh, &iobase, &irq) == 0) {
		DPRINTF("%s: fmv_find failed\n", __func__);
		goto out;
	}

	if (iobase != ia->ia_io[0].ir_addr) {
		DPRINTF("%s: unexpected iobase in board: 0x%x\n",
		    __func__, iobase);
		goto out;
	}

	if (fmv_detect(iot, ioh, myea) == 0) { /* XXX necessary? */
		DPRINTF("%s: fmv_detect failed\n", __func__);
		goto out;
	}

	if (ia->ia_irq[0].ir_irq != ISA_UNKNOWN_IRQ) {
		if (ia->ia_irq[0].ir_irq != irq) {
			aprint_error("%s: irq mismatch; "
			    "kernel configured %d != board configured %d\n",
			    __func__, ia->ia_irq[0].ir_irq, irq);
			goto out;
		}
	} else
		ia->ia_irq[0].ir_irq = irq;

	ia->ia_nio = 1;
	ia->ia_io[0].ir_size = FMV_NPORTS;

	ia->ia_nirq = 1;

	ia->ia_niomem = 0;
	ia->ia_ndrq = 0;

	rv = 1;

 out:
	bus_space_unmap(iot, ioh, FMV_NPORTS);
	return rv;
}

/*
 * Check for specific bits in specific registers have specific values.
 */
static inline int
fe_simple_probe(bus_space_tag_t iot, bus_space_handle_t ioh,
    const struct fe_simple_probe_struct *sp)
{
	uint8_t val;
	const struct fe_simple_probe_struct *p;

	for (p = sp; p->mask != 0; p++) {
		val = bus_space_read_1(iot, ioh, p->port);
		if ((val & p->mask) != p->bits) {
			DPRINTF("%s: %x & %x != %x\n",
			    __func__, val, p->mask, p->bits);
			return 0;
		}
	}

	return 1;
}

/*
 * Hardware (vendor) specific probe routines.
 */

/*
 * Probe Fujitsu FMV-180 series boards and get iobase and irq from
 * board.
 */
static int
fmv_find(bus_space_tag_t iot, bus_space_handle_t ioh, int *iobase, int *irq)
{
	uint8_t config;
	static const int fmv_irqmap[4] = { 3, 7, 10, 15 };
	static const struct fe_simple_probe_struct probe_table[] = {
		{ FE_DLCR2, 0x70, 0x00 },
		{ FE_DLCR4, 0x08, 0x00 },
	    /*	{ FE_DLCR5, 0x80, 0x00 },	Doesn't work. */

		{ FE_FMV0, FE_FMV0_MAGIC_MASK, FE_FMV0_MAGIC_VALUE },
		{ FE_FMV1, FE_FMV1_MAGIC_MASK, FE_FMV1_MAGIC_VALUE },
		{ FE_FMV3, FE_FMV3_EXTRA_MASK, FE_FMV3_EXTRA_VALUE },
#if 1
	/*
	 * Test *vendor* part of the station address for Fujitsu.
	 * The test will gain reliability of probe process, but
	 * it rejects FMV-180 clone boards manufactured by other vendors.
	 * We have to turn the test off when such cards are made available.
	 */
		{ FE_FMV4, 0xFF, 0x00 },
		{ FE_FMV5, 0xFF, 0x00 },
		{ FE_FMV6, 0xFF, 0x0E },
#else
	/*
	 * We can always verify the *first* 2 bits (in Ehternet
	 * bit order) are "no multicast" and "no local" even for
	 * unknown vendors.
	 */
		{ FE_FMV4, 0x03, 0x00 },
#endif
		{ 0,	   0x00, 0x00 },
	};

	/* Simple probe. */
	if (fe_simple_probe(iot, ioh, probe_table) == 0)
		return 0;

	/* Check if our I/O address matches config info on EEPROM. */
	config = bus_space_read_1(iot, ioh, FE_FMV2);
	*iobase = fmv_iomap[(config & FE_FMV2_ADDR) >> FE_FMV2_ADDR_SHIFT];

	/*
	 * Determine which IRQ to be used.
	 *
	 * In this version, we always get an IRQ assignment from the
	 * FMV-180's configuration EEPROM, ignoring that specified in
	 * config file.
	 */
	*irq = fmv_irqmap[(config & FE_FMV2_IRQ) >> FE_FMV2_IRQ_SHIFT];

	return 1;
}

void
fmv_isa_attach(device_t parent, device_t self, void *aux)
{
	struct fmv_isa_softc *isc = device_private(self);
	struct mb86960_softc *sc = &isc->sc_mb86960;
	struct isa_attach_args *ia = aux;
	bus_space_tag_t iot = ia->ia_iot;
	bus_space_handle_t ioh;

	sc->sc_dev = self;

	/* Map i/o space. */
	if (bus_space_map(iot, ia->ia_io[0].ir_addr, FMV_NPORTS, 0, &ioh)) {
		aprint_error(": can't map i/o space\n");
		return;
	}

	sc->sc_bst = iot;
	sc->sc_bsh = ioh;

	fmv_attach(sc);

	/* Establish the interrupt handler. */
	isc->sc_ih = isa_intr_establish(ia->ia_ic, ia->ia_irq[0].ir_irq,
	    IST_EDGE, IPL_NET, mb86960_intr, sc);
	if (isc->sc_ih == NULL)
		aprint_error_dev(sc->sc_dev,
		    "couldn't establish interrupt handler\n");
}
