/*	$NetBSD: apple_smc_fan.c,v 1.5 2015/04/23 23:23:00 pgoyette Exp $	*/

/*
 * Apple System Management Controller: Fans
 */

/*-
 * Copyright (c) 2013 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Taylor R. Campbell.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: apple_smc_fan.c,v 1.5 2015/04/23 23:23:00 pgoyette Exp $");

#include <sys/types.h>
#include <sys/param.h>
#include <sys/device.h>
#include <sys/kmem.h>
#include <sys/module.h>
#if 0                           /* XXX sysctl */
#include <sys/sysctl.h>
#endif
#include <sys/systm.h>

#include <dev/ic/apple_smc.h>

#include <dev/sysmon/sysmonvar.h>

#define	APPLE_SMC_NFANS_KEY		"FNum"

static const struct fan_sensor {
	const char *fs_name;
	const char *fs_key_suffix;
} fan_sensors[] = {
	{ "actual",	"Ac" },
	{ "minimum",	"Mn" },
	{ "maximum",	"Mx" },
	{ "safe",	"Sf" },
	{ "target",	"Tg" },
};

struct apple_smc_fan_softc {
	device_t		sc_dev;
	struct apple_smc_tag	*sc_smc;
	struct sysmon_envsys	*sc_sme;
	uint8_t			sc_nfans;
	struct {
		struct {
			struct apple_smc_key	*sensor_key;
			struct envsys_data	sensor_data;
		} sensors[__arraycount(fan_sensors)];
	}			*sc_fans;

#if 0				/* XXX sysctl */
	struct sysctllog	*sc_sysctl_log;
	const struct sysctlnode	*sc_sysctl_node;
#endif
};

struct fan_desc {
	uint8_t fd_type;
	uint8_t fd_zone;
	uint8_t fd_location;
	uint8_t fd_reserved0;
	char fd_name[12];
} __packed;

static int	apple_smc_fan_match(device_t, cfdata_t, void *);
static void	apple_smc_fan_attach(device_t, device_t, void *);
static int	apple_smc_fan_detach(device_t, int);
static int	apple_smc_fan_attach_sensors(struct apple_smc_fan_softc *);
static void	apple_smc_fan_attach_sensor(struct apple_smc_fan_softc *,
		    uint8_t, const char *, uint8_t);
static void	apple_smc_fan_refresh(struct sysmon_envsys *,
		    struct envsys_data *);
static void	apple_smc_fan_release_keys(struct apple_smc_fan_softc *);
#if 0				/* XXX sysctl */
static int	apple_smc_fan_sysctl_setup(struct apple_smc_fan_softc *);
static void	apple_smc_fan_sysctl_setup_1(struct apple_smc_tag *,
		    uint8_t);
#endif

CFATTACH_DECL_NEW(apple_smc_fan, sizeof(struct apple_smc_fan_softc),
    apple_smc_fan_match, apple_smc_fan_attach, apple_smc_fan_detach, NULL);

static int
apple_smc_fan_match(device_t parent, cfdata_t match, void *aux)
{
	const struct apple_smc_attach_args *asa = aux;
	struct apple_smc_key *nfans_key;
	uint8_t nfans;
	int rv = 0;
	int error;

	/* Find how to find how many fans there are.  */
	error = apple_smc_named_key(asa->asa_smc, APPLE_SMC_NFANS_KEY,
	    APPLE_SMC_TYPE_UINT8, &nfans_key);
	if (error)
		goto out0;

	/* Find how many fans there are.  */
	error = apple_smc_read_key_1(asa->asa_smc, nfans_key, &nfans);
	if (error)
		goto out1;

	/* Attach only if there's at least one fan.  */
	if (nfans > 0)
		rv = 1;

out1:	apple_smc_release_key(asa->asa_smc, nfans_key);
out0:	return rv;
}

static void
apple_smc_fan_attach(device_t parent, device_t self, void *aux)
{
	struct apple_smc_fan_softc *sc = device_private(self);
	const struct apple_smc_attach_args *asa = aux;
	struct apple_smc_key *nfans_key;
	int error;

	/* Identify ourselves.  */
	aprint_normal(": Apple SMC fan sensors\n");

	/* Initialize the softc.  */
	sc->sc_dev = self;
	sc->sc_smc = asa->asa_smc;

	/* Find how to find how many fans there are.  */
	error = apple_smc_named_key(sc->sc_smc, APPLE_SMC_NFANS_KEY,
	    APPLE_SMC_TYPE_UINT8, &nfans_key);
	if (error)
		goto out0;

	/* Find how many fans there are.  */
	error = apple_smc_read_key_1(sc->sc_smc, nfans_key, &sc->sc_nfans);
	if (error)
		goto out1;

	/*
	 * There should be at least one, but just in case the hardware
	 * changed its mind in the interim...
	 */
	if (sc->sc_nfans == 0) {
		aprint_error_dev(self, "no fans\n");
		goto out1;
	}

	/*
	 * The number of fans must fit in a single decimal digit for
	 * the names of the fan keys; see the fan_sensor table above.
	 */
	if (sc->sc_nfans >= 10) {
		aprint_error_dev(self, "too many fans: %"PRIu8"\n",
		    sc->sc_nfans);
		sc->sc_nfans = 9;
	}

#if 0				/* XXX sysctl */
	/* Set up the sysctl tree for controlling the fans.  */
	error = apple_smc_fan_sysctl_setup(sc);
	if (error)
		goto fail0;
#endif

	/* Attach the sensors to sysmon_envsys.  */
	error = apple_smc_fan_attach_sensors(sc);
	if (error)
		goto fail1;

	/* Success!  */
	goto out1;

#if 0
fail2:
	apple_smc_fan_detach_sensors(sc);
#endif

fail1:
#if 0				/* XXX sysctl */
	sysctl_teardown(&sc->sc_sysctl_log);
fail0:
#endif

out1:	apple_smc_release_key(sc->sc_smc, nfans_key);
out0:	return;
}

static int
apple_smc_fan_detach(device_t self, int flags)
{
	struct apple_smc_fan_softc *sc = device_private(self);

	/* If we registered with sysmon_envsys, unregister.  */
	if (sc->sc_sme != NULL) {
		sysmon_envsys_unregister(sc->sc_sme);
		sc->sc_sme = NULL;

		KASSERT(sc->sc_fans != NULL);
		KASSERT(sc->sc_nfans > 0);
		KASSERT(sc->sc_nfans < 10);

		/* Release the keys and free the memory for fan records. */
		apple_smc_fan_release_keys(sc);
		kmem_free(sc->sc_fans,
		    (sizeof(sc->sc_fans[0]) * sc->sc_nfans));
		sc->sc_fans = NULL;
		sc->sc_nfans = 0;
	}

#if 0				/* XXX sysctl */
	/* Tear down all the sysctl knobs we set up.  */
	sysctl_teardown(&sc->sc_sysctl_log);
#endif

	return 0;
}

static int
apple_smc_fan_attach_sensors(struct apple_smc_fan_softc *sc)
{
	uint8_t fan, sensor;
	char fan_desc_key_name[4 + 1];
	struct apple_smc_key *fan_desc_key;
	struct fan_desc fan_desc;
	char name[sizeof(fan_desc.fd_name) + 1];
	int error;

	/* Create a sysmon_envsys record, but don't register it yet.  */
	sc->sc_sme = sysmon_envsys_create();
	sc->sc_sme->sme_name = device_xname(sc->sc_dev);
	sc->sc_sme->sme_cookie = sc;
	sc->sc_sme->sme_refresh = apple_smc_fan_refresh;

	/* Create an array of fan sensor records.  */
	CTASSERT(10 <= (SIZE_MAX / sizeof(sc->sc_fans[0])));
	sc->sc_fans = kmem_zalloc((sizeof(sc->sc_fans[0]) * sc->sc_nfans),
	    KM_SLEEP);

	/* Find all the fans.  */
	for (fan = 0; fan < sc->sc_nfans; fan++) {

		/* Format the name of the key for the fan's description.  */
		(void)snprintf(fan_desc_key_name, sizeof(fan_desc_key_name),
		    "F%"PRIu8"ID", fan);
		KASSERT(4 == strlen(fan_desc_key_name));

		/* Look up the key for this fan's description.  */
		error = apple_smc_named_key(sc->sc_smc, fan_desc_key_name,
		    APPLE_SMC_TYPE_FANDESC, &fan_desc_key);
		if (error) {
			aprint_error_dev(sc->sc_dev,
			    "error identifying fan %"PRIu8": %d\n",
			    fan, error);
			continue;
		}

		/* Read the description of this fan.  */
		error = apple_smc_read_key(sc->sc_smc, fan_desc_key, &fan_desc,
		    sizeof(fan_desc));
		if (error) {
			aprint_error_dev(sc->sc_dev,
			    "error identifying fan %"PRIu8": %d\n",
			    fan, error);
			continue;
		}

		/*
		 * XXX Do more with the fan description...
		 */

		/* Make a null-terminated copy of this fan's description.  */
		(void)memcpy(name, fan_desc.fd_name, sizeof(fan_desc.fd_name));
		name[sizeof(fan_desc.fd_name)] = '\0';

		/* Attach all the sensors for this fan.  */
		for (sensor = 0; sensor < __arraycount(fan_sensors); sensor++)
			apple_smc_fan_attach_sensor(sc, fan, name, sensor);

#if 0				/* XXX sysctl */
		/* Attach sysctl knobs to control this fan.  */
		apple_smc_fan_sysctl_setup_1(sc, fan);
#endif
	}

	/* Fan sensors are all attached.  Register with sysmon_envsys now.  */
	error = sysmon_envsys_register(sc->sc_sme);
	if (error)
		goto fail;

	/* Success!  */
	error = 0;
	goto out;

fail:	sysmon_envsys_destroy(sc->sc_sme);
	sc->sc_sme = NULL;
out:	return error;
}

static void
apple_smc_fan_attach_sensor(struct apple_smc_fan_softc *sc, uint8_t fan,
    const char *name, uint8_t sensor)
{
	char key_name[4 + 1];
	struct apple_smc_key **keyp;
	struct envsys_data *edata;
	int error;

	KASSERT(fan < sc->sc_nfans);
	KASSERT(sensor < __arraycount(fan_sensors));

	/* Format the name of the key for this fan sensor.   */
	(void)snprintf(key_name, sizeof(key_name), "F%d%s",
	    (int)sensor, fan_sensors[sensor].fs_key_suffix);
	KASSERT(strlen(key_name) == 4);

	/* Look up the key for this fan sensor. */
	keyp = &sc->sc_fans[fan].sensors[sensor].sensor_key;
	error = apple_smc_named_key(sc->sc_smc, key_name, APPLE_SMC_TYPE_FPE2,
	    keyp);
	if (error)
		goto fail0;

	/* Initialize the envsys_data record for this fan sensor.  */
	edata = &sc->sc_fans[fan].sensors[sensor].sensor_data;
	edata->units = ENVSYS_SFANRPM;
	edata->state = ENVSYS_SINVALID;
	edata->flags = ENVSYS_FHAS_ENTROPY;
	(void)snprintf(edata->desc, sizeof(edata->desc), "fan %s %s speed",
	    name, fan_sensors[sensor].fs_name);

	/* Attach this fan sensor to sysmon_envsys.  */
	error = sysmon_envsys_sensor_attach(sc->sc_sme, edata);
	if (error)
		goto fail1;

	/* Success!  */
	return;

fail1:	apple_smc_release_key(sc->sc_smc, *keyp);
fail0:	*keyp = NULL;
	aprint_error_dev(sc->sc_dev,
	    "failed to attach fan %s %s speed sensor: %d\n",
	    name, fan_sensors[sensor].fs_name, error);
}

static void
apple_smc_fan_refresh(struct sysmon_envsys *sme, struct envsys_data *edata)
{
	struct apple_smc_fan_softc *sc = sme->sme_cookie;
	uint8_t fan, sensor;
	struct apple_smc_key *key;
	uint16_t rpm;
	int error;

	/* Sanity-check the sensor number out of paranoia.  */
	CTASSERT(10 <= (SIZE_MAX / __arraycount(fan_sensors)));
	KASSERT(sc->sc_nfans < 10);
	if (edata->sensor >= (sc->sc_nfans * __arraycount(fan_sensors))) {
		aprint_error_dev(sc->sc_dev, "unknown sensor %"PRIu32"\n",
		    edata->sensor);
		return;
	}

	/* Pick apart the fan number and its sensor number.  */
	fan = (edata->sensor / __arraycount(fan_sensors));
	sensor = (edata->sensor % __arraycount(fan_sensors));

	KASSERT(fan < sc->sc_nfans);
	KASSERT(sensor < __arraycount(fan_sensors));
	KASSERT(edata == &sc->sc_fans[fan].sensors[sensor].sensor_data);

	/*
	 * If we're refreshing, this sensor got attached, so we ought
	 * to have a sensor key.  Grab it.
	 */
	key = sc->sc_fans[fan].sensors[sensor].sensor_key;
	KASSERT(key != NULL);

	/* Read the fan sensor value, in rpm.  */
	error = apple_smc_read_key_2(sc->sc_smc, key, &rpm);
	if (error) {
		aprint_error_dev(sc->sc_dev,
		    "failed to read fan %d %s speed: %d\n",
		    fan, fan_sensors[sensor].fs_name, error);
		edata->state = ENVSYS_SINVALID;
		return;
	}

	/* Success!  */
	edata->value_cur = rpm;
	edata->state = ENVSYS_SVALID;
}

static void
apple_smc_fan_release_keys(struct apple_smc_fan_softc *sc)
{
	uint8_t fan, sensor;

	for (fan = 0; fan < sc->sc_nfans; fan++) {
		for (sensor = 0;
		     sensor < __arraycount(fan_sensors);
		     sensor++) {
			struct apple_smc_key **const keyp =
			    &sc->sc_fans[fan].sensors[sensor].sensor_key;
			if (*keyp != NULL) {
				apple_smc_release_key(sc->sc_smc, *keyp);
				*keyp = NULL;
			}
		}
	}
}

#if 0				/* XXX sysctl */
static int
apple_smc_fan_sysctl_setup(struct apple_smc_fan_softc *sc)
{
	...
}

static void
apple_smc_fan_sysctl_setup_1(struct apple_smc_fan_softc *sc, uint8_t fan)
{
}
#endif

MODULE(MODULE_CLASS_DRIVER, apple_smc_fan, "apple_smc,sysmon_envsys");

#ifdef _MODULE
#include "ioconf.c"
#endif

static int
apple_smc_fan_modcmd(modcmd_t cmd, void *arg __unused)
{
#ifdef _MODULE
	int error;
#endif

	switch (cmd) {
	case MODULE_CMD_INIT:
#ifdef _MODULE
		error = config_init_component(cfdriver_ioconf_apple_smc_fan,
		    cfattach_ioconf_apple_smc_fan,
		    cfdata_ioconf_apple_smc_fan);
		if (error)
			return error;
#endif
		return 0;

	case MODULE_CMD_FINI:
#ifdef _MODULE
		error = config_fini_component(cfdriver_ioconf_apple_smc_fan,
		    cfattach_ioconf_apple_smc_fan,
		    cfdata_ioconf_apple_smc_fan);
		if (error)
			return error;
#endif
		return 0;

	default:
		return ENOTTY;
	}
}
