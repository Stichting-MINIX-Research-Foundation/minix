/*	$NetBSD: mpconfig.h,v 1.15 2015/04/27 06:51:40 knakahara Exp $	*/

/*
 * Definitions originally from the mpbios code, but now used for ACPI
 * MP config as well.
 */

#ifndef _X86_MPCONFIG_H_
#define _X86_MPCONFIG_H_

/*
 * XXX
 */
#include <sys/bus.h>
#include <dev/pci/pcivar.h>
#include <machine/pci_machdep.h>

/* 
 * Interrupt types
 */
#define MPS_INTTYPE_INT         0
#define MPS_INTTYPE_NMI         1
#define MPS_INTTYPE_SMI         2
#define MPS_INTTYPE_ExtINT      3
 
#define MPS_INTPO_DEF           0
#define MPS_INTPO_ACTHI         1
#define MPS_INTPO_ACTLO         3
 
#define MPS_INTTR_DEF           0 
#define MPS_INTTR_EDGE          1
#define MPS_INTTR_LEVEL         3

#ifndef _LOCORE

struct mpbios_int;

struct mp_bus
{
	const char *mb_name;		/* XXX bus name */
	int mb_idx;		/* XXX bus index */
	void (*mb_intr_print)(int);
	void (*mb_intr_cfg)(const struct mpbios_int *, uint32_t *);
	struct mp_intr_map *mb_intrs;
	uint32_t mb_data;	/* random bus-specific datum. */
	device_t mb_dev;	/* has been autoconfigured if mb_dev != NULL */
	pcitag_t *mb_pci_bridge_tag;
	pci_chipset_tag_t mb_pci_chipset_tag;
};

struct mp_intr_map
{
	struct mp_intr_map *next;
	struct mp_bus *bus;
	/*
	 * encoding of bus_pin is mp_bus dependant.
	 * for pci, bus_pin = (pci_device_number << 2) | pin
	 * where pin is 0=INTA ... 3=INTD.
	 */
	int bus_pin;
	struct pic *ioapic;	/* NULL for local apic */
	int ioapic_pin;
	intr_handle_t ioapic_ih;	/* int handle, see i82093var.h for encoding */
	int type;		/* from mp spec intr record */
 	int flags;		/* from mp spec intr record */
	uint32_t redir;
	uint32_t cpu_id;
	int global_int;		/* ACPI global interrupt number */
	int sflags;		/* other, software flags (see below) */
	void *linkdev;
	int sourceindex;
};

#define MPI_OVR		0x0001	/* Was overridden by an ACPI OVR */

#if defined(_KERNEL)
extern int mp_verbose;
extern struct mp_bus *mp_busses;
extern struct mp_intr_map *mp_intrs;
extern int mp_nintr;
extern int mp_isa_bus, mp_eisa_bus;
extern int mp_nbus;
int mp_pci_scan(device_t, struct pcibus_attach_args *, cfprint_t);
void mp_pci_childdetached(device_t, device_t);
#endif
#endif

#endif /* _X86_MPCONFIG_H_ */
