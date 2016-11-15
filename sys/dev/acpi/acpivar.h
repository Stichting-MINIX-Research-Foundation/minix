/*	$NetBSD: acpivar.h,v 1.73 2011/08/01 11:25:59 jmcneill Exp $	*/

/*
 * Copyright 2001 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
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
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _SYS_DEV_ACPI_ACPIVAR_H
#define _SYS_DEV_ACPI_ACPIVAR_H

/*
 * This file defines the ACPI interface provided to the rest of the
 * kernel, as well as the autoconfiguration structures for ACPI
 * support.
 */

#include <sys/bus.h>
#include <dev/pci/pcivar.h>
#include <dev/isa/isavar.h>

#include <dev/acpi/acpica.h>
#include <dev/acpi/acpi_util.h>

#include <dev/sysmon/sysmonvar.h>

/*
 * This structure is used to attach the ACPI "bus".
 */
struct acpibus_attach_args {
	bus_space_tag_t		 aa_iot;	/* PCI I/O space tag */
	bus_space_tag_t		 aa_memt;	/* PCI MEM space tag */
	pci_chipset_tag_t	 aa_pc;		/* PCI chipset */
	int			 aa_pciflags;	/* PCI bus flags */
	isa_chipset_tag_t	 aa_ic;		/* ISA chipset */
};

/*
 * PCI information for ACPI device nodes that correspond to PCI devices.
 *
 * Remarks:
 *
 *	ap_bus		<= 255
 *	ap_device	<= 31
 *	ap_function	<= 7		or	ap_function == 0xFFFF
 *	ap_downbus	<= 255
 *
 * Validity of some fields depends on the value of ap_flags:
 *
 *	ap_segment				always valid
 *	ap_bus, ap_device, ap_function		valid for PCI devices
 *	ap_downbus				valid for PCI bridges
 *
 * The device and function numbers are encoded in the value returned by
 * _ADR.  A function number of 0xFFFF is used to refer to all the
 * functions on a PCI device (ACPI 4.0a, p. 200).
 */
struct acpi_pci_info {
	uint16_t		 ap_flags;	/* Flags (cf. below) */
	uint16_t		 ap_segment;	/* PCI segment group */
	uint16_t		 ap_bus;	/* PCI bus */
	uint16_t		 ap_device;	/* PCI device */
	uint16_t		 ap_function;	/* PCI function */
	uint16_t		 ap_downbus;	/* PCI bridge downstream bus */
};

/*
 * Flags for PCI information.
 */
#define ACPI_PCI_INFO_DEVICE	__BIT(0)	/* PCI device */
#define ACPI_PCI_INFO_BRIDGE	__BIT(1)	/* PCI bridge */

/*
 * An ACPI device node.
 *
 * Remarks:
 *
 *	ad_device	NULL if no device has attached to the node
 *	ad_root		never NULL
 *	ad_parent	only NULL if root of the tree ("\")
 *	ad_pciinfo	NULL if not a PCI device
 *	ad_wakedev	NULL if no wakeup capabilities
 *	ad_notify	NULL if there is no notify handler
 *	ad_devinfo	never NULL
 *	ad_handle	never NULL
 *
 * Each ACPI device node is associated with its handle. The function
 * acpi_match_node() can be used to get the node structure from a handle.
 */
struct acpi_devnode {
	device_t		 ad_device;	/* Device */
	device_t		 ad_root;	/* Backpointer to acpi_softc */
	struct acpi_devnode	*ad_parent;	/* Backpointer to parent */
	struct acpi_pci_info	*ad_pciinfo;	/* PCI info */
	struct acpi_wakedev	*ad_wakedev;	/* Device wake */
	ACPI_NOTIFY_HANDLER	 ad_notify;	/* Device notify */
	ACPI_DEVICE_INFO	*ad_devinfo;	/* Device info */
	ACPI_HANDLE		 ad_handle;	/* Device handle */
	char			 ad_name[5];	/* Device name */
	uint32_t		 ad_flags;	/* Device flags */
	uint32_t		 ad_type;	/* Device type */
	int			 ad_state;	/* Device power state */

	SIMPLEQ_ENTRY(acpi_devnode)	ad_list;
	SIMPLEQ_ENTRY(acpi_devnode)	ad_child_list;
	SIMPLEQ_HEAD(, acpi_devnode)	ad_child_head;
};

/*
 * ACPI driver capabilities (ad_flags).
 */
#define ACPI_DEVICE_POWER	__BIT(0)	/* Support for D-states  */
#define ACPI_DEVICE_WAKEUP	__BIT(1)	/* Support for wake-up */
#define ACPI_DEVICE_EJECT	__BIT(2)	/* Support for "ejection" */
#define ACPI_DEVICE_DOCK	__BIT(3)	/* Support for docking */

/*
 * Software state of the ACPI subsystem.
 */
struct acpi_softc {
	device_t		 sc_dev;	/* base device info */
	device_t		 sc_apmbus;	/* APM pseudo-bus */
	device_t		 sc_hpet;	/* hpet(4) pseudo-bus */
	device_t		 sc_wdrt;	/* acpiwdrt(4) pseudo-bus */

	struct acpi_devnode	*sc_root;	/* root of the device tree */

	bus_space_tag_t		 sc_iot;	/* PCI I/O space tag */
	bus_space_tag_t		 sc_memt;	/* PCI MEM space tag */
	pci_chipset_tag_t	 sc_pc;		/* PCI chipset tag */
	int			 sc_pciflags;	/* PCI bus flags */
	int			 sc_pci_bus;	/* internal PCI fixup */
	isa_chipset_tag_t	 sc_ic;		/* ISA chipset tag */

	void			*sc_sdhook;	/* shutdown hook */

	int			 sc_quirks;
	int			 sc_sleepstate;
	int			 sc_sleepstates;

	struct sysmon_pswitch	 sc_smpsw_power;
	struct sysmon_pswitch	 sc_smpsw_sleep;

	SIMPLEQ_HEAD(, acpi_devnode)	ad_head;
};

/*
 * acpi_attach_args:
 *
 *	Used to attach a device instance to the acpi "bus".
 */
struct acpi_attach_args {
	struct acpi_devnode *aa_node;	/* ACPI device node */
	bus_space_tag_t aa_iot;		/* PCI I/O space tag */
	bus_space_tag_t aa_memt;	/* PCI MEM space tag */
	pci_chipset_tag_t aa_pc;	/* PCI chipset tag */
	int aa_pciflags;		/* PCI bus flags */
	isa_chipset_tag_t aa_ic;	/* ISA chipset */
};

/*
 * ACPI resources:
 *
 *	acpi_io		I/O ports
 *	acpi_iorange	I/O port range
 *	acpi_mem	memory region
 *	acpi_memrange	memory range
 *	acpi_irq	Interrupt Request
 *	acpi_drq	DMA request
 */
struct acpi_io {
	SIMPLEQ_ENTRY(acpi_io) ar_list;
	int		ar_index;
	uint32_t	ar_base;
	uint32_t	ar_length;
};

struct acpi_iorange {
	SIMPLEQ_ENTRY(acpi_iorange) ar_list;
	int		ar_index;
	uint32_t	ar_low;
	uint32_t	ar_high;
	uint32_t	ar_length;
	uint32_t	ar_align;
};

struct acpi_mem {
	SIMPLEQ_ENTRY(acpi_mem) ar_list;
	int		ar_index;
	uint32_t	ar_base;
	uint32_t	ar_length;
};

struct acpi_memrange {
	SIMPLEQ_ENTRY(acpi_memrange) ar_list;
	int		ar_index;
	uint32_t	ar_low;
	uint32_t	ar_high;
	uint32_t	ar_length;
	uint32_t	ar_align;
};

struct acpi_irq {
	SIMPLEQ_ENTRY(acpi_irq) ar_list;
	int		ar_index;
	uint32_t	ar_irq;
	uint32_t	ar_type;
};

struct acpi_drq {
	SIMPLEQ_ENTRY(acpi_drq) ar_list;
	int		ar_index;
	uint32_t	ar_drq;
};

struct acpi_resources {
	SIMPLEQ_HEAD(, acpi_io) ar_io;
	int ar_nio;

	SIMPLEQ_HEAD(, acpi_iorange) ar_iorange;
	int ar_niorange;

	SIMPLEQ_HEAD(, acpi_mem) ar_mem;
	int ar_nmem;

	SIMPLEQ_HEAD(, acpi_memrange) ar_memrange;
	int ar_nmemrange;

	SIMPLEQ_HEAD(, acpi_irq) ar_irq;
	int ar_nirq;

	SIMPLEQ_HEAD(, acpi_drq) ar_drq;
	int ar_ndrq;
};

/*
 * acpi_resource_parse_ops:
 *
 *	The client of ACPI resources specifies these operations
 *	when the resources are parsed.
 */
struct acpi_resource_parse_ops {
	void	(*init)(device_t, void *, void **);
	void	(*fini)(device_t, void *);

	void	(*ioport)(device_t, void *, uint32_t, uint32_t);
	void	(*iorange)(device_t, void *, uint32_t, uint32_t,
		    uint32_t, uint32_t);

	void	(*memory)(device_t, void *, uint32_t, uint32_t);
	void	(*memrange)(device_t, void *, uint32_t, uint32_t,
		    uint32_t, uint32_t);

	void	(*irq)(device_t, void *, uint32_t, uint32_t);
	void	(*drq)(device_t, void *, uint32_t);

	void	(*start_dep)(device_t, void *, int);
	void	(*end_dep)(device_t, void *);
};

extern struct acpi_softc *acpi_softc;
extern int acpi_active;

extern const struct acpi_resource_parse_ops acpi_resource_parse_ops_default;
extern const struct acpi_resource_parse_ops acpi_resource_parse_ops_quiet;

int		acpi_probe(void);
void		acpi_disable(void);
int		acpi_check(device_t, const char *);

int		acpi_reset(void);

ACPI_PHYSICAL_ADDRESS	acpi_OsGetRootPointer(void);

bool		acpi_register_notify(struct acpi_devnode *,
				     ACPI_NOTIFY_HANDLER);
void		acpi_deregister_notify(struct acpi_devnode *);

ACPI_STATUS	acpi_resource_parse(device_t, ACPI_HANDLE, const char *,
		    void *, const struct acpi_resource_parse_ops *);
void		acpi_resource_print(device_t, struct acpi_resources *);
void		acpi_resource_cleanup(struct acpi_resources *);

void *		acpi_pci_link_devbyhandle(ACPI_HANDLE);
void		acpi_pci_link_add_reference(void *, int, int, int, int);
int		acpi_pci_link_route_interrupt(void *, int, int *, int *, int *);
char *		acpi_pci_link_name(void *);
ACPI_HANDLE	acpi_pci_link_handle(void *);
void		acpi_pci_link_state(void);
void		acpi_pci_link_resume(void);

struct acpi_io		*acpi_res_io(struct acpi_resources *, int);
struct acpi_iorange	*acpi_res_iorange(struct acpi_resources *, int);
struct acpi_mem		*acpi_res_mem(struct acpi_resources *, int);
struct acpi_memrange	*acpi_res_memrange(struct acpi_resources *, int);
struct acpi_irq		*acpi_res_irq(struct acpi_resources *, int);
struct acpi_drq		*acpi_res_drq(struct acpi_resources *, int);

/*
 * Sleep state transition.
 */
void			acpi_enter_sleep_state(int);

/*
 * MADT.
 */
#define ACPI_PLATFORM_INT_PMI	1
#define ACPI_PLATFORM_INT_INIT	2
#define ACPI_PLATFORM_INT_CERR	3

ACPI_STATUS		acpi_madt_map(void);
void			acpi_madt_unmap(void);
void			acpi_madt_walk(ACPI_STATUS (*)(ACPI_SUBTABLE_HEADER *,
				       void *), void *);

/*
 * Quirk handling.
 */
struct acpi_quirk {
	const char	*aq_tabletype;		/* Type of table */
	const char	*aq_oemid;		/* "OemId" field */
	int		 aq_oemrev;		/* "OemRev" field */
	int		 aq_cmpop;		/* "OemRev" comparison */
	const char	*aq_tabid;		/* "TableId */
	int		 aq_quirks;		/* The actual quirk */
};

#define ACPI_QUIRK_BROKEN	0x00000001	/* totally broken */
#define ACPI_QUIRK_BADPCI	0x00000002	/* bad PCI hierarchy */
#define ACPI_QUIRK_BADBBN	0x00000004	/* _BBN broken */
#define ACPI_QUIRK_IRQ0		0x00000008	/* bad 0->2 irq override */
#define ACPI_QUIRK_OLDBIOS	0x00000010	/* BIOS date blacklisted */

int	acpi_find_quirks(void);
int	acpi_quirks_osi_add(const char *);
int	acpi_quirks_osi_del(const char *);

#ifdef ACPI_DEBUG
void	acpi_debug_init(void);
#endif

/*
 * Misc routines with vectors updated by acpiverbose module.
 */
extern void	(*acpi_print_verbose)(struct acpi_softc *);
extern void	(*acpi_print_dev)(const char *);

void		acpi_load_verbose(void);
extern int	acpi_verbose_loaded;

#endif	/* !_SYS_DEV_ACPI_ACPIVAR_H */
