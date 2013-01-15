/*	$NetBSD: pci_machdep_common.h,v 1.9 2012/06/15 13:58:34 yamt Exp $	*/

/*
 * Copyright (c) 1996 Christopher G. Demetriou.  All rights reserved.
 * Copyright (c) 1994 Charles M. Hannum.  All rights reserved.
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
 *	This product includes software developed by Charles M. Hannum.
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

#ifndef _X86_PCI_MACHDEP_COMMON_H_
#define _X86_PCI_MACHDEP_COMMON_H_

/*
 * Machine-specific definitions for PCI autoconfiguration.
 */
#define	__HAVE_PCIIDE_MACHDEP_COMPAT_INTR_ESTABLISH
#ifndef XEN
#define	__HAVE_PCIIDE_MACHDEP_COMPAT_INTR_DISESTABLISH
#endif

/*
 * i386-specific PCI structure and type definitions.
 * NOT TO BE USED DIRECTLY BY MACHINE INDEPENDENT CODE.
 *
 * Configuration tag; created from a {bus,device,function} triplet by
 * pci_make_tag(), and passed to pci_conf_read() and pci_conf_write().
 * We could instead always pass the {bus,device,function} triplet to
 * the read and write routines, but this would cause extra overhead.
 *
 * Mode 2 is historical and deprecated by the Revision 2.0 specification.
 *
 *
 * Mode 1 tag:
 *	 31              24           16 15     11 10  8
 *	+---------------------------------------------------------------+
 *	|1|      0      |      BUS      |   DEV   |FUNC |       0       |
 *	+---------------------------------------------------------------+
 */
union x86_pci_tag_u {
	uint32_t mode1;
	struct {
		uint16_t port;
		uint8_t enable;
		uint8_t forward;
	} mode2;
};

extern struct x86_bus_dma_tag pci_bus_dma_tag;
#ifdef _LP64
extern struct x86_bus_dma_tag pci_bus_dma64_tag;
#endif

struct pci_attach_args;
struct pci_chipset_tag;

/*
 * Types provided to machine-independent PCI code
 */
typedef struct pci_chipset_tag *pci_chipset_tag_t;
typedef union x86_pci_tag_u pcitag_t;

struct pci_chipset_tag {
	pci_chipset_tag_t pc_super;
	uint64_t pc_present;
	const struct pci_overrides *pc_ov;
	void *pc_ctx;
};

/*
 * i386-specific PCI variables and functions.
 * NOT TO BE USED DIRECTLY BY MACHINE INDEPENDENT CODE.
 */
int		pci_bus_flags(void);
int		pci_mode_detect(void);
void		pci_mode_set(int);

/*
 * Functions provided to machine-independent PCI code.
 */
void		pci_attach_hook(device_t, device_t,
		    struct pcibus_attach_args *);
int		pci_bus_maxdevs(pci_chipset_tag_t, int);
pcitag_t	pci_make_tag(pci_chipset_tag_t, int, int, int);
void		pci_decompose_tag(pci_chipset_tag_t, pcitag_t,
		    int *, int *, int *);
pcireg_t	pci_conf_read(pci_chipset_tag_t, pcitag_t, int);
void		pci_conf_write(pci_chipset_tag_t, pcitag_t, int,
		    pcireg_t);
int		pci_intr_map(const struct pci_attach_args *,
		    pci_intr_handle_t *);
const char	*pci_intr_string(pci_chipset_tag_t, pci_intr_handle_t);
const struct evcnt *pci_intr_evcnt(pci_chipset_tag_t, pci_intr_handle_t);
void		*pci_intr_establish(pci_chipset_tag_t, pci_intr_handle_t,
		    int, int (*)(void *), void *);
void		pci_intr_disestablish(pci_chipset_tag_t, void *);

/* experimental MSI support */
void *pci_msi_establish(struct pci_attach_args *, int, int (*)(void *), void *);
void pci_msi_disestablish(void *);

/*
 * ALL OF THE FOLLOWING ARE MACHINE-DEPENDENT, AND SHOULD NOT BE USED
 * BY PORTABLE CODE.
 */

/* Extract Bus Number for a host bridge or -1 if unknown. */
int		pchb_get_bus_number(pci_chipset_tag_t, pcitag_t);

/*
 * Section 6.2.4, `Miscellaneous Functions' of the PCI Specification,
 * says that 255 means `unknown' or `no connection' to the interrupt
 * controller on a PC.
 */
#define	X86_PCI_INTERRUPT_LINE_NO_CONNECTION	0xff

void pci_device_foreach(pci_chipset_tag_t, int,
			void (*)(pci_chipset_tag_t, pcitag_t, void*),
			void *);
        
void pci_device_foreach_min(pci_chipset_tag_t, int, int,
			    void (*)(pci_chipset_tag_t, pcitag_t, void*),
			    void *);
        
void pci_bridge_foreach(pci_chipset_tag_t, int, int,
	void (*) (pci_chipset_tag_t, pcitag_t, void *), void *);

void pci_ranges_infer(pci_chipset_tag_t, int, int, bus_addr_t *,
    bus_size_t *, bus_addr_t *, bus_size_t *);

extern prop_dictionary_t pci_rsrc_dict;
prop_dictionary_t pci_rsrc_filter(prop_dictionary_t,
    bool (*)(void *, prop_dictionary_t), void *arg);

#endif /* _X86_PCI_MACHDEP_COMMON_H_ */
