/*	$NetBSD: apple_smc_temp.c,v 1.5 2015/04/23 23:23:00 pgoyette Exp $	*/

/*
 * Apple System Management Controller: Temperature Sensors
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
__KERNEL_RCSID(0, "$NetBSD: apple_smc_temp.c,v 1.5 2015/04/23 23:23:00 pgoyette Exp $");

#include <sys/types.h>
#include <sys/param.h>
#include <sys/device.h>
#include <sys/kmem.h>
#include <sys/module.h>
#include <sys/systm.h>

#include <dev/ic/apple_smc.h>

#include <dev/sysmon/sysmonvar.h>

struct apple_smc_temp_softc {
	device_t		sc_dev;
	struct apple_smc_tag	*sc_smc;
	struct sysmon_envsys	*sc_sme;
	struct {
		struct apple_smc_key	*sensor_key;
		struct envsys_data	sensor_data;
	}			*sc_sensors;
	size_t			sc_nsensors;
};

static int	apple_smc_temp_match(device_t, cfdata_t, void *);
static void	apple_smc_temp_attach(device_t, device_t, void *);
static int	apple_smc_temp_detach(device_t, int);
static void	apple_smc_temp_refresh(struct sysmon_envsys *,
		    struct envsys_data *);
static int	apple_smc_temp_count_sensors(struct apple_smc_tag *,
		    uint32_t *);
static void	apple_smc_temp_count_sensors_scanner(struct apple_smc_tag *,
		    void *, struct apple_smc_key *);
static int	apple_smc_temp_find_sensors(struct apple_smc_temp_softc *);
static int	apple_smc_temp_find_sensors_init(struct apple_smc_tag *,
		    void *, uint32_t);
static void	apple_smc_temp_find_sensors_scanner(struct apple_smc_tag *,
		    void *, struct apple_smc_key *);
static void	apple_smc_temp_release_keys(struct apple_smc_temp_softc *);
static int	apple_smc_scan_temp_sensors(struct apple_smc_tag *, void *,
		    int (*)(struct apple_smc_tag *, void *, uint32_t),
		    void (*)(struct apple_smc_tag *, void *,
			struct apple_smc_key *));
static int	apple_smc_bound_temp_sensors(struct apple_smc_tag *,
		    uint32_t *, uint32_t *);
static bool	apple_smc_temp_sensor_p(const struct apple_smc_key *);

CFATTACH_DECL_NEW(apple_smc_temp, sizeof(struct apple_smc_temp_softc),
    apple_smc_temp_match, apple_smc_temp_attach, apple_smc_temp_detach, NULL);

static int
apple_smc_temp_match(device_t parent, cfdata_t match, void *aux)
{
	const struct apple_smc_attach_args *const asa = aux;
	uint32_t nsensors;
	int error;

	/* Find how many temperature sensors we have. */
	error = apple_smc_temp_count_sensors(asa->asa_smc, &nsensors);
	if (error)
		return 0;

	/* If there aren't any, don't bother attaching.  */
	if (nsensors == 0)
		return 0;

	return 1;
}

static void
apple_smc_temp_attach(device_t parent, device_t self, void *aux)
{
	struct apple_smc_temp_softc *const sc = device_private(self);
	const struct apple_smc_attach_args *const asa = aux;
	int error;

	/* Identify ourselves.  */
	aprint_normal(": Apple SMC temperature sensors\n");

	/* Initialize the softc. */
	sc->sc_dev = self;
	sc->sc_smc = asa->asa_smc;

	/* Create a sysmon_envsys record, but don't register it yet.  */
	sc->sc_sme = sysmon_envsys_create();
	sc->sc_sme->sme_name = device_xname(self);
	sc->sc_sme->sme_cookie = sc;
	sc->sc_sme->sme_refresh = apple_smc_temp_refresh;

	/* Find and attach all the sensors.  */
	error = apple_smc_temp_find_sensors(sc);
	if (error) {
		aprint_error_dev(self, "failed to find sensors: %d\n", error);
		goto fail;
	}

	/* Sensors are all attached.  Register with sysmon_envsys now.  */
	error = sysmon_envsys_register(sc->sc_sme);
	if (error) {
		aprint_error_dev(self, "failed to register with sysmon_envsys:"
		    " %d\n", error);
		goto fail;
	}

	/* Success!  */
	return;

fail:	sysmon_envsys_destroy(sc->sc_sme);
	sc->sc_sme = NULL;
}

static int
apple_smc_temp_detach(device_t self, int flags)
{
	struct apple_smc_temp_softc *const sc = device_private(self);

	/* If we registered with sysmon_envsys, unregister.  */
	if (sc->sc_sme != NULL) {
		sysmon_envsys_unregister(sc->sc_sme);
		sc->sc_sme = NULL;

		KASSERT(sc->sc_sensors != NULL);
		KASSERT(sc->sc_nsensors > 0);

		/* Release the keys and free the memory for sensor records.  */
		apple_smc_temp_release_keys(sc);
		kmem_free(sc->sc_sensors,
		    (sizeof(sc->sc_sensors[0]) * sc->sc_nsensors));
		sc->sc_sensors = NULL;
		sc->sc_nsensors = 0;
	}

	/* Success!  */
	return 0;
}

static void
apple_smc_temp_refresh(struct sysmon_envsys *sme, struct envsys_data *edata)
{
	struct apple_smc_temp_softc *const sc = sme->sme_cookie;
	const struct apple_smc_key *key;
	uint16_t utemp16;
	int32_t temp;
	int error;

	/* Sanity-check the sensor number out of paranoia.  */
	if (edata->sensor >= sc->sc_nsensors) {
		aprint_error_dev(sc->sc_dev, "unknown sensor %"PRIu32"\n",
		    edata->sensor);
		return;
	}

	/* Read the raw temperature sensor value.  */
	key = sc->sc_sensors[edata->sensor].sensor_key;
	KASSERT(key != NULL);
	error = apple_smc_read_key_2(sc->sc_smc, key, &utemp16);
	if (error) {
		aprint_error_dev(sc->sc_dev,
		    "failed to read temperature sensor %"PRIu32" (%s): %d\n",
		    edata->sensor, apple_smc_key_name(key), error);
		edata->state = ENVSYS_SINVALID;
		return;
	}

	/* Sign-extend, in case we ever get below freezing...  */
	temp = (int16_t)utemp16;

	/* Convert to `millicentigrade'.  */
	temp *= 250;
	temp >>= 6;

	/* Convert to millikelvins.  */
	temp += 273150;

	/* Finally, convert to microkelvins as sysmon_envsys wants.  */
	temp *= 1000;

	/* Success!  */
	edata->value_cur = temp;
	edata->state = ENVSYS_SVALID;
}

static int
apple_smc_temp_count_sensors(struct apple_smc_tag *smc, uint32_t *nsensors)
{

	/* Start with zero sensors.  */
	*nsensors = 0;

	/* Count 'em.  */
	return apple_smc_scan_temp_sensors(smc, nsensors,
	    NULL,
	    &apple_smc_temp_count_sensors_scanner);
}

static void
apple_smc_temp_count_sensors_scanner(struct apple_smc_tag *smc, void *arg,
    struct apple_smc_key *key)
{
	uint32_t *const nsensors = arg;

	(*nsensors)++;
	apple_smc_release_key(smc, key);
}

struct fss {			/* Find Sensors State */
	struct apple_smc_temp_softc	*fss_sc;
	unsigned int			fss_sensor;
};

static int
apple_smc_temp_find_sensors(struct apple_smc_temp_softc *sc)
{
	struct fss fss;
	int error;

	/* Start with zero sensors.  */
	fss.fss_sc = sc;
	fss.fss_sensor = 0;

	/* Find 'em.  */
	error = apple_smc_scan_temp_sensors(sc->sc_smc, &fss,
	    &apple_smc_temp_find_sensors_init,
	    &apple_smc_temp_find_sensors_scanner);
	if (error)
		return error;

	/*
	 * Success guarantees that sc->sc_nsensors will be nonzero and
	 * sc->sc_sensors will be allocated.
	 */
	KASSERT(sc->sc_sensors != NULL);
	KASSERT(sc->sc_nsensors > 0);

	/* If we didn't find any sensors, bail.  */
	if (fss.fss_sensor == 0) {
		kmem_free(sc->sc_sensors, sc->sc_nsensors);
		sc->sc_sensors = NULL;
		sc->sc_nsensors = 0;
		return EIO;
	}

	/* Shrink the array if we overshot.  */
	if (fss.fss_sensor < sc->sc_nsensors) {
		void *const sensors = kmem_alloc((fss.fss_sensor *
			sizeof(sc->sc_sensors[0])), KM_SLEEP);

		(void)memcpy(sensors, sc->sc_sensors,
		    (fss.fss_sensor * sizeof(sc->sc_sensors[0])));
		kmem_free(sc->sc_sensors, sc->sc_nsensors);
		sc->sc_sensors = sensors;
		sc->sc_nsensors = fss.fss_sensor;
	}

	/* Success!  */
	return 0;
}

static int
apple_smc_temp_find_sensors_init(struct apple_smc_tag *smc, void *arg,
    uint32_t nsensors)
{
	struct fss *const fss = arg;

	/* Record the maximum number of sensors we may have.  */
	fss->fss_sc->sc_nsensors = nsensors;

	/* If we found a maximum of zero sensors, bail.  */
	if (nsensors == 0) {
		fss->fss_sc->sc_sensors = NULL;
		return EIO;
	}

	/*
	 * If there may be any sensors, optimistically allocate as many
	 * records for them as we may possibly need.
	 */
	fss->fss_sc->sc_sensors = kmem_alloc((nsensors *
		sizeof(fss->fss_sc->sc_sensors[0])), KM_SLEEP);

	/* Success!  */
	return 0;
}

static void
apple_smc_temp_find_sensors_scanner(struct apple_smc_tag *smc, void *arg,
    struct apple_smc_key *key)
{
	struct fss *const fss = arg;
	const uint32_t sensor = fss->fss_sensor;
	struct envsys_data *const edata =
	    &fss->fss_sc->sc_sensors[sensor].sensor_data;
	int error;

	/* Initialize the envsys_data record for this temperature sensor.  */
	edata->units = ENVSYS_STEMP;
	edata->state = ENVSYS_SINVALID;
	edata->flags = ENVSYS_FHAS_ENTROPY;

	/*
	 * Use the SMC key name as the temperature sensor's name.
	 *
	 * XXX We ought to use a more meaningful name based on a table
	 * of known temperature sensors.
	 */
	CTASSERT(sizeof(edata->desc) >= 4);
	(void)strlcpy(edata->desc, apple_smc_key_name(key), 4);

	/* Attach this temperature sensor to sysmon_envsys.  */
	error = sysmon_envsys_sensor_attach(fss->fss_sc->sc_sme, edata);
	if (error) {
		aprint_error_dev(fss->fss_sc->sc_dev,
		    "failed to attach temperature sensor %s: %d\n",
		    apple_smc_key_name(key), error);
		return;
	}

	/* Success!  */
	fss->fss_sc->sc_sensors[sensor].sensor_key = key;
	fss->fss_sensor++;
}

static void
apple_smc_temp_release_keys(struct apple_smc_temp_softc *sc)
{
	uint32_t sensor;

	for (sensor = 0; sensor < sc->sc_nsensors; sensor++) {
		KASSERT(sc->sc_sensors[sensor].sensor_key != NULL);
		apple_smc_release_key(sc->sc_smc,
		    sc->sc_sensors[sensor].sensor_key);
	}
}

static int
apple_smc_scan_temp_sensors(struct apple_smc_tag *smc, void *arg,
    int (*init)(struct apple_smc_tag *, void *, uint32_t),
    void (*scanner)(struct apple_smc_tag *, void *, struct apple_smc_key *))
{
	uint32_t tstart, ustart, i;
	struct apple_smc_key *key;
	int error;

	/* Find [start, end) bounds on the temperature sensor key indices.  */
	error = apple_smc_bound_temp_sensors(smc, &tstart, &ustart);
	if (error)
		return error;
	KASSERT(tstart <= ustart);

	/* Inform the caller of the number of candidates.  */
	if (init != NULL) {
		error = (*init)(smc, arg, (ustart - tstart));
		if (error)
			return error;
	}

	/* Take a closer look at all the candidates.  */
	for (i = tstart; i < ustart; i++) {
		error = apple_smc_nth_key(smc, i, NULL, &key);
		if (error)
			continue;

		/* Skip it if it's not a temperature sensor.  */
		if (!apple_smc_temp_sensor_p(key)) {
			apple_smc_release_key(smc, key);
			continue;
		}

		/* Scan it if it is one.  */
		(*scanner)(smc, arg, key);
	}

	/* Success!  */
	return 0;
}

static bool
apple_smc_temp_sensor_p(const struct apple_smc_key *key)
{

	/* It's a temperature sensor iff its type is sp78.  */
	return (0 == memcmp(apple_smc_key_desc(key)->asd_type,
		APPLE_SMC_TYPE_SP78, 4));
}

static int
apple_smc_bound_temp_sensors(struct apple_smc_tag *smc, uint32_t *tstart,
    uint32_t *ustart)
{
	int error;

	/* Find the first `T...' key.  */
	error = apple_smc_key_search(smc, "T", tstart);
	if (error)
		return error;

	/* Find the first `U...' key.  */
	error = apple_smc_key_search(smc, "U", ustart);
	if (error)
		return error;

	/* Sanity check: `T...' keys had better precede `U...' keys.  */
	if (!(*tstart <= *ustart))
		return EIO;

	/* Success!  */
	return 0;
}

MODULE(MODULE_CLASS_DRIVER, apple_smc_temp, "apple_smc,sysmon_envsys");

#ifdef _MODULE
#include "ioconf.c"
#endif

static int
apple_smc_temp_modcmd(modcmd_t cmd, void *arg __unused)
{
#ifdef _MODULE
	int error;
#endif

	switch (cmd) {
	case MODULE_CMD_INIT:
#ifdef _MODULE
		error = config_init_component(cfdriver_ioconf_apple_smc_temp,
		    cfattach_ioconf_apple_smc_temp,
		    cfdata_ioconf_apple_smc_temp);
		if (error)
			return error;
#endif
		return 0;

	case MODULE_CMD_FINI:
#ifdef _MODULE
		error = config_fini_component(cfdriver_ioconf_apple_smc_temp,
		    cfattach_ioconf_apple_smc_temp,
		    cfdata_ioconf_apple_smc_temp);
		if (error)
			return error;
#endif
		return 0;

	default:
		return ENOTTY;
	}
}
