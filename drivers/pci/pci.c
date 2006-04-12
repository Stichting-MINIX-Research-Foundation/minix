#define USER_SPACE 1
/*
pci.c

Configure devices on the PCI bus

Created:	Jan 2000 by Philip Homburg <philip@cs.vu.nl>
*/

#include "../drivers.h"
#define	NDEBUG			/* disable assertions */
#include <assert.h>
#include <ibm/pci.h>
#include <sys/vm.h>
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

#define NR_PCIBUS	10
#define NR_PCIDEV	40

#define PBT_INTEL_HOST	 1
#define PBT_PCIBRIDGE	 2
#define PBT_CARDBUS	 3

#define BAM_NR		6	/* Number of base-address registers */

PRIVATE int debug= 0;

PRIVATE struct pcibus
{
	int pb_type;
	int pb_needinit;
	int pb_isabridge_dev;
	int pb_isabridge_type;

	int pb_devind;
	int pb_busnr;
	u8_t (*pb_rreg8)(int busind, int devind, int port);
	u16_t (*pb_rreg16)(int busind, int devind, int port);
	u32_t (*pb_rreg32)(int busind, int devind, int port);
	void (*pb_wreg8)(int busind, int devind, int port, U8_t value);
	void (*pb_wreg16)(int busind, int devind, int port, U16_t value);
	void (*pb_wreg32)(int busind, int devind, int port, u32_t value);
	u16_t (*pb_rsts)(int busind);
	void (*pb_wsts)(int busind, U16_t value);
} pcibus[NR_PCIBUS];
PRIVATE int nr_pcibus= 0;

PRIVATE struct pcidev
{
	u8_t pd_busnr;
	u8_t pd_dev;
	u8_t pd_func;
	u8_t pd_baseclass;
	u8_t pd_subclass;
	u8_t pd_infclass;
	u16_t pd_vid;
	u16_t pd_did;
	u8_t pd_ilr;
	u8_t pd_inuse;

	struct bar
	{
		int pb_flags;
		int pb_nr;
		u32_t pb_base;
		u32_t pb_size;
	} pd_bar[BAM_NR];
	int pd_bar_nr;

	char pd_name[M3_STRING];
} pcidev[NR_PCIDEV];

/* pb_flags */
#define PBF_IO		1	/* I/O else memory */
#define PBF_INCOMPLETE	2	/* not allocated */

PRIVATE int nr_pcidev= 0;

/* Work around the limitations of the PCI emulation in QEMU 0.7.1 */
PRIVATE int qemu_pci= 0;

FORWARD _PROTOTYPE( void pci_intel_init, (void)				);
FORWARD _PROTOTYPE( void probe_bus, (int busind)			);
FORWARD _PROTOTYPE( int is_duplicate, (U8_t busnr, U8_t dev, U8_t func)	);
FORWARD _PROTOTYPE( void record_irq, (int devind)			);
FORWARD _PROTOTYPE( void record_bars, (int devind)			);
FORWARD _PROTOTYPE( void record_bars_bridge, (int devind)		);
FORWARD _PROTOTYPE( void record_bars_cardbus, (int devind)		);
FORWARD _PROTOTYPE( void record_bar, (int devind, int bar_nr)		);
FORWARD _PROTOTYPE( void complete_bridges, (void)			);
FORWARD _PROTOTYPE( void complete_bars, (void)				);
FORWARD _PROTOTYPE( void update_bridge4dev_io, (int devind,
					u32_t io_base, u32_t io_size)	);
FORWARD _PROTOTYPE( int get_freebus, (void)				);
FORWARD _PROTOTYPE( int do_isabridge, (int busind)			);
FORWARD _PROTOTYPE( void do_pcibridge, (int busind)			);
FORWARD _PROTOTYPE( int get_busind, (int busnr)				);
FORWARD _PROTOTYPE( int do_piix, (int devind)				);
FORWARD _PROTOTYPE( int do_amd_isabr, (int devind)			);
FORWARD _PROTOTYPE( int do_sis_isabr, (int devind)			);
FORWARD _PROTOTYPE( int do_via_isabr, (int devind)			);
FORWARD _PROTOTYPE( void report_vga, (int devind)			);
FORWARD _PROTOTYPE( char *pci_vid_name, (U16_t vid)			);
FORWARD _PROTOTYPE( char *pci_baseclass_name, (U8_t baseclass)		);
FORWARD _PROTOTYPE( char *pci_subclass_name, (U8_t baseclass,
					U8_t subclass, U8_t infclass)	);
FORWARD _PROTOTYPE( void ntostr, (unsigned n, char **str, char *end)	);
FORWARD _PROTOTYPE( u16_t pci_attr_rsts, (int devind)			);
FORWARD _PROTOTYPE( void pci_attr_wsts, (int devind, U16_t value)	);
FORWARD _PROTOTYPE( u16_t pcibr_std_rsts, (int busind)		);
FORWARD _PROTOTYPE( void pcibr_std_wsts, (int busind, U16_t value)	);
FORWARD _PROTOTYPE( u16_t pcibr_cb_rsts, (int busind)		);
FORWARD _PROTOTYPE( void pcibr_cb_wsts, (int busind, U16_t value)	);
FORWARD _PROTOTYPE( u16_t pcibr_via_rsts, (int busind)			);
FORWARD _PROTOTYPE( void pcibr_via_wsts, (int busind, U16_t value)	);
FORWARD _PROTOTYPE( u8_t pcii_rreg8, (int busind, int devind, int port)	);
FORWARD _PROTOTYPE( u16_t pcii_rreg16, (int busind, int devind,
							int port)	);
FORWARD _PROTOTYPE( u32_t pcii_rreg32, (int busind, int devind,
							int port)	);
FORWARD _PROTOTYPE( void pcii_wreg8, (int busind, int devind, int port,
							U8_t value)	);
FORWARD _PROTOTYPE( void pcii_wreg16, (int busind, int devind, int port,
							U16_t value)	);
FORWARD _PROTOTYPE( void pcii_wreg32, (int busind, int devind, int port,
							u32_t value)	);
FORWARD _PROTOTYPE( u16_t pcii_rsts, (int busind)			);
FORWARD _PROTOTYPE( void pcii_wsts, (int busind, U16_t value)		);
FORWARD _PROTOTYPE( void print_capabilities, (int devind)		);

/*===========================================================================*
 *			helper functions for I/O			     *
 *===========================================================================*/
PUBLIC unsigned pci_inb(U16_t port) {
	u32_t value;
	int s;
	if ((s=sys_inb(port, &value)) !=OK)
		printf("PCI: warning, sys_inb failed: %d\n", s);
	return value;
}
PUBLIC unsigned pci_inw(U16_t port) {
	u32_t value;
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
		if (pcidev[devind].pd_busnr == bus &&
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
 *				pci_reserve3				     *
 *===========================================================================*/
PUBLIC void pci_reserve3(devind, proc, name)
int devind;
int proc;
char *name;
{
	int i, r;
	u8_t ilr;
	struct io_range ior;
	struct mem_range mr;

	assert(devind <= nr_pcidev);
	assert(!pcidev[devind].pd_inuse);
	pcidev[devind].pd_inuse= 1;
	strcpy(pcidev[devind].pd_name, name);

	for (i= 0; i<pcidev[devind].pd_bar_nr; i++)
	{
		if (pcidev[devind].pd_bar[i].pb_flags & PBF_INCOMPLETE)
		{
			printf("pci_reserve3: BAR %d is incomplete\n", i);
			continue;
		}
		if (pcidev[devind].pd_bar[i].pb_flags & PBF_IO)
		{
			ior.ior_base= pcidev[devind].pd_bar[i].pb_base;
			ior.ior_limit= ior.ior_base +
				pcidev[devind].pd_bar[i].pb_size-1;

			if(debug) {
			   printf(
		"pci_reserve3: for proc %d, adding I/O range [0x%x..0x%x]\n",
				proc, ior.ior_base, ior.ior_limit);
			}
			r= sys_privctl(proc, SYS_PRIV_ADD_IO, 0, &ior);
			if (r != OK)
			{
				printf("sys_privctl failed for proc %d: %d\n",
					proc, r);
			}
		}
		else
		{
			mr.mr_base= pcidev[devind].pd_bar[i].pb_base;
			mr.mr_limit= mr.mr_base +
				pcidev[devind].pd_bar[i].pb_size-1;

			if(debug) {
			   printf(
	"pci_reserve3: for proc %d, should add memory range [0x%x..0x%x]\n",
				proc, mr.mr_base, mr.mr_limit);
			}
			r= sys_privctl(proc, SYS_PRIV_ADD_MEM, 0, &mr);
			if (r != OK)
			{
				printf("sys_privctl failed for proc %d: %d\n",
					proc, r);
			}
		}
	}
	ilr= pcidev[devind].pd_ilr;
	if (ilr != PCI_ILR_UNKNOWN)
	{
		if(debug) printf("pci_reserve3: adding IRQ %d\n", ilr);
		r= sys_privctl(proc, SYS_PRIV_ADD_IRQ, ilr, NULL);
		if (r != OK)
		{
			printf("sys_privctl failed for proc %d: %d\n",
				proc, r);
		}
	}
}

/*===========================================================================*
 *				pci_release				     *
 *===========================================================================*/
PUBLIC void pci_release(name)
char *name;
{
	int i;

	for (i= 0; i<nr_pcidev; i++)
	{
		if (!pcidev[i].pd_inuse)
			continue;
		if (strcmp(pcidev[i].pd_name, name) != 0)
			continue;
		pcidev[i].pd_inuse= 0;
	}
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
 *				pci_rescan_bus				     *
 *===========================================================================*/
PUBLIC void pci_rescan_bus(busnr)
u8_t busnr;
{
	int busind;

	busind= get_busind(busnr);
	probe_bus(busind);

	/* Allocate bus numbers for uninitialized bridges */
	complete_bridges();

	/* Allocate I/O and memory resources for uninitialized devices */
	complete_bars();
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

	ntostr(pcidev[devind].pd_busnr, &p, end);
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
	int busnr, busind;

	busnr= pcidev[devind].pd_busnr;
	busind= get_busind(busnr);
	return pcibus[busind].pb_rreg8(busind, devind, port);
}

/*===========================================================================*
 *				pci_attr_r16				     *
 *===========================================================================*/
PUBLIC u16_t pci_attr_r16(devind, port)
int devind;
int port;
{
	int busnr, busind;

	busnr= pcidev[devind].pd_busnr;
	busind= get_busind(busnr);
	return pcibus[busind].pb_rreg16(busind, devind, port);
}

/*===========================================================================*
 *				pci_attr_r32				     *
 *===========================================================================*/
PUBLIC u32_t pci_attr_r32(devind, port)
int devind;
int port;
{
	int busnr, busind;

	busnr= pcidev[devind].pd_busnr;
	busind= get_busind(busnr);
	return pcibus[busind].pb_rreg32(busind, devind, port);
}

/*===========================================================================*
 *				pci_attr_w8				     *
 *===========================================================================*/
PUBLIC void pci_attr_w8(devind, port, value)
int devind;
int port;
u16_t value;
{
	int busnr, busind;

	busnr= pcidev[devind].pd_busnr;
	busind= get_busind(busnr);
	pcibus[busind].pb_wreg8(busind, devind, port, value);
}

/*===========================================================================*
 *				pci_attr_w16				     *
 *===========================================================================*/
PUBLIC void pci_attr_w16(devind, port, value)
int devind;
int port;
u16_t value;
{
	int busnr, busind;

	busnr= pcidev[devind].pd_busnr;
	busind= get_busind(busnr);
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
	int busnr, busind;

	busnr= pcidev[devind].pd_busnr;
	busind= get_busind(busnr);
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
	 * PCI controller. 
	 */
	u32_t bus, dev, func;
	u16_t vid, did;
	int s, i, r, busind, busnr;
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

#if 0
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
#endif

	if (nr_pcibus >= NR_PCIBUS)
		panic("PCI","too many PCI busses", nr_pcibus);
	busind= nr_pcibus;
	nr_pcibus++;
	pcibus[busind].pb_type= PBT_INTEL_HOST;
	pcibus[busind].pb_needinit= 0;
	pcibus[busind].pb_isabridge_dev= -1;
	pcibus[busind].pb_isabridge_type= 0;
	pcibus[busind].pb_devind= -1;
	pcibus[busind].pb_busnr= 0;
	pcibus[busind].pb_rreg8= pcii_rreg8;
	pcibus[busind].pb_rreg16= pcii_rreg16;
	pcibus[busind].pb_rreg32= pcii_rreg32;
	pcibus[busind].pb_wreg8= pcii_wreg8;
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
		busnr= pcibus[busind].pb_busnr;

		/* Disable all devices for this bus */
		for (i= 0; i<nr_pcidev; i++)
		{
			if (pcidev[i].pd_busnr != busnr)
				continue;
			pcidev[i].pd_inuse= 1;
		}
		return;
	}

	/* Look for PCI bridges */
	do_pcibridge(busind);

	/* Allocate bus numbers for uninitialized bridges */
	complete_bridges();

	/* Allocate I/O and memory resources for uninitialized devices */
	complete_bars();
}

/*===========================================================================*
 *				probe_bus				     *
 *===========================================================================*/
PRIVATE void probe_bus(busind)
int busind;
{
	u32_t dev, func, t3;
	u16_t vid, did, sts;
	u8_t headt;
	u8_t baseclass, subclass, infclass;
	int devind, busnr;
	char *s, *dstr;

#if DEBUG
printf("probe_bus(%d)\n", busind);
#endif
	if (nr_pcidev >= NR_PCIDEV)
		panic("PCI","too many PCI devices", nr_pcidev);
	devind= nr_pcidev;

	busnr= pcibus[busind].pb_busnr;
	for (dev= 0; dev<32; dev++)
	{

		for (func= 0; func < 8; func++)
		{
			pcidev[devind].pd_busnr= busnr;
			pcidev[devind].pd_dev= dev;
			pcidev[devind].pd_func= func;

			pci_attr_wsts(devind, 
				PSR_SSE|PSR_RMAS|PSR_RTAS);
			vid= pci_attr_r16(devind, PCI_VID);
			did= pci_attr_r16(devind, PCI_DID);
			headt= pci_attr_r8(devind, PCI_HEADT);
			sts= pci_attr_rsts(devind);

#if 0
			printf("vid 0x%x, did 0x%x, headt 0x%x, sts 0x%x\n",
				vid, did, headt, sts);
#endif

			if (vid == NO_VID)
			{
				if (func == 0)
					break;	/* Nothing here */

				/* Scan all functions of a multifunction
				 * device.
				 */
				continue;
			}

			if (sts & (PSR_SSE|PSR_RMAS|PSR_RTAS))
			{
				if (qemu_pci)
				{
					printf(
			"pci: ignoring bad value 0x%x in sts for QEMU\n",
					sts & (PSR_SSE|PSR_RMAS|PSR_RTAS));
				}
				else
				{
					if (func == 0)
						break;	/* Nothing here */

					/* Scan all functions of a
					 * multifunction device.
					 */
					continue;
				}
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
				printf("Device index: %d\n", devind);
				printf("Subsystem: Vid 0x%x, did 0x%x\n",
					pci_attr_r16(devind, PCI_SUBVID),
					pci_attr_r16(devind, PCI_SUBDID));
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

			if (is_duplicate(busnr, dev, func))
			{
				printf("\tduplicate!\n");
				if (func == 0 && !(headt & PHT_MULTIFUNC))
					break;
				continue;
			}

			devind= nr_pcidev;
			nr_pcidev++;
			pcidev[devind].pd_baseclass= baseclass;
			pcidev[devind].pd_subclass= subclass;
			pcidev[devind].pd_infclass= infclass;
			pcidev[devind].pd_vid= vid;
			pcidev[devind].pd_did= did;
			pcidev[devind].pd_inuse= 0;
			pcidev[devind].pd_bar_nr= 0;
			record_irq(devind);
			switch(headt & PHT_MASK)
			{
			case PHT_NORMAL:
				record_bars(devind);
				break;
			case PHT_BRIDGE:
				record_bars_bridge(devind);
				break;
			case PHT_CARDBUS:
				record_bars_cardbus(devind);
				break;
			default:
				printf("\t%d.%d.%d: unknown header type %d\n",
					busind, dev, func,
					headt & PHT_MASK);
				break;
			}
			if (debug)
				print_capabilities(devind);

			t3= ((baseclass << 16) | (subclass << 8) | infclass);
			if (t3 == PCI_T3_VGA || t3 == PCI_T3_VGA_OLD)
				report_vga(devind);

			if (nr_pcidev >= NR_PCIDEV)
			  panic("PCI","too many PCI devices", nr_pcidev);
			devind= nr_pcidev;

			if (func == 0 && !(headt & PHT_MULTIFUNC))
				break;
		}
	}
}

/*===========================================================================*
 *				is_duplicate				     *
 *===========================================================================*/
PRIVATE int is_duplicate(busnr, dev, func)
u8_t busnr;
u8_t dev;
u8_t func;
{
	int i;

	for (i= 0; i<nr_pcidev; i++)
	{
		if (pcidev[i].pd_busnr == busnr &&
			pcidev[i].pd_dev == dev &&
			pcidev[i].pd_func == func)
		{
			return 1;
		}
	}
	return 0;
}

/*===========================================================================*
 *				record_irq				     *
 *===========================================================================*/
PRIVATE void record_irq(devind)
int devind;
{
	int ilr, ipr, busnr, busind, cb_devind;

	ilr= pci_attr_r8(devind, PCI_ILR);
	ipr= pci_attr_r8(devind, PCI_IPR);
	if (ilr == 0)
	{
		static int first= 1;
		if (ipr && first && debug)
		{
			first= 0;
			printf("PCI: strange, BIOS assigned IRQ0\n");
		}
		ilr= PCI_ILR_UNKNOWN;
	}
	pcidev[devind].pd_ilr= ilr;
	if (ilr == PCI_ILR_UNKNOWN && !ipr)
	{
	}
	else if (ilr != PCI_ILR_UNKNOWN && ipr)
	{
		if (debug)
			printf("\tIRQ %d for INT%c\n", ilr, 'A' + ipr-1);
	}
	else if (ilr != PCI_ILR_UNKNOWN)
	{
		printf(
	"PCI: IRQ %d is assigned, but device %d.%d.%d does not need it\n",
			ilr, pcidev[devind].pd_busnr, pcidev[devind].pd_dev,
			pcidev[devind].pd_func);
	}
	else
	{
		/* Check for cardbus devices */
		busnr= pcidev[devind].pd_busnr;
		busind= get_busind(busnr);
		if (pcibus[busind].pb_type == PBT_CARDBUS)
		{
			cb_devind= pcibus[busind].pb_devind;
			ilr= pcidev[cb_devind].pd_ilr;
			if (ilr != PCI_ILR_UNKNOWN)
			{
				if (debug)
				{
					printf(
					"assigning IRQ %d to Cardbus device\n",
						ilr);
				}
				pci_attr_w8(devind, PCI_ILR, ilr);
				pcidev[devind].pd_ilr= ilr;
				return;
			}
		}
		if(debug) {
			printf(
		"PCI: device %d.%d.%d uses INT%c but is not assigned any IRQ\n",
			pcidev[devind].pd_busnr, pcidev[devind].pd_dev,
			pcidev[devind].pd_func, 'A' + ipr-1);
		}
	}
}

/*===========================================================================*
 *				record_bars				     *
 *===========================================================================*/
PRIVATE void record_bars(devind)
int devind;
{
	int i, j, reg, prefetch, type, clear_01, clear_23, pb_nr;
	u32_t bar, bar2;

	for (i= 0, reg= PCI_BAR; reg <= PCI_BAR_6; i++, reg += 4)
	{
		record_bar(devind, i);
	}

	/* Special case code for IDE controllers in compatibility mode */
	if (pcidev[devind].pd_baseclass == PCI_BCR_MASS_STORAGE &&
		pcidev[devind].pd_subclass == PCI_MS_IDE)
	{
		/* IDE device */
		clear_01= 0;
		clear_23= 0;
		if (!(pcidev[devind].pd_infclass & PCI_IDE_PRI_NATIVE))
		{
			if (debug)
			{
				printf(
	"primary channel is not in native mode, clearing BARs 0 and 1\n");
			}
			clear_01= 1;
		}
		if (!(pcidev[devind].pd_infclass & PCI_IDE_SEC_NATIVE))
		{
			if (debug)
			{
				printf(
	"primary channel is not in native mode, clearing BARs 2 and 3\n");
			}
			clear_23= 1;
		}

		j= 0;
		for (i= 0; i<pcidev[devind].pd_bar_nr; i++)
		{
			pb_nr= pcidev[devind].pd_bar[i].pb_nr;
			if ((pb_nr == 0 || pb_nr == 1) && clear_01)
			{
				if (debug) printf("skipping bar %d\n", pb_nr);
				continue;	/* Skip */
			}
			if ((pb_nr == 2 || pb_nr == 3) && clear_23)
			{
				if (debug) printf("skipping bar %d\n", pb_nr);
				continue;	/* Skip */
			}
			if (i == j)
				continue;	/* No need to copy */
			pcidev[devind].pd_bar[j]=
				pcidev[devind].pd_bar[i];
			j++;
		}
		pcidev[devind].pd_bar_nr= j;
	}
}

/*===========================================================================*
 *				record_bars_bridge			     *
 *===========================================================================*/
PRIVATE void record_bars_bridge(devind)
int devind;
{
	u32_t base, limit, size;

	record_bar(devind, 0);
	record_bar(devind, 1);

	base= ((pci_attr_r8(devind, PPB_IOBASE) & PPB_IOB_MASK) << 8) |
		(pci_attr_r16(devind, PPB_IOBASEU16) << 16);
	limit= 0xff |
		((pci_attr_r8(devind, PPB_IOLIMIT) & PPB_IOL_MASK) << 8) |
		((~PPB_IOL_MASK & 0xff) << 8) |
		(pci_attr_r16(devind, PPB_IOLIMITU16) << 16);
	size= limit-base + 1;
	if (debug)
	{
		printf("\tI/O window: base 0x%x, limit 0x%x, size %d\n",
			base, limit, size);
	}

	base= ((pci_attr_r16(devind, PPB_MEMBASE) & PPB_MEMB_MASK) << 16);
	limit= 0xffff |
		((pci_attr_r16(devind, PPB_MEMLIMIT) & PPB_MEML_MASK) << 16) |
		((~PPB_MEML_MASK & 0xffff) << 16);
	size= limit-base + 1;
	if (debug)
	{
		printf("\tMemory window: base 0x%x, limit 0x%x, size 0x%x\n",
			base, limit, size);
	}

	/* Ignore the upper 32 bits */
	base= ((pci_attr_r16(devind, PPB_PFMEMBASE) & PPB_PFMEMB_MASK) << 16);
	limit= 0xffff |
		((pci_attr_r16(devind, PPB_PFMEMLIMIT) &
			PPB_PFMEML_MASK) << 16) |
		((~PPB_PFMEML_MASK & 0xffff) << 16);
	size= limit-base + 1;
	if (debug)
	{
		printf(
	"\tPrefetchable memory window: base 0x%x, limit 0x%x, size 0x%x\n",
			base, limit, size);
	}
}

/*===========================================================================*
 *				record_bars_cardbus			     *
 *===========================================================================*/
PRIVATE void record_bars_cardbus(devind)
int devind;
{
	u32_t base, limit, size;

	record_bar(devind, 0);

	base= pci_attr_r32(devind, CBB_MEMBASE_0);
	limit= pci_attr_r32(devind, CBB_MEMLIMIT_0) |
		(~CBB_MEML_MASK & 0xffffffff);
	size= limit-base + 1;
	if (debug)
	{
		printf("\tMemory window 0: base 0x%x, limit 0x%x, size %d\n",
			base, limit, size);
	}

	base= pci_attr_r32(devind, CBB_MEMBASE_1);
	limit= pci_attr_r32(devind, CBB_MEMLIMIT_1) |
		(~CBB_MEML_MASK & 0xffffffff);
	size= limit-base + 1;
	if (debug)
	{
		printf("\tMemory window 1: base 0x%x, limit 0x%x, size %d\n",
			base, limit, size);
	}

	base= pci_attr_r32(devind, CBB_IOBASE_0);
	limit= pci_attr_r32(devind, CBB_IOLIMIT_0) |
		(~CBB_IOL_MASK & 0xffffffff);
	size= limit-base + 1;
	if (debug)
	{
		printf("\tI/O window 0: base 0x%x, limit 0x%x, size %d\n",
			base, limit, size);
	}

	base= pci_attr_r32(devind, CBB_IOBASE_1);
	limit= pci_attr_r32(devind, CBB_IOLIMIT_1) |
		(~CBB_IOL_MASK & 0xffffffff);
	size= limit-base + 1;
	if (debug)
	{
		printf("\tI/O window 1: base 0x%x, limit 0x%x, size %d\n",
			base, limit, size);
	}
}

/*===========================================================================*
 *				record_bar				     *
 *===========================================================================*/
PRIVATE void record_bar(devind, bar_nr)
int devind;
int bar_nr;
{
	int reg, prefetch, type, dev_bar_nr;
	u32_t bar, bar2;

	reg= PCI_BAR+4*bar_nr;

	bar= pci_attr_r32(devind, reg);
	if (bar & PCI_BAR_IO)
	{
		/* Size register */
		pci_attr_w32(devind, reg, 0xffffffff);
		bar2= pci_attr_r32(devind, reg);
		pci_attr_w32(devind, reg, bar);

		bar &= ~(u32_t)3;	/* Clear non-address bits */
		bar2 &= ~(u32_t)3;
		bar2= (~bar2 & 0xffff)+1;
		if (debug)
		{
			printf("\tbar_%d: %d bytes at 0x%x I/O\n",
				bar_nr, bar2, bar);
		}

		dev_bar_nr= pcidev[devind].pd_bar_nr++;
		assert(dev_bar_nr < BAR_NR);
		pcidev[devind].pd_bar[dev_bar_nr].pb_flags= PBF_IO;
		pcidev[devind].pd_bar[dev_bar_nr].pb_base= bar;
		pcidev[devind].pd_bar[dev_bar_nr].pb_size= bar2;
		pcidev[devind].pd_bar[dev_bar_nr].pb_nr= bar_nr;
		if (bar == 0)
		{
			pcidev[devind].pd_bar[dev_bar_nr].pb_flags |= 
				PBF_INCOMPLETE;
		}
	}
	else
	{
		/* Size register */
		pci_attr_w32(devind, reg, 0xffffffff);
		bar2= pci_attr_r32(devind, reg);
		pci_attr_w32(devind, reg, bar);

		if (bar2 == 0)
			return;	/* Reg. is not implemented */

		prefetch= !!(bar & PCI_BAR_PREFETCH);
		type= (bar & PCI_BAR_TYPE);
		bar &= ~(u32_t)0xf;	/* Clear non-address bits */
		bar2 &= ~(u32_t)0xf;
		bar2= (~bar2)+1;
		if (debug)
		{
			printf("\tbar_%d: 0x%x bytes at 0x%x%s memory\n",
				bar_nr, bar2, bar,
				prefetch ? " prefetchable" : "");
			if (type != 0)
				printf("type = 0x%x\n", type);
		}

		dev_bar_nr= pcidev[devind].pd_bar_nr++;
		assert(dev_bar_nr < BAR_NR);
		pcidev[devind].pd_bar[dev_bar_nr].pb_flags= 0;
		pcidev[devind].pd_bar[dev_bar_nr].pb_base= bar;
		pcidev[devind].pd_bar[dev_bar_nr].pb_size= bar2;
		pcidev[devind].pd_bar[dev_bar_nr].pb_nr= bar_nr;
		if (bar == 0)
		{
			pcidev[devind].pd_bar[dev_bar_nr].pb_flags |= 
				PBF_INCOMPLETE;
		}
	}
}

/*===========================================================================*
 *				complete_bridges			     *
 *===========================================================================*/
PRIVATE void complete_bridges()
{
	int i, freebus, devind, prim_busnr;

	for (i= 0; i<nr_pcibus; i++)
	{
		if (!pcibus[i].pb_needinit)
			continue;
		printf("should allocate bus number for bus %d\n", i);
		freebus= get_freebus();
		printf("got bus number %d\n", freebus);

		devind= pcibus[i].pb_devind;

		prim_busnr= pcidev[devind].pd_busnr;
		if (prim_busnr != 0)
		{
			printf(
	"complete_bridge: updating subordinate bus number not implemented\n");
		}

		pcibus[i].pb_needinit= 0;
		pcibus[i].pb_busnr= freebus;

		printf("devind = %d\n", devind);
		printf("prim_busnr= %d\n", prim_busnr);

		pci_attr_w8(devind, PPB_PRIMBN, prim_busnr);
		pci_attr_w8(devind, PPB_SECBN, freebus);
		pci_attr_w8(devind, PPB_SUBORDBN, freebus);

		printf("CR = 0x%x\n", pci_attr_r16(devind, PCI_CR));
		printf("SECBLT = 0x%x\n", pci_attr_r8(devind, PPB_SECBLT));
		printf("BRIDGECTRL = 0x%x\n",
			pci_attr_r16(devind, PPB_BRIDGECTRL));
	}
}

/*===========================================================================*
 *				complete_bars				     *
 *===========================================================================*/
PRIVATE void complete_bars()
{
	int i, j, r, bar_nr, reg;
	u32_t memgap_low, memgap_high, iogap_low, iogap_high, io_high,
		base, size, v32, diff1, diff2;
	char *cp, *next;
	char memstr[256];

	r= env_get_param("memory", memstr, sizeof(memstr));
	if (r != OK)
		panic("pci", "env_get_param failed", r);
	
	/* Set memgap_low to just above physical memory */
	memgap_low= 0;
	cp= memstr;
	while (*cp != '\0')
	{
		base= strtoul(cp, &next, 16);
		if (next == cp || *next != ':')
		{
			printf("pci: bad memory environment string '%s'\n",
				memstr);
			panic(NULL, NULL, NO_NUM);
		}
		cp= next+1;
		size= strtoul(cp, &next, 16);
		if (next == cp || (*next != ',' && *next != '\0'))
		{
			printf("pci: bad memory environment string '%s'\n",
				memstr);
			panic(NULL, NULL, NO_NUM);
		}
		cp= next+1;

		if (base+size > memgap_low)
			memgap_low= base+size;
	}

	memgap_high= 0xfe000000;	/* Leave space for the CPU (APIC) */

	if (debug)
	{
		printf("complete_bars: initial gap: [0x%x .. 0x%x>\n",
			memgap_low, memgap_high);
	}

	/* Find the lowest memory base */
	for (i= 0; i<nr_pcidev; i++)
	{
		for (j= 0; j<pcidev[i].pd_bar_nr; j++)
		{
			if (pcidev[i].pd_bar[j].pb_flags & PBF_IO)
				continue;
			if (pcidev[i].pd_bar[j].pb_flags & PBF_INCOMPLETE)
				continue;
			base= pcidev[i].pd_bar[j].pb_base;
			size= pcidev[i].pd_bar[j].pb_size;

			if (base >= memgap_high)
				continue;	/* Not in the gap */
			if (base+size <= memgap_low)
				continue;	/* Not in the gap */

			/* Reduce the gap by the smallest amount */
			diff1= base+size-memgap_low;
			diff2= memgap_high-base;

			if (diff1 < diff2)
				memgap_low= base+size;
			else
				memgap_high= base;
		}
	}

	if (debug)
	{
		printf("complete_bars: intermediate gap: [0x%x .. 0x%x>\n",
			memgap_low, memgap_high);
	}

	/* Should check main memory size */
	if (memgap_high < memgap_low)
	{
		printf("pci: bad memory gap: [0x%x .. 0x%x>\n",
			memgap_low, memgap_high);
		panic(NULL, NULL, NO_NUM);
	}

	iogap_high= 0x10000;
	iogap_low= 0x400;

	/* Find the free I/O space */
	for (i= 0; i<nr_pcidev; i++)
	{
		for (j= 0; j<pcidev[i].pd_bar_nr; j++)
		{
			if (!(pcidev[i].pd_bar[j].pb_flags & PBF_IO))
				continue;
			if (pcidev[i].pd_bar[j].pb_flags & PBF_INCOMPLETE)
				continue;
			base= pcidev[i].pd_bar[j].pb_base;
			size= pcidev[i].pd_bar[j].pb_size;
			if (base >= iogap_high)
				continue;
			if (base+size <= iogap_low)
				continue;
#if 0
			if (debug)
			{
				printf(
		"pci device %d (%04x/%04x), bar %d: base 0x%x, size 0x%x\n",
					i, pcidev[i].pd_vid, pcidev[i].pd_did,
					j, base, size);
			}
#endif
			if (base+size-iogap_low < iogap_high-base)
				iogap_low= base+size;
			else
				iogap_high= base;
		}
	}

	if (iogap_high < iogap_low)
	{
		if (debug)
		{
			printf("iogap_high too low, should panic\n");
		}
		else
			panic("pci", "iogap_high too low", iogap_high);
	}
	if (debug)
		printf("I/O range = [0x%x..0x%x>\n", iogap_low, iogap_high);

	for (i= 0; i<nr_pcidev; i++)
	{
		for (j= 0; j<pcidev[i].pd_bar_nr; j++)
		{
			if (pcidev[i].pd_bar[j].pb_flags & PBF_IO)
				continue;
			if (!(pcidev[i].pd_bar[j].pb_flags & PBF_INCOMPLETE))
				continue;
			size= pcidev[i].pd_bar[j].pb_size;
			if (size < PAGE_SIZE)
				size= PAGE_SIZE;
			base= memgap_high-size;
			base &= ~(u32_t)(size-1);
			if (base < memgap_low)
				panic("pci", "memory base too low", base);
			memgap_high= base;
			bar_nr= pcidev[i].pd_bar[j].pb_nr;
			reg= PCI_BAR + 4*bar_nr;
			v32= pci_attr_r32(i, reg);
			pci_attr_w32(i, reg, v32 | base);
			if (debug)
			{
				printf(
		"complete_bars: allocated 0x%x size %d to %d.%d.%d, bar_%d\n",
					base, size, pcidev[i].pd_busnr,
					pcidev[i].pd_dev, pcidev[i].pd_func,
					bar_nr);
			}
			pcidev[i].pd_bar[j].pb_base= base;
			pcidev[i].pd_bar[j].pb_flags &= ~PBF_INCOMPLETE;
		}

		io_high= iogap_high;
		for (j= 0; j<pcidev[i].pd_bar_nr; j++)
		{
			if (!(pcidev[i].pd_bar[j].pb_flags & PBF_IO))
				continue;
			if (!(pcidev[i].pd_bar[j].pb_flags & PBF_INCOMPLETE))
				continue;
			size= pcidev[i].pd_bar[j].pb_size;
			base= iogap_high-size;
			base &= ~(u32_t)(size-1);

			/* Assume that ISA compatibility is required. Only
			 * use the lowest 256 bytes out of every 1024 bytes.
			 */
			base &= 0xfcff;

			if (base < iogap_low)
				panic("pci", "I/O base too low", base);

			iogap_high= base;
			bar_nr= pcidev[i].pd_bar[j].pb_nr;
			reg= PCI_BAR + 4*bar_nr;
			v32= pci_attr_r32(i, reg);
			pci_attr_w32(i, reg, v32 | base);
			if (debug)
			{
				printf(
		"complete_bars: allocated 0x%x size %d to %d.%d.%d, bar_%d\n",
					base, size, pcidev[i].pd_busnr,
					pcidev[i].pd_dev, pcidev[i].pd_func,
					bar_nr);
			}
			pcidev[i].pd_bar[j].pb_base= base;
			pcidev[i].pd_bar[j].pb_flags &= ~PBF_INCOMPLETE;

		}
		if (iogap_high != io_high)
		{
			update_bridge4dev_io(i, iogap_high,
				io_high-iogap_high);
		}
	}

	for (i= 0; i<nr_pcidev; i++)
	{
		for (j= 0; j<pcidev[i].pd_bar_nr; j++)
		{
			if (!(pcidev[i].pd_bar[j].pb_flags & PBF_INCOMPLETE))
				continue;
			printf("should allocate resources for device %d\n", i);
		}
	}
}

/*===========================================================================*
 *				update_bridge4dev_io			     *
 *===========================================================================*/
PRIVATE void update_bridge4dev_io(devind, io_base, io_size)
int devind;
u32_t io_base;
u32_t io_size;
{
	int busnr, busind, type, br_devind;
	u16_t v16;

	busnr= pcidev[devind].pd_busnr;
	busind= get_busind(busnr);
	type= pcibus[busind].pb_type;
	if (type == PBT_INTEL_HOST)
		return;	/* Nothing to do for host controller */
	if (type == PBT_PCIBRIDGE)
	{
		printf(
		"update_bridge4dev_io: not implemented for PCI bridges\n");
		return;	
	}
	if (type != PBT_CARDBUS)
		panic("pci", "update_bridge4dev_io: strange bus type", type);

	if (debug)
	{
		printf("update_bridge4dev_io: adding 0x%x at 0x%x\n",
			io_size, io_base);
	}
	br_devind= pcibus[busind].pb_devind;
	pci_attr_w32(br_devind, CBB_IOLIMIT_0, io_base+io_size-1);
	pci_attr_w32(br_devind, CBB_IOBASE_0, io_base);

	/* Enable I/O access. Enable busmaster access as well. */
	v16= pci_attr_r16(devind, PCI_CR);
	pci_attr_w16(devind, PCI_CR, v16 | PCI_CR_IO_EN | PCI_CR_MAST_EN);
}

/*===========================================================================*
 *				get_freebus				     *
 *===========================================================================*/
PRIVATE int get_freebus()
{
	int i, freebus;

	freebus= 1;
	for (i= 0; i<nr_pcibus; i++)
	{
		if (pcibus[i].pb_needinit)
			continue;
		if (pcibus[i].pb_type == PBT_INTEL_HOST)
			continue;
		if (pcibus[i].pb_busnr <= freebus)
			freebus= pcibus[i].pb_busnr+1;
		printf("get_freebus: should check suboridinate bus number\n");
	}
	return freebus;
}

/*===========================================================================*
 *				do_isabridge				     *
 *===========================================================================*/
PRIVATE int do_isabridge(busind)
int busind;
{
	int i, j, r, type, busnr, unknown_bridge, bridge_dev;
	u16_t vid, did;
	u32_t t3;
	char *dstr;

	unknown_bridge= -1;
	bridge_dev= -1;
	j= 0;	/* lint */
	vid= did= 0;	/* lint */
	busnr= pcibus[busind].pb_busnr;
	for (i= 0; i< nr_pcidev; i++)
	{
		if (pcidev[i].pd_busnr != busnr)
			continue;
		t3= ((pcidev[i].pd_baseclass << 16) |
			(pcidev[i].pd_subclass << 8) | pcidev[i].pd_infclass);
		if (t3 == PCI_T3_ISA)
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
		if (debug)
		{
			printf("(warning) no ISA bridge found on bus %d\n",
				busind);
		}
		return 0;
	}
	if (debug)
	{
		printf(
		"(warning) unsupported ISA bridge %04X/%04X for bus %d\n",
			pcidev[unknown_bridge].pd_vid,
			pcidev[unknown_bridge].pd_did, busind);
	}
	return 0;
}

/*===========================================================================*
 *				do_pcibridge				     *
 *===========================================================================*/
PRIVATE void do_pcibridge(busind)
int busind;
{
	int i, devind, busnr;
	int ind, type;
	u16_t vid, did;
	u8_t sbusn, baseclass, subclass, infclass, headt;
	u32_t t3;

	vid= did= 0;	/* lint */
	busnr= pcibus[busind].pb_busnr;
	for (devind= 0; devind< nr_pcidev; devind++)
	{
#if 0
		printf("do_pcibridge: trying %u.%u.%u\n",
			pcidev[devind].pd_busind, pcidev[devind].pd_dev, 
			pcidev[devind].pd_func);
#endif

		if (pcidev[devind].pd_busnr != busnr)
		{
#if 0
			printf("wrong bus\n");
#endif
			continue;
		}

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
		type= pci_pcibridge[i].type;
		if (pci_pcibridge[i].vid == 0)
		{
			headt= pci_attr_r8(devind, PCI_HEADT);
			type= 0;
			if ((headt & PHT_MASK) == PHT_BRIDGE)
				type= PCI_PPB_STD;
			else if ((headt & PHT_MASK) == PHT_CARDBUS)
				type= PCI_PPB_CB;
			else
			{
#if 0
				printf("not a bridge\n");
#endif
				continue;	/* Not a bridge */
			}

			baseclass= pci_attr_r8(devind, PCI_BCR);
			subclass= pci_attr_r8(devind, PCI_SCR);
			infclass= pci_attr_r8(devind, PCI_PIFR);
			t3= ((baseclass << 16) | (subclass << 8) | infclass);
			if (type == PCI_PPB_STD &&
				t3 != PCI_T3_PCI2PCI &&
				t3 != PCI_T3_PCI2PCI_SUBTR)
			{
				printf(
"Unknown PCI class %02x:%02x:%02x for PCI-to-PCI bridge, device %04X/%04X\n",
					baseclass, subclass, infclass,
					vid, did);
				continue;
			 }
			if (type == PCI_PPB_CB &&
				t3 != PCI_T3_CARDBUS)
			{
				printf(
"Unknown PCI class %02x:%02x:%02x for Cardbus bridge, device %04X/%04X\n",
					baseclass, subclass, infclass,
					vid, did);
				continue;
			 }
		}

		if (debug)
		{
			printf("%u.%u.%u: PCI-to-PCI bridge: %04X/%04X\n",
				pcidev[devind].pd_busnr,
				pcidev[devind].pd_dev, 
				pcidev[devind].pd_func, vid, did);
		}

		/* Assume that the BIOS initialized the secondary bus
		 * number.
		 */
		sbusn= pci_attr_r8(devind, PPB_SECBN);
#if DEBUG
		printf("sbusn = %d\n", sbusn);
		printf("subordn = %d\n", pci_attr_r8(devind, PPB_SUBORDBN));
#endif

		if (nr_pcibus >= NR_PCIBUS)
			panic("PCI","too many PCI busses", nr_pcibus);
		ind= nr_pcibus;
		nr_pcibus++;
		pcibus[ind].pb_type= PBT_PCIBRIDGE;
		pcibus[ind].pb_needinit= 1;
		pcibus[ind].pb_isabridge_dev= -1;
		pcibus[ind].pb_isabridge_type= 0;
		pcibus[ind].pb_devind= devind;
		pcibus[ind].pb_busnr= sbusn;
		pcibus[ind].pb_rreg8= pcibus[busind].pb_rreg8;
		pcibus[ind].pb_rreg16= pcibus[busind].pb_rreg16;
		pcibus[ind].pb_rreg32= pcibus[busind].pb_rreg32;
		pcibus[ind].pb_wreg8= pcibus[busind].pb_wreg8;
		pcibus[ind].pb_wreg16= pcibus[busind].pb_wreg16;
		pcibus[ind].pb_wreg32= pcibus[busind].pb_wreg32;
		switch(type)
		{
		case PCI_PPB_STD:
			pcibus[ind].pb_rsts= pcibr_std_rsts;
			pcibus[ind].pb_wsts= pcibr_std_wsts;
			break;
		case PCI_PPB_CB:
			pcibus[ind].pb_type= PBT_CARDBUS;
			pcibus[ind].pb_rsts= pcibr_cb_rsts;
			pcibus[ind].pb_wsts= pcibr_cb_wsts;
			break;
		case PCI_AGPB_VIA:
			pcibus[ind].pb_rsts= pcibr_via_rsts;
			pcibus[ind].pb_wsts= pcibr_via_wsts;
			break;
		default:
		    panic("PCI","unknown PCI-PCI bridge type", type);
		}
		if (sbusn == 0)
		{
			printf("Secondary bus number not initialized\n");
			continue;
		}
		pcibus[ind].pb_needinit= 0;

		probe_bus(ind);

		/* Look for PCI bridges */
		do_pcibridge(ind);
	}
}

/*===========================================================================*
 *				get_busind					     *
 *===========================================================================*/
PRIVATE int get_busind(busnr)
int busnr;
{
	int i;

	for (i= 0; i<nr_pcibus; i++)
	{
		if (pcibus[i].pb_busnr == busnr)
			return i;
	}
	panic("pci", "get_busind: can't find bus", busnr);
}

/*===========================================================================*
 *				do_piix					     *
 *===========================================================================*/
PRIVATE int do_piix(devind)
int devind;
{
	int i, s, dev, func, irqrc, irq;
	u32_t elcr1, elcr2, elcr;

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
	int i, busnr, dev, func, xdevind, irq, edge;
	u8_t levmask;
	u16_t pciirq;

	/* Find required function */
	func= AMD_ISABR_FUNC;
	busnr= pcidev[devind].pd_busnr;
	dev= pcidev[devind].pd_dev;

	/* Fake a device with the required function */
	if (nr_pcidev >= NR_PCIDEV)
		panic("PCI","too many PCI devices", nr_pcidev);
	xdevind= nr_pcidev;
	pcidev[xdevind].pd_busnr= busnr;
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
 *				report_vga				     *
 *===========================================================================*/
PRIVATE void report_vga(devind)
int devind;
{
	/* Report the amount of video memory. This is needed by the X11R6
	 * postinstall script to chmem the X server. Hopefully this can be
	 * removed when we get virtual memory.
	 */
	size_t amount, size;
	int i;

	amount= 0;
	for (i= 0; i<pcidev[devind].pd_bar_nr; i++)
	{
		if (pcidev[devind].pd_bar[i].pb_flags & PBF_IO)
			continue;
		size= pcidev[devind].pd_bar[i].pb_size;
		if (size < amount)
			continue;
		amount= size;
	}
	if (size != 0)
	{
		printf("PCI: video memory for device at %d.%d.%d: %d bytes\n",
			pcidev[devind].pd_busnr,
			pcidev[devind].pd_dev,
			pcidev[devind].pd_func,
			amount);
	}
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
	int busnr, busind;

	busnr= pcidev[devind].pd_busnr;
	busind= get_busind(busnr);
	return pcibus[busind].pb_rsts(busind);
}
				

/*===========================================================================*
 *				pcibr_std_rsts				     *
 *===========================================================================*/
PRIVATE u16_t pcibr_std_rsts(busind)
int busind;
{
	int devind;

	devind= pcibus[busind].pb_devind;
	return pci_attr_r16(devind, PPB_SSTS);
}

/*===========================================================================*
 *				pcibr_std_wsts				     *
 *===========================================================================*/
PRIVATE void pcibr_std_wsts(busind, value)
int busind;
u16_t value;
{
	int devind;
	devind= pcibus[busind].pb_devind;

#if 0
	printf("pcibr_std_wsts(%d, 0x%X), devind= %d\n", 
		busind, value, devind);
#endif
	pci_attr_w16(devind, PPB_SSTS, value);
}

/*===========================================================================*
 *				pcibr_cb_rsts				     *
 *===========================================================================*/
PRIVATE u16_t pcibr_cb_rsts(busind)
int busind;
{
	int devind;
	devind= pcibus[busind].pb_devind;

	return pci_attr_r16(devind, CBB_SSTS);
}

/*===========================================================================*
 *				pcibr_cb_wsts				     *
 *===========================================================================*/
PRIVATE void pcibr_cb_wsts(busind, value)
int busind;
u16_t value;
{
	int devind;
	devind= pcibus[busind].pb_devind;

#if 0
	printf("pcibr_cb_wsts(%d, 0x%X), devind= %d\n", 
		busind, value, devind);
#endif
	pci_attr_w16(devind, CBB_SSTS, value);
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
	int busnr, busind;

	busnr= pcidev[devind].pd_busnr;
	busind= get_busind(busnr);
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

	v= PCII_RREG8_(pcibus[busind].pb_busnr, 
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

	v= PCII_RREG16_(pcibus[busind].pb_busnr, 
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

	v= PCII_RREG32_(pcibus[busind].pb_busnr, 
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
 *				pcii_wreg8				     *
 *===========================================================================*/
PRIVATE void pcii_wreg8(busind, devind, port, value)
int busind;
int devind;
int port;
u8_t value;
{
	int s;
#if 0
	printf("pcii_wreg8(%d, %d, 0x%X, 0x%X): %d.%d.%d\n",
		busind, devind, port, value,
		pcibus[busind].pb_bus, pcidev[devind].pd_dev,
		pcidev[devind].pd_func);
#endif
	PCII_WREG8_(pcibus[busind].pb_busnr, 
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
	PCII_WREG16_(pcibus[busind].pb_busnr, 
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
	PCII_WREG32_(pcibus[busind].pb_busnr, 
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

	v= PCII_RREG16_(pcibus[busind].pb_busnr, 0, 0, PCI_SR);
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
	PCII_WREG16_(pcibus[busind].pb_busnr, 0, 0, PCI_SR, value);
#if USER_SPACE
	if (OK != (s=sys_outl(PCII_CONFADD, PCII_UNSEL)))
		printf("PCI: warning, sys_outl failed: %d\n", s);
#else
	outl(PCII_CONFADD, PCII_UNSEL);
#endif
}


/*===========================================================================*
 *				print_capabilities			     *
 *===========================================================================*/
PRIVATE void print_capabilities(devind)
int devind;
{
	u8_t status, capptr, type, next;
	char *str;

	/* Check capabilities bit in the device status register */
	status= pci_attr_r16(devind, PCI_SR);
	if (!(status & PSR_CAPPTR))
		return;

	capptr= (pci_attr_r8(devind, PCI_CAPPTR) & PCI_CP_MASK);
	while (capptr != 0)
	{
		type = pci_attr_r8(devind, capptr+CAP_TYPE);
		next= (pci_attr_r8(devind, capptr+CAP_NEXT) & PCI_CP_MASK);
		switch(type)
		{
		case 1: str= "PCI Power Management"; break;
		case 2: str= "AGP"; break;
		case 3: str= "Vital Product Data"; break;
		case 4:	str= "Slot Identification"; break;
		case 5: str= "Message Signaled Interrupts"; break;
		case 6: str= "CompactPCI Hot Swap"; break;
		case 8: str= "AMD HyperTransport"; break;
		case 0xf: str= "AMD I/O MMU"; break;
		defuault: str= "(unknown type)"; break;
		}

		printf(" @0x%x: capability type 0x%x: %s\n",
			capptr, type, str);
		capptr= next;
	}
}

/*
 * $PchId: pci.c,v 1.7 2003/08/07 09:06:51 philip Exp $
 */
