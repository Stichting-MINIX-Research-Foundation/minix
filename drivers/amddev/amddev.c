/*
amddev.c

Driver for the AMD Device Exclusion Vector (DEV)
*/

#define _SYSTEM
#define _MINIX

#include <minix/config.h>
#include <minix/type.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/vm_i386.h>
#include <minix/com.h>
#include <minix/const.h>
#include <minix/ipc.h>
#include <minix/syslib.h>
#include <minix/sysutil.h>
#include <minix/endpoint.h>
#include <ibm/pci.h>

/* Offsets from capability pointer */
#define DEV_OP		4	/* Selects control/status register to access */
#define		DEV_OP_FUNC_SHIFT	8	/* Function part in OP reg. */
#define DEV_DATA	8	/* Read/write to access reg. selected */

/* Functions */
#define DEVF_BASE_LO	0
#define DEVF_BASE_HI	1
#define DEVF_MAP		2
#define DEVF_CAP		3
#define		DEVF_CAP_MAPS_MASK	0x00ff0000
#define		DEVF_CAP_MAPS_SHIFT	16
#define		DEVF_CAP_DOMS_MASK	0x0000ff00
#define		DEVF_CAP_DOMS_SHIFT	8
#define		DEVF_CAP_REV_MASK	0x000000ff
#define		DEVF_CAP_REV_SHIFT	0
#define DEVF_CR		4
#define DEVF_ERR_STATUS	5
#define DEVF_ERR_ADDR_LO	6
#define DEVF_ERR_ADDR_HI	7

static int dev_devind;
static u8_t dev_capptr;
static u8_t *table;

static int find_dev(int *devindp, u8_t *capaddrp);
static u32_t read_reg(int function, int index);
static void write_reg(int function, int index, u32_t value);
static void init_domain(int index);
static void init_map(int index);
static int do_add_phys(message *m);
static int do_del_phys(message *m);
static int do_add4pci(message *m);
static void add_range(u32_t busaddr, u32_t size);
static void del_range(u32_t busaddr, u32_t size);
static void do_pm_notify(message *m);
static void report_exceptions(void);

/* SEF functions and variables. */
FORWARD _PROTOTYPE( void sef_local_startup, (void) );
FORWARD _PROTOTYPE( int sef_cb_init_fresh, (int type, sef_init_info_t *info) );

int main(void)
{
	int r;
	message m;

	/* SEF local startup. */
	sef_local_startup();

	for(;;)
	{
		report_exceptions();

		r= sef_receive(ANY, &m);
		if (r != OK)
			panic(__FILE__, "sef_receive failed", r);
		if (is_notify(m.m_type)) {
			if (_ENDPOINT_P(m.m_source) == PM_PROC_NR) {
				do_pm_notify(&m);
				continue;
			}
		}
		else if (m.m_type == IOMMU_MAP) {
			r= do_add4pci(&m);
			m.m_type= r;
			send(m.m_source, &m);
			continue;
		}
		printf("amddev: got message from %d\n", m.m_source);
	}
}

/*===========================================================================*
 *			       sef_local_startup			     *
 *===========================================================================*/
PRIVATE void sef_local_startup()
{
  /* Register init callbacks. */
  sef_setcb_init_fresh(sef_cb_init_fresh);
  sef_setcb_init_lu(sef_cb_init_fresh);
  sef_setcb_init_restart(sef_cb_init_fresh);

  /* Register live update callbacks. */
  sef_setcb_lu_prepare(sef_cb_lu_prepare_always_ready);
  sef_setcb_lu_state_isvalid(sef_cb_lu_state_isvalid_standard);

  /* Let SEF perform startup. */
  sef_startup();
}

/*===========================================================================*
 *		            sef_cb_init_fresh                                *
 *===========================================================================*/
PRIVATE int sef_cb_init_fresh(int type, sef_init_info_t *info)
{
/* Initialize the amddev driver. */
	int r, n_maps, n_domains, revision;
	u16_t flags;
	u32_t bits;

	printf("amddev: starting\n");

	r= find_dev(&dev_devind, &dev_capptr);
	if (!r)
		return r;
	flags= pci_attr_r16(dev_devind, dev_capptr+CAP_SD_INFO);
	printf("amddev`init: flags = 0x%x\n", flags);

	bits= read_reg(DEVF_CAP, 0);
	n_maps= ((bits & DEVF_CAP_MAPS_MASK) >> DEVF_CAP_MAPS_SHIFT);
	n_domains= ((bits & DEVF_CAP_DOMS_MASK) >> DEVF_CAP_DOMS_SHIFT);
	revision= ((bits & DEVF_CAP_REV_MASK) >> DEVF_CAP_REV_SHIFT);
	printf("amddev`init: DEVF_CAP = 0x%x (%d maps, %d domains, rev 0x%x)\n",
		bits, n_maps, n_domains, revision);

	printf("status = 0x%x, addr-lo = 0x%x, addr-hi = 0x%x\n",
		read_reg(DEVF_ERR_STATUS, 0),
		read_reg(DEVF_ERR_ADDR_LO, 0),
		read_reg(DEVF_ERR_ADDR_HI, 0));

	init_domain(0);
	init_map(0);
#if 0
	init_domain(1);
#endif

	write_reg(DEVF_CR, 0, 0x10 | 0x8 | 0x4 | 1);

	printf("after write: DEVF_CR: 0x%x\n", read_reg(DEVF_CR, 0));

	return(OK);
}

/* Returns 0 if no device found, or 1 if a device is found. */
static int find_dev(devindp, capaddrp)
int *devindp;
u8_t *capaddrp;
{
	int r, devind, first;
	u8_t capptr, type, next, subtype;
	u16_t vid, did, status;

	pci_init();

	first= 1;
	for(;;)
	{
		if (first)
		{
			first= 0;
			r= pci_first_dev(&devind, &vid, &did);
			if (!r)
			{
				printf("amddev`find_dev: no first dev\n");
				return 0;
			}
		}
		else
		{
			r= pci_next_dev(&devind, &vid, &did);
			if (!r)
			{
				printf("amddev`find_dev: no next dev\n");
				return 0;
			}
		}

		printf("amddev`find_dev: got devind %d, vid 0x%x, did 0x%x\n",
			devind, vid, did);

		/* Check capabilities bit in the device status register */
		status= pci_attr_r16(devind, PCI_SR);
		if (!(status & PSR_CAPPTR))
			continue;

		capptr= (pci_attr_r8(devind, PCI_CAPPTR) & PCI_CP_MASK);
		while (capptr != 0)
		{
			type = pci_attr_r8(devind, capptr+CAP_TYPE);
			next= (pci_attr_r8(devind, capptr+CAP_NEXT) &
				PCI_CP_MASK);
			if (type == CAP_T_SECURE_DEV)
			{
				printf(
				"amddev`find_dev: found secure device\n");
				subtype= (pci_attr_r8(devind, capptr+
					CAP_SD_INFO) & CAP_SD_SUBTYPE_MASK);
				if (subtype == CAP_T_SD_DEV)
				{
					printf("amddev`find_dev: AMD DEV\n");
					pci_reserve(devind);
					*devindp= devind;
					*capaddrp= capptr;
					return 1;
				}
			}
			capptr= next;
		}
	}
	return 0;
}

static u32_t read_reg(int function, int index)
{
	pci_attr_w32(dev_devind, dev_capptr + DEV_OP, ((function <<
		DEV_OP_FUNC_SHIFT) | index));
	return pci_attr_r32(dev_devind, dev_capptr + DEV_DATA);
}

static void write_reg(int function, int index, u32_t value)
{
	pci_attr_w32(dev_devind, dev_capptr + DEV_OP, ((function <<
		DEV_OP_FUNC_SHIFT) | index));
	pci_attr_w32(dev_devind, dev_capptr + DEV_DATA, value);
}

static void init_domain(int index)
{
	size_t size, memsize;
	phys_bytes busaddr;

	size= 0x100000 / 8;
	table= alloc_contig(size, AC_ALIGN4K, &busaddr);
	if (table == NULL)
		panic("AMDDEV","malloc failed", NO_NUM);
	if (index == 0)
	{
		memset(table, 0, size);
		memsize= 0x37000 / 8;
		printf("memsize = 0x%x / 8\n", memsize*8);
		memset(table, 0xff, memsize);
	}
	else
	{
		memset(table, 0xff, size);
		memset(table, 0x00, size);
	}

printf("init_domain: busaddr = %p\n", busaddr);

	write_reg(DEVF_BASE_HI, index, 0);
	write_reg(DEVF_BASE_LO, index, busaddr | 3);

	printf("after write: DEVF_BASE_LO: 0x%x\n",
		read_reg(DEVF_BASE_LO, index));
}

static void init_map(int index)
{
	u32_t v, dom, busno, unit0, unit1;

	dom= 1;
	busno= 7;
	unit1= 9;
	unit0= 9;
	v= (dom << 26) | (dom << 20) | (busno << 12) |
		(0 << 11) | (unit1 << 6) |
		(0 << 5) | (unit0 << 0);
	write_reg(DEVF_MAP, index, v);

	printf("after write: DEVF_MAP: 0x%x\n", read_reg(DEVF_MAP, index));
}

#if 0
static int do_add(message *m)
{
	int r;
	endpoint_t proc;
	vir_bytes start;
	size_t size;
	phys_bytes busaddr;

	proc= m->m_source;
	start= m->m2_l1;
	size= m->m2_l2;

#if 0
	printf("amddev`do_add: got request for 0x%x@0x%x from %d\n",
		size, start, proc);
#endif

	if (start % I386_PAGE_SIZE)
	{
		printf("amddev`do_add: bad start 0x%x from proc %d\n",
			start, proc);
		return EINVAL;
	}
	if (size % I386_PAGE_SIZE)
	{
		printf("amddev`do_add: bad size 0x%x from proc %d\n",
			size, proc);
		return EINVAL;
	}
	r= sys_umap(proc, VM_D, (vir_bytes)start, size, &busaddr);
	if (r != OK)
	{
		printf("amddev`do_add: umap failed for 0x%x@0x%x, proc %d\n",
			size, start, proc);
		return r;
	}
	add_range(busaddr, size);

}
#endif

static int do_add_phys(message *m)
{
	int i;
	phys_bytes start;
	size_t size;

	start= m->m2_l1;
	size= m->m2_l2;

#if 0
	printf("amddev`do_add_phys: got request for 0x%x@0x%x\n",
		size, start);
#endif

	if (start % I386_PAGE_SIZE)
	{
		printf("amddev`do_add_phys: bad start 0x%x\n", start);
		return EINVAL;
	}
	if (size % I386_PAGE_SIZE)
	{
		printf("amddev`do_add_phys: bad size 0x%x\n", size);
		return EINVAL;
	}
	add_range(start, size);

	write_reg(DEVF_CR, 0, 0x10);
	for (i= 0; i<1000000; i++)
	{
		if (read_reg(DEVF_CR, 0) & 0x10)
			continue;
		return OK;
	}
	return EBUSY;
}

static int do_del_phys(message *m)
{
	phys_bytes start;
	size_t size;

	start= m->m2_l1;
	size= m->m2_l2;

#if 0
	printf("amddev`do_del_phys: got request for 0x%x@0x%x\n",
		size, start);
#endif

	if (start % I386_PAGE_SIZE)
	{
		printf("amddev`do_del_phys: bad start 0x%x\n", start);
		return EINVAL;
	}
	if (size % I386_PAGE_SIZE)
	{
		printf("amddev`do_del_phys: bad size 0x%x\n", size);
		return EINVAL;
	}
	del_range(start, size);

	write_reg(DEVF_CR, 0, 0x10);

	return OK;
}

static int do_add4pci(message *m)
{
	int r, pci_bus, pci_dev, pci_func;
	endpoint_t proc;
	vir_bytes start;
	size_t size;
	phys_bytes busaddr;

	proc= m->m_source;
	start= m->m2_l1;
	size= m->m2_l2;
	pci_bus= m->m1_i1;
	pci_dev= m->m1_i2;
	pci_func= m->m1_i3;

	printf(
"amddev`do_add4pci: got request for 0x%x@0x%x from %d for pci dev %u.%u.%u\n",
		size, start, proc, pci_bus, pci_dev, pci_func);

	if (start % I386_PAGE_SIZE)
	{
		printf("amddev`do_add4pci: bad start 0x%x from proc %d\n",
			start, proc);
		return EINVAL;
	}
	if (size % I386_PAGE_SIZE)
	{
		printf("amddev`do_add4pci: bad size 0x%x from proc %d\n",
			size, proc);
		return EINVAL;
	}

	printf("amddev`do_add4pci: should check with PCI\n");

	r= sys_umap(proc, VM_D, (vir_bytes)start, size, &busaddr);
	if (r != OK)
	{
		printf(
		"amddev`do_add4pci: umap failed for 0x%x@0x%x, proc %d: %d\n",
			size, start, proc, r);
		return r;
	}

	r= adddma(proc, start, size);
	if (r != 0)
	{
		r= -errno;
		printf(
		"amddev`do_add4pci: adddma failed for 0x%x@0x%x, proc %d: %d\n",
			size, start, proc, r);
		return r;
	}

	add_range(busaddr, size);

	return OK;
}


static void add_range(u32_t busaddr, u32_t size)
{
	u32_t o, bit;

#if 0
	printf("add_range: mapping 0x%x@0x%x\n", size, busaddr);
#endif

	for (o= 0; o<size; o += I386_PAGE_SIZE)
	{
		bit= (busaddr+o)/I386_PAGE_SIZE;
		table[bit/8] &= ~(1 << (bit % 8));
	}
}

static void del_range(u32_t busaddr, u32_t size)
{
	u32_t o, bit;

#if 0
	printf("del_range: mapping 0x%x@0x%x\n", size, busaddr);
#endif

	for (o= 0; o<size; o += I386_PAGE_SIZE)
	{
		bit= (busaddr+o)/I386_PAGE_SIZE;
		table[bit/8] |= (1 << (bit % 8));
	}
}

static void do_pm_notify(message *m)
{
	int r;
	endpoint_t proc_e;
	phys_bytes base, size;

	if (m->m_source != PM_PROC_NR)
	{
		printf("amddev`do_pm_notify: notify not from PM (from %d)\n",
			m->m_source);
		return;
	}

	for (;;)
	{
		r= getdma(&proc_e, &base, &size);
		if (r == -1)
		{
			if (errno != -EAGAIN)
			{
				printf(
				"amddev`do_pm_notify: getdma failed: %d\n",
					errno);
			}
			break;
		}

		printf(
		"amddev`do_pm_notify: deleting 0x%x@0x%x for proc %d\n",
			size, base, proc_e);
		del_range(base, size);
		r= deldma(proc_e, base, size);
		if (r == -1)
		{
			printf("amddev`do_pm_notify: deldma failed: %d\n",
				errno);
			break;
		}
	}
}

static void report_exceptions(void)
{
	u32_t status;

	status= read_reg(DEVF_ERR_STATUS, 0);
	if (!(status & 0x80000000))
		return;
	printf("amddev: status = 0x%x, addr-lo = 0x%x, addr-hi = 0x%x\n",
		status, read_reg(DEVF_ERR_ADDR_LO, 0),
			read_reg(DEVF_ERR_ADDR_HI, 0));
	write_reg(DEVF_ERR_STATUS, 0, 0);
}
