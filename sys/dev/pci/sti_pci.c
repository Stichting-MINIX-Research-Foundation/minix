/*	$NetBSD: sti_pci.c,v 1.1 2010/11/09 12:24:48 skrll Exp $	*/

/*	$OpenBSD: sti_pci.c,v 1.7 2009/02/06 22:51:04 miod Exp $	*/

/*
 * Copyright (c) 2006, 2007 Miodrag Vallat.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice, this permission notice, and the disclaimer below
 * appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/wscons/wsdisplayvar.h>

#include <dev/ic/stireg.h>
#include <dev/ic/stivar.h>

#ifdef STIDEBUG
#define	DPRINTF(s)	do {	\
	if (stidebug)		\
		printf s;	\
} while(0)

extern int stidebug;
#else
#define	DPRINTF(s)	/* */
#endif

int	sti_pci_match(device_t, cfdata_t, void *);
void	sti_pci_attach(device_t, device_t, void *);

void	sti_pci_end_attach(device_t dev);

struct	sti_pci_softc {
	device_t		sc_dev;

	struct sti_softc	sc_base;

	pci_chipset_tag_t	sc_pc;
	pcitag_t		sc_tag;

	bus_space_handle_t	sc_romh;
};

CFATTACH_DECL_NEW(sti_pci, sizeof(struct sti_pci_softc),
    sti_pci_match, sti_pci_attach, NULL, NULL);

int	sti_readbar(struct sti_softc *, struct pci_attach_args *, u_int, int);
int	sti_check_rom(struct sti_pci_softc *, struct pci_attach_args *);
void	sti_pci_enable_rom(struct sti_softc *);
void	sti_pci_disable_rom(struct sti_softc *);
void	sti_pci_enable_rom_internal(struct sti_pci_softc *);
void	sti_pci_disable_rom_internal(struct sti_pci_softc *);

int	sti_pci_is_console(struct pci_attach_args *, bus_addr_t *);

#define PCI_ROM_ENABLE                  0x00000001
#define PCI_ROM_ADDR_MASK               0xfffff800
#define PCI_ROM_ADDR(mr)                                                \
            ((mr) & PCI_ROM_ADDR_MASK)
#define PCI_ROM_SIZE(mr)                                                \
            (PCI_ROM_ADDR(mr) & -PCI_ROM_ADDR(mr))

int
sti_pci_match(device_t parent, cfdata_t cf, void *aux)
{
	struct pci_attach_args *paa = aux;

	if (PCI_VENDOR(paa->pa_id) != PCI_VENDOR_HP)
		return 0;

	if (PCI_PRODUCT(paa->pa_id) == PCI_PRODUCT_HP_VISUALIZE_EG ||
	    PCI_PRODUCT(paa->pa_id) == PCI_PRODUCT_HP_VISUALIZE_FX2 ||
	    PCI_PRODUCT(paa->pa_id) == PCI_PRODUCT_HP_VISUALIZE_FX4 ||
	    PCI_PRODUCT(paa->pa_id) == PCI_PRODUCT_HP_VISUALIZE_FX6 ||
	    PCI_PRODUCT(paa->pa_id) == PCI_PRODUCT_HP_VISUALIZE_FXE)
		return 1;

	return 0;
}

void
sti_pci_attach(device_t parent, device_t self, void *aux)
{
	struct sti_pci_softc *spc = device_private(self);
	struct pci_attach_args *paa = aux;
	int ret;

	spc->sc_dev = self;

	spc->sc_pc = paa->pa_pc;
	spc->sc_tag = paa->pa_tag;
	spc->sc_base.sc_dev = self;
	spc->sc_base.sc_enable_rom = sti_pci_enable_rom;
	spc->sc_base.sc_disable_rom = sti_pci_disable_rom;

	aprint_normal("\n");

	if (sti_check_rom(spc, paa) != 0)
		return;

	aprint_normal("%s", device_xname(self));
	ret = sti_pci_is_console(paa, spc->sc_base. bases);
	if (ret != 0)
		spc->sc_base.sc_flags |= STI_CONSOLE;

	ret = sti_attach_common(&spc->sc_base, paa->pa_iot, paa->pa_memt,
	    spc->sc_romh, STI_CODEBASE_MAIN);
	if (ret == 0)
		config_interrupts(self, sti_pci_end_attach);

}

void sti_pci_end_attach(device_t dev)
{
	struct sti_pci_softc *spc = device_private(dev);
	struct sti_softc *sc = &spc->sc_base;

	sti_end_attach(sc);
}


/*
 * Grovel the STI ROM image.
 */
int
sti_check_rom(struct sti_pci_softc *spc, struct pci_attach_args *pa)
{
	struct sti_softc *sc = &spc->sc_base;
	pcireg_t address, mask;
	bus_space_handle_t romh;
	bus_size_t romsize, subsize, stiromsize;
	bus_addr_t selected, offs, suboffs;
	uint32_t tmp;
	int i;
	int rc;

	/* sort of inline sti_pci_enable_rom(sc) */
	address = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_MAPREG_ROM);
	pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_MAPREG_ROM, ~PCI_ROM_ENABLE);
	mask = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_MAPREG_ROM);
	address |= PCI_ROM_ENABLE;
	pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_MAPREG_ROM, address);
	sc->sc_flags |= STI_ROM_ENABLED;
	/*
	 * Map the complete ROM for now.
	 */

	romsize = PCI_ROM_SIZE(mask);
	DPRINTF(("%s: mapping rom @ %lx for %lx\n", __func__,
	    (long)PCI_ROM_ADDR(address), (long)romsize));

	rc = bus_space_map(pa->pa_memt, PCI_ROM_ADDR(address), romsize,
	    0, &romh);
	if (rc != 0) {
		aprint_error_dev(sc->sc_dev, "can't map PCI ROM (%d)\n", rc);
		goto fail2;
	}

	sti_pci_disable_rom_internal(spc);
	/*
	 * Iterate over the ROM images, pick the best candidate.
	 */

	selected = (bus_addr_t)-1;
	for (offs = 0; offs < romsize; offs += subsize) {
		sti_pci_enable_rom_internal(spc);
		/*
		 * Check for a valid ROM header.
		 */
		tmp = bus_space_read_4(pa->pa_memt, romh, offs + 0);
		tmp = le32toh(tmp);
		if (tmp != 0x55aa0000) {
			sti_pci_disable_rom_internal(spc);
			if (offs == 0) {
				aprint_error_dev(sc->sc_dev,
				    "invalid PCI ROM header signature (%08x)\n",
				     tmp);
				rc = EINVAL;
			}
			break;
		}

		/*
		 * Check ROM type.
		 */
		tmp = bus_space_read_4(pa->pa_memt, romh, offs + 4);
		tmp = le32toh(tmp);
		if (tmp != 0x00000001) {	/* 1 == STI ROM */
			sti_pci_disable_rom_internal(spc);
			if (offs == 0) {
				aprint_error_dev(sc->sc_dev,
				    "invalid PCI ROM type (%08x)\n", tmp);
				rc = EINVAL;
			}
			break;
		}

		subsize = (bus_addr_t)bus_space_read_2(pa->pa_memt, romh,
		    offs + 0x0c);
		subsize <<= 9;

#ifdef STIDEBUG
		sti_pci_disable_rom_internal(spc);
		DPRINTF(("ROM offset %08x size %08x type %08x",
		    (u_int)offs, (u_int)subsize, tmp));
		sti_pci_enable_rom_internal(spc);
#endif

		/*
		 * Check for a valid ROM data structure.
		 * We do not need it except to know what architecture the ROM
		 * code is for.
		 */

		suboffs = offs +(bus_addr_t)bus_space_read_2(pa->pa_memt, romh,
		    offs + 0x18);
		tmp = bus_space_read_4(pa->pa_memt, romh, suboffs + 0);
		tmp = le32toh(tmp);
		if (tmp != 0x50434952) {	/* PCIR */
			sti_pci_disable_rom_internal(spc);
			if (offs == 0) {
				aprint_error_dev(sc->sc_dev, "invalid PCI data"
				    " signature (%08x)\n", tmp);
				rc = EINVAL;
			} else {
				DPRINTF((" invalid PCI data signature %08x\n",
				    tmp));
				continue;
			}
		}

		tmp = bus_space_read_1(pa->pa_memt, romh, suboffs + 0x14);
		sti_pci_disable_rom_internal(spc);
		DPRINTF((" code %02x", tmp));

		switch (tmp) {
#ifdef __hppa__
		case 0x10:
			if (selected == (bus_addr_t)-1)
				selected = offs;
			break;
#endif
#ifdef __i386__
		case 0x00:
			if (selected == (bus_addr_t)-1)
				selected = offs;
			break;
#endif
		default:
#ifdef STIDEBUG
			DPRINTF((" (wrong architecture)"));
#endif
			break;
		}
		DPRINTF(("%s\n", selected == offs ? " -> SELECTED" : ""));
	}

	if (selected == (bus_addr_t)-1) {
		if (rc == 0) {
			aprint_error_dev(sc->sc_dev, "found no ROM with "
			    "correct microcode architecture\n");
			rc = ENOEXEC;
		}
		goto fail;
	}

	/*
	 * Read the STI region BAR assignments.
	 */

	sti_pci_enable_rom_internal(spc);
	offs = selected +
	    (bus_addr_t)bus_space_read_2(pa->pa_memt, romh, selected + 0x0e);
	for (i = 0; i < STI_REGION_MAX; i++) {
		rc = sti_readbar(sc, pa, i,
		    bus_space_read_1(pa->pa_memt, romh, offs + i));
		if (rc != 0)
			goto fail;
	}

	/*
	 * Find out where the STI ROM itself lies, and its size.
	 */

	offs = selected +
	    (bus_addr_t)bus_space_read_4(pa->pa_memt, romh, selected + 0x08);
	stiromsize = (bus_addr_t)bus_space_read_4(pa->pa_memt, romh,
	    offs + 0x18);
	stiromsize = le32toh(stiromsize);
	sti_pci_disable_rom_internal(spc);

	/*
	 * Replace our mapping with a smaller mapping of only the area
	 * we are interested in.
	 */

	DPRINTF(("remapping rom @ %lx for %lx\n",
	    (long)(PCI_ROM_ADDR(address) + offs), (long)stiromsize));
	bus_space_unmap(pa->pa_memt, romh, romsize);
	rc = bus_space_map(pa->pa_memt, PCI_ROM_ADDR(address) + offs,
	    stiromsize, 0, &spc->sc_romh);
	if (rc != 0) {
		aprint_error_dev(sc->sc_dev, "can't map STI ROM (%d)\n",
		    rc);
		goto fail2;
	}
 	sti_pci_disable_rom_internal(spc);
	sc->sc_flags &= ~STI_ROM_ENABLED;

	return 0;

fail:
	bus_space_unmap(pa->pa_memt, romh, romsize);
fail2:
	sti_pci_disable_rom_internal(spc);

	return rc;
}

/*
 * Decode a BAR register.
 */
int
sti_readbar(struct sti_softc *sc, struct pci_attach_args *pa, u_int region,
    int bar)
{
	bus_addr_t addr;
	bus_size_t size;
	uint32_t cf;
	int rc;

	if (bar == 0) {
		sc->bases[region] = 0;
		return (0);
	}

#ifdef DIAGNOSTIC
	if (bar < PCI_MAPREG_START || bar > PCI_MAPREG_PPB_END) {
		sti_pci_disable_rom(sc);
		printf("%s: unexpected bar %02x for region %d\n",
		    device_xname(sc->sc_dev), bar, region);
		sti_pci_enable_rom(sc);
	}
#endif

	cf = pci_conf_read(pa->pa_pc, pa->pa_tag, bar);

	rc = pci_mapreg_info(pa->pa_pc, pa->pa_tag, bar, PCI_MAPREG_TYPE(cf),
	    &addr, &size, NULL);

	if (rc != 0) {
		sti_pci_disable_rom(sc);
		aprint_error_dev(sc->sc_dev, "invalid bar %02x for region %d\n",
		    bar, region);
		sti_pci_enable_rom(sc);
		return (rc);
	}

	sc->bases[region] = addr;
	return (0);
}

/*
 * Enable PCI ROM.
 */
void
sti_pci_enable_rom_internal(struct sti_pci_softc *spc)
{
	pcireg_t address;

	KASSERT(spc != NULL);

	address = pci_conf_read(spc->sc_pc, spc->sc_tag, PCI_MAPREG_ROM);
	address |= PCI_ROM_ENABLE;
	pci_conf_write(spc->sc_pc, spc->sc_tag, PCI_MAPREG_ROM, address);
}

void
sti_pci_enable_rom(struct sti_softc *sc)
{
	struct sti_pci_softc *spc = device_private(sc->sc_dev);

	if (!ISSET(sc->sc_flags, STI_ROM_ENABLED)) {
		sti_pci_enable_rom_internal(spc);
	}
	SET(sc->sc_flags, STI_ROM_ENABLED);
}

/*
 * Disable PCI ROM.
 */
void
sti_pci_disable_rom_internal(struct sti_pci_softc *spc)
{
	pcireg_t address;

	KASSERT(spc != NULL);

	address = pci_conf_read(spc->sc_pc, spc->sc_tag, PCI_MAPREG_ROM);
	address &= ~PCI_ROM_ENABLE;
	pci_conf_write(spc->sc_pc, spc->sc_tag, PCI_MAPREG_ROM, address);
}

void
sti_pci_disable_rom(struct sti_softc *sc)
{
	struct sti_pci_softc *spc = device_private(sc->sc_dev);

	if (ISSET(sc->sc_flags, STI_ROM_ENABLED)) {
		sti_pci_disable_rom_internal(spc);
	}
	CLR(sc->sc_flags, STI_ROM_ENABLED);
}
