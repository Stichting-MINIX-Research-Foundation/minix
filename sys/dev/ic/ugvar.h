/* $NetBSD: ugvar.h,v 1.4 2008/03/26 16:09:37 xtraeme Exp $ */

/*
 * Copyright (c) 2007 Mihai Chelaru <kefren@netbsd.ro>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _UGVAR_H_
#define _UGVAR_H_

struct ug_softc {
	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_ioh;

	struct sysmon_envsys *sc_sme;
	envsys_data_t sc_sensor[UG_MAX_SENSORS];
	uint8_t version;
	void *mbsens;
};

struct ug2_sensor_info {
	const char *name;
	int port;
	int type;
	int multiplier;
	int divisor;
	int offset;
};

struct ug2_motherboard_info {
	uint16_t id;
	const char *name;
	struct ug2_sensor_info sensors[UG2_MAX_NO_SENSORS + 1];
};

/* driver internal functions */
int ug_reset(struct ug_softc *);
uint8_t ug_read(struct ug_softc *, unsigned short);
int ug_waitfor(struct ug_softc *, uint16_t, uint8_t);
void ug_setup_sensors(struct ug_softc *);
void ug2_attach(device_t);
int ug2_wait_ready(struct ug_softc *);
int ug2_wait_readable(struct ug_softc *);
int ug2_sync(struct ug_softc *);
int ug2_read(struct ug_softc *, uint8_t, uint8_t, uint8_t, uint8_t*);

/* Envsys */
void ug_refresh(struct sysmon_envsys *, envsys_data_t *);
void ug2_refresh(struct sysmon_envsys *, envsys_data_t *);

#endif		/* _UGVAR_H_ */
