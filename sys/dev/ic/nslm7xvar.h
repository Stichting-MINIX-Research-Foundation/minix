/*	$NetBSD: nslm7xvar.h,v 1.28 2012/01/17 16:14:47 jakllsch Exp $ */

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

#ifndef _DEV_ISA_NSLM7XVAR_H_
#define _DEV_ISA_NSLM7XVAR_H_

#include <dev/sysmon/sysmonvar.h>

/*
 * National Semiconductor LM78/79/81 registers.
 */

/* Control registers */

#define LMC_ADDR	0x05
#define LMC_DATA	0x06

/* Data registers */

#define LMD_POST_RAM	0x00	/* POST RAM occupies 0x00 -- 0x1f */
#define LMD_VALUE_RAM	0x20	/* Value RAM occupies 0x20 -- 0x3f */
#define LMD_FAN1	0x28	/* FAN1 reading */
#define LMD_FAN2	0x29	/* FAN2 reading */
#define LMD_FAN3	0x2a	/* FAN3 reading */

#define LMD_CONFIG	0x40	/* Configuration */
#define LMD_ISR1	0x41	/* Interrupt Status 1 */
#define LMD_ISR2	0x42	/* Interrupt Status 2 */
#define LMD_SMI1	0x43	/* SMI Mask 1 */
#define LMD_SMI2	0x44	/* SMI Mask 2 */
#define LMD_NMI1	0x45	/* NMI Mask 1 */
#define LMD_NMI2	0x46	/* NMI Mask 2 */
#define LMD_VIDFAN	0x47	/* VID/Fan Divisor */
#define LMD_SBUSADDR	0x48	/* Serial Bus Address */
#define LMD_CHIPID	0x49	/* Chip Reset/ID */

/* Chip IDs */

#define LM_NUM_SENSORS	11
#define LM_ID_LM78	0x00
#define LM_ID_LM78J	0x40
#define LM_ID_LM79	0xC0
#define LM_ID_LM81	0x80
#define LM_ID_MASK	0xFE


/*
 * Winbond registers
 *
 * Several models exists.  The W83781D is mostly compatible with the
 * LM78, but has two extra temperatures.  Later models add extra
 * voltage sensors, fans and bigger fan divisors to accomodate slow
 * running fans.  To accomodate the extra sensors some models have
 * different memory banks.
 */

#define WB_T23ADDR	0x4a	/* Temperature 2 and 3 Serial Bus Address */
#define WB_PIN		0x4b	/* Pin Control */
#define WB_BANKSEL	0x4e	/* Bank Select */
#define WB_VENDID	0x4f	/* Vendor ID */

/* Bank 0 regs */
#define WB_BANK0_CHIPID	0x58	/* Chip ID */
#define WB_BANK0_RESVD1	0x59	/* Resvd, bits 6-4 select temp sensor mode */
#define WB_BANK0_FAN45	0x5c	/* Fan 4/5 Divisor Control (W83791D only) */
#define WB_BANK0_VBAT	0x5d	/* VBAT Monitor Control */
#define WB_BANK0_FAN4	0xba	/* Fan 4 reading (W83791D only) */
#define WB_BANK0_FAN5	0xbb	/* Fan 5 reading (W83791D only) */

#define WB_BANK0_CONFIG	0x18	/* VRM & OVT Config (W83627THF/W83637HF) */

/* Bank 1 registers */
#define WB_BANK1_T2H	0x50	/* Temperature 2 High Byte */
#define WB_BANK1_T2L	0x51	/* Temperature 2 Low Byte */

/* Bank 2 registers */
#define WB_BANK2_T3H	0x50	/* Temperature 3 High Byte */
#define WB_BANK2_T3L	0x51	/* Temperature 3 Low Byte */

/* Bank 4 registers (W83782D/W83627HF and later models only) */
#define WB_BANK4_T1OFF	0x54	/* Temperature 1 Offset */
#define WB_BANK4_T2OFF	0x55	/* Temperature 2 Offset */
#define WB_BANK4_T3OFF	0x56	/* Temperature 3 Offset */

/* Bank 5 registers (W83782D/W83627HF and later models only) */
#define WB_BANK5_5VSB	0x50	/* 5VSB reading */
#define WB_BANK5_VBAT	0x51	/* VBAT reading */

/* Bank selection */
#define WB_BANKSEL_B0	0x00	/* Bank 0 */
#define WB_BANKSEL_B1	0x01	/* Bank 1 */
#define WB_BANKSEL_B2	0x02	/* Bank 2 */
#define WB_BANKSEL_B3	0x03	/* Bank 3 */
#define WB_BANKSEL_B4	0x04	/* Bank 4 */
#define WB_BANKSEL_B5	0x05	/* Bank 5 */
#define WB_BANKSEL_HBAC	0x80	/* Register 0x4f High Byte Access */

/* Vendor IDs */
#define WB_VENDID_WINBOND	0x5ca3	/* Winbond */
#define WB_VENDID_ASUS		0x12c3	/* ASUS */

/* Chip IDs */
#define WB_CHIPID_W83781D	0x10
#define WB_CHIPID_W83781D_2	0x11
#define WB_CHIPID_W83627HF	0x21
#define WB_CHIPID_AS99127F	0x31	/* Asus W83781D clone */
#define WB_CHIPID_W83782D	0x30
#define WB_CHIPID_W83783S	0x40
#define WB_CHIPID_W83697HF	0x60
#define WB_CHIPID_W83791D	0x71
#define WB_CHIPID_W83791SD	0x72
#define WB_CHIPID_W83792D	0x7a
#define WB_CHIPID_W83637HF	0x80
#define WB_CHIPID_W83627EHF_A	0x88 /* early version, only for ASUS MBs */
#define WB_CHIPID_W83627THF	0x90
#define WB_CHIPID_W83627EHF	0xa1
#define WB_CHIPID_W83627DHG	0xc1

/* Config bits */
#define WB_CONFIG_VMR9		0x01

/* Reference voltage (mV) */
#define WB_VREF			3600
#define WB_W83627EHF_VREF	2048

#define WB_MAX_SENSORS		19

struct lm_softc {
	device_t sc_dev;

	callout_t sc_callout;

	envsys_data_t sensors[WB_MAX_SENSORS];
	struct sysmon_envsys *sc_sme;
	uint8_t numsensors;

	void (*refresh_sensor_data)(struct lm_softc *);

	uint8_t (*lm_readreg)(struct lm_softc *, int);
	void (*lm_writereg)(struct lm_softc *, int, int);

	struct lm_sensor *lm_sensors;
	uint8_t	chipid;
	uint8_t	vrm9;
};

struct lm_sensor {
	const char *desc;
	enum envsys_units type;
	uint8_t bank;
	uint8_t reg;
	void (*refresh)(struct lm_softc *, int);
	int rfact;
};

void 	lm_attach(struct lm_softc *);
void	lm_detach(struct lm_softc *);
int 	lm_probe(struct lm_softc *);

#endif /* _DEV_ISA_NSLM7XVAR_H_ */
