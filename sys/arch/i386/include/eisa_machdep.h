/*	$NetBSD: eisa_machdep.h,v 1.14 2015/04/27 08:30:11 knakahara Exp $	*/

/*
 * Copyright (c) 1996 Christopher G. Demetriou.  All rights reserved.
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
 *      This product includes software developed by Christopher G. Demetriou
 *	for the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
 * Machine-specific definitions for EISA autoconfiguration.
 */

/*
 * i386-specific EISA definitions.
 * NOT TO BE USED DIRECTLY BY MACHINE INDEPENDENT CODE.
 */
#define	EISA_ID			"EISA"
#define	EISA_ID_LEN		(sizeof(EISA_ID) - 1)
#define	EISA_ID_PADDR		0xfffd9

extern struct x86_bus_dma_tag eisa_bus_dma_tag;

/* EISA Edge/Level trigger control registers */
#define ELCR0   0x4d0                   /* eisa irq 0-7 */
#define ELCR1   0x4d1                   /* eisa irq 8-15 */

#include <machine/intr.h> /* for intr_handle_t */
/*
 * Types provided to machine-independent EISA code.
 */
typedef void *eisa_chipset_tag_t;
typedef intr_handle_t eisa_intr_handle_t;

/*
 * Functions provided to machine-independent EISA code.
 */
void		eisa_attach_hook(device_t, device_t,
		    struct eisabus_attach_args *);
int		eisa_maxslots(eisa_chipset_tag_t);
int		eisa_intr_map(eisa_chipset_tag_t, u_int,
		    eisa_intr_handle_t *);
const char	*eisa_intr_string(eisa_chipset_tag_t, eisa_intr_handle_t,
		    char *, size_t);
const struct evcnt *eisa_intr_evcnt(eisa_chipset_tag_t, eisa_intr_handle_t);
void		*eisa_intr_establish(eisa_chipset_tag_t,
		    eisa_intr_handle_t, int, int, int (*)(void *), void *);
void		eisa_intr_disestablish(eisa_chipset_tag_t, void *);

int		eisa_conf_read_mem(eisa_chipset_tag_t, int, int, int,
		    struct eisa_cfg_mem *);
int		eisa_conf_read_irq(eisa_chipset_tag_t, int, int, int,
		    struct eisa_cfg_irq *);
int		eisa_conf_read_dma(eisa_chipset_tag_t, int, int, int,
		    struct eisa_cfg_dma *);
int		eisa_conf_read_io(eisa_chipset_tag_t, int, int, int,
		    struct eisa_cfg_io *);
