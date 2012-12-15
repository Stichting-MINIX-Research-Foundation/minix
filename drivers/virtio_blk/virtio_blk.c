/*
 * virtio block driver for MINIX 3
 *
 * Copyright (c) 2013, A. Welzel, <arne.welzel@gmail.com>
 *
 * This software is released under the BSD license. See the LICENSE file
 * included in the main directory of this source distribution for the
 * license terms and conditions.
 */

#include <assert.h>

#include <minix/drivers.h>
#include <minix/blockdriver_mt.h>
#include <minix/drvlib.h>
#include <minix/virtio.h>
#include <minix/sysutil.h>

#include <sys/ioc_disk.h>

#include "virtio_blk.h"

#define mystatus(tid)  (status_vir[(tid)] & 0xFF)

#define dprintf(s) do {						\
	printf("%s: ", name);					\
	printf s;						\
	printf("\n");						\
} while (0)

/* Number of threads to use */
#define VIRTIO_BLK_NUM_THREADS		4

/* virtio-blk blocksize is always 512 bytes */
#define VIRTIO_BLK_BLOCK_SIZE		512

static const char *const name = "virtio-blk";

/* static device handle */
static struct virtio_device *blk_dev;

static struct virtio_blk_config blk_config;

struct virtio_feature blkf[] = {
	{ "barrier",	VIRTIO_BLK_F_BARRIER,	0,	0	},
	{ "sizemax",	VIRTIO_BLK_F_SIZE_MAX,	0,	0	},
	{ "segmax",	VIRTIO_BLK_F_SEG_MAX,	0,	0	},
	{ "geometry",	VIRTIO_BLK_F_GEOMETRY,	0,	0	},
	{ "read-only",	VIRTIO_BLK_F_RO,	0,	0	},
	{ "blocksize",	VIRTIO_BLK_F_BLK_SIZE,	0,	0	},
	{ "scsi",	VIRTIO_BLK_F_SCSI,	0,	0	},
	{ "flush",	VIRTIO_BLK_F_FLUSH,	0,	0	},
	{ "topology",	VIRTIO_BLK_F_TOPOLOGY,	0,	0	},
	{ "idbytes",	VIRTIO_BLK_ID_BYTES,	0,	0	}
};

/* State information */
static int spurious_interrupt = 0;
static int terminating = 0;
static int open_count = 0;

/* Partition magic */
#define VIRTIO_BLK_SUB_PER_DRIVE	(NR_PARTITIONS * NR_PARTITIONS)
struct device part[DEV_PER_DRIVE];
struct device subpart[VIRTIO_BLK_SUB_PER_DRIVE];

/* Headers for requests */
static struct virtio_blk_outhdr *hdrs_vir;
static phys_bytes hdrs_phys;

/* Status bytes for requests.
 *
 * Usually a status is only one byte in length, but we need the lowest bit
 * to propagate writable. For this reason we take u16_t and use a mask for
 * the lower byte later.
 */
static u16_t *status_vir;
static phys_bytes status_phys;

/* Prototypes */
static int virtio_blk_open(dev_t minor, int access);
static int virtio_blk_close(dev_t minor);
static ssize_t virtio_blk_transfer(dev_t minor, int write, u64_t position,
				   endpoint_t endpt, iovec_t *iovec,
				   unsigned int cnt, int flags);
static int virtio_blk_ioctl(dev_t minor, unsigned int req, endpoint_t endpt,
			    cp_grant_id_t grant);
static struct device * virtio_blk_part(dev_t minor);
static void virtio_blk_geometry(dev_t minor, struct partition *entry);
static void virtio_blk_device_intr(void);
static void virtio_blk_spurious_intr(void);
static void virtio_blk_intr(unsigned int irqs);
static int virtio_blk_device(dev_t minor, device_id_t *id);

static int virtio_blk_flush(void);
static void virtio_blk_terminate(void);
static void virtio_blk_cleanup(void);
static int virtio_blk_status2error(u8_t status);
static int virtio_blk_alloc_requests(void);
static void virtio_blk_free_requests(void);
static int virtio_blk_feature_setup(void);
static int virtio_blk_config(void);
static int virtio_blk_probe(int skip);

/* libblockdriver driver tab */
static struct blockdriver virtio_blk_dtab  = {
	BLOCKDRIVER_TYPE_DISK,
	virtio_blk_open,
	virtio_blk_close,
	virtio_blk_transfer,
	virtio_blk_ioctl,
	NULL,		/* bdr_cleanup */
	virtio_blk_part,
	virtio_blk_geometry,
	virtio_blk_intr,
	NULL,		/* bdr_alarm */
	NULL,		/* bdr_other */
	virtio_blk_device
};

static int
virtio_blk_open(dev_t minor, int access)
{
	struct device *dev = virtio_blk_part(minor);

	/* Check if this device exists */
	if (!dev)
		return ENXIO;

	/* Read only devices should only be mounted... read-only */
	if ((access & W_BIT) && virtio_host_supports(blk_dev, VIRTIO_BLK_F_RO))
		return EACCES;

	/* Partition magic when opened the first time or re-opened after
	 * being fully closed
	 */
	if (open_count == 0) {
		memset(part, 0, sizeof(part));
		memset(subpart, 0, sizeof(subpart));
		part[0].dv_size = blk_config.capacity * VIRTIO_BLK_BLOCK_SIZE;
		partition(&virtio_blk_dtab, 0, P_PRIMARY, 0 /* ATAPI */);
		blockdriver_mt_set_workers(0, VIRTIO_BLK_NUM_THREADS);
	}

	open_count++;
	return OK;
}

static int
virtio_blk_close(dev_t minor)
{
	struct device *dev = virtio_blk_part(minor);

	/* Check if this device exists */
	if (!dev)
		return ENXIO;

	if (open_count == 0) {
		dprintf(("Closing one too many times?"));
		return EINVAL;
	}

	open_count--;

	/* If fully closed, flush the device and set workes to 1 */
	if (open_count == 0) {
		virtio_blk_flush();
		blockdriver_mt_set_workers(0, 1);
	}

	/* If supposed to terminate and fully closed, do it! */
	if (terminating && open_count == 0)
		virtio_blk_terminate();

	return OK;
}

static int
prepare_bufs(struct vumap_vir *vir, struct vumap_phys *phys, int cnt, int w)
{
	for (int i = 0; i < cnt ; i++) {

		/* So you gave us a byte aligned buffer? Good job! */
		if (phys[i].vp_addr & 1) {
			dprintf(("byte aligned %08lx", phys[i].vp_addr));
			return EINVAL;
		}

		/* Check if the buffer is good */
		if (phys[i].vp_size != vir[i].vv_size) {
			dprintf(("Non-contig buf %08lx", phys[i].vp_addr));
			return EINVAL;
		}

		/* If write, the buffers only need to be read */
		phys[i].vp_addr |= !w;
	}

	return OK;
}

static int
prepare_vir_vec(endpoint_t endpt, struct vumap_vir *vir, iovec_s_t *iv,
		int cnt, vir_bytes *size)
{
	/* This is pretty much the same as sum_iovec from AHCI,
	 * except that we don't support any iovecs where the size
	 * is not a multiple of 512
	 */
	vir_bytes s, total = 0;
	for (int i = 0; i < cnt; i++) {
		s = iv[i].iov_size;

		if (s == 0 || (s % VIRTIO_BLK_BLOCK_SIZE) || s > LONG_MAX) {
			dprintf(("bad iv[%d].iov_size (%lu) from %d", i, s,
								      endpt));
			return EINVAL;
		}

		total += s;

		if (total > LONG_MAX) {
			dprintf(("total overflow from %d", endpt));
			return EINVAL;
		}

		if (endpt == SELF)
			vir[i].vv_addr = (vir_bytes)iv[i].iov_grant;
		else
			vir[i].vv_grant = iv[i].iov_grant;

		vir[i].vv_size = iv[i].iov_size;

	}

	*size = total;
	return OK;
}

static ssize_t
virtio_blk_transfer(dev_t minor, int write, u64_t position, endpoint_t endpt,
		    iovec_t *iovec, unsigned int cnt, int flags)
{
	/* Need to translate vir to phys */
	struct vumap_vir vir[NR_IOREQS];

	/* Physical addresses of buffers, including header and trailer */
	struct vumap_phys phys[NR_IOREQS + 2];

	/* Which thread is doing the transfer? */
	thread_id_t tid = blockdriver_mt_get_tid();

	vir_bytes size = 0;
	vir_bytes size_tmp = 0;
	struct device *dv;
	u64_t sector;
	u64_t end_part;
	int r, pcnt = sizeof(phys) / sizeof(phys[0]);

	iovec_s_t *iv = (iovec_s_t *)iovec;
	int access = write ? VUA_READ : VUA_WRITE;

	/* Make sure we don't touch this one anymore */
	iovec = NULL;

	if (cnt > NR_IOREQS)
		return EINVAL;

	/* position greater than capacity? */
	if (position >= blk_config.capacity * VIRTIO_BLK_BLOCK_SIZE)
		return 0;

	dv = virtio_blk_part(minor);

	/* Does device exist? */
	if (!dv)
		return ENXIO;

	position += dv->dv_base;
	end_part = dv->dv_base + dv->dv_size;

	/* Hmmm, AHCI tries to fix this up, but lets just say everything
	 * needs to be sector (512 byte) aligned...
	 */
	if (position % VIRTIO_BLK_BLOCK_SIZE) {
		dprintf(("Non sector-aligned access %016llx", position));
		return EINVAL;
	}

	sector = position / VIRTIO_BLK_BLOCK_SIZE;

	r = prepare_vir_vec(endpt, vir, iv, cnt, &size);

	if (r != OK)
		return r;

	if (position >= end_part)
		return 0;

	/* Truncate if the partition is smaller than that */
	if (position + size > end_part - 1) {
		size = end_part - position;

		/* Fix up later */
		size_tmp = 0;
		cnt = 0;
	} else {
		/* Use all buffers */
		size_tmp = size;
	}

	/* Fix up the number of vectors if size was truncated */
	while (size_tmp < size)
		size_tmp += vir[cnt++].vv_size;

	/* If the last vector was too big, just truncate it */
	if (size_tmp > size) {
		vir[cnt - 1].vv_size = vir[cnt -1].vv_size - (size_tmp - size);
		size_tmp -= (size_tmp - size);
	}

	if (size % VIRTIO_BLK_BLOCK_SIZE) {
		dprintf(("non-sector sized read (%lu) from %d", size, endpt));
		return EINVAL;
	}

	/* Map vir to phys */
	if ((r = sys_vumap(endpt, vir, cnt, 0, access,
			   &phys[1], &pcnt)) != OK) {

		dprintf(("Unable to map memory from %d (%d)", endpt, r));
		return r;
	}

	/* Prepare the header */
	memset(&hdrs_vir[tid], 0, sizeof(hdrs_vir[0]));

	if (write)
		hdrs_vir[tid].type = VIRTIO_BLK_T_OUT;
	else
		hdrs_vir[tid].type = VIRTIO_BLK_T_IN;

	hdrs_vir[tid].ioprio = 0;
	hdrs_vir[tid].sector = sector;

	/* First the header */
	phys[0].vp_addr = hdrs_phys + tid * sizeof(hdrs_vir[0]);
	phys[0].vp_size = sizeof(hdrs_vir[0]);

	/* Put the physical buffers into phys */
	if ((r = prepare_bufs(vir, &phys[1], pcnt, write)) != OK)
		return r;

	/* Put the status at the end */
	phys[pcnt + 1].vp_addr = status_phys + tid * sizeof(status_vir[0]);
	phys[pcnt + 1].vp_size = sizeof(u8_t);

	/* Status always needs write access */
	phys[1 + pcnt].vp_addr |= 1;

	/* Send addresses to queue */
	virtio_to_queue(blk_dev, 0, phys, 2 + pcnt, &tid);

	/* Wait for completion */
	blockdriver_mt_sleep();

	/* All was good */
	if (mystatus(tid) == VIRTIO_BLK_S_OK)
		return size;

	/* Error path */
	dprintf(("ERROR status=%02x sector=%llu len=%lx cnt=%d op=%s t=%d",
		 mystatus(tid), sector, size, pcnt,
		 write ? "write" : "read", tid));

	return virtio_blk_status2error(mystatus(tid));
}

static int
virtio_blk_ioctl(dev_t minor, unsigned int req, endpoint_t endpt,
		 cp_grant_id_t grant)
{
	switch (req) {

	case DIOCOPENCT:
		return sys_safecopyto(endpt, grant, 0,
			(vir_bytes) &open_count, sizeof(open_count));

	case DIOCFLUSH:
		return virtio_blk_flush();

	}

	return EINVAL;
}

static struct device *
virtio_blk_part(dev_t minor)
{
	/* There's only a single drive attached to this device, alyways.
	 * Lets take some shortcuts...
	 */

	/* Take care of d0 d0p0 ... */
	if (minor < 5)
		return &part[minor];

	/* subparts start at 128 */
	if (minor >= 128) {

		/* Mask away upper bits */
		minor = minor & 0x7F;

		/* Only for the first disk */
		if (minor > 15)
			return NULL;

		return &subpart[minor];
	}

	return NULL;
}

static void
virtio_blk_geometry(dev_t minor, struct partition *entry)
{
	/* Only for the drive */
	if (minor != 0)
		return;

	/* Only if the host supports it */
	if(!virtio_host_supports(blk_dev, VIRTIO_BLK_F_GEOMETRY))
		return;

	entry->cylinders = blk_config.geometry.cylinders;
	entry->heads = blk_config.geometry.heads;
	entry->sectors = blk_config.geometry.sectors;
}

static void
virtio_blk_device_intr(void)
{
	thread_id_t *tid;

	/* Multiple requests might have finished */
	while (!virtio_from_queue(blk_dev, 0, (void**)&tid))
		blockdriver_mt_wakeup(*tid);
}

static void
virtio_blk_spurious_intr(void)
{
	/* Output a single message about spurious interrupts */
	if (spurious_interrupt)
		return;

	dprintf(("Got spurious interrupt"));
	spurious_interrupt = 1;
}

static void
virtio_blk_intr(unsigned int irqs)
{

	if (virtio_had_irq(blk_dev))
		virtio_blk_device_intr();
	else
		virtio_blk_spurious_intr();

	virtio_irq_enable(blk_dev);
}

static int
virtio_blk_device(dev_t minor, device_id_t *id)
{
	struct device *dev = virtio_blk_part(minor);

	/* Check if this device exists */
	if (!dev)
		return ENXIO;

	*id = 0;
	return OK;
}

static int
virtio_blk_flush(void)
{
	struct vumap_phys phys[2];
	size_t phys_cnt = sizeof(phys) / sizeof(phys[0]);

	/* Which thread is doing this request? */
	thread_id_t tid = blockdriver_mt_get_tid();

	/* Host may not support flushing */
	if (!virtio_host_supports(blk_dev, VIRTIO_BLK_F_FLUSH))
		return EOPNOTSUPP;

	/* Prepare the header */
	memset(&hdrs_vir[tid], 0, sizeof(hdrs_vir[0]));
	hdrs_vir[tid].type = VIRTIO_BLK_T_FLUSH;

	/* Let this be a barrier if the host supports it */
	if (virtio_host_supports(blk_dev, VIRTIO_BLK_F_BARRIER))
		hdrs_vir[tid].type |= VIRTIO_BLK_T_BARRIER;

	/* Header and status for the queue */
	phys[0].vp_addr = hdrs_phys + tid * sizeof(hdrs_vir[0]);
	phys[0].vp_size = sizeof(hdrs_vir[0]);
	phys[1].vp_addr = status_phys + tid * sizeof(status_vir[0]);
	phys[1].vp_size = 1;

	/* Status always needs write access */
	phys[1].vp_addr |= 1;

	/* Send flush request to queue */
	virtio_to_queue(blk_dev, 0, phys, phys_cnt, &tid);

	blockdriver_mt_sleep();

	/* All was good */
	if (mystatus(tid) == VIRTIO_BLK_S_OK)
		return OK;

	/* Error path */
	dprintf(("ERROR status=%02x op=flush t=%d", mystatus(tid), tid));

	return virtio_blk_status2error(mystatus(tid));
}

static void
virtio_blk_terminate(void)
{
	/* Don't terminate if still opened */
	if (open_count > 0)
		return;

	blockdriver_mt_terminate();
}

static void
virtio_blk_cleanup(void)
{
	/* Just free the memory we allocated */
	virtio_blk_free_requests();
	virtio_reset_device(blk_dev);
	virtio_free_queues(blk_dev);
	virtio_free_device(blk_dev);
	blk_dev = NULL;
}

static int
virtio_blk_status2error(u8_t status)
{
	/* Convert a status from the host to an error */
	switch (status) {
		case VIRTIO_BLK_S_IOERR:
			return EIO;
		case VIRTIO_BLK_S_UNSUPP:
			return ENOTSUP;
		default:
			panic("%s: unknown status: %02x", name, status);
	}
	/* Never reached */
	return OK;
}

static int
virtio_blk_alloc_requests(void)
{
	/* Allocate memory for request headers and status field */

	hdrs_vir = alloc_contig(VIRTIO_BLK_NUM_THREADS * sizeof(hdrs_vir[0]),
				AC_ALIGN4K, &hdrs_phys);

	if (!hdrs_vir)
		return ENOMEM;

	status_vir = alloc_contig(VIRTIO_BLK_NUM_THREADS * sizeof(status_vir[0]),
				  AC_ALIGN4K, &status_phys);

	if (!status_vir) {
		free_contig(hdrs_vir, VIRTIO_BLK_NUM_THREADS * sizeof(hdrs_vir[0]));
		return ENOMEM;
	}

	return OK;
}

static void
virtio_blk_free_requests(void)
{
	free_contig(hdrs_vir, VIRTIO_BLK_NUM_THREADS * sizeof(hdrs_vir[0]));
	free_contig(status_vir, VIRTIO_BLK_NUM_THREADS * sizeof(status_vir[0]));
}

static int
virtio_blk_feature_setup(void)
{
	/* Feature setup for virtio-blk
	 *
	 * FIXME: Besides the geometry, everything is just debug output
	 * FIXME2: magic numbers
	 */
	if (virtio_host_supports(blk_dev, VIRTIO_BLK_F_SEG_MAX)) {
		blk_config.seg_max = virtio_sread32(blk_dev, 12);
		dprintf(("Seg Max: %d", blk_config.seg_max));
	}

	if (virtio_host_supports(blk_dev, VIRTIO_BLK_F_GEOMETRY)) {
		blk_config.geometry.cylinders = virtio_sread16(blk_dev, 16);
		blk_config.geometry.heads = virtio_sread8(blk_dev, 18);
		blk_config.geometry.sectors = virtio_sread8(blk_dev, 19);

		dprintf(("Geometry: cyl=%d heads=%d sectors=%d",
					blk_config.geometry.cylinders,
					blk_config.geometry.heads,
					blk_config.geometry.sectors));
	}

	if (virtio_host_supports(blk_dev, VIRTIO_BLK_F_SIZE_MAX))
		dprintf(("Has size max"));

	if (virtio_host_supports(blk_dev, VIRTIO_BLK_F_FLUSH))
		dprintf(("Supports flushing"));

	if (virtio_host_supports(blk_dev, VIRTIO_BLK_F_BLK_SIZE)) {
		blk_config.blk_size = virtio_sread32(blk_dev, 20);
		dprintf(("Block Size: %d", blk_config.blk_size));
	}

	if (virtio_host_supports(blk_dev, VIRTIO_BLK_F_BARRIER))
		dprintf(("Supports barrier"));

	return 0;
}

static int
virtio_blk_config(void)
{
	u32_t sectors_low, sectors_high, size_mbs;

	/* capacity is always there */
	sectors_low = virtio_sread32(blk_dev, 0);
	sectors_high = virtio_sread32(blk_dev, 4);
	blk_config.capacity = ((u64_t)sectors_high << 32) | sectors_low;

	/* If this gets truncated, you have a big disk... */
	size_mbs = (u32_t)(blk_config.capacity * 512 / 1024 / 1024);
	dprintf(("Capacity: %d MB", size_mbs));

	/* do feature setup */
	virtio_blk_feature_setup();
	return 0;
}

static int
virtio_blk_probe(int skip)
{
	int r;

	/* sub device id for virtio-blk is 0x0002 */
	blk_dev = virtio_setup_device(0x0002, name, blkf,
				      sizeof(blkf) / sizeof(blkf[0]),
				      VIRTIO_BLK_NUM_THREADS, skip);
	if (!blk_dev)
		return ENXIO;

	/* virtio-blk has one queue only */
	if ((r = virtio_alloc_queues(blk_dev, 1)) != OK) {
		virtio_free_device(blk_dev);
		return r;
	}

	/* Allocate memory for headers and status */
	if ((r = virtio_blk_alloc_requests() != OK)) {
		virtio_free_queues(blk_dev);
		virtio_free_device(blk_dev);
		return r;
	}

	virtio_blk_config();

	/* Let the host now that we are ready */
	virtio_device_ready(blk_dev);

	virtio_irq_enable(blk_dev);

	return OK;
}

static int
sef_cb_init_fresh(int type, sef_init_info_t *info)
{
	long instance = 0;
	int r;

	env_parse("instance", "d", 0, &instance, 0, 255);

	if ((r = virtio_blk_probe((int)instance)) == OK) {
		blockdriver_announce(type);
		return OK;
	}

	/* Error path */
	if (r == ENXIO)
		panic("%s: No device found", name);

	if (r == ENOMEM)
		panic("%s: Not enough memory", name);

	panic("%s: Unexpected failure (%d)", name, r);
}

static void
sef_cb_signal_handler(int signo)
{
	/* Ignore all signals but SIGTERM */
	if (signo != SIGTERM)
		return;

	terminating = 1;
	virtio_blk_terminate();

	/* If we get a signal when completely closed, call
	 * exit(). We only leave the blockdriver_mt_task()
	 * loop after completing a request which is not the
	 * case for signals.
	 */
	if (open_count == 0)
		exit(0);
}

static void
sef_local_startup(void)
{
	sef_setcb_init_fresh(sef_cb_init_fresh);
	sef_setcb_init_lu(sef_cb_init_fresh);
	sef_setcb_signal_handler(sef_cb_signal_handler);

	sef_startup();
}

int
main(int argc, char **argv)
{
	env_setargs(argc, argv);
	sef_local_startup();

	blockdriver_mt_task(&virtio_blk_dtab);

	dprintf(("Terminating"));
	virtio_blk_cleanup();

	return OK;
}
