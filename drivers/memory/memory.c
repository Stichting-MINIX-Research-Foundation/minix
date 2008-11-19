/* This file contains the device dependent part of the drivers for the
 * following special files:
 *     /dev/ram		- RAM disk 
 *     /dev/mem		- absolute memory
 *     /dev/kmem	- kernel virtual memory
 *     /dev/null	- null device (data sink)
 *     /dev/boot	- boot device loaded from boot image 
 *     /dev/zero	- null byte stream generator
 *
 *  Changes:
 *	Apr 29, 2005	added null byte generator  (Jorrit N. Herder)
 *	Apr 09, 2005	added support for boot device  (Jorrit N. Herder)
 *	Jul 26, 2004	moved RAM driver to user-space  (Jorrit N. Herder)
 *	Apr 20, 1992	device dependent/independent split  (Kees J. Bot)
 */

#include "../drivers.h"
#include "../libdriver/driver.h"
#include <sys/ioc_memory.h>
#include <env.h>
#include <minix/ds.h>
#include "../../kernel/const.h"
#include "../../kernel/config.h"
#include "../../kernel/type.h"

#define MY_DS_NAME_BASE "dev:memory:ramdisk_base"
#define MY_DS_NAME_SIZE "dev:memory:ramdisk_size"

#include <sys/vm.h>
#include <sys/vm_i386.h>

#include "assert.h"

#include "local.h"

#define NR_DEVS            7		/* number of minor devices */

PRIVATE struct device m_geom[NR_DEVS];  /* base and size of each device */
PRIVATE int m_seg[NR_DEVS];  		/* segment index of each device */
PRIVATE int m_device;			/* current device */
PRIVATE struct kinfo kinfo;		/* kernel information */ 

extern int errno;			/* error number for PM calls */

FORWARD _PROTOTYPE( char *m_name, (void) 				);
FORWARD _PROTOTYPE( struct device *m_prepare, (int device) 		);
FORWARD _PROTOTYPE( int m_transfer, (int proc_nr, int opcode, u64_t position,
				iovec_t *iov, unsigned nr_req, int safe));
FORWARD _PROTOTYPE( int m_do_open, (struct driver *dp, message *m_ptr) 	);
FORWARD _PROTOTYPE( void m_init, (void) );
FORWARD _PROTOTYPE( int m_ioctl, (struct driver *dp, message *m_ptr, int safe));
FORWARD _PROTOTYPE( void m_geometry, (struct partition *entry) 		);

/* Entry points to this driver. */
PRIVATE struct driver m_dtab = {
  m_name,	/* current device's name */
  m_do_open,	/* open or mount */
  do_nop,	/* nothing on a close */
  m_ioctl,	/* specify ram disk geometry */
  m_prepare,	/* prepare for I/O on a given minor device */
  m_transfer,	/* do the I/O */
  nop_cleanup,	/* no need to clean up */
  m_geometry,	/* memory device "geometry" */
  nop_signal,	/* system signals */
  nop_alarm,
  nop_cancel,
  nop_select,
  NULL,
  NULL
};

/* One page of temporary mapping area - enough to be able to page-align
 * one page.
 */
static char pagedata_buf[2*I386_PAGE_SIZE];
vir_bytes pagedata_aligned;

/* Buffer for the /dev/zero null byte feed. */
#define ZERO_BUF_SIZE 			1024
PRIVATE char dev_zero[ZERO_BUF_SIZE];

#define click_to_round_k(n) \
	((unsigned) ((((unsigned long) (n) << CLICK_SHIFT) + 512) / 1024))

/*===========================================================================*
 *				   main 				     *
 *===========================================================================*/
PUBLIC int main(void)
{
/* Main program. Initialize the memory driver and start the main loop. */
  struct sigaction sa;

  sa.sa_handler = SIG_MESS;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  if (sigaction(SIGTERM,&sa,NULL)<0) panic("MEM","sigaction failed", errno);

  m_init();			
  driver_task(&m_dtab);		
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
  if (device < 0 || device >= NR_DEVS) return(NIL_DEV);
  m_device = device;

  return(&m_geom[device]);
}

/*===========================================================================*
 *				m_transfer				     *
 *===========================================================================*/
PRIVATE int m_transfer(proc_nr, opcode, pos64, iov, nr_req, safe)
int proc_nr;			/* process doing the request */
int opcode;			/* DEV_GATHER_S or DEV_SCATTER_S */
u64_t pos64;			/* offset on device to read or write */
iovec_t *iov;			/* pointer to read or write request vector */
unsigned nr_req;		/* length of request vector */
int safe;			/* safe copies */
{
/* Read or write one the driver's minor devices. */
  phys_bytes mem_phys;
  int seg;
  unsigned count, left, chunk;
  vir_bytes user_vir, vir_offset = 0;
  struct device *dv;
  unsigned long dv_size;
  int s, r;
  off_t position;

  if(!safe) {
	printf("m_transfer: unsafe?\n");
	return EPERM;
  }

  /* ZERO_DEV and NULL_DEV are infinite in size. */
  if (m_device != ZERO_DEV && m_device != NULL_DEV && ex64hi(pos64) != 0)
	return OK;	/* Beyond EOF */
  position= cv64ul(pos64);

  /* Get minor device number and check for /dev/null. */
  dv = &m_geom[m_device];
  dv_size = cv64ul(dv->dv_size);

  while (nr_req > 0) {

	/* How much to transfer and where to / from. */
	count = iov->iov_size;
	user_vir = iov->iov_addr;

	switch (m_device) {

	/* No copying; ignore request. */
	case NULL_DEV:
	    if (opcode == DEV_GATHER_S) return(OK);	/* always at EOF */
	    break;

	/* Virtual copying. For RAM disk, kernel memory and boot device. */
	case KMEM_DEV:
		return EIO;
		break;
	case RAM_DEV:
	case BOOT_DEV:
	    if (position >= dv_size) return(OK); 	/* check for EOF */
	    if (position + count > dv_size) count = dv_size - position;
	    seg = m_seg[m_device];

	    if (opcode == DEV_GATHER_S) {			/* copy actual data */
	        r=sys_safecopyto(proc_nr, user_vir, vir_offset,
	  	  position, count, seg);
	    } else {
	        r=sys_safecopyfrom(proc_nr, user_vir, vir_offset,
	  	  position, count, seg);
	    }
	    if(r != OK) {
              panic("MEM","I/O copy failed",r);
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
	    int r;
	    u32_t subcount;

	    if (position >= dv_size)
		return(OK); 	/* check for EOF */
	    if (position + count > dv_size)
		count = dv_size - position;
	    mem_phys = cv64ul(dv->dv_base) + position;

	    page_off = mem_phys % I386_PAGE_SIZE;
	    pagestart = mem_phys - page_off; 

	    /* All memory to the map call has to be page-aligned.
	     * Don't have to map same page over and over.
	     */
	    if(!any_mapped || pagestart_mapped != pagestart) {
#if 0
	      if((r=sys_vm_map(SELF, 1, pagedata_aligned,
		I386_PAGE_SIZE, pagestart)) != OK) {
#else
		if(1) {
#endif
		printf("memory: sys_vm_map failed: %d\n", r);
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
		       vir_offset, pagedata_aligned+page_off, subcount, D);
	    } else {
	           s=sys_safecopyfrom(proc_nr, user_vir,
		       vir_offset, pagedata_aligned+page_off, subcount, D);
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
	    	        report("MEM","sys_safecopyto failed", s);
	    	    left -= chunk;
 	            suboffset += chunk;
	    	}
	    }
	    break;

	case IMGRD_DEV:
	    if (position >= dv_size) return(OK); 	/* check for EOF */
	    if (position + count > dv_size) count = dv_size - position;

	    if (opcode == DEV_GATHER_S) {	/* copy actual data */
	          s=sys_safecopyto(proc_nr, user_vir, vir_offset,
	  	     (vir_bytes)&imgrd[position], count, D);
	    } else {
	          s=sys_safecopyfrom(proc_nr, user_vir, vir_offset,
	  	     (vir_bytes)&imgrd[position], count, D);
	    }
	    break;

	/* Unknown (illegal) minor device. */
	default:
	    return(EINVAL);
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
  if (m_prepare(m_ptr->DEVICE) == NIL_DEV) return(ENXIO);
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
  return(OK);
}

/*===========================================================================*
 *				m_init					     *
 *===========================================================================*/
PRIVATE void m_init()
{
  /* Initialize this task. All minor devices are initialized one by one. */
  u32_t ramdev_size;
  u32_t ramdev_base;
  int i, s;

  if (OK != (s=sys_getkinfo(&kinfo))) {
      panic("MEM","Couldn't get kernel information.",s);
  }

  /* Install remote segment for /dev/kmem memory. */
#if 0
  m_geom[KMEM_DEV].dv_base = cvul64(kinfo.kmem_base);
  m_geom[KMEM_DEV].dv_size = cvul64(kinfo.kmem_size);
  if (OK != (s=sys_segctl(&m_seg[KMEM_DEV], (u16_t *) &s, (vir_bytes *) &s, 
  		kinfo.kmem_base, kinfo.kmem_size))) {
      panic("MEM","Couldn't install remote segment.",s);
  }
#endif

  /* Install remote segment for /dev/boot memory, if enabled. */
  m_geom[BOOT_DEV].dv_base = cvul64(kinfo.bootdev_base);
  m_geom[BOOT_DEV].dv_size = cvul64(kinfo.bootdev_size);
  if (kinfo.bootdev_base > 0) {
      if (OK != (s=sys_segctl(&m_seg[BOOT_DEV], (u16_t *) &s, (vir_bytes *) &s, 
              kinfo.bootdev_base, kinfo.bootdev_size))) {
          panic("MEM","Couldn't install remote segment.",s);
      }
  }

  /* See if there are already RAM disk details at the Data Store server. */
  if(ds_retrieve_u32(MY_DS_NAME_BASE, &ramdev_base) == OK &&
     ds_retrieve_u32(MY_DS_NAME_SIZE, &ramdev_size) == OK) {
  	printf("MEM retrieved size %u and base %u from DS, status %d\n",
    		ramdev_size, ramdev_base, s);
  	if (OK != (s=sys_segctl(&m_seg[RAM_DEV], (u16_t *) &s, 
		(vir_bytes *) &s, ramdev_base, ramdev_size))) {
      		panic("MEM","Couldn't install remote segment.",s);
  	}
  	m_geom[RAM_DEV].dv_base = cvul64(ramdev_base);
 	m_geom[RAM_DEV].dv_size = cvul64(ramdev_size);
	printf("MEM stored retrieved details as new RAM disk\n");
  }

  /* Ramdisk image built into the memory driver */
  m_geom[IMGRD_DEV].dv_base= cvul64(0);
  m_geom[IMGRD_DEV].dv_size= cvul64(imgrd_size);

  /* Initialize /dev/zero. Simply write zeros into the buffer. */
  for (i=0; i<ZERO_BUF_SIZE; i++) {
       dev_zero[i] = '\0';
  }

  /* Page-align page pointer. */
  pagedata_aligned = (u32_t) pagedata_buf + I386_PAGE_SIZE;
  pagedata_aligned -= pagedata_aligned % I386_PAGE_SIZE;

  /* Set up memory range for /dev/mem. */
  m_geom[MEM_DEV].dv_size = cvul64(0xffffffff);
}

/*===========================================================================*
 *				m_ioctl					     *
 *===========================================================================*/
PRIVATE int m_ioctl(dp, m_ptr, safe)
struct driver *dp;			/* pointer to driver structure */
message *m_ptr;				/* pointer to control message */
int safe;
{
/* I/O controls for the memory driver. Currently there is one I/O control:
 * - MIOCRAMSIZE: to set the size of the RAM disk.
 */
  struct device *dv;

  if(!safe) {
	printf("m_transfer: unsafe?\n");
	return EPERM;
  }

  switch (m_ptr->REQUEST) {
    case MIOCRAMSIZE: {
	/* Someone wants to create a new RAM disk with the given size. */
	static int first_time= 1;

	u32_t ramdev_size;
	phys_bytes ramdev_base;
	int s;

	/* A ramdisk can be created only once, and only on RAM disk device. */
	if (!first_time) return(EPERM);
	if (m_ptr->DEVICE != RAM_DEV) return(EINVAL);
        if ((dv = m_prepare(m_ptr->DEVICE)) == NIL_DEV) return(ENXIO);

	/* Get request structure */
	   s= sys_safecopyfrom(m_ptr->IO_ENDPT, (vir_bytes)m_ptr->IO_GRANT,
		0, (vir_bytes)&ramdev_size, sizeof(ramdev_size), D);
	if (s != OK)
		return s;

#if DEBUG
	printf("allocating ramdisk of size 0x%x\n", ramdev_size);
#endif

	/* Try to allocate a piece of memory for the RAM disk. */
        if (allocmem(ramdev_size, &ramdev_base) < 0) {
            report("MEM", "warning, allocmem failed", errno);
            return(ENOMEM);
        }

	/* Store the values we got in the data store so we can retrieve
	 * them later on, in the unfortunate event of a crash.
	 */
	if(ds_publish_u32(MY_DS_NAME_BASE, ramdev_base) != OK ||
	   ds_publish_u32(MY_DS_NAME_SIZE, ramdev_size) != OK) {
      		panic("MEM","Couldn't store RAM disk details at DS.",s);
	}

#if DEBUG
	printf("MEM stored size %u and base %u at DS, names %s and %s\n",
	    ramdev_size, ramdev_base, MY_DS_NAME_BASE, MY_DS_NAME_SIZE);
#endif

  	if (OK != (s=sys_segctl(&m_seg[RAM_DEV], (u16_t *) &s, 
		(vir_bytes *) &s, ramdev_base, ramdev_size))) {
      		panic("MEM","Couldn't install remote segment.",s);
  	}

	dv->dv_base = cvul64(ramdev_base);
	dv->dv_size = cvul64(ramdev_size);
	first_time= 0;
	break;
    }

    default:
  	return(do_diocntl(&m_dtab, m_ptr, safe));
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
