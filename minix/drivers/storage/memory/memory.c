/* This file contains the device dependent part of the drivers for the
 * following special files:
 *     /dev/ram		- RAM disk 
 *     /dev/mem		- absolute memory
 *     /dev/kmem	- kernel virtual memory
 *     /dev/null	- null device (data sink)
 *     /dev/boot	- boot device loaded from boot image 
 *     /dev/zero	- null byte stream generator
 *     /dev/imgrd	- boot image RAM disk
 *
 *  Changes:
 *	Apr 29, 2005	added null byte generator  (Jorrit N. Herder)
 *	Apr 09, 2005	added support for boot device  (Jorrit N. Herder)
 *	Jul 26, 2004	moved RAM driver to user-space  (Jorrit N. Herder)
 *	Apr 20, 1992	device dependent/independent split  (Kees J. Bot)
 */

#include <assert.h>
#include <minix/drivers.h>
#include <minix/chardriver.h>
#include <minix/blockdriver.h>
#include <sys/ioc_memory.h>
#include <minix/ds.h>
#include <minix/vm.h>
#include <machine/param.h>
#include <machine/vmparam.h>
#include <sys/mman.h>
#include "kernel/const.h"
#include "kernel/config.h"
#include "kernel/type.h"

#include <machine/vm.h>

#include "local.h"

/* ramdisks (/dev/ram*) */
#define RAMDISKS     6

#define RAM_DEV_LAST (RAM_DEV_FIRST+RAMDISKS-1)

#define NR_DEVS            (7+RAMDISKS)	/* number of minor devices */

static struct device m_geom[NR_DEVS];  /* base and size of each device */
static vir_bytes m_vaddrs[NR_DEVS];

static int openct[NR_DEVS];

static ssize_t m_char_read(devminor_t minor, u64_t position, endpoint_t endpt,
	cp_grant_id_t grant, size_t size, int flags, cdev_id_t id);
static ssize_t m_char_write(devminor_t minor, u64_t position, endpoint_t endpt,
	cp_grant_id_t grant, size_t size, int flags, cdev_id_t id);
static int m_char_open(devminor_t minor, int access, endpoint_t user_endpt);
static int m_char_close(devminor_t minor);

static struct device *m_block_part(devminor_t minor);
static ssize_t m_block_transfer(devminor_t minor, int do_write, u64_t position,
	endpoint_t endpt, iovec_t *iov, unsigned int nr_req, int flags);
static int m_block_open(devminor_t minor, int access);
static int m_block_close(devminor_t minor);
static int m_block_ioctl(devminor_t minor, unsigned long request, endpoint_t
	endpt, cp_grant_id_t grant, endpoint_t user_endpt);

/* Entry points to the CHARACTER part of this driver. */
static struct chardriver m_cdtab = {
  .cdr_open	= m_char_open,		/* open device */
  .cdr_close	= m_char_close,		/* close device */
  .cdr_read	= m_char_read,		/* read from device */
  .cdr_write	= m_char_write		/* write to device */
};

/* Entry points to the BLOCK part of this driver. */
static struct blockdriver m_bdtab = {
  .bdr_type	= BLOCKDRIVER_TYPE_DISK,/* handle partition requests */
  .bdr_open	= m_block_open,		/* open device */
  .bdr_close	= m_block_close,	/* nothing on a close */
  .bdr_transfer	= m_block_transfer,	/* do the I/O */
  .bdr_ioctl	= m_block_ioctl,	/* ram disk I/O control */
  .bdr_part	= m_block_part		/* return partition information */
};

/* SEF functions and variables. */
static void sef_local_startup(void);
static int sef_cb_init_fresh(int type, sef_init_info_t *info);

/*===========================================================================*
 *				   main 				     *
 *===========================================================================*/
int main(void)
{
  message msg;
  int r, ipc_status;

  /* SEF local startup. */
  sef_local_startup();

  /* The receive loop. */
  for (;;) {
	if ((r = driver_receive(ANY, &msg, &ipc_status)) != OK)
		panic("memory: driver_receive failed (%d)", r);

	if (IS_BDEV_RQ(msg.m_type))
		blockdriver_process(&m_bdtab, &msg, ipc_status);
	else
		chardriver_process(&m_cdtab, &msg, ipc_status);
  }

  return(OK);
}

/*===========================================================================*
 *			       sef_local_startup			     *
 *===========================================================================*/
static void sef_local_startup()
{
  /* Register init callbacks. */
  sef_setcb_init_fresh(sef_cb_init_fresh);
  sef_setcb_init_restart(SEF_CB_INIT_RESTART_STATEFUL);

  /* Let SEF perform startup. */
  sef_startup();
}

/*===========================================================================*
 *		            sef_cb_init_fresh                                *
 *===========================================================================*/
static int sef_cb_init_fresh(int type, sef_init_info_t *UNUSED(info))
{
/* Initialize the memory driver. */
  int i;
#if 0
  struct kinfo kinfo;		/* kernel information */
  int s;

  if (OK != (s=sys_getkinfo(&kinfo))) {
      panic("Couldn't get kernel information: %d", s);
  }

  /* Map in kernel memory for /dev/kmem. */
  m_geom[KMEM_DEV].dv_base = kinfo.kmem_base;
  m_geom[KMEM_DEV].dv_size = kinfo.kmem_size;
  if((m_vaddrs[KMEM_DEV] = vm_map_phys(SELF, (void *) kinfo.kmem_base,
	kinfo.kmem_size)) == MAP_FAILED) {
	printf("MEM: Couldn't map in /dev/kmem.");
  }
#endif

  /* Ramdisk image built into the memory driver */
  m_geom[IMGRD_DEV].dv_base= 0;
  m_geom[IMGRD_DEV].dv_size= imgrd_size;
  m_vaddrs[IMGRD_DEV] = (vir_bytes) imgrd;

  for(i = 0; i < NR_DEVS; i++)
	openct[i] = 0;

  /* Set up memory range for /dev/mem. */
  m_geom[MEM_DEV].dv_base = 0;
  m_geom[MEM_DEV].dv_size = 0xffffffffULL;

  m_vaddrs[MEM_DEV] = (vir_bytes) MAP_FAILED; /* we are not mapping this in. */

  chardriver_announce();
  blockdriver_announce(type);

  return(OK);
}

/*===========================================================================*
 *				m_is_block				     *
 *===========================================================================*/
static int m_is_block(devminor_t minor)
{
/* Return TRUE iff the given minor device number is for a block device. */

  switch (minor) {
  case MEM_DEV:
  case KMEM_DEV:
  case NULL_DEV:
  case ZERO_DEV:
	return FALSE;

  default:
	return TRUE;
  }
}

/*===========================================================================*
 *				m_transfer_kmem				     *
 *===========================================================================*/
static ssize_t m_transfer_kmem(devminor_t minor, int do_write, u64_t position,
	endpoint_t endpt, cp_grant_id_t grant, size_t size)
{
/* Transfer from or to the KMEM device. */
  u64_t dv_size, dev_vaddr;
  int r;

  dv_size = m_geom[minor].dv_size;
  dev_vaddr = m_vaddrs[minor];

  if (!dev_vaddr || dev_vaddr == (vir_bytes) MAP_FAILED) {
	printf("MEM: dev %d not initialized\n", minor);
	return EIO;
  }

  if (position >= dv_size) return 0;	/* check for EOF */
  if (position + size > dv_size) size = dv_size - position;

  if (!do_write)			/* copy actual data */
	r = sys_safecopyto(endpt, grant, 0, dev_vaddr + position, size);
  else
	r = sys_safecopyfrom(endpt, grant, 0, dev_vaddr + position, size);

  return (r != OK) ? r : size;
}

/*===========================================================================*
 *				m_transfer_mem				     *
 *===========================================================================*/
static ssize_t m_transfer_mem(devminor_t minor, int do_write, u64_t position,
	endpoint_t endpt, cp_grant_id_t grant, size_t size)
{
/* Transfer from or to the MEM device. */
  static int any_mapped = 0;
  static phys_bytes pagestart_mapped;
  static char *vaddr;
  phys_bytes mem_phys, pagestart;
  size_t off, page_off, subcount;
  u64_t dv_size;
  int r;

  dv_size = m_geom[minor].dv_size;
  if (position >= dv_size) return 0;	/* check for EOF */
  if (position + size > dv_size) size = dv_size - position;

  /* Physical copying. Only used to access entire memory.
   * Transfer one 'page window' at a time.
   */
  off = 0;
  while (off < size) {
	mem_phys = (phys_bytes) position;

	page_off = (size_t) (mem_phys % PAGE_SIZE);
	pagestart = mem_phys - page_off;

	/* All memory to the map call has to be page-aligned.
	 * Don't have to map same page over and over.
	 */
	if (!any_mapped || pagestart_mapped != pagestart) {
		if (any_mapped) {
			if (vm_unmap_phys(SELF, vaddr, PAGE_SIZE) != OK)
				panic("vm_unmap_phys failed");
			any_mapped = 0;
		}

		vaddr = vm_map_phys(SELF, (void *) pagestart, PAGE_SIZE);
		if (vaddr == MAP_FAILED) {
			printf("memory: vm_map_phys failed\n");
			return ENOMEM;
		}
		any_mapped = 1;
		pagestart_mapped = pagestart;
	}

	/* how much to be done within this page. */
	subcount = PAGE_SIZE - page_off;
	if (subcount > size)
		subcount = size;

	if (!do_write)	/* copy data */
		r = sys_safecopyto(endpt, grant, off,
			(vir_bytes) vaddr + page_off, subcount);
	else
		r = sys_safecopyfrom(endpt, grant, off,
			(vir_bytes) vaddr + page_off, subcount);
	if (r != OK)
		return r;

	position += subcount;
	off += subcount;
  }

  return off;
}

/*===========================================================================*
 *				m_char_read				     *
 *===========================================================================*/
static ssize_t m_char_read(devminor_t minor, u64_t position, endpoint_t endpt,
	cp_grant_id_t grant, size_t size, int UNUSED(flags),
	cdev_id_t UNUSED(id))
{
/* Read from one of the driver's character devices. */
  ssize_t r;

  /* Check if the minor device number is ok. */
  if (minor < 0 || minor >= NR_DEVS || m_is_block(minor)) return ENXIO;

  switch (minor) {
  case NULL_DEV:
	r = 0;	/* always at EOF */
	break;

  case ZERO_DEV:
	/* Fill the target area with zeroes. In fact, let the kernel do it! */
	if ((r = sys_safememset(endpt, grant, 0, '\0', size)) == OK)
		r = size;
	break;

  case KMEM_DEV:
	r = m_transfer_kmem(minor, FALSE, position, endpt, grant, size);
	break;

  case MEM_DEV:
	r = m_transfer_mem(minor, FALSE, position, endpt, grant, size);
	break;

  default:
	panic("unknown character device %d", minor);
  }

  return r;
}

/*===========================================================================*
 *				m_char_write				     *
 *===========================================================================*/
static ssize_t m_char_write(devminor_t minor, u64_t position, endpoint_t endpt,
	cp_grant_id_t grant, size_t size, int UNUSED(flags),
	cdev_id_t UNUSED(id))
{
/* Write to one of the driver's character devices. */
  ssize_t r;

  /* Check if the minor device number is ok. */
  if (minor < 0 || minor >= NR_DEVS || m_is_block(minor)) return ENXIO;

  switch (minor) {
  case NULL_DEV:
  case ZERO_DEV:
	r = size;	/* just eat everything */
	break;

  case KMEM_DEV:
	r = m_transfer_kmem(minor, TRUE, position, endpt, grant, size);
	break;

  case MEM_DEV:
	r = m_transfer_mem(minor, TRUE, position, endpt, grant, size);
	break;

  default:
	panic("unknown character device %d", minor);
  }

  return r;
}

/*===========================================================================*
 *				m_char_open				     *
 *===========================================================================*/
static int m_char_open(devminor_t minor, int access, endpoint_t user_endpt)
{
/* Open a memory character device. */

  /* Check if the minor device number is ok. */
  if (minor < 0 || minor >= NR_DEVS || m_is_block(minor)) return ENXIO;

#if defined(__i386__)
  if (minor == MEM_DEV)
  {
	int r = sys_enable_iop(user_endpt);
	if (r != OK)
	{
		printf("m_char_open: sys_enable_iop failed for %d: %d\n",
			user_endpt, r);
		return r;
	}
  }
#endif

  openct[minor]++;

  return(OK);
}

/*===========================================================================*
 *				m_char_close				     *
 *===========================================================================*/
static int m_char_close(devminor_t minor)
{
/* Close a memory character device. */

  if (minor < 0 || minor >= NR_DEVS || m_is_block(minor)) return ENXIO;

  if(openct[minor] < 1) {
	printf("MEMORY: closing unopened device %d\n", minor);
	return(EINVAL);
  }
  openct[minor]--;

  return(OK);
}

/*===========================================================================*
 *				m_block_part				     *
 *===========================================================================*/
static struct device *m_block_part(devminor_t minor)
{
/* Prepare for I/O on a device: check if the minor device number is ok. */
  if (minor < 0 || minor >= NR_DEVS || !m_is_block(minor)) return(NULL);

  return(&m_geom[minor]);
}

/*===========================================================================*
 *				m_block_transfer			     *
 *===========================================================================*/
static int m_block_transfer(
  devminor_t minor,		/* minor device number */
  int do_write,			/* read or write? */
  u64_t position,		/* offset on device to read or write */
  endpoint_t endpt,		/* process doing the request */
  iovec_t *iov,			/* pointer to read or write request vector */
  unsigned int nr_req,		/* length of request vector */
  int UNUSED(flags)		/* transfer flags */
)
{
/* Read or write one the driver's block devices. */
  unsigned count;
  vir_bytes vir_offset = 0;
  struct device *dv;
  u64_t dv_size;
  int r;
  vir_bytes dev_vaddr;
  cp_grant_id_t grant;
  ssize_t total = 0;

  /* Get minor device information. */
  if ((dv = m_block_part(minor)) == NULL) return(ENXIO);
  dv_size = dv->dv_size;
  dev_vaddr = m_vaddrs[minor];

  if (ex64hi(position) != 0)
	return OK;	/* Beyond EOF */

  while (nr_req > 0) {

	/* How much to transfer and where to / from. */
	count = iov->iov_size;
	grant = (cp_grant_id_t) iov->iov_addr;

	/* Virtual copying. For RAM disks and internal FS. */
	if(!dev_vaddr || dev_vaddr == (vir_bytes) MAP_FAILED) {
		printf("MEM: dev %d not initialized\n", minor);
		return EIO;
	}
	if (position >= dv_size) return(total);	/* check for EOF */
	if (position + count > dv_size) count = dv_size - position;
	if (!do_write) {	/* copy actual data */
	        r=sys_safecopyto(endpt, grant, vir_offset,
		  dev_vaddr + position, count);
	} else {
	        r=sys_safecopyfrom(endpt, grant, vir_offset,
		  dev_vaddr + position, count);
	}
	if(r != OK) {
		panic("I/O copy failed: %d", r);
	}

	/* Book the number of bytes transferred. */
	position += count;
	vir_offset += count;
	total += count;
	if ((iov->iov_size -= count) == 0) { iov++; nr_req--; vir_offset = 0; }

  }
  return(total);
}

/*===========================================================================*
 *				m_block_open				     *
 *===========================================================================*/
static int m_block_open(devminor_t minor, int UNUSED(access))
{
/* Open a memory block device. */
  if (m_block_part(minor) == NULL) return(ENXIO);

  openct[minor]++;

  return(OK);
}

/*===========================================================================*
 *				m_block_close				     *
 *===========================================================================*/
static int m_block_close(devminor_t minor)
{
/* Close a memory block device. */
  if (m_block_part(minor) == NULL) return(ENXIO);

  if(openct[minor] < 1) {
	printf("MEMORY: closing unopened device %d\n", minor);
	return(EINVAL);
  }
  openct[minor]--;

  return(OK);
}

/*===========================================================================*
 *				m_block_ioctl				     *
 *===========================================================================*/
static int m_block_ioctl(devminor_t minor, unsigned long request,
	endpoint_t endpt, cp_grant_id_t grant, endpoint_t UNUSED(user_endpt))
{
/* I/O controls for the block devices of the memory driver. Currently there is
 * one I/O control specific to the memory driver:
 * - MIOCRAMSIZE: to set the size of the RAM disk.
 */
  struct device *dv;
  u32_t ramdev_size;
  int s;
  void *mem;
  int is_imgrd = 0;

  if (request != MIOCRAMSIZE)
	return EINVAL;

  if(minor == IMGRD_DEV) 
	is_imgrd = 1;

  /* Someone wants to create a new RAM disk with the given size.
   * A ramdisk can be created only once, and only on RAM disk device.
   */
  if ((dv = m_block_part(minor)) == NULL) return ENXIO;
  if((minor < RAM_DEV_FIRST || minor > RAM_DEV_LAST) &&
  	minor != RAM_DEV_OLD && !is_imgrd) {
	printf("MEM: MIOCRAMSIZE: %d not a ramdisk\n", minor);
	return EINVAL;
  }

  /* Get request structure */
  s= sys_safecopyfrom(endpt, grant, 0, (vir_bytes)&ramdev_size,
	sizeof(ramdev_size));
  if (s != OK)
	return s;
  if(is_imgrd)
  	ramdev_size = 0;
  if(m_vaddrs[minor] && dv->dv_size == (u64_t) ramdev_size) {
	return(OK);
  }
  /* openct is 1 for the ioctl(). */
  if(openct[minor] != 1) {
	printf("MEM: MIOCRAMSIZE: %d in use (count %d)\n",
		minor, openct[minor]);
	return(EBUSY);
  }
  if(m_vaddrs[minor]) {
	u32_t a, o;
	u64_t size;
	int r;
	if(ex64hi(dv->dv_size)) {
		panic("huge old ramdisk");
	}
	size = dv->dv_size;
	a = m_vaddrs[minor];
	if((o = a % PAGE_SIZE)) {
		vir_bytes l = PAGE_SIZE - o;
		a += l;
		size -= l;
	}
	size = rounddown(size, PAGE_SIZE);
	r = munmap((void *) a, size);
	if(r != OK) {
		printf("memory: WARNING: munmap failed: %d\n", r);
	}
	m_vaddrs[minor] = (vir_bytes) NULL;
	dv->dv_size = 0;
  }

#if DEBUG
  printf("MEM:%d: allocating ramdisk of size 0x%x\n", minor, ramdev_size);
#endif

  mem = NULL;

  /* Try to allocate a piece of memory for the RAM disk. */
  if(ramdev_size > 0 &&
  	(mem = mmap(NULL, ramdev_size, PROT_READ|PROT_WRITE,
		MAP_PREALLOC|MAP_ANON, -1, 0)) == MAP_FAILED) {
	printf("MEM: failed to get memory for ramdisk\n");
	return(ENOMEM);
  }

  m_vaddrs[minor] = (vir_bytes) mem;

  dv->dv_size = ramdev_size;

  return(OK);
}
