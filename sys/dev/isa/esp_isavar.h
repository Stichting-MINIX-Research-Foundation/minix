/*	$NetBSD: esp_isavar.h,v 1.5 2008/04/13 04:55:53 tsutsui Exp $	*/

/*
 * Copyright (c) 1997 Allen Briggs.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Peter Galbavy.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

/*
 * Copyright (c) 1997 Eric S. Hvozda (hvozda@netcom.com)
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Eric S. Hvozda.
 * 4. The name of Eric S. Hvozda may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

struct esp_isa_softc {
	struct ncr53c9x_softc	sc_ncr53c9x;	/* glue to MI code */

	int		sc_active;		/* Pseudo-DMA state vars */
	int		sc_tc;
	int		sc_datain;
	size_t		sc_dmasize;
	size_t		sc_dmatrans;
	uint8_t		**sc_dmaaddr;
	size_t		*sc_pdmalen;

	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_ioh;
	void *sc_ih;
	int sc_irq;
	int sc_drq;

#ifdef ESP_DEBUG
	int sc_debug;
#endif
};

struct esp_isa_probe_data {
	int sc_irq;
	int sc_isncr;
	int sc_rev;
	int sc_isfast;
	int sc_msize;
	int sc_parity;
	int sc_sync;
	int sc_id;
	uint8_t sc_cfg4, sc_cfg5;
};

#define ESP_ISA_IOSIZE  16

int	esp_isa_find(bus_space_tag_t, bus_space_handle_t,
	    struct esp_isa_probe_data *);
void    esp_isa_init(struct esp_isa_softc *, struct esp_isa_probe_data *);

#ifdef ESP_DEBUG
extern int esp_isa_debug;

#define ESP_SHOWTRAC    0x01
#define ESP_SHOWREGS    0x02
#define ESP_SHOWMISC    0x04

#define ESP_TRACE(str)  \
        do {if (esp_isa_debug & ESP_SHOWTRAC) printf str;} while (0)
#define ESP_REGS(str)  \
        do {if (esp_isa_debug & ESP_SHOWREGS) printf str;} while (0)
#define ESP_MISC(str)  \
        do {if (esp_isa_debug & ESP_SHOWMISC) printf str;} while (0)
#else
#define ESP_TRACE(str)
#define ESP_REGS(str)
#define ESP_MISC(str)
#endif
