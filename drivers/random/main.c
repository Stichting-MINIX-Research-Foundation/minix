/* This file contains the device dependent part of the drivers for the
 * following special files:
 *     /dev/random	- random number generator
 */

#include <minix/drivers.h>
#include <minix/chardriver.h>
#include <minix/type.h>

#include "assert.h"
#include "random.h"

#define NR_DEVS            1		/* number of minor devices */
#  define RANDOM_DEV  0			/* minor device for /dev/random */

#define KRANDOM_PERIOD    1 		/* ticks between krandom calls */

static struct device m_geom[NR_DEVS];  /* base and size of each device */
static dev_t m_device;			/* current device */

extern int errno;			/* error number for PM calls */

static struct device *r_prepare(dev_t device);
static int r_transfer(endpoint_t endpt, int opcode, u64_t position,
	iovec_t *iov, unsigned int nr_req, endpoint_t user_endpt, unsigned int
	flags);
static int r_do_open(message *m_ptr);
static void r_random(message *m_ptr);
static void r_updatebin(int source, struct k_randomness_bin *rb);

/* Entry points to this driver. */
static struct chardriver r_dtab = {
  r_do_open,	/* open or mount */
  do_nop,	/* nothing on a close */
  nop_ioctl,	/* no I/O controls supported */
  r_prepare,	/* prepare for I/O on a given minor device */
  r_transfer,	/* do the I/O */
  nop_cleanup,	/* no need to clean up */
  r_random, 	/* get randomness from kernel (alarm) */
  nop_cancel,	/* cancel not supported */
  nop_select,	/* select not supported */
  NULL,		/* other messages not supported */
};

/* Buffer for the /dev/random number generator. */
#define RANDOM_BUF_SIZE 		1024
static char random_buf[RANDOM_BUF_SIZE];

/* SEF functions and variables. */
static void sef_local_startup(void);
static int sef_cb_init_fresh(int type, sef_init_info_t *info);

/*===========================================================================*
 *				   main 				     *
 *===========================================================================*/
int main(void)
{
  /* SEF local startup. */
  sef_local_startup();

  /* Call the generic receive loop. */
  chardriver_task(&r_dtab, CHARDRIVER_ASYNC);

  return(OK);
}

/*===========================================================================*
 *			       sef_local_startup			     *
 *===========================================================================*/
static void sef_local_startup()
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
static int sef_cb_init_fresh(int UNUSED(type), sef_init_info_t *UNUSED(info))
{
/* Initialize the random driver. */
  static struct k_randomness krandom;
  int i, s;

  random_init();
  r_random(NULL);				/* also set periodic timer */

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
  chardriver_announce();

  return(OK);
}

/*===========================================================================*
 *				r_prepare				     *
 *===========================================================================*/
static struct device *r_prepare(dev_t device)
{
/* Prepare for I/O on a device: check if the minor device number is ok. */

  if (device >= NR_DEVS) return(NULL);
  m_device = device;

  return(&m_geom[device]);
}

/*===========================================================================*
 *				r_transfer				     *
 *===========================================================================*/
static int r_transfer(
  endpoint_t endpt,		/* endpoint of grant owner */
  int opcode,			/* DEV_GATHER or DEV_SCATTER */
  u64_t position,		/* offset on device to read or write */
  iovec_t *iov,			/* pointer to read or write request vector */
  unsigned int nr_req,		/* length of request vector */
  endpoint_t UNUSED(user_endpt),/* endpoint of user process */
  unsigned int UNUSED(flags)
)
{
/* Read or write one the driver's minor devices. */
  unsigned count, left, chunk;
  cp_grant_id_t grant;
  struct device *dv;
  int r;
  size_t vir_offset = 0;

  /* Get minor device number and check for /dev/null. */
  dv = &m_geom[m_device];

  while (nr_req > 0) {

	/* How much to transfer and where to / from. */
	count = iov->iov_size;
	grant = (cp_grant_id_t) iov->iov_addr;

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
		    r= sys_safecopyto(endpt, grant, vir_offset,
			(vir_bytes) random_buf, chunk);
		    if (r != OK)
		    {
			printf("random: sys_safecopyto failed for proc %d, "
				"grant %d\n", endpt, grant);
			return r;
		    }
 	        } else if (opcode == DEV_SCATTER_S) {
		    r= sys_safecopyfrom(endpt, grant, vir_offset,
			(vir_bytes) random_buf, chunk);
		    if (r != OK)
		    {
			printf("random: sys_safecopyfrom failed for proc %d, "
				"grant %d\n", endpt, grant);
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

/*===========================================================================*
 *				r_do_open				     *
 *===========================================================================*/
static int r_do_open(message *m_ptr)
{
/* Check device number on open.
 */
  if (r_prepare(m_ptr->DEVICE) == NULL) return(ENXIO);

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

/*===========================================================================*
 *				r_updatebin				     *
 *===========================================================================*/
static void r_updatebin(int source, struct k_randomness_bin *rb)
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

/*===========================================================================*
 *				r_random				     *
 *===========================================================================*/
static void r_random(message *UNUSED(m_ptr))
{
  /* Fetch random information from the kernel to update /dev/random. */
  int s;
  static int bin = 0;
  static struct k_randomness_bin krandom_bin;
  u32_t hi, lo;
  rand_t r;
  int nextperiod = random_isseeded() ? KRANDOM_PERIOD*500 : KRANDOM_PERIOD;

  bin = (bin+1) % RANDOM_SOURCES;

  if(sys_getrandom_bin(&krandom_bin, bin) == OK)
	r_updatebin(bin, &krandom_bin);

  /* Add our own timing source. */
  read_tsc(&hi, &lo);
  r = lo;
  random_update(RND_TIMING, &r, 1);

  /* Schedule new alarm for next m_random call. */
  if (OK != (s=sys_setalarm(nextperiod, 0)))
  	printf("RANDOM: sys_setalarm failed: %d\n", s);
}

