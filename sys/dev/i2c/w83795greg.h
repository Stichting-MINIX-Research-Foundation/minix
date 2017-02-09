/*	$NetBSD: w83795greg.h,v 1.1 2013/08/06 15:58:25 soren Exp $	*/

/*
 * Copyright (c) 2013 Soren S. Jorvang.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Nuvoton_W83795G_W83795ADG_Datasheet_V1.43.pdf
 */

#define W83795G_BANKSEL	0x00		/* Bank Selection */
#define BANKSEL_HBACS		0x80
#define BANKSEL_BANK		0x07

/*
 * Bank 0 registers
 */
#define W83795G_CONFIG	0x01		/* Configuration */
#define CONFIG_START		0x01
#define CONFIG_INT_CLEAR	0x02
#define CONFIG_CONFIG48		0x04
#define CONFIG_CLKSEL		0x18
#define CONFIG_RST_VDD_MD	0x20
#define CONFIG_SYS_RST_MD	0x40
#define CONFIG_INIT		0x80

#define W83795G_V_CTRL1	0x02		/* Voltage monitoring control */
#define W83795G_V_CTRL2	0x03		/* Voltage monitoring control */
#define W83795G_T_CTRL1	0x04		/* Temp monitoring control */
#define W83795G_T_CTRL2	0x05		/* Temp monitoring control */
#define W83795G_F_CTRL1	0x06		/* FANIN monitoring control */
#define W83795G_F_CTRL2	0x07		/* FANIN monitoring control */
#define W83795G_VMIGB_C	0x08		/* 8x voltage input gain control */
#define VMIGB_C_GAIN_VDSEN14	0x01
#define VMIGB_C_GAIN_VDSEN15	0x02
#define VMIGB_C_GAIN_VDSEN16	0x04
#define VMIGB_C_GAIN_VDSEN17	0x08
#define W83795G_GPIO_M	0x09		/* GPIO I/O mode control */
#define W83795G_GPIO_I	0x0a		/* GPIO input data */
#define W83795G_GPIO_O	0x0b		/* GPIO output data */

#define W83795G_WDTLOCK	0x0c		/* Lock Watch Dog */
#define WDTLOCK_ENABLE_SOFT	0x55
#define WDTLOCK_DISABLE_SOFT	0xaa 
#define WDTLOCK_ENABLE_HARD	0x33
#define WDTLOCK_DISABLE_HARD	0xcc

#define W83795G_WDT_ENA	0x0d		/* Watch Dog Enable */
#define WDT_ENA_SOFT		0x01
#define WDT_ENA_HARD		0x02
#define WDT_ENA_ENWDT		0x04

#define W83795G_WDT_STS	0x0e		/* Watch Dog Status */
#define WDT_STS_SOFT_TO		0x01
#define WDT_STS_HARD_TO		0x02
#define WDT_STS_WDT_ST		0x0c

#define W83795G_WDT_CNT	0x0f		/* Watch Dog Timeout Counter */

#define W83795G_VSEN1	0x10
#define W83795G_VSEN2	0x11
#define W83795G_VSEN3	0x12
#define W83795G_VSEN4	0x13
#define W83795G_VSEN5	0x14
#define W83795G_VSEN6	0x15
#define W83795G_VSEN7	0x16
#define W83795G_VSEN8	0x17
#define W83795G_VSEN9	0x18
#define W83795G_VSEN10	0x19
#define W83795G_VSEN11	0x1a
#define W83795G_VTT	0x1b
#define W83795G_3VDD	0x1c
#define W83795G_3VSB	0x1d
#define W83795G_VBAT	0x1e
#define W83795G_TR5	0x1f
#define W83795G_VSEN12	W83795G_TR5
#define W83795G_TR6	0x20
#define W83795G_VSEN13	W83795G_TR6
#define W83795G_TD1	0x21
#define W83795G_TR1	W83795G_TD1
#define W83795G_VDSEN14	W83795G_TD1
#define W83795G_TD2	0x22
#define W83795G_TR2	W83795G_TD2
#define W83795G_VDSEN15	W83795G_TD2
#define W83795G_TD3	0x23
#define W83795G_TR3	W83795G_TD3
#define W83795G_VDSEN16	W83795G_TD3
#define W83795G_TD4	0x24
#define W83795G_TR4	W83795G_TD4
#define W83795G_VDSEN17	W83795G_TD4
#define W83795G_DTS1	0x26
#define W83795G_DTS2	0x27
#define W83795G_DTS3	0x28
#define W83795G_DTS4	0x29
#define W83795G_DTS5	0x2a
#define W83795G_DTS6	0x2b
#define W83795G_DTS7	0x2c
#define W83795G_DTS8	0x2d
#define W83795G_FANIN1	0x2e
#define W83795G_FANIN2	0x2f
#define W83795G_FANIN3	0x30
#define W83795G_FANIN4	0x31
#define W83795G_FANIN5	0x32
#define W83795G_FANIN6	0x33
#define W83795G_FANIN7	0x34
#define W83795G_FANIN8	0x35
#define W83795G_FANIN9	0x36
#define W83795G_FANIN10	0x37
#define W83795G_FANIN11	0x38
#define W83795G_FANIN12	0x39
#define W83795G_FANIN13	0x3a
#define W83795G_FANIN14	0x3b
#define W83795G_VR_LSB	0x3c		/* Monitored Channel Readout Low Byte */

#define W83795G_SMICTRL	0x40		/* SMI Control */
#define W83795G_SMISTS1	0x41		/* SMI Status 1 */
#define W83795G_SMISTS2	0x42		/* SMI Status 2 */
#define W83795G_SMISTS3	0x43		/* SMI Status 3 */
#define W83795G_SMISTS4	0x44		/* SMI Status 4 */
#define W83795G_SMISTS5	0x45		/* SMI Status 5 */
#define W83795G_SMISTS6	0x46		/* SMI Status 6 */
#define W83795G_SMISTS7	0x47		/* SMI Status 7 */
#define W83795G_SMIMSK1	0x48		/* SMI Mask 1 */
#define W83795G_SMIMSK2	0x49		/* SMI Mask 2 */
#define W83795G_SMIMSK3	0x4a		/* SMI Mask 3 */
#define W83795G_SMIMSK4	0x4b		/* SMI Mask 4 */
#define W83795G_SMIMSK5	0x4c		/* SMI Mask 5 */
#define W83795G_SMIMSK6	0x4d		/* SMI Mask 6 */
#define W83795G_SMIMSK7	0x4e		/* SMI Mask 7 */
#define W83795G_BEEP1	0x50		/* BEEP Control 1 */
#define W83795G_BEEP2	0x51		/* BEEP Control 1 */
#define W83795G_BEEP3	0x52		/* BEEP Control 2 */
#define W83795G_BEEP4	0x53		/* BEEP Control 3 */
#define W83795G_BEEP5	0x54		/* BEEP Control 4 */
#define W83795G_BEEP6	0x55		/* BEEP Control 5 */
#define W83795G_OVT_GLB	0x58		/* OVT Global Enable */
#define W83795G_OVT1_C1	0x59		/* OVT1 Control 1 */
#define W83795G_OVT1_C2	0x5a		/* OVT1 Control 1 */
#define W83795G_OVT2_C1	0x5b		/* OVT2 Control 1 */
#define W83795G_OVT2_C2	0x5c		/* OVT2 Control 1 */
#define W83795G_OVT3_C1	0x5d		/* OVT3 Control 1 */
#define W83795G_OVT3_C2	0x5e		/* OVT3 Control 1 */
#define W83795G_THERM	0x5f		/* THERMTRIP Control/Status */
#define W83795G_PROCSTS	0x60		/* PROCHOT Processor Hot Status */
#define W83795G_PROC1	0x61		/* PROCHOT1# Processor Hot Control */
#define W83795G_PROC2	0x62		/* PROCHOT2# Processor Hot Control */
#define W83795G_PROC3	0x63		/* PROCHOT3# Processor Hot Control */
#define W83795G_PROC4	0x64		/* PROCHOT4# Processor Hot Control */
#define W83795G_VFAULT1	0x65		/* VOLT_FAULT# Control 1 */
#define W83795G_VFAULT2	0x66		/* VOLT_FAULT# Control 2 */
#define W83795G_VFAULT3	0x67		/* VOLT_FAULT# Control 3 */
#define W83795G_FFAULT1	0x68		/* FAN_FAULT# Control 1 */
#define W83795G_FFAULT2	0x69		/* FAN_FAULT# Control 2 */
#define W83795G_VIDCTRL	0x6a		/* VID Control */
#define W83795G_DVID_HI	0x6b		/* Dynamic VID High Tolerance */
#define W83795G_DVID_LO	0x6c		/* Dynamic VID Low Tolerance */
#define W83795G_V1VIDIN	0x6d		/* VSEN1 VID Input Value */
#define W83795G_V2VIDIN	0x6e		/* VSEN2 VID Input Value */
#define W83795G_V3VIDIN	0x6f		/* VSEN3 VID Input Value */

#define W83795G_VSEN1HL	0x70
#define W83795G_VSEN1LL	0x71
#define W83795G_VSEN2HL	0x72
#define W83795G_VSEN2LL	0x73
#define W83795G_VSEN3HL	0x74
#define W83795G_VSEN3LL	0x75
#define W83795G_VSEN4HL	0x76
#define W83795G_VSEN4LL	0x77
#define W83795G_VSEN5HL	0x78
#define W83795G_VSEN5LL	0x79
#define W83795G_VSEN6HL	0x7a
#define W83795G_VSEN6LL	0x7b
#define W83795G_VSEN7HL	0x7c
#define W83795G_VSEN7LL	0x7d
#define W83795G_VSEN8HL	0x7e
#define W83795G_VSEN8LL	0x7f
#define W83795G_VSEN9HL	0x80
#define W83795G_VSEN9LL	0x81
#define W83795G_VSEN10H	0x82
#define W83795G_VSEN10L	0x83
#define W83795G_VSEN11H	0x84
#define W83795G_VSEN11L	0x85
#define W83795G_VTT_HL	0x86
#define W83795G_VTT_LL	0x87
#define W83795G_3VDD_HL	0x88
#define W83795G_3VDD_LL	0x89
#define W83795G_3VSB_HL	0x8a
#define W83795G_3VSB_LL	0x8b
#define W83795G_VBAT_HL	0x8c
#define W83795G_VBAT_LL	0x8d
#define W83795G_V1H_LSB	0x8e
#define W83795G_V1L_LSB	0x8f
#define W83795G_V2H_LSB	0x90
#define W83795G_V2L_LSB	0x91
#define W83795G_V3H_LSB	0x92
#define W83795G_V3L_LSB	0x93
#define W83795G_V4H_LSB	0x94
#define W83795G_V4L_LSB	0x95
#define W83795G_TD1CRIT	0x96
#define W83795G_VD14_HL	W83795G_TD1CRIT
#define W83795G_TD1CRTH	0x97
#define W83795G_VD14_LL	W83795G_TD1CRTH
#define W83795G_TD1WARN	0x98
#define W83795G_VD14HLL	W83795G_TD1WARN
#define W83795G_TD1WRNH	0x99
#define W83795G_VD14LLL	W83795G_TD1WRNH
#define W83795G_TD2CRIT	0x9a
#define W83795G_VD15_HL	W83795G_TD2CRIT
#define W83795G_TD2CRTH	0x9b
#define W83795G_VD15_LL	W83795G_TD2CRTH
#define W83795G_TD2WARN	0x9c
#define W83795G_VD15HLL	W83795G_TD2WARN
#define W83795G_TD2WRNH	0x9d
#define W83795G_VD15LLL	W83795G_TD2WRNH
#define W83795G_TD3CRIT	0x9e
#define W83795G_VD16_HL	W83795G_TD3CRIT
#define W83795G_TD3CRTH	0x9f
#define W83795G_VD16_LL	W83795G_TD3CRTH
#define W83795G_TD3WARN	0xa0
#define W83795G_VD16HLL	W83795G_TD3WARN
#define W83795G_TD3WRNH	0xa1
#define W83795G_VD16LLL	W83795G_TD3WRNH
#define W83795G_TD4CRIT	0xa2
#define W83795G_VD17_HL	W83795G_TD4CRIT
#define W83795G_TD4CRTH	0xa3
#define W83795G_VD17_LL	W83795G_TD4CRTH
#define W83795G_TD4WARN	0xa4
#define W83795G_VD17HLL	W83795G_TD4WARN
#define W83795G_TD4WRNH	0xa5
#define W83795G_VD17LLL	W83795G_TD4WRNH
#define W83795G_TR5CRIT	0xa6
#define W83795G_VS12_HL	W83795G_TR5CRIT
#define W83795G_TR5CRTH	0xa7
#define W83795G_VS12_LL	W83795G_TR5CRIH
#define W83795G_TR5WARN	0xa8
#define W83795G_VS12HLL	W83795G_TR5WARN
#define W83795G_TR5WRNH	0xa9
#define W83795G_VS12LLL	W83795G_TR5WRNH
#define W83795G_TR6CRIT	0xaa
#define W83795G_VS13_HL	W83795G_TR6CRIT
#define W83795G_TR6CRTH	0xab
#define W83795G_VS13_LL	W83795G_TR6CRIH
#define W83795G_TR6WARN	0xac
#define W83795G_VS13HLL	W83795G_TR6WARN
#define W83795G_TR6WRNH	0xad
#define W83795G_VS13LLL	W83795G_TR6WRNH
#define W83795G_DTSCRIT	0xb2
#define W83795G_DTSCRTH	0xb3
#define W83795G_DTSWARN	0xb4
#define W83795G_DTSWRNH	0xb5
#define W83795G_FAN1HL	0xb6
#define W83795G_FAN2HL	0xb7
#define W83795G_FAN3HL	0xb8
#define W83795G_FAN4HL	0xb9
#define W83795G_FAN5HL	0xba
#define W83795G_FAN6HL	0xbb
#define W83795G_FAN7HL	0xbc
#define W83795G_FAN8HL	0xbd
#define W83795G_FAN9HL	0xbe
#define W83795G_FAN10HL	0xbf
#define W83795G_FAN11HL	0xc0
#define W83795G_FAN12HL	0xc1
#define W83795G_FAN13HL	0xc2
#define W83795G_FAN14HL	0xc3
#define W83795G_FHL1LSB	0xc4
#define W83795G_FHL2LSB	0xc5
#define W83795G_FHL3LSB	0xc6
#define W83795G_FHL4LSB	0xc7
#define W83795G_FHL5LSB	0xc8
#define W83795G_FHL6LSB	0xc9
#define W83795G_FHL7LSB	0xca

#define W83795G_TD1_OFF	0xd0
#define W83795G_TD2_OFF	0xd1
#define W83795G_TD3_OFF	0xd2
#define W83795G_TD4_OFF	0xd3
#define W83795G_TD56OFF	0xd4

#define W83795G_DEVICE	0xfb		/* Nuvoton Device ID */
#define DEVICE_B		0x51
#define DEVICE_C		0x52

#define W83795G_I2CADDR	0xfc		/* I2C Address */
#define I2CADDR_MINADDR		0x2c	/* Datasheet says 0x58 ! */
#define I2CADDR_MAXADDR		0x2f	/* Datasheet says 0x5e ! */

#define W83795G_VENDOR	0xfd		/* Nuvoton Vendor ID */
#define VENDOR_NUVOTON		0x5c
#define VENDOR_NUVOTON_ID_HI    0x5c
#define VENDOR_NUVOTON_ID_LO    0xa3

#define W83795G_CHIP	0xfe		/* Nuvoton Chip ID */
#define CHIP_W83795G		0x79

#define W83795G_DEVICEA	0xff
#define DEVICEA_A		0x50

/*
 * Bank 1 registers
 *
 * UDID/ASF
 */

/*
 * Bank 2 registers
 */
#define W83795G_FCMS1	0x01		/* Fan Control Mode Selection */
#define W83795G_T1FMR	0x02		/* Temperature to Fan Mapping */
#define W83795G_T2FMR	0x03		/* Temperature to Fan Mapping */
#define W83795G_T3FMR	0x04		/* Temperature to Fan Mapping */
#define W83795G_T4FMR	0x05		/* Temperature to Fan Mapping */
#define W83795G_T5FMR	0x06		/* Temperature to Fan Mapping */
#define W83795G_T6FMR	0x07		/* Temperature to Fan Mapping */
#define W83795G_FCMS2	0x08		/* Fan Control Mode Selection */
#define W83795G_T12TSS	0x09		/* Temperature Source Selection */
#define W83795G_T34TSS	0x0a		/* Temperature Source Selection */
#define W83795G_T56TSS	0x0b		/* Temperature Source Selection */
#define W83795G_DFSP	0x0c		/* Default Fan Speed at Power-on */
#define W83795G_SFOSUT	0x0d		/* SmartFan Output Step Up Time */
#define W83795G_SFOSDT	0x0e		/* SmartFan Output Step Down Time */
#define W83795G_FOMC	0x0f		/* Fan Output Mode Control */
#define W83795G_F1OV	0x10		/* Fan Output Value */
#define W83795G_F2OV	0x11		/* Fan Output Value */
#define W83795G_F3OV	0x12		/* Fan Output Value */
#define W83795G_F4OV	0x13		/* Fan Output Value */
#define W83795G_F5OV	0x14		/* Fan Output Value */
#define W83795G_F6OV	0x15		/* Fan Output Value */
#define W83795G_F7OV	0x16		/* Fan Output Value */
#define W83795G_F8OV	0x17		/* Fan Output Value */
#define W83795G_F1PFP	0x18		/* Fan Output PWM Frequency Prescalar */
#define W83795G_F2PFP	0x19		/* Fan Output PWM Frequency Prescalar */
#define W83795G_F3PFP	0x1a		/* Fan Output PWM Frequency Prescalar */
#define W83795G_FdPFP	0x1b		/* Fan Output PWM Frequency Prescalar */
#define W83795G_F5PFP	0x1c		/* Fan Output PWM Frequency Prescalar */
#define W83795G_F6PFP	0x1d		/* Fan Output PWM Frequency Prescalar */
#define W83795G_F7PFP	0x1e		/* Fan Output PWM Frequency Prescalar */
#define W83795G_F8PFP	0x1f		/* Fan Output PWM Frequency Prescalar */
#define W83795G_F1OSV	0x20		/* Fan Output Start-up Value */
#define W83795G_F2OSV	0x21		/* Fan Output Start-up Value */
#define W83795G_F3OSV	0x22		/* Fan Output Start-up Value */
#define W83795G_F4OSV	0x23		/* Fan Output Start-up Value */
#define W83795G_F5OSV	0x24		/* Fan Output Start-up Value */
#define W83795G_F6OSV	0x25		/* Fan Output Start-up Value */
#define W83795G_F7OSV	0x26		/* Fan Output Start-up Value */
#define W83795G_F8OSV	0x27		/* Fan Output Start-up Value */
#define W83795G_F1ONV	0x28		/* Fan Output Nonstop Value */
#define W83795G_F2ONV	0x29		/* Fan Output Nonstop Value */
#define W83795G_F3ONV	0x2a		/* Fan Output Nonstop Value */
#define W83795G_F4ONV	0x2b		/* Fan Output Nonstop Value */
#define W83795G_F5ONV	0x2c		/* Fan Output Nonstop Value */
#define W83795G_F6ONV	0x2d		/* Fan Output Nonstop Value */
#define W83795G_F7ONV	0x2e		/* Fan Output Nonstop Value */
#define W83795G_F8ONV	0x2f		/* Fan Output Nonstop Value */
#define W83795G_F1OST	0x30		/* Fan Output Stop Time */
#define W83795G_F2OST	0x31		/* Fan Output Stop Time */
#define W83795G_F3OST	0x32		/* Fan Output Stop Time */
#define W83795G_F4OST	0x33		/* Fan Output Stop Time */
#define W83795G_F5OST	0x34		/* Fan Output Stop Time */
#define W83795G_F6OST	0x35		/* Fan Output Stop Time */
#define W83795G_F7OST	0x36		/* Fan Output Stop Time */
#define W83795G_F8OST	0x37		/* Fan Output Stop Time */
#define W83795G_FOPPC	0x38		/* Fan Output PWM Polarity Control */
#define W83795G_F1TSH	0x40		/* FANIN Target Speed */
#define W83795G_F1TSL	0x41		/* FANIN Target Speed */
#define W83795G_F2TSH	0x42		/* FANIN Target Speed */
#define W83795G_F2TSL	0x43		/* FANIN Target Speed */
#define W83795G_F3TSH	0x44		/* FANIN Target Speed */
#define W83795G_F3TSL	0x45		/* FANIN Target Speed */
#define W83795G_F4TSH	0x46		/* FANIN Target Speed */
#define W83795G_F4TSL	0x47		/* FANIN Target Speed */
#define W83795G_F5TSH	0x48		/* FANIN Target Speed */
#define W83795G_F5TSL	0x49		/* FANIN Target Speed */
#define W83795G_F6TSH	0x4a		/* FANIN Target Speed */
#define W83795G_F6TSL	0x4b		/* FANIN Target Speed */
#define W83795G_F7TSH	0x4c		/* FANIN Target Speed */
#define W83795G_F7TSL	0x4d		/* FANIN Target Speed */
#define W83795G_F8TSH	0x4e		/* FANIN Target Speed */
#define W83795G_F8TSL	0x4f		/* FANIN Target Speed */
#define W83795G_TFTS	0x50		/* Tolerance of FANIN Target Speed */
#define W83795G_T1TTI	0x60		/* Target Temperature of Inputs */
#define W83795G_T2TTI	0x61		/* Target Temperature of Inputs */
#define W83795G_T3TTI	0x62		/* Target Temperature of Inputs */
#define W83795G_T4TTI	0x63		/* Target Temperature of Inputs */
#define W83795G_T5TTI	0x64		/* Target Temperature of Inputs */
#define W83795G_T6TTI	0x65		/* Target Temperature of Inputs */
#define W83795G_T1CTFS	0x68		/* Critical Temperature to Full Speed */
#define W83795G_T2CTFS	0x69		/* Critical Temperature to Full Speed */
#define W83795G_T3CTFS	0x6a		/* Critical Temperature to Full Speed */
#define W83795G_T4CTFS	0x6b		/* Critical Temperature to Full Speed */
#define W83795G_T5CTFS	0x6c		/* Critical Temperature to Full Speed */
#define W83795G_HT1	0x70		/* Hysteresis of Temperature */
#define W83795G_HT2	0x71		/* Hysteresis of Temperature */
#define W83795G_HT3	0x72		/* Hysteresis of Temperature */
#define W83795G_HT4	0x73		/* Hysteresis of Temperature */
#define W83795G_HT5	0x74		/* Hysteresis of Temperature */
#define W83795G_HT6	0x75		/* Hysteresis of Temperature */
#define W83795G_SFIV	0x80		/* SMART FAN IV Temperature Maps */
#define W83795G_CRPE1	0xe0		/* Configuration of PECI Error */
#define W83795G_CRPE2	0xe1		/* Configuration of PECI Error */
#define W83795G_F1OMV	0xe2		/* Fan Output Min Value on PECI Error */
#define W83795G_F2OMV	0xe3		/* Fan Output Min Value on PECI Error */
#define W83795G_F3OMV	0xe4		/* Fan Output Min Value on PECI Error */
#define W83795G_F4OMV	0xe5		/* Fan Output Min Value on PECI Error */
#define W83795G_F5OMV	0xe6		/* Fan Output Min Value on PECI Error */
#define W83795G_F6OMV	0xe7		/* Fan Output Min Value on PECI Error */
#define W83795G_F7OMV	0xe8		/* Fan Output Min Value on PECI Error */
#define W83795G_F8OMV	0xe9		/* Fan Output Min Value on PECI Error */

/*
 * Bank 3 registers
 *
 * PECI/SB-TSI
 */
