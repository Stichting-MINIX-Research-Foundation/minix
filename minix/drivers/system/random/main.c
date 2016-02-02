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
static ssize_t r_read(devminor_t minor, u64_t position, endpoint_t endpt,
	cp_grant_id_t grant, size_t size, int flags, cdev_id_t id);
static ssize_t r_write(devminor_t minor, u64_t position, endpoint_t endpt,
	cp_grant_id_t grant, size_t size, int flags, cdev_id_t id);
static int r_open(devminor_t minor, int access, endpoint_t user_endpt);
static void r_random(clock_t stamp);
static void r_updatebin(int source, struct k_randomness_bin *rb);
static int r_select(devminor_t, unsigned int, endpoint_t);

/* Entry points to this driver. */
static struct chardriver r_dtab = {
  .cdr_open	= r_open,	/* open device */
  .cdr_read	= r_read,	/* read from device */
  .cdr_write	= r_write,	/* write to device (seeding it) */
  .cdr_select	= r_select,	/* select hook */
  .cdr_alarm	= r_random 	/* get randomness from kernel (alarm) */
};

/* select requestor */
static endpoint_t random_select = NONE;

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
  chardriver_task(&r_dtab);

  return(OK);
}

/*===========================================================================*
 *			       sef_local_startup			     *
 *===========================================================================*/
static void sef_local_startup()
{
  /* Register init callbacks. */
  sef_setcb_init_fresh(sef_cb_init_fresh);
  sef_setcb_init_restart(sef_cb_init_fresh);

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
  r_random(0);				/* also set periodic timer */

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
 *				r_read					     *
 *===========================================================================*/
static ssize_t r_read(devminor_t minor, u64_t UNUSED(position),
	endpoint_t endpt, cp_grant_id_t grant, size_t size, int UNUSED(flags),
	cdev_id_t UNUSED(id))
{
/* Read from one of the driver's minor devices. */
  size_t offset, chunk;
  int r;

  if (minor != RANDOM_DEV) return(EIO);

  if (!random_isseeded()) return(EAGAIN);

  for (offset = 0; offset < size; offset += chunk) {
	chunk = MIN(size - offset, RANDOM_BUF_SIZE);
	random_getbytes(random_buf, chunk);
	r = sys_safecopyto(endpt, grant, offset, (vir_bytes)random_buf, chunk);
	if (r != OK) {
		printf("random: sys_safecopyto failed for proc %d, grant %d\n",
			endpt, grant);
		return r;
	}
  }

  return size;
}

/*===========================================================================*
 *				r_write					     *
 *===========================================================================*/
static ssize_t r_write(devminor_t minor, u64_t UNUSED(position),
	endpoint_t endpt, cp_grant_id_t grant, size_t size, int UNUSED(flags),
	cdev_id_t UNUSED(id))
{
/* Write to one of the driver's minor devices. */
  size_t offset, chunk;
  int r;

  if (minor != RANDOM_DEV) return(EIO);

  for (offset = 0; offset < size; offset += chunk) {
	chunk = MIN(size - offset, RANDOM_BUF_SIZE);
	r = sys_safecopyfrom(endpt, grant, offset, (vir_bytes)random_buf,
		chunk);
	if (r != OK) {
		printf("random: sys_safecopyfrom failed for proc %d,"
			" grant %d\n", endpt, grant);
		return r;
	}
	random_putbytes(random_buf, chunk);
  }

  return size;
}

/*===========================================================================*
 *				r_open					     *
 *===========================================================================*/
static int r_open(devminor_t minor, int access, endpoint_t UNUSED(user_endpt))
{
/* Check device number on open.
 */

  if (minor < 0 || minor >= NR_DEVS) return(ENXIO);

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
static void r_random(clock_t UNUSED(stamp))
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

/*===========================================================================*
 *				r_select				     *
 *===========================================================================*/
static int r_select(devminor_t minor, unsigned int ops, endpoint_t ep)
{
	/* random device is always writable; it's infinitely readable
	 * once seeded, and doesn't block when it's not, so all operations
	 * are instantly possible. we ignore CDEV_OP_ERR.
	 */
	int ready_ops = 0;
	if (minor != RANDOM_DEV) return(EIO);
	return ops & (CDEV_OP_RD | CDEV_OP_WR);
}
