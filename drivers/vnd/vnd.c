/* VNode Disk driver, by D.C. van Moolenbroek <david@minix3.org> */

#include <minix/drivers.h>
#include <minix/blockdriver.h>
#include <minix/drvlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>

#define VND_BUF_SIZE	65536

static struct {
	int fd;			/* file descriptor for the underlying file */
	int openct;		/* number of times the device is open */
	int exiting;		/* exit after the last close? */
	int rdonly;		/* is the device set up read-only? */
	dev_t dev;		/* device on which the file resides */
	ino_t ino;		/* inode number of the file */
	struct device part[DEV_PER_DRIVE];	/* partition bases and sizes */
	struct device subpart[SUB_PER_DRIVE];	/* same for subpartitions */
	struct part_geom geom;	/* geometry information */
	char *buf;		/* intermediate I/O transfer buffer */
} state;

static unsigned int instance;

static int vnd_open(devminor_t, int);
static int vnd_close(devminor_t);
static int vnd_transfer(devminor_t, int, u64_t, endpoint_t, iovec_t *,
	unsigned int, int);
static int vnd_ioctl(devminor_t, unsigned long, endpoint_t, cp_grant_id_t,
	endpoint_t);
static struct device *vnd_part(devminor_t);
static void vnd_geometry(devminor_t, struct part_geom *);

static struct blockdriver vnd_dtab = {
	.bdr_type	= BLOCKDRIVER_TYPE_DISK,
	.bdr_open	= vnd_open,
	.bdr_close	= vnd_close,
	.bdr_transfer	= vnd_transfer,
	.bdr_ioctl	= vnd_ioctl,
	.bdr_part	= vnd_part,
	.bdr_geometry	= vnd_geometry
};

/*
 * Parse partition tables.
 */
static void
vnd_partition(void)
{
	memset(state.part, 0, sizeof(state.part));
	memset(state.subpart, 0, sizeof(state.subpart));

	state.part[0].dv_size = state.geom.size;

	partition(&vnd_dtab, 0, P_PRIMARY, FALSE /*atapi*/);
}

/*
 * Open a device.
 */
static int
vnd_open(devminor_t minor, int access)
{
	/* No sub/partition devices are available before initialization. */
	if (state.fd == -1 && minor != 0)
		return ENXIO;
	else if (state.fd != -1 && vnd_part(minor) == NULL)
		return ENXIO;

	/*
	 * If the device either is not configured or configured as read-only,
	 * block open calls that request write permission.  This is what user-
	 * land expects, although it does mean that vnconfig(8) has to open the
	 * device as read-only in order to (un)configure it.
	 */
	if (access & BDEV_W_BIT) {
		if (state.fd == -1)
			return ENXIO;
		if (state.rdonly)
			return EACCES;
	}

	/*
	 * Userland expects that if the device is opened after having been
	 * fully closed, partition tables are (re)parsed.  Since we already
	 * parse partition tables upon initialization, we could skip this for
	 * the first open, but that would introduce more state.
	 */
	if (state.fd != -1 && state.openct == 0) {
		vnd_partition();

		/* Make sure our target device didn't just disappear. */
		if (vnd_part(minor) == NULL)
			return ENXIO;
	}

	state.openct++;

	return OK;
}

/*
 * Close a device.
 */
static int
vnd_close(devminor_t UNUSED(minor))
{
	if (state.openct == 0) {
		printf("VND%u: closing already-closed device\n", instance);
		return EINVAL;
	}

	state.openct--;

	if (state.exiting)
		blockdriver_terminate();

	return OK;
}

/*
 * Copy a number of bytes from or to the caller, to or from the intermediate
 * buffer.  If the given endpoint is SELF, a local memory copy must be made.
 */
static int
vnd_copy(iovec_s_t *iov, size_t iov_off, size_t bytes, endpoint_t endpt,
	int do_write)
{
	struct vscp_vec vvec[SCPVEC_NR], *vvp;
	size_t off, chunk;
	int count;
	char *ptr;

	assert(bytes > 0 && bytes <= VND_BUF_SIZE);

	vvp = vvec;
	count = 0;

	for (off = 0; off < bytes; off += chunk) {
		chunk = MIN(bytes - off, iov->iov_size - iov_off);

		if (endpt == SELF) {
			ptr = (char *) iov->iov_grant + iov_off;

			if (do_write)
				memcpy(&state.buf[off], ptr, chunk);
			else
				memcpy(ptr, &state.buf[off], chunk);
		} else {
			assert(count < SCPVEC_NR); /* SCPVEC_NR >= NR_IOREQS */

			vvp->v_from = do_write ? endpt : SELF;
			vvp->v_to = do_write ? SELF : endpt;
			vvp->v_bytes = chunk;
			vvp->v_gid = iov->iov_grant;
			vvp->v_offset = iov_off;
			vvp->v_addr = (vir_bytes) &state.buf[off];

			vvp++;
			count++;
		}

		iov_off += chunk;
		if (iov_off == iov->iov_size) {
			iov++;
			iov_off = 0;
		}
	}

	if (endpt != SELF)
		return sys_vsafecopy(vvec, count);
	else
		return OK;
}

/*
 * Advance the given I/O vector, and the offset into its first element, by the
 * given number of bytes.
 */
static iovec_s_t *
vnd_advance(iovec_s_t *iov, size_t *iov_offp, size_t bytes)
{
	size_t iov_off;

	assert(bytes > 0 && bytes <= VND_BUF_SIZE);

	iov_off = *iov_offp;

	while (bytes > 0) {
		if (bytes >= iov->iov_size - iov_off) {
			bytes -= iov->iov_size - iov_off;
			iov++;
			iov_off = 0;
		} else {
			iov_off += bytes;
			bytes = 0;
		}
	}

	*iov_offp = iov_off;
	return iov;
}

/*
 * Perform data transfer on the selected device.
 */
static int
vnd_transfer(devminor_t minor, int do_write, u64_t position,
	endpoint_t endpt, iovec_t *iovt, unsigned int nr_req, int flags)
{
	struct device *dv;
	iovec_s_t *iov;
	size_t off, chunk, bytes, iov_off;
	ssize_t r;
	unsigned int i;

	iov = (iovec_s_t *) iovt;

	if (state.fd == -1 || (dv = vnd_part(minor)) == NULL)
		return ENXIO;

	/* Prevent write operations on devices opened as write-only. */
	if (do_write && state.rdonly)
		return EACCES;

	/* Determine the total number of bytes to transfer. */
	if (position >= dv->dv_size)
		return 0;

	bytes = 0;

	for (i = 0; i < nr_req; i++) {
		if (iov[i].iov_size == 0 || iov[i].iov_size > LONG_MAX)
			return EINVAL;
		bytes += iov[i].iov_size;
		if (bytes > LONG_MAX)
			return EINVAL;
	}

	if (bytes > dv->dv_size - position)
		bytes = dv->dv_size - position;

	position += dv->dv_base;

	/* Perform the actual transfer, in chunks if necessary. */
	iov_off = 0;

	for (off = 0; off < bytes; off += chunk) {
		chunk = MIN(bytes - off, VND_BUF_SIZE);

		assert((unsigned int) (iov - (iovec_s_t *) iovt) < nr_req);

		/* For reads, read in the data for the chunk; possibly less. */
		if (!do_write) {
			chunk = r = pread(state.fd, state.buf, chunk,
			    position);

			if (r < 0) {
				printf("VND%u: pread failed (%d)\n", instance,
				    -errno);
				return -errno;
			}
			if (r == 0)
				break;
		}

		/* Copy the data for this chunk from or to the caller. */
		if ((r = vnd_copy(iov, iov_off, chunk, endpt, do_write)) < 0) {
			printf("VND%u: data copy failed (%d)\n", instance, r);
			return r;
		}

		/* For writes, write the data to the file; possibly less. */
		if (do_write) {
			chunk = r = pwrite(state.fd, state.buf, chunk,
			    position);

			if (r <= 0) {
				if (r < 0)
					r = -errno;
				printf("VND%u: pwrite failed (%d)\n", instance,
				    r);
				return (r < 0) ? r : EIO;
			}
		}

		/* Move ahead on the I/O vector and the file position. */
		iov = vnd_advance(iov, &iov_off, chunk);

		position += chunk;
	}

	/* If force-write is requested, flush the underlying file to disk. */
	if (do_write && (flags & BDEV_FORCEWRITE))
		fsync(state.fd);

	/* Return the number of bytes transferred. */
	return off;
}

/*
 * Initialize the size and geometry for the device and any partitions.  If the
 * user provided a geometry, this will be used; otherwise, a geometry will be
 * computed.
 */
static int
vnd_layout(u64_t size, struct vnd_ioctl *vnd)
{
	u64_t sectors;

	state.geom.base = 0ULL;

	if (vnd->vnd_flags & VNDIOF_HASGEOM) {
		/*
		 * The geometry determines the accessible part of the file.
		 * The resulting size must not exceed the file size.
		 */
		state.geom.cylinders = vnd->vnd_geom.vng_ncylinders;
		state.geom.heads = vnd->vnd_geom.vng_ntracks;
		state.geom.sectors = vnd->vnd_geom.vng_nsectors;

		state.geom.size = (u64_t) state.geom.cylinders *
		    state.geom.heads * state.geom.sectors *
		    vnd->vnd_geom.vng_secsize;
		if (state.geom.size == 0 || state.geom.size > size)
			return EINVAL;
	} else {
		sectors = size / SECTOR_SIZE;
		state.geom.size = sectors * SECTOR_SIZE;

		if (sectors >= 32 * 64) {
			state.geom.cylinders = sectors / (32 * 64);
			state.geom.heads = 64;
			state.geom.sectors = 32;
		} else {
			state.geom.cylinders = sectors;
			state.geom.heads = 1;
			state.geom.sectors = 1;
		}
	}

	/*
	 * Parse partition tables immediately, so that (sub)partitions can be
	 * opened right away.  The first open will perform the same procedure,
	 * but that is only necessary to match userland expectations.
	 */
	vnd_partition();

	return OK;
}

/*
 * Process I/O control requests.
 */
static int
vnd_ioctl(devminor_t UNUSED(minor), unsigned long request, endpoint_t endpt,
	cp_grant_id_t grant, endpoint_t user_endpt)
{
	struct vnd_ioctl vnd;
	struct vnd_user vnu;
	struct stat st;
	int r;

	switch (request) {
	case VNDIOCSET:
		/*
		 * The VND must not be busy.  Note that the caller has the
		 * device open to perform the IOCTL request.
		 */
		if (state.fd != -1 || state.openct != 1)
			return EBUSY;

		if ((r = sys_safecopyfrom(endpt, grant, 0, (vir_bytes) &vnd,
		    sizeof(vnd))) != OK)
			return r;

		/*
		 * Issue a special VFS backcall that copies a file descriptor
		 * to the current process, from the user process ultimately
		 * making the IOCTL call.  The result is either a newly
		 * allocated file descriptor or an error.
		 */
		if ((r = copyfd(user_endpt, vnd.vnd_fildes, COPYFD_FROM)) < 0)
			return r;

		state.fd = r;

		/* The target file must be regular. */
		if (fstat(state.fd, &st) == -1) {
			printf("VND%u: fstat failed (%d)\n", instance, -errno);
			r = -errno;
		}
		if (r == OK && !S_ISREG(st.st_mode))
			r = EINVAL;

		/*
		 * Allocate memory for an intermediate I/O transfer buffer. In
		 * order to save on memory in the common case, the buffer is
		 * only allocated when the vnd is in use.  We use mmap instead
		 * of malloc to allow the memory to be actually freed later.
		 */
		if (r == OK) {
			state.buf = mmap(NULL, VND_BUF_SIZE, PROT_READ |
			    PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
			if (state.buf == MAP_FAILED)
				r = ENOMEM;
		}

		if (r != OK) {
			close(state.fd);
			state.fd = -1;
			return r;
		}

		/* Set various device state fields. */
		state.dev = st.st_dev;
		state.ino = st.st_ino;
		state.rdonly = !!(vnd.vnd_flags & VNDIOF_READONLY);

		r = vnd_layout(st.st_size, &vnd);

		/* Upon success, return the device size to userland. */
		if (r == OK) {
			vnd.vnd_size = state.geom.size;

			r = sys_safecopyto(endpt, grant, 0, (vir_bytes) &vnd,
			    sizeof(vnd));
		}

		if (r != OK) {
			munmap(state.buf, VND_BUF_SIZE);
			close(state.fd);
			state.fd = -1;
		}

		return r;

	case VNDIOCCLR:
		/* The VND can only be cleared if it has been configured. */
		if (state.fd == -1)
			return ENXIO;

		if ((r = sys_safecopyfrom(endpt, grant, 0, (vir_bytes) &vnd,
		    sizeof(vnd))) != OK)
			return r;

		/* The caller has the device open to do the IOCTL request. */
		if (!(vnd.vnd_flags & VNDIOF_FORCE) && state.openct != 1)
			return EBUSY;

		/*
		 * Close the associated file descriptor immediately, but do not
		 * allow reuse until the device has been closed by the other
		 * users.
		 */
		munmap(state.buf, VND_BUF_SIZE);
		close(state.fd);
		state.fd = -1;

		return OK;

	case VNDIOCGET:
		/*
		 * We need not copy in the given structure.  It would contain
		 * the requested unit number, but each driver instance provides
		 * only one unit anyway.
		 */

		memset(&vnu, 0, sizeof(vnu));

		vnu.vnu_unit = instance;

		/* Leave these fields zeroed if the device is not in use. */
		if (state.fd != -1) {
			vnu.vnu_dev = state.dev;
			vnu.vnu_ino = state.ino;
		}

		return sys_safecopyto(endpt, grant, 0, (vir_bytes) &vnu,
		    sizeof(vnu));

	case DIOCOPENCT:
		return sys_safecopyto(endpt, grant, 0,
		    (vir_bytes) &state.openct, sizeof(state.openct));

	case DIOCFLUSH:
		if (state.fd == -1)
			return ENXIO;

		fsync(state.fd);

		return OK;
	}

	return ENOTTY;
}

/*
 * Return a pointer to the partition structure for the given minor device.
 */
static struct device *
vnd_part(devminor_t minor)
{
	if (minor >= 0 && minor < DEV_PER_DRIVE)
		return &state.part[minor];
	else if ((unsigned int) (minor -= MINOR_d0p0s0) < SUB_PER_DRIVE)
		return &state.subpart[minor];
	else
		return NULL;
}

/*
 * Return geometry information.
 */
static void
vnd_geometry(devminor_t UNUSED(minor), struct part_geom *part)
{
	part->cylinders = state.geom.cylinders;
	part->heads = state.geom.heads;
	part->sectors = state.geom.sectors;
}

/*
 * Initialize the device.
 */
static int
vnd_init(int UNUSED(type), sef_init_info_t *UNUSED(info))
{
	long v;

	/*
	 * No support for crash recovery.  The driver would have no way to
	 * reacquire the file descriptor for the target file.
	 */

	/*
	 * The instance number is used for two purposes: reporting errors, and
	 * returning the proper unit number to userland in VNDIOCGET calls.
	 */
	v = 0;
	(void) env_parse("instance", "d", 0, &v, 0, 255);
	instance = (unsigned int) v;

	state.openct = 0;
	state.exiting = FALSE;
	state.fd = -1;

	return OK;
}

/*
 * Process an incoming signal.
 */
static void
vnd_signal(int signo)
{

	/* In case of a termination signal, initiate driver shutdown. */
	if (signo != SIGTERM)
		return;

	state.exiting = TRUE;

	/* Keep running until the device has been fully closed. */
	if (state.openct == 0)
		blockdriver_terminate();
}

/*
 * Set callbacks and initialize the System Event Framework (SEF).
 */
static void
vnd_startup(void)
{

	/* Register init and signal callbacks. */
	sef_setcb_init_fresh(vnd_init);
	sef_setcb_signal_handler(vnd_signal);

	/* Let SEF perform startup. */
	sef_startup();
}

/*
 * Driver task.
 */
int
main(int argc, char **argv)
{

	/* Initialize the driver. */
	env_setargs(argc, argv);
	vnd_startup();

	/* Process requests until shutdown. */
	blockdriver_task(&vnd_dtab);

	return 0;
}
