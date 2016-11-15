/*	$NetBSD: gtpcivar.h,v 1.11 2012/09/07 04:25:37 matt Exp $	*/
/*
 * Copyright (c) 2008 KIYOHARA Takashi
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

#ifndef	_GTPCIVAR_H_
#define	_GTPCIVAR_H_

struct gtpci_softc {
	device_t sc_dev;
	int sc_model;
	int sc_rev;
	int sc_unit;

	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_ioh;
	pci_chipset_tag_t sc_pc;
};

/* protection flags */ 
struct gtpci_prot {
	uint32_t acbl_flags;
	uint32_t acs_flags;
};

#if NPCI > 0
void gtpci_attach_hook(device_t, device_t, struct pcibus_attach_args *);
int gtpci_bus_maxdevs(void *, int);
pcitag_t gtpci_make_tag(void *, int, int, int);
void gtpci_decompose_tag(void *, pcitag_t, int *, int *, int *);
pcireg_t gtpci_conf_read(void *, pcitag_t, int);
void gtpci_conf_write(void *, pcitag_t, int, pcireg_t);
int gtpci_conf_hook(void *, int, int, int, pcireg_t);
void gtpci_conf_interrupt(void *, int, int, int, int, int *);
int gtpci_intr_map(struct pci_attach_args *, pci_intr_handle_t *);
const char *gtpci_intr_string(void *, pci_intr_handle_t);
const struct evcnt *gtpci_intr_evcnt(void *, pci_intr_handle_t);
void *gtpci_intr_establish(void *, pci_intr_handle_t, int, int (*)(void *),
			   void *);
void gtpci_intr_disestablish(void *, void *);
#endif

#endif	/* _GTPCIVAR_H_ */
