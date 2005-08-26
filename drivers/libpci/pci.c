#define USER_SPACE 1
/*
pci.c

Configure devices on the PCI bus

Created:	Jan 2000 by Philip Homburg <philip@cs.vu.nl>
*/

#include "../drivers.h"
#define	NDEBUG			/* disable assertions */
#include <assert.h>
#include <minix/com.h>
#include <minix/syslib.h>

#include "pci.h"
#include "pci_amd.h"
#include "pci_intel.h"
#include "pci_sis.h"
#include "pci_via.h"
#if __minix_vmd
#include "config.h"
#endif

#if !__minix_vmd
#define irq_mode_pci(irq) ((void)0)
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <minix/sysutil.h>

#define NR_PCIBUS	 4
#define NR_PCIDEV	40

#define PBT_INTEL	 1
#define PBT_PCIBRIDGE	 2

PRIVATE int debug= 0;

PRIVATE struct pcibus
{
	int pb_type;
	int pb_isabridge_dev;
	int pb_isabridge_type;

	int pb_devind;
	int pb_bus;
	u8_t (*pb_rreg8)(int busind, int devind, int port);
	u16_t (*pb_rreg16)(int busind, int devind, int port);
	u32_t (*pb_rreg32)(int busind, int devind, int port);
	void (*pb_wreg16)(int busind, int devind, int port, U16_t value);
	void (*pb_wreg32)(int busind, int devind, int port, u32_t value);
	u16_t (*pb_rsts)(int busind);
	void (*pb_wsts)(int busind, U16_t value);
} pcibus[NR_PCIBUS];
PRIVATE int nr_pcibus= 0;

PRIVATE struct pcidev
{
	u8_t pd_busind;
	u8_t pd_dev;
	u8_t pd_func;
	u8_t pd_baseclass;
	u8_t pd_subclass;
	u8_t pd_infclass;
	u16_t pd_vid;
	u16_t pd_did;
	u8_t pd_inuse;
} pcidev[NR_PCIDEV];
PRIVATE int nr_pcidev= 0;

/* Work around the limitation of the PCI emulation in QEMU 0.7.1 */
PRIVATE int qemu_pci= 0;

FORWARD _PROTOTYPE( void pci_intel_init, (void)				);
FORWARD _PROTOTYPE( void probe_bus, (int busind)			);
FORWARD _PROTOTYPE( int do_isabridge, (int busind)			);
FORWARD _PROTOTYPE( void do_pcibridge, (int busind)			);
FORWARD _PROTOTYPE( int do_piix, (int devind)				);
FORWARD _PROTOTYPE( int do_amd_isabr, (int devind)			);
FORWARD _PROTOTYPE( int do_sis_isabr, (int devind)			);
FORWARD _PROTOTYPE( int do_via_isabr, (int devind)			);
FORWARD _PROTOTYPE( char *pci_vid_name, (U16_t vid)			);
FORWARD _PROTOTYPE( char *pci_baseclass_name, (U8_t baseclass)		);
FORWARD _PROTOTYPE( char *pci_subclass_name, (U8_t baseclass,
					U8_t subclass, U8_t infclass)	);
FORWARD _PROTOTYPE( void ntostr, (unsigned n, char **str, char *end)	);
FORWARD _PROTOTYPE( u16_t pci_attr_rsts, (int devind)			);
FORWARD _PROTOTYPE( void pci_attr_wsts, (int devind, U16_t value)	);
FORWARD _PROTOTYPE( u16_t pcibr_intel_rsts, (int busind)		);
FORWARD _PROTOTYPE( void pcibr_intel_wsts, (int busind, U16_t value)	);
FORWARD _PROTOTYPE( u16_t pcibr_via_rsts, (int busind)			);
FORWARD _PROTOTYPE( void pcibr_via_wsts, (int busind, U16_t value)	);
FORWARD _PROTOTYPE( u8_t pcii_rreg8, (int busind, int devind, int port)	);
FORWARD _PROTOTYPE( u16_t pcii_rreg16, (int busind, int devind,
							int port)	);
FORWARD _PROTOTYPE( u32_t pcii_rreg32, (int busind, int devind,
							int port)	);
FORWARD _PROTOTYPE( void pcii_wreg16, (int busind, int devind, int port,
							U16_t value)	);
FORWARD _PROTOTYPE( void pcii_wreg32, (int busind, int devind, int port,
							u32_t value)	);
FORWARD _PROTOTYPE( u16_t pcii_rsts, (int busind)			);
FORWARD _PROTOTYPE( void pcii_wsts, (int busind, U16_t value)		);

/*===========================================================================*
 *			helper functions for I/O			     *
 *===========================================================================*/
PUBLIC unsigned pci_inb(U16_t port) {
	U8_t value;
	int s;
	if ((s=sys_inb(port, &value)) !=OK)
		printf("PCI: warning, sys_inb failed: %d\n", s);
	return value;
}
PUBLIC unsigned pci_inw(U16_t port) {
	U16_t value;
	int s;
	if ((s=sys_inw(port, &value)) !=OK)
		printf("PCI: warning, sys_inw failed: %d\n", s);
	return value;
}
PUBLIC unsigned pci_inl(U16_t port) {
	U32_t value;
	int s;
	if ((s=sys_inl(port, &value)) !=OK)
		printf("PCI: warning, sys_inl failed: %d\n", s);
	return value;
}
PUBLIC void pci_outb(U16_t port, U8_t value) {
	int s;
	if ((s=sys_outb(port, value)) !=OK)
		printf("PCI: warning, sys_outb failed: %d\n", s);
}
PUBLIC void pci_outw(U16_t port, U16_t value) {
	int s;
	if ((s=sys_outw(port, value)) !=OK)
		printf("PCI: warning, sys_outw failed: %d\n", s);
}
PUBLIC void pci_outl(U16_t port, U32_t value) {
	int s;
	if ((s=sys_outl(port, value)) !=OK)
		printf("PCI: warning, sys_outl failed: %d\n", s);
}

/*===========================================================================*
 *				pci_init				     *
 *===========================================================================*/
PUBLIC void pci_init()
{
	static int first_time= 1;

	long v;

	if (!first_time)
		return;

	v= 0;
	env_parse("qemu_pci", "d", 0, &v, 0, 1);
	qemu_pci= v;

	v= 0;
	env_parse("pci_debug", "d", 0, &v, 0, 1);
	debug= v;

	/* We don't expect to interrupted */
	assert(first_time == 1);
	first_time= -1;

	/* Only Intel (compatible) PCI controllers are supported at the
	 * moment.
	 */
	pci_intel_init();

	first_time= 0;
}

/*===========================================================================*
 *				pci_find_dev				     *
 *===========================================================================*/
PUBLIC int pci_find_dev(bus, dev, func, devindp)
u8_t bus;
u8_t dev;
u8_t func;
int *devindp;
{
	int devind;

	for (devind= 0; devind < nr_pcidev; devind++)
	{
		if (pcidev[devind].pd_busind == bus &&
			pcidev[devind].pd_dev == dev &&
			pcidev[devind].pd_func == func)
		{
			break;
		}
	}
	if (devind >= nr_pcidev)
		return 0;
	if (pcidev[devind].pd_inuse)
		return 0;
	*devindp= devind;
	return 1;
}

/*===========================================================================*
 *				pci_first_dev				     *
 *===========================================================================*/
PUBLIC int pci_first_dev(devindp, vidp, didp)
int *devindp;
u16_t *vidp;
u16_t *didp;
{
	int devind;

	for (devind= 0; devind < nr_pcidev; devind++)
	{
		if (!pcidev[devind].pd_inuse)
			break;
	}
	if (devind >= nr_pcidev)
		return 0;
	*devindp= devind;
	*vidp= pcidev[devind].pd_vid;
	*didp= pcidev[devind].pd_did;
	return 1;
}

/*===========================================================================*
 *				pci_next_dev				     *
 *===========================================================================*/
PUBLIC int pci_next_dev(devindp, vidp, didp)
int *devindp;
u16_t *vidp;
u16_t *didp;
{
	int devind;

	for (devind= *devindp+1; devind < nr_pcidev; devind++)
	{
		if (!pcidev[devind].pd_inuse)
			break;
	}
	if (devind >= nr_pcidev)
		return 0;
	*devindp= devind;
	*vidp= pcidev[devind].pd_vid;
	*didp= pcidev[devind].pd_did;
	return 1;
}

/*===========================================================================*
 *				pci_reserve				     *
 *===========================================================================*/
PUBLIC void pci_reserve(devind)
int devind;
{
	assert(devind <= nr_pcidev);
	assert(!pcidev[devind].pd_inuse);
	pcidev[devind].pd_inuse= 1;
}

/*===========================================================================*
 *				pci_ids					     *
 *===========================================================================*/
PUBLIC void pci_ids(devind, vidp, didp)
int devind;
u16_t *vidp;
u16_t *didp;
{
	assert(devind <= nr_pcidev);
	*vidp= pcidev[devind].pd_vid;
	*didp= pcidev[devind].pd_did;
}

/*===========================================================================*
 *				pci_slot_name				     *
 *===========================================================================*/
PUBLIC char *pci_slot_name(devind)
int devind;
{
	static char label[]= "ddd.ddd.ddd";
	char *end;
	char *p;

	p= label;
	end= label+sizeof(label);

	ntostr(pcidev[devind].pd_busind, &p, end);
	*p++= '.';

	ntostr(pcidev[devind].pd_dev, &p, end);
	*p++= '.';

	ntostr(pcidev[devind].pd_func, &p, end);

	return label;
}

/*===========================================================================*
 *				pci_dev_name				     *
 *===========================================================================*/
PUBLIC char *pci_dev_name(vid, did)
u16_t vid;
u16_t did;
{
	int i;

	for (i= 0; pci_device_table[i].name; i++)
	{
		if (pci_device_table[i].vid == vid &&
			pci_device_table[i].did == did)
		{
			return pci_device_table[i].name;
		}
	}
	return NULL;
}

/*===========================================================================*
 *				pci_attr_r8				     *
 *===========================================================================*/
PUBLIC u8_t pci_attr_r8(devind, port)
int devind;
int port;
{
	int busind;

	busind= pcidev[devind].pd_busind;
	return pcibus[busind].pb_rreg8(busind, devind, port);
}

/*===========================================================================*
 *				pci_attr_r16				     *
 *===========================================================================*/
PUBLIC u16_t pci_attr_r16(devind, port)
int devind;
int port;
{
	int busind;

	busind= pcidev[devind].pd_busind;
	return pcibus[busind].pb_rreg16(busind, devind, port);
}

/*===========================================================================*
 *				pci_attr_r32				     *
 *===========================================================================*/
PUBLIC u32_t pci_attr_r32(devind, port)
int devind;
int port;
{
	int busind;

	busind= pcidev[devind].pd_busind;
	return pcibus[busind].pb_rreg32(busind, devind, port);
}

/*===========================================================================*
 *				pci_attr_w16				     *
 *===========================================================================*/
PUBLIC void pci_attr_w16(devind, port, value)
int devind;
int port;
u16_t value;
{
	int busind;

	busind= pcidev[devind].pd_busind;
	pcibus[busind].pb_wreg16(busind, devind, port, value);
}

/*===========================================================================*
 *				pci_attr_w32				     *
 *===========================================================================*/
PUBLIC void pci_attr_w32(devind, port, value)
int devind;
int port;
u32_t value;
{
	int busind;

	busind= pcidev[devind].pd_busind;
	pcibus[busind].pb_wreg32(busind, devind, port, value);
}

/*===========================================================================*
 *				pci_intel_init				     *
 *===========================================================================*/
PRIVATE void pci_intel_init()
{
	/* Try to detect a know PCI controller. Read the Vendor ID and
	 * the Device ID for function 0 of device 0.
	 * Two times the value 0xffff suggests a system without a (compatible)
	 * PCI controller. Only controllers with values listed in the table
	 * pci_intel_ctrl are actually used.
	 */
	u32_t bus, dev, func;
	u16_t vid, did;
	int s, i, r, busind;
	char *dstr;

	bus= 0;
	dev= 0;
	func= 0;

	vid= PCII_RREG16_(bus, dev, func, PCI_VID);
	did= PCII_RREG16_(bus, dev, func, PCI_DID);
#if USER_SPACE
	if (OK != (s=sys_outl(PCII_CONFADD, PCII_UNSEL)))
		printf("PCI: warning, sys_outl failed: %d\n", s);
#else
	outl(PCII_CONFADD, PCII_UNSEL);
#endif

	if (vid == 0xffff && did == 0xffff)
		return;	/* Nothing here */

	for (i= 0; pci_intel_ctrl[i].vid; i++)
	{
		if (pci_intel_ctrl[i].vid == vid &&
			pci_intel_ctrl[i].did == did)
		{
			break;
		}
	}

	if (!pci_intel_ctrl[i].vid)
	{
		printf("pci_intel_init (warning): unknown PCI-controller:\n"
			"\tvendor %04X (%s), device %04X\n",
			vid, pci_vid_name(vid), did);
	}

	if (nr_pcibus >= NR_PCIBUS)
		panic("PCI","too many PCI busses", nr_pcibus);
	busind= nr_pcibus;
	nr_pcibus++;
	pcibus[busind].pb_type= PBT_INTEL;
	pcibus[busind].pb_isabridge_dev= -1;
	pcibus[busind].pb_isabridge_type= 0;
	pcibus[busind].pb_devind= -1;
	pcibus[busind].pb_bus= 0;
	pcibus[busind].pb_rreg8= pcii_rreg8;
	pcibus[busind].pb_rreg16= pcii_rreg16;
	pcibus[busind].pb_rreg32= pcii_rreg32;
	pcibus[busind].pb_wreg16= pcii_wreg16;
	pcibus[busind].pb_wreg32= pcii_wreg32;
	pcibus[busind].pb_rsts= pcii_rsts;
	pcibus[busind].pb_wsts= pcii_wsts;

	dstr= pci_dev_name(vid, did);
	if (!dstr)
		dstr= "unknown device";
	if (debug)
	{
		printf("pci_intel_init: %s (%04X/%04X)\n",
			dstr, vid, did);
	}

	probe_bus(busind);

	r= do_isabridge(busind);
	if (r != OK)
	{
		/* Disable all devices for this bus */
		for (i= 0; i<nr_pcidev; i++)
		{
			if (pcidev[i].pd_busind != busind)
				continue;
			pcidev[i].pd_inuse= 1;
		}
		return;
	}

	/* Look for PCI bridges (for AGP) */
	do_pcibridge(busind);
}

/*===========================================================================*
 *				probe_bus				     *
 *===========================================================================*/
PRIVATE void probe_bus(busind)
int busind;
{
	u32_t dev, func;
	u16_t vid, did, sts;
	u8_t headt;
	u8_t baseclass, subclass, infclass;
	int devind;
	char *s, *dstr;

#if DEBUG
printf("probe_bus(%d)\n", busind);
#endif
	if (nr_pcidev >= NR_PCIDEV)
		panic("PCI","too many PCI devices", nr_pcidev);
	devind= nr_pcidev;

	for (dev= 0; dev<32; dev++)
	{

		for (func= 0; func < 8; func++)
		{
			pcidev[devind].pd_busind= busind;
			pcidev[devind].pd_dev= dev;
			pcidev[devind].pd_func= func;

			pci_attr_wsts(devind, 
				PSR_SSE|PSR_RMAS|PSR_RTAS);
			vid= pci_attr_r16(devind, PCI_VID);
			did= pci_attr_r16(devind, PCI_DID);
			headt= pci_attr_r8(devind, PCI_HEADT);
			sts= pci_attr_rsts(devind);

			if (vid == NO_VID)
				break;	/* Nothing here */

			if (sts & (PSR_SSE|PSR_RMAS|PSR_RTAS))
			{
				if (qemu_pci)
				{
					printf(
			"pci: ignoring bad value 0x%x in sts for QEMU\n",
					sts & (PSR_SSE|PSR_RMAS|PSR_RTAS));
				}
				else
					break;
			}

			dstr= pci_dev_name(vid, did);
			if (debug)
			{
				if (dstr)
				{
					printf("%d.%lu.%lu: %s (%04X/%04X)\n",
						busind, (unsigned long)dev,
						(unsigned long)func, dstr,
						vid, did);
				}
				else
				{
					printf(
		"%d.%lu.%lu: Unknown device, vendor %04X (%s), device %04X\n",
						busind, (unsigned long)dev,
						(unsigned long)func, vid,
						pci_vid_name(vid), did);
				}
			}

			baseclass= pci_attr_r8(devind, PCI_BCR);
			subclass= pci_attr_r8(devind, PCI_SCR);
			infclass= pci_attr_r8(devind, PCI_PIFR);
			s= pci_subclass_name(baseclass, subclass, infclass);
			if (!s)
				s= pci_baseclass_name(baseclass);
			{
				if (!s)
					s= "(unknown class)";
			}
			if (debug)
			{
				printf("\tclass %s (%X/%X/%X)\n", s,
					baseclass, subclass, infclass);
			}

			devind= nr_pcidev;
			nr_pcidev++;
			pcidev[devind].pd_baseclass= baseclass;
			pcidev[devind].pd_subclass= subclass;
			pcidev[devind].pd_infclass= infclass;
			pcidev[devind].pd_vid= vid;
			pcidev[devind].pd_did= did;
			pcidev[devind].pd_inuse= 0;

			if (nr_pcidev >= NR_PCIDEV)
			  panic("PCI","too many PCI devices", nr_pcidev);
			devind= nr_pcidev;

			if (func == 0 && !(headt & PHT_MULTIFUNC))
				break;
		}
	}
}

/*===========================================================================*
 *				do_isabridge				     *
 *===========================================================================*/
PRIVATE int do_isabridge(busind)
int busind;
{
	int unknown_bridge= -1;
	int bridge_dev= -1;
	int i, j, r, type;
	u16_t vid, did;
	char *dstr;

	j= 0;	/* lint */
	vid= did= 0;	/* lint */
	for (i= 0; i< nr_pcidev; i++)
	{
		if (pcidev[i].pd_busind != busind)
			continue;
		if (pcidev[i].pd_baseclass == 0x06 &&
			pcidev[i].pd_subclass == 0x01 &&
			pcidev[i].pd_infclass == 0x00)
		{
			/* ISA bridge. Report if no supported bridge is
			 * found.
			 */
			unknown_bridge= i;
		}

		vid= pcidev[i].pd_vid;
		did= pcidev[i].pd_did;
		for (j= 0; pci_isabridge[j].vid != 0; j++)
		{
			if (pci_isabridge[j].vid != vid)
				continue;
			if (pci_isabridge[j].did != did)
				continue;
			if (pci_isabridge[j].checkclass &&
				unknown_bridge != i)
			{
				/* This part of multifunction device is
				 * not the bridge.
				 */
				continue;
			}
			break;
		}
		if (pci_isabridge[j].vid)
		{
			bridge_dev= i;
			break;
		}
	}

	if (bridge_dev != -1)
	{
		dstr= pci_dev_name(vid, did);
		if (!dstr)
			dstr= "unknown device";
		if (debug)
		{
			printf("found ISA bridge (%04X/%04X) %s\n",
				vid, did, dstr);
		}
		pcibus[busind].pb_isabridge_dev= bridge_dev;
		type= pci_isabridge[j].type;
		pcibus[busind].pb_isabridge_type= type;
		switch(type)
		{
		case PCI_IB_PIIX:
			r= do_piix(bridge_dev);
			break;
		case PCI_IB_VIA:
			r= do_via_isabr(bridge_dev);
			break;
		case PCI_IB_AMD:
			r= do_amd_isabr(bridge_dev);
			break;
		case PCI_IB_SIS:
			r= do_sis_isabr(bridge_dev);
			break;
		default:
			panic("PCI","unknown ISA bridge type", type);
		}
		return r;
	}

	if (unknown_bridge == -1)
	{
		printf("(warning) no ISA bridge found on bus %d", busind);
		return 0;
	}
	printf("(warning) unsupported ISA bridge %04X/%04X for bus %d\n",
		pcidev[unknown_bridge].pd_vid,
		pcidev[unknown_bridge].pd_did,
		busind);
	return 0;
}

/*===========================================================================*
 *				do_pcibridge				     *
 *===========================================================================*/
PRIVATE void do_pcibridge(busind)
int busind;
{
	int devind, i;
	int ind, type;
	u16_t vid, did;
	u8_t sbusn, baseclass, subclass, infclass;
	u32_t t3;

	vid= did= 0;	/* lint */
	for (devind= 0; devind< nr_pcidev; devind++)
	{
		if (pcidev[devind].pd_busind != busind)
			continue;

		vid= pcidev[devind].pd_vid;
		did= pcidev[devind].pd_did;
		for (i= 0; pci_pcibridge[i].vid != 0; i++)
		{
			if (pci_pcibridge[i].vid != vid)
				continue;
			if (pci_pcibridge[i].did != did)
				continue;
			break;
		}
		if (pci_pcibridge[i].vid == 0)
		{
			if (debug)
			{
				/* Report unsupported bridges */
				baseclass= pci_attr_r8(devind, PCI_BCR);
				subclass= pci_attr_r8(devind, PCI_SCR);
				infclass= pci_attr_r8(devind, PCI_PIFR);
				t3= ((baseclass << 16) | (subclass << 8) |
					infclass);
				if (t3 != PCI_T3_PCI2PCI &&
					t3 != PCI_T3_PCI2PCI_SUBTR)
				{
					/* No a PCI-to-PCI bridge */
					continue;
				}
				printf(
			"Ignoring unknown PCI-to-PCI bridge: %04X/%04X\n",
					vid, did);
			}
			continue;
		}
		type= pci_pcibridge[i].type;

		if (debug)
			printf("PCI-to-PCI bridge: %04X/%04X\n", vid, did);

		/* Assume that the BIOS initialized the secondary bus
		 * number.
		 */
		sbusn= pci_attr_r8(devind, PPB_SBUSN);
#if DEBUG
		printf("sbusn = %d\n", sbusn);
#endif

		if (nr_pcibus >= NR_PCIBUS)
			panic("PCI","too many PCI busses", nr_pcibus);
		ind= nr_pcibus;
		nr_pcibus++;
		pcibus[ind].pb_type= PBT_PCIBRIDGE;
		pcibus[ind].pb_isabridge_dev= -1;
		pcibus[ind].pb_isabridge_type= 0;
		pcibus[ind].pb_devind= devind;
		pcibus[ind].pb_bus= sbusn;
		pcibus[ind].pb_rreg8= pcibus[busind].pb_rreg8;
		pcibus[ind].pb_rreg16= pcibus[busind].pb_rreg16;
		pcibus[ind].pb_rreg32= pcibus[busind].pb_rreg32;
		pcibus[ind].pb_wreg16= pcibus[busind].pb_wreg16;
		pcibus[ind].pb_wreg32= pcibus[busind].pb_wreg32;
		switch(type)
		{
		case PCI_PCIB_INTEL:
		case PCI_AGPB_INTEL:
			pcibus[ind].pb_rsts= pcibr_intel_rsts;
			pcibus[ind].pb_wsts= pcibr_intel_wsts;
			break;
		case PCI_AGPB_VIA:
			pcibus[ind].pb_rsts= pcibr_via_rsts;
			pcibus[ind].pb_wsts= pcibr_via_wsts;
			break;
		default:
		    panic("PCI","unknown PCI-PCI bridge type", type);
		}

		probe_bus(ind);
	}
}

/*===========================================================================*
 *				do_piix					     *
 *===========================================================================*/
PRIVATE int do_piix(devind)
int devind;
{
	int i, s, dev, func, irqrc, irq;
	u16_t elcr1, elcr2, elcr;

#if DEBUG
	printf("in piix\n");
#endif
	dev= pcidev[devind].pd_dev;
	func= pcidev[devind].pd_func;
#if USER_SPACE
	if (OK != (s=sys_inb(PIIX_ELCR1, &elcr1)))
		printf("Warning, sys_inb failed: %d\n", s);
	if (OK != (s=sys_inb(PIIX_ELCR2, &elcr2)))
		printf("Warning, sys_inb failed: %d\n", s);
#else
	elcr1= inb(PIIX_ELCR1);
	elcr2= inb(PIIX_ELCR2);
#endif
	elcr= elcr1 | (elcr2 << 8);
	for (i= 0; i<4; i++)
	{
		irqrc= pci_attr_r8(devind, PIIX_PIRQRCA+i);
		if (irqrc & PIIX_IRQ_DI)
		{
			if (debug)
				printf("INT%c: disabled\n", 'A'+i);
		}
		else
		{
			irq= irqrc & PIIX_IRQ_MASK;
			if (debug)
				printf("INT%c: %d\n", 'A'+i, irq);
			if (!(elcr & (1 << irq)))
			{
				if (debug)
				{
					printf(
				"(warning) IRQ %d is not level triggered\n", 
						irq);
				}
			}
			irq_mode_pci(irq);
		}
	}
	return 0;
}

/*===========================================================================*
 *				do_amd_isabr				     *
 *===========================================================================*/
PRIVATE int do_amd_isabr(devind)
int devind;
{
	int i, bus, dev, func, xdevind, irq, edge;
	u8_t levmask;
	u16_t pciirq;

	/* Find required function */
	func= AMD_ISABR_FUNC;
	bus= pcidev[devind].pd_busind;
	dev= pcidev[devind].pd_dev;

	/* Fake a device with the required function */
	if (nr_pcidev >= NR_PCIDEV)
		panic("PCI","too many PCI devices", nr_pcidev);
	xdevind= nr_pcidev;
	pcidev[xdevind].pd_busind= bus;
	pcidev[xdevind].pd_dev= dev;
	pcidev[xdevind].pd_func= func;
	pcidev[xdevind].pd_inuse= 1;
	nr_pcidev++;

	levmask= pci_attr_r8(xdevind, AMD_ISABR_PCIIRQ_LEV);
	pciirq= pci_attr_r16(xdevind, AMD_ISABR_PCIIRQ_ROUTE);
	for (i= 0; i<4; i++)
	{
		edge= (levmask >> i) & 1;
		irq= (pciirq >> (4*i)) & 0xf;
		if (!irq)
		{
			if (debug)
				printf("INT%c: disabled\n", 'A'+i);
		}
		else
		{
			if (debug)
				printf("INT%c: %d\n", 'A'+i, irq);
			if (edge && debug)
			{
				printf(
				"(warning) IRQ %d is not level triggered\n",
					irq);
			}
			irq_mode_pci(irq);
		}
	}
	nr_pcidev--;
	return 0;
}

/*===========================================================================*
 *				do_sis_isabr				     *
 *===========================================================================*/
PRIVATE int do_sis_isabr(devind)
int devind;
{
	int i, dev, func, irq;

	dev= pcidev[devind].pd_dev;
	func= pcidev[devind].pd_func;
	irq= 0;	/* lint */
	for (i= 0; i<4; i++)
	{
		irq= pci_attr_r8(devind, SIS_ISABR_IRQ_A+i);
		if (irq & SIS_IRQ_DISABLED)
		{
			if (debug)
				printf("INT%c: disabled\n", 'A'+i);
		}
		else
		{
			irq &= SIS_IRQ_MASK;
			if (debug)
				printf("INT%c: %d\n", 'A'+i, irq);
			irq_mode_pci(irq);
		}
	}
	return 0;
}

/*===========================================================================*
 *				do_via_isabr				     *
 *===========================================================================*/
PRIVATE int do_via_isabr(devind)
int devind;
{
	int i, dev, func, irq, edge;
	u8_t levmask;

	dev= pcidev[devind].pd_dev;
	func= pcidev[devind].pd_func;
	levmask= pci_attr_r8(devind, VIA_ISABR_EL);
	irq= 0;	/* lint */
	edge= 0; /* lint */
	for (i= 0; i<4; i++)
	{
		switch(i)
		{
		case 0:
			edge= (levmask & VIA_ISABR_EL_INTA);
			irq= pci_attr_r8(devind, VIA_ISABR_IRQ_R2) >> 4;
			break;
		case 1:
			edge= (levmask & VIA_ISABR_EL_INTB);
			irq= pci_attr_r8(devind, VIA_ISABR_IRQ_R2);
			break;
		case 2:
			edge= (levmask & VIA_ISABR_EL_INTC);
			irq= pci_attr_r8(devind, VIA_ISABR_IRQ_R3) >> 4;
			break;
		case 3:
			edge= (levmask & VIA_ISABR_EL_INTD);
			irq= pci_attr_r8(devind, VIA_ISABR_IRQ_R1) >> 4;
			break;
		default:
			assert(0);
		}
		irq &= 0xf;
		if (!irq)
		{
			if (debug)
				printf("INT%c: disabled\n", 'A'+i);
		}
		else
		{
			if (debug)
				printf("INT%c: %d\n", 'A'+i, irq);
			if (edge && debug)
			{
				printf(
				"(warning) IRQ %d is not level triggered\n",
					irq);
			}
			irq_mode_pci(irq);
		}
	}
	return 0;
}

/*===========================================================================*
 *				pci_vid_name				     *
 *===========================================================================*/
PRIVATE char *pci_vid_name(vid)
u16_t vid;
{
	int i;

	for (i= 0; pci_vendor_table[i].name; i++)
	{
		if (pci_vendor_table[i].vid == vid)
			return pci_vendor_table[i].name;
	}
	return "unknown";
}

/*===========================================================================*
 *				pci_baseclass_name			     *
 *===========================================================================*/
PRIVATE char *pci_baseclass_name(baseclass)
u8_t baseclass;
{
	int i;

	for (i= 0; pci_baseclass_table[i].name; i++)
	{
		if (pci_baseclass_table[i].baseclass == baseclass)
			return pci_baseclass_table[i].name;
	}
	return NULL;
}

/*===========================================================================*
 *				pci_subclass_name			     *
 *===========================================================================*/
PRIVATE char *pci_subclass_name(baseclass, subclass, infclass)
u8_t baseclass;
u8_t subclass;
u8_t infclass;
{
	int i;

	for (i= 0; pci_subclass_table[i].name; i++)
	{
		if (pci_subclass_table[i].baseclass != baseclass)
			continue;
		if (pci_subclass_table[i].subclass != subclass)
			continue;
		if (pci_subclass_table[i].infclass != infclass &&
			pci_subclass_table[i].infclass != (u16_t)-1)
		{
			continue;
		}
		return pci_subclass_table[i].name;
	}
	return NULL;
}

/*===========================================================================*
 *				ntostr					     *
 *===========================================================================*/
PRIVATE void ntostr(n, str, end)
unsigned n;
char **str;
char *end;
{
	char tmpstr[20];
	int i;

	if (n == 0)
	{
		tmpstr[0]= '0';
		i= 1;
	}
	else
	{
		for (i= 0; n; i++)
		{
			tmpstr[i]= '0' + (n%10);
			n /= 10;
		}
	}
	for (; i>0; i--)
	{
		if (*str == end)
		{
			break;
		}
		**str= tmpstr[i-1];
		(*str)++;
	}
	if (*str == end)	
		end[-1]= '\0';
	else
		**str= '\0';
}

/*===========================================================================*
 *				pci_attr_rsts				     *
 *===========================================================================*/
PRIVATE u16_t pci_attr_rsts(devind)
int devind;
{
	int busind;

	busind= pcidev[devind].pd_busind;
	return pcibus[busind].pb_rsts(busind);
}
				

/*===========================================================================*
 *				pcibr_intel_rsts			     *
 *===========================================================================*/
PRIVATE u16_t pcibr_intel_rsts(busind)
int busind;
{
	int devind;
	devind= pcibus[busind].pb_devind;

	return pci_attr_r16(devind, PPB_SSTS);
}

/*===========================================================================*
 *				pcibr_intel_wsts			     *
 *===========================================================================*/
PRIVATE void pcibr_intel_wsts(busind, value)
int busind;
u16_t value;
{
	int devind;
	devind= pcibus[busind].pb_devind;

#if 0
	printf("pcibr_intel_wsts(%d, 0x%X), devind= %d\n", 
		busind, value, devind);
#endif
	pci_attr_w16(devind, PPB_SSTS, value);
}

/*===========================================================================*
 *				pcibr_via_rsts				     *
 *===========================================================================*/
PRIVATE u16_t pcibr_via_rsts(busind)
int busind;
{
	int devind;
	devind= pcibus[busind].pb_devind;

	return 0;
}

/*===========================================================================*
 *				pcibr_via_wsts				     *
 *===========================================================================*/
PRIVATE void pcibr_via_wsts(busind, value)
int busind;
u16_t value;
{
	int devind;
	devind= pcibus[busind].pb_devind;

#if 0
	printf("pcibr_via_wsts(%d, 0x%X), devind= %d (not implemented)\n", 
		busind, value, devind);
#endif
}

/*===========================================================================*
 *				pci_attr_wsts				     *
 *===========================================================================*/
PRIVATE void pci_attr_wsts(devind, value)
int devind;
u16_t value;
{
	int busind;

	busind= pcidev[devind].pd_busind;
	pcibus[busind].pb_wsts(busind, value);
}
				

/*===========================================================================*
 *				pcii_rreg8				     *
 *===========================================================================*/
PRIVATE u8_t pcii_rreg8(busind, devind, port)
int busind;
int devind;
int port;
{
	u8_t v;
	int s;

	v= PCII_RREG8_(pcibus[busind].pb_bus, 
		pcidev[devind].pd_dev, pcidev[devind].pd_func,
		port);
#if USER_SPACE
	if (OK != (s=sys_outl(PCII_CONFADD, PCII_UNSEL)))
		printf("PCI: warning, sys_outl failed: %d\n", s);
#else
	outl(PCII_CONFADD, PCII_UNSEL);
#endif
#if 0
	printf("pcii_rreg8(%d, %d, 0x%X): %d.%d.%d= 0x%X\n",
		busind, devind, port,
		pcibus[busind].pb_bus, pcidev[devind].pd_dev,
		pcidev[devind].pd_func, v);
#endif
	return v;
}

/*===========================================================================*
 *				pcii_rreg16				     *
 *===========================================================================*/
PRIVATE u16_t pcii_rreg16(busind, devind, port)
int busind;
int devind;
int port;
{
	u16_t v;
	int s;

	v= PCII_RREG16_(pcibus[busind].pb_bus, 
		pcidev[devind].pd_dev, pcidev[devind].pd_func,
		port);
#if USER_SPACE
	if (OK != (s=sys_outl(PCII_CONFADD, PCII_UNSEL)))
		printf("PCI: warning, sys_outl failed: %d\n");
#else
	outl(PCII_CONFADD, PCII_UNSEL);
#endif
#if 0
	printf("pcii_rreg16(%d, %d, 0x%X): %d.%d.%d= 0x%X\n",
		busind, devind, port,
		pcibus[busind].pb_bus, pcidev[devind].pd_dev,
		pcidev[devind].pd_func, v);
#endif
	return v;
}

/*===========================================================================*
 *				pcii_rreg32				     *
 *===========================================================================*/
PRIVATE u32_t pcii_rreg32(busind, devind, port)
int busind;
int devind;
int port;
{
	u32_t v;
	int s;

	v= PCII_RREG32_(pcibus[busind].pb_bus, 
		pcidev[devind].pd_dev, pcidev[devind].pd_func,
		port);
#if USER_SPACE
	if (OK != (s=sys_outl(PCII_CONFADD, PCII_UNSEL)))
		printf("PCI: warning, sys_outl failed: %d\n", s);
#else
	outl(PCII_CONFADD, PCII_UNSEL);
#endif
#if 0
	printf("pcii_rreg32(%d, %d, 0x%X): %d.%d.%d= 0x%X\n",
		busind, devind, port,
		pcibus[busind].pb_bus, pcidev[devind].pd_dev,
		pcidev[devind].pd_func, v);
#endif
	return v;
}

/*===========================================================================*
 *				pcii_wreg16				     *
 *===========================================================================*/
PRIVATE void pcii_wreg16(busind, devind, port, value)
int busind;
int devind;
int port;
u16_t value;
{
	int s;
#if 0
	printf("pcii_wreg16(%d, %d, 0x%X, 0x%X): %d.%d.%d\n",
		busind, devind, port, value,
		pcibus[busind].pb_bus, pcidev[devind].pd_dev,
		pcidev[devind].pd_func);
#endif
	PCII_WREG16_(pcibus[busind].pb_bus, 
		pcidev[devind].pd_dev, pcidev[devind].pd_func,
		port, value);
#if USER_SPACE
	if (OK != (s=sys_outl(PCII_CONFADD, PCII_UNSEL)))
		printf("PCI: warning, sys_outl failed: %d\n", s);
#else
	outl(PCII_CONFADD, PCII_UNSEL);
#endif
}

/*===========================================================================*
 *				pcii_wreg32				     *
 *===========================================================================*/
PRIVATE void pcii_wreg32(busind, devind, port, value)
int busind;
int devind;
int port;
u32_t value;
{
	int s;
#if 0
	printf("pcii_wreg32(%d, %d, 0x%X, 0x%X): %d.%d.%d\n",
		busind, devind, port, value,
		pcibus[busind].pb_bus, pcidev[devind].pd_dev,
		pcidev[devind].pd_func);
#endif
	PCII_WREG32_(pcibus[busind].pb_bus, 
		pcidev[devind].pd_dev, pcidev[devind].pd_func,
		port, value);
#if USER_SPACE
	if (OK != (s=sys_outl(PCII_CONFADD, PCII_UNSEL)))
		printf("PCI: warning, sys_outl failed: %d\n");
#else
	outl(PCII_CONFADD, PCII_UNSEL);
#endif
}

/*===========================================================================*
 *				pcii_rsts				     *
 *===========================================================================*/
PRIVATE u16_t pcii_rsts(busind)
int busind;
{
	u16_t v;
	int s;

	v= PCII_RREG16_(pcibus[busind].pb_bus, 0, 0, PCI_PCISTS);
#if USER_SPACE
	if (OK != (s=sys_outl(PCII_CONFADD, PCII_UNSEL)))
		printf("PCI: warning, sys_outl failed: %d\n", s);
#else
	outl(PCII_CONFADD, PCII_UNSEL);
#endif
	return v;
}

/*===========================================================================*
 *				pcii_wsts				     *
 *===========================================================================*/
PRIVATE void pcii_wsts(busind, value)
int busind;
u16_t value;
{
	int s;
	PCII_WREG16_(pcibus[busind].pb_bus, 0, 0, PCI_PCISTS, value);
#if USER_SPACE
	if (OK != (s=sys_outl(PCII_CONFADD, PCII_UNSEL)))
		printf("PCI: warning, sys_outl failed: %d\n", s);
#else
	outl(PCII_CONFADD, PCII_UNSEL);
#endif
}

/*
 * $PchId: pci.c,v 1.7 2003/08/07 09:06:51 philip Exp $
 */
