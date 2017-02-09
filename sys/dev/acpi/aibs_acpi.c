/* $NetBSD: aibs_acpi.c,v 1.5 2015/04/23 23:23:00 pgoyette Exp $ */

/*-
 * Copyright (c) 2011 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jukka Ruohonen.
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

/*	$OpenBSD: atk0110.c,v 1.1 2009/07/23 01:38:16 cnst Exp $	*/
/*
 * Copyright (c) 2009 Constantine A. Murenin <cnst+netbsd@bugmail.mojo.ru>
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: aibs_acpi.c,v 1.5 2015/04/23 23:23:00 pgoyette Exp $");

#include <sys/param.h>
#include <sys/kmem.h>
#include <sys/module.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>

/*
 * ASUSTeK AI Booster (ACPI ASOC ATK0110).
 *
 * This code was originally written for OpenBSD after the techniques
 * described in the Linux's asus_atk0110.c and FreeBSD's acpi_aiboost.c
 * were verified to be accurate on the actual hardware kindly provided by
 * Sam Fourman Jr.  It was subsequently ported from OpenBSD to DragonFly BSD,
 * and then to the NetBSD's sysmon_envsys(9) framework.
 *
 *				  -- Constantine A. Murenin <http://cnst.su/>
 */

#define _COMPONENT		 ACPI_RESOURCE_COMPONENT
ACPI_MODULE_NAME		 ("acpi_aibs")

#define AIBS_MUX_HWMON		 0x00000006
#define AIBS_MUX_MGMT		 0x00000011

#define AIBS_TYPE(x)		 (((x) >> 16) & 0xff)
#define AIBS_TYPE_VOLT		 2
#define AIBS_TYPE_TEMP		 3
#define AIBS_TYPE_FAN		 4

struct aibs_sensor {
	envsys_data_t			 as_sensor;
	uint64_t			 as_type;
	uint64_t			 as_liml;
	uint64_t			 as_limh;

	SIMPLEQ_ENTRY(aibs_sensor)	 as_list;
};

struct aibs_softc {
	device_t			 sc_dev;
	struct acpi_devnode		*sc_node;
	struct sysmon_envsys		*sc_sme;
	bool				 sc_model;	/* new model = true */

	SIMPLEQ_HEAD(, aibs_sensor)	 as_head;
};

static int	aibs_match(device_t, cfdata_t, void *);
static void	aibs_attach(device_t, device_t, void *);
static int	aibs_detach(device_t, int);

static void	aibs_init(device_t);
static void	aibs_init_new(device_t);
static void	aibs_init_old(device_t, int);

static void	aibs_sensor_add(device_t, ACPI_OBJECT *);
static bool	aibs_sensor_value(device_t, struct aibs_sensor *, uint64_t *);
static void	aibs_sensor_refresh(struct sysmon_envsys *, envsys_data_t *);
static void	aibs_sensor_limits(struct sysmon_envsys *, envsys_data_t *,
				   sysmon_envsys_lim_t *, uint32_t *);

CFATTACH_DECL_NEW(aibs, sizeof(struct aibs_softc),
    aibs_match, aibs_attach, aibs_detach, NULL);

static const char* const aibs_hid[] = {
	"ATK0110",
	NULL
};

static int
aibs_match(device_t parent, cfdata_t match, void *aux)
{
	struct acpi_attach_args *aa = aux;

	if (aa->aa_node->ad_type != ACPI_TYPE_DEVICE)
		return 0;

	return acpi_match_hid(aa->aa_node->ad_devinfo, aibs_hid);
}

static void
aibs_attach(device_t parent, device_t self, void *aux)
{
	struct aibs_softc *sc = device_private(self);
	struct acpi_attach_args *aa = aux;

	sc->sc_dev = self;
	sc->sc_node = aa->aa_node;

	aprint_naive("\n");
	aprint_normal(": ASUSTeK AI Booster\n");

	sc->sc_sme = sysmon_envsys_create();

	sc->sc_sme->sme_cookie = sc;
	sc->sc_sme->sme_name = device_xname(self);
	sc->sc_sme->sme_refresh = aibs_sensor_refresh;
	sc->sc_sme->sme_get_limits = aibs_sensor_limits;

	aibs_init(self);
	SIMPLEQ_INIT(&sc->as_head);

	if (sc->sc_model != false)
		aibs_init_new(self);
	else {
		aibs_init_old(self, AIBS_TYPE_FAN);
		aibs_init_old(self, AIBS_TYPE_TEMP);
		aibs_init_old(self, AIBS_TYPE_VOLT);
	}

	(void)pmf_device_register(self, NULL, NULL);

	if (sc->sc_sme->sme_nsensors == 0) {
		aprint_error_dev(self, "no sensors found\n");
		sysmon_envsys_destroy(sc->sc_sme);
		sc->sc_sme = NULL;
		return;
	}

	if (sysmon_envsys_register(sc->sc_sme) != 0)
		aprint_error_dev(self, "failed to register with sysmon\n");
}

static int
aibs_detach(device_t self, int flags)
{
	struct aibs_softc *sc = device_private(self);
	struct aibs_sensor *as;

	pmf_device_deregister(self);

	if (sc->sc_sme != NULL)
		sysmon_envsys_unregister(sc->sc_sme);

	while (SIMPLEQ_FIRST(&sc->as_head) != NULL) {
		as = SIMPLEQ_FIRST(&sc->as_head);
		SIMPLEQ_REMOVE_HEAD(&sc->as_head, as_list);
		kmem_free(as, sizeof(*as));
	}

	return 0;
}

static void
aibs_init(device_t self)
{
	struct aibs_softc *sc = device_private(self);
	ACPI_HANDLE tmp;
	ACPI_STATUS rv;

	/*
	 * Old model uses the tuple { TSIF, VSIF, FSIF } to
	 * enumerate the sensors and { RTMP, RVLT, RFAN }
	 * to obtain the values. New mode uses GGRP for the
	 * enumeration and { GITM, SITM } as accessors.
	 */
	rv = AcpiGetHandle(sc->sc_node->ad_handle, "GGRP", &tmp);

	if (ACPI_FAILURE(rv)) {
		sc->sc_model = false;
		return;
	}

	rv = AcpiGetHandle(sc->sc_node->ad_handle, "GITM", &tmp);

	if (ACPI_FAILURE(rv)) {
		sc->sc_model = false;
		return;
	}

	rv = AcpiGetHandle(sc->sc_node->ad_handle, "SITM", &tmp);

	if (ACPI_FAILURE(rv)) {
		sc->sc_model = false;
		return;
	}

	sc->sc_model = true;

	/*
	 * If both the new and the old methods are present, prefer
	 * the old one; GGRP/GITM may not be functional in this case.
	 */
	rv = AcpiGetHandle(sc->sc_node->ad_handle, "FSIF", &tmp);

	if (ACPI_FAILURE(rv))
		return;

	rv = AcpiGetHandle(sc->sc_node->ad_handle, "TSIF", &tmp);

	if (ACPI_FAILURE(rv))
		return;

	rv = AcpiGetHandle(sc->sc_node->ad_handle, "VSIF", &tmp);

	if (ACPI_FAILURE(rv))
		return;

	rv = AcpiGetHandle(sc->sc_node->ad_handle, "RFAN", &tmp);

	if (ACPI_FAILURE(rv))
		return;

	rv = AcpiGetHandle(sc->sc_node->ad_handle, "RTMP", &tmp);

	if (ACPI_FAILURE(rv))
		return;

	rv = AcpiGetHandle(sc->sc_node->ad_handle, "RVLT", &tmp);

	if (ACPI_FAILURE(rv))
		return;

	sc->sc_model = false;
}

static void
aibs_init_new(device_t self)
{
	struct aibs_softc *sc = device_private(self);
	ACPI_OBJECT_LIST arg;
	ACPI_OBJECT id, *obj;
	ACPI_BUFFER buf;
	ACPI_STATUS rv;
	uint32_t i, n;

	arg.Count = 1;
	arg.Pointer = &id;

	id.Type = ACPI_TYPE_INTEGER;
	id.Integer.Value = AIBS_MUX_HWMON;

	buf.Pointer = NULL;
	buf.Length = ACPI_ALLOCATE_LOCAL_BUFFER;

	rv = AcpiEvaluateObject(sc->sc_node->ad_handle, "GGRP", &arg, &buf);

	if (ACPI_FAILURE(rv))
		goto out;

	obj = buf.Pointer;

	if (obj->Type != ACPI_TYPE_PACKAGE) {
		rv = AE_TYPE;
		goto out;
	}

	if (obj->Package.Count > UINT32_MAX) {
		rv = AE_AML_NUMERIC_OVERFLOW;
		goto out;
	}

	n = obj->Package.Count;

	if (n == 0) {
		rv = AE_NOT_EXIST;
		goto out;
	}

	for (i = 0; i < n; i++)
		aibs_sensor_add(self, &obj->Package.Elements[i]);

out:
	if (buf.Pointer != NULL)
		ACPI_FREE(buf.Pointer);

	if (ACPI_FAILURE(rv)) {

		aprint_error_dev(self, "failed to evaluate "
		    "GGRP: %s\n", AcpiFormatException(rv));
	}
}

static void
aibs_init_old(device_t self, int type)
{
	struct aibs_softc *sc = device_private(self);
	char path[] = "?SIF";
	ACPI_OBJECT *elm, *obj;
	ACPI_BUFFER buf;
	ACPI_STATUS rv;
	uint32_t i, n;

	switch (type) {

	case AIBS_TYPE_FAN:
		path[0] = 'F';
		break;

	case AIBS_TYPE_TEMP:
		path[0] = 'T';
		break;

	case AIBS_TYPE_VOLT:
		path[0] = 'V';
		break;

	default:
		return;
	}

	rv = acpi_eval_struct(sc->sc_node->ad_handle, path, &buf);

	if (ACPI_FAILURE(rv))
		goto out;

	obj = buf.Pointer;

	if (obj->Type != ACPI_TYPE_PACKAGE) {
		rv = AE_TYPE;
		goto out;
	}

	elm = obj->Package.Elements;

	if (elm[0].Type != ACPI_TYPE_INTEGER) {
		rv = AE_TYPE;
		goto out;
	}

	if (elm[0].Integer.Value > UINT32_MAX) {
		rv = AE_AML_NUMERIC_OVERFLOW;
		goto out;
	}

	n = elm[0].Integer.Value;

	if (n == 0) {
		rv = AE_NOT_EXIST;
		goto out;
	}

	if (obj->Package.Count - 1 != n) {
		rv = AE_BAD_VALUE;
		goto out;
	}

	for (i = 1; i < obj->Package.Count; i++) {

		if (elm[i].Type != ACPI_TYPE_PACKAGE)
			continue;

		aibs_sensor_add(self, &elm[i]);
	}

out:
	if (buf.Pointer != NULL)
		ACPI_FREE(buf.Pointer);

	if (ACPI_FAILURE(rv)) {

		aprint_error_dev(self, "failed to evaluate "
		    "%s: %s\n", path, AcpiFormatException(rv));
	}
}

static void
aibs_sensor_add(device_t self, ACPI_OBJECT *obj)
{
	struct aibs_softc *sc = device_private(self);
	struct aibs_sensor *as;
	int ena, len, lhi, llo;
	const char *name;
	ACPI_STATUS rv;

	as = NULL;
	rv = AE_OK;

	if (obj->Type != ACPI_TYPE_PACKAGE) {
		rv = AE_TYPE;
		goto out;
	}

	/*
	 * The known formats are:
	 *
	 *	index		type		old		new
	 *	-----		----		---		---
	 *	0		integer		flags		flags
	 *	1		string		name		name
	 *	2		integer		limit1		unknown
	 *	3		integer		limit2		unknown
	 *	4		integer		enable		limit1
	 *	5		integer		-		limit2
	 *	6		integer		-		enable
	 */
	if (sc->sc_model != false) {
		len = 7;
		llo = 4;
		lhi = 5;
		ena = 6;
	} else {
		len = 5;
		llo = 2;
		lhi = 3;
		ena = 4;
	}

	if (obj->Package.Count != (uint32_t)len) {
		rv = AE_LIMIT;
		goto out;
	}

	if (obj->Package.Elements[0].Type != ACPI_TYPE_INTEGER ||
	    obj->Package.Elements[1].Type != ACPI_TYPE_STRING ||
	    obj->Package.Elements[llo].Type != ACPI_TYPE_INTEGER ||
	    obj->Package.Elements[lhi].Type != ACPI_TYPE_INTEGER ||
	    obj->Package.Elements[ena].Type != ACPI_TYPE_INTEGER) {
		rv = AE_TYPE;
		goto out;
	}

	as = kmem_zalloc(sizeof(*as), KM_SLEEP);

	if (as == NULL) {
		rv = AE_NO_MEMORY;
		goto out;
	}

	name = obj->Package.Elements[1].String.Pointer;

	as->as_type = obj->Package.Elements[0].Integer.Value;
	as->as_liml = obj->Package.Elements[llo].Integer.Value;
	as->as_limh = obj->Package.Elements[lhi].Integer.Value;

	if (sc->sc_model != false)
		as->as_limh += as->as_liml;	/* A range in the new model. */

	as->as_sensor.state = ENVSYS_SINVALID;

	switch (AIBS_TYPE(as->as_type)) {

	case AIBS_TYPE_FAN:
		as->as_sensor.units = ENVSYS_SFANRPM;
		as->as_sensor.flags = ENVSYS_FMONLIMITS | ENVSYS_FHAS_ENTROPY;
		break;

	case AIBS_TYPE_TEMP:
		as->as_sensor.units = ENVSYS_STEMP;
		as->as_sensor.flags = ENVSYS_FMONLIMITS | ENVSYS_FHAS_ENTROPY;
		break;

	case AIBS_TYPE_VOLT:
		as->as_sensor.units = ENVSYS_SVOLTS_DC;
		as->as_sensor.flags = ENVSYS_FMONLIMITS | ENVSYS_FHAS_ENTROPY;
		break;

	default:
		rv = AE_TYPE;
		goto out;
	}

	(void)strlcpy(as->as_sensor.desc, name, sizeof(as->as_sensor.desc));

	if (sysmon_envsys_sensor_attach(sc->sc_sme, &as->as_sensor) != 0) {
		rv = AE_AML_INTERNAL;
		goto out;
	}

	SIMPLEQ_INSERT_TAIL(&sc->as_head, as, as_list);

out:
	if (ACPI_FAILURE(rv)) {

		if (as != NULL)
			kmem_free(as, sizeof(*as));

		aprint_error_dev(self, "failed to add "
		    "sensor: %s\n",  AcpiFormatException(rv));
	}
}

static bool
aibs_sensor_value(device_t self, struct aibs_sensor *as, uint64_t *val)
{
	struct aibs_softc *sc = device_private(self);
	uint32_t type, *ret, cmb[3];
	ACPI_OBJECT_LIST arg;
	ACPI_OBJECT cmi, tmp;
	ACPI_OBJECT *obj;
	ACPI_BUFFER buf;
	ACPI_STATUS rv;
	const char *path;

	if (sc->sc_model != false) {

		path = "GITM";

		cmb[0] = as->as_type;
		cmb[1] = 0;
		cmb[2] = 0;

		arg.Count = 1;
		arg.Pointer = &tmp;

		tmp.Buffer.Length = sizeof(cmb);
		tmp.Buffer.Pointer = (uint8_t *)cmb;
		tmp.Type = type = ACPI_TYPE_BUFFER;

	} else {

		arg.Count = 1;
		arg.Pointer = &cmi;

		cmi.Integer.Value = as->as_type;
		cmi.Type = type = ACPI_TYPE_INTEGER;

		switch (AIBS_TYPE(as->as_type)) {

		case AIBS_TYPE_FAN:
			path = "RFAN";
			break;

		case AIBS_TYPE_TEMP:
			path = "RTMP";
			break;

		case AIBS_TYPE_VOLT:
			path = "RVLT";
			break;

		default:
			return false;
		}
	}

	buf.Pointer = NULL;
	buf.Length = ACPI_ALLOCATE_LOCAL_BUFFER;

	rv = AcpiEvaluateObject(sc->sc_node->ad_handle, path, &arg, &buf);

	if (ACPI_FAILURE(rv))
		goto out;

	obj = buf.Pointer;

	if (obj->Type != type) {
		rv = AE_TYPE;
		goto out;
	}

	if (sc->sc_model != true)
		*val = obj->Integer.Value;
	else {
		/*
		 * The return buffer contains at least:
		 *
		 *	uint32_t buf[0]	 flags
		 *	uint32_t buf[1]	 return value
		 *	uint8_t  buf[2-] unknown
		 */
		if (obj->Buffer.Length < 8) {
			rv = AE_BUFFER_OVERFLOW;
			goto out;
		}

		ret = (uint32_t *)obj->Buffer.Pointer;

		if (ret[0] == 0) {
			rv = AE_BAD_VALUE;
			goto out;
		}

		*val = ret[1];
	}

out:
	if (buf.Pointer != NULL)
		ACPI_FREE(buf.Pointer);

	if (ACPI_FAILURE(rv)) {

		aprint_error_dev(self, "failed to evaluate "
		    "%s: %s\n", path, AcpiFormatException(rv));

		return false;
	}

	return true;
}

static void
aibs_sensor_refresh(struct sysmon_envsys *sme, envsys_data_t *edata)
{
	struct aibs_softc *sc = sme->sme_cookie;
	struct aibs_sensor *tmp, *as = NULL;
	envsys_data_t *s = edata;
	uint64_t val = 0;

	SIMPLEQ_FOREACH(tmp, &sc->as_head, as_list) {

		if (tmp->as_sensor.sensor == s->sensor) {
			as = tmp;
			break;
		}
	}

	if (as == NULL) {
		aprint_debug_dev(sc->sc_dev, "failed to find sensor\n");
		return;
	}

	as->as_sensor.state = ENVSYS_SINVALID;
	as->as_sensor.flags |= ENVSYS_FMONNOTSUPP;

	if (aibs_sensor_value(sc->sc_dev, as, &val) != true)
		return;

	switch (as->as_sensor.units) {

	case ENVSYS_SFANRPM:
		as->as_sensor.value_cur = val;
		break;

	case ENVSYS_STEMP:

		if (val == 0)
			return;

		as->as_sensor.value_cur = val * 100 * 1000 + 273150000;
		break;

	case ENVSYS_SVOLTS_DC:
		as->as_sensor.value_cur = val * 1000;
		break;

	default:
		return;
	}

	as->as_sensor.state = ENVSYS_SVALID;
	as->as_sensor.flags &= ~ENVSYS_FMONNOTSUPP;
}

static void
aibs_sensor_limits(struct sysmon_envsys *sme, envsys_data_t *edata,
    sysmon_envsys_lim_t *limits, uint32_t *props)
{
	struct aibs_softc *sc = sme->sme_cookie;
	struct aibs_sensor *tmp, *as = NULL;
	sysmon_envsys_lim_t *lim = limits;
	envsys_data_t *s = edata;

	SIMPLEQ_FOREACH(tmp, &sc->as_head, as_list) {

		if (tmp->as_sensor.sensor == s->sensor) {
			as = tmp;
			break;
		}
	}

	if (as == NULL) {
		aprint_debug_dev(sc->sc_dev, "failed to find sensor\n");
		return;
	}

	switch (as->as_sensor.units) {

	case ENVSYS_SFANRPM:

		/*
		 * Some boards have strange limits for fans.
		 */
		if (as->as_liml == 0) {
			lim->sel_warnmin = as->as_limh;
			*props = PROP_WARNMIN;

		} else {
			lim->sel_warnmin = as->as_liml;
			lim->sel_warnmax = as->as_limh;
			*props = PROP_WARNMIN | PROP_WARNMAX;
		}

		break;

	case ENVSYS_STEMP:
		lim->sel_critmax = as->as_limh * 100 * 1000 + 273150000;
		lim->sel_warnmax = as->as_liml * 100 * 1000 + 273150000;

		*props = PROP_CRITMAX | PROP_WARNMAX;
		break;

	case ENVSYS_SVOLTS_DC:
		lim->sel_critmin = as->as_liml * 1000;
		lim->sel_critmax = as->as_limh * 1000;
		*props = PROP_CRITMIN | PROP_CRITMAX;
		break;

	default:
		return;
	}
}

MODULE(MODULE_CLASS_DRIVER, aibs, "sysmon_envsys");

#ifdef _MODULE
#include "ioconf.c"
#endif

static int
aibs_modcmd(modcmd_t cmd, void *aux)
{
	int rv = 0;

	switch (cmd) {

	case MODULE_CMD_INIT:

#ifdef _MODULE
		rv = config_init_component(cfdriver_ioconf_aibs,
		    cfattach_ioconf_aibs, cfdata_ioconf_aibs);
#endif
		break;

	case MODULE_CMD_FINI:

#ifdef _MODULE
		rv = config_fini_component(cfdriver_ioconf_aibs,
		    cfattach_ioconf_aibs, cfdata_ioconf_aibs);
#endif
		break;

	default:
		rv = ENOTTY;
	}

	return rv;
}
