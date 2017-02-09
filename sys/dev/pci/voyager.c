/*	$NetBSD: voyager.c,v 1.10 2014/03/29 19:28:25 christos Exp $	*/

/*
 * Copyright (c) 2009, 2011 Michael Lorenz
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
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
 
#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: voyager.c,v 1.10 2014/03/29 19:28:25 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/lwp.h>
#include <sys/kauth.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>
#include <dev/pci/pciio.h>
#include <dev/ic/sm502reg.h>
#include <dev/i2c/i2cvar.h>
#include <dev/i2c/i2c_bitbang.h>

#include <sys/evcnt.h>
#include <sys/bitops.h>

#include <dev/pci/voyagervar.h>

#include "opt_voyager.h"
#include "voyagerfb.h"
#include "pwmclock.h"

#ifdef VOYAGER_DEBUG
#define DPRINTF aprint_normal
#else
#define DPRINTF while (0) printf
#endif

/* interrupt stuff */
struct voyager_intr {
	int (*vih_func)(void *);
	void *vih_arg;
	struct evcnt vih_count;
	char vih_name[32];
};

struct voyager_softc {
	device_t sc_dev;

	pci_chipset_tag_t sc_pc;
	pcitag_t sc_pcitag;

	bus_space_tag_t sc_memt;
	bus_space_tag_t sc_iot;

	bus_space_handle_t sc_fbh, sc_regh;
	bus_addr_t sc_fb, sc_reg;
	bus_size_t sc_fbsize, sc_regsize;

	struct i2c_controller sc_i2c;
	kmutex_t sc_i2c_lock;

	/* interrupt dispatcher */
	void *sc_ih;
	struct voyager_intr sc_intrs[32];
};

static int	voyager_match(device_t, cfdata_t, void *);
static void	voyager_attach(device_t, device_t, void *);
static int	voyager_print(void *, const char *);
static int	voyager_intr(void *);

CFATTACH_DECL_NEW(voyager, sizeof(struct voyager_softc),
    voyager_match, voyager_attach, NULL, NULL);
    
/* I2C glue */
static int voyager_i2c_acquire_bus(void *, int);
static void voyager_i2c_release_bus(void *, int);
static int voyager_i2c_send_start(void *, int);
static int voyager_i2c_send_stop(void *, int);
static int voyager_i2c_initiate_xfer(void *, i2c_addr_t, int);
static int voyager_i2c_read_byte(void *, uint8_t *, int);
static int voyager_i2c_write_byte(void *, uint8_t, int);

/* I2C bitbang glue */
static void voyager_i2cbb_set_bits(void *, uint32_t);
static void voyager_i2cbb_set_dir(void *, uint32_t);
static uint32_t voyager_i2cbb_read(void *);

static const struct i2c_bitbang_ops voyager_i2cbb_ops = {
	voyager_i2cbb_set_bits,
	voyager_i2cbb_set_dir,
	voyager_i2cbb_read,
	{
		1 << 13,
		1 << 6,
		1 << 13,
		0
	}
};
#define GPIO_I2C_BITS ((1 << 6) | (1 << 13))

#ifdef VOYAGER_DEBUG
static void voyager_print_pwm(struct voyager_softc *, int);
#endif

static int
voyager_match(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa = (struct pci_attach_args *)aux;

	if (PCI_CLASS(pa->pa_class) != PCI_CLASS_DISPLAY)
		return 0;
	if (PCI_VENDOR(pa->pa_id) != PCI_VENDOR_SILMOTION)
		return 0;

	/* only chip tested on so far - may need a list */
	if (PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_SILMOTION_SM502)
		return 100;
	return (0);
}

static void
voyager_attach(device_t parent, device_t self, void *aux)
{
	struct voyager_softc	*sc = device_private(self);
	struct pci_attach_args	*pa = aux;
	pci_intr_handle_t ih;
	struct voyager_attach_args vaa;
	struct i2cbus_attach_args iba;
	uint32_t reg;
	const char *intrstr;
	int i;
	char intrbuf[PCI_INTRSTR_LEN];

	sc->sc_pc = pa->pa_pc;
	sc->sc_pcitag = pa->pa_tag;
	sc->sc_memt = pa->pa_memt;
	sc->sc_iot = pa->pa_iot;
	sc->sc_dev = self;

	pci_aprint_devinfo(pa, NULL);

	if (pci_mapreg_map(pa, 0x14, PCI_MAPREG_TYPE_MEM, 0,
	    &sc->sc_memt, &sc->sc_regh, &sc->sc_reg, &sc->sc_regsize)) {
		aprint_error("%s: failed to map registers.\n",
		    device_xname(sc->sc_dev));
	}

	if (pci_mapreg_map(pa, 0x10, PCI_MAPREG_TYPE_MEM,
	    BUS_SPACE_MAP_LINEAR,
	    &sc->sc_memt, &sc->sc_fbh, &sc->sc_fb, &sc->sc_fbsize)) {
		aprint_error("%s: failed to map the frame buffer.\n",
		    device_xname(sc->sc_dev));
	}

	/* disable all interrupts */
	bus_space_write_4(sc->sc_memt, sc->sc_regh, SM502_INTR_MASK, 0);
	
	/* initialize handler list */
	for (i = 0; i < 32; i++) {
		sc->sc_intrs[i].vih_func = NULL;
		snprintf(sc->sc_intrs[i].vih_name, 32, "int %d", i);
		evcnt_attach_dynamic(&sc->sc_intrs[i].vih_count,
		    EVCNT_TYPE_INTR, NULL, "voyager", sc->sc_intrs[i].vih_name);
	}

	/* Map and establish the interrupt. */
	if (pci_intr_map(pa, &ih)) {
		aprint_error_dev(self, "couldn't map interrupt\n");
		return;
	}

	intrstr = pci_intr_string(sc->sc_pc, ih, intrbuf, sizeof(intrbuf));
	sc->sc_ih = pci_intr_establish(sc->sc_pc, ih, IPL_AUDIO, voyager_intr, sc);
	if (sc->sc_ih == NULL) {
		aprint_error_dev(self, "couldn't establish interrupt");
		if (intrstr != NULL)
			aprint_error(" at %s", intrstr);
		aprint_error("\n");
		return;
	}
	aprint_normal_dev(self, "interrupting at %s\n", intrstr);

#ifdef VOYAGER_DEBUG
	voyager_print_pwm(sc, SM502_PWM0);
	voyager_print_pwm(sc, SM502_PWM1);
	voyager_print_pwm(sc, SM502_PWM2);
#endif

	/* attach the framebuffer driver */
	vaa.vaa_memh = sc->sc_fbh;
	vaa.vaa_mem_pa = sc->sc_fb;
	vaa.vaa_regh = sc->sc_regh;
	vaa.vaa_reg_pa = sc->sc_reg;
	vaa.vaa_tag = sc->sc_memt;
	vaa.vaa_pc = sc->sc_pc;
	vaa.vaa_pcitag = sc->sc_pcitag;
#if NVOYAGERFB > 0
	strcpy(vaa.vaa_name, "voyagerfb");
	config_found_ia(sc->sc_dev, "voyagerbus", &vaa, voyager_print);
#endif
#if NPWMCLOCK > 0
	strcpy(vaa.vaa_name, "pwmclock");
	config_found_ia(sc->sc_dev, "voyagerbus", &vaa, voyager_print);
#endif
#ifdef notyet
	strcpy(vaa.vaa_name, "vac");
	config_found_ia(sc->sc_dev, "voyagerbus", &vaa, voyager_print);
#endif
	/* we use this mutex wether there's an i2c bus or not */
	mutex_init(&sc->sc_i2c_lock, MUTEX_DEFAULT, IPL_NONE);

	/*
	 * see if the i2c pins are configured as gpio and if so, use them
	 * should probably be a compile time option
	 */
	reg = bus_space_read_4(sc->sc_memt, sc->sc_regh, SM502_GPIO0_CONTROL);
	if ((reg & GPIO_I2C_BITS) == 0) {
		/* both bits as outputs */
		voyager_gpio_dir(sc, 0xffffffff, GPIO_I2C_BITS);
		
		/* Fill in the i2c tag */
		sc->sc_i2c.ic_cookie = sc;
		sc->sc_i2c.ic_acquire_bus = voyager_i2c_acquire_bus;
		sc->sc_i2c.ic_release_bus = voyager_i2c_release_bus;
		sc->sc_i2c.ic_send_start = voyager_i2c_send_start;
		sc->sc_i2c.ic_send_stop = voyager_i2c_send_stop;
		sc->sc_i2c.ic_initiate_xfer = voyager_i2c_initiate_xfer;
		sc->sc_i2c.ic_read_byte = voyager_i2c_read_byte;
		sc->sc_i2c.ic_write_byte = voyager_i2c_write_byte;
		sc->sc_i2c.ic_exec = NULL;
		iba.iba_tag = &sc->sc_i2c;
		config_found_ia(self, "i2cbus", &iba, iicbus_print);
	}
	voyager_control_gpio(sc, ~(1 << 16), 0);
	voyager_gpio_dir(sc, 0xffffffff, 1 << 16);
	voyager_write_gpio(sc, 0xffffffff, 1 << 16);
}

static int
voyager_print(void *aux, const char *what)
{
	/*struct voyager_attach_args *vaa = aux;*/

	if (what == NULL)
		return 0;

	printf("%s:", what);

	return 0;
}

static void
voyager_i2cbb_set_bits(void *cookie, uint32_t bits)
{
	struct voyager_softc *sc = cookie;
	uint32_t reg;

	reg = bus_space_read_4(sc->sc_memt, sc->sc_regh, SM502_GPIO_DATA0);
	reg &= ~GPIO_I2C_BITS;
	reg |= bits;
	bus_space_write_4(sc->sc_memt, sc->sc_regh, SM502_GPIO_DATA0, reg);
}

static void
voyager_i2cbb_set_dir(void *cookie, uint32_t bits)
{
	struct voyager_softc *sc = cookie;
	uint32_t reg;

	reg = bus_space_read_4(sc->sc_memt, sc->sc_regh, SM502_GPIO_DIR0);
	reg &= ~(1 << 13);
	reg |= bits;
	bus_space_write_4(sc->sc_memt, sc->sc_regh, SM502_GPIO_DIR0, reg);
}

static uint32_t
voyager_i2cbb_read(void *cookie)
{
	struct voyager_softc *sc = cookie;
	uint32_t reg;

	reg = bus_space_read_4(sc->sc_memt, sc->sc_regh, SM502_GPIO_DATA0);
	return reg;
}

/* higher level I2C stuff */
static int
voyager_i2c_acquire_bus(void *cookie, int flags)
{
	struct voyager_softc *sc = cookie;

	mutex_enter(&sc->sc_i2c_lock);
	return 0;
}

static void
voyager_i2c_release_bus(void *cookie, int flags)
{
	struct voyager_softc *sc = cookie;

	mutex_exit(&sc->sc_i2c_lock);
}

static int
voyager_i2c_send_start(void *cookie, int flags)
{
	return (i2c_bitbang_send_start(cookie, flags, &voyager_i2cbb_ops));
}

static int
voyager_i2c_send_stop(void *cookie, int flags)
{

	return (i2c_bitbang_send_stop(cookie, flags, &voyager_i2cbb_ops));
}

static int
voyager_i2c_initiate_xfer(void *cookie, i2c_addr_t addr, int flags)
{
	/*
	 * for some reason i2c_bitbang_initiate_xfer left-shifts
	 * the I2C-address and then sets the direction bit
	 */
	return (i2c_bitbang_initiate_xfer(cookie, addr, flags, 
	    &voyager_i2cbb_ops));
}

static int
voyager_i2c_read_byte(void *cookie, uint8_t *valp, int flags)
{
	int ret;

	ret = i2c_bitbang_read_byte(cookie, valp, flags, &voyager_i2cbb_ops);
	return ret;
}

static int
voyager_i2c_write_byte(void *cookie, uint8_t val, int flags)
{
	int ret;

	ret = i2c_bitbang_write_byte(cookie, val, flags, &voyager_i2cbb_ops);
	delay(500);
	return ret;
}

void
voyager_twiddle_bits(void *cookie, int regnum, uint32_t mask, uint32_t bits)
{
	struct voyager_softc *sc = cookie;
	uint32_t reg;

	/* don't interfere with i2c ops */
	mutex_enter(&sc->sc_i2c_lock);
	reg = bus_space_read_4(sc->sc_memt, sc->sc_regh, regnum);
	reg &= mask;
	reg |= bits;
	bus_space_write_4(sc->sc_memt, sc->sc_regh, regnum, reg);
	mutex_exit(&sc->sc_i2c_lock);
}

static int
voyager_intr(void *cookie)
{
	struct voyager_softc *sc = cookie;
	struct voyager_intr *ih;
	uint32_t intrs;
	uint32_t mask, bit;
	int num;

	intrs = bus_space_read_4(sc->sc_memt, sc->sc_regh, SM502_INTR_STATUS);
	mask = bus_space_read_4(sc->sc_memt, sc->sc_regh, SM502_INTR_MASK);
	intrs &= mask;

	while (intrs != 0) {
		num = ffs32(intrs) - 1;
		bit = 1 << num;
		intrs &= ~bit;
		ih = &sc->sc_intrs[num];
		if (ih->vih_func != NULL) {
			ih->vih_func(ih->vih_arg);
		}
		ih->vih_count.ev_count++;
	}
	return 0;
}	

void *
voyager_establish_intr(device_t dev, int bit, int (*handler)(void *), void *arg)
{
	struct voyager_softc *sc = device_private(dev);
	struct voyager_intr *ih;
	uint32_t reg;

	if ((bit < 0) || (bit > 31)) {
		aprint_error_dev(dev, "bogus interrupt %d\n", bit);
		return NULL;
	}

	ih = &sc->sc_intrs[bit];
	if (ih->vih_func != NULL) {
		aprint_error_dev(dev, "interrupt %d is already in use\n", bit);
		return NULL;
	}
	ih->vih_func = handler;
	ih->vih_arg = arg;
	reg = bus_space_read_4(sc->sc_memt, sc->sc_regh, SM502_INTR_MASK);
	reg |= 1 << bit;
	bus_space_write_4(sc->sc_memt, sc->sc_regh, SM502_INTR_MASK, reg);
	
	return (void *)(uintptr_t)(0x80000000 | bit);
}

void
voyager_disestablish_intr(device_t dev, void *ih)
{
}

/* timer */
#ifdef VOYAGER_DEBUG
static void
voyager_print_pwm(struct voyager_softc *sc, int pwmreg)
{
	uint32_t reg;

	reg = bus_space_read_4(sc->sc_memt, sc->sc_regh, pwmreg);
	aprint_debug_dev(sc->sc_dev, "%08x: %08x = %d Hz, %d high, %d low\n", 
	    pwmreg, reg,
	    96000000 / (1 << ((reg & SM502_PWM_CLOCK_DIV_MASK) >> SM502_PWM_CLOCK_DIV_SHIFT)),
	    (reg & SM502_PWM_CLOCK_HIGH_MASK) >> SM502_PWM_CLOCK_HIGH_SHIFT,
	    (reg & SM502_PWM_CLOCK_LOW_MASK) >> SM502_PWM_CLOCK_LOW_SHIFT);
}
#endif

uint32_t
voyager_set_pwm(int freq, int duty_cycle)
{
	int ifreq, factor, bit, steps;
	uint32_t reg = 0, hi, lo;

	/*
	 * find the smallest divider that gets us within 4096 steps of the
	 * target frequency
	 */
	ifreq = freq * 4096;
	factor = 96000000 / ifreq;
	bit = fls32(factor);
	factor = 1 << bit;
	steps = 96000000 / (factor * freq);
	/* can't have it all off */
	if (duty_cycle < 1)
		duty_cycle = 1;
	/* can't be always on either */
	if (duty_cycle > 999)
		duty_cycle = 999;
	hi = steps * duty_cycle / 1000;
	if (hi < 1)
		hi = 1;
	lo = steps - hi;
	if (lo < 1) {
		hi = steps - 1;
		lo = 1;
	}
	DPRINTF("%d hz -> %d, %d, %d / %d\n", freq, factor, steps, lo, hi);
	reg = ((hi - 1) & 0xfff) << SM502_PWM_CLOCK_HIGH_SHIFT;
	reg |= ((lo - 1) & 0xfff) << SM502_PWM_CLOCK_LOW_SHIFT;
	reg |= (bit & 0xf) << SM502_PWM_CLOCK_DIV_SHIFT;
	DPRINTF("reg: %08x\n", reg);
	return reg;
}
