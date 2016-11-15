/*	$NetBSD: smscvar.h,v 1.5 2012/10/27 17:18:25 chs Exp $ */

/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Bill Squier.
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

#ifndef _DEV_SMSC47B397VAR_H_
#define _DEV_SMSC47B397VAR_H_

/*
 * SMSC LPC47B397-NC "super-io" chip
 */

#define SMSC_ADDR		0
#define SMSC_DATA		1

/* Chip control registers */

#define SMSC_LOGICAL_DEV_SEL	0x07	/* Selector for logical device */
#define SMSC_DEVICE_ID		0x20	/* Device ID register */
#define SMSC_DEVICE_REVISION	0x21	/* Device revision */
#define SMSC_IO_BASE_MSB	0x60
#define SMSC_IO_BASE_LSB	0x61


#define SMSC_LOGICAL_DEVICE	0x08	/* Magic number to select monitoring
					   functions. */
#define SMSC_CONFIG_START	0x55	/* Start configuration mode */
#define SMSC_CONFIG_END		0xAA	/* End configuration mode */

#define SMSC_ID_47B397		0x6F	/* Chip ID */
#define SMSC_ID_SCH5307NS	0x81
#define SMSC_ID_SCH5317		0x85

/* Data registers */
#define SMSC_TEMP1		0x25
#define SMSC_TEMP2		0x26
#define SMSC_TEMP3		0x27
#define SMSC_TEMP4		0x80

/* NOTE: Reading the Fan LSB locks the Fan MSB. The LSB Must be read first. */
#define SMSC_FAN1_LSB		0x28
#define SMSC_FAN1_MSB		0x29
#define SMSC_FAN2_LSB		0x2A
#define SMSC_FAN2_MSB		0x2B
#define SMSC_FAN3_LSB		0x2C
#define SMSC_FAN3_MSB		0x2D
#define SMSC_FAN4_LSB		0x2E
#define SMSC_FAN4_MSB		0x2F

#define SMSC_MAX_SENSORS	8	/* 4 temp sensors, 4 fan sensors */

struct smsc_softc {
	bus_space_tag_t 	sc_iot;
	bus_space_handle_t 	sc_ioh;

	int 			sc_flags;
	struct sysmon_envsys 	*sc_sme;
	envsys_data_t 		sc_sensor[SMSC_MAX_SENSORS];

	uint8_t 		sc_regs[SMSC_MAX_SENSORS];
};

#endif /* _DEV_SMSC47B397VAR_H_ */
