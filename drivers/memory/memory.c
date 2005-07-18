/* This file contains the device dependent part of the drivers for the
 * following special files:
 *     /dev/ram		- RAM disk 
 *     /dev/mem		- absolute memory
 *     /dev/kmem	- kernel virtual memory
 *     /dev/null	- null device (data sink)
 *     /dev/boot	- boot device loaded from boot image 
 *     /dev/random	- random number generator
 *     /dev/zero	- null byte stream generator
 *
 *  Changes:
 *	Apr 29, 2005	added null byte generator  (Jorrit N. Herder)
 *	Apr 27, 2005	added random device handling  (Jorrit N. Herder)
 *	Apr 09, 2005	added support for boot device  (Jorrit N. Herder)
 *	Jul 26, 2004	moved RAM driver to user-space  (Jorrit N. Herder)
 *	Apr 20, 1992	device dependent/independent split  (Kees J. Bot)
 */

#include "../drivers.h"
#include "../libdriver/driver.h"
#include <sys/ioc_memory.h>
#include "../../kernel/const.h"
#include "../../kernel/config.h"
#include "../../kernel/type.h"

#include "assert.h"
#include "random.h"

#define NR_DEVS            7		/* number of minor devices */
#define KRANDOM_PERIOD    10 		/* ticks between krandom calls */

PRIVATE struct device m_geom[NR_DEVS];  /* base and size of each device */
PRIVATE int m_seg[NR_DEVS];  		/* segment index of each device */
PRIVATE int m_device;			/* current device */
PRIVATE struct kinfo kinfo;		/* kernel information */ 
PRIVATE struct machine machine;		/* machine information */ 
PRIVATE struct randomness krandom;	/* randomness from the kernel */ 

extern int errno;			/* error number for PM calls */

FORWARD _PROTOTYPE( char *m_name, (void) );
FORWARD _PROTOTYPE( struct device *m_prepare, (int device) );
FORWARD _PROTOTYPE( int m_transfer, (int proc_nr, int opcode, off_t position,
					iovec_t *iov, unsigned nr_req) );
FORWARD _PROTOTYPE( int m_do_open, (struct driver *dp, message *m_ptr) );
FORWARD _PROTOTYPE( void m_init, (void) );
FORWARD _PROTOTYPE( int m_ioctl, (struct driver *dp, message *m_ptr) );
FORWARD _PROTOTYPE( void m_geometry, (struct partition *entry) );
FORWARD _PROTOTYPE( void m_random, (struct driver *dp) );

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
  nop_stop,	/* no need to clean up on shutdown */
  m_random, 	/* get randomness from kernel (alarm) */
  nop_fkey,	/* ignore function key presses and CANCELs */
  nop_cancel,
  nop_select,
  NULL
};

/* Buffer for the /dev/zero null byte feed. */
#define ZERO_BUF_SIZE 			1024
PRIVATE char dev_zero[ZERO_BUF_SIZE];

/* Buffer for the /dev/random number generator. */
#define RANDOM_BUF_SIZE 		1024
PRIVATE char random_buf[RANDOM_BUF_SIZE];


#define click_to_round_k(n) \
	((unsigned) ((((unsigned long) (n) << CLICK_SHIFT) + 512) / 1024))


/*===========================================================================*
 *				   main 				     *
 *===========================================================================*/
PUBLIC void main(void)
{
  m_init();			/* initialize the memory driver */
  driver_task(&m_dtab);		/* start driver's main loop */
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
PRIVATE int m_transfer(proc_nr, opcode, position, iov, nr_req)
int proc_nr;			/* process doing the request */
int opcode;			/* DEV_GATHER or DEV_SCATTER */
off_t position;			/* offset on device to read or write */
iovec_t *iov;			/* pointer to read or write request vector */
unsigned nr_req;		/* length of request vector */
{
/* Read or write one the driver's minor devices. */
  phys_bytes mem_phys, user_phys;
  int seg;
  unsigned count, left, chunk;
  vir_bytes user_vir;
  struct device *dv;
  unsigned long dv_size;
  int s;

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
	    if (opcode == DEV_GATHER) return(OK);	/* always at EOF */
	    break;

	/* Virtual copying. For boot device. */
	case RAM_DEV:
	case KMEM_DEV:
	case BOOT_DEV:
	    if (position >= dv_size) return(OK); 	/* check for EOF */
	    if (position + count > dv_size) count = dv_size - position;
	    seg = m_seg[m_device];

	    if (opcode == DEV_GATHER) {			/* copy actual data */
	        sys_vircopy(SELF,seg,position, proc_nr,D,user_vir, count);
	    } else {
	        sys_vircopy(proc_nr,D,user_vir, SELF,seg,position, count);
	    }
	    break;

	/* Physical copying. Only used to access entire memory. */
	case MEM_DEV:
	    if (position >= dv_size) return(OK); 	/* check for EOF */
	    if (position + count > dv_size) count = dv_size - position;
	    mem_phys = cv64ul(dv->dv_base) + position;

	    if (opcode == DEV_GATHER) {			/* copy data */
	        sys_physcopy(NONE, PHYS_SEG, mem_phys, 
	        	proc_nr, D, user_vir, count);
	    } else {
	        sys_physcopy(proc_nr, D, user_vir, 
	        	NONE, PHYS_SEG, mem_phys, count);
	    }
	    break;

	/* Random number generator. Character instead of block device. */
	case RANDOM_DEV:
	    if (opcode == DEV_GATHER)
	    {
		s= random_reseed();
		if (s < 0)
		    return(EAGAIN);
	    }
	    left = count;
	    while (left > 0) {
	    	chunk = (left > RANDOM_BUF_SIZE) ? RANDOM_BUF_SIZE : left;
 	        if (opcode == DEV_GATHER) {
		    random_getbytes(random_buf, chunk);
	    	    sys_vircopy(SELF, D, (vir_bytes) random_buf, 
	    	        proc_nr, D, user_vir, chunk);
 	        } else if (opcode == DEV_SCATTER) {
	    	    sys_vircopy(proc_nr, D, user_vir, 
	    	        SELF, D, (vir_bytes) random_buf, chunk);
	    	        random_putbytes(random_buf, chunk);
 	        }
	    	left -= chunk;
	    }
	    break;

	/* Null byte stream generator. */
	case ZERO_DEV:
	    if (opcode == DEV_GATHER) {
	        left = count;
	    	while (left > 0) {
	    	    chunk = (left > ZERO_BUF_SIZE) ? ZERO_BUF_SIZE : left;
	    	    if (OK != (s=sys_vircopy(SELF, D, (vir_bytes) dev_zero, 
	    	            proc_nr, D, user_vir, chunk)))
	    	        report("MEM","sys_vircopy failed", s);
	    	    left -= chunk;
	    	}
	    }
	    break;

	/* Unknown (illegal) minor device. */
	default:
	    return(EINVAL);
	}

	/* Book the number of bytes transferred. */
	position += count;
	iov->iov_addr += count;
  	if ((iov->iov_size -= count) == 0) { iov++; nr_req--; }

  }
  return(OK);
}


/*============================================================================*
 *				m_do_open				      *
 *============================================================================*/
PRIVATE int m_do_open(dp, m_ptr)
struct driver *dp;
message *m_ptr;
{
/* Check device number on open.  (This used to give I/O privileges to a 
 * process opening /dev/mem or /dev/kmem. This may be needed in case of 
 * memory mapped I/O. With system calls to do I/O this is no longer needed.)
 */
  if (m_prepare(m_ptr->DEVICE) == NIL_DEV) return(ENXIO);

  return(OK);
}


/*===========================================================================*
 *				m_init					     *
 *===========================================================================*/
PRIVATE void m_init()
{
  /* Initialize this task. All minor devices are initialized one by one. */
  int i, s;

  if (OK != (s=sys_getkinfo(&kinfo))) {
      panic("MEM","Couldn't get kernel information.",s);
  }

  /* Install remote segment for /dev/kmem memory. */
  m_geom[KMEM_DEV].dv_base = cvul64(kinfo.kmem_base);
  m_geom[KMEM_DEV].dv_size = cvul64(kinfo.kmem_size);
  if (OK != (s=sys_segctl(&m_seg[KMEM_DEV], (u16_t *) &s, (vir_bytes *) &s, 
  		kinfo.kmem_base, kinfo.kmem_size))) {
      panic("MEM","Couldn't install remote segment.",s);
  }

  /* Install remote segment for /dev/boot memory, if enabled. */
  m_geom[BOOT_DEV].dv_base = cvul64(kinfo.bootdev_base);
  m_geom[BOOT_DEV].dv_size = cvul64(kinfo.bootdev_size);
  if (kinfo.bootdev_base > 0) {
      if (OK != (s=sys_segctl(&m_seg[BOOT_DEV], (u16_t *) &s, (vir_bytes *) &s, 
              kinfo.bootdev_base, kinfo.bootdev_size))) {
          panic("MEM","Couldn't install remote segment.",s);
      }
  }

  /* Initialize /dev/zero. Simply write zeros into the buffer. */
  for (i=0; i<ZERO_BUF_SIZE; i++) {
       dev_zero[i] = '\0';
  }

  random_init();
  m_random(NULL);				/* also set periodic timer */

  /* Set up memory ranges for /dev/mem. */
#if (CHIP == INTEL)
  if (OK != (s=sys_getmachine(&machine))) {
      panic("MEM","Couldn't get machine information.",s);
  }
  if (! machine.protected) {
	m_geom[MEM_DEV].dv_size =   cvul64(0x100000); /* 1M for 8086 systems */
  } else {
#if _WORD_SIZE == 2
	m_geom[MEM_DEV].dv_size =  cvul64(0x1000000); /* 16M for 286 systems */
#else
	m_geom[MEM_DEV].dv_size = cvul64(0xFFFFFFFF); /* 4G-1 for 386 systems */
#endif
  }
#else /* !(CHIP == INTEL) */
#if (CHIP == M68000)
  m_geom[MEM_DEV].dv_size = cvul64(MEM_BYTES);
#else /* !(CHIP == M68000) */
#error /* memory limit not set up */
#endif /* !(CHIP == M68000) */
#endif /* !(CHIP == INTEL) */

  /* Initialization succeeded. Print welcome message. */
  report("MEM","user-space memory driver has been initialized.", NO_NUM);
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
  if ((dv = m_prepare(m_ptr->DEVICE)) == NIL_DEV) return(ENXIO);

  switch (m_ptr->REQUEST) {
    case MIOCRAMSIZE: {
	/* FS wants to create a new RAM disk with the given size. */
	phys_bytes ramdev_size;
	phys_bytes ramdev_base;
	message m;
	int s;

	if (m_ptr->PROC_NR != FS_PROC_NR) {
	    report("MEM", "warning, MIOCRAMSIZE called by", m_ptr->PROC_NR);
	    return(EPERM);
	}

	/* Try to allocate a piece of memory for the RAM disk. */
	ramdev_size = m_ptr->POSITION;
        if (allocmem(ramdev_size, &ramdev_base) < 0) {
            report("MEM", "warning, allocmem failed", errno);
            return(ENOMEM);
        }
	dv->dv_base = cvul64(ramdev_base);
	dv->dv_size = cvul64(ramdev_size);

  	if (OK != (s=sys_segctl(&m_seg[RAM_DEV], (u16_t *) &s, (vir_bytes *) &s, 
  		ramdev_base, ramdev_size))) {
      		panic("MEM","Couldn't install remote segment.",s);
  	}
	break;
    }

    default:
  	return(do_diocntl(&m_dtab, m_ptr));
  }
  return(OK);
}


/*============================================================================*
 *				m_random				      *
 *============================================================================*/
PRIVATE void m_random(dp)
struct driver *dp;			/* pointer to driver structure */
{
  /* Fetch random information from the kernel to update /dev/random. */
  int i, s, r_next, r_size, r_high;
  struct randomness krandom;

  if (OK != (s=sys_getrandomness(&krandom)))
  	report("MEM", "sys_getrandomness failed", s);

  for (i= 0; i<RANDOM_SOURCES; i++)	
  {
  	r_next= krandom.bin[i].r_next;
  	r_size= krandom.bin[i].r_size;
  	r_high= r_next+r_size;
  	if (r_high <= RANDOM_ELEMENTS)
  	{
  		random_update(i, &krandom.bin[i].r_buf[r_next], r_size);
	}
	else
	{
		assert(r_next < RANDOM_ELEMENTS);
  		random_update(i, &krandom.bin[i].r_buf[r_next],
  			RANDOM_ELEMENTS-r_next);
  		random_update(i, &krandom.bin[i].r_buf[0],
  			r_high-RANDOM_ELEMENTS);
	}
  }

  /* Schedule new alarm for next m_random call. */
  if (OK != (s=sys_syncalrm(SELF, KRANDOM_PERIOD, 0)))
  	report("MEM", "sys_syncalarm failed", s);
}

/*============================================================================*
 *				m_geometry				      *
 *============================================================================*/
PRIVATE void m_geometry(entry)
struct partition *entry;
{
  /* Memory devices don't have a geometry, but the outside world insists. */
  entry->cylinders = div64u(m_geom[m_device].dv_size, SECTOR_SIZE) / (64 * 32);
  entry->heads = 64;
  entry->sectors = 32;
}


