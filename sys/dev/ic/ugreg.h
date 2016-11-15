/* $NetBSD: ugreg.h,v 1.1 2007/05/08 16:48:38 xtraeme Exp $ */

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

#ifndef _UGREG_H_
#define _UGREG_H_

#define UG_DRV_VERSION  1000

/*
 * Abit uGuru (first version)
 */

#define UG_DELAY_CYCLES 5000
#define UG_NUM_SENSORS  19
#define UG_MAX_SENSORS  32

/* Data and Cmd offsets - Base is ussualy 0xE0 */
#define UG_CMD          0
#define UG_DATA         4

/* Temp and Voltage Sensors */
#define UG_CPUTEMP      0x2100
#define UG_SYSTEMP      0x2101
#define UG_HTV          0x2102
#define UG_VCORE        0x2103
#define UG_DDRVDD       0x2104
#define UG_3V3          0x2105
#define UG_5V           0x2106
#define UG_NBVDD        0x2108
#define UG_AGP          0x2109
#define UG_DDRVTT       0x210A
#define UG_5VSB         0x210B
#define UG_3VDUAL       0x210D
#define UG_SBVDD        0x210E
#define UG_PWMTEMP      0x210F

/* Fans */
#define UG_CPUFAN       0x2600
#define UG_NBFAN        0x2601
#define UG_SYSFAN       0x2602
#define UG_AUXFAN1      0x2603
#define UG_AUXFAN2      0x2604

/* RFacts */
#define UG_RFACT        1000
#define UG_RFACT3       3490 * UG_RFACT / 255
#define UG_RFACT4       4360 * UG_RFACT / 255
#define UG_RFACT6       6250 * UG_RFACT / 255
#define UG_RFACT_FAN    15300/255

/* Voltage and Fan sensors offsets */
#define UG_VOLT_MIN     3
#define UG_FAN_MIN      14

/*
 * Abit uGuru2 or uGuru 2005 settings
 */

/* Sensor banks */
#define UG2_SETTINGS_BANK               0x01
#define UG2_SENSORS_BANK                0x08
#define UG2_MISC_BANK                   0x09

/* Sensor offsets */
#define UG2_ALARMS_OFFSET               0x1E
#define UG2_SETTINGS_OFFSET             0x24
#define UG2_VALUES_OFFSET               0x80

/* Misc Sensor */
#define UG2_BOARD_ID                    0x0A

/* sensor types */
#define UG2_VOLTAGE_SENSOR              0
#define UG2_TEMP_SENSOR                 1
#define UG2_FAN_SENSOR                  2

/* uGuru status flags */
#define UG2_STATUS_READY_FOR_READ       0x01
#define UG2_STATUS_BUSY                 0x02
/* No more than 32 sensors */
#define UG2_MAX_NO_SENSORS 32

/* Unknown board should be the last. Now is 0x0016 */
#define UG_MAX_MSB_BOARD 0x00
#define UG_MAX_LSB_BOARD 0x16
#define UG_MIN_LSB_BOARD 0x0c

#endif		/* _UGREG_H_ */
