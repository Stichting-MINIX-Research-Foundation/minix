/*	$NetBSD: amd64_mainbus.c,v 1.7 2021/08/07 16:18:41 thorpej Exp $	*/
/*	NetBSD: mainbus.c,v 1.39 2018/12/02 08:19:44 cherry Exp 	*/

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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: amd64_mainbus.c,v 1.7 2021/08/07 16:18:41 thorpej Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/reboot.h>
#include <sys/bus.h>

#include <dev/isa/isavar.h>
#include <dev/pci/pcivar.h>

#include <dev/isa/isareg.h>

#include "pci.h"
#include "isa.h"
#include "isadma.h"
#include "acpica.h"
#include "ipmi.h"

#include "opt_acpi.h"
#include "opt_mpbios.h"
#include "opt_pcifixup.h"

#include <machine/cpuvar.h>
#include <machine/i82093var.h>
#include <machine/mpbiosvar.h>
#include <machine/mpacpi.h>

#if NACPICA > 0
#include <dev/acpi/acpivar.h>
#endif

#if NIPMI > 0
#include <x86/ipmivar.h>
#endif

#if NPCI > 0
#if defined(PCI_BUS_FIXUP)
#include <arch/x86/pci/pci_bus_fixup.h>
#if defined(PCI_ADDR_FIXUP)
#include <arch/x86/pci/pci_addr_fixup.h>
#endif
#endif
#ifdef __HAVE_PCI_MSI_MSIX
#include <arch/x86/pci/msipic.h>
#endif /* __HAVE_PCI_MSI_MSIX */
#endif

/*
 * XXXfvdl ACPI
 */

int	amd64_mainbus_match(device_t, cfdata_t, void *);
void	amd64_mainbus_attach(device_t, device_t, void *);
int	amd64_mainbus_print(void *, const char *);

union amd64_mainbus_attach_args {
	const char *mba_busname;		/* first elem of all */
	struct pcibus_attach_args mba_pba;
	struct isabus_attach_args mba_iba;
	struct cpu_attach_args mba_caa;
#if NACPICA > 0
	struct acpibus_attach_args mba_acpi;
#endif
	struct apic_attach_args aaa_caa;
#if NIPMI > 0
	struct ipmi_attach_args mba_ipmi;
#endif
};

/*
 * This is set when the ISA bus is attached.  If it's not set by the
 * time it's checked below, then mainbus attempts to attach an ISA.
 */
int isa_has_been_seen;
struct x86_isa_chipset x86_isa_chipset;
#if NISA > 0
static const struct isabus_attach_args mba_iba = {
	._iba_busname = "isa",
	.iba_dmat = &isa_bus_dma_tag,
	.iba_ic = &x86_isa_chipset
};
#endif

#if defined(MPBIOS) || NACPICA > 0
struct mp_bus *mp_busses;
int mp_nbus;
struct mp_intr_map *mp_intrs;
int mp_nintr;

int mp_isa_bus = -1;
int mp_eisa_bus = -1;

extern bool acpi_present;
extern bool mpacpi_active;

# ifdef MPVERBOSE
#  if MPVERBOSE > 0
int mp_verbose = MPVERBOSE;
#  else
int mp_verbose = 1;
#  endif
# else
int mp_verbose = 0;
# endif
#endif

/*
 * Probe for the mainbus; always succeeds.
 */
int
amd64_mainbus_match(device_t parent, cfdata_t match, void *aux)
{

	return 1;
}

/*
 * Attach the mainbus.
 */
void
amd64_mainbus_attach(device_t parent, device_t self, void *aux)
{
#if NISA > 0 || NPCI > 0 || NACPICA > 0 || NIPMI > 0
	union amd64_mainbus_attach_args mba;
#endif

#if NISADMA > 0 && NACPICA > 0
	/*
	 * ACPI needs ISA DMA initialized before they start probing.
	 */
	isa_dmainit(&x86_isa_chipset, x86_bus_space_io, &isa_bus_dma_tag,
	    self);
#endif

#if NACPICA > 0
	if (acpi_present) {
		mba.mba_acpi.aa_iot = x86_bus_space_io;
		mba.mba_acpi.aa_memt = x86_bus_space_mem;
		mba.mba_acpi.aa_pc = NULL;
		mba.mba_acpi.aa_pciflags =
		    PCI_FLAGS_IO_OKAY | PCI_FLAGS_MEM_OKAY |
		    PCI_FLAGS_MRL_OKAY | PCI_FLAGS_MRM_OKAY |
		    PCI_FLAGS_MWI_OKAY;
		mba.mba_acpi.aa_ic = &x86_isa_chipset;
		mba.mba_acpi.aa_dmat = &pci_bus_dma_tag;
		mba.mba_acpi.aa_dmat64 = &pci_bus_dma64_tag;
		config_found(self, &mba.mba_acpi, NULL,
		    CFARGS(.iattr = "acpibus"));
	}
#endif

#if NIPMI > 0
	memset(&mba.mba_ipmi, 0, sizeof(mba.mba_ipmi));
	mba.mba_ipmi.iaa_iot = x86_bus_space_io;
	mba.mba_ipmi.iaa_memt = x86_bus_space_mem;
	if (ipmi_probe(&mba.mba_ipmi))
		config_found(self, &mba.mba_ipmi, NULL,
		    CFARGS(.iattr = "ipmibus"));
#endif

#if NPCI > 0
	if (pci_mode_detect() != 0) {
		int npcibus = 0;

		mba.mba_pba.pba_iot = x86_bus_space_io;
		mba.mba_pba.pba_memt = x86_bus_space_mem;
		mba.mba_pba.pba_dmat = &pci_bus_dma_tag;
		mba.mba_pba.pba_dmat64 = &pci_bus_dma64_tag;
		mba.mba_pba.pba_pc = NULL;
		mba.mba_pba.pba_flags =
		    PCI_FLAGS_IO_OKAY | PCI_FLAGS_MEM_OKAY |
		    PCI_FLAGS_MRL_OKAY | PCI_FLAGS_MRM_OKAY |
		    PCI_FLAGS_MWI_OKAY;
		mba.mba_pba.pba_bus = 0;
		mba.mba_pba.pba_bridgetag = NULL;
#if NACPICA > 0 && defined(ACPI_SCANPCI)
		if (npcibus == 0 && mpacpi_active)
			npcibus = mp_pci_scan(self, &mba.mba_pba, pcibusprint);
#endif
#if defined(MPBIOS) && defined(MPBIOS_SCANPCI)
		if (npcibus == 0 && mpbios_scanned != 0)
			npcibus = mp_pci_scan(self, &mba.mba_pba, pcibusprint);
#endif
		if (npcibus == 0)
			config_found(self, &mba.mba_pba, pcibusprint,
			    CFARGS(.iattr = "pcibus"));

#if NACPICA > 0
		if (mp_verbose)
			acpi_pci_link_state();
#endif
	}
#endif

#if NISA > 0
	if (isa_has_been_seen == 0) {
		mba.mba_iba = mba_iba;
		mba.mba_iba.iba_iot = x86_bus_space_io;
		mba.mba_iba.iba_memt = x86_bus_space_mem;
		config_found(self, &mba.mba_iba, isabusprint,
		    CFARGS(.iattr = "isabus"));
	}
#endif

	if (!pmf_device_register(self, NULL, NULL))
		aprint_error_dev(self, "couldn't establish power handler\n");
}

int
amd64_mainbus_print(void *aux, const char *pnp)
{
	union amd64_mainbus_attach_args *mba = aux;

	if (pnp)
		aprint_normal("%s at %s", mba->mba_busname, pnp);
	return UNCONF;
}
