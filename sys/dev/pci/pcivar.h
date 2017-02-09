/*	$NetBSD: pcivar.h,v 1.105 2015/10/02 05:22:53 msaitoh Exp $	*/

/*
 * Copyright (c) 1996, 1997 Christopher G. Demetriou.  All rights reserved.
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

#ifndef _DEV_PCI_PCIVAR_H_
#define	_DEV_PCI_PCIVAR_H_

/*
 * Definitions for PCI autoconfiguration.
 *
 * This file describes types and functions which are used for PCI
 * configuration.  Some of this information is machine-specific, and is
 * provided by pci_machdep.h.
 */

#include <sys/device.h>
#include <sys/pmf.h>
#include <sys/bus.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pci_verbose.h>

/*
 * Structures and definitions needed by the machine-dependent header.
 */
struct pcibus_attach_args;
struct pci_attach_args;
struct pci_softc;

#ifdef _KERNEL
/*
 * Machine-dependent definitions.
 */
#include <machine/pci_machdep.h>

enum pci_override_idx {
	  PCI_OVERRIDE_CONF_READ		= __BIT(0)
	, PCI_OVERRIDE_CONF_WRITE		= __BIT(1)
	, PCI_OVERRIDE_INTR_MAP			= __BIT(2)
	, PCI_OVERRIDE_INTR_STRING		= __BIT(3)
	, PCI_OVERRIDE_INTR_EVCNT		= __BIT(4)
	, PCI_OVERRIDE_INTR_ESTABLISH		= __BIT(5)
	, PCI_OVERRIDE_INTR_DISESTABLISH	= __BIT(6)
	, PCI_OVERRIDE_MAKE_TAG			= __BIT(7)
	, PCI_OVERRIDE_DECOMPOSE_TAG		= __BIT(8)
};

/* Only add new fields to the end of this structure! */
struct pci_overrides {
	pcireg_t (*ov_conf_read)(void *, pci_chipset_tag_t, pcitag_t, int);
	void (*ov_conf_write)(void *, pci_chipset_tag_t, pcitag_t, int,
	    pcireg_t);
	int (*ov_intr_map)(void *, const struct pci_attach_args *,
	   pci_intr_handle_t *);
	const char *(*ov_intr_string)(void *, pci_chipset_tag_t,
	    pci_intr_handle_t, char *, size_t);
	const struct evcnt *(*ov_intr_evcnt)(void *, pci_chipset_tag_t,
	    pci_intr_handle_t);
	void *(*ov_intr_establish)(void *, pci_chipset_tag_t, pci_intr_handle_t,
	    int, int (*)(void *), void *);
	void (*ov_intr_disestablish)(void *, pci_chipset_tag_t, void *);
	pcitag_t (*ov_make_tag)(void *, pci_chipset_tag_t, int, int, int);
	void (*ov_decompose_tag)(void *, pci_chipset_tag_t, pcitag_t,
	    int *, int *, int *);
};

/*
 * PCI bus attach arguments.
 */
struct pcibus_attach_args {
	char		*_pba_busname;	/* XXX placeholder */
	bus_space_tag_t pba_iot;	/* pci i/o space tag */
	bus_space_tag_t pba_memt;	/* pci mem space tag */
	bus_dma_tag_t pba_dmat;		/* DMA tag */
	bus_dma_tag_t pba_dmat64;	/* DMA tag */
	pci_chipset_tag_t pba_pc;
	int		pba_flags;	/* flags; see below */

	int		pba_bus;	/* PCI bus number */
	int		pba_sub;	/* pba_bus >= pba_sub: no
					 * buses are subordinate to
					 * pba_bus.
					 *
					 * pba_bus < pba_sub: buses
					 * [pba_bus + 1, pba_sub] are
					 * subordinate to pba_bus.
					 */

	/*
	 * Pointer to the pcitag of our parent bridge.  If there is no
	 * parent bridge, then we assume we are a root bus.
	 */
	pcitag_t	*pba_bridgetag;

	/*
	 * Interrupt swizzling information.  These fields
	 * are only used by secondary busses.
	 */
	u_int		pba_intrswiz;	/* how to swizzle pins */
	pcitag_t	pba_intrtag;	/* intr. appears to come from here */
};

/*
 * This is used by <machine/pci_machdep.h> to access the pba_pc member.  It
 * can't use it directly since pcibus_attach_args has yet to be defined.
 */
static inline pci_chipset_tag_t
pcibus_attach_args_pc(struct pcibus_attach_args *pba)
{
	return pba->pba_pc;
}

/*
 * PCI device attach arguments.
 */
struct pci_attach_args {
	bus_space_tag_t pa_iot;		/* pci i/o space tag */
	bus_space_tag_t pa_memt;	/* pci mem space tag */
	bus_dma_tag_t pa_dmat;		/* DMA tag */
	bus_dma_tag_t pa_dmat64;	/* DMA tag */
	pci_chipset_tag_t pa_pc;
	int		pa_flags;	/* flags; see below */

	u_int		pa_bus;
	u_int		pa_device;
	u_int		pa_function;
	pcitag_t	pa_tag;
	pcireg_t	pa_id, pa_class;

	/*
	 * Interrupt information.
	 *
	 * "Intrline" is used on systems whose firmware puts
	 * the right routing data into the line register in
	 * configuration space.  The rest are used on systems
	 * that do not.
	 */
	u_int		pa_intrswiz;	/* how to swizzle pins if ppb */
	pcitag_t	pa_intrtag;	/* intr. appears to come from here */
	pci_intr_pin_t	pa_intrpin;	/* intr. appears on this pin */
	pci_intr_line_t	pa_intrline;	/* intr. routing information */
	pci_intr_pin_t  pa_rawintrpin; 	/* unswizzled pin */
};

/*
 * This is used by <machine/pci_machdep.h> to access the pa_pc member.  It
 * can't use it directly since pci_attach_args has yet to be defined.
 */
static inline pci_chipset_tag_t
pci_attach_args_pc(const struct pci_attach_args *pa)
{
	return pa->pa_pc;
}

/*
 * Flags given in the bus and device attachment args.
 */
#define	PCI_FLAGS_IO_OKAY	0x01		/* I/O space is okay */
#define	PCI_FLAGS_MEM_OKAY	0x02		/* memory space is okay */
#define	PCI_FLAGS_MRL_OKAY	0x04		/* Memory Read Line okay */
#define	PCI_FLAGS_MRM_OKAY	0x08		/* Memory Read Multiple okay */
#define	PCI_FLAGS_MWI_OKAY	0x10		/* Memory Write and Invalidate
						   okay */
#define	PCI_FLAGS_MSI_OKAY	0x20		/* Message Signaled Interrupts
						   okay */
#define	PCI_FLAGS_MSIX_OKAY	0x40		/* Message Signaled Interrupts
						   (Extended) okay */

/*
 * PCI device 'quirks'.
 *
 * In general strange behaviour which can be handled by a driver (e.g.
 * a bridge's inability to pass a type of access correctly) should be.
 * The quirks table should only contain information which impacts
 * the operation of the MI PCI code and which can't be pushed lower
 * (e.g. because it's unacceptable to require a driver to be present
 * for the information to be known).
 */
struct pci_quirkdata {
	pci_vendor_id_t		vendor;		/* Vendor ID */
	pci_product_id_t	product;	/* Product ID */
	int			quirks;		/* quirks; see below */
};
#define	PCI_QUIRK_MULTIFUNCTION		1
#define	PCI_QUIRK_MONOFUNCTION		2
#define	PCI_QUIRK_SKIP_FUNC(n)		(4 << n)
#define	PCI_QUIRK_SKIP_FUNC0		PCI_QUIRK_SKIP_FUNC(0)
#define	PCI_QUIRK_SKIP_FUNC1		PCI_QUIRK_SKIP_FUNC(1)
#define	PCI_QUIRK_SKIP_FUNC2		PCI_QUIRK_SKIP_FUNC(2)
#define	PCI_QUIRK_SKIP_FUNC3		PCI_QUIRK_SKIP_FUNC(3)
#define	PCI_QUIRK_SKIP_FUNC4		PCI_QUIRK_SKIP_FUNC(4)
#define	PCI_QUIRK_SKIP_FUNC5		PCI_QUIRK_SKIP_FUNC(5)
#define	PCI_QUIRK_SKIP_FUNC6		PCI_QUIRK_SKIP_FUNC(6)
#define	PCI_QUIRK_SKIP_FUNC7		PCI_QUIRK_SKIP_FUNC(7)

struct pci_conf_state {
	pcireg_t reg[16];
};

struct pci_range {
	bus_addr_t		r_offset;
	bus_size_t		r_size;
	int			r_flags;
};

struct pci_child {
	device_t		c_dev;
	bool			c_psok;
	pcireg_t		c_powerstate;
	struct pci_conf_state	c_conf;
	struct pci_range	c_range[8];
};

struct pci_softc {
	device_t sc_dev;
	bus_space_tag_t sc_iot, sc_memt;
	bus_dma_tag_t sc_dmat;
	bus_dma_tag_t sc_dmat64;
	pci_chipset_tag_t sc_pc;
	int sc_bus, sc_maxndevs;
	pcitag_t *sc_bridgetag;
	u_int sc_intrswiz;
	pcitag_t sc_intrtag;
	int sc_flags;
	/* accounting of child devices */
	struct pci_child sc_devices[32*8];
#define PCI_SC_DEVICESC(d, f) sc_devices[(d) * 8 + (f)]
};

extern struct cfdriver pci_cd;

int pcibusprint(void *, const char *);

/*
 * Configuration space access and utility functions.  (Note that most,
 * e.g. make_tag, conf_read, conf_write are declared by pci_machdep.h.)
 */
int	pci_mapreg_probe(pci_chipset_tag_t, pcitag_t, int, pcireg_t *);
pcireg_t pci_mapreg_type(pci_chipset_tag_t, pcitag_t, int);
int	pci_mapreg_info(pci_chipset_tag_t, pcitag_t, int, pcireg_t,
	    bus_addr_t *, bus_size_t *, int *);
int	pci_mapreg_map(const struct pci_attach_args *, int, pcireg_t, int,
	    bus_space_tag_t *, bus_space_handle_t *, bus_addr_t *,
	    bus_size_t *);
int	pci_mapreg_submap(const struct pci_attach_args *, int, pcireg_t, int,
	    bus_size_t, bus_size_t, bus_space_tag_t *, bus_space_handle_t *, 
	    bus_addr_t *, bus_size_t *);


int pci_find_rom(const struct pci_attach_args *, bus_space_tag_t,
	    bus_space_handle_t, bus_size_t,
	    int, bus_space_handle_t *, bus_size_t *);

int	pci_get_capability(pci_chipset_tag_t, pcitag_t, int, int *, pcireg_t *);
int	pci_get_ht_capability(pci_chipset_tag_t, pcitag_t, int, int *,
	    pcireg_t *);
int	pci_get_ext_capability(pci_chipset_tag_t, pcitag_t, int, int *,
	    pcireg_t *);

int	pci_msi_count(pci_chipset_tag_t, pcitag_t);
int	pci_msix_count(pci_chipset_tag_t, pcitag_t);

/*
 * Helper functions for autoconfiguration.
 */
#ifndef PCI_MACHDEP_ENUMERATE_BUS
int	pci_enumerate_bus(struct pci_softc *, const int *,
	    int (*)(const struct pci_attach_args *), struct pci_attach_args *);
#endif
int	pci_probe_device(struct pci_softc *, pcitag_t tag,
	    int (*)(const struct pci_attach_args *),
	    struct pci_attach_args *);
void	pci_devinfo(pcireg_t, pcireg_t, int, char *, size_t);
void	pci_aprint_devinfo_fancy(const struct pci_attach_args *,
				 const char *, const char *, int);
#define pci_aprint_devinfo(pap, naive) \
	pci_aprint_devinfo_fancy(pap, naive, NULL, 0);
void	pci_conf_print(pci_chipset_tag_t, pcitag_t,
	    void (*)(pci_chipset_tag_t, pcitag_t, const pcireg_t *));
const struct pci_quirkdata *
	pci_lookup_quirkdata(pci_vendor_id_t, pci_product_id_t);

/*
 * Helper functions for user access to the PCI bus.
 */
struct proc;
int	pci_devioctl(pci_chipset_tag_t, pcitag_t, u_long, void *,
	    int flag, struct lwp *);

/*
 * Power Management (PCI 2.2)
 */

#define PCI_PWR_D0	0
#define PCI_PWR_D1	1
#define PCI_PWR_D2	2
#define PCI_PWR_D3	3
int	pci_powerstate(pci_chipset_tag_t, pcitag_t, const int *, int *);

/*
 * Vital Product Data (PCI 2.2)
 */
int	pci_vpd_read(pci_chipset_tag_t, pcitag_t, int, int, pcireg_t *);
int	pci_vpd_write(pci_chipset_tag_t, pcitag_t, int, int, pcireg_t *);

/*
 * Misc.
 */
int	pci_find_device(struct pci_attach_args *pa,
			int (*match)(const struct pci_attach_args *));
int	pci_dma64_available(const struct pci_attach_args *);
void	pci_conf_capture(pci_chipset_tag_t, pcitag_t, struct pci_conf_state *);
void	pci_conf_restore(pci_chipset_tag_t, pcitag_t, struct pci_conf_state *);
int	pci_get_powerstate(pci_chipset_tag_t, pcitag_t, pcireg_t *);
int	pci_set_powerstate(pci_chipset_tag_t, pcitag_t, pcireg_t);
int	pci_activate(pci_chipset_tag_t, pcitag_t, device_t,
    int (*)(pci_chipset_tag_t, pcitag_t, device_t, pcireg_t));
int	pci_activate_null(pci_chipset_tag_t, pcitag_t, device_t, pcireg_t);
int	pci_chipset_tag_create(pci_chipset_tag_t, uint64_t,
	                       const struct pci_overrides *,
	                       void *, pci_chipset_tag_t *);
void	pci_chipset_tag_destroy(pci_chipset_tag_t);
int	pci_bus_devorder(pci_chipset_tag_t, int, uint8_t *, int);
void	*pci_intr_establish_xname(pci_chipset_tag_t, pci_intr_handle_t,
				  int, int (*)(void *), void *, const char *);

/*
 * Device abstraction for inheritance by elanpci(4), for example.
 */
int pcimatch(device_t, cfdata_t, void *);
void pciattach(device_t, device_t, void *);
int pcidetach(device_t, int);
void pcidevdetached(device_t, device_t);
int pcirescan(device_t, const char *, const int *);

/*
 * Interrupts.
 */
#define	PCI_INTR_MPSAFE		1

int	pci_intr_setattr(pci_chipset_tag_t, pci_intr_handle_t *, int, uint64_t);

#endif /* _KERNEL */

#endif /* _DEV_PCI_PCIVAR_H_ */
