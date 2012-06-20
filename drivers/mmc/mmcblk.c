/* This file contains a generic in memory block driver that will be hoocked to the mmc 
 * sub-system
 */
#include <minix/syslib.h>
#include <minix/driver.h>
#include <minix/blockdriver.h>

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#define DUMMY_SIZE_IN_BLOCKS 100
static char dummy_data[SECTOR_SIZE * DUMMY_SIZE_IN_BLOCKS];

static struct device device_geometry = {
	.dv_base = 0,
	.dv_size = SECTOR_SIZE * DUMMY_SIZE_IN_BLOCKS
};

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
  printf("Initializing the MMB block device\n");	
  blockdriver_task(&mmc_driver);

  return EXIT_SUCCESS;
};

/*===========================================================================*
 *                    block_open                                             *
 *===========================================================================*/
static int block_open(dev_t minor, int access)
{
  /* open on the memory device nothing special to do here */
  //TODO: increase open count ? */
  return OK;
}

/*===========================================================================*
 *                    block_close                                            *
 *===========================================================================*/
static int block_close(dev_t minor)
{
  /* handle close on the memory device nothing special to do here */
  //TODO: decrease the open count ? */
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
	//TODO: better understand the return value of this method
	//libblockdriver apparently expects either the number of
	//written/read byte or an error code so I don't understand 
	//the return OK;

	unsigned int counter;
	iovec_t *ciov;          /* Current IO Vector */
	struct device * dev;    /* The device used */
	vir_bytes io_size; 
	vir_bytes input_offset; 
	int r;

	/* get the current "device" geomerty */
	dev = block_part(minor);

	input_offset =0;

	if (position >  dev->dv_size) { return OK; }; /* Why is this OK??*/

	ciov = iov;
	for( counter = 0 ; counter < nr_req ; counter ++){
		printf("HELLO\n");	
		assert(ciov != NULL);
		io_size = ciov->iov_size;

		/* check we are not reading past the end */
		if (position + input_offset + io_size > dev->dv_size ) {
			io_size = dev->dv_size - position - input_offset;
		};
		if(do_write){
			/* @TODO understand why the thrid argument of safecopyto is not of pointer type */
			r=sys_safecopyto(endpt, ciov->iov_addr,0 /* offset */,(vir_bytes) dummy_data + input_offset ,io_size);
		} else {
			r=sys_safecopyfrom(endpt, ciov->iov_addr,0 /* offset */,(vir_bytes) dummy_data + input_offset,io_size);
		}
		if (r != OK){
			panic("I/O copy failed: %d", r);
		}
		ciov++;		
		input_offset += io_size;
	}
	return OK;
}

/*===========================================================================*
 *                    block_part                                             *
 *===========================================================================*/
static struct device *block_part(dev_t minor)
{
	return  &device_geometry;
}
