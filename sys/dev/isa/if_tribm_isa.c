/*	$NetBSD: if_tribm_isa.c,v 1.14 2009/05/12 09:10:15 cegger Exp $	*/

/* XXXJRT changes isa_attach_args too early */

/*
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Onno van der Linden.
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
__KERNEL_RCSID(0, "$NetBSD: if_tribm_isa.c,v 1.14 2009/05/12 09:10:15 cegger Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <sys/device.h>

#include <net/if.h>
#include <net/if_ether.h>
#include <net/if_media.h>

#include <sys/bus.h>

#include <dev/isa/isavar.h>

#include <dev/ic/tropicreg.h>
#include <dev/ic/tropicvar.h>

int	tribm_isa_probe(device_t, cfdata_t, void *);
int	tr_isa_map_io(struct isa_attach_args *, bus_space_handle_t *,
	    bus_space_handle_t *);
void	tr_isa_unmap_io(struct isa_attach_args *, bus_space_handle_t,
	    bus_space_handle_t);

int
tribm_isa_probe(device_t parent, cfdata_t match, void *aux)
{
	struct isa_attach_args *ia = aux;
	static int irq_f[4] = { 9, 3, 6, 7 };
	static int irq_e[4] = { 9, 3, 10, 11 };
	bus_space_tag_t piot = ia->ia_iot;
	bus_space_tag_t memt = ia->ia_memt;
	bus_space_handle_t pioh, mmioh;
	int i, irq;
	u_int8_t s;

	if (ia->ia_nio < 1)
		return (0);
	if (ia->ia_niomem < 1)
		return (0);
	if (ia->ia_nirq < 1)
		return (0);

	if (ISA_DIRECT_CONFIG(ia))
		return (0);

#ifdef notyet
/* XXX Try both 0xa20 and 0xa24 and store that info like 3com */
	if (ia->ia_iobase == IOBASEUNK)
		ia->ia_iobase = 0xa20;
#else
	if (ia->ia_io[0].ir_addr == ISA_UNKNOWN_PORT)
		return (0);
#endif

	/*
	 * XXXJRT Should not modify attach_args unless we know we match!
	 */

	ia->ia_io[0].ir_size = 4;
	ia->ia_aux = NULL;

	if (tr_isa_map_io(ia, &pioh, &mmioh))
		return 0;

/*
 * F = Token-Ring Network PC Adapter
 *     Token-Ring Network PC Adapter II
 *     Token-Ring Network Adapter/A
 * E = Token-Ring Network 16/4 Adapter/A (long card)
 *     Token-Ring Network 16/4 Adapter
 * D = Token-Ring Network 16/4 Adapter/A (short card)
 *     16/4 ISA-16 Adapter
 * C = Auto 16/4 Token-Ring ISA Adapter
 *     Auto 16/4 Token-Ring MC Adapter
 */
/*
 * XXX Both 0xD and 0xC types should be able to use 16-bit read and writes
 */
	switch (bus_space_read_1(memt, mmioh, TR_TYP_OFFSET)) {
	case 0xF:
	case 0xE:
	case 0xD:
		if (ia->ia_iomem[0].ir_addr == ISA_UNKNOWN_IOMEM)
#ifdef notyet
			ia->ia_maddr = TR_SRAM_DEFAULT;
#else
			return 0;
#endif
		break;
	case 0xC:
		i = bus_space_read_1(memt, mmioh, TR_ACA_OFFSET) << 12;
		if (ia->ia_iomem[0].ir_addr == ISA_UNKNOWN_IOMEM)
			ia->ia_iomem[0].ir_addr = i;
		else if (ia->ia_iomem[0].ir_addr != i) {
			printf(
"tribm_isa_probe: sram mismatch; kernel configured %x != board configured %x\n",
				ia->ia_iomem[0].ir_addr, i);
			tr_isa_unmap_io(ia, pioh, mmioh);
			return 0;
		}
		break;
	default:
		printf("tribm_isa_probe: unknown type code %x\n",
		    bus_space_read_1(memt, mmioh, TR_TYP_OFFSET));
		tr_isa_unmap_io(ia, pioh, mmioh);
		return 0;
	}

	s = bus_space_read_1(piot, pioh, TR_SWITCH);

	switch (bus_space_read_1(memt, mmioh, TR_IRQ_OFFSET)) {
	case 0xF:
		irq = irq_f[s & 3];
		break;
	case 0xE:
		irq = irq_e[s & 3];
		break;
	default:
		printf("tribm_isa_probe: Unknown IRQ code %x\n",
		    bus_space_read_1(memt, mmioh, TR_IRQ_OFFSET));
		tr_isa_unmap_io(ia, pioh, mmioh);
		return 0;
	}

	if (ia->ia_irq[0].ir_irq == ISA_UNKNOWN_IRQ)
		ia->ia_irq[0].ir_irq = irq;
	else if (ia->ia_irq[0].ir_irq != irq) {
		printf(
"tribm_isa_probe: irq mismatch; kernel configured %d != board configured %d\n",
			ia->ia_irq[0].ir_irq, irq);
		tr_isa_unmap_io(ia, pioh, mmioh);
		return 0;
	}
/*
 * XXX 0x0c == MSIZEMASK (MSIZEBITS)
 */
	ia->ia_iomem[0].ir_size = 8192 <<
	    ((bus_space_read_1(memt, mmioh, TR_ACA_OFFSET + 1) & 0x0c) >> 2);
	tr_isa_unmap_io(ia, pioh, mmioh);
	/* Check alignment of membase. */
	if ((ia->ia_iomem[0].ir_addr & (ia->ia_iomem[0].ir_size-1)) != 0) {
		printf("tribm_isa_probe: SRAM unaligned 0x%04x/%d\n",
		    ia->ia_iomem[0].ir_addr, ia->ia_iomem[0].ir_size);
		return 0;
	}

	ia->ia_nio = 1;
	ia->ia_niomem = 1;
	ia->ia_nirq = 1;

	ia->ia_ndrq = 0;

 	return 1;
}
