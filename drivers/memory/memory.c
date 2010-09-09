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

#include <minix/drivers.h>
#include <minix/driver.h>
#include <sys/ioc_memory.h>
#include <minix/ds.h>
#include <minix/vm.h>
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

PRIVATE struct device m_geom[NR_DEVS];  /* base and size of each device */
PRIVATE vir_bytes m_vaddrs[NR_DEVS];
PRIVATE int m_device;			/* current device */
PRIVATE struct kinfo kinfo;		/* kernel information */ 

extern int errno;			/* error number for PM calls */

PRIVATE int openct[NR_DEVS];

FORWARD _PROTOTYPE( char *m_name, (void) 				);
FORWARD _PROTOTYPE( struct device *m_prepare, (int device) 		);
FORWARD _PROTOTYPE( int m_transfer, (int proc_nr, int opcode,
			u64_t position, iovec_t *iov, unsigned nr_req)	);
FORWARD _PROTOTYPE( int m_do_open, (struct driver *dp, message *m_ptr) 	);
FORWARD _PROTOTYPE( int m_do_close, (struct driver *dp, message *m_ptr)	);
FORWARD _PROTOTYPE( int m_ioctl, (struct driver *dp, message *m_ptr)	);
FORWARD _PROTOTYPE( void m_geometry, (struct partition *entry) 		);

/* Entry points to this driver. */
PRIVATE struct driver m_dtab = {
  m_name,	/* current device's name */
  m_do_open,	/* open or mount */
  m_do_close,	/* nothing on a close */
  m_ioctl,	/* specify ram disk geometry */
  m_prepare,	/* prepare for I/O on a given minor device */
  m_transfer,	/* do the I/O */
  nop_cleanup,	/* no need to clean up */
  m_geometry,	/* memory device "geometry" */
  nop_alarm,
  nop_cancel,
  nop_select,
  NULL,
  NULL
};

/* Buffer for the /dev/zero null byte feed. */
#define ZERO_BUF_SIZE 			1024
PRIVATE char dev_zero[ZERO_BUF_SIZE];

#define click_to_round_k(n) \
	((unsigned) ((((unsigned long) (n) << CLICK_SHIFT) + 512) / 1024))

/* SEF functions and variables. */
FORWARD _PROTOTYPE( void sef_local_startup, (void) );
FORWARD _PROTOTYPE( int sef_cb_init_fresh, (int type, sef_init_info_t *info) );

/*===========================================================================*
 *				   main 				     *
 *===========================================================================*/
PUBLIC int main(void)
{
  /* SEF local startup. */
  sef_local_startup();

  /* Call the generic receive loop. */
  driver_task(&m_dtab, DRIVER_STD);

  return(OK);
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
/* Initialize the memory driver. */
  u32_t ramdev_size;
  int i, s;

  /* Initialize all minor devices one by one. */
  if (OK != (s=sys_getkinfo(&kinfo))) {
      panic("Couldn't get kernel information: %d", s);
  }

#if 0
  /* Map in kernel memory for /dev/kmem. */
  m_geom[KMEM_DEV].dv_base = cvul64(kinfo.kmem_base);
  m_geom[KMEM_DEV].dv_size = cvul64(kinfo.kmem_size);
  if((m_vaddrs[KMEM_DEV] = vm_map_phys(SELF, (void *) kinfo.kmem_base,
	kinfo.kmem_size)) == MAP_FAILED) {
	printf("MEM: Couldn't map in /dev/kmem.");
  }
#endif

  /* Ramdisk image built into the memory driver */
  m_geom[IMGRD_DEV].dv_base= cvul64(0);
  m_geom[IMGRD_DEV].dv_size= cvul64(imgrd_size);
  m_vaddrs[IMGRD_DEV] = (vir_bytes) imgrd;

  /* Initialize /dev/zero. Simply write zeros into the buffer. */
  for (i=0; i<ZERO_BUF_SIZE; i++) {
       dev_zero[i] = '\0';
  }

  for(i = 0; i < NR_DEVS; i++)
	openct[i] = 0;

  /* Set up memory range for /dev/mem. */
  m_geom[MEM_DEV].dv_base = cvul64(0);
  m_geom[MEM_DEV].dv_size = cvul64(0xffffffff);

  m_vaddrs[MEM_DEV] = (vir_bytes) MAP_FAILED; /* we are not mapping this in. */

  return(OK);
}

/*===========================================================================*
 *				 m_name					     *
 *===========================================================================*/
PRIVATE char *m_name()
{
/* Return a name for the current device. */
  static char name[] = "memory";
  return name;  
}

/*===========================================================================*
 *				m_prepare				     *
 *===========================================================================*/
PRIVATE struct device *m_prepare(device)
int device;
{
/* Prepare for I/O on a device: check if the minor device number is ok. */
  if (device < 0 || device >= NR_DEVS) return(NULL);
  m_device = device;

  return(&m_geom[device]);
}

/*===========================================================================*
 *				m_transfer				     *
 *===========================================================================*/
PRIVATE int m_transfer(proc_nr, opcode, pos64, iov, nr_req)
int proc_nr;			/* process doing the request */
int opcode;			/* DEV_GATHER_S or DEV_SCATTER_S */
u64_t pos64;			/* offset on device to read or write */
iovec_t *iov;			/* pointer to read or write request vector */
unsigned nr_req;		/* length of request vector */
{
/* Read or write one the driver's minor devices. */
  unsigned count, left, chunk;
  vir_bytes user_vir, vir_offset = 0;
  struct device *dv;
  unsigned long dv_size;
  int s, r;
  off_t position;
  vir_bytes dev_vaddr;

  /* ZERO_DEV and NULL_DEV are infinite in size. */
  if (m_device != ZERO_DEV && m_device != NULL_DEV && ex64hi(pos64) != 0)
	return OK;	/* Beyond EOF */
  position= cv64ul(pos64);

  /* Get minor device number and check for /dev/null. */
  dv = &m_geom[m_device];
  dv_size = cv64ul(dv->dv_size);
  dev_vaddr = m_vaddrs[m_device];

  while (nr_req > 0) {

	/* How much to transfer and where to / from. */
	count = iov->iov_size;
	user_vir = iov->iov_addr;

	switch (m_device) {

	/* No copying; ignore request. */
	case NULL_DEV:
	    if (opcode == DEV_GATHER_S) return(OK);	/* always at EOF */
	    break;

	/* Virtual copying. For RAM disks, kernel memory and internal FS. */
	default:
	case KMEM_DEV:
	case RAM_DEV_OLD:
	case IMGRD_DEV:
	    /* Bogus number. */
	    if(m_device < 0 || m_device >= NR_DEVS) {
		    return(EINVAL);
	    }
  	    if(!dev_vaddr || dev_vaddr == (vir_bytes) MAP_FAILED) {
		printf("MEM: dev %d not initialized\n", m_device);
		return EIO;
	    }
	    if (position >= dv_size) return(OK); 	/* check for EOF */
	    if (position + count > dv_size) count = dv_size - position;
	    if (opcode == DEV_GATHER_S) {	/* copy actual data */
	        r=sys_safecopyto(proc_nr, user_vir, vir_offset,
	  	  dev_vaddr + position, count, D);
	    } else {
	        r=sys_safecopyfrom(proc_nr, user_vir, vir_offset,
	  	  dev_vaddr + position, count, D);
	    }
	    if(r != OK) {
              panic("I/O copy failed: %d", r);
	    }
	    break;

	/* Physical copying. Only used to access entire memory.
	 * Transfer one 'page window' at a time.
	 */
	case MEM_DEV:
	{
	    u32_t pagestart, page_off;
	    static u32_t pagestart_mapped;
	    static int any_mapped = 0;
	    static char *vaddr;
	    int r;
	    u32_t subcount;
  	    phys_bytes mem_phys;

	    if (position >= dv_size)
		return(OK); 	/* check for EOF */
	    if (position + count > dv_size)
		count = dv_size - position;
	    mem_phys = position;

	    page_off = mem_phys % I386_PAGE_SIZE;
	    pagestart = mem_phys - page_off; 

	    /* All memory to the map call has to be page-aligned.
	     * Don't have to map same page over and over.
	     */
	    if(!any_mapped || pagestart_mapped != pagestart) {
	     if(any_mapped) {
		if(vm_unmap_phys(SELF, vaddr, I386_PAGE_SIZE) != OK)
      			panic("vm_unmap_phys failed");
		any_mapped = 0;
	     }
	     vaddr = vm_map_phys(SELF, (void *) pagestart, I386_PAGE_SIZE);
	     if(vaddr == MAP_FAILED) 
		r = ENOMEM;
	     else
		r = OK;
	     if(r != OK) {
		printf("memory: vm_map_phys failed\n");
		return r;
	     }
	     any_mapped = 1;
	     pagestart_mapped = pagestart;
	   }

	    /* how much to be done within this page. */
	    subcount = I386_PAGE_SIZE-page_off;
	    if(subcount > count)
		subcount = count;

	    if (opcode == DEV_GATHER_S) {			/* copy data */
	           s=sys_safecopyto(proc_nr, user_vir,
		       vir_offset, (vir_bytes) vaddr+page_off, subcount, D);
	    } else {
	           s=sys_safecopyfrom(proc_nr, user_vir,
		       vir_offset, (vir_bytes) vaddr+page_off, subcount, D);
	    }
	    if(s != OK)
		return s;
	    count = subcount;
	    break;
	}

	/* Null byte stream generator. */
	case ZERO_DEV:
	    if (opcode == DEV_GATHER_S) {
		size_t suboffset = 0;
	        left = count;
	    	while (left > 0) {
	    	    chunk = (left > ZERO_BUF_SIZE) ? ZERO_BUF_SIZE : left;
	             s=sys_safecopyto(proc_nr, user_vir,
		       vir_offset+suboffset, (vir_bytes) dev_zero, chunk, D);
		    if(s != OK)
	    	        return s;
	    	    left -= chunk;
 	            suboffset += chunk;
	    	}
	    }
	    break;

	}

	/* Book the number of bytes transferred. */
	position += count;
	vir_offset += count;
  	if ((iov->iov_size -= count) == 0) { iov++; nr_req--; vir_offset = 0; }

  }
  return(OK);
}

/*===========================================================================*
 *				m_do_open				     *
 *===========================================================================*/
PRIVATE int m_do_open(dp, m_ptr)
struct driver *dp;
message *m_ptr;
{
  int r;

/* Check device number on open. */
  if (m_prepare(m_ptr->DEVICE) == NULL) return(ENXIO);
  if (m_device == MEM_DEV)
  {
	r = sys_enable_iop(m_ptr->IO_ENDPT);
	if (r != OK)
	{
		printf("m_do_open: sys_enable_iop failed for %d: %d\n",
			m_ptr->IO_ENDPT, r);
		return r;
	}
  }

  if(m_device < 0 || m_device >= NR_DEVS) {
      panic("wrong m_device: %d", m_device);
  }

  openct[m_device]++;

  return(OK);
}

/*===========================================================================*
 *				m_do_close				     *
 *===========================================================================*/
PRIVATE int m_do_close(dp, m_ptr)
struct driver *dp;
message *m_ptr;
{
  int r;

  if (m_prepare(m_ptr->DEVICE) == NULL) return(ENXIO);

  if(m_device < 0 || m_device >= NR_DEVS) {
      panic("wrong m_device: %d", m_device);
  }

  if(openct[m_device] < 1) {
      panic("closed too often");
  }
  openct[m_device]--;

  return(OK);
}

/*===========================================================================*
 *				m_ioctl					     *
 *===========================================================================*/
PRIVATE int m_ioctl(dp, m_ptr)
struct driver *dp;			/* pointer to driver structure */
message *m_ptr;				/* pointer to control message */
{
/* I/O controls for the memory driver. Currently there is one I/O control:
 * - MIOCRAMSIZE: to set the size of the RAM disk.
 */
  struct device *dv;

  switch (m_ptr->REQUEST) {
    case MIOCRAMSIZE: {
	/* Someone wants to create a new RAM disk with the given size. */
	u32_t ramdev_size;
	int s, dev;
	void *mem;

	/* A ramdisk can be created only once, and only on RAM disk device. */
	dev = m_ptr->DEVICE;
	if(dev < 0 || dev >= NR_DEVS) {
		printf("MEM: MIOCRAMSIZE: %d not a valid device\n", dev);
	}
	if((dev < RAM_DEV_FIRST || dev > RAM_DEV_LAST) && dev != RAM_DEV_OLD) {
		printf("MEM: MIOCRAMSIZE: %d not a ramdisk\n", dev);
	}
        if ((dv = m_prepare(dev)) == NULL) return(ENXIO);

	/* Get request structure */
	   s= sys_safecopyfrom(m_ptr->IO_ENDPT, (vir_bytes)m_ptr->IO_GRANT,
		0, (vir_bytes)&ramdev_size, sizeof(ramdev_size), D);
	if (s != OK)
		return s;
	if(m_vaddrs[dev] && !cmp64(dv->dv_size, cvul64(ramdev_size))) {
		return(OK);
	}
	/* openct is 1 for the ioctl(). */
	if(openct[dev] != 1) {
		printf("MEM: MIOCRAMSIZE: %d in use (count %d)\n",
			dev, openct[dev]);
		return(EBUSY);
	}
	if(m_vaddrs[dev]) {
		u32_t size;
		if(ex64hi(dv->dv_size)) {
			panic("huge old ramdisk");
		}
		size = ex64lo(dv->dv_size);
		munmap((void *) m_vaddrs[dev], size);
		m_vaddrs[dev] = (vir_bytes) NULL;
	}

#if DEBUG
	printf("MEM:%d: allocating ramdisk of size 0x%x\n", dev, ramdev_size);
#endif

	/* Try to allocate a piece of memory for the RAM disk. */
	if((mem = mmap(NULL, ramdev_size, PROT_READ|PROT_WRITE,
	  MAP_PREALLOC|MAP_ANON, -1, 0)) == MAP_FAILED) {
	    printf("MEM: failed to get memory for ramdisk\n");
            return(ENOMEM);
        }

	m_vaddrs[dev] = (vir_bytes) mem;

	dv->dv_size = cvul64(ramdev_size);

	break;
    }

    default:
  	return(do_diocntl(&m_dtab, m_ptr));
  }
  return(OK);
}

/*===========================================================================*
 *				m_geometry				     *
 *===========================================================================*/
PRIVATE void m_geometry(entry)
struct partition *entry;
{
  /* Memory devices don't have a geometry, but the outside world insists. */
  entry->cylinders = div64u(m_geom[m_device].dv_size, SECTOR_SIZE) / (64 * 32);
  entry->heads = 64;
  entry->sectors = 32;
}

