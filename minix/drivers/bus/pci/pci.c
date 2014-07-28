/*
pci.c

Configure devices on the PCI bus

Created:	Jan 2000 by Philip Homburg <philip@cs.vu.nl>
*/

#include <assert.h>
#include <machine/pci.h>
#include <machine/vm.h>
#include <machine/vmparam.h>
#include <minix/com.h>
#include <minix/ds.h>
#include <minix/syslib.h>
#include <minix/param.h>

#include "pci.h"
#include <machine/pci_amd.h>
#include <machine/pci_intel.h>
#include <machine/pci_sis.h>
#include <machine/pci_via.h>
#if __minix_vmd
#include "config.h"
#endif

#if !__minix_vmd
#define irq_mode_pci(irq) ((void)0)
#endif

#include <stdlib.h>
#include <stdio.h>
#include <minix/sysutil.h>
#include <minix/acpi.h>

#define PBT_INTEL_HOST	 1
#define PBT_PCIBRIDGE	 2
#define PBT_CARDBUS	 3

#define BAM_NR		6	/* Number of base-address registers */

int debug= 0;

static struct pcibus
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
	void (*pb_wreg8)(int busind, int devind, int port, u8_t value);
	void (*pb_wreg16)(int busind, int devind, int port, u16_t value);
	void (*pb_wreg32)(int busind, int devind, int port, u32_t value);
	u16_t (*pb_rsts)(int busind);
	void (*pb_wsts)(int busind, u16_t value);
} pcibus[NR_PCIBUS];
static int nr_pcibus= 0;

static struct pcidev
{
	u8_t pd_busnr;
	u8_t pd_dev;
	u8_t pd_func;
	u8_t pd_baseclass;
	u8_t pd_subclass;
	u8_t pd_infclass;
	u16_t pd_vid;
	u16_t pd_did;
	u16_t pd_sub_vid;
	u16_t pd_sub_did;
	u8_t pd_ilr;

	u8_t pd_inuse;
	endpoint_t pd_proc;

	struct bar
	{
		int pb_flags;
		int pb_nr;
		u32_t pb_base;
		u32_t pb_size;
	} pd_bar[BAM_NR];
	int pd_bar_nr;
} pcidev[NR_PCIDEV];

EXTERN struct pci_acl pci_acl[NR_DRIVERS];

/* pb_flags */
#define PBF_IO		1	/* I/O else memory */
#define PBF_INCOMPLETE	2	/* not allocated */

static int nr_pcidev= 0;

static void pci_intel_init(void);
static void probe_bus(int busind);
static int is_duplicate(u8_t busnr, u8_t dev, u8_t func);
static void record_irq(int devind);
static void record_bars_normal(int devind);
static void record_bars_bridge(int devind);
static void record_bars_cardbus(int devind);
static void record_bars(int devind, int last_reg);
static int record_bar(int devind, int bar_nr, int last);
static void complete_bridges(void);
static void complete_bars(void);
static void update_bridge4dev_io(int devind, u32_t io_base, u32_t
	io_size);
static int get_freebus(void);
static int do_isabridge(int busind);
static void do_pcibridge(int busind);
static int get_busind(int busnr);
static int do_piix(int devind);
static int do_amd_isabr(int devind);
static int do_sis_isabr(int devind);
static int do_via_isabr(int devind);
#if 0
static void report_vga(int devind);
#endif
static char *pci_vid_name(u16_t vid);
static char *pci_baseclass_name(u8_t baseclass);
static char *pci_subclass_name(u8_t baseclass, u8_t subclass, u8_t
	infclass);
static void ntostr(unsigned n, char **str, const char *end);

static u8_t pci_attr_r8_u(int devind, int port);
static u32_t pci_attr_r32_u(int devind, int port);

static u16_t pci_attr_rsts(int devind);
static void pci_attr_wsts(int devind, u16_t value);
static u16_t pcibr_std_rsts(int busind);
static void pcibr_std_wsts(int busind, u16_t value);
static u16_t pcibr_cb_rsts(int busind);
static void pcibr_cb_wsts(int busind, u16_t value);
static u16_t pcibr_via_rsts(int busind);
static void pcibr_via_wsts(int busind, u16_t value);
static u8_t pcii_rreg8(int busind, int devind, int port);
static u16_t pcii_rreg16(int busind, int devind, int port);
static u32_t pcii_rreg32(int busind, int devind, int port);
static void pcii_wreg8(int busind, int devind, int port, u8_t value);
static void pcii_wreg16(int busind, int devind, int port, u16_t value);
static void pcii_wreg32(int busind, int devind, int port, u32_t value);
static u16_t pcii_rsts(int busind);
static void pcii_wsts(int busind, u16_t value);
static void print_capabilities(int devind);
static int visible(struct rs_pci *aclp, int devind);
static void print_hyper_cap(int devind, u8_t capptr);

static struct machine machine;
static endpoint_t acpi_ep;

/*===========================================================================*
 *				sef_cb_init_fresh			     *
 *===========================================================================*/
int sef_cb_init_fresh(int type, sef_init_info_t *info)
{
/* Initialize the pci driver. */
	long v;
	int i, r;
	struct rprocpub rprocpub[NR_BOOT_PROCS];

	v= 0;
	env_parse("pci_debug", "d", 0, &v, 0, 1);
	debug= v;

	if (sys_getmachine(&machine)) {
		printf("PCI: no machine\n");
		return ENODEV;
	}
	if (machine.apic_enabled &&
			ds_retrieve_label_endpt("acpi", &acpi_ep) != OK) {
		panic("PCI: Cannot use APIC mode without ACPI!\n");
	}

	/* Only Intel (compatible) PCI controllers are supported at the
	 * moment.
	 */
	pci_intel_init();

	/* Map all the services in the boot image. */
	if((r = sys_safecopyfrom(RS_PROC_NR, info->rproctab_gid, 0,
		(vir_bytes) rprocpub, sizeof(rprocpub))) != OK) {
		panic("sys_safecopyfrom failed: %d", r);
	}
	for(i=0;i < NR_BOOT_PROCS;i++) {
		if(rprocpub[i].in_use) {
			if((r = map_service(&rprocpub[i])) != OK) {
				panic("unable to map service: %d", r);
			}
		}
	}

	return(OK);
}

/*===========================================================================*
 *		               map_service                                   *
 *===========================================================================*/
int map_service(rpub)
struct rprocpub *rpub;
{
/* Map a new service by registering a new acl entry if required. */
	int i;

	/* Stop right now if no pci device or class is found. */
	if(rpub->pci_acl.rsp_nr_device == 0
		&& rpub->pci_acl.rsp_nr_class == 0) {
		return(OK);
	}

	/* Find a free acl slot. */
	for (i= 0; i<NR_DRIVERS; i++)
	{
		if (!pci_acl[i].inuse)
			break;
	}
	if (i >= NR_DRIVERS)
	{
		printf("PCI: map_service: table is full\n");
		return ENOMEM;
	}

	/* Initialize acl slot. */
	pci_acl[i].inuse = 1;
	pci_acl[i].acl = rpub->pci_acl;

	return(OK);
}

/*===========================================================================*
 *			helper functions for I/O			     *
 *===========================================================================*/
unsigned pci_inb(u16_t port) {
	u32_t value;
	int s;
	if ((s=sys_inb(port, &value)) !=OK)
		printf("PCI: warning, sys_inb failed: %d\n", s);
	return value;
}
unsigned pci_inw(u16_t port) {
	u32_t value;
	int s;
	if ((s=sys_inw(port, &value)) !=OK)
		printf("PCI: warning, sys_inw failed: %d\n", s);
	return value;
}
unsigned pci_inl(u16_t port) {
	u32_t value;
	int s;
	if ((s=sys_inl(port, &value)) !=OK)
		printf("PCI: warning, sys_inl failed: %d\n", s);
	return value;
}
void pci_outb(u16_t port, u8_t value) {
	int s;
	if ((s=sys_outb(port, value)) !=OK)
		printf("PCI: warning, sys_outb failed: %d\n", s);
}
void pci_outw(u16_t port, u16_t value) {
	int s;
	if ((s=sys_outw(port, value)) !=OK)
		printf("PCI: warning, sys_outw failed: %d\n", s);
}
void pci_outl(u16_t port, u32_t value) {
	int s;
	if ((s=sys_outl(port, value)) !=OK)
		printf("PCI: warning, sys_outl failed: %d\n", s);
}

/*===========================================================================*
 *				pci_find_dev				     *
 *===========================================================================*/
int pci_find_dev(u8_t bus, u8_t dev, u8_t func, int *devindp)
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
#if 0
	if (pcidev[devind].pd_inuse)
		return 0;
#endif
	*devindp= devind;
	return 1;
}

/*===========================================================================*
 *				pci_first_dev_a				     *
 *===========================================================================*/
int pci_first_dev_a(
  struct rs_pci *aclp,
  int *devindp,
  u16_t *vidp,
  u16_t *didp
)
{
	int devind;

	for (devind= 0; devind < nr_pcidev; devind++)
	{
#if 0
		if (pcidev[devind].pd_inuse)
			continue;
#endif
		if (!visible(aclp, devind))
			continue;
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
int pci_next_dev_a(
  struct rs_pci *aclp,
  int *devindp,
  u16_t *vidp,
  u16_t *didp
)
{
	int devind;

	for (devind= *devindp+1; devind < nr_pcidev; devind++)
	{
#if 0
		if (pcidev[devind].pd_inuse)
			continue;
#endif
		if (!visible(aclp, devind))
			continue;
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
 *				pci_reserve_a				     *
 *===========================================================================*/
int pci_reserve_a(devind, proc, aclp)
int devind;
endpoint_t proc;
struct rs_pci *aclp;
{
	int i, r;
	int ilr;
	struct io_range ior;
	struct minix_mem_range mr;

	if (devind < 0 || devind >= nr_pcidev)
	{
		printf("pci_reserve_a: bad devind: %d\n", devind);
		return EINVAL;
	}
	if (!visible(aclp, devind))
	{
		printf("pci_reserve_a: %u is not allowed to reserve %d\n",
			proc, devind);
		return EPERM;
	}

	if(pcidev[devind].pd_inuse && pcidev[devind].pd_proc != proc)
		return EBUSY;
	pcidev[devind].pd_inuse= 1;
	pcidev[devind].pd_proc= proc;

	for (i= 0; i<pcidev[devind].pd_bar_nr; i++)
	{
		if (pcidev[devind].pd_bar[i].pb_flags & PBF_INCOMPLETE)
		{
			printf("pci_reserve_a: BAR %d is incomplete\n", i);
			continue;
		}
		if (pcidev[devind].pd_bar[i].pb_flags & PBF_IO)
		{
			ior.ior_base= pcidev[devind].pd_bar[i].pb_base;
			ior.ior_limit= ior.ior_base +
				pcidev[devind].pd_bar[i].pb_size-1;

			if(debug) {
			   printf(
		"pci_reserve_a: for proc %d, adding I/O range [0x%x..0x%x]\n",
				proc, ior.ior_base, ior.ior_limit);
			}
			r= sys_privctl(proc, SYS_PRIV_ADD_IO, &ior);
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

			r= sys_privctl(proc, SYS_PRIV_ADD_MEM, &mr);
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
		if(debug) printf("pci_reserve_a: adding IRQ %d\n", ilr);
		r= sys_privctl(proc, SYS_PRIV_ADD_IRQ, &ilr);
		if (r != OK)
		{
			printf("sys_privctl failed for proc %d: %d\n",
				proc, r);
		}
	}

	return OK;
}

/*===========================================================================*
 *				pci_release				     *
 *===========================================================================*/
void pci_release(proc)
endpoint_t proc;
{
	int i;

	for (i= 0; i<nr_pcidev; i++)
	{
		if (!pcidev[i].pd_inuse)
			continue;
		if (pcidev[i].pd_proc != proc)
			continue;
		pcidev[i].pd_inuse= 0;
	}
}

/*===========================================================================*
 *				pci_ids_s				     *
 *===========================================================================*/
int pci_ids_s(int devind, u16_t *vidp, u16_t *didp)
{
	if (devind < 0 || devind >= nr_pcidev)
		return EINVAL;

	*vidp= pcidev[devind].pd_vid;
	*didp= pcidev[devind].pd_did;
	return OK;
}

/*===========================================================================*
 *				pci_rescan_bus				     *
 *===========================================================================*/
void pci_rescan_bus(u8_t busnr)
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
 *				pci_slot_name_s				     *
 *===========================================================================*/
int pci_slot_name_s(devind, cpp)
int devind;
char **cpp;
{
	static char label[]= "ddd.ddd.ddd";
	char *end;
	char *p;

	if (devind < 0 || devind >= nr_pcidev)
		return EINVAL;

	p= label;
	end= label+sizeof(label);

	ntostr(pcidev[devind].pd_busnr, &p, end);
	*p++= '.';

	ntostr(pcidev[devind].pd_dev, &p, end);
	*p++= '.';

	ntostr(pcidev[devind].pd_func, &p, end);

	*cpp= label;
	return OK;
}

/*===========================================================================*
 *				pci_dev_name				     *
 *===========================================================================*/
char *pci_dev_name(u16_t vid, u16_t did)
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
 *				pci_get_bar_s				     *
 *===========================================================================*/
int pci_get_bar_s(int devind, int port, u32_t *base, u32_t *size,
	int *ioflag)
{
	int i, reg;

	if (devind < 0 || devind >= nr_pcidev)
		return EINVAL;

	for (i= 0; i < pcidev[devind].pd_bar_nr; i++)
	{
		reg= PCI_BAR+4*pcidev[devind].pd_bar[i].pb_nr;

		if (reg == port)
		{
			if (pcidev[devind].pd_bar[i].pb_flags & PBF_INCOMPLETE)
				return EINVAL;

			*base= pcidev[devind].pd_bar[i].pb_base;
			*size= pcidev[devind].pd_bar[i].pb_size;
			*ioflag=
				!!(pcidev[devind].pd_bar[i].pb_flags & PBF_IO);
			return OK;
		}
	}
	return EINVAL;
}

/*===========================================================================*
 *				pci_attr_r8_s				     *
 *===========================================================================*/
int pci_attr_r8_s(int devind, int port, u8_t *vp)
{
	if (devind < 0 || devind >= nr_pcidev)
		return EINVAL;
	if (port < 0 || port > 255)
		return EINVAL;

	*vp= pci_attr_r8_u(devind, port);
	return OK;
}

/*===========================================================================*
 *				pci_attr_r8_u				     *
 *===========================================================================*/
static u8_t pci_attr_r8_u(devind, port)
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
u16_t pci_attr_r16(devind, port)
int devind;
int port;
{
	int busnr, busind;

	busnr= pcidev[devind].pd_busnr;
	busind= get_busind(busnr);
	return pcibus[busind].pb_rreg16(busind, devind, port);
}

/*===========================================================================*
 *				pci_attr_r32_s				     *
 *===========================================================================*/
int pci_attr_r32_s(int devind, int port, u32_t *vp)
{
	if (devind < 0 || devind >= nr_pcidev)
		return EINVAL;
	if (port < 0 || port > 256-4)
		return EINVAL;

	*vp= pci_attr_r32_u(devind, port);
	return OK;
}

/*===========================================================================*
 *				pci_attr_r32_u				     *
 *===========================================================================*/
static u32_t pci_attr_r32_u(devind, port)
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
void pci_attr_w8(int devind, int port, u8_t value)
{
	int busnr, busind;

	busnr= pcidev[devind].pd_busnr;
	busind= get_busind(busnr);
	pcibus[busind].pb_wreg8(busind, devind, port, value);
}

/*===========================================================================*
 *				pci_attr_w16				     *
 *===========================================================================*/
void pci_attr_w16(int devind, int port, u16_t value)
{
	int busnr, busind;

	busnr= pcidev[devind].pd_busnr;
	busind= get_busind(busnr);
	pcibus[busind].pb_wreg16(busind, devind, port, value);
}

/*===========================================================================*
 *				pci_attr_w32				     *
 *===========================================================================*/
void pci_attr_w32(int devind, int port, u32_t value)
{
	int busnr, busind;

	busnr= pcidev[devind].pd_busnr;
	busind= get_busind(busnr);
	pcibus[busind].pb_wreg32(busind, devind, port, value);
}

/*===========================================================================*
 *				pci_intel_init				     *
 *===========================================================================*/
static void pci_intel_init()
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
	if (OK != (s=sys_outl(PCII_CONFADD, PCII_UNSEL)))
		printf("PCI: warning, sys_outl failed: %d\n", s);

#if 0
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
#endif

	if (nr_pcibus >= NR_PCIBUS)
		panic("too many PCI busses: %d", nr_pcibus);
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
		printf("pci_intel_init: %s (%04X:%04X)\n",
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
static void probe_bus(int busind)
{
	u32_t dev, func, t3;
	u16_t vid, did, sts, sub_vid, sub_did;
	u8_t headt;
	u8_t baseclass, subclass, infclass;
	int devind, busnr;
	char *s, *dstr;

	if (debug)
		printf("probe_bus(%d)\n", busind);
	if (nr_pcidev >= NR_PCIDEV)
		panic("too many PCI devices: %d", nr_pcidev);
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
			headt= pci_attr_r8_u(devind, PCI_HEADT);
			sts= pci_attr_rsts(devind);

#if 0
			printf("vid 0x%x, did 0x%x, headt 0x%x, sts 0x%x\n",
				vid, did, headt, sts);
#endif

			if (vid == NO_VID && did == NO_VID)
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
			    static int warned = 0;

			    if(!warned) {
				printf(
					"PCI: ignoring bad value 0x%x in sts for QEMU\n",
					sts & (PSR_SSE|PSR_RMAS|PSR_RTAS));
				warned = 1;
			    }
			}

			sub_vid= pci_attr_r16(devind, PCI_SUBVID);
			sub_did= pci_attr_r16(devind, PCI_SUBDID);

			dstr= pci_dev_name(vid, did);
			if (debug)
			{
				if (dstr)
				{
					printf("%d.%lu.%lu: %s (%04X:%04X)\n",
						busnr, (unsigned long)dev,
						(unsigned long)func, dstr,
						vid, did);
				}
				else
				{
					printf(
		"%d.%lu.%lu: Unknown device, vendor %04X (%s), device %04X\n",
						busnr, (unsigned long)dev,
						(unsigned long)func, vid,
						pci_vid_name(vid), did);
				}
				printf("Device index: %d\n", devind);
				printf("Subsystem: Vid 0x%x, did 0x%x\n",
					sub_vid, sub_did);
			}

			baseclass= pci_attr_r8_u(devind, PCI_BCR);
			subclass= pci_attr_r8_u(devind, PCI_SCR);
			infclass= pci_attr_r8_u(devind, PCI_PIFR);
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
			pcidev[devind].pd_sub_vid= sub_vid;
			pcidev[devind].pd_sub_did= sub_did;
			pcidev[devind].pd_inuse= 0;
			pcidev[devind].pd_bar_nr= 0;
			record_irq(devind);
			switch(headt & PHT_MASK)
			{
			case PHT_NORMAL:
				record_bars_normal(devind);
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
#if 0
			if (t3 == PCI_T3_VGA || t3 == PCI_T3_VGA_OLD)
				report_vga(devind);
#endif

			if (nr_pcidev >= NR_PCIDEV)
			  panic("too many PCI devices: %d", nr_pcidev);
			devind= nr_pcidev;

			if (func == 0 && !(headt & PHT_MULTIFUNC))
				break;
		}
	}
}

/*===========================================================================*
 *				is_duplicate				     *
 *===========================================================================*/
static int is_duplicate(u8_t busnr, u8_t dev, u8_t func)
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

static int acpi_get_irq(unsigned bus, unsigned dev, unsigned pin)
{
	int err;
	message m;

	((struct acpi_get_irq_req *)&m)->hdr.request = ACPI_REQ_GET_IRQ;
	((struct acpi_get_irq_req *)&m)->bus = bus;
	((struct acpi_get_irq_req *)&m)->dev = dev;
	((struct acpi_get_irq_req *)&m)->pin = pin;

	if ((err = ipc_sendrec(acpi_ep, &m)) != OK)
		panic("PCI: error %d while receiveing from ACPI\n", err);

	return ((struct acpi_get_irq_resp *)&m)->irq;
}

static int derive_irq(struct pcidev * dev, int pin)
{
	struct pcidev * parent_bridge;
	int slot;
	
	parent_bridge = &pcidev[pcibus[get_busind(dev->pd_busnr)].pb_devind];

	/*
	 * We don't support PCI-Express, no ARI, decode the slot of the device
	 * and mangle the pin as the device is behind a bridge
	 */
	slot = ((dev->pd_func) >> 3) & 0x1f;

	return acpi_get_irq(parent_bridge->pd_busnr,
			parent_bridge->pd_dev, (pin + slot) % 4);
}

/*===========================================================================*
 *				record_irq				     *
 *===========================================================================*/
static void record_irq(devind)
int devind;
{
	int ilr, ipr, busnr, busind, cb_devind;

	ilr= pci_attr_r8_u(devind, PCI_ILR);
	ipr= pci_attr_r8_u(devind, PCI_IPR);

	if (ipr && machine.apic_enabled) {
		int irq;

		irq = acpi_get_irq(pcidev[devind].pd_busnr,
				pcidev[devind].pd_dev, ipr - 1);

		if (irq < 0)
			irq = derive_irq(&pcidev[devind], ipr - 1);

		if (irq >= 0) {
			ilr = irq;
			pci_attr_w8(devind, PCI_ILR, ilr);
			if (debug)
				printf("PCI: ACPI IRQ %d for "
						"device %d.%d.%d INT%c\n",
						irq,
						pcidev[devind].pd_busnr,
						pcidev[devind].pd_dev,
						pcidev[devind].pd_func,
						'A' + ipr-1);
		}
		else if (debug) {
			printf("PCI: no ACPI IRQ routing for "
					"device %d.%d.%d INT%c\n",
					pcidev[devind].pd_busnr,
					pcidev[devind].pd_dev,
					pcidev[devind].pd_func,
					'A' + ipr-1);
		}
	}

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
 *				record_bars_normal			     *
 *===========================================================================*/
static void record_bars_normal(devind)
int devind;
{
	int i, j, clear_01, clear_23, pb_nr;

	/* The BAR area of normal devices is six DWORDs in size. */
	record_bars(devind, PCI_BAR_6);

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
	"secondary channel is not in native mode, clearing BARs 2 and 3\n");
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
			{
				j++;
				continue;	/* No need to copy */
			}
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
static void record_bars_bridge(devind)
int devind;
{
	u32_t base, limit, size;

	/* The generic BAR area of PCI-to-PCI bridges is two DWORDs in size.
	 * It may contain up to two 32-bit BARs, or one 64-bit BAR.
	 */
	record_bars(devind, PCI_BAR_2);

	base= ((pci_attr_r8_u(devind, PPB_IOBASE) & PPB_IOB_MASK) << 8) |
		(pci_attr_r16(devind, PPB_IOBASEU16) << 16);
	limit= 0xff |
		((pci_attr_r8_u(devind, PPB_IOLIMIT) & PPB_IOL_MASK) << 8) |
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
static void record_bars_cardbus(devind)
int devind;
{
	u32_t base, limit, size;

	/* The generic BAR area of CardBus devices is one DWORD in size. */
	record_bars(devind, PCI_BAR);

	base= pci_attr_r32_u(devind, CBB_MEMBASE_0);
	limit= pci_attr_r32_u(devind, CBB_MEMLIMIT_0) |
		(~CBB_MEML_MASK & 0xffffffff);
	size= limit-base + 1;
	if (debug)
	{
		printf("\tMemory window 0: base 0x%x, limit 0x%x, size %d\n",
			base, limit, size);
	}

	base= pci_attr_r32_u(devind, CBB_MEMBASE_1);
	limit= pci_attr_r32_u(devind, CBB_MEMLIMIT_1) |
		(~CBB_MEML_MASK & 0xffffffff);
	size= limit-base + 1;
	if (debug)
	{
		printf("\tMemory window 1: base 0x%x, limit 0x%x, size %d\n",
			base, limit, size);
	}

	base= pci_attr_r32_u(devind, CBB_IOBASE_0);
	limit= pci_attr_r32_u(devind, CBB_IOLIMIT_0) |
		(~CBB_IOL_MASK & 0xffffffff);
	size= limit-base + 1;
	if (debug)
	{
		printf("\tI/O window 0: base 0x%x, limit 0x%x, size %d\n",
			base, limit, size);
	}

	base= pci_attr_r32_u(devind, CBB_IOBASE_1);
	limit= pci_attr_r32_u(devind, CBB_IOLIMIT_1) |
		(~CBB_IOL_MASK & 0xffffffff);
	size= limit-base + 1;
	if (debug)
	{
		printf("\tI/O window 1: base 0x%x, limit 0x%x, size %d\n",
			base, limit, size);
	}
}

/*===========================================================================*
 *				record_bars				     *
 *===========================================================================*/
static void record_bars(int devind, int last_reg)
{
	int i, reg, width;

	for (i= 0, reg= PCI_BAR; reg <= last_reg; i += width, reg += 4 * width)
	{
		width = record_bar(devind, i, reg == last_reg);
	}
}

/*===========================================================================*
 *				record_bar				     *
 *===========================================================================*/
static int record_bar(devind, bar_nr, last)
int devind;
int bar_nr;
int last;
{
	int reg, prefetch, type, dev_bar_nr, width;
	u32_t bar, bar2;
	u16_t cmd;

	/* Start by assuming that this is a 32-bit bar, taking up one DWORD. */
	width = 1;

	reg= PCI_BAR+4*bar_nr;

	bar= pci_attr_r32_u(devind, reg);
	if (bar & PCI_BAR_IO)
	{
		/* Disable I/O access before probing for BAR's size */
		cmd = pci_attr_r16(devind, PCI_CR);
		pci_attr_w16(devind, PCI_CR, cmd & ~PCI_CR_IO_EN);

		/* Probe BAR's size */
		pci_attr_w32(devind, reg, 0xffffffff);
		bar2= pci_attr_r32_u(devind, reg);

		/* Restore original state */
		pci_attr_w32(devind, reg, bar);
		pci_attr_w16(devind, PCI_CR, cmd);

		bar &= PCI_BAR_IO_MASK;		/* Clear non-address bits */
		bar2 &= PCI_BAR_IO_MASK;
		bar2= (~bar2 & 0xffff)+1;
		if (debug)
		{
			printf("\tbar_%d: %d bytes at 0x%x I/O\n",
				bar_nr, bar2, bar);
		}

		dev_bar_nr= pcidev[devind].pd_bar_nr++;
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
		type= (bar & PCI_BAR_TYPE);

		switch(type) {
		case PCI_TYPE_32:
		case PCI_TYPE_32_1M:
			break;

		case PCI_TYPE_64:
			/* A 64-bit BAR takes up two consecutive DWORDs. */
			if (last)
			{
				printf("PCI: device %d.%d.%d BAR %d extends"
					" beyond designated area\n",
					pcidev[devind].pd_busnr,
					pcidev[devind].pd_dev,
					pcidev[devind].pd_func, bar_nr);

				return width;
			}
			width++;

			bar2= pci_attr_r32_u(devind, reg+4);

			/* If the upper 32 bits of the BAR are not zero, the
			 * memory is inaccessible to us; ignore the BAR.
			 */
			if (bar2 != 0)
			{
				if (debug)
				{
					printf("\tbar_%d: (64-bit BAR with"
						" high bits set)\n", bar_nr);
				}

				return width;
			}

			break;

		default:
			/* Ignore the BAR. */
			if (debug)
			{
				printf("\tbar_%d: (unknown type %x)\n",
					bar_nr, type);
			}

			return width;
		}

		/* Disable mem access before probing for BAR's size */
		cmd = pci_attr_r16(devind, PCI_CR);
		pci_attr_w16(devind, PCI_CR, cmd & ~PCI_CR_MEM_EN);

		/* Probe BAR's size */
		pci_attr_w32(devind, reg, 0xffffffff);
		bar2= pci_attr_r32_u(devind, reg);

		/* Restore original values */
		pci_attr_w32(devind, reg, bar);
		pci_attr_w16(devind, PCI_CR, cmd);

		if (bar2 == 0)
			return width;	/* Reg. is not implemented */

		prefetch= !!(bar & PCI_BAR_PREFETCH);
		bar &= PCI_BAR_MEM_MASK;	/* Clear non-address bits */
		bar2 &= PCI_BAR_MEM_MASK;
		bar2= (~bar2)+1;
		if (debug)
		{
			printf("\tbar_%d: 0x%x bytes at 0x%x%s memory%s\n",
				bar_nr, bar2, bar,
				prefetch ? " prefetchable" : "",
				type == PCI_TYPE_64 ? ", 64-bit" : "");
		}

		dev_bar_nr= pcidev[devind].pd_bar_nr++;
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

	return width;
}

/*===========================================================================*
 *				complete_bridges			     *
 *===========================================================================*/
static void complete_bridges()
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
		printf("SECBLT = 0x%x\n", pci_attr_r8_u(devind, PPB_SECBLT));
		printf("BRIDGECTRL = 0x%x\n",
			pci_attr_r16(devind, PPB_BRIDGECTRL));
	}
}

/*===========================================================================*
 *				complete_bars				     *
 *===========================================================================*/
static void complete_bars(void)
{
	int i, j, bar_nr, reg;
	u32_t memgap_low, memgap_high, iogap_low, iogap_high, io_high,
		base, size, v32, diff1, diff2;
	kinfo_t kinfo;

	if(OK != sys_getkinfo(&kinfo))
		panic("can't get kinfo");

	/* Set memgap_low to just above physical memory */
	memgap_low= kinfo.mem_high_phys;
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
		printf("PCI: bad memory gap: [0x%x .. 0x%x>\n",
			memgap_low, memgap_high);
		panic(NULL);
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
		"pci device %d (%04x:%04x), bar %d: base 0x%x, size 0x%x\n",
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
			panic("iogap_high too low: %d", iogap_high);
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
				panic("memory base too low: %d", base);
			memgap_high= base;
			bar_nr= pcidev[i].pd_bar[j].pb_nr;
			reg= PCI_BAR + 4*bar_nr;
			v32= pci_attr_r32_u(i, reg);
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
				panic("I/O base too low: %d", base);

			iogap_high= base;
			bar_nr= pcidev[i].pd_bar[j].pb_nr;
			reg= PCI_BAR + 4*bar_nr;
			v32= pci_attr_r32_u(i, reg);
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
	return;
}

/*===========================================================================*
 *				update_bridge4dev_io			     *
 *===========================================================================*/
static void update_bridge4dev_io(
  int devind,
  u32_t io_base,
  u32_t io_size
)
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
		panic("update_bridge4dev_io: strange bus type: %d", type);

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
static int get_freebus()
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
static int do_isabridge(busind)
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
			printf("found ISA bridge (%04X:%04X) %s\n",
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
			panic("unknown ISA bridge type: %d", type);
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
		"(warning) unsupported ISA bridge %04X:%04X for bus %d\n",
			pcidev[unknown_bridge].pd_vid,
			pcidev[unknown_bridge].pd_did, busind);
	}
	return 0;
}

/*
 * tells acpi which two busses are connected by this bridge. The primary bus
 * (pbnr) must be already known to acpi and it must map dev as the connection to
 * the secondary (sbnr) bus
 */
static void acpi_map_bridge(unsigned pbnr, unsigned dev, unsigned sbnr)
{
	int err;
	message m;

	((struct acpi_map_bridge_req *)&m)->hdr.request = ACPI_REQ_MAP_BRIDGE;
	((struct acpi_map_bridge_req *)&m)->primary_bus = pbnr;
	((struct acpi_map_bridge_req *)&m)->secondary_bus = sbnr;
	((struct acpi_map_bridge_req *)&m)->device = dev;

	if ((err = ipc_sendrec(acpi_ep, &m)) != OK)
		panic("PCI: error %d while receiveing from ACPI\n", err);

	if (((struct acpi_map_bridge_resp *)&m)->err != OK)
		printf("PCI: acpi failed to map pci (%d) to pci (%d) bridge\n",
								pbnr, sbnr);
}

/*===========================================================================*
 *				do_pcibridge				     *
 *===========================================================================*/
static void do_pcibridge(busind)
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
			pcidev[devind].pd_busnr, pcidev[devind].pd_dev, 
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
			headt= pci_attr_r8_u(devind, PCI_HEADT);
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

			baseclass= pci_attr_r8_u(devind, PCI_BCR);
			subclass= pci_attr_r8_u(devind, PCI_SCR);
			infclass= pci_attr_r8_u(devind, PCI_PIFR);
			t3= ((baseclass << 16) | (subclass << 8) | infclass);
			if (type == PCI_PPB_STD &&
				t3 != PCI_T3_PCI2PCI &&
				t3 != PCI_T3_PCI2PCI_SUBTR)
			{
				printf(
"Unknown PCI class %02x/%02x/%02x for PCI-to-PCI bridge, device %04X:%04X\n",
					baseclass, subclass, infclass,
					vid, did);
				continue;
			 }
			if (type == PCI_PPB_CB &&
				t3 != PCI_T3_CARDBUS)
			{
				printf(
"Unknown PCI class %02x/%02x/%02x for Cardbus bridge, device %04X:%04X\n",
					baseclass, subclass, infclass,
					vid, did);
				continue;
			 }
		}

		if (debug)
		{
			printf("%u.%u.%u: PCI-to-PCI bridge: %04X:%04X\n",
				pcidev[devind].pd_busnr,
				pcidev[devind].pd_dev, 
				pcidev[devind].pd_func, vid, did);
		}

		/* Assume that the BIOS initialized the secondary bus
		 * number.
		 */
		sbusn= pci_attr_r8_u(devind, PPB_SECBN);

		if (nr_pcibus >= NR_PCIBUS)
			panic("too many PCI busses: %d", nr_pcibus);
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
		    panic("unknown PCI-PCI bridge type: %d", type);
		}

		if (machine.apic_enabled)
			acpi_map_bridge(pcidev[devind].pd_busnr,
					pcidev[devind].pd_dev, sbusn);

		if (debug)
		{
			printf(
			"bus(table) = %d, bus(sec) = %d, bus(subord) = %d\n",
				ind, sbusn, pci_attr_r8_u(devind, PPB_SUBORDBN));
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
static int get_busind(busnr)
int busnr;
{
	int i;

	for (i= 0; i<nr_pcibus; i++)
	{
		if (pcibus[i].pb_busnr == busnr)
			return i;
	}
	panic("get_busind: can't find bus: %d", busnr);
}

/*===========================================================================*
 *				do_piix					     *
 *===========================================================================*/
static int do_piix(int devind)
{
	int i, s, irqrc, irq;
	u32_t elcr1, elcr2, elcr;

#if DEBUG
	printf("in piix\n");
#endif
	if (OK != (s=sys_inb(PIIX_ELCR1, &elcr1)))
		printf("Warning, sys_inb failed: %d\n", s);
	if (OK != (s=sys_inb(PIIX_ELCR2, &elcr2)))
		printf("Warning, sys_inb failed: %d\n", s);
	elcr= elcr1 | (elcr2 << 8);
	for (i= 0; i<4; i++)
	{
		irqrc= pci_attr_r8_u(devind, PIIX_PIRQRCA+i);
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
static int do_amd_isabr(int devind)
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
		panic("too many PCI devices: %d", nr_pcidev);
	xdevind= nr_pcidev;
	pcidev[xdevind].pd_busnr= busnr;
	pcidev[xdevind].pd_dev= dev;
	pcidev[xdevind].pd_func= func;
	pcidev[xdevind].pd_inuse= 1;
	nr_pcidev++;

	levmask= pci_attr_r8_u(xdevind, AMD_ISABR_PCIIRQ_LEV);
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
static int do_sis_isabr(int devind)
{
	int i, irq;

	irq= 0;	/* lint */
	for (i= 0; i<4; i++)
	{
		irq= pci_attr_r8_u(devind, SIS_ISABR_IRQ_A+i);
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
static int do_via_isabr(int devind)
{
	int i, irq, edge;
	u8_t levmask;

	levmask= pci_attr_r8_u(devind, VIA_ISABR_EL);
	irq= 0;	/* lint */
	edge= 0; /* lint */
	for (i= 0; i<4; i++)
	{
		switch(i)
		{
		case 0:
			edge= (levmask & VIA_ISABR_EL_INTA);
			irq= pci_attr_r8_u(devind, VIA_ISABR_IRQ_R2) >> 4;
			break;
		case 1:
			edge= (levmask & VIA_ISABR_EL_INTB);
			irq= pci_attr_r8_u(devind, VIA_ISABR_IRQ_R2);
			break;
		case 2:
			edge= (levmask & VIA_ISABR_EL_INTC);
			irq= pci_attr_r8_u(devind, VIA_ISABR_IRQ_R3) >> 4;
			break;
		case 3:
			edge= (levmask & VIA_ISABR_EL_INTD);
			irq= pci_attr_r8_u(devind, VIA_ISABR_IRQ_R1) >> 4;
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


#if 0
/*===========================================================================*
 *				report_vga				     *
 *===========================================================================*/
static void report_vga(devind)
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
#endif


/*===========================================================================*
 *				pci_vid_name				     *
 *===========================================================================*/
static char *pci_vid_name(u16_t vid)
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
static char *pci_baseclass_name(u8_t baseclass)
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
static char *pci_subclass_name(u8_t baseclass, u8_t subclass, u8_t infclass)
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
static void ntostr(n, str, end)
unsigned n;
char **str;
const char *end;
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
		(*str)[-1]= '\0';
	else
		**str= '\0';
}

/*===========================================================================*
 *				pci_attr_rsts				     *
 *===========================================================================*/
static u16_t pci_attr_rsts(devind)
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
static u16_t pcibr_std_rsts(busind)
int busind;
{
	int devind;

	devind= pcibus[busind].pb_devind;
	return pci_attr_r16(devind, PPB_SSTS);
}

/*===========================================================================*
 *				pcibr_std_wsts				     *
 *===========================================================================*/
static void pcibr_std_wsts(int busind, u16_t value)
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
static u16_t pcibr_cb_rsts(busind)
int busind;
{
	int devind;
	devind= pcibus[busind].pb_devind;

	return pci_attr_r16(devind, CBB_SSTS);
}

/*===========================================================================*
 *				pcibr_cb_wsts				     *
 *===========================================================================*/
static void pcibr_cb_wsts(int busind, u16_t value)
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
static u16_t pcibr_via_rsts(int busind)
{
	return 0;
}

/*===========================================================================*
 *				pcibr_via_wsts				     *
 *===========================================================================*/
static void pcibr_via_wsts(int busind, u16_t value)
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
static void pci_attr_wsts(int devind, u16_t value)
{
	int busnr, busind;

	busnr= pcidev[devind].pd_busnr;
	busind= get_busind(busnr);
	pcibus[busind].pb_wsts(busind, value);
}
				

/*===========================================================================*
 *				pcii_rreg8				     *
 *===========================================================================*/
static u8_t pcii_rreg8(busind, devind, port)
int busind;
int devind;
int port;
{
	u8_t v;
	int s;

	v= PCII_RREG8_(pcibus[busind].pb_busnr, 
		pcidev[devind].pd_dev, pcidev[devind].pd_func,
		port);
	if (OK != (s=sys_outl(PCII_CONFADD, PCII_UNSEL)))
		printf("PCI: warning, sys_outl failed: %d\n", s);
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
static u16_t pcii_rreg16(int busind, int devind, int port)
{
	u16_t v;
	int s;

	v= PCII_RREG16_(pcibus[busind].pb_busnr, 
		pcidev[devind].pd_dev, pcidev[devind].pd_func,
		port);
	if (OK != (s=sys_outl(PCII_CONFADD, PCII_UNSEL)))
		printf("PCI: warning, sys_outl failed: %d\n", s);
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
static u32_t pcii_rreg32(int busind, int devind, int port)
{
	u32_t v;
	int s;

	v= PCII_RREG32_(pcibus[busind].pb_busnr, 
		pcidev[devind].pd_dev, pcidev[devind].pd_func,
		port);
	if (OK != (s=sys_outl(PCII_CONFADD, PCII_UNSEL)))
		printf("PCI: warning, sys_outl failed: %d\n", s);
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
static void pcii_wreg8(
  int busind,
  int devind,
  int port,
  u8_t value
)
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
	if (OK != (s=sys_outl(PCII_CONFADD, PCII_UNSEL)))
		printf("PCI: warning, sys_outl failed: %d\n", s);
}

/*===========================================================================*
 *				pcii_wreg16				     *
 *===========================================================================*/
static void pcii_wreg16(
  int busind,
  int devind,
  int port,
  u16_t value
)
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
	if (OK != (s=sys_outl(PCII_CONFADD, PCII_UNSEL)))
		printf("PCI: warning, sys_outl failed: %d\n", s);
}

/*===========================================================================*
 *				pcii_wreg32				     *
 *===========================================================================*/
static void pcii_wreg32(
  int busind,
  int devind,
  int port,
  u32_t value
)
{
	int s;
#if 0
	printf("pcii_wreg32(%d, %d, 0x%X, 0x%X): %d.%d.%d\n",
		busind, devind, port, value,
		pcibus[busind].pb_busnr, pcidev[devind].pd_dev,
		pcidev[devind].pd_func);
#endif
	PCII_WREG32_(pcibus[busind].pb_busnr, 
		pcidev[devind].pd_dev, pcidev[devind].pd_func,
		port, value);
	if (OK != (s=sys_outl(PCII_CONFADD, PCII_UNSEL)))
		printf("PCI: warning, sys_outl failed: %d\n",s);
}

/*===========================================================================*
 *				pcii_rsts				     *
 *===========================================================================*/
static u16_t pcii_rsts(int busind)
{
	u16_t v;
	int s;

	v= PCII_RREG16_(pcibus[busind].pb_busnr, 0, 0, PCI_SR);
	if (OK != (s=sys_outl(PCII_CONFADD, PCII_UNSEL)))
		printf("PCI: warning, sys_outl failed: %d\n", s);
	return v;
}

/*===========================================================================*
 *				pcii_wsts				     *
 *===========================================================================*/
static void pcii_wsts(int busind, u16_t value)
{
	int s;
	PCII_WREG16_(pcibus[busind].pb_busnr, 0, 0, PCI_SR, value);
	if (OK != (s=sys_outl(PCII_CONFADD, PCII_UNSEL)))
		printf("PCI: warning, sys_outl failed: %d\n", s);
}


/*===========================================================================*
 *				print_capabilities			     *
 *===========================================================================*/
static void print_capabilities(int devind)
{
	u8_t status, capptr, type, next, subtype;
	char *str;

	/* Check capabilities bit in the device status register */
	status= pci_attr_r16(devind, PCI_SR);
	if (!(status & PSR_CAPPTR))
		return;

	capptr= (pci_attr_r8_u(devind, PCI_CAPPTR) & PCI_CP_MASK);
	while (capptr != 0)
	{
		type = pci_attr_r8_u(devind, capptr+CAP_TYPE);
		next= (pci_attr_r8_u(devind, capptr+CAP_NEXT) & PCI_CP_MASK);
		switch(type)
		{
		case 1: str= "PCI Power Management"; break;
		case 2: str= "AGP"; break;
		case 3: str= "Vital Product Data"; break;
		case 4:	str= "Slot Identification"; break;
		case 5: str= "Message Signaled Interrupts"; break;
		case 6: str= "CompactPCI Hot Swap"; break;
		case 8: str= "AMD HyperTransport"; break;
		case 0xf: str= "Secure Device"; break;
		default: str= "(unknown type)"; break;
		}

		printf(" @0x%x (0x%08x): capability type 0x%x: %s",
			capptr, pci_attr_r32_u(devind, capptr), type, str);
		if (type == 0x08)
			print_hyper_cap(devind, capptr);
		else if (type == 0x0f)
		{
			subtype= (pci_attr_r8_u(devind, capptr+2) & 0x07);
			switch(subtype)
			{
			case 0: str= "Device Exclusion Vector"; break;
			case 3: str= "IOMMU"; break;
			default: str= "(unknown type)"; break;
			}
			printf(", sub type 0%o: %s", subtype, str);
		}
		printf("\n");
		capptr= next;
	}
}


/*===========================================================================*
 *				visible					     *
 *===========================================================================*/
static int visible(aclp, devind)
struct rs_pci *aclp;
int devind;
{
	u16_t acl_sub_vid, acl_sub_did;
	int i;
	u32_t class_id;

	if (!aclp)
		return TRUE;	/* Should be changed when ACLs become
				 * mandatory. Do note that procfs relies
				 * on being able to see all devices.
				 */
	/* Check whether the caller is allowed to get this device. */
	for (i= 0; i<aclp->rsp_nr_device; i++)
	{
		acl_sub_vid = aclp->rsp_device[i].sub_vid;
		acl_sub_did = aclp->rsp_device[i].sub_did;
		if (aclp->rsp_device[i].vid == pcidev[devind].pd_vid &&
			aclp->rsp_device[i].did == pcidev[devind].pd_did &&
			(acl_sub_vid == NO_SUB_VID ||
			acl_sub_vid == pcidev[devind].pd_sub_vid) &&
			(acl_sub_did == NO_SUB_DID ||
			acl_sub_did == pcidev[devind].pd_sub_did))
		{
			return TRUE;
		}
	}
	if (!aclp->rsp_nr_class)
		return FALSE;

	class_id= (pcidev[devind].pd_baseclass << 16) |
		(pcidev[devind].pd_subclass << 8) |
		pcidev[devind].pd_infclass;
	for (i= 0; i<aclp->rsp_nr_class; i++)
	{
		if (aclp->rsp_class[i].pciclass ==
			(class_id & aclp->rsp_class[i].mask))
		{
			return TRUE;
		}
	}

	return FALSE;
}

/*===========================================================================*
 *				print_hyper_cap				     *
 *===========================================================================*/
static void print_hyper_cap(int devind, u8_t capptr)
{ 
	u32_t v;
	u16_t cmd;
	int type0, type1;

	printf("\n");
	v= pci_attr_r32_u(devind, capptr);
	printf("print_hyper_cap: @0x%x, off 0 (cap):", capptr);
	cmd= (v >> 16) & 0xffff;
#if 0
	if (v & 0x10000)
	{
		printf(" WarmReset");
		v &= ~0x10000;
	}
	if (v & 0x20000)
	{
		printf(" DblEnded");
		v &= ~0x20000;
	}
	printf(" DevNum %d", (v & 0x7C0000) >> 18);
	v &= ~0x7C0000;
#endif
	type0= (cmd & 0xE000) >> 13;
	type1= (cmd & 0xF800) >> 11;
	if (type0 == 0 || type0 == 1)
	{
		printf("Capability Type: %s\n",
			type0 == 0 ? "Slave or Primary Interface" :
			"Host or Secondary Interface");
		cmd &= ~0xE000;
	}
	else
	{
		printf(" Capability Type 0x%x", type1);
		cmd &= ~0xF800;
	}
	if (cmd)
		printf(" undecoded 0x%x\n", cmd);

#if 0
	printf("print_hyper_cap: off 4 (ctl): 0x%x\n", 
		pci_attr_r32_u(devind, capptr+4));
	printf("print_hyper_cap: off 8 (freq/rev): 0x%x\n", 
		pci_attr_r32_u(devind, capptr+8));
	printf("print_hyper_cap: off 12 (cap): 0x%x\n", 
		pci_attr_r32_u(devind, capptr+12));
	printf("print_hyper_cap: off 16 (buf count): 0x%x\n", 
		pci_attr_r32_u(devind, capptr+16));
	v= pci_attr_r32_u(devind, capptr+20);
	printf("print_hyper_cap: @0x%x, off 20 (bus nr): ", 
		capptr+20);
	printf("prim %d", v & 0xff);
	printf(", sec %d", (v >> 8) & 0xff);
	printf(", sub %d", (v >> 16) & 0xff);
	if (v >> 24)
		printf(", reserved %d", (v >> 24) & 0xff);
	printf("\n");
	printf("print_hyper_cap: off 24 (type): 0x%x\n", 
		pci_attr_r32_u(devind, capptr+24));
#endif
}

/*
 * $PchId: pci.c,v 1.7 2003/08/07 09:06:51 philip Exp $
 */
