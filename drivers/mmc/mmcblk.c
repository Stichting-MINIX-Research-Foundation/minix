/*
 * Block driver for Multi Media Cards (MMC).
 */
/* kernel headers */
#include <minix/syslib.h>
#include <minix/driver.h>
#include <minix/blockdriver.h>
#include <minix/drvlib.h>
#include <minix/minlib.h>

/* system headers */
#include <sys/ioc_disk.h>	/* disk IOCTL's */

/* usr headers */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <signal.h>

/* local headers */
#include "mmchost.h"
#include "mmclog.h"

/* used for logging */
static struct mmclog log = {
	.name = "mmc_block",
	.log_level = LEVEL_INFO,
	.log_func = default_log
};

/* holding the current host controller */
static struct mmc_host host;

#define SUB_PER_DRIVE           (NR_PARTITIONS * NR_PARTITIONS)
#define NR_SUBDEVS              (MAX_DRIVES * SUB_PER_DRIVE)

/* When passing data over a grant one needs to pass
 * a buffer to sys_safecopy copybuff is used for that*/
#define COPYBUFF_SIZE 0x1000	/* 4k buff */
static unsigned char copybuff[COPYBUFF_SIZE];

static struct sd_slot *get_slot(dev_t minor);

/* Prototypes for the block device */
static int block_open(dev_t minor, int access);
static int block_close(dev_t minor);
static int block_transfer(dev_t minor,
    int do_write,
    u64_t position,
    endpoint_t endpt, iovec_t * iov, unsigned int nr_req, int flags);

static int block_ioctl(dev_t minor,
    unsigned int request, endpoint_t endpt, cp_grant_id_t grant);
static struct device *block_part(dev_t minor);

/* System even handling */
static void sef_local_startup();
static int block_system_event_cb(int type, sef_init_info_t * info);
static void block_signal_handler_cb(int signo);

void
bdr_alarm(clock_t stamp)
{
	mmc_log_debug(&log, "alarm %d\n", stamp);

}

static int apply_env();
static void hw_intr(unsigned int irqs);

/* set the global logging level */
static void set_log_level(int level);

/* Entry points for the BLOCK driver. */
static struct blockdriver mmc_driver = {
	BLOCKDRIVER_TYPE_DISK,	/* handle partition requests */
	block_open,		/* open or mount */
	block_close,		/* on a close */
	block_transfer,		/* does the I/O */
	block_ioctl,		/* ioclt's */
	NULL,			/* no need to clean up (yet) */
	block_part,		/* return partition information */
	NULL,			/* no geometry */
	hw_intr,		/* left over interrupts */
	bdr_alarm,		/* no alarm processing */
	NULL,			/* no processing of other messages */
	NULL			/* no threading support */
};

static void
hw_intr(unsigned int irqs)
{
	mmc_log_debug(&log, "Hardware inter left over\n");
	host.hw_intr(irqs);
}

static int
apply_env()
{
	long v;
	/* apply the env setting passed to this driver parameters accepted
	 * log_level=[0-4] (NONE,WARN,INFO,DEBUG,TRACE) instance=[0-3]
	 * instance/bus number to use for this driver Passing these arguments
	 * is done when starting the driver using the service command in the
	 * following way service up /sbin/mmc -args "log_level=2 instance=1
	 * driver=dummy" -dev /dev/c2d0 */
	char driver[16];
	memset(driver, '\0', 16);
	(void) env_get_param("driver", driver, 16);
	if (strlen(driver) == 0
	    || strncmp(driver, "mmchs", strlen("mmchs") + 1) == 0) {
		/* early init of host mmc host controller. This code should
		 * depend on knowing the hardware that is running bellow. */
		host_initialize_host_structure_mmchs(&host);
	} else if (strncmp(driver, "dummy", strlen("dummy") + 1) == 0) {
		host_initialize_host_structure_dummy(&host);
	} else {
		mmc_log_warn(&log, "Unknown driver %s\n", driver);
	}
	/* Initialize the verbosity level. */
	v = 0;
	if (env_parse("log_level", "d", 0, &v, LEVEL_NONE,
		LEVEL_TRACE) == EP_SET) {
		set_log_level(v);
	}

	/* Find out which driver instance we are. */
	v = 0;
	env_parse("instance", "d", 0, &v, 0, 3);
	if (host.host_set_instance(&host, v)) {
		mmc_log_warn(&log, "Failed to set mmc instance to  %d\n", v);
		return -1;	/* NOT OK */
	}
	return OK;
}

;

/*===========================================================================*
 *                    block_open                                             *
 *===========================================================================*/
static int
block_open(dev_t minor, int access)
{
	struct sd_slot *slot;
	slot = get_slot(minor);
	int i, j;
	int part_count, sub_part_count;

	i = j = part_count = sub_part_count = 0;

	if (!slot) {
		mmc_log_debug(&log,
		    "Not handling open on non existing slot\n");
		return EIO;
	}

	assert(slot->host != NULL);

	if (!slot->host->card_detect(slot)) {
		mmc_log_debug(&log, "No card inserted in the SD slot\n");
		return EIO;
	}

	/* If we are already open just increase the open count and return */
	if (slot->card.state == SD_MODE_DATA_TRANSFER_MODE) {
		assert(slot->card.open_ct >= 0);
		slot->card.open_ct++;
		mmc_log_trace(&log, "increased open count to %d\n",
		    slot->card.open_ct);
		return OK;
	}

	/* We did not have an sd-card inserted so we are going to probe for it 
	 */
	mmc_log_debug(&log, "First open on (%d)\n", minor);
	if (!host.card_initialize(slot)) {
		// * TODO: set card state to INVALID until removed? */
		return EIO;
	}

	partition(&mmc_driver, 0 /* first card on bus */ , P_PRIMARY,
	    0 /* atapi device?? */ );

	mmc_log_trace(&log, "descr \toffset(bytes)      size(bytes)\n", minor);

	mmc_log_trace(&log, "disk %d\t0x%016llx 0x%016llx\n", i,
	    slot->card.part[0].dv_base, slot->card.part[0].dv_size);
	for (i = 1; i < 5; i++) {
		if (slot->card.part[i].dv_size == 0)
			continue;
		part_count++;
		mmc_log_trace(&log, "part %d\t0x%016llx 0x%016llx\n", i,
		    slot->card.part[i].dv_base, slot->card.part[i].dv_size);
		for (j = 0; j < 4; j++) {
			if (slot->card.subpart[(i - 1) * 4 + j].dv_size == 0)
				continue;
			sub_part_count++;
			mmc_log_trace(&log,
			    " sub %d/%d\t0x%016llx 0x%016llx\n", i, j,
			    slot->card.subpart[(i - 1) * 4 + j].dv_base,
			    slot->card.subpart[(i - 1) * 4 + j].dv_size);
		}
	}
	mmc_log_debug(&log, "Found %d partitions and %d sub partitions\n",
	    part_count, sub_part_count);
	slot->card.open_ct++;
	assert(slot->card.open_ct == 1);
	return OK;
}

/*===========================================================================*
 *                    block_close                                            *
 *===========================================================================*/
static int
block_close(dev_t minor)
{
	struct sd_slot *slot;

	slot = get_slot(minor);
	if (!slot) {
		mmc_log_debug(&log,
		    "Not handling open on non existing slot\n");
		return EIO;
	}

	/* if we arrived here we expect a card to be present, we will need do
	 * deal with removal later */
	assert(slot->host != NULL);
	assert(slot->card.open_ct >= 1);

	/* If this is not the last open count simply decrease the counter and
	 * return */
	if (slot->card.open_ct > 1) {
		slot->card.open_ct--;
		mmc_log_trace(&log, "decreased open count to %d\n",
		    slot->card.open_ct);
		return OK;
	}

	assert(slot->card.open_ct == 1);
	mmc_log_debug(&log,
	    "freeing the block device as it is no longer used\n");

	/* release the card as check the open_ct should be 0 */
	slot->host->card_release(&slot->card);
	assert(slot->card.open_ct == 0);
	return OK;
}

static int
copyto(endpoint_t dst_e,
    cp_grant_id_t gr_id, vir_bytes offset, vir_bytes address, size_t bytes)
{
	/* Helper function that used memcpy to copy data when the endpoint ==
	 * SELF */
	if (dst_e == SELF) {
		memcpy((char *) gr_id + offset, (char *) address, bytes);
		return OK;
	} else {
		/* Read io_size bytes from our data at the correct * offset
		 * and write it to the output buffer at 0 */
		return sys_safecopyto(dst_e, gr_id, offset, address, bytes);
	}
}

static int
copyfrom(endpoint_t src_e,
    cp_grant_id_t gr_id, vir_bytes offset, vir_bytes address, size_t bytes)
{
	/* Helper function that used memcpy to copy data when the endpoint ==
	 * SELF */
	if (src_e == SELF) {
		memcpy((char *) address, (char *) gr_id + offset, bytes);
		return OK;
	} else {
		return sys_safecopyfrom(src_e, gr_id, offset, address, bytes);
	}
}

/*===========================================================================*
 *                    block_transfer                                         *
 *===========================================================================*/
static int
block_transfer(dev_t minor,	/* minor device number */
    int do_write,		/* read or write? */
    u64_t position,		/* offset on device to read or write */
    endpoint_t endpt,		/* process doing the request */
    iovec_t * iov,		/* pointer to read or write request vector */
    unsigned int nr_req,	/* length of request vector */
    int flags			/* transfer flags */
    )
{
	unsigned long counter;
	iovec_t *ciov;		/* Current IO Vector */
	struct device *dev;	/* The device used */
	struct sd_slot *slot;	/* The sd slot the requests is pointed to */
	vir_bytes io_size;	/* Size to read/write to/from the iov */
	vir_bytes io_offset;	/* Size to read/write to/from the iov */
	vir_bytes bytes_written;

	int r, blk_size, i;

	/* Get the current "device" geometry */
	dev = block_part(minor);
	if (dev == NULL) {
		mmc_log_warn(&log,
		    "Transfer requested on unknown device minor(%d)\n", minor);
		/* Unknown device */
		return ENXIO;
	}
	mmc_log_trace(&log, "I/O on minor(%d) %s at 0x%016llx\n", minor,
	    (do_write) ? "Write" : "Read", position);

	slot = get_slot(minor);
	assert(slot);

	if (slot->card.blk_size == 0) {
		mmc_log_warn(&log, "Request on a card with block size of 0\n");
		return EINVAL;
	}
	if (slot->card.blk_size > COPYBUFF_SIZE) {
		mmc_log_warn(&log,
		    "Card block size (%d) exceeds internal buffer size %d\n",
		    slot->card.blk_size, COPYBUFF_SIZE);
		return EINVAL;
	}

	/* It is fully up to the driver to decide on restrictions for the
	 * parameters of transfers, in those cases we return EINVAL */
	if (position % slot->card.blk_size != 0) {
		/* Starting at a block boundary */
		mmc_log_warn(&log,
		    "Requests must start at a block boundary"
		    "(start,block size)=(%016llx,%08x)\n", position,
		    slot->card.blk_size);
		return EINVAL;
	}

	blk_size = slot->card.blk_size;

	bytes_written = 0;

	/* Are we trying to start reading past the end */
	if (position >= dev->dv_size) {
		mmc_log_warn(&log, "start reading past drive size\n");
		return 0;
	};

	ciov = iov;
	/* do some more validation */
	for (counter = 0; counter < nr_req; counter++) {
		assert(ciov != NULL);
		if (ciov->iov_size % blk_size != 0) {
			/* transfer a multiple of blk_size */
			mmc_log_warn(&log,
			    "Requests must start at a block boundary "
			    "(start,block size)=(%016llx,%08x)\n", position,
			    slot->card.blk_size);
			return EINVAL;
		}

		if (ciov->iov_size <= 0) {
			mmc_log_warn(&log,
			    "Invalid iov size for iov %d of %d size\n",
			    counter, nr_req, ciov->iov_size);
			return EINVAL;
		}
		ciov++;
	}

	ciov = iov;
	for (counter = 0; counter < nr_req; counter++) {
		/* Assume we are to transfer the amount of data given in the
		 * input/output vector but ensure we are not doing i/o past
		 * our own boundaries */
		io_size = ciov->iov_size;
		io_offset = position + bytes_written;

		/* Check we are not reading/writing past the end */
		if (position + bytes_written + io_size > dev->dv_size) {
			io_size = dev->dv_size - (position + bytes_written);
		};

		mmc_log_trace(&log,
		    "I/O %s request(%d/%d) iov(grant,size,iosize,"
		    "offset)=(%d,%d,%d,%d)\n",
		    (do_write) ? "write" : "read", counter + 1, nr_req,
		    ciov->iov_addr, ciov->iov_size, io_size, io_offset);
		/* transfer max one block at the time */
		for (i = 0; i < io_size / blk_size; i++) {
			if (do_write) {
				/* Read io_size bytes from i/o vector starting
				 * at 0 and write it to out buffer at the
				 * correct offset */
				r = copyfrom(endpt, ciov->iov_addr,
				    i * blk_size, (vir_bytes) copybuff,
				    blk_size);
				if (r != OK) {
					mmc_log_warn(&log,
					    "I/O write error: %s iov(base,size)=(%d,%d)"
					    " at offset=%d\n",
					    strerror(_SIGN r), ciov->iov_addr,
					    ciov->iov_size, io_offset);
					return EINVAL;
				}

				/* write a single block */
				slot->host->write(&slot->card,
				    (dev->dv_base / blk_size) +
				    (io_offset / blk_size) + i, 1, copybuff);
				bytes_written += blk_size;
			} else {
				/* read a single block info copybuff */
				slot->host->read(&slot->card,
				    (dev->dv_base / blk_size) +
				    (io_offset / blk_size) + i, 1, copybuff);
				/* Read io_size bytes from our data at the
				 * correct offset and write it to the output
				 * buffer at 0 */
				r = copyto(endpt, ciov->iov_addr, i * blk_size,
				    (vir_bytes) copybuff, blk_size);
				if (r != OK) {
					mmc_log_warn(&log,
					    "I/O read error: %s iov(base,size)=(%d,%d)"
					    " at offset=%d\n",
					    strerror(_SIGN r), ciov->iov_addr,
					    ciov->iov_size, io_offset);
					return EINVAL;
				}
				bytes_written += blk_size;
			}
		}
		ciov++;
	}
	return bytes_written;
}

/*===========================================================================*
 *				block_ioctl		                     *
 *===========================================================================*/
static int
block_ioctl(dev_t minor,
    unsigned int request, endpoint_t endpt, cp_grant_id_t grant)
{
	/* IOCTL handling */
	struct sd_slot *slot;
	mmc_log_trace(&log,
	    "enter (minor,request,endpoint,grant)=(%d,%lu,%d)\n", minor,
	    request, endpt, grant);

	slot = get_slot(minor);
	if (!slot) {
		mmc_log_warn(&log,
		    "Doing ioctl on non existing block device(%d)\n", minor);
		return EINVAL;
	}

	switch (request) {
	case DIOCOPENCT:
		// TODO: add a check for card validity */
		mmc_log_trace(&log, "returning open count %d\n",
		    slot->card.open_ct);
		/* return the current open count */
		return sys_safecopyto(endpt, grant, 0,
		    (vir_bytes) & slot->card.open_ct,
		    sizeof(slot->card.open_ct));
	case DIOCFLUSH:
		/* No need to flush but some devices like movinands require
		 * 500 ms inactivity */
		return OK;
	}

	return EINVAL;
}

/*===========================================================================*
 *                    block_part                                             *
 *===========================================================================*/
static struct device *
block_part(dev_t minor)
{
	/* 
	 * Reuse the existing MINIX major/minor partitioning scheme.
	 * - 8 drives
	 * - 5 devices per drive allowing direct access to the disk and up to 4
	 *   partitions (IBM style partitioning without extended partitions)
	 * - 4 Minix style sub partitions per partitions
	 */
	struct device *dev;
	struct sd_slot *slot;

	dev = NULL;
	slot = get_slot(minor);
	if (!slot) {
		mmc_log_warn(&log,
		    "Device information requested for non existing partition "
		    "minor(%d)\n", minor);
		return NULL;
	}

	if (!slot->host->card_detect(slot)) {
		mmc_log_warn(&log,
		    "Device information requested from empty slot(%d)\n",
		    minor);
		return NULL;
	}

	if (minor < 5) {
		/* we are talking about the first disk */
		dev = &slot->card.part[minor];
		mmc_log_trace(&log,
		    "returning partition(%d) (base,size)=(0x%016llx,0x%016llx)\n",
		    minor, dev->dv_base, dev->dv_size);
	} else if (minor >= 128 && minor < 128 + 16) {
		/* sub partitions of the first disk we don't care about the
		 * rest */
		dev = &slot->card.subpart[minor - 128];
		mmc_log_trace(&log,
		    "returning sub partition(%d) (base,size)=(0x%016llx,0x%016llx)\n",
		    minor - 128, dev->dv_base, dev->dv_size);

	} else {
		mmc_log_warn(&log,
		    "Device information requested for non existing "
		    "partition minor(%d)\n", minor);
	}
	return dev;
}

/*===========================================================================*
 *                         sef_local_startup                                 *
 *===========================================================================*/
static void
sef_local_startup()
{
	mmc_log_info(&log, "Initializing the MMC block device\n");
	if (apply_env()) {
		mmc_log_warn(&log,
		    "Failed while applying environment settings\n");
		exit(EXIT_FAILURE);
	}

	if (host.host_init(&host)) {
		mmc_log_warn(&log,
		    "Failed to initialize the host controller\n");
		exit(EXIT_FAILURE);
	}
	/* 
	 * Register callbacks for fresh start, live update and restart.
	 *  Use the same function for all event types
	 */
	sef_setcb_init_fresh(block_system_event_cb);
	sef_setcb_init_lu(block_system_event_cb);

	/* Register a signal handler */
	sef_setcb_signal_handler(block_signal_handler_cb);

	/* SEF startup */
	sef_startup();
}

/*===========================================================================*
 *                         block_system_event_cb                             *
 *===========================================================================*/
static int
block_system_event_cb(int type, sef_init_info_t * info)
{
	/* 
	 * Callbacks for the System event framework as registered in 
	 * sef_local_startup */
	switch (type) {
	case SEF_INIT_FRESH:
		mmc_log_info(&log, "System event framework fresh start\n");
		break;

	case SEF_INIT_LU:
		/* Restore the state. post update */
		mmc_log_info(&log, "System event framework live update\n");
		break;

	case SEF_INIT_RESTART:
		mmc_log_info(&log, "System event framework post restart\n");
		break;
	}
	blockdriver_announce(type);
	return OK;
}

/*===========================================================================*
 *                         block_signal_handler_cb                           *
 *===========================================================================*/
static void
block_signal_handler_cb(int signo)
{
	struct sd_slot *slot;

	mmc_log_debug(&log, "System event framework signal handler sig(%d)\n",
	    signo);
	/* Only check for termination signal, ignore anything else. */
	if (signo != SIGTERM)
		return;

	/* we only have a single slot and need an open count idealy we should
	 * iterate over the card to determine the open count */
	slot = get_slot(0);
	assert(slot);
	if (slot->card.open_ct > 0) {
		mmc_log_debug(&log, "Not responding to SIGTERM (open count=%d)\n",
		    slot->card.open_ct);
		return;
	}

	mmc_log_info(&log, "MMC driver exit");
	exit(0);
}

#define IS_MINIX_SUB_PARTITION_MINOR(minor) (minor >= MINOR_d0p0s0 )

static struct sd_slot *
get_slot(dev_t minor)
{
	/* 
	 * Get an sd_slot based on the minor number.
	 *
	 * This driver only supports a single card at at time. Also as
	 * we are following the major/minor scheme of other driver we
	 * must return a slot for all minors on disk 0 these are  0-5
	 * for the disk and 4 main partitions and
	 * number 128 till 144 for sub partitions.
	 */
	/* If this is a minor for the first disk (e.g. minor 0 till 5) */
	if (minor / DEV_PER_DRIVE == 0) {
		/* we are talking about the first disk and that is all we
		 * support */
		return &host.slot[0];
	} else if (IS_MINIX_SUB_PARTITION_MINOR(minor)
	    && (((minor - MINOR_d0p0s0) / SUB_PER_DRIVE) == 0)) {
		/* a minor from the first disk */
		return &host.slot[0];
	} else {
		mmc_log_trace(&log,
		    "Device information requested for non existing partition "
		    "minor(%d)\n", minor);
		return NULL;
	}
}

static void
set_log_level(int level)
{
	if (level < 0 || level >= 4) {
		return;
	}
	mmc_log_info(&log, "Setting verbosity level to %d\n", level);
	log.log_level = level;
	if (host.set_log_level) {
		host.set_log_level(level);
	}
}

int
main(int argc, char **argv)
{

	/* Set and apply the environment */
	env_setargs(argc, argv);
	sef_local_startup();
	blockdriver_task(&mmc_driver);
	return EXIT_SUCCESS;
}
