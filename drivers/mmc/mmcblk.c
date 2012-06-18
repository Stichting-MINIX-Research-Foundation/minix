/* This file contains a generic in memory block driver that will be hoocked to the mmc 
 * sub-system
 */
#include <minix/syslib.h>
#include <minix/driver.h>
#include <minix/blockdriver.h>

#include <stdio.h>
#include <stdlib.h>

#define DUMMY_SIZE_IN_BLOCKS 100
static char dummy_data[SECTOR_SIZE * DUMMY_SIZE_IN_BLOCKS];

/* Prototypes for the block device */
static int block_open(dev_t minor, int access);
static int block_close(dev_t minor);
static int block_transfer(dev_t minor, int do_write, u64_t position,
        endpoint_t endpt, iovec_t *iov, unsigned int nr_req, int flags);
static struct device *block_part(dev_t minor);

/* Entry points for the BLOCK driver. */
static struct blockdriver mmc_driver = {
  BLOCKDRIVER_TYPE_DISK,/* handle partition requests */
  block_open,         /* open or mount */
  block_close,        /* nothing on a close */
  block_transfer,     /* do the I/O */
  NULL,               /* No ioclt's */
  NULL,               /* no need to clean up (yet)*/
  block_part,         /* return partition information */
  NULL,               /* no geometry */
  NULL,               /* no interrupt processing */
  NULL,               /* no alarm processing */
  NULL,               /* no processing of other messages */
  NULL                /* no threading support */
};

int main(int argc, char **argv)
{
  /* SEF startup */
  sef_startup();
  blockdriver_task(&mmc_driver);

  return EXIT_SUCCESS;
};

/*===========================================================================*
 *                    block_open                                             *
 *===========================================================================*/
static int block_open(dev_t minor, int access)
{
  /* open on the memory device nothing special to do here */
  //TODO: increase open count */
  return OK;
}

/*===========================================================================*
 *                    block_close                                            *
 *===========================================================================*/
static int block_close(dev_t minor)
{
  /* handle close on the memory device nothing special to do here */
  //TODO: decrease the open count */
  return OK;
}

/*===========================================================================*
 *                    block_transfer                                         *
 *===========================================================================*/
static int block_transfer(
  dev_t minor,          /* minor device number */
  int do_write,         /* read or write? */
  u64_t position,       /* offset on device to read or write */
  endpoint_t endpt,     /* process doing the request */
  iovec_t *iov,         /* pointer to read or write request vector */
  unsigned int nr_req,  /* length of request vector */ 
  int flags             /* transfer flags */
  )
{
	u64_t size;
	/* determin the transfer size */

	return OK;
}
