/*	$NetBSD: itesio_isavar.h,v 1.9 2012/11/15 04:45:01 msaitoh Exp $	*/
/*	$OpenBSD: itvar.h,v 1.2 2003/11/05 20:57:10 grange Exp $	*/

/*
 * Copyright (c) 2006-2007 Juan Romero Pardines <xtraeme@netbsd.org>
 * Copyright (c) 2003 Julien Bordet <zejames@greyhats.org>
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
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITD TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITD TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _DEV_ISA_ITESIO_ISAVAR_H
#define _DEV_ISA_ITESIO_ISAVAR_H

#define IT_NUM_SENSORS	15

/* Super I/O Configuration Registers */
#define ITESIO_ADDR	0	/* Address Port = 0x2e */
#define ITESIO_DATA	1	/* Data Port = 0x2f */

#define ITESIO_LDNSEL	0x07	/* Logical Device Number Selection */

#define ITESIO_EC_LDN	0x04	/* EC Logical Device Number */
#define ITESIO_EC_MSB	0x60	/* EC Base Address (MSB) */
#define ITESIO_EC_LSB	0x61	/* EC Base Address (LSB) */

#define ITESIO_WDT_LDN	0x07	/* Watchdog Logical Device Number */
#define ITESIO_WDT_CTL	0x71	/* Watchdog Control Register */
#define ITESIO_WDT_CNF	0x72	/* Watchdog Configuration Register */
#define   ITESIO_WDT_CNF_SECS	(1<<7)	/* Use seconds for the timer */
#define   ITESIO_WDT_CNF_KRST	(1<<6)	/* Enable KRST */
#define   ITESIO_WDT_CNF_PWROK	(1<<4)	/* Enable PWROK */
#define ITESIO_WDT_TMO_LSB	0x73	/* Watchdog Timeout Value LSB */
#define ITESIO_WDT_TMO_MSB	0x74	/* Watchdog Timeout Value MSB */

#define ITESIO_WDT_MAXTIMO	0xffff	/* either seconds or minutes */

#define ITESIO_CHIPID1	0x20	/* Chip ID 1 */
#define ITESIO_CHIPID2	0x21	/* Chip ID 2 */
#define ITESIO_DEVREV	0x22	/* Device Revision */

#define ITESIO_ID8705	0x8705
#define ITESIO_ID8712	0x8712
#define ITESIO_ID8716	0x8716
#define ITESIO_ID8718	0x8718
#define ITESIO_ID8720	0x8720
#define ITESIO_ID8721	0x8721
#define ITESIO_ID8726	0x8726

/* 
 * Control registers for the Environmental Controller, relative
 * to the Base Address Register.
 */
#define ITESIO_EC_ADDR 	0x05
#define ITESIO_EC_DATA 	0x06

/* Data registers */
#define ITESIO_EC_CONFIG 	0x00
#define ITESIO_EC_ISR1 		0x01
#define ITESIO_EC_ISR2 		0x02
#define ITESIO_EC_ISR3 		0x03
#define ITESIO_EC_SMI1 		0x04
#define ITESIO_EC_SMI2 		0x05
#define ITESIO_EC_SMI3 		0x06
#define ITESIO_EC_IMR1 		0x07
#define ITESIO_EC_IMR2 		0x08
#define ITESIO_EC_IMR3 		0x09
#define ITESIO_EC_VID 		0x0a
#define ITESIO_EC_FAN_TDR 	0x0b	/* Fan Tachometer Divisor Register */
#define ITESIO_EC_FAN16_CER 	0x0c	/* Fan Tachometer 16-bit Counter Enable Register */

#define ITESIO_EC_VOLTENABLE 	0x50
#define ITESIO_EC_TEMPENABLE 	0x51

#define ITESIO_EC_FANMINBASE	0x10
#define ITESIO_EC_FANENABLE	0x13

#define ITESIO_EC_SENSORFANBASE 	0x0d	/* Fan from 0x0d to 0x0f */
#define ITESIO_EC_SENSORFANEXTBASE 	0x18 	/* Fan (MSB) from 0x18 to 0x1A */
#define ITESIO_EC_SENSORVOLTBASE 	0x20 	/* Voltage from 0x20 to 0x28 */
#define ITESIO_EC_SENSORTEMPBASE 	0x29 	/* Temperature from 0x29 to 0x2b */

#define ITESIO_EC_VIN0 	0x20
#define ITESIO_EC_VIN1 	0x21
#define ITESIO_EC_VIN2 	0x22
#define ITESIO_EC_VIN3 	0x23
#define ITESIO_EC_VIN4 	0x24
#define ITESIO_EC_VIN5 	0x25
#define ITESIO_EC_VIN6 	0x26
#define ITESIO_EC_VIN7 	0x27
#define ITESIO_EC_VBAT 	0x28

#define ITESIO_EC_UPDATEVBAT 	0x40 	/* Update VBAT voltage reading */
#define ITESIO_EC_BEEPEER 	0x5c	/* Beep Event Enable Register */

/* High and Low limits for voltages */
#define ITESIO_EC_VIN0_HIGH_LIMIT 	0x30
#define ITESIO_EC_VIN0_LOW_LIMIT 	0x31
#define ITESIO_EC_VIN1_HIGH_LIMIT 	0x32
#define ITESIO_EC_VIN1_LOW_LIMIT 	0x33
#define ITESIO_EC_VIN2_HIGH_LIMIT 	0x34
#define ITESIO_EC_VIN2_LOW_LIMIT 	0x35
#define ITESIO_EC_VIN3_HIGH_LIMIT	0x36
#define ITESIO_EC_VIN3_LOW_LIMIT 	0x37
#define ITESIO_EC_VIN4_HIGH_LIMIT 	0x38
#define ITESIO_EC_VIN4_LOW_LIMIT 	0x39
#define ITESIO_EC_VIN5_HIGH_LIMIT 	0x3a
#define ITESIO_EC_VIN5_LOW_LIMIT 	0x3b
#define ITESIO_EC_VIN6_HIGH_LIMIT 	0x3c
#define ITESIO_EC_VIN6_LOW_LIMIT 	0x3d
#define ITESIO_EC_VIN7_HIGH_LIMIT 	0x3e
#define ITESIO_EC_VIN7_LOW_LIMIT 	0x3f

/* High and Low limits for temperatures */
#define ITESIO_EC_TEMP0_HIGH_LIMIT	0x40
#define ITESIO_EC_TEMP0_LOW_LIMIT	0x41
#define ITESIO_EC_TEMP1_HIGH_LIMIT	0x42
#define ITESIO_EC_TEMP1_LOW_LIMIT	0x43
#define ITESIO_EC_TEMP2_HIGH_LIMIT	0x44
#define ITESIO_EC_TEMP2_LOW_LIMIT	0x45

#define ITESIO_EC_VREF			(4096) /* Vref = 4.096 V */

struct itesio_softc {
	bus_space_tag_t 	sc_iot;

	bus_space_handle_t 	sc_pnp_ioh;
	bus_space_handle_t	sc_ec_ioh;

	struct sysmon_wdog	sc_smw;
	struct sysmon_envsys 	*sc_sme;
	envsys_data_t 		sc_sensor[IT_NUM_SENSORS];
	
	uint16_t 		sc_hwmon_baseaddr;
	bool 			sc_hwmon_mapped;
	bool 			sc_hwmon_enabled;
	bool 			sc_wdt_enabled;

	uint16_t 		sc_chipid;
	uint8_t 		sc_devrev;
};

#endif /* _DEV_ISA_ITSIO_ISAVAR_H_ */
