/*	$NetBSD: w83795g.c,v 1.2 2014/04/13 12:42:47 christos Exp $	*/

/*
 * Copyright (c) 2013 Soren S. Jorvang.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: w83795g.c,v 1.2 2014/04/13 12:42:47 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/gpio.h>
#include <sys/wdog.h>

#include <dev/i2c/i2cvar.h>
#include <dev/gpio/gpiovar.h>    
#include <dev/sysmon/sysmonvar.h>

#include <dev/i2c/w83795greg.h>

#define NUM_SENSORS 53
static const struct w83795g_sensor {
	const char *desc;
	enum envsys_units type;
	uint8_t en_reg;
	uint8_t en_mask;
	uint8_t en_bits;
	uint8_t msb;
} sensors[NUM_SENSORS] = {
#define _VOLT ENVSYS_SVOLTS_DC
	{ "VSEN1",   _VOLT, W83795G_V_CTRL1, 0x01, 0x01, W83795G_VSEN1 },
	{ "VSEN2",   _VOLT, W83795G_V_CTRL1, 0x02, 0x02, W83795G_VSEN2 },
	{ "VSEN3",   _VOLT, W83795G_V_CTRL1, 0x04, 0x04, W83795G_VSEN3 },
	{ "VSEN4",   _VOLT, W83795G_V_CTRL1, 0x08, 0x08, W83795G_VSEN4 },
	{ "VSEN5",   _VOLT, W83795G_V_CTRL1, 0x10, 0x10, W83795G_VSEN5 },
	{ "VSEN6",   _VOLT, W83795G_V_CTRL1, 0x20, 0x20, W83795G_VSEN6 },
	{ "VSEN7",   _VOLT, W83795G_V_CTRL1, 0x40, 0x40, W83795G_VSEN7 },
	{ "VSEN8",   _VOLT, W83795G_V_CTRL1, 0x80, 0x80, W83795G_VSEN8 },
	{ "VSEN9",   _VOLT, W83795G_V_CTRL2, 0x01, 0x01, W83795G_VSEN9 },
	{ "VSEN10",  _VOLT, W83795G_V_CTRL2, 0x02, 0x02, W83795G_VSEN10 },
	{ "VSEN11",  _VOLT, W83795G_V_CTRL2, 0x04, 0x04, W83795G_VSEN11 },
	{ "VTT",     _VOLT, W83795G_V_CTRL2, 0x08, 0x08, W83795G_VTT },
	{ "3VDD",    _VOLT, W83795G_V_CTRL2, 0x10, 0x10, W83795G_3VDD },
	{ "3VSB",    _VOLT, W83795G_V_CTRL2, 0x20, 0x20, W83795G_3VSB },
	{ "VBAT",    _VOLT, W83795G_V_CTRL2, 0x40, 0x40, W83795G_VBAT },
	{ "VSEN12",  _VOLT, W83795G_T_CTRL1, 0x03, 0x02, W83795G_VSEN12 },
	{ "VSEN13",  _VOLT, W83795G_T_CTRL1, 0x0c, 0x08, W83795G_VSEN13 },
	{ "VDSEN14", _VOLT, W83795G_T_CTRL2, 0x03, 0x02, W83795G_VDSEN14 },
	{ "VDSEN15", _VOLT, W83795G_T_CTRL2, 0x0c, 0x08, W83795G_VDSEN15 },
	{ "VDSEN16", _VOLT, W83795G_T_CTRL2, 0x30, 0x20, W83795G_VDSEN16 },
	{ "VDSEN17", _VOLT, W83795G_T_CTRL2, 0xc0, 0x80, W83795G_VDSEN17 },
#define _TEMP ENVSYS_STEMP
	{ "TR5",     _TEMP, W83795G_T_CTRL1, 0x03, 0x03, W83795G_TR5 },
	{ "TR6",     _TEMP, W83795G_T_CTRL1, 0x0c, 0x0c, W83795G_TR6 },
	{ "TD1",     _TEMP, W83795G_T_CTRL2, 0x03, 0x01, W83795G_TD1 },
	{ "TD2",     _TEMP, W83795G_T_CTRL2, 0x0c, 0x04, W83795G_TD2 },
	{ "TD3",     _TEMP, W83795G_T_CTRL2, 0x30, 0x10, W83795G_TD3 },
	{ "TD4",     _TEMP, W83795G_T_CTRL2, 0xc0, 0x40, W83795G_TD4 },
	{ "TR1",     _TEMP, W83795G_T_CTRL2, 0x03, 0x03, W83795G_TR1 },
	{ "TR2",     _TEMP, W83795G_T_CTRL2, 0x0c, 0x0c, W83795G_TR2 },
	{ "TR3",     _TEMP, W83795G_T_CTRL2, 0x30, 0x30, W83795G_TR3 },
	{ "TR4",     _TEMP, W83795G_T_CTRL2, 0xc0, 0xc0, W83795G_TR4 },
	{ "DTS1",    _TEMP, W83795G_T_CTRL1, 0x20, 0x20, W83795G_DTS1 },
	{ "DTS2",    _TEMP, W83795G_T_CTRL1, 0x20, 0x20, W83795G_DTS2 },
	{ "DTS3",    _TEMP, W83795G_T_CTRL1, 0x20, 0x20, W83795G_DTS3 },
	{ "DTS4",    _TEMP, W83795G_T_CTRL1, 0x20, 0x20, W83795G_DTS4 },
	{ "DTS5",    _TEMP, W83795G_T_CTRL1, 0x20, 0x20, W83795G_DTS5 },
	{ "DTS6",    _TEMP, W83795G_T_CTRL1, 0x20, 0x20, W83795G_DTS6 },
	{ "DTS7",    _TEMP, W83795G_T_CTRL1, 0x20, 0x20, W83795G_DTS7 },
	{ "DTS8",    _TEMP, W83795G_T_CTRL1, 0x20, 0x20, W83795G_DTS8 },
#define _FAN ENVSYS_SFANRPM
	{ "FANIN1",  _FAN,  W83795G_F_CTRL1, 0x01, 0x01, W83795G_FANIN1 },
	{ "FANIN2",  _FAN,  W83795G_F_CTRL1, 0x02, 0x02, W83795G_FANIN2 },
	{ "FANIN3",  _FAN,  W83795G_F_CTRL1, 0x04, 0x04, W83795G_FANIN3 },
	{ "FANIN4",  _FAN,  W83795G_F_CTRL1, 0x08, 0x08, W83795G_FANIN4 },
	{ "FANIN5",  _FAN,  W83795G_F_CTRL1, 0x10, 0x10, W83795G_FANIN5 },
	{ "FANIN6",  _FAN,  W83795G_F_CTRL1, 0x20, 0x20, W83795G_FANIN6 },
	{ "FANIN7",  _FAN,  W83795G_F_CTRL1, 0x40, 0x40, W83795G_FANIN7 },
	{ "FANIN8",  _FAN,  W83795G_F_CTRL1, 0x80, 0x80, W83795G_FANIN8 },
	{ "FANIN9",  _FAN,  W83795G_F_CTRL2, 0x01, 0x01, W83795G_FANIN9 },
	{ "FANIN10", _FAN,  W83795G_F_CTRL2, 0x02, 0x02, W83795G_FANIN10 },
	{ "FANIN11", _FAN,  W83795G_F_CTRL2, 0x04, 0x04, W83795G_FANIN11 },
	{ "FANIN12", _FAN,  W83795G_F_CTRL2, 0x08, 0x08, W83795G_FANIN12 },
	{ "FANIN13", _FAN,  W83795G_F_CTRL2, 0x10, 0x10, W83795G_FANIN13 },
	{ "FANIN14", _FAN,  W83795G_F_CTRL2, 0x20, 0x20, W83795G_FANIN14 },
};

struct w83795g_softc {
	device_t		sc_dev;
	i2c_tag_t		sc_tag;
	i2c_addr_t		sc_addr;
	struct gpio_chipset_tag	sc_gpio_gc;
	gpio_pin_t		sc_gpio_pins[8];
	struct sysmon_envsys	*sc_sme;
	envsys_data_t 		sc_sensors[NUM_SENSORS];
	struct sysmon_wdog	sc_smw;
};

static int	w83795g_match(device_t, cfdata_t, void *);
static void	w83795g_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(w83795g, sizeof(struct w83795g_softc),
    w83795g_match, w83795g_attach, NULL, NULL);

static void	w83795g_refresh(struct sysmon_envsys *, envsys_data_t *);
static void	w83795g_get_limits(struct sysmon_envsys *, envsys_data_t *,
   sysmon_envsys_lim_t *limits, uint32_t *props);

static int	w83795g_gpio_read(void *, int);   
static void	w83795g_gpio_write(void *, int, int); 
static void	w83795g_gpio_ctl(void *, int, int);

static int	w83795g_wdog_setmode(struct sysmon_wdog *);
static int	w83795g_wdog_tickle(struct sysmon_wdog *);

static int
w83795g_match(device_t parent, cfdata_t match, void *aux)
{
	struct i2c_attach_args *ia = aux;
	uint8_t bank, vend, chip, deva;

	if (ia->ia_addr < I2CADDR_MINADDR || ia->ia_addr > I2CADDR_MAXADDR)
		return 0;

	iic_acquire_bus(ia->ia_tag, 0);
	iic_smbus_read_byte(ia->ia_tag, ia->ia_addr, W83795G_BANKSEL, &bank, 0);
	iic_smbus_read_byte(ia->ia_tag, ia->ia_addr, W83795G_VENDOR, &vend, 0);
	iic_smbus_read_byte(ia->ia_tag, ia->ia_addr, W83795G_CHIP, &chip, 0);
	iic_smbus_read_byte(ia->ia_tag, ia->ia_addr, W83795G_DEVICEA, &deva, 0);
	iic_release_bus(ia->ia_tag, 0);

	if ((bank & BANKSEL_HBACS && vend == VENDOR_NUVOTON_ID_HI) ||
	   (~bank & BANKSEL_HBACS && vend == VENDOR_NUVOTON_ID_LO))
		if (chip == CHIP_W83795G && deva == DEVICEA_A)
			return 1;

	return 0;
}

static void
w83795g_attach(device_t parent, device_t self, void *aux)
{
	struct w83795g_softc *sc = device_private(self);
	struct i2c_attach_args *ia = aux;
	struct gpiobus_attach_args gba;
	uint8_t conf, rev, reg, gpiom, en_reg;
	int i;

	sc->sc_dev = self;
	sc->sc_tag = ia->ia_tag;
	sc->sc_addr = ia->ia_addr;
	sc->sc_gpio_gc.gp_cookie = sc;
	sc->sc_gpio_gc.gp_pin_read = w83795g_gpio_read;
	sc->sc_gpio_gc.gp_pin_write = w83795g_gpio_write;
	sc->sc_gpio_gc.gp_pin_ctl = w83795g_gpio_ctl;
	sc->sc_sme = sysmon_envsys_create();
	sc->sc_sme->sme_name = device_xname(self);
	sc->sc_sme->sme_cookie = sc;
	sc->sc_sme->sme_refresh = w83795g_refresh;
	sc->sc_sme->sme_get_limits = w83795g_get_limits;
	sc->sc_smw.smw_name = device_xname(self);
	sc->sc_smw.smw_cookie = sc;
	sc->sc_smw.smw_setmode = w83795g_wdog_setmode;
	sc->sc_smw.smw_tickle = w83795g_wdog_tickle;
	sc->sc_smw.smw_period = 60;

	iic_acquire_bus(sc->sc_tag, 0);
	iic_smbus_write_byte(sc->sc_tag, sc->sc_addr, W83795G_BANKSEL, 0, 0);
	iic_smbus_read_byte(sc->sc_tag, sc->sc_addr, W83795G_CONFIG, &conf, 0);
	iic_smbus_read_byte(sc->sc_tag, sc->sc_addr, W83795G_DEVICE, &rev, 0);

	aprint_normal(": Nuvaton W83795");
	if (conf & CONFIG_CONFIG48)
		aprint_normal("ADG");
	else
		aprint_normal("G");
	aprint_verbose(" (rev %c)", rev - DEVICEA_A + 'A');
	aprint_normal(" Hardware Monitor\n");
	aprint_naive(": Hardware Monitor\n");

	/* Debug dump of all register banks */
	for (i = 0; i < 1024; i++) {
		if (i % 256 == 0) {
			iic_smbus_write_byte(sc->sc_tag, sc->sc_addr,
			    W83795G_BANKSEL, i / 256, 0);
			aprint_debug_dev(self, "register bank %d:\n", i / 256);
		}
		if (i % 32 == 0)
			aprint_debug_dev(self, "%02x ", i % 256);
		iic_smbus_read_byte(sc->sc_tag, sc->sc_addr, i % 256, &reg, 0);
		aprint_debug("%02x", reg);
		if (i % 32 == 31)
			aprint_debug("\n");
		else if (i % 8 == 7)
			aprint_debug(" ");
	}

	iic_smbus_write_byte(sc->sc_tag, sc->sc_addr, W83795G_BANKSEL, 0, 0);

	for (i = 0; i < NUM_SENSORS; i++) {
		iic_smbus_read_byte(sc->sc_tag, sc->sc_addr,
		    sensors[i].en_reg, &en_reg, 0);

		if ((en_reg & sensors[i].en_mask) != sensors[i].en_bits)
			continue;
		
		strcpy(sc->sc_sensors[i].desc, sensors[i].desc);
		sc->sc_sensors[i].units = sensors[i].type;
		sc->sc_sensors[i].state = ENVSYS_SINVALID;
		sc->sc_sensors[i].flags = ENVSYS_FMONLIMITS;
		sc->sc_sensors[i].flags |= ENVSYS_FHAS_ENTROPY;
		sc->sc_sensors[i].private = i;
		sysmon_envsys_sensor_attach(sc->sc_sme, &sc->sc_sensors[i]);
	}

	iic_smbus_read_byte(sc->sc_tag, sc->sc_addr, W83795G_GPIO_M, &gpiom, 0);
	iic_release_bus(sc->sc_tag, 0);
	
	if (conf & CONFIG_CONFIG48)
		gba.gba_npins = 4;
	else
		gba.gba_npins = 8;
	gba.gba_gc = &sc->sc_gpio_gc;
	gba.gba_pins = sc->sc_gpio_pins;

	for (i = 0; i < gba.gba_npins; i++) {
		sc->sc_gpio_pins[i].pin_num = i;  
		sc->sc_gpio_pins[i].pin_caps = GPIO_PIN_OUTPUT | GPIO_PIN_INPUT;
		sc->sc_gpio_pins[i].pin_flags = (gpiom & (1 << i)) ?
		    GPIO_PIN_OUTPUT : GPIO_PIN_INPUT;
		sc->sc_gpio_pins[i].pin_state = w83795g_gpio_read(sc, i);
	}

	if (sysmon_envsys_register(sc->sc_sme))
		aprint_error_dev(self, "unable to register with sysmon\n");

	if (sysmon_wdog_register(&sc->sc_smw) != 0)
		aprint_error_dev(self, "couldn't register watchdog\n");

	if (!pmf_device_register(self, NULL, NULL))
		aprint_error_dev(self, "couldn't establish power handler\n");

	config_found(self, &gba, gpiobus_print);
}

static void
w83795g_refresh(struct sysmon_envsys *sme, envsys_data_t *edata)
{
	struct w83795g_softc *sc = sme->sme_cookie;
	const struct w83795g_sensor *sensor = &sensors[edata->private];
	uint8_t msb, lsb;

	sensor = &sensors[edata->private];

	iic_acquire_bus(sc->sc_tag, 0);
	iic_smbus_write_byte(sc->sc_tag, sc->sc_addr, W83795G_BANKSEL, 0, 0);
	iic_smbus_read_byte(sc->sc_tag, sc->sc_addr, sensor->msb, &msb, 0);
	iic_smbus_read_byte(sc->sc_tag, sc->sc_addr, W83795G_VR_LSB, &lsb, 0);
	iic_release_bus(sc->sc_tag, 0);

	switch (edata->units) {
	case ENVSYS_SVOLTS_DC:
		if (sensor->msb == W83795G_3VDD ||
		    sensor->msb == W83795G_3VSB ||
		    sensor->msb == W83795G_VBAT)
			edata->value_cur = (msb << 2 | lsb >> 6) * 6000;
		else
			edata->value_cur = (msb << 2 | lsb >> 6) * 2000;
		break;
	case ENVSYS_STEMP:
		edata->value_cur = ((int8_t)msb << 2 | lsb >> 6) * 250000 +
		    273150000;
		break;
	case ENVSYS_SFANRPM:
		edata->value_cur = 1350000 / (msb << 4 | lsb >> 4);
		break;
	}

	edata->state = ENVSYS_SVALID;
}

static void
w83795g_get_limits(struct sysmon_envsys *sme, envsys_data_t *edata,
    sysmon_envsys_lim_t *limits, uint32_t *props)
{
	struct w83795g_softc *sc = sme->sme_cookie;
	const struct w83795g_sensor *sensor = &sensors[edata->private];
	uint8_t index, msb, lsb;

	iic_acquire_bus(sc->sc_tag, 0);
	iic_smbus_write_byte(sc->sc_tag, sc->sc_addr, W83795G_BANKSEL, 0, 0);

	switch (edata->units) {
	case ENVSYS_SVOLTS_DC:
		break;
	case ENVSYS_STEMP:
		if (sensor->msb == W83795G_TR5)
			index = W83795G_TR5CRIT;
		else if (sensor->msb == W83795G_TR6)
			index = W83795G_TR6CRIT;
		else if (sensor->msb >= W83795G_DTS1)
			index = W83795G_DTSCRIT;
		else
			index = W83795G_TD1CRIT +
			    (sensor->msb - W83795G_TD1) * 4;
		iic_smbus_read_byte(sc->sc_tag, sc->sc_addr, index, &msb, 0);
		limits->sel_critmax = (int8_t)msb * 1000000 + 273150000;
		index += 2;
		iic_smbus_read_byte(sc->sc_tag, sc->sc_addr, index++, &msb, 0);
		limits->sel_warnmax = (int8_t)msb * 1000000 + 273150000;
		*props |= PROP_CRITMAX | PROP_WARNMAX;
		break;
	case ENVSYS_SFANRPM:
		index = W83795G_FAN1HL + (sensor->msb - W83795G_FANIN1) * 2;
		iic_smbus_read_byte(sc->sc_tag, sc->sc_addr, index, &msb, 0);
		index = W83795G_FHL1LSB + (sensor->msb - W83795G_FANIN1) / 2;
		iic_smbus_read_byte(sc->sc_tag, sc->sc_addr, index, &lsb, 0);
		if (index % 2)
			lsb >>= 4;
		else
			lsb &= 0xf;
		limits->sel_warnmin = 1350000 / (msb << 4 | lsb);
		*props |= PROP_WARNMIN;
		break;
	}

	iic_release_bus(sc->sc_tag, 0);
}

static int
w83795g_gpio_read(void *arg, int pin)
{
	struct w83795g_softc *sc = arg;
	uint8_t in, out;

	iic_acquire_bus(sc->sc_tag, 0);
	iic_smbus_write_byte(sc->sc_tag, sc->sc_addr, W83795G_BANKSEL, 0, 0);
	iic_smbus_read_byte(sc->sc_tag, sc->sc_addr, W83795G_GPIO_I, &in, 0);
	iic_smbus_read_byte(sc->sc_tag, sc->sc_addr, W83795G_GPIO_O, &out, 0);
	iic_release_bus(sc->sc_tag, 0);

	if (sc->sc_gpio_pins[pin].pin_flags == GPIO_PIN_OUTPUT)
		in = out;
	
	return (in & (1 << pin)) ? GPIO_PIN_HIGH : GPIO_PIN_LOW;
}

static void
w83795g_gpio_write(void *arg, int pin, int value)
{
	struct w83795g_softc *sc = arg;
	uint8_t out;

	iic_acquire_bus(sc->sc_tag, 0);
	iic_smbus_write_byte(sc->sc_tag, sc->sc_addr, W83795G_BANKSEL, 0, 0);
	iic_smbus_read_byte(sc->sc_tag, sc->sc_addr, W83795G_GPIO_O, &out, 0);

	if (value == GPIO_PIN_LOW)
		out &= ~(1 << pin);
	else if (value == GPIO_PIN_HIGH)
		out |= (1 << pin);

	iic_smbus_write_byte(sc->sc_tag, sc->sc_addr, W83795G_GPIO_O, out, 0);
	iic_release_bus(sc->sc_tag, 0);
}

static void
w83795g_gpio_ctl(void *arg, int pin, int flags)
{
	struct w83795g_softc *sc = arg;
	uint8_t mode;

	iic_acquire_bus(sc->sc_tag, 0);
	iic_smbus_write_byte(sc->sc_tag, sc->sc_addr, W83795G_BANKSEL, 0, 0);
	iic_smbus_read_byte(sc->sc_tag, sc->sc_addr, W83795G_GPIO_M, &mode, 0);

	if (flags & GPIO_PIN_INPUT)
		mode &= ~(1 << pin);
	if (flags & GPIO_PIN_OUTPUT)
		mode |= (1 << pin);

	iic_smbus_write_byte(sc->sc_tag, sc->sc_addr, W83795G_GPIO_M, mode, 0);
	iic_release_bus(sc->sc_tag, 0);
}

static int
w83795g_wdog_setmode(struct sysmon_wdog *smw)
{
	struct w83795g_softc *sc = smw->smw_cookie;

	/*
	 * This device also supports a "hard" watchdog mode, which survives
	 * across reboots, but making use of that would require sysmon_wdog
	 * to have a way of querying the watchdog state at startup.
	 */

	iic_acquire_bus(sc->sc_tag, 0);
	iic_smbus_write_byte(sc->sc_tag, sc->sc_addr, W83795G_BANKSEL, 0, 0);
	iic_smbus_write_byte(sc->sc_tag, sc->sc_addr, W83795G_WDT_ENA,
	    WDT_ENA_ENWDT | WDT_ENA_SOFT, 0);
	if ((smw->smw_mode & WDOG_MODE_MASK) == WDOG_MODE_DISARMED)
		iic_smbus_write_byte(sc->sc_tag, sc->sc_addr, W83795G_WDTLOCK,
		    WDTLOCK_DISABLE_SOFT, 0);
	else
		iic_smbus_write_byte(sc->sc_tag, sc->sc_addr, W83795G_WDTLOCK,
		    WDTLOCK_ENABLE_SOFT, 0);
	iic_release_bus(sc->sc_tag, 0);

	if (smw->smw_period == WDOG_PERIOD_DEFAULT)
		smw->smw_period = 60;
	smw->smw_period = roundup(smw->smw_period, 60);
	w83795g_wdog_tickle(smw);

	return 0;
}

static int
w83795g_wdog_tickle(struct sysmon_wdog *smw)
{
	struct w83795g_softc *sc = smw->smw_cookie;

	iic_acquire_bus(sc->sc_tag, 0);
	iic_smbus_write_byte(sc->sc_tag, sc->sc_addr, W83795G_BANKSEL, 0, 0);
	iic_smbus_write_byte(sc->sc_tag, sc->sc_addr, W83795G_WDT_CNT,
	    smw->smw_period / 60, 0);
	iic_release_bus(sc->sc_tag, 0);

	return 0;
}
