/* This file contains the device dependent part of the drivers for the
 * following special files:
 *     /dev/ram		- RAM disk 
 *     /dev/mem		- absolute memory
 *     /dev/kmem	- kernel virtual memory
 *     /dev/null	- null device (data sink)
 *     /dev/boot	- boot FS loaded from boot image 
 *
 *  Changes:
 *	Apr 09, 2005	added support for boot FS  (Jorrit N. Herder)
 *	Sep 03, 2004	secured code with ENABLE_USERPRIV  (Jorrit N. Herder)
 *	Jul 26, 2004	moved RAM driver to user-space  (Jorrit N. Herder)
 *	Apr 20, 1992	device dependent/independent split  (Kees J. Bot)
 */

#include "../drivers.h"
#include "../libdriver/driver.h"
#include <sys/ioc_memory.h>
#if (CHIP == INTEL) && ENABLE_USERBIOS
#include <ibm/int86.h>
#endif

#define NR_DEVS            6		/* number of RAM-type devices */

PRIVATE struct device m_geom[NR_DEVS];  /* base and size of each RAM disk */
PRIVATE int m_device;			/* current device */
PRIVATE struct kenviron kenv;		/* need protected_mode */
PRIVATE struct psinfo psinfo = { NR_TASKS, NR_PROCS, 0, 0, 0 };

#define RANDOM_BUFFER_SIZE	(1024*32)
PRIVATE char random_state[RANDOM_BUFFER_SIZE];

FORWARD _PROTOTYPE( struct device *m_prepare, (int device) );
FORWARD _PROTOTYPE( int m_transfer, (int proc_nr, int opcode, off_t position,
					iovec_t *iov, unsigned nr_req) );
FORWARD _PROTOTYPE( int m_do_open, (struct driver *dp, message *m_ptr) );
FORWARD _PROTOTYPE( void m_init, (void) );
FORWARD _PROTOTYPE( int m_ioctl, (struct driver *dp, message *m_ptr) );
FORWARD _PROTOTYPE( void m_geometry, (struct partition *entry) );
FORWARD _PROTOTYPE( char *m_name, (void) );


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
  nop_alarm,	/* ignore leftover alarms */
};


/*===========================================================================*
 *				   main 				     *
 *===========================================================================*/
PUBLIC void main(void)
{
  m_init();
  driver_task(&m_dtab);
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
/* Read or write /dev/null, /dev/mem, /dev/kmem, /dev/ram, /dev/boot,
 * /dev/random, or /dev/urandom
 */

  int device;
  phys_bytes mem_phys, user_phys;
  unsigned count;
  vir_bytes user_vir;
  struct device *dv;
  unsigned long dv_size;

  /* Get minor device number and check for /dev/null. */
  device = m_device;
  dv = &m_geom[device];
  dv_size = cv64ul(dv->dv_size);


  while (nr_req > 0) {
	count = iov->iov_size;
	user_vir = iov->iov_addr;

	switch (device) {
	case NULL_DEV:
	    if (opcode == DEV_GATHER) return(OK);	/* always at EOF */
	    break;
	case RANDOM_DEV:
		return OK;
		break;

	default:
	    /* /dev/mem, /dev/kmem, /dev/ram, /dev/boot: check for EOF */
	    if (position >= dv_size) return(OK);
	    if (position + count > dv_size) count = dv_size - position;
	    mem_phys = cv64ul(dv->dv_base) + position;

	    /* Copy the data. */
	    if (opcode == DEV_GATHER) {
	        sys_copy(ABS, D, mem_phys, proc_nr, D, user_vir, count);
	    } else {
	        sys_copy(proc_nr, D, user_vir, ABS, D, mem_phys, count);
	    }
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
/* Check device number on open.  Give I/O privileges to a process opening
 * /dev/mem or /dev/kmem. This is needed for systems with memory mapped I/O.
 */
  if (m_prepare(m_ptr->DEVICE) == NIL_DEV) return(ENXIO);

#if (CHIP == INTEL) && ENABLE_USERPRIV && ENABLE_USERIOPL
  if (m_device == MEM_DEV || m_device == KMEM_DEV) {
	sys_enable_iop(m_ptr->PROC_NR);
	printf("MEMORY: sys_enable_iop for proc nr %d.\n", m_ptr->PROC_NR);
  }
#endif

  return(OK);
}


/*===========================================================================*
 *				m_init					     *
 *===========================================================================*/
PRIVATE void m_init()
{
  /* Initialize this task. */
  extern int end;
  int s;

  /* Print welcome message. */
  printf("MEMORY: user-level memory (RAM) driver is alive");

  /* Get kernel environment (protected_mode and addresses). */
  if (OK != (s=sys_getkenviron(&kenv))) {
      server_panic("MEM","Couldn't get kernel environment.",s);
  }
  m_geom[KMEM_DEV].dv_base = cvul64(kenv.kmem_base);
  m_geom[KMEM_DEV].dv_size = cvul64(kenv.kmem_size);
  m_geom[BOOT_DEV].dv_base = cvul64(kenv.bootfs_base);
  m_geom[BOOT_DEV].dv_size = cvul64(kenv.bootfs_size);

  /* dv_base isn't used for the random device */
  m_geom[RANDOM_DEV].dv_base = cvul64(NULL);
  m_geom[RANDOM_DEV].dv_size = cvul64(RANDOM_BUFFER_SIZE);

  psinfo.proc = kenv.proc_addr;

#if (CHIP == INTEL)
  if (!kenv.protected) {
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
}


/*===========================================================================*
 *				m_ioctl					     *
 *===========================================================================*/
PRIVATE int m_ioctl(dp, m_ptr)
struct driver *dp;
message *m_ptr;			/* pointer to read or write message */
{
/* Set parameters for the RAM disk. */

  struct device *dv;

  if ((dv = m_prepare(m_ptr->DEVICE)) == NIL_DEV) return(ENXIO);

  switch (m_ptr->REQUEST) {
    case MIOCRAMSIZE: {
	/* FS wants to create a new RAM disk with the given size. */
	unsigned long bytesize;
	phys_bytes base;
	int s;

	if (m_ptr->PROC_NR != FS_PROC_NR) return(EPERM);

	/* Try to allocate a piece of kernel memory for the RAM disk. */
	bytesize = m_ptr->POSITION;
	if (OK != (s = sys_kmalloc(bytesize, &base)))
	    server_panic("MEM","Couldn't allocate kernel memory", s);
	dv->dv_base = cvul64(base);
	dv->dv_size = cvul64(bytesize);
	break;
    }
    /* Perhaps it is cleaner to move all code relating to psinfo to the info
     * server??? (Note that psinfo is global; psinfo.proc is set in m_init.)
     * This requires changes to ioctl as well. 
     */
    case MIOCSPSINFO: {		
	/* MM or FS set the address of their process table. */
	phys_bytes psinfo_phys;

	if (m_ptr->PROC_NR == MM_PROC_NR) {
		psinfo.mproc = (vir_bytes) m_ptr->ADDRESS;
	} else
	if (m_ptr->PROC_NR == FS_PROC_NR) {
		psinfo.fproc = (vir_bytes) m_ptr->ADDRESS;
	} else {
		return(EPERM);
	}
	break;
    }
    case MIOCGPSINFO: {
	/* The ps program wants the process table addresses. */
	if (sys_datacopy(SELF, (vir_bytes) &psinfo,
		m_ptr->PROC_NR, (vir_bytes) m_ptr->ADDRESS,
		sizeof(psinfo)) != OK) return(EFAULT);
	break;
    }

    default:
  	return(do_diocntl(&m_dtab, m_ptr));
  }
  return(OK);
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
