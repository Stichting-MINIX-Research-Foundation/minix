/*	$NetBSD: g760a.c,v 1.4 2012/07/29 07:04:09 mlelstv Exp $	*/

/*-
 * Copyright (C) 2008 A.Leo.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * The driver for the G760A FAN Speed PWM controller.
 */

#include <sys/cdefs.h>

__KERNEL_RCSID(0, "$NetBSD: g760a.c,v 1.4 2012/07/29 07:04:09 mlelstv Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/sysctl.h>

#include <dev/sysmon/sysmonvar.h>

#include <dev/i2c/i2cvar.h>
#include <dev/i2c/g760areg.h>


struct g760a_softc {
	device_t sc_dev;
	struct sysmon_envsys *sc_sme;

	envsys_data_t sc_sensor;
	i2c_tag_t sc_tag;
	int sc_addr;
};

static int g760a_match(device_t, struct cfdata*, void*);
static void g760a_attach(device_t, device_t, void*);
static void g760a_setup(struct g760a_softc*);
static uint8_t g760a_readreg(struct g760a_softc*, uint8_t);
static void g760a_writereg(struct g760a_softc*, uint8_t, uint8_t);

static int g760a_reg2rpm(int);

static void g760a_refresh(struct sysmon_envsys*, envsys_data_t*);
static int sysctl_g760a_rpm(SYSCTLFN_PROTO);

CFATTACH_DECL_NEW(g760a, sizeof(struct g760a_softc),
		g760a_match, g760a_attach, NULL, NULL);

static int
g760a_match(device_t parent, struct cfdata* cf, void* arg)
{
	struct i2c_attach_args* ia = arg;

	if (ia->ia_addr == G760A_ADDR) {
		/*
		 * TODO: set up minimal speed? 
		 */
		return 1;
	}

	return 0;
}


static void
g760a_attach(device_t parent, device_t self, void* arg)
{
	struct i2c_attach_args* ia = arg;
	struct g760a_softc* sc = device_private(self);

	aprint_normal(": G760A Fan Controller\n");

	sc->sc_dev = self;
	sc->sc_tag = ia->ia_tag;
	sc->sc_addr = ia->ia_addr;

	g760a_setup(sc);
}


static int
g760a_reg2rpm(int n)
{
	if(n == 255)
		return 0;

	if(n == 0)
		return 255;

	return G760A_N2RPM(n);
}


static uint8_t
g760a_readreg(struct g760a_softc* sc, uint8_t reg)
{
	uint8_t data;

	if (iic_acquire_bus(sc->sc_tag, 0)) {
		aprint_error_dev(sc->sc_dev, "unable to acquire the iic bus\n");
		return 0;
	}

	iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP, sc->sc_addr, &reg, 1,
	    &data, 1, 0);
	iic_release_bus(sc->sc_tag, 0);

	return data;
}


static void
g760a_writereg(struct g760a_softc* sc, uint8_t reg, uint8_t data)
{

	if (iic_acquire_bus(sc->sc_tag, 0)) {
		aprint_error_dev(sc->sc_dev, "unable to acquire the iic bus\n");
		return;
	}

	iic_exec(sc->sc_tag, I2C_OP_WRITE_WITH_STOP, sc->sc_addr, &reg, 1,
	    &data, 1, 0);
	iic_release_bus(sc->sc_tag, 0);
}


SYSCTL_SETUP(sysctl_g760a_setup, "sysctl g760a subtree setup")
{

	sysctl_createv(NULL, 0, NULL, NULL,  
			CTLFLAG_PERMANENT,
			CTLTYPE_NODE, "machdep", NULL, 
			NULL, 0, NULL, 0,
			CTL_MACHDEP, CTL_EOL);
}


/*ARGUSED*/
static int
sysctl_g760a_rpm(SYSCTLFN_ARGS)
{
	int error, t;
	struct sysctlnode node;
	struct g760a_softc* sc;
       	
	node = *rnode;
	sc = node.sysctl_data;

	t = g760a_readreg(sc, G760A_REG_SET_CNT);
	t = g760a_reg2rpm(t);

	node.sysctl_data = &t;

	error = sysctl_lookup(SYSCTLFN_CALL(&node));

	if (error || newp == NULL)
		return error;

	if (t > 20000 || t < G760A_N2RPM(254))
		return EINVAL;

	t = g760a_reg2rpm(t);

	g760a_writereg(sc, G760A_REG_SET_CNT, t);

	return 0;
}


static void
g760a_refresh(struct sysmon_envsys* sme, envsys_data_t* edata)
{
	struct g760a_softc* sc = sme->sme_cookie;
	
	switch (edata->units) {
		case ENVSYS_SFANRPM:
			{
				uint8_t n;
				
				n = g760a_readreg(sc, G760A_REG_ACT_CNT);
				edata->value_cur = g760a_reg2rpm(n);
			}
			break;
		default:
			aprint_error_dev(sc->sc_dev, "oops\n");
	}

	edata->state = ENVSYS_SVALID;
}


static void
g760a_setup(struct g760a_softc* sc)
{
	int error;
	int ret;
	const struct sysctlnode *me, *node;

	sc->sc_sme = sysmon_envsys_create();

	ret = sysctl_createv(NULL, 0, NULL, &me,
			CTLFLAG_READWRITE,
			CTLTYPE_NODE, device_xname(sc->sc_dev), NULL,
			NULL, 0, NULL, 0,
			CTL_MACHDEP, CTL_CREATE, CTL_EOL);

	(void)strlcpy(sc->sc_sensor.desc, "sysfan rpm",
			sizeof(sc->sc_sensor.desc));
	sc->sc_sensor.units = ENVSYS_SFANRPM;
	sc->sc_sensor.state = ENVSYS_SINVALID;

	if (sysmon_envsys_sensor_attach(sc->sc_sme, &sc->sc_sensor))
		goto out;

	ret = sysctl_createv(NULL, 0, NULL, &node,
			CTLFLAG_READWRITE,
			CTLTYPE_INT, "rpm", sc->sc_sensor.desc,
			sysctl_g760a_rpm, 0x42, (void*)sc, 0,
			CTL_MACHDEP, me->sysctl_num, CTL_CREATE, CTL_EOL);

	sc->sc_sme->sme_name = device_xname(sc->sc_dev);
	sc->sc_sme->sme_cookie = sc;
	sc->sc_sme->sme_refresh = g760a_refresh;

	error = sysmon_envsys_register(sc->sc_sme);

	if (error) {
		aprint_error_dev(sc->sc_dev,
		    "unable to register with sysmon. errorcode %i\n", error);
		goto out;
	}

	return;
out:
	sysmon_envsys_destroy(sc->sc_sme);
}
