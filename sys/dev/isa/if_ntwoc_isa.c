/*	$NetBSD: if_ntwoc_isa.c,v 1.25 2014/03/23 02:45:02 christos Exp $	*/
/*
 * Copyright (c) 1999 Christian E. Hopps
 * Copyright (c) 1996 John Hay.
 * Copyright (c) 1996 SDL Communications, Inc.
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
 * 3. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: if_ntwoc_isa.c,v 1.25 2014/03/23 02:45:02 christos Exp $
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_ntwoc_isa.c,v 1.25 2014/03/23 02:45:02 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/mbuf.h>
#include <sys/socket.h>

#include <net/if.h>

#include <sys/cpu.h>
#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/isa/isavar.h>

#include <dev/ic/hd64570reg.h>
#include <dev/ic/hd64570var.h>

#include <dev/isa/if_ntwoc_isareg.h>

#if 1
#define NTWO_DEBUG
#endif

#ifdef NTWO_DEBUG
#define NTWO_DPRINTF(x) printf x
#else
#define NTWO_DPRINTF(x)
#endif

#if __NetBSD_Version__ >= 104160000
static	void ntwoc_isa_config_interrupts(device_t);
#else
#define	SCA_BASECLOCK	9830400
#endif

/* hard core 16k for now */
#define	NTWOC_WIN_SIZE	0x4000

struct ntwoc_isa_softc {
	/* Generic device stuff */
	device_t sc_dev;		/* Common to all devices */

	/* PCI chipset glue */
	void		*sc_ih;	/* Interrupt handler */
	isa_chipset_tag_t sc_ic;	/* ISA chipset handle */

	struct sca_softc sc_sca;	/* the SCA itself */
};

static  int ntwoc_isa_probe(device_t, cfdata_t, void *);
static  void ntwoc_isa_attach(device_t, device_t, void *);

static	void ntwoc_isa_clock_callback(void *, int, int);
static	void ntwoc_isa_dtr_callback(void *, int, int);
static	int ntwoc_isa_intr(void *);
static	void ntwoc_isa_get_clock(struct sca_port *, u_int8_t, u_int8_t,
    u_int8_t, u_int8_t);
static	void ntwoc_isa_setup_memory(struct sca_softc *sc);
static	void ntwoc_isa_shutdown(void *sc);

CFATTACH_DECL_NEW(ntwoc_isa, sizeof(struct ntwoc_isa_softc),
    ntwoc_isa_probe, ntwoc_isa_attach, NULL, NULL);

/*
 * Names for daughter card types.  These match the NTWOC_DB_* defines.
 */
const char *ntwoc_db_names[] = {
	"V.35", "Unknown 0x01", "Test", "Unknown 0x03",
	"RS232", "Unknown 0x05", "RS422", "None"
};

/* some weird offset XXX */
#define SCA_REG(r)	(((r) & 0xf) + (((r) & 0xf0) << 6))

/*
 * functions that read and write to the sca registers
 */
static void
ntwoc_isa_sca_write_1(struct sca_softc *sc, u_int reg, u_int8_t val)
{
	bus_space_write_1(sc->sc_iot, sc->scu_sca_ioh[(reg & 0xf0) >> 4],
	    (reg & 0xf), val);
}

static void
ntwoc_isa_sca_write_2(struct sca_softc *sc, u_int reg, u_int16_t val)
{
	bus_space_write_2(sc->sc_iot, sc->scu_sca_ioh[(reg & 0xf0) >> 4],
	    (reg & 0xf), val);
}

static u_int8_t
ntwoc_isa_sca_read_1(struct sca_softc *sc, u_int reg)
{
	return
	    bus_space_read_1(sc->sc_iot, sc->scu_sca_ioh[(reg & 0xf0) >> 4],
	    (reg & 0xf));
}

static u_int16_t
ntwoc_isa_sca_read_2(struct sca_softc *sc, u_int reg)
{
	return
	    bus_space_read_2(sc->sc_iot, sc->scu_sca_ioh[(reg & 0xf0) >> 4],
	    (reg & 0xf));
}

/*
 * set the correct window/page
 */
static void
ntwoc_isa_set_page(struct sca_softc *sca, bus_addr_t addr)
{
	u_int8_t psr;

	/* get old psr value replace old window with new */
	psr = bus_space_read_1(sca->sc_iot, sca->sc_ioh, NTWOC_PSR);
	psr &= ~NTWOC_PG_MSK;
	psr |= ((addr >> sca->scu_pageshift) & NTWOC_PG_MSK);
	bus_space_write_1(sca->sc_iot, sca->sc_ioh, NTWOC_PSR, psr);
}

/*
 * enable the memory window
 */
static void
ntwoc_isa_set_on(struct sca_softc *sca)
{
	u_int8_t pcr;

	/* get old value and add window enable */
	pcr = bus_space_read_1(sca->sc_iot, sca->sc_ioh, NTWOC_PCR);
	pcr |= NTWOC_PCR_MEM_WIN;
	bus_space_write_1(sca->sc_iot, sca->sc_ioh, NTWOC_PCR, pcr);
}

/*
 * turn off memory window
 */
static void
ntwoc_isa_set_off(struct sca_softc *sca)
{
	u_int8_t pcr;

	/* get old value and remove window enable */
	pcr = bus_space_read_1(sca->sc_iot, sca->sc_ioh, NTWOC_PCR);
	pcr &= ~NTWOC_PCR_MEM_WIN;
	bus_space_write_1(sca->sc_iot, sca->sc_ioh, NTWOC_PCR, pcr);
}

static int
ntwoc_isa_probe(device_t parent, cfdata_t match, void *aux)
{
	struct isa_attach_args *ia;
	bus_space_tag_t iot, memt;
	bus_space_handle_t ioh, memh, sca_ioh[16];
	int i, tmp, rv;
	int gotmem, gotsca[16];
	u_int32_t ioport;

	ia = (struct isa_attach_args *)aux;
	iot = ia->ia_iot;
	memt = ia->ia_memt;

	if (ia->ia_nio < 1)
		return (0);
	if (ia->ia_niomem < 1)
		return (0);
	if (ia->ia_nirq < 1)
		return (0);

	if (ISA_DIRECT_CONFIG(ia))
		return (0);

	memset(gotsca, 0, sizeof(gotsca));
	gotmem = rv = 0;

	/* disallow wildcarded I/O base */
	if (ia->ia_io[0].ir_addr == ISA_UNKNOWN_PORT) {
		printf("ntwoc_isa_probe: must specify port address\n");
		return (0);
	}

	if (ia->ia_irq[0].ir_irq == ISA_UNKNOWN_IRQ) {
		printf("ntwoc_isa_probe: must specify irq\n");
		return (0);
	}

	if (ia->ia_iomem[0].ir_addr == ISA_UNKNOWN_IOMEM) {
		printf("ntwoc_isa_probe: must specify iomem\n");
		return (0);
	}

	tmp = (match->cf_flags & NTWOC_FLAGS_NPORT_MASK) + 1;
	if (tmp < 1 || tmp > 2) {
		printf("ntwoc_isa_probe: only 1 or 2 ports allowed\n");
		return (0);
	}

	/* map the isa io addresses */
	if ((tmp = bus_space_map(iot, ia->ia_io[0].ir_addr,
	     NTWOC_SRC_IOPORT_SIZE, 0, &ioh))) {
		printf("ntwoc_isa_probe: mapping port 0x%x sz %d failed: %d\n",
		    ia->ia_io[0].ir_addr, NTWOC_SRC_IOPORT_SIZE, tmp);
		return (0);
	}

	ioport = ia->ia_io[0].ir_addr + 0x8000;
	for (i = 0; i < 16; ioport += (0x10 << 6), i++) {
		/* map the isa io addresses */
		if ((tmp = bus_space_map(iot, ioport, 16, 0, &sca_ioh[i]))) {
			printf(
			 "ntwoc_isa_probe: mapping sca 0x%x sz %d failed: %d\n",
			    ioport, 16, tmp);
			goto out;
		}
		gotsca[i] = 1;
	}

	/* map the isa memory addresses */
	/* XXX we really want the user to select this */
	if ((tmp = bus_space_map(ia->ia_memt, ia->ia_iomem[0].ir_addr,
	     NTWOC_WIN_SIZE, 0, &memh))) {
		printf("ntwoc_isa_probe: mapping mem 0x%x sz %d failed: %d\n",
		    ia->ia_iomem[0].ir_addr, NTWOC_WIN_SIZE, tmp);
		goto out;
	}
	gotmem = 1;

	/* turn off the card */
	bus_space_write_1(iot, ioh, NTWOC_PCR, 0);

	/*
	 * Next, we'll test the Base Address Register to retension of
	 * data... ... seeing if we're *really* talking to an N2.
	 */
	for (i = 0; i < 0x100; i++) {
		bus_space_write_1(iot, ioh, NTWOC_BAR, i);
		(void)bus_space_read_1(iot, ioh, NTWOC_PCR);
		if (bus_space_read_1(iot, ioh, NTWOC_BAR) != i) {
			printf("ntwoc_isa_probe failed (BAR %x, %x)\n", i, tmp);
			goto out;
		}
	}

	/* XXX XXX update the calls to SCA_REG to use our mapping */

	/*
	 * Now see if we can see the SCA.
	 */
	bus_space_write_1(iot, ioh, NTWOC_PCR,
	     NTWOC_PCR_SCARUN | bus_space_read_1(iot, ioh, NTWOC_PCR));
	bus_space_write_1(iot, sca_ioh[0], SCA_REG(SCA_WCRL), 0);
	bus_space_write_1(iot, sca_ioh[0], SCA_REG(SCA_WCRM), 0);
	bus_space_write_1(iot, sca_ioh[0], SCA_REG(SCA_WCRH), 0);
	bus_space_write_1(iot, sca_ioh[0], SCA_REG(SCA_PCR), 0);

	bus_space_write_1(iot, sca_ioh[0], SCA_REG(SCA_TMC0), 0);
	(void)bus_space_read_1(iot, ioh, 0);
	if ((tmp = bus_space_read_1(iot, sca_ioh[0], SCA_REG(SCA_TMC0))) != 0) {
		printf("ntwoc_isa_probe: Error reading SCA (TMC0 0, %x)\n",
		   tmp);
		goto out;
	}

	bus_space_write_1(iot, sca_ioh[0], SCA_REG(SCA_TMC0), 0x5A);
	(void)bus_space_read_1(iot, ioh, 0);

	tmp = bus_space_read_1(iot, sca_ioh[0], SCA_REG(SCA_TMC0));
	if (tmp != 0x5A) {
		printf("ntwoc_isa_probe: Error reading SCA (TMC0 5A, %x)\n",
		    tmp);
		goto out;
	}

	bus_space_write_2(iot, sca_ioh[0], SCA_REG(SCA_CDAL0), 0);
	(void)bus_space_read_1(iot, ioh, 0);
	tmp = bus_space_read_2(iot, sca_ioh[0], SCA_REG(SCA_CDAL0));
	if (tmp != 0) {
		printf("ntwoc_isa_probe: Error reading SCA (CDAL0 0, %x)\n",
		    tmp);
		goto out;
	}

	bus_space_write_2(iot, sca_ioh[0], SCA_REG(SCA_CDAL0), 0x55AA);
	(void)bus_space_read_1(iot, ioh, 0);
	tmp = bus_space_read_2(iot, sca_ioh[0], SCA_REG(SCA_CDAL0));
	if (tmp != 0x55AA) {
		printf("ntwoc_isa_probe: Error reading SCA (CDAL0 55AA, %x)\n",
		    tmp);
		goto out;
	}

	/*
	 * I had a weird card that didn't function correctly on a certain
	 * newer MB.  I suspect it was the whacky port addresses.
	 * The following correctly failed it.
	 */
	bus_space_write_2(iot, sca_ioh[0], SCA_REG(SCA_TCNTL0), 0x0);
	(void)bus_space_read_1(iot, ioh, 0);
	tmp = bus_space_read_2(iot, sca_ioh[0], SCA_REG(SCA_TCNTL0));
	if (tmp != 0) {
		printf("ntwoc_isa_probe: Error reading SCA (TCNTL0 0, %x)\n",
		    tmp);
		goto out;
	}

	bus_space_write_2(iot, sca_ioh[0], SCA_REG(SCA_TCNTL0), 0x55AA);
	(void)bus_space_read_1(iot, ioh, 0);
	tmp = bus_space_read_2(iot, sca_ioh[0], SCA_REG(SCA_TCNTL0));
	if (tmp != 0x55AA) {
		printf("ntwoc_isa_probe: Error reading SCA (TCNTL0 55AA, %x)\n",
		    tmp);
		goto out;
	}

	ia->ia_nio = 1;
	ia->ia_io[0].ir_size = NTWOC_SRC_IOPORT_SIZE;

	ia->ia_niomem = 1;
	ia->ia_iomem[0].ir_size = NTWOC_WIN_SIZE;

	ia->ia_nirq = 1;

	ia->ia_ndrq = 0;

	rv = 1;
out:
	/* turn off the card */
	bus_space_write_1(iot, ioh, NTWOC_PCR, 0);

	if (gotmem)
		bus_space_unmap(memt, memh, NTWOC_WIN_SIZE);
	for (i = 0; i < 16; i++) {
		if (gotsca[i])
			bus_space_unmap(iot, sca_ioh[i], 16);
	}
	bus_space_unmap(iot, ioh, NTWOC_SRC_IOPORT_SIZE);
	return (rv);
}

/*
 * we win! attach the card
 */
static void
ntwoc_isa_attach(device_t parent, device_t self, void *aux)
{
	struct ntwoc_isa_softc *sc;
	struct isa_attach_args *ia;
	struct sca_softc *sca;
	bus_addr_t addr;
	u_int8_t rdiv, tdiv, tmc;
	u_int32_t flags, ioport;
	u_int16_t tmp;
	int i, pgs, rv;

	ia = (struct isa_attach_args *)aux;
	sc = device_private(self);
	sc->sc_dev = self;
	sca = &sc->sc_sca;

	printf(": N2 Serial Interface\n");
	flags = device_cfdata(sc->sc_dev)->cf_flags;

	sc->sc_ic = ia->ia_ic;
	sca->sc_parent = sc->sc_dev;
	sca->sc_numports = (flags & NTWOC_FLAGS_NPORT_MASK) + 1;
	sca->sc_usedma = 0;
	sca->sc_aux = sc;
	sca->sc_dtr_callback = ntwoc_isa_dtr_callback;
	sca->sc_clock_callback = ntwoc_isa_clock_callback;
	sca->sc_read_1 = ntwoc_isa_sca_read_1;
	sca->sc_read_2 = ntwoc_isa_sca_read_2;
	sca->sc_write_1 = ntwoc_isa_sca_write_1;
	sca->sc_write_2 = ntwoc_isa_sca_write_2;
	sca->scu_set_page = ntwoc_isa_set_page;
	sca->scu_page_on = ntwoc_isa_set_on;
	sca->scu_page_off = ntwoc_isa_set_off;

	/* map the io */
	sca->sc_iot = ia->ia_iot;
	if ((rv = bus_space_map(ia->ia_iot, ia->ia_io[0].ir_addr,
	    NTWOC_SRC_IOPORT_SIZE, 0, &sca->sc_ioh))) {
		aprint_error_dev(sc->sc_dev, "can't map io 0x%x sz %d, %d\n",
		    ia->ia_io[0].ir_addr,
		    NTWOC_SRC_IOPORT_SIZE, rv);
		return;
	}

	/* support weird mapping (they used this to avoid 10-bit aliasing) */
	ioport = ia->ia_io[0].ir_addr + 0x8000;
	for (i = 0; i < 16; ioport += (0x10 << 6), i++) {
		/* map the isa io addresses */
		if ((tmp = bus_space_map(ia->ia_iot, ioport, 16, 0,
		    &sca->scu_sca_ioh[i]))) {
			aprint_error_dev(sc->sc_dev, "mapping sca 0x%x sz %d failed: %d\n",
			    ioport, 16, tmp);
			return;
		}
	}

	/* map the isa memory */
	sca->scu_memt = ia->ia_memt;
	sca->scu_pagesize = 0x4000;	/* force 16k for now */
	if (sca->scu_pagesize < 0x8000) {
		/* round down to 16k */
		sca->scu_pagesize = 0x4000;
		sca->scu_pageshift = 14;
		tmp = NTWOC_PSR_WIN_16K;
	} else if (sca->scu_pagesize < 0x10000) {
		/* round down to 32k */
		sca->scu_pagesize = 0x8000;
		sca->scu_pageshift = 15;
		tmp = NTWOC_PSR_WIN_32K;
	} else if (sca->scu_pagesize < 0x20000) {
		/* round down to 64k */
		sca->scu_pagesize = 0x10000;
		sca->scu_pageshift = 16;
		tmp = NTWOC_PSR_WIN_64K;
	} else {
		sca->scu_pagesize = 0x20000;
		sca->scu_pageshift = 17;
		tmp = NTWOC_PSR_WIN_128K;
	}
	sca->scu_pagemask = sca->scu_pagesize - 1;
	if ((rv = bus_space_map(ia->ia_memt, ia->ia_iomem[0].ir_addr,
	     sca->scu_pagesize, 0, &sca->scu_memh))) {
		aprint_error_dev(sc->sc_dev, "can't map mem 0x%x sz %ld, %d\n",
		    ia->ia_iomem[0].ir_addr,
		    (u_long)sca->scu_pagesize, rv);
		return;
	}

	/* turn the card on!! */
	bus_space_write_1(sca->sc_iot, sca->sc_ioh, NTWOC_PCR,
	    bus_space_read_1(sca->sc_iot, sca->sc_ioh, NTWOC_PCR)
	    | NTWOC_PCR_SCARUN);

	/* set the window size to 16k */
	bus_space_write_1(sca->sc_iot, sca->sc_ioh, NTWOC_PSR, tmp);

	/* reset mcr */
	bus_space_write_1(sca->sc_iot, sca->sc_ioh, NTWOC_MCR,
	    NTWOC_MCR_DTR0 | NTWOC_MCR_DTR1 | NTWOC_MCR_TE0 | NTWOC_MCR_TE1);


	/* allow for address above 1M and 16 bit i/o */
#if 0
	bus_space_write_1(sca->sc_iot, sca->sc_ioh, NTWOC_PCR,
	    bus_space_read_1(sca->sc_iot, sca->sc_ioh, NTWOC_PCR)
	    | NTWOC_PCR_EN_VPM | NTWOC_PCR_ISA16);
#endif
	bus_space_write_1(sca->sc_iot, sca->sc_ioh, NTWOC_PCR,
	    bus_space_read_1(sca->sc_iot, sca->sc_ioh, NTWOC_PCR)
	    | NTWOC_PCR_ISA16);

	/* program the card with the io address */
	bus_space_write_1(sca->sc_iot, sca->sc_ioh, NTWOC_PCR,
	    ((ia->ia_iomem[0].ir_addr >> 16) & NTWOC_PCR_16M_SEL)
	    |
	    (bus_space_read_1(sca->sc_iot, sca->sc_ioh, NTWOC_PCR)
	    & ~NTWOC_PCR_16M_SEL));
	bus_space_write_1(sca->sc_iot, sca->sc_ioh, NTWOC_BAR,
	    (ia->ia_iomem[0].ir_addr >> 12));

	/* enable the memory window */
	ntwoc_isa_set_on(sca);

	/*
	 * write a magic value into each possible page of memory
	 * incrementing by our window size
	 */
	addr = 0;
	for (i = 0; i <= NTWOC_PSR_PG_SEL; addr += sca->scu_pagesize, i++) {
		/* select the page */
		ntwoc_isa_set_page(sca, addr);
		bus_space_write_2(sca->scu_memt, sca->scu_memh, 0, 0xAA55);
	}

	/*
	 * go back through pages and verify that value is different
	 * after writing to previous page
	 */
	addr = 0;
	for (i = 0; i <= NTWOC_PSR_PG_SEL; addr += sca->scu_pagesize, i++) {
		ntwoc_isa_set_page(sca, addr);

		tmp = bus_space_read_2(sca->scu_memt, sca->scu_memh, 0);
		if (tmp != 0xAA55)
			break;

		/* write a different value into this page now */
		bus_space_write_2(sca->scu_memt, sca->scu_memh, 0, i);
	}
	sca->scu_npages = pgs = i;	/* final count of 16K pages */

	/* erase the pages */
	addr = 0;
	for (i = 0; i <= pgs; addr += sca->scu_pagesize, i++) {
		ntwoc_isa_set_page(sca, addr);
		bus_space_set_region_1(sca->scu_memt, sca->scu_memh, 0, 0,
		    sca->scu_pagesize);
	}

#if 0
	printf("%s: sca port 0x%x-0x%x dpram %ldk %d serial port%s\n",
	    device_xname(sc->sc_dev), ia->ia_io[0].ir_addr | 0x8000,
	    (ia->ia_io[0].ir_addr | 0x8000) + NTWOC_SRC_ASIC_SIZE - 1,
	    pgs * (sca->scu_pagesize / 1024), sca->sc_numports,
	    (sca->sc_numports > 1 ? "s" : ""));
#else
	printf("%s: dpram %ldk %d serial port%s\n",
	    device_xname(sc->sc_dev), (u_long)pgs * (sca->scu_pagesize / 1024),
	    sca->sc_numports, (sca->sc_numports > 1 ? "s" : ""));
#endif

	/* disable the memory window */
	ntwoc_isa_set_off(sca);

	/* enabled sca DMA */
	bus_space_write_1(sca->sc_iot, sca->sc_ioh, NTWOC_PSR,
	    bus_space_read_1(sca->sc_iot, sca->sc_ioh, NTWOC_PSR)
	    | NTWOC_PSR_EN_SCA_DMA);

	/* now establish our irq -- perhaps sanity check the value */
	sc->sc_ih = isa_intr_establish(ia->ia_ic, ia->ia_irq[0].ir_irq,
	    IST_EDGE, IPL_NET, ntwoc_isa_intr, sc);
	if (sc->sc_ih == NULL) {
		aprint_error_dev(sc->sc_dev, "can't establish interrupt\n");
		return;
	}

	/* make sure we have 2 pages for each port */
	if (pgs < 2 * sca->sc_numports) {
		printf("%s: %d less than required pages of memory of %d\n",
		    device_xname(sc->sc_dev), pgs, 2 * sca->sc_numports);
		return;
	}

	/* sca_get_base_clock(sca); */

	/*
	 * get clock information from user
	 */
	rdiv = (flags & NTWOC_FLAGS_RXDIV_MASK) >> NTWOC_FLAGS_RXDIV_SHIFT;
	if (rdiv > 9)
		panic("bad rx divisor in flags");

	tdiv = (flags & NTWOC_FLAGS_TXDIV_MASK) >> NTWOC_FLAGS_TXDIV_SHIFT;
	if (tdiv > 9)
		panic("bad tx divisor in flags");
	tmc = (flags & NTWOC_FLAGS_TMC_MASK) >> NTWOC_FLAGS_TMC_SHIFT;

	ntwoc_isa_get_clock(&sca->sc_ports[0],
	    flags & NTWOC_FLAGS_CLK0_MASK, tmc, rdiv, tdiv);
	if (sca->sc_numports > 1)
		ntwoc_isa_get_clock(&sca->sc_ports[1],
		    (flags & NTWOC_FLAGS_CLK1_MASK) >> NTWOC_FLAGS_CLK1_SHIFT,
		    tmc, rdiv, tdiv);

	ntwoc_isa_setup_memory(sca);

	sca_init(sca);

	/* attach configured ports */
	sca_port_attach(sca, 0);
	if (sca->sc_numports == 2)
		sca_port_attach(sca, 1);

	/*
	 * Add shutdown hook so that DMA is disabled prior to reboot. Not
	 * doing do could allow DMA to corrupt kernel memory during the
	 * reboot before the driver initializes.
	 */
	shutdownhook_establish(ntwoc_isa_shutdown, sc);

#if __NetBSD_Version__ >= 104160000
	/*
	 * defer getting the base clock until interrupts are enabled
	 * (and thus we have microtime())
	 */
	config_interrupts(self, ntwoc_isa_config_interrupts);
#else
	/* no callback pre 1.4-mumble */
	sca->sc_baseclock = SCA_BASECLOCK;
	sca_print_clock_info(&sc->sc_sca);
#endif
}

/*
 * extract the clock information for a port from the flags field
 */
static void
ntwoc_isa_get_clock(struct sca_port *scp, u_int8_t flags, u_int8_t tmc,
    u_int8_t rdiv, u_int8_t tdiv)
{
	scp->sp_eclock =
	    (flags & NTWOC_FLAGS_ECLOCK_MASK) >> NTWOC_FLAGS_ECLOCK_SHIFT;
	scp->sp_rxs = rdiv;
	scp->sp_txs = tdiv;
	scp->sp_tmc = tmc;

	/* get rx source */
	switch ((flags & NTWOC_FLAGS_RXS_MASK) >> NTWOC_FLAGS_RXS_SHIFT) {
	case NTWOC_FLAGS_RXS_LINE:
		scp->sp_rxs = 0;
		break;
	case NTWOC_FLAGS_RXS_LINE_SN:
		scp->sp_rxs |= SCA_RXS_CLK_LINE_SN;
		break;
	case NTWOC_FLAGS_RXS_INTERNAL:
		scp->sp_rxs |= SCA_RXS_CLK_INTERNAL;
		break;
	case NTWOC_FLAGS_RXS_ADPLL_OUT:
		scp->sp_rxs |= SCA_RXS_CLK_ADPLL_OUT;
		break;
	case NTWOC_FLAGS_RXS_ADPLL_IN:
		scp->sp_rxs |= SCA_RXS_CLK_ADPLL_IN;
		break;
	default:
		panic("bad rx source in flags");
	}

	/* get tx source */
	switch ((flags & NTWOC_FLAGS_TXS_MASK) >> NTWOC_FLAGS_TXS_SHIFT) {
	case NTWOC_FLAGS_TXS_LINE:
		scp->sp_txs = 0;
		break;
	case NTWOC_FLAGS_TXS_INTERNAL:
		scp->sp_txs |= SCA_TXS_CLK_INTERNAL;
		break;
	case NTWOC_FLAGS_TXS_RXCLOCK:
		scp->sp_txs |= SCA_TXS_CLK_RXCLK;
		break;
	default:
		panic("bad rx source in flags");
	}
}


static int
ntwoc_isa_intr(void *arg)
{
	struct ntwoc_isa_softc *sc = (struct ntwoc_isa_softc *)arg;

	return sca_hardintr(&sc->sc_sca);
}

/*
 * shut down interrupts and DMA, so we don't trash the kernel on warm
 * boot.  Also, lower DTR on each port and disable card interrupts.
 */
static void
ntwoc_isa_shutdown(void *aux)
{
	struct ntwoc_isa_softc *sc = aux;
	u_int16_t mcr;

	/*
	 * shut down the SCA ports
	 */
	sca_shutdown(&sc->sc_sca);

	/*
	 * lower DTR on both ports
	 */
	mcr = bus_space_read_1(sc->sc_sca.sc_iot, sc->sc_sca.sc_ioh, NTWOC_MCR);
	mcr |= (NTWOC_MCR_DTR0 | NTWOC_MCR_DTR1);
	bus_space_write_1(sc->sc_sca.sc_iot, sc->sc_sca.sc_ioh, NTWOC_MCR, mcr);

	/* turn off the card */
	bus_space_write_1(sc->sc_sca.sc_iot, sc->sc_sca.sc_ioh, NTWOC_PCR, 0);
}

static void
ntwoc_isa_dtr_callback(void *aux, int port, int state)
{
	struct ntwoc_isa_softc *sc = aux;
	u_int8_t mcr;

	mcr = bus_space_read_1(sc->sc_sca.sc_iot, sc->sc_sca.sc_ioh, NTWOC_MCR);

	NTWO_DPRINTF(("port == %d, state == %d, old mcr:  0x%02x\n",
	    port, state, mcr));

	if (port == 0) {
		if (state == 0)
			mcr |= NTWOC_MCR_DTR0;
		else
			mcr &= ~NTWOC_MCR_DTR0;
	} else {
		if (state == 0)
			mcr |= NTWOC_MCR_DTR1;
		else
			mcr &= ~NTWOC_MCR_DTR1;
	}

	NTWO_DPRINTF(("new mcr:  0x%02x\n", mcr));

	bus_space_write_1(sc->sc_sca.sc_iot, sc->sc_sca.sc_ioh, NTWOC_MCR, mcr);
}

static void
ntwoc_isa_clock_callback(void *aux, int port, int enable)
{
	struct ntwoc_isa_softc *sc = aux;
	u_int8_t mcr;

	mcr = bus_space_read_1(sc->sc_sca.sc_iot, sc->sc_sca.sc_ioh, NTWOC_MCR);

	NTWO_DPRINTF(("clock: port == %d, enable == %d, old mcr:  0x%02x\n",
	    port, enable, mcr));

	if (port == 0) {
		if (enable == 0)
			mcr &= ~NTWOC_MCR_ETC0;
		else
			mcr |= NTWOC_MCR_ETC0;
	} else {
		if (enable == 0)
			mcr &= ~NTWOC_MCR_ETC1;
		else
			mcr |= NTWOC_MCR_ETC1;
	}

	NTWO_DPRINTF(("clock: new mcr:  0x%02x\n", mcr));

	bus_space_write_1(sc->sc_sca.sc_iot, sc->sc_sca.sc_ioh, NTWOC_MCR, mcr);
}

static void
ntwoc_isa_setup_memory(struct sca_softc *sc)
{
	struct sca_port *scp;
	u_int i, j;

	/* allocate enough descriptors for a full page */

	sc->sc_ports[0].sp_ntxdesc = (sc->scu_pagesize / SCA_BSIZE) - 1;
	sc->sc_ports[0].sp_nrxdesc = (sc->scu_pagesize / SCA_BSIZE) - 1;
	if (sc->sc_numports == 2) {
		sc->sc_ports[1].sp_ntxdesc = sc->sc_ports[0].sp_ntxdesc;
		sc->sc_ports[1].sp_nrxdesc = sc->sc_ports[0].sp_nrxdesc;
	}

	j = 0;
	for (i = 0; i < sc->sc_numports; i++) {
		scp = &sc->sc_ports[i];
		scp->sp_txdesc_p = (bus_addr_t)(j * sc->scu_pagesize);
		scp->sp_txdesc = (void *)(uintptr_t)scp->sp_txdesc_p;
		scp->sp_txbuf_p = scp->sp_txdesc_p;
		scp->sp_txbuf_p += SCA_BSIZE;
		scp->sp_txbuf = (void *)(uintptr_t)scp->sp_txbuf_p;
		j++;

		scp->sp_rxdesc_p = (bus_addr_t)(j * sc->scu_pagesize);
		scp->sp_rxdesc = (void *)(uintptr_t)scp->sp_txdesc_p;
		scp->sp_rxbuf_p = scp->sp_rxdesc_p;
		scp->sp_rxbuf_p += SCA_BSIZE;
		scp->sp_rxbuf = (void *)(uintptr_t)scp->sp_rxbuf_p;
		j++;
	}
}

#if __NetBSD_Version__ >= 104160000
/*
 * get the base clock frequency
 */
static void
ntwoc_isa_config_interrupts(device_t self)
{
	struct ntwoc_isa_softc *sc;

	sc = device_private(self);
	sca_get_base_clock(&sc->sc_sca);
	sca_print_clock_info(&sc->sc_sca);
}
#endif
