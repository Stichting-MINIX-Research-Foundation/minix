/* $NetBSD: emdtv_board.c,v 1.1 2011/07/11 18:02:04 jmcneill Exp $ */

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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: emdtv_board.c,v 1.1 2011/07/11 18:02:04 jmcneill Exp $");

#include <sys/types.h>
#include <sys/device.h>

#include <dev/usb/usbdevs.h>
#include <dev/usb/emdtvvar.h>

static const struct emdtv_board emdtv_boards[] = {
	{
		.eb_name = "ATI TV Wonder 600 USB",
		.eb_vendor = USB_VENDOR_AMD,
		.eb_product = USB_PRODUCT_AMD_TV_WONDER_600_USB,
		.eb_tuner = EMDTV_TUNER_XC3028L,
		.eb_demod = EMDTV_DEMOD_LG3303,
		.eb_manual_gpio = true,
		.eb_gpio_regs = {
			.ts1_on = EMDTV_GPIO_BIT_VAL(0, 0, 0),
			.a_on = EMDTV_GPIO_BIT_VAL(1, 0, 0),
			.t1_on = EMDTV_GPIO_BIT_VAL(6, 0, 0),
			.t1_reset = EMDTV_GPIO_BIT_VAL(4, 0, 1),
			.d1_reset = EMDTV_GPIO_BIT_VAL(19, 0, 1),
		},
	},
	{
		.eb_name = "Pinnacle PCTV HD Pro Stick 800e",
		.eb_vendor = USB_VENDOR_PINNACLE,
		.eb_product = USB_PRODUCT_PINNACLE_PCTV800E,
		.eb_tuner = EMDTV_TUNER_XC3028,
		.eb_demod = EMDTV_DEMOD_LG3303,
		.eb_manual_gpio = true,
		.eb_gpio_regs = {
			.ts1_on = EMDTV_GPIO_BIT_VAL(0, 0, 0),
			.a_on = EMDTV_GPIO_BIT_VAL(1, 0, 0),
			.t1_on = EMDTV_GPIO_BIT_VAL(6, 0, 0),
			.t1_reset = EMDTV_GPIO_BIT_VAL(4, 0, 1),
			.d1_reset = EMDTV_GPIO_BIT_VAL(19, 0, 1),
		},
	},
	{
		.eb_name = "Empia Hybrid XS ATSC",
		.eb_vendor = USB_VENDOR_EMPIA,
		.eb_product = USB_PRODUCT_EMPIA_EM2883,
		.eb_tuner = EMDTV_TUNER_XC5000,
		.eb_demod = EMDTV_DEMOD_LG3304,
	},
};

const struct emdtv_board *
emdtv_board_lookup(uint16_t vendor, uint16_t product)
{
	const struct emdtv_board *eb;
	unsigned int i;

	for (i = 0; i < __arraycount(emdtv_boards); i++) {
		eb = &emdtv_boards[i];
		if (vendor == eb->eb_vendor && product == eb->eb_product)
			return eb;
	}

	return NULL;
}
