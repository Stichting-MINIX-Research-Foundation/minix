/*	$NetBSD: voyagervar.h,v 1.4 2011/12/07 09:08:00 macallan Exp $	*/

/*
 * Copyright (c) 2011 Michael Lorenz
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
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: voyagervar.h,v 1.4 2011/12/07 09:08:00 macallan Exp $");

#ifndef VOYAGERVAR_H
#define VOYAGERVAR_H

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>

struct voyager_attach_args {
	pci_chipset_tag_t		vaa_pc;
	pcitag_t					vaa_pcitag;
	bus_space_tag_t		vaa_tag;
	bus_space_handle_t	vaa_regh;
	bus_space_handle_t	vaa_memh;
	bus_addr_t		vaa_reg_pa;
	bus_addr_t		vaa_mem_pa;
	char			vaa_name[32];
};

void voyager_twiddle_bits(void *, int, uint32_t, uint32_t);

/* set gpio bits - (register & param1) | param2 */
#define voyager_write_gpio(cookie, m_and, m_or) \
	voyager_twiddle_bits(cookie, SM502_GPIO_DATA0, m_and, m_or)
/* control gpio pin usage - 0 is gpio, 1 is other stuff ( like PWM ) */
#define voyager_control_gpio(cookie, m_and, m_or) \
	voyager_twiddle_bits(cookie, SM502_GPIO0_CONTROL, m_and, m_or)
/* gpio direction - 1 is output */
#define voyager_gpio_dir(cookie, m_and, m_or) \
	voyager_twiddle_bits(cookie, SM502_GPIO_DIR0, m_and, m_or)
#define voyager_control_power_0(cookie, m_and, m_or) \
	voyager_twiddle_bits(cookie, SM502_POWER_MODE0_GATE, m_and, m_or)

void *voyager_establish_intr(device_t, int, int (*)(void *), void *);
void  voyager_disestablish_intr(device_t, void *);

/* frequency in Hz, duty cycle in 1000ths */
uint32_t voyager_set_pwm(int, int);

#endif