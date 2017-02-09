/*	$NetBSD: igma.c,v 1.2 2014/07/13 01:02:20 mlelstv Exp $	*/

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

/*
 * Intel Graphic Media Accelerator
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: igma.c,v 1.2 2014/07/13 01:02:20 mlelstv Exp $");

#include "vga.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/bus.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>
#include <dev/pci/pciio.h>

#include <dev/i2c/i2cvar.h>
#include <dev/i2c/i2c_bitbang.h>
#include <dev/i2c/ddcvar.h>

#include <dev/videomode/videomode.h>
#include <dev/videomode/edidvar.h>

#include <dev/wscons/wsdisplayvar.h>

#if NVGA > 0
#include <dev/ic/mc6845reg.h>
#include <dev/ic/pcdisplayvar.h>
#include <dev/ic/vgareg.h>
#include <dev/ic/vgavar.h>
#endif

#include <dev/pci/igmareg.h>
#include <dev/pci/igmavar.h>

#include "igmafb.h"

struct igma_softc;
struct igma_i2c {
	kmutex_t		ii_lock;
	struct igma_softc	*ii_sc;
	bus_addr_t		ii_reg;
	struct i2c_controller	ii_i2c;
	const char		*ii_name;
	u_int32_t		ii_dir;
};

struct igma_softc {
	device_t		sc_dev;
	struct igma_chip        sc_chip;
	struct igma_i2c		sc_ii[GMBUS_NUM_PORTS];
};

static int igma_match(device_t, cfdata_t, void *);
static void igma_attach(device_t, device_t, void *);
static int igma_print(void *, const char *);

static void igma_i2c_attach(struct igma_softc *);

CFATTACH_DECL_NEW(igma, sizeof(struct igma_softc),
    igma_match, igma_attach, NULL, NULL);

static int igma_i2c_acquire_bus(void *, int);
static void igma_i2c_release_bus(void *, int);
static int igma_i2c_send_start(void *, int);
static int igma_i2c_send_stop(void *, int);
static int igma_i2c_initiate_xfer(void *, i2c_addr_t, int);
static int igma_i2c_read_byte(void *, uint8_t *, int);
static int igma_i2c_write_byte(void *, uint8_t, int);
static void igma_i2cbb_set_bits(void *, uint32_t);
static void igma_i2cbb_set_dir(void *, uint32_t);
static uint32_t igma_i2cbb_read(void *);

static void igma_reg_barrier(const struct igma_chip *, int);
static u_int32_t igma_reg_read(const struct igma_chip *, int);
static void igma_reg_write(const struct igma_chip *, int, u_int32_t);
static u_int8_t igma_vga_read(const struct igma_chip *, int);
static void igma_vga_write(const struct igma_chip *, int , u_int8_t);
#if 0
static u_int8_t igma_crtc_read(const struct igma_chip *, int);
static void igma_crtc_write(const struct igma_chip *, int, u_int8_t);
#endif

static const struct i2c_bitbang_ops igma_i2cbb_ops = {
	igma_i2cbb_set_bits,
	igma_i2cbb_set_dir,
	igma_i2cbb_read,
	{ 1, 2, 0, 1 }
};

static const struct igma_chip_ops igma_bus_ops = {
	igma_reg_barrier,
	igma_reg_read,
	igma_reg_write,
	igma_vga_read,
	igma_vga_write,
#if 0
	igma_crtc_read,
	igma_crtc_write,
#endif
};

static struct igma_product {
        u_int16_t product;
	int gentype;
	int num_pipes;
} const igma_products[] = {
	/* i830 */
	{ PCI_PRODUCT_INTEL_82830MP_IV,           200,2 },
	/* i845g */
	{ PCI_PRODUCT_INTEL_82845G_IGD,           200,2 },
	/* i85x */
	{ PCI_PRODUCT_INTEL_82855GM_IGD,          200,2 },
// 0x358e ?
	/* i865g */
	{ PCI_PRODUCT_INTEL_82865_IGD,            200,2 },
	/* i915g */
	{ PCI_PRODUCT_INTEL_82915G_IGD,           200,2 },
	{ PCI_PRODUCT_INTEL_E7221_IGD,            200,2 },
	/* i915gm */
	{ PCI_PRODUCT_INTEL_82915GM_IGD,          300,2 },
	/* i945g */
	{ PCI_PRODUCT_INTEL_82945P_IGD,           300,2 },
	/* i945gm */
	{ PCI_PRODUCT_INTEL_82945GM_IGD,          300,2 },
	{ PCI_PRODUCT_INTEL_82945GM_IGD_1,        300,2 },
	{ PCI_PRODUCT_INTEL_82945GME_IGD,         300,2 },
	/* i965g */
	{ PCI_PRODUCT_INTEL_82946GZ_IGD,          300,2 },
	{ PCI_PRODUCT_INTEL_82G35_IGD,            300,2 },
	{ PCI_PRODUCT_INTEL_82G35_IGD_1,          300,2 },
	{ PCI_PRODUCT_INTEL_82965Q_IGD,           300,2 },
	{ PCI_PRODUCT_INTEL_82965Q_IGD_1,         300,2 },
	{ PCI_PRODUCT_INTEL_82965G_IGD,           300,2 },
	{ PCI_PRODUCT_INTEL_82965G_IGD_1,         300,2 },
	/* g33 */
	{ PCI_PRODUCT_INTEL_82G33_IGD,            300,2 },
	{ PCI_PRODUCT_INTEL_82G33_IGD_1,          300,2 },
	{ PCI_PRODUCT_INTEL_82Q33_IGD,            300,2 },
	{ PCI_PRODUCT_INTEL_82Q33_IGD_1,          300,2 },
	{ PCI_PRODUCT_INTEL_82Q35_IGD,            300,2 },
	{ PCI_PRODUCT_INTEL_82Q35_IGD_1,          300,2 },
	/* pineview */
	{ PCI_PRODUCT_INTEL_PINEVIEW_IGD,         350,2 },
	{ PCI_PRODUCT_INTEL_PINEVIEW_M_IGD,       350,2 },
	/* i965gm */
	{ PCI_PRODUCT_INTEL_82965PM_IGD,          400,2 },
	{ PCI_PRODUCT_INTEL_82965PM_IGD_1,        400,2 },
	{ PCI_PRODUCT_INTEL_82965GME_IGD,         400,2 },
	/* gm45 */
	{ PCI_PRODUCT_INTEL_82GM45_IGD,           450,2 },
	{ PCI_PRODUCT_INTEL_82GM45_IGD_1,         450,2 },
	/* g45 */
	{ PCI_PRODUCT_INTEL_82IGD_E_IGD,          450,2 },
	{ PCI_PRODUCT_INTEL_82Q45_IGD,            450,2 },
	{ PCI_PRODUCT_INTEL_82G45_IGD,            450,2 },
	{ PCI_PRODUCT_INTEL_82G41_IGD,            450,2 },
	{ PCI_PRODUCT_INTEL_82B43_IGD,            450,2 },
// 0x2e92 ?
	/* ironlake d */
	{ PCI_PRODUCT_INTEL_IRONLAKE_D_IGD,       500,2 },
	/* ironlake m */
	{ PCI_PRODUCT_INTEL_IRONLAKE_M_IGD,       500,2 },
	/* sandy bridge */
	{ PCI_PRODUCT_INTEL_SANDYBRIDGE_IGD,      600,2 },
	{ PCI_PRODUCT_INTEL_SANDYBRIDGE_IGD_1,    600,2 },
	{ PCI_PRODUCT_INTEL_SANDYBRIDGE_IGD_2,    600,2 },
	/* sandy bridge m */
	{ PCI_PRODUCT_INTEL_SANDYBRIDGE_M_IGD,    600,2 },
	{ PCI_PRODUCT_INTEL_SANDYBRIDGE_M_IGD_1,  600,2 },
	{ PCI_PRODUCT_INTEL_SANDYBRIDGE_M_IGD_2,  600,2 },
	/* sandy bridge s */
	{ PCI_PRODUCT_INTEL_SANDYBRIDGE_S_IGD,    600,2 },
	/* ivy bridge */
	{ PCI_PRODUCT_INTEL_IVYBRIDGE_IGD,        700,3 },
	{ PCI_PRODUCT_INTEL_IVYBRIDGE_IGD_1,      700,3 },
	/* ivy bridge m */
	{ PCI_PRODUCT_INTEL_IVYBRIDGE_M_IGD,      700,3 },
	{ PCI_PRODUCT_INTEL_IVYBRIDGE_M_IGD_1,    700,3 },
	/* ivy bridge s */
	{ PCI_PRODUCT_INTEL_IVYBRIDGE_S_IGD,      700,3 },
	{ PCI_PRODUCT_INTEL_IVYBRIDGE_S_IGD_1,    700,3 },
#if 0
	/* valleyview d */
	/* valleyview m */
	{ PCI_PRODUCT_INTEL_HASWELL_IGD_1,        800,3 },
	/* haswell d */
	{ PCI_PRODUCT_INTEL_HASWELL_IGD,          800,3 },
	{ PCI_PRODUCT_INTEL_HASWELL_IGD_1,        800,3 },
	/* haswell m */
	/* broadwell d */
	/* broadwell m */
#endif
};

static int
igma_newpch_match(const struct pci_attach_args *pa)
{
	if (PCI_VENDOR(pa->pa_id) != PCI_VENDOR_INTEL)
		return 0;
	switch (0xff00 & PCI_PRODUCT(pa->pa_id)) {
	case 0x3b00: /* ibex peak */
	case 0x1c00: /* cougar point */
	case 0x1e00: /* panther point */
	case 0x8c00: /* lynx point */
	case 0x9c00: /* lynx point lp */
		return 1;
	}

	return 0;
}

static const struct igma_product *
igma_lookup(const struct pci_attach_args *pa)
{ 
        const struct igma_product *ip;
	int i;

	if (PCI_VENDOR(pa->pa_id) != PCI_VENDOR_INTEL)
		return NULL;
	for (i=0; i < __arraycount(igma_products); ++i) {
		ip = &igma_products[i];
                if (PCI_PRODUCT(pa->pa_id) == ip->product)
                        return ip;
        }
        return NULL;
}

static void
igma_product_to_chip(const struct pci_attach_args *pa, struct igma_chip *cd)
{
	const struct igma_product *ip;
	struct pci_attach_args PA;

	ip = igma_lookup(pa);
	KASSERT(ip != NULL);

	cd->ops = &igma_bus_ops;
	cd->num_gmbus = 6;
	cd->num_pipes = ip->num_pipes;
	cd->quirks = 0;
	cd->backlight_factor = 1;

	cd->gpio_offset = OLD_GPIOA;
	cd->vga_cntrl = PCH_VGA_CNTRL;
	cd->backlight_cntrl = OLD_BLC_PWM_CTL;
	cd->backlight_cntrl2 = OLD_BLC_PWM_CTL2;

	PA = *pa;
	if (pci_find_device(&PA, igma_newpch_match)) {
		cd->gpio_offset = PCH_GPIOA;
		cd->vga_cntrl = CPU_VGA_CNTRL;
		cd->backlight_cntrl = CPU_BLC_PWM_CTL;
		cd->backlight_cntrl2 = CPU_BLC_PWM_CTL2;
	}

	switch (ip->gentype) {
	case 200:
		cd->backlight_factor = 2;
		break;
	case 300:
	case 350:
		cd->backlight_factor = 2;
		cd->quirks |= IGMA_PFITDISABLE_QUIRK;
		break;
	case 450:
		cd->pri_cntrl = PRI_CTRL_NOTRICKLE;
		cd->quirks  |= IGMA_PLANESTART_QUIRK;
		break;
	default:
		cd->pri_cntrl = 0;
		break;
	}
}

static void
igma_adjust_chip(struct igma_softc *sc, struct igma_chip *cd)
{
	const struct igma_chip_ops *co = cd->ops;
	u_int32_t reg;

	reg = co->read_reg(cd, cd->vga_cntrl);
	if (reg & VGA_PIPE_B_SELECT)
		cd->use_pipe = 1;
}

static int
igma_print(void *aux, const char *pnp)
{
	if (pnp)
		aprint_normal("drm at %s", pnp);
	return (UNCONF);
}

static int
igma_match(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa = (struct pci_attach_args *)aux;
	const struct igma_product *ip;

	if (PCI_CLASS(pa->pa_class) != PCI_CLASS_DISPLAY)
		return 0;

	ip = igma_lookup(pa);
	if (ip != NULL)
		return 100;

	return 0;
}

static void
igma_attach(device_t parent, device_t self, void *aux)
{
	struct igma_softc *sc = device_private(self);
	const struct pci_attach_args *pa = (struct pci_attach_args *)aux;
	struct igma_attach_args iaa;
	bus_space_tag_t gttmmt, gmt, regt;
	bus_space_handle_t gttmmh, gmh, regh;
	bus_addr_t gttmmb, gmb;

	pci_aprint_devinfo(pa, NULL);

	sc->sc_dev = self;

	/* Initialize according to chip type */
	igma_product_to_chip(pa, &sc->sc_chip);

	if (pci_mapreg_map(pa, PCI_BAR0, PCI_MAPREG_TYPE_MEM,
			BUS_SPACE_MAP_LINEAR,
			&gttmmt, &gttmmh, &gttmmb, NULL)) {
		aprint_error_dev(sc->sc_dev, "unable to map GTTMM\n");
		return;
	}
	sc->sc_chip.mmiot = gttmmt;
	if (bus_space_subregion(gttmmt, gttmmh, 0, 2*1024*1024,
			&sc->sc_chip.mmioh)) {
		aprint_error_dev(sc->sc_dev, "unable to submap MMIO\n");
		return;
	}
	sc->sc_chip.gttt = gttmmt;
	if (bus_space_subregion(gttmmt, gttmmh, 2*1024*1024, 2*1024*1024,
			&sc->sc_chip.gtth)) {
		aprint_error_dev(sc->sc_dev, "unable to submap GTT\n");
		return;
	}

	if (pci_mapreg_map(pa, PCI_BAR2, PCI_MAPREG_TYPE_MEM,
			BUS_SPACE_MAP_LINEAR | BUS_SPACE_MAP_PREFETCHABLE,
			&gmt, &gmh, &gmb, NULL)) {
		aprint_error_dev(sc->sc_dev, "unable to map aperture\n");
		return;
	}
	sc->sc_chip.gmt = gmt;
	sc->sc_chip.gmh = gmh;
	sc->sc_chip.gmb = gmb;

	if (pci_mapreg_map(pa, PCI_BAR4, PCI_MAPREG_TYPE_IO, 0,
			&regt, &regh, NULL, NULL)) {
		aprint_error_dev(sc->sc_dev, "unable to map IO registers\n");
		return;
	}

#if NVGA > 0
	iaa.iaa_console = vga_cndetach() ? true : false;
#else
	iaa.iaa_console = 0;
#endif
	sc->sc_chip.vgat = regt;
	if (bus_space_map(regt, 0x3c0, 0x10, 0, &sc->sc_chip.vgah)) {
		aprint_error_dev(sc->sc_dev, "unable to map VGA registers\n");
		return;
	}

	/* Check hardware for more information */
	igma_adjust_chip(sc, &sc->sc_chip);

	aprint_normal("%s: VGA_CNTRL: 0x%x\n",device_xname(sc->sc_dev),
		sc->sc_chip.vga_cntrl);
	aprint_normal("%s: GPIO_OFFSET: 0x%x\n",device_xname(sc->sc_dev),
		sc->sc_chip.gpio_offset);
	aprint_normal("%s: BACKLIGHT_CTRL: 0x%x\n",device_xname(sc->sc_dev),
		sc->sc_chip.backlight_cntrl);
	aprint_normal("%s: BACKLIGHT_CTRL2: 0x%x\n",device_xname(sc->sc_dev),
		sc->sc_chip.backlight_cntrl2);

#if NIGMAFB > 0
	strcpy(iaa.iaa_name, "igmafb");
	iaa.iaa_chip = sc->sc_chip;
	config_found_ia(sc->sc_dev, "igmabus", &iaa, igma_print);
#endif

	igma_i2c_attach(sc);
}

static void
igma_i2c_attach(struct igma_softc *sc)
{
	struct igma_i2c *ii;
	int i;
#if 0
	struct i2cbus_attach_args iba;
#endif

	for (i=0; i<sc->sc_chip.num_gmbus; ++i) {
		ii = &sc->sc_ii[i];
		ii->ii_sc = sc;

		/* XXX */
		ii->ii_reg = sc->sc_chip.gpio_offset - PCH_GPIOA;
		switch (i) {
		case 0:
			ii->ii_reg += PCH_GPIOB;
			ii->ii_name = "ssc";
			break;
		case 1:
			ii->ii_reg += PCH_GPIOA;
			ii->ii_name = "vga";
			break;
		case 2:
			ii->ii_reg += PCH_GPIOC;
			ii->ii_name = "panel";
			break;
		case 3:
			ii->ii_reg += PCH_GPIOD;
			ii->ii_name = "dpc";
			break;
		case 4:
			ii->ii_reg += PCH_GPIOE;
			ii->ii_name = "dpb";
			break;
		case 5:
			ii->ii_reg += PCH_GPIOF;
			ii->ii_name = "dpd";
			break;
		default:
			panic("don't know GMBUS %d\n",i);
		}

		mutex_init(&ii->ii_lock, MUTEX_DEFAULT, IPL_NONE);

		ii->ii_i2c.ic_cookie = ii;
		ii->ii_i2c.ic_acquire_bus = igma_i2c_acquire_bus;
		ii->ii_i2c.ic_release_bus = igma_i2c_release_bus;
		ii->ii_i2c.ic_send_start = igma_i2c_send_start;
		ii->ii_i2c.ic_send_stop = igma_i2c_send_stop;
		ii->ii_i2c.ic_initiate_xfer = igma_i2c_initiate_xfer;
		ii->ii_i2c.ic_read_byte = igma_i2c_read_byte;
		ii->ii_i2c.ic_write_byte = igma_i2c_write_byte;
		ii->ii_i2c.ic_exec = NULL;

#if 0
		iba.iba_type = I2C_TYPE_SMBUS;
		iba.iba_tag = &ii->ii_i2c;
		config_found_ia(sc->sc_dev, "i2cbus", &iba, iicbus_print);
#endif
	}
}

/*
 * I2C interface
 */

static int
igma_i2c_acquire_bus(void *cookie, int flags)
{
	struct igma_i2c *ii = cookie;
	mutex_enter(&ii->ii_lock);
	return 0;
}

static void
igma_i2c_release_bus(void *cookie, int flags)
{
	struct igma_i2c *ii = cookie;
	mutex_exit(&ii->ii_lock);
}

static int
igma_i2c_send_start(void *cookie, int flags)
{
	return i2c_bitbang_send_start(cookie, flags, &igma_i2cbb_ops);
}

static int
igma_i2c_send_stop(void *cookie, int flags)
{
	return i2c_bitbang_send_stop(cookie, flags, &igma_i2cbb_ops);
}

static int
igma_i2c_initiate_xfer(void *cookie, i2c_addr_t addr, int flags)
{
	return i2c_bitbang_initiate_xfer(cookie, addr, flags, &igma_i2cbb_ops);
}

static int
igma_i2c_read_byte(void *cookie, uint8_t *valp, int flags)
{
	return i2c_bitbang_read_byte(cookie, valp, flags, &igma_i2cbb_ops);
}

static int
igma_i2c_write_byte(void *cookie, uint8_t val, int flags)
{
	return i2c_bitbang_write_byte(cookie, val, flags, &igma_i2cbb_ops);
}

static void
igma_i2cbb_set_bits(void *cookie, uint32_t bits)
{
	struct igma_i2c *ii = cookie;
	struct igma_softc *sc = ii->ii_sc;
	const struct igma_chip *cd = &sc->sc_chip;
	const struct igma_chip_ops *co = cd->ops;
	uint32_t reg;

	reg = co->read_reg(cd, ii->ii_reg);
	reg &= GPIO_DATA_PULLUP_DISABLE | GPIO_CLOCK_PULLUP_DISABLE;

	if ((bits | ii->ii_dir) & 1)
		/* make data input, signal is pulled high */
		reg |= GPIO_DATA_DIR_IN | GPIO_DATA_DIR_MASK;
        else
		/* make data output, signal is driven low */
		reg |= GPIO_DATA_DIR_OUT | GPIO_DATA_DIR_MASK
			| GPIO_DATA_VAL_MASK;

	if (bits & 2)
		/* make clock input, signal is pulled high */
		reg |= GPIO_CLOCK_DIR_IN | GPIO_CLOCK_DIR_MASK;
	else
		/* make clock output, signal is driven low */
		reg |= GPIO_CLOCK_DIR_OUT | GPIO_CLOCK_DIR_MASK
			| GPIO_CLOCK_VAL_MASK;

	co->write_reg(cd, ii->ii_reg, reg);
#if 1
	reg = co->read_reg(cd, ii->ii_reg);
#else
	co->barrier(cd, ii->ii_reg);
#endif
}

static void
igma_i2cbb_set_dir(void *cookie, uint32_t bits)
{
	struct igma_i2c *ii = cookie;

	ii->ii_dir = bits;
}

static uint32_t
igma_i2cbb_read(void *cookie)
{
	struct igma_i2c *ii = cookie;
	struct igma_softc *sc = ii->ii_sc;
	const struct igma_chip *cd = &sc->sc_chip;
	const struct igma_chip_ops *co = cd->ops;
	uint32_t reg;
	int sda, scl;

	reg = co->read_reg(cd, ii->ii_reg);

	sda = reg & GPIO_DATA_VAL_IN;
	scl = reg & GPIO_CLOCK_VAL_IN;

	reg = (sda ? 1 : 0) | (scl ? 2 : 0);
	return reg;
}

static void
igma_reg_barrier(const struct igma_chip *cd, int r)
{
	bus_space_barrier(cd->mmiot, cd->mmioh, r, sizeof(u_int32_t),
		BUS_SPACE_BARRIER_READ | BUS_SPACE_BARRIER_WRITE);
}

static u_int32_t
igma_reg_read(const struct igma_chip *cd, int r)
{
	return bus_space_read_4(cd->mmiot, cd->mmioh, r);
}

static void
igma_reg_write(const struct igma_chip *cd, int r, u_int32_t v)
{
	bus_space_write_4(cd->mmiot, cd->mmioh, r, v);
}

static u_int8_t
igma_vga_read(const struct igma_chip *cd, int r)
{
	bus_space_write_1(cd->vgat, cd->vgah, 0x4, r | 0x20);
	return bus_space_read_1(cd->vgat, cd->vgah, 0x5);
}

static void
igma_vga_write(const struct igma_chip *cd, int r, u_int8_t v)
{
	bus_space_write_1(cd->vgat, cd->vgah, 0x4, r | 0x20);
	bus_space_write_1(cd->vgat, cd->vgah, 0x5, v);
}

#if 0
static u_int8_t
igma_crtc_read(const struct igma_chip *cd, int r)
{
	bus_space_write_1(cd->crtct, cd->crtch, 0x4, r);
	return bus_space_read_1(cd->crtct, cd->crtch, 0x5);
}

static void
igma_crtc_write(const struct igma_chip *cd, int r, u_int8_t v)
{
	bus_space_write_1(cd->crtct, cd->crtch, 0x4, r);
	bus_space_write_1(cd->crtct, cd->crtch, 0x5, v);
}
#endif
