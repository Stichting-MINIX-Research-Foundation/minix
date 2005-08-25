/* This file contains the device dependent part of the drivers for the
 * following special files:
 *     /dev/random	- random number generator
 */

#include "../drivers.h"
#include "../libdriver/driver.h"
#include <sys/ioc_memory.h>
#include "../../kernel/const.h"
#include "../../kernel/config.h"
#include "../../kernel/type.h"

#include "assert.h"
#include "random.h"

#define NR_DEVS            1		/* number of minor devices */
#  define RANDOM_DEV  0			/* minor device for /dev/random */

#define KRANDOM_PERIOD    1 		/* ticks between krandom calls */

PRIVATE struct device m_geom[NR_DEVS];  /* base and size of each device */
PRIVATE int m_device;			/* current device */

extern int errno;			/* error number for PM calls */

FORWARD _PROTOTYPE( char *r_name, (void) );
FORWARD _PROTOTYPE( struct device *r_prepare, (int device) );
FORWARD _PROTOTYPE( int r_transfer, (int proc_nr, int opcode, off_t position,
					iovec_t *iov, unsigned nr_req) );
FORWARD _PROTOTYPE( int r_do_open, (struct driver *dp, message *m_ptr) );
FORWARD _PROTOTYPE( void r_init, (void) );
FORWARD _PROTOTYPE( int r_ioctl, (struct driver *dp, message *m_ptr) );
FORWARD _PROTOTYPE( void r_geometry, (struct partition *entry) );
FORWARD _PROTOTYPE( void r_random, (struct driver *dp, message *m_ptr) );

/* Entry points to this driver. */
PRIVATE struct driver r_dtab = {
  r_name,	/* current device's name */
  r_do_open,	/* open or mount */
  do_nop,	/* nothing on a close */
  r_ioctl,	/* specify ram disk geometry */
  r_prepare,	/* prepare for I/O on a given minor device */
  r_transfer,	/* do the I/O */
  nop_cleanup,	/* no need to clean up */
  r_geometry,	/* device "geometry" */
  nop_signal,	/* system signals */
  r_random, 	/* get randomness from kernel (alarm) */
  nop_cancel,
  nop_select,
  NULL,
  NULL
};

/* Buffer for the /dev/random number generator. */
#define RANDOM_BUF_SIZE 		1024
PRIVATE char random_buf[RANDOM_BUF_SIZE];

/*===========================================================================*
 *				   main 				     *
 *===========================================================================*/
PUBLIC int main(void)
{
  r_init();			/* initialize the memory driver */
  driver_task(&r_dtab);		/* start driver's main loop */
  return(OK);
}

/*===========================================================================*
 *				 r_name					     *
 *===========================================================================*/
PRIVATE char *r_name()
{
/* Return a name for the current device. */
  static char name[] = "random";
  return name;  
}

/*===========================================================================*
 *				r_prepare				     *
 *===========================================================================*/
PRIVATE struct device *r_prepare(device)
int device;
{
/* Prepare for I/O on a device: check if the minor device number is ok. */

  if (device < 0 || device >= NR_DEVS) return(NIL_DEV);
  m_device = device;

  return(&m_geom[device]);
}

/*===========================================================================*
 *				r_transfer				     *
 *===========================================================================*/
PRIVATE int r_transfer(proc_nr, opcode, position, iov, nr_req)
int proc_nr;			/* process doing the request */
int opcode;			/* DEV_GATHER or DEV_SCATTER */
off_t position;			/* offset on device to read or write */
iovec_t *iov;			/* pointer to read or write request vector */
unsigned nr_req;		/* length of request vector */
{
/* Read or write one the driver's minor devices. */
  unsigned count, left, chunk;
  vir_bytes user_vir;
  struct device *dv;
  unsigned long dv_size;

  /* Get minor device number and check for /dev/null. */
  dv = &m_geom[m_device];
  dv_size = cv64ul(dv->dv_size);

  while (nr_req > 0) {

	/* How much to transfer and where to / from. */
	count = iov->iov_size;
	user_vir = iov->iov_addr;

	switch (m_device) {

	/* Random number generator. Character instead of block device. */
	case RANDOM_DEV:
	    if (opcode == DEV_GATHER && !random_isseeded())
		    return(EAGAIN);
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
 	        user_vir += chunk;
	    	left -= chunk;
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
 *				r_do_open				      *
 *============================================================================*/
PRIVATE int r_do_open(dp, m_ptr)
struct driver *dp;
message *m_ptr;
{
/* Check device number on open.  
 */
  if (r_prepare(m_ptr->DEVICE) == NIL_DEV) return(ENXIO);

  return(OK);
}

/*===========================================================================*
 *				r_init					     *
 *===========================================================================*/
PRIVATE void r_init()
{
  /* Initialize this task. All minor devices are initialized one by one. */
  random_init();
  r_random(NULL, NULL);				/* also set periodic timer */
}

/*===========================================================================*
 *				r_ioctl					     *
 *===========================================================================*/
PRIVATE int r_ioctl(dp, m_ptr)
struct driver *dp;			/* pointer to driver structure */
message *m_ptr;				/* pointer to control message */
{
  struct device *dv;
  if ((dv = r_prepare(m_ptr->DEVICE)) == NIL_DEV) return(ENXIO);

  switch (m_ptr->REQUEST) {

    default:
  	return(do_diocntl(&r_dtab, m_ptr));
  }
  return(OK);
}

/*============================================================================*
 *				r_random				      *
 *============================================================================*/
PRIVATE void r_random(dp, m_ptr)
struct driver *dp;			/* pointer to driver structure */
message *m_ptr;				/* pointer to alarm message */
{
  /* Fetch random information from the kernel to update /dev/random. */
  int i, s, r_next, r_size, r_high;
  struct randomness krandom;

  if (OK != (s=sys_getrandomness(&krandom)))
  	report("RANDOM", "sys_getrandomness failed", s);

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
  if (OK != (s=sys_setalarm(KRANDOM_PERIOD, 0)))
  	report("RANDOM", "sys_setalarm failed", s);
}

/*============================================================================*
 *				r_geometry				      *
 *============================================================================*/
PRIVATE void r_geometry(entry)
struct partition *entry;
{
  /* Memory devices don't have a geometry, but the outside world insists. */
  entry->cylinders = div64u(m_geom[m_device].dv_size, SECTOR_SIZE) / (64 * 32);
  entry->heads = 64;
  entry->sectors = 32;
}

