/*	$NetBSD: mvpexvar.h,v 1.6 2015/06/24 10:00:37 knakahara Exp $	*/
/*
 * Copyright (c) 2009 KIYOHARA Takashi
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
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef	_MVPEXVAR_H_
#define	_MVPEXVAR_H_

extern enum marvell_tags *mvpex_bar2_tags;

struct mvpex_intrhand {
	LIST_ENTRY(mvpex_intrhand) ih_q;
	int (*ih_func)(void *);
	void *ih_arg;
	int ih_type;

	void *ih_intrtab;

	char ih_evname[PCI_INTRSTR_LEN];
	struct evcnt ih_evcnt;
};

struct mvpex_intrtab {
	int intr_pin;
	int intr_refcnt;
	LIST_HEAD(, mvpex_intrhand) intr_list;
};

struct mvpex_softc {
	device_t sc_dev;

	int sc_model;
	int sc_rev;

	bus_size_t sc_offset;
	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_ioh;

	struct mvpex_intrtab sc_intrtab[PCI_INTERRUPT_PIN_MAX];
};

#if NPCI > 0
void mvpex_attach_hook(device_t, device_t, struct pcibus_attach_args *);
int mvpex_bus_maxdevs(void *, int);
pcitag_t mvpex_make_tag(void *, int, int, int);
void mvpex_decompose_tag(void *, pcitag_t, int *, int *, int *);
pcireg_t mvpex_conf_read(void *, pcitag_t, int);
void mvpex_conf_write(void *, pcitag_t, int, pcireg_t);
int mvpex_conf_hook(void *, int, int, int, pcireg_t);
void mvpex_conf_interrupt(void *, int, int, int, int, int *);
int mvpex_intr_map(const struct pci_attach_args *, pci_intr_handle_t *);
const char *mvpex_intr_string(void *, pci_intr_handle_t, char *, size_t);
const struct evcnt *mvpex_intr_evcnt(void *, pci_intr_handle_t);
void *mvpex_intr_establish(void *, pci_intr_handle_t, int, int (*)(void *),
			   void *);
void mvpex_intr_disestablish(void *, void *);
#endif

#endif	/* _MVPEXVAR_H_ */
