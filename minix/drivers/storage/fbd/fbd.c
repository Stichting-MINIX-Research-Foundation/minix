/* Faulty Block Device (fault injection proxy), by D.C. van Moolenbroek */
#include <stdlib.h>
#include <minix/drivers.h>
#include <minix/blockdriver.h>
#include <minix/drvlib.h>
#include <minix/ioctl.h>
#include <sys/ioc_fbd.h>
#include <minix/ds.h>
#include <minix/optset.h>
#include <assert.h>

#include "rule.h"

/* Constants. */
#define BUF_SIZE (NR_IOREQS * CLICK_SIZE)	/* 256k */

/* Function declarations. */
static int fbd_open(devminor_t minor, int access);
static int fbd_close(devminor_t minor);
static int fbd_transfer(devminor_t minor, int do_write, u64_t position,
	endpoint_t endpt, iovec_t *iov, unsigned int nr_req, int flags);
static int fbd_ioctl(devminor_t minor, unsigned long request, endpoint_t endpt,
	cp_grant_id_t grant, endpoint_t user_endpt);

/* Variables. */
static char *fbd_buf;			/* scratch buffer */

static char driver_label[32] = "";	/* driver DS label */
static devminor_t driver_minor = -1;	/* driver's partition minor to use */
static endpoint_t driver_endpt;		/* driver endpoint */

/* Entry points to this driver. */
static struct blockdriver fbd_dtab = {
	.bdr_type	= BLOCKDRIVER_TYPE_OTHER,/* do not handle part. reqs */
	.bdr_open	= fbd_open,	/* open request, initialize device */
	.bdr_close	= fbd_close,	/* release device */
	.bdr_transfer	= fbd_transfer,	/* do the I/O */
	.bdr_ioctl	= fbd_ioctl	/* perform I/O control request */
};

/* Options supported by this driver. */
static struct optset optset_table[] = {
	{ "label",	OPT_STRING,	driver_label,	sizeof(driver_label) },
	{ "minor",	OPT_INT,	&driver_minor,	10		     },
	{ NULL,		0,		NULL,		0		     }
};

/*===========================================================================*
 *				sef_cb_init_fresh			     *
 *===========================================================================*/
static int sef_cb_init_fresh(int type, sef_init_info_t *UNUSED(info))
{

	/* Parse the given parameters. */
	if (env_argc > 1)
		optset_parse(optset_table, env_argv[1]);

	if (driver_label[0] == '\0')
		panic("no driver label given");

	if (ds_retrieve_label_endpt(driver_label, &driver_endpt))
		panic("unable to resolve driver label");

	if (driver_minor > 255)
		panic("no or invalid driver minor given");

#if DEBUG
	printf("FBD: driver label '%s' (endpt %d), minor %d\n",
		driver_label, driver_endpt, driver_minor);
#endif

	/* Initialize resources. */
	fbd_buf = alloc_contig(BUF_SIZE, 0, NULL);

	if (fbd_buf == NULL)
		panic("unable to allocate buffer");

	srand48(getticks());

	/* Announce we are up! */
	blockdriver_announce(type);

	return OK;
}

/*===========================================================================*
 *				sef_cb_signal_handler			     *
 *===========================================================================*/
static void sef_cb_signal_handler(int signo)
{
	/* Terminate immediately upon receiving a SIGTERM. */
	if (signo != SIGTERM) return;

#if DEBUG
	printf("FBD: shutting down\n");
#endif

	/* Clean up resources. */
	free_contig(fbd_buf, BUF_SIZE);

	exit(0);
}

/*===========================================================================*
 *				sef_local_startup			     *
 *===========================================================================*/
static void sef_local_startup(void)
{
	/* Register init callbacks. */
	sef_setcb_init_fresh(sef_cb_init_fresh);
	sef_setcb_init_restart(sef_cb_init_fresh);

	/* Register signal callback. */
	sef_setcb_signal_handler(sef_cb_signal_handler);

	/* Let SEF perform startup. */
	sef_startup();
}

/*===========================================================================*
 *				main					     *
 *===========================================================================*/
int main(int argc, char **argv)
{
	/* SEF local startup. */
	env_setargs(argc, argv);
	sef_local_startup();

	/* Call the generic receive loop. */
	blockdriver_task(&fbd_dtab);

	return OK;
}

/*===========================================================================*
 *				fbd_open				     *
 *===========================================================================*/
static int fbd_open(devminor_t UNUSED(minor), int access)
{
	/* Open a device. */
	message m;
	int r;

	/* We simply forward this request to the real driver. */
	memset(&m, 0, sizeof(m));
	m.m_type = BDEV_OPEN;
	m.m_lbdev_lblockdriver_msg.minor = driver_minor;
	m.m_lbdev_lblockdriver_msg.access = access;
	m.m_lbdev_lblockdriver_msg.id = 0;

	if ((r = ipc_sendrec(driver_endpt, &m)) != OK)
		panic("ipc_sendrec to driver failed (%d)\n", r);

	if (m.m_type != BDEV_REPLY)
		panic("invalid reply from driver (%d)\n", m.m_type);

	return m.m_lblockdriver_lbdev_reply.status;
}

/*===========================================================================*
 *				fbd_close				     *
 *===========================================================================*/
static int fbd_close(devminor_t UNUSED(minor))
{
	/* Close a device. */
	message m;
	int r;

	/* We simply forward this request to the real driver. */
	memset(&m, 0, sizeof(m));
	m.m_type = BDEV_CLOSE;
	m.m_lbdev_lblockdriver_msg.minor = driver_minor;
	m.m_lbdev_lblockdriver_msg.id = 0;

	if ((r = ipc_sendrec(driver_endpt, &m)) != OK)
		panic("ipc_sendrec to driver failed (%d)\n", r);

	if (m.m_type != BDEV_REPLY)
		panic("invalid reply from driver (%d)\n", m.m_type);

	return m.m_lblockdriver_lbdev_reply.status;
}

/*===========================================================================*
 *				fbd_ioctl				     *
 *===========================================================================*/
static int fbd_ioctl(devminor_t UNUSED(minor), unsigned long request,
	endpoint_t endpt, cp_grant_id_t grant, endpoint_t UNUSED(user_endpt))
{
	/* Handle an I/O control request. */
	cp_grant_id_t gid;
	message m;
	int r;

	/* We only handle the FBD requests, and pass on everything else. */
	switch (request) {
	case FBDCADDRULE:
	case FBDCDELRULE:
	case FBDCGETRULE:
		return rule_ctl(request, endpt, grant);
	}

	assert(grant != GRANT_INVALID);

	gid = cpf_grant_indirect(driver_endpt, endpt, grant);
	assert(gid != GRANT_INVALID);

	memset(&m, 0, sizeof(m));
	m.m_type = BDEV_IOCTL;
	m.m_lbdev_lblockdriver_msg.minor = driver_minor;
	m.m_lbdev_lblockdriver_msg.request = request;
	m.m_lbdev_lblockdriver_msg.grant = gid;
	m.m_lbdev_lblockdriver_msg.user = NONE;
	m.m_lbdev_lblockdriver_msg.id = 0;

	if ((r = ipc_sendrec(driver_endpt, &m)) != OK)
		panic("ipc_sendrec to driver failed (%d)\n", r);

	if (m.m_type != BDEV_REPLY)
		panic("invalid reply from driver (%d)\n", m.m_type);

	cpf_revoke(gid);

	return m.m_lblockdriver_lbdev_reply.status;
}

/*===========================================================================*
 *				fbd_transfer_direct			     *
 *===========================================================================*/
static ssize_t fbd_transfer_direct(int do_write, u64_t position,
	endpoint_t endpt, iovec_t *iov, unsigned int count, int flags)
{
	/* Forward the entire transfer request, without any intervention. */
	iovec_s_t iovec[NR_IOREQS];
	cp_grant_id_t grant;
	message m;
	int i, r;

	for (i = 0; i < count; i++) {
		iovec[i].iov_size = iov[i].iov_size;
		iovec[i].iov_grant = cpf_grant_indirect(driver_endpt, endpt,
			iov[i].iov_addr);
		assert(iovec[i].iov_grant != GRANT_INVALID);
	}

	grant = cpf_grant_direct(driver_endpt, (vir_bytes) iovec,
		count * sizeof(iovec[0]), CPF_READ);
	assert(grant != GRANT_INVALID);

	m.m_type = do_write ? BDEV_SCATTER : BDEV_GATHER;
	m.m_lbdev_lblockdriver_msg.minor = driver_minor;
	m.m_lbdev_lblockdriver_msg.count = count;
	m.m_lbdev_lblockdriver_msg.grant = grant;
	m.m_lbdev_lblockdriver_msg.flags = flags;
	m.m_lbdev_lblockdriver_msg.id = 0;
	m.m_lbdev_lblockdriver_msg.pos = position;

	if ((r = ipc_sendrec(driver_endpt, &m)) != OK)
		panic("ipc_sendrec to driver failed (%d)\n", r);

	if (m.m_type != BDEV_REPLY)
		panic("invalid reply from driver (%d)\n", m.m_type);

	cpf_revoke(grant);

	for (i = 0; i < count; i++)
		cpf_revoke(iovec[i].iov_grant);

	return m.m_lblockdriver_lbdev_reply.status;
}

/*===========================================================================*
 *				fbd_transfer_copy			     *
 *===========================================================================*/
static ssize_t fbd_transfer_copy(int do_write, u64_t position,
	endpoint_t endpt, iovec_t *iov, unsigned int count, size_t size,
	int flags)
{
	/* Interpose on the request. */
	iovec_s_t iovec[NR_IOREQS];
	struct vscp_vec vscp_vec[SCPVEC_NR];
	cp_grant_id_t grant;
	size_t off, len;
	message m;
	char *ptr;
	int i, j, r;
	ssize_t rsize;

	assert(count > 0 && count <= SCPVEC_NR);

	if (size > BUF_SIZE) {
		printf("FBD: allocating memory for %d bytes\n", size);

		ptr = alloc_contig(size, 0, NULL);

		assert(ptr != NULL);
	}
	else ptr = fbd_buf;

	/* For write operations, first copy in the data to write. */
	if (do_write) {
		for (i = off = 0; i < count; i++) {
			len = iov[i].iov_size;

			vscp_vec[i].v_from = endpt;
			vscp_vec[i].v_to = SELF;
			vscp_vec[i].v_gid = iov[i].iov_addr;
			vscp_vec[i].v_offset = 0;
			vscp_vec[i].v_addr = (vir_bytes) (ptr + off);
			vscp_vec[i].v_bytes = len;

			off += len;
		}

		if ((r = sys_vsafecopy(vscp_vec, i)) != OK)
			panic("vsafecopy failed (%d)\n", r);

		/* Trigger write hook. */
		rule_io_hook(ptr, size, position, FBD_FLAG_WRITE);
	}

	/* Allocate grants for the data, in the same chunking as the original
	 * vector. This avoids performance fluctuations with bad hardware as
	 * observed with the filter driver.
	 */
	for (i = off = 0; i < count; i++) {
		len = iov[i].iov_size;

		iovec[i].iov_size = len;
		iovec[i].iov_grant = cpf_grant_direct(driver_endpt,
			(vir_bytes) (ptr + off), len,
			do_write ? CPF_READ : CPF_WRITE);
		assert(iovec[i].iov_grant != GRANT_INVALID);

		off += len;
	}

	grant = cpf_grant_direct(driver_endpt, (vir_bytes) iovec,
		count * sizeof(iovec[0]), CPF_READ);
	assert(grant != GRANT_INVALID);

	m.m_type = do_write ? BDEV_SCATTER : BDEV_GATHER;
	m.m_lbdev_lblockdriver_msg.minor = driver_minor;
	m.m_lbdev_lblockdriver_msg.count = count;
	m.m_lbdev_lblockdriver_msg.grant = grant;
	m.m_lbdev_lblockdriver_msg.flags = flags;
	m.m_lbdev_lblockdriver_msg.id = 0;
	m.m_lbdev_lblockdriver_msg.pos = position;

	if ((r = ipc_sendrec(driver_endpt, &m)) != OK)
		panic("ipc_sendrec to driver failed (%d)\n", r);

	if (m.m_type != BDEV_REPLY)
		panic("invalid reply from driver (%d)\n", m.m_type);

	cpf_revoke(grant);

	for (i = 0; i < count; i++)
		cpf_revoke(iovec[i].iov_grant);

	/* For read operations, finish by copying out the data read. */
	if (!do_write) {
		/* Trigger read hook. */
		rule_io_hook(ptr, size, position, FBD_FLAG_READ);

		/* Upon success, copy back whatever has been processed. */
		rsize = m.m_lblockdriver_lbdev_reply.status;
		for (i = j = off = 0; rsize > 0 && i < count; i++) {
			len = MIN(rsize, iov[i].iov_size);

			vscp_vec[j].v_from = SELF;
			vscp_vec[j].v_to = endpt;
			vscp_vec[j].v_gid = iov[i].iov_addr;
			vscp_vec[j].v_offset = 0;
			vscp_vec[j].v_addr = (vir_bytes) (ptr + off);
			vscp_vec[j].v_bytes = len;

			off += len;
			rsize -= len;
			j++;
		}

		if (j > 0 && (r = sys_vsafecopy(vscp_vec, j)) != OK)
			panic("vsafecopy failed (%d)\n", r);
	}

	if (ptr != fbd_buf)
		free_contig(ptr, size);

	return m.m_lblockdriver_lbdev_reply.status;
}

/*===========================================================================*
 *				fbd_transfer				     *
 *===========================================================================*/
static int fbd_transfer(devminor_t UNUSED(minor), int do_write, u64_t position,
	endpoint_t endpt, iovec_t *iov, unsigned int nr_req, int flags)
{
	/* Transfer data from or to the device. */
	unsigned int count;
	size_t size, osize;
	int i, hooks;
	ssize_t r;

	/* Compute the total size of the request. */
	for (size = i = 0; i < nr_req; i++)
		size += iov[i].iov_size;

	osize = size;
	count = nr_req;

	hooks = rule_find(position, size,
		do_write ? FBD_FLAG_WRITE : FBD_FLAG_READ);

#if DEBUG
	printf("FBD: %s operation for pos %"PRIx64" size %u -> hooks %x\n",
		do_write ? "write" : "read", position, size, hooks);
#endif

	if (hooks & PRE_HOOK)
		rule_pre_hook(iov, &count, &size, &position);

	if (count > 0) {
		if (hooks & IO_HOOK) {
			r = fbd_transfer_copy(do_write, position, endpt, iov,
				count, size, flags);
		} else {
			r = fbd_transfer_direct(do_write, position, endpt, iov,
				count, flags);
		}
	}
	else r = 0;

	if (hooks & POST_HOOK)
		rule_post_hook(osize, &r);

#if DEBUG
	printf("FBD: returning %d\n", r);
#endif

	return r;
}
