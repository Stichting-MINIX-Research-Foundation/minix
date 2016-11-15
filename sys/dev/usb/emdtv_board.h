/* $NetBSD: emdtv_board.h,v 1.1 2011/07/11 18:02:04 jmcneill Exp $ */

/*-
 * Copyright (c) 2008 Jared D. McNeill <jmcneill@invisible.ca>
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

#ifndef _DEV_USB_EMDTV_BOARD_H
#define _DEV_USB_EMDTV_BOARD_H

typedef enum emdtv_tuner_type {
	EMDTV_TUNER_XC3028L,
	EMDTV_TUNER_XC3028,
	EMDTV_TUNER_XC5000,
} emdtv_tuner_type_t;

typedef enum emdtv_demod_type {
	EMDTV_DEMOD_LG3303,
	EMDTV_DEMOD_LG3304,
} emdtv_demod_type_t;

typedef enum emdtv_gpio_reg {
	EMDTV_GPIO_DEMOD1_RESET = 16,
	EMDTV_GPIO_TUNER1_RESET = 17,
	EMDTV_GPIO_TUNER1_ON = 23,
	EMDTV_GPIO_ANALOG_ON = 24,
	EMDTV_GPIO_TS1_ON = 25,
} emdtv_gpio_reg_t;

#define EMDTV_GPIO_BIT_VAL(reg, val, reset) \
	((reg) | 0x80 | (reset ? (1 << 6) : 0) | (val << 5))

struct emdtv_gpio_regs {
	uint16_t	ts1_on;
	uint16_t	a_on;
	uint16_t	t1_on;
	uint16_t	t1_reset;
	uint16_t	d1_reset;
};

struct emdtv_board {
	const char		*eb_name;
	uint16_t		eb_vendor;
	uint16_t		eb_product;
	emdtv_tuner_type_t	eb_tuner;
	emdtv_demod_type_t	eb_demod;
	bool			eb_manual_gpio;
	struct emdtv_gpio_regs	eb_gpio_regs;
};

const struct emdtv_board *emdtv_board_lookup(uint16_t, uint16_t);

#endif /* !_DEV_USB_EMDTV_BOARD_H */
