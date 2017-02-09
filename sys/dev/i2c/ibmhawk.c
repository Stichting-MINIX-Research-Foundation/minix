/* $NetBSD: ibmhawk.c,v 1.3 2011/06/21 12:38:27 hannken Exp $ */

/*-
 * Copyright (c) 2011 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Juergen Hannken-Illjes.
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

#include <sys/systm.h>
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/bswap.h>

#include <dev/sysmon/sysmonvar.h>
#include <dev/i2c/i2cvar.h>
#include <dev/i2c/ibmhawkreg.h>
#include <dev/i2c/ibmhawkvar.h>

#if !defined(IBMHAWK_DEBUG)	/* Set to 2 for verbose debug. */
#if defined(DEBUG)
#define IBMHAWK_DEBUG	1
#else
#define IBMHAWK_DEBUG	0
#endif
#endif

/*
 * Known sensors.
 */
static struct ibmhawk_sensordesc {
	const char *desc;
	uint32_t units;
	int offset;
} ibmhawk_sensors[] = {
	{ "Ambient temperature", ENVSYS_STEMP,     IBMHAWK_T_AMBIENT   },
	{ "CPU 1 temperature",   ENVSYS_STEMP,     IBMHAWK_T_CPU       },
	{ "CPU 2 temperature",   ENVSYS_STEMP,     IBMHAWK_T_CPU+1     },
	{ "12 Voltage sensor",   ENVSYS_SVOLTS_DC, IBMHAWK_V_VOLTAGE   },
	{ "5 Voltage sensor",    ENVSYS_SVOLTS_DC, IBMHAWK_V_VOLTAGE+1 },
	{ "3.3 Voltage sensor",  ENVSYS_SVOLTS_DC, IBMHAWK_V_VOLTAGE+2 },
	{ "2.5 Voltage sensor",  ENVSYS_SVOLTS_DC, IBMHAWK_V_VOLTAGE+3 },
	{ "1.5 Voltage sensor",  ENVSYS_SVOLTS_DC, IBMHAWK_V_VOLTAGE+4 },
	{ "1.25 Voltage sensor", ENVSYS_SVOLTS_DC, IBMHAWK_V_VOLTAGE+5 },
	{ "VRM 1",               ENVSYS_SVOLTS_DC, IBMHAWK_V_VOLTAGE+6 },
	{ "Fan 1",               ENVSYS_SFANRPM,   IBMHAWK_F_FAN       },
	{ "Fan 2",               ENVSYS_SFANRPM,   IBMHAWK_F_FAN+1     },
	{ "Fan 3",               ENVSYS_SFANRPM,   IBMHAWK_F_FAN+2     },
	{ "Fan 4",               ENVSYS_SFANRPM,   IBMHAWK_F_FAN+3     },
	{ "Fan 5",               ENVSYS_SFANRPM,   IBMHAWK_F_FAN+4     },
	{ "Fan 6",               ENVSYS_SFANRPM,   IBMHAWK_F_FAN+5     },
};
static const int ibmhawk_num_sensors =
    (sizeof(ibmhawk_sensors)/sizeof(ibmhawk_sensors[0]));

static int ibmhawk_match(device_t, cfdata_t, void *);
static void ibmhawk_attach(device_t, device_t, void *);
static int ibmhawk_detach(device_t, int);
static uint8_t ibmhawk_cksum(uint8_t *);
static int ibmhawk_request(struct ibmhawk_softc *,
    uint8_t, ibmhawk_response_t *);
static uint32_t ibmhawk_normalize(int, uint32_t);
static void ibmhawk_set(struct ibmhawk_softc *, int, int, bool, bool);
static void ibmhawk_refreshall(struct ibmhawk_softc *, bool);
static void ibmhawk_refresh(struct sysmon_envsys *, envsys_data_t *);
static void ibmhawk_get_limits(struct sysmon_envsys *, envsys_data_t *,
    sysmon_envsys_lim_t *, uint32_t *);

CFATTACH_DECL_NEW(ibmhawk, sizeof(struct ibmhawk_softc),
    ibmhawk_match, ibmhawk_attach, ibmhawk_detach, NULL);

static int
ibmhawk_match(device_t parent, cfdata_t match, void *aux)
{
	struct i2c_attach_args *ia = aux;
	ibmhawk_response_t resp;
	static struct ibmhawk_softc sc;

	sc.sc_tag = ia->ia_tag;
	sc.sc_addr = ia->ia_addr;
	if (ibmhawk_request(&sc, IHR_EQUIP, &resp))
		return 0;
	return 1;
}

static void
ibmhawk_attach(device_t parent, device_t self, void *aux)
{
	struct ibmhawk_softc *sc = device_private(self);
	struct i2c_attach_args *ia = aux;
	ibmhawk_response_t resp;
	int i;

	sc->sc_dev = self;
	sc->sc_tag = ia->ia_tag;
	sc->sc_addr = ia->ia_addr;

	if (!pmf_device_register(self, NULL, NULL))
		aprint_error_dev(self, "couldn't establish power handler\n");
	if (ibmhawk_request(sc, IHR_NAME, &resp)) {
		aprint_normal(": communication failed\n");
		return;
	}
	aprint_normal(": IBM Hawk \"%.16s\"\n", resp.ihr_name);
	if (ibmhawk_request(sc, IHR_EQUIP, &resp)) {
		aprint_error_dev(sc->sc_dev, "equip query failed\n");
		return;
	}
	sc->sc_numcpus = min(resp.ihr_numcpus, IBMHAWK_MAX_CPU);
	sc->sc_numfans = min(resp.ihr_numfans, IBMHAWK_MAX_FAN);
#if IBMHAWK_DEBUG > 0
	aprint_normal_dev(sc->sc_dev, "monitoring %d/%d cpu(s) %d/%d fan(s)\n",
	    sc->sc_numcpus, resp.ihr_numcpus, sc->sc_numfans, resp.ihr_numfans);
#endif
	/* Request and set sensor thresholds. */
	if (ibmhawk_request(sc, IHR_TEMP_THR, &resp)) {
		aprint_error_dev(sc->sc_dev, "temp threshold query failed\n");
		return;
	}
	for (i = 0; i < sc->sc_numcpus; i++)
		sc->sc_sensordata[IBMHAWK_T_CPU+i].ihs_warnmax =
		    resp.ihr_t_warn_thr;
	if (ibmhawk_request(sc, IHR_VOLT_THR, &resp)) {
		aprint_error_dev(sc->sc_dev, "volt threshold query failed\n");
		return;
	}
	for (i = 0; i < IBMHAWK_MAX_VOLTAGE; i++) {
		sc->sc_sensordata[IBMHAWK_V_VOLTAGE+i].ihs_warnmax =
		    bswap16(resp.ihr_v_voltage_thr[i*2]);
		sc->sc_sensordata[IBMHAWK_V_VOLTAGE+i].ihs_warnmin =
		    bswap16(resp.ihr_v_voltage_thr[i*2+1]);
	}
	if ((sc->sc_sme = sysmon_envsys_create()) == NULL) {
		aprint_error_dev(sc->sc_dev, "sysmon_envsys_create failed\n");
		return;
	}
	ibmhawk_refreshall(sc, true);
	sc->sc_sme->sme_name = device_xname(sc->sc_dev);
	sc->sc_sme->sme_cookie = sc;
	sc->sc_sme->sme_refresh = ibmhawk_refresh;
	sc->sc_sme->sme_get_limits = ibmhawk_get_limits;
	if (sysmon_envsys_register(sc->sc_sme)) {
		aprint_error_dev(sc->sc_dev, "sysmon_envsys_register failed\n");
		sysmon_envsys_destroy(sc->sc_sme);
		sc->sc_sme = NULL;
		return;
	}
}

static int
ibmhawk_detach(device_t self, int flags)
{
	struct ibmhawk_softc *sc = device_private(self);

	if (sc->sc_sme)
		sysmon_envsys_destroy(sc->sc_sme);
	return 0;
}


/*
 * Compute the message checksum.
 */
static uint8_t
ibmhawk_cksum(uint8_t *buf)
{
	int len = *buf++;
	int s = 0;

	while (--len > 0)
		s += *buf++;
	return -s;
}

/*
 * Request information from the management processor.
 * The response will be zeroed on error.
 * Request and response have the form <n> <data 0:n-2> <checksum>.
 */
static int
ibmhawk_request(struct ibmhawk_softc *sc, uint8_t request,
    ibmhawk_response_t *response)
{
	int i, error, retries;;
	uint8_t buf[sizeof(ibmhawk_response_t)+3], dummy;

	error = EIO;	/* Fail until we have a valid response. */
	retries = 0;

	if (iic_acquire_bus(sc->sc_tag, 0))
		return error;

again:
	memset(response, 0, sizeof(*response));

	/* Build and send the request. */
	buf[0] = 2;
	buf[1] = request;
	buf[2] = ibmhawk_cksum(buf);
#if IBMHAWK_DEBUG > 1
	printf("[");
	for (i = 0; i < 3; i++)
		printf(" %02x", buf[i]);
	printf(" ]");
#endif
	for (i = 0; i < 3; i++)
		if (iic_smbus_send_byte(sc->sc_tag, sc->sc_addr, buf[i], 0))
			goto bad;

	/* Receive and check the response. */
#if IBMHAWK_DEBUG > 1
	printf(" => [");
#endif
	if (iic_smbus_receive_byte(sc->sc_tag, sc->sc_addr, &buf[0], 0))
		goto bad;
	if (buf[0] == 0 || buf[0] == 255)
		goto bad;
	for (i = 1; i < buf[0]+1; i++)
		if (iic_smbus_receive_byte(sc->sc_tag, sc->sc_addr,
		    (i < sizeof buf ? &buf[i] : &dummy), 0))
			goto bad;
	if (buf[0] >= sizeof(buf) || buf[1] != request ||
	    ibmhawk_cksum(buf) != buf[buf[0]])
		goto bad;
	if (buf[0] > 2)
		memcpy(response, buf+2, buf[0]-2);
	error = 0;

bad:
#if IBMHAWK_DEBUG > 1
	for (i = 0; i < min(buf[0]+1, sizeof buf); i++)
		printf(" %02x", buf[i]);
	printf(" ] => %d\n", error);
#endif
	if (error != 0 && retries++ < 3)
		goto again;

	iic_release_bus(sc->sc_tag, 0);
	return error;
}

static uint32_t
ibmhawk_normalize(int value, uint32_t units)
{

	if (value == 0)
		return 0;

	switch (units) {
	case ENVSYS_STEMP:
		return 273150000+1000000*value;
	case ENVSYS_SVOLTS_DC:
		return 10000*value;
	default:
		return value;
	}
}

static void
ibmhawk_set(struct ibmhawk_softc *sc,
    int offset, int value, bool valid, bool create)
{
	int i;
	struct ibmhawk_sensordesc *sp;
	struct ibmhawk_sensordata *sd;
	envsys_data_t *dp;

	sd = &sc->sc_sensordata[offset];
	dp = &sd->ihs_edata;
	sp = NULL;
	if (create) {
		for (i = 0; i < ibmhawk_num_sensors; i++)
			if (ibmhawk_sensors[i].offset == offset) {
				sp = ibmhawk_sensors+i;
				break;
			}
		if (sp == NULL) {
#if IBMHAWK_DEBUG > 0
			aprint_error_dev(sc->sc_dev,
			    "offset %d: no sensor found\n", offset);
#endif
			return;
		}
		strlcpy(dp->desc, sp->desc, sizeof(dp->desc));
		dp->units = sp->units;
		if (sd->ihs_warnmin != 0 || sd->ihs_warnmax != 0) {
			sd->ihs_warnmin =
			    ibmhawk_normalize(sd->ihs_warnmin, dp->units);
			sd->ihs_warnmax =
			    ibmhawk_normalize(sd->ihs_warnmax, dp->units);
			dp->flags |= ENVSYS_FMONLIMITS;
		}
	}

	if (valid) {
		dp->value_cur = ibmhawk_normalize(value, dp->units);
		dp->state = ENVSYS_SVALID;
	} else
		dp->state = ENVSYS_SINVALID;

	if (create) {
		if (sysmon_envsys_sensor_attach(sc->sc_sme, dp))
			aprint_error_dev(sc->sc_dev,
			    "failed to attach \"%s\"\n", dp->desc);
	}
}

static void
ibmhawk_refreshall(struct ibmhawk_softc *sc, bool create)
{
	int i;
	bool valid;
	ibmhawk_response_t resp;

	valid = (ibmhawk_request(sc, IHR_TEMP, &resp) == 0);
	ibmhawk_set(sc, IBMHAWK_T_AMBIENT, resp.ihr_t_ambient, valid, create);
	for (i = 0; i < sc->sc_numcpus; i++)
		ibmhawk_set(sc, IBMHAWK_T_CPU+i,
		    resp.ihr_t_cpu[i], valid, create);

	valid = (ibmhawk_request(sc, IHR_FANRPM, &resp) == 0);
	for (i = 0; i < sc->sc_numfans; i++)
		ibmhawk_set(sc, IBMHAWK_F_FAN+i,
		    bswap16(resp.ihr_fanrpm[i]), valid, create);

	valid = (ibmhawk_request(sc, IHR_VOLT, &resp) == 0);
	for (i = 0; i < IBMHAWK_MAX_VOLTAGE; i++)
		ibmhawk_set(sc, IBMHAWK_V_VOLTAGE+i,
		    bswap16(resp.ihr_v_voltage[i]), valid, create);
}

static void
ibmhawk_refresh(struct sysmon_envsys *sme, envsys_data_t *edata)
{
	struct ibmhawk_softc *sc = sme->sme_cookie;

	/* No more than two refreshes per second. */
	if (hardclock_ticks-sc->sc_refresh < hz/2)
		return;
#if IBMHAWK_DEBUG > 1
	aprint_normal_dev(sc->sc_dev, "refresh \"%s\" delta %d\n",
	    edata->desc, hardclock_ticks-sc->sc_refresh);
#endif
	sc->sc_refresh = hardclock_ticks;
	ibmhawk_refreshall(sc, false);
}

static void
ibmhawk_get_limits(struct sysmon_envsys *sme, envsys_data_t *edata,
    sysmon_envsys_lim_t *limits, uint32_t *props)
{
	struct ibmhawk_sensordata *sd = (struct ibmhawk_sensordata *)edata;

	if (sd->ihs_warnmin != 0) {
		limits->sel_warnmin = sd->ihs_warnmin;
		*props |= PROP_WARNMIN;
	}
	if (sd->ihs_warnmax != 0) {
		limits->sel_warnmax = sd->ihs_warnmax;
		*props |= PROP_WARNMAX;
	}
}
