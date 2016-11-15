/*	$NetBSD: ulpireg.h,v 1.1 2010/11/27 13:41:49 bsh Exp $	*/
/*
 * Copyright (c) 2009, 2010  Genetec Corporation.  All rights reserved.
 * Written by Hashimoto Kenichi for Genetec Corporation.
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
 * THIS SOFTWARE IS PROVIDED BY GENETEC CORPORATION ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GENETEC CORPORATION
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef _DEV_USB_ULPI_ULPIREG_H
#define	_DEV_USB_ULPI_ULPIREG_H

/* commands */
#define	ULPI_CMD_SPECIAL	(0x0 << 6)
#define	ULPI_CMD_NOOP   	(ULPI_CMD_SPECIAL | 0x00)
#define	ULPI_CMD_TRANSMIT	(0x1 << 6)
#define	ULPI_CMD_PID(n)  	(ULPI_CMD_TRANSMIT | (n))
#define	ULPI_CMD_REGWRITE	(0x2 << 6)
#define	ULPI_CMD_EXTW   	(ULPI_CMD_REGWRITE | 0x2f)
#define	ULPI_CMD_REGREAD	(0x3 << 6)
#define	ULPI_CMD_EXTR   	(ULPI_CMD_REGREAD | 0x2f)

/* registers */
#define	ULPI_VENDOR_ID_LOW 		0x00
#define	ULPI_VENDOR_ID_HIGH 		0x01
#define	ULPI_PRODUCT_ID_LOW 		0x02
#define	ULPI_PRODUCT_ID_HIGH    	0x03
#define	ULPI_FUNCTION_CONTROL    	0x04
#define	 FUNCTION_CONTROL_XCVRSELECT	__BITS(0,1)
#define	 XCVRSELECT_HS			0x0
#define	 XCVRSELECT_FS			0x1
#define	 XCVRSELECT_LS			0x2
#define	 XCVRSELECT_FSLS		0x3  /* FS Xceiver for LS packets */
#define	 FUNCTION_CONTROL_TERMSELECT	__BIT(2)
#define	 FUNCTION_CONTROL_OPMODE	__BITS(3,4)
#define	 FUNCTION_CONTROL_RESET  	__BIT(5)
#define	 FUNCTION_CONTROL_SUSPENDM  	__BIT(6)
#define	ULPI_INTERFACE_CONTROL  	0x07
#define	ULPI_OTG_CONTROL 		0x0a
#define	 OTG_CONTROL_IDPULLUP		__BIT(0)	/* ID pull up */
#define	 OTG_CONTROL_DPPULLDOWN 	__BIT(1)	/* D+ pull down */
#define	 OTG_CONTROL_DMPULLDOWN  	__BIT(2)	/* D- pull down */
#define	 OTG_CONTROL_DISCHRGVBUS 	__BIT(3)	/* discharge Vbus */
#define	 OTG_CONTROL_CHRGVBUS   	__BIT(4)	/* charge Vbus */
#define	 OTG_CONTROL_DRVVBUS    	__BIT(5)	/* drive 5V on Vbus */
#define	 OTG_CONTROL_DRVVBUSEXT  	__BIT(6)	/* drive Vbus external */
#define	 OTG_CONTROL_USEEXTVBUSIND	__BIT(7)	/* use external Vbus over-current
							   indecator */
#define	ULPI_USB_INT_RISING_EDGE 	0x0d
#define	ULPI_USB_INT_FALLING_EDGE	0x10
#define	ULPI_USB_INT_STATUS 		0x13
#define	ULPI_USB_INTERRUPT_LATCH 	0x14
#define	ULPI_DEBUG			0x15
#define	ULPI_SCRATCH 			0x16
#define	ULPI_CARKIT_CONTROL		0x19
#define	ULPI_CARKIT_INTERRUPT_DELAY	0x1c
#define	ULPI_CARKIT_INTERRUPT_ENABLE	0x1d
#define	ULPI_CARKIT_INTERRUPT_STATUS	0x20
#define	ULPI_CARKIT_INTERRUPT_LATCH	0x21
#define	ULPI_CARKIT_PULSE_CONTROL	0x22
#define	ULPI_TRANSMIT_POSITIVE_WIDTH	0x25
#define	ULPI_TRANSMIT_NEGATIVE_WIDTH	0x26
#define	ULPI_RECEIVE_POLARITY_RECOVERY	0x27

#define	ULPI_VENDOR_SPECIFIC		0x30

#define	ULPI_REG_WRITE  	0
#define	ULPI_REG_SET 		1
#define	ULPI_REG_CLEAR 		2

#endif	/* _DEV_USB_ULPI_ULPIREG_H */
