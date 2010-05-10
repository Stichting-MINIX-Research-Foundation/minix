/* This file contains the device dependent part of the drivers for the
 * following special files:
 *     /dev/random	- random number generator
 */

#include <minix/drivers.h>
#include <minix/driver.h>
#include <minix/type.h>

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
FORWARD _PROTOTYPE( int r_transfer, (int proc_nr, int opcode, u64_t position,
				iovec_t *iov, unsigned nr_req) );
FORWARD _PROTOTYPE( int r_do_open, (struct driver *dp, message *m_ptr) );
FORWARD _PROTOTYPE( int r_ioctl, (struct driver *dp, message *m_ptr) );
FORWARD _PROTOTYPE( void r_geometry, (struct partition *entry) );
FORWARD _PROTOTYPE( void r_random, (struct driver *dp, message *m_ptr) );
FORWARD _PROTOTYPE( void r_updatebin, (int source, struct k_randomness_bin *rb));

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
  r_random, 	/* get randomness from kernel (alarm) */
  nop_cancel,
  nop_select,
  NULL,
  NULL
};

/* Buffer for the /dev/random number generator. */
#define RANDOM_BUF_SIZE 		1024
PRIVATE char random_buf[RANDOM_BUF_SIZE];

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
  driver_task(&r_dtab, DRIVER_ASYN);

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
/* Initialize the random driver. */
  static struct k_randomness krandom;
  int i, s;

  random_init();
  r_random(NULL, NULL);				/* also set periodic timer */

  /* Retrieve first randomness buffer with parameters. */
  if (OK != (s=sys_getrandomness(&krandom))) {
  	printf("RANDOM: sys_getrandomness failed: %d\n", s);
	exit(1);
  }

  /* Do sanity check on parameters. */
  if(krandom.random_sources != RANDOM_SOURCES ||
     krandom.random_elements != RANDOM_ELEMENTS) {
     printf("random: parameters (%d, %d) don't match kernel's (%d, %d)\n",
	RANDOM_SOURCES, RANDOM_ELEMENTS,
	krandom.random_sources, krandom.random_elements);
     exit(1);
  }

  /* Feed initial batch. */
  for(i = 0; i < RANDOM_SOURCES; i++)
	r_updatebin(i, &krandom.bin[i]);

  /* Announce we are up! */
  driver_announce();

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

  if (device < 0 || device >= NR_DEVS) return(NULL);
  m_device = device;

  return(&m_geom[device]);
}

/*===========================================================================*
 *				r_transfer				     *
 *===========================================================================*/
PRIVATE int r_transfer(proc_nr, opcode, position, iov, nr_req)
int proc_nr;			/* process doing the request */
int opcode;			/* DEV_GATHER or DEV_SCATTER */
u64_t position;			/* offset on device to read or write */
iovec_t *iov;			/* pointer to read or write request vector */
unsigned nr_req;		/* length of request vector */
{
/* Read or write one the driver's minor devices. */
  unsigned count, left, chunk;
  vir_bytes user_vir;
  struct device *dv;
  int r;
  size_t vir_offset = 0;

  /* Get minor device number and check for /dev/null. */
  dv = &m_geom[m_device];

  while (nr_req > 0) {

	/* How much to transfer and where to / from. */
	count = iov->iov_size;
	user_vir = iov->iov_addr;

	switch (m_device) {

	/* Random number generator. Character instead of block device. */
	case RANDOM_DEV:
	    if (opcode == DEV_GATHER_S && !random_isseeded())
		    return(EAGAIN);
	    left = count;
	    while (left > 0) {
	    	chunk = (left > RANDOM_BUF_SIZE) ? RANDOM_BUF_SIZE : left;
 	        if (opcode == DEV_GATHER_S) {
		    random_getbytes(random_buf, chunk);
		    r= sys_safecopyto(proc_nr, user_vir, vir_offset,
			(vir_bytes) random_buf, chunk, D);
		    if (r != OK)
		    {
			printf(
			"random: sys_safecopyto failed for proc %d, grant %d\n",
				proc_nr, user_vir);
			return r;
		    }
 	        } else if (opcode == DEV_SCATTER_S) {
		    r= sys_safecopyfrom(proc_nr, user_vir, vir_offset,
			(vir_bytes) random_buf, chunk, D);
		    if (r != OK)
		    {
			printf(
		"random: sys_safecopyfrom failed for proc %d, grant %d\n",
				proc_nr, user_vir);
			return r;
		    }
	    	    random_putbytes(random_buf, chunk);
 	        }
 	        vir_offset += chunk;
	    	left -= chunk;
	    }
	    break;

	/* Unknown (illegal) minor device. */
	default:
	    return(EINVAL);
	}

	/* Book the number of bytes transferred. */
	position= add64u(position, count);
  	if ((iov->iov_size -= count) == 0) { iov++; nr_req--; vir_offset = 0; }

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
  if (r_prepare(m_ptr->DEVICE) == NULL) return(ENXIO);

  return(OK);
}

/*===========================================================================*
 *				r_ioctl					     *
 *===========================================================================*/
PRIVATE int r_ioctl(dp, m_ptr)
struct driver *dp;			/* pointer to driver structure */
message *m_ptr;				/* pointer to control message */
{
  if (r_prepare(m_ptr->DEVICE) == NULL) return(ENXIO);

  switch (m_ptr->REQUEST) {

    default:
  	return(do_diocntl(&r_dtab, m_ptr));
  }
  return(OK);
}

#define UPDATE(binnumber, bp, startitem, elems) 	{	\
		rand_t *r;					\
		int n = elems, item = startitem;\
		int high;					\
		assert(binnumber >= 0 && binnumber < RANDOM_SOURCES);	 \
		assert(item >= 0 && item < RANDOM_ELEMENTS);	\
		if(n > 0) {					\
			high = item+n-1;			\
			assert(high >= item);				\
			assert(high >= 0 && high < RANDOM_ELEMENTS);	\
			r = &bp->r_buf[item];		\
	  		random_update(binnumber, r, n);			\
		}							\
}

PRIVATE void r_updatebin(int source, struct k_randomness_bin *rb)
{
  	int r_next, r_size, r_high;

  	r_next= rb->r_next;
  	r_size= rb->r_size;

	assert(r_next >= 0 && r_next < RANDOM_ELEMENTS);
	assert(r_size >= 0 && r_size <= RANDOM_ELEMENTS);

  	r_high= r_next+r_size;

  	if (r_high <= RANDOM_ELEMENTS) {
		UPDATE(source, rb, r_next, r_size);
	} else {
		assert(r_next < RANDOM_ELEMENTS);
		UPDATE(source, rb, r_next, RANDOM_ELEMENTS-r_next);
		UPDATE(source, rb, 0, r_high-RANDOM_ELEMENTS);
	}

	return;
}

/*============================================================================*
 *				r_random				      *
 *============================================================================*/
PRIVATE void r_random(dp, m_ptr)
struct driver *dp;			/* pointer to driver structure */
message *m_ptr;				/* pointer to alarm message */
{
  /* Fetch random information from the kernel to update /dev/random. */
  int s;
  static int bin = 0;
  static struct k_randomness_bin krandom_bin;
  u32_t hi, lo;
  rand_t r;

  bin = (bin+1) % RANDOM_SOURCES;

  if(sys_getrandom_bin(&krandom_bin, bin) == OK)
	r_updatebin(bin, &krandom_bin);

  /* Add our own timing source. */
  read_tsc(&hi, &lo);
  r = lo;
  random_update(RND_TIMING, &r, 1);

  /* Schedule new alarm for next m_random call. */
  if (OK != (s=sys_setalarm(KRANDOM_PERIOD, 0)))
  	printf("RANDOM: sys_setalarm failed: %d\n", s);
}

/*============================================================================*
 *				r_geometry				      *
 *============================================================================*/
PRIVATE void r_geometry(struct partition *entry)
{
  /* Memory devices don't have a geometry, but the outside world insists. */
  entry->cylinders = div64u(m_geom[m_device].dv_size, SECTOR_SIZE) / (64 * 32);
  entry->heads = 64;
  entry->sectors = 32;
}

