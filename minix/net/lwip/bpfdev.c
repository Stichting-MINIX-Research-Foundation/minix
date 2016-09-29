/* LWIP service - bpfdev.c - Berkeley Packet Filter (/dev/bpf) interface */
/*
 * BPF is a cloning device: opening /dev/bpf returns a new BPF device which is
 * independent from any other opened BPF devices.  We assume that each BPF
 * device is used by one single user process, and this implementation therefore
 * does not support multiple concurrent device calls on the same BPF device.
 *
 * Packet buffering basically follows the BSD model: each BPF device that is
 * configured (that is, it has been attached to an interface) has two buffers,
 * each of the configured size: a store buffer, where new packets are stored,
 * and a hold buffer, which is typically full and awaiting retrieval through a
 * read call from userland.  The buffers are swapped ("rotated") when the store
 * buffer is filled up and the hold buffer is empty - if the hold buffer is not
 * empty is not empty either, additional packets are dropped.
 *
 * These buffers are allocated when the BPF device is attached to an interface.
 * The interface may later disappear, in which case the BPF device is detached
 * from it, allowing any final packets to be read before read requests start
 * returning I/O errors.  The buffers are freed only when the device is closed.
 */

#include "lwip.h"
#include "bpfdev.h"

#include <minix/chardriver.h>
#include <net/if.h>
#include <net/bpfdesc.h>
#include <minix/bpf.h>
#include <sys/mman.h>

/*
 * Make sure that our implementation matches the BPF version in the NetBSD
 * headers.  If they change the version number, we may have to make changes
 * here accordingly.
 */
#if BPF_MAJOR_VERSION != 1 || BPF_MINOR_VERSION != 1
#error "NetBSD BPF version has changed"
#endif

/* The number of BPF devices. */
#define NR_BPFDEV		16

/* BPF receive buffer size: allowed range and default. */
#define BPF_BUF_MIN		BPF_WORDALIGN(sizeof(struct bpf_hdr))
#define BPF_BUF_DEF		32768
#define BPF_BUF_MAX		262144

/*
 * By opening /dev/bpf, one will obtain a cloned device with a different minor
 * number, which maps to one of the BPF devices.
 */
#define BPFDEV_MINOR		0	/* minor number of /dev/bpf */
#define BPFDEV_BASE_MINOR	1	/* base minor number for BPF devices */

static struct bpfdev {
	struct bpfdev_link bpf_link;	/* structure link, MUST be first */
	TAILQ_ENTRY(bpfdev) bpf_next;	/* next on free or interface list */
	struct ifdev *bpf_ifdev;	/* associated interface, or NULL */
	unsigned int bpf_flags;		/* flags (BPFF_) */
	size_t bpf_size;		/* size of packet buffers */
	char *bpf_sbuf;			/* store buffer (mmap'd, or NULL) */
	char *bpf_hbuf;			/* hold buffer (mmap'd, or NULL) */
	size_t bpf_slen;		/* used part of store buffer */
	size_t bpf_hlen;		/* used part of hold buffer */
	struct bpf_insn *bpf_filter;	/* verified BPF filter, or NULL */
	size_t bpf_filterlen;		/* length of filter, for munmap */
	pid_t bpf_pid;			/* process ID of last using process */
	clock_t bpf_timeout;		/* timeout for read calls (0 = none) */
	struct {			/* state for pending read request */
		endpoint_t br_endpt;	/* reading endpoint, or NONE */
		cp_grant_id_t br_grant;	/* grant for reader's buffer */
		cdev_id_t br_id;	/* read request identifier */
		minix_timer_t br_timer;	/* timer for read timeout */
	} bpf_read;
	struct {			/* state for pending select request */
		endpoint_t bs_endpt;	/* selecting endpoint, or NONE */
		unsigned int bs_selops;	/* pending select operations */
	} bpf_select;
	struct {			/* packet capture statistics */
		uint64_t bs_recv;	/* # of packets run through filter */
		uint64_t bs_drop;	/* # of packets dropped: buffer full */
		uint64_t bs_capt;	/* # of packets accepted by filter */
	} bpf_stat;
} bpf_array[NR_BPFDEV];

#define BPFF_IN_USE	0x01		/* this BPF device object is in use */
#define BPFF_PROMISC	0x02		/* promiscuous mode enabled */
#define BPFF_IMMEDIATE	0x04		/* immediate mode is enabled */
#define BPFF_SEESENT	0x08		/* also process host-sent packets */
#define BPFF_HDRCMPLT	0x10		/* do not fill in link-layer source */
#define BPFF_FEEDBACK	0x20		/* feed back written packet as input */

static TAILQ_HEAD(, bpfdev_link) bpfl_freelist;	/* list of free BPF devices */

static struct bpf_stat bpf_stat;

static ssize_t bpfdev_peers(struct rmib_call *, struct rmib_node *,
	struct rmib_oldp *, struct rmib_newp *);

/* The CTL_NET NET_BPF subtree.  All nodes are dynamically numbered. */
static struct rmib_node net_bpf_table[] = {
	RMIB_INT(RMIB_RO, BPF_BUF_MAX, "maxbufsize",
	    "Maximum size for data capture buffer"), /* TODO: read-write */
	RMIB_STRUCT(RMIB_RO, sizeof(bpf_stat), &bpf_stat, "stats",
	    "BPF stats"),
	RMIB_FUNC(RMIB_RO | CTLTYPE_NODE, 0, bpfdev_peers, "peers",
	    "BPF peers"),
};

static struct rmib_node net_bpf_node =
    RMIB_NODE(RMIB_RO, net_bpf_table, "bpf", "BPF options");

/*
 * Initialize the BPF module.
 */
void
bpfdev_init(void)
{
	const int mib[] = { CTL_NET, NET_BPF };
	unsigned int slot;
	int r;

	/* Initialize data structures. */
	TAILQ_INIT(&bpfl_freelist);

	for (slot = 0; slot < __arraycount(bpf_array); slot++) {
		bpf_array[slot].bpf_flags = 0;

		TAILQ_INSERT_TAIL(&bpfl_freelist, &bpf_array[slot].bpf_link,
		    bpfl_next);
	}

	memset(&bpf_stat, 0, sizeof(bpf_stat));

	/* Register the "net.bpf" subtree with the MIB service. */
	if ((r = rmib_register(mib, __arraycount(mib), &net_bpf_node)) != OK)
		panic("unable to register net.bpf RMIB tree: %d", r);
}

/*
 * Given a BPF device object, return the corresponding minor number.
 */
static devminor_t
bpfdev_get_minor(struct bpfdev * bpfdev)
{

	assert(bpfdev != NULL);

	return BPFDEV_BASE_MINOR + (devminor_t)(bpfdev - bpf_array);
}

/*
 * Given a minor number, return the corresponding BPF device object, or NULL if
 * the minor number does not identify a BPF device.
 */
static struct bpfdev *
bpfdev_get_by_minor(devminor_t minor)
{

	if (minor < BPFDEV_BASE_MINOR ||
	    (unsigned int)minor >= BPFDEV_BASE_MINOR + __arraycount(bpf_array))
		return NULL;

	return &bpf_array[minor - BPFDEV_BASE_MINOR];
}

/*
 * Open a BPF device, returning a cloned device instance.
 */
static int
bpfdev_open(devminor_t minor, int access __unused, endpoint_t user_endpt)
{
	struct bpfdev_link *bpfl;
	struct bpfdev *bpf;

	/* Disallow opening cloned devices through device nodes. */
	if (minor != BPFDEV_MINOR)
		return ENXIO;

	if (TAILQ_EMPTY(&bpfl_freelist))
		return ENOBUFS;

	bpfl = TAILQ_FIRST(&bpfl_freelist);
	TAILQ_REMOVE(&bpfl_freelist, bpfl, bpfl_next);

	bpf = (struct bpfdev *)bpfl;

	memset(bpf, 0, sizeof(*bpf));

	bpf->bpf_flags = BPFF_IN_USE | BPFF_SEESENT;
	bpf->bpf_size = BPF_BUF_DEF;
	bpf->bpf_pid = getnpid(user_endpt);
	bpf->bpf_read.br_endpt = NONE;
	bpf->bpf_select.bs_endpt = NONE;

	return CDEV_CLONED | bpfdev_get_minor(bpf);
}

/*
 * Close a BPF device.
 */
static int
bpfdev_close(devminor_t minor)
{
	struct bpfdev *bpf;

	if ((bpf = bpfdev_get_by_minor(minor)) == NULL)
		return EINVAL;

	/*
	 * There cannot possibly be a pending read request, so we never need to
	 * cancel the read timer from here either.
	 */
	assert(bpf->bpf_read.br_endpt == NONE);

	if (bpf->bpf_sbuf != NULL) {
		assert(bpf->bpf_hbuf != NULL);

		if (munmap(bpf->bpf_sbuf, bpf->bpf_size) != 0)
			panic("munmap failed: %d", -errno);
		if (munmap(bpf->bpf_hbuf, bpf->bpf_size) != 0)
			panic("munmap failed: %d", -errno);

		bpf->bpf_sbuf = NULL;
		bpf->bpf_hbuf = NULL;
	} else
		assert(bpf->bpf_hbuf == NULL);

	if (bpf->bpf_filter != NULL) {
		assert(bpf->bpf_filterlen > 0);

		if (munmap(bpf->bpf_filter, bpf->bpf_filterlen) != 0)
			panic("munmap failed: %d", -errno);

		bpf->bpf_filter = NULL;
	}

	/*
	 * If the BPF device was attached to an interface, and that interface
	 * has not disappeared in the meantime, detach from it now.
	 */
	if (bpf->bpf_ifdev != NULL) {
		if (bpf->bpf_flags & BPFF_PROMISC)
			ifdev_clear_promisc(bpf->bpf_ifdev);

		ifdev_detach_bpf(bpf->bpf_ifdev, &bpf->bpf_link);

		bpf->bpf_ifdev = NULL;
	}

	bpf->bpf_flags = 0;		/* mark as no longer in use */

	TAILQ_INSERT_HEAD(&bpfl_freelist, &bpf->bpf_link, bpfl_next);

	return OK;
}

/*
 * Rotate buffers for the BPF device, by swapping the store buffer and the hold
 * buffer.
 */
static void
bpfdev_rotate(struct bpfdev * bpf)
{
	char *buf;
	size_t len;

	/*
	 * When rotating, the store buffer may or may not be empty, but the
	 * hold buffer must always be empty.
	 */
	assert(bpf->bpf_hlen == 0);

	buf = bpf->bpf_sbuf;
	len = bpf->bpf_slen;
	bpf->bpf_sbuf = bpf->bpf_hbuf;
	bpf->bpf_slen = bpf->bpf_hlen;
	bpf->bpf_hbuf = buf;
	bpf->bpf_hlen = len;
}

/*
 * Test whether any of the given select operations are ready on the BPF device,
 * and return the set of ready operations.
 */
static unsigned int
bpfdev_test_select(struct bpfdev * bpf, unsigned int ops)
{
	unsigned int ready_ops;

	ready_ops = 0;

	/*
	 * The BPF device is ready for reading if the hold buffer is not empty
	 * (i.e.: the store buffer has been filled up completely and was
	 * therefore rotated) or if immediate mode is set and the store buffer
	 * is not empty (i.e.: any packet is available at all).  In the latter
	 * case, the buffers will be rotated during the read.  We do not
	 * support applying the read timeout to selects and maintaining state
	 * between the select and the following read, because despite that
	 * libpcap claims that it is the right behavior, that is just insane.
	 */
	if (ops & CDEV_OP_RD) {
		if (bpf->bpf_ifdev == NULL)
			ready_ops |= CDEV_OP_RD;
		else if (bpf->bpf_hlen > 0)
			ready_ops |= CDEV_OP_RD;
		else if ((bpf->bpf_flags & BPFF_IMMEDIATE) &&
		    bpf->bpf_slen > 0)
			ready_ops |= CDEV_OP_RD;
	}

	if (ops & CDEV_OP_WR)
		ready_ops |= CDEV_OP_WR;

	return ready_ops;
}

/*
 * There has been a state change on the BPF device.  If now possible, resume a
 * pending select query, if any.
 */
static void
bpfdev_resume_select(struct bpfdev * bpf)
{
	unsigned int ops, ready_ops;
	endpoint_t endpt;

	/* First see if there is a pending select request at all. */
	if ((endpt = bpf->bpf_select.bs_endpt) == NONE)
		return;
	ops = bpf->bpf_select.bs_selops;

	assert(ops != 0);

	/* Then see if any of the pending operations are now ready. */
	if ((ready_ops = bpfdev_test_select(bpf, ops)) == 0)
		return;

	/* If so, notify VFS about the ready operations. */
	chardriver_reply_select(bpf->bpf_select.bs_endpt,
	    bpfdev_get_minor(bpf), ready_ops);

	/*
	 * Forget about the ready operations.  If that leaves no pending
	 * operations, forget about the select request altogether.
	 */
	if ((bpf->bpf_select.bs_selops &= ~ready_ops) == 0)
		bpf->bpf_select.bs_endpt = NONE;
}

/*
 * There has been a state change on the BPF device.  If now possible, resume a
 * pending read request, if any.  If the call is a result of a timeout,
 * 'is_timeout' is set.  In that case, the read request must be resumed with an
 * EAGAIN error if no packets are available, and the running timer must be
 * canceled.  Otherwise, the resumption is due to a full buffer or a
 * disappeared interface, and 'is_timeout' is not set.  In this case, the read
 * request must be resumed with an I/O error if no packets are available.
 */
static void
bpfdev_resume_read(struct bpfdev * bpf, int is_timeout)
{
	ssize_t r;

	assert(bpf->bpf_read.br_endpt != NONE);

	/*
	 * If the hold buffer is still empty, see if the store buffer has
	 * any packets to copy out.
	 */
	if (bpf->bpf_hlen == 0)
		bpfdev_rotate(bpf);

	/* Return any available packets, or otherwise an error. */
	if (bpf->bpf_hlen > 0) {
		assert(bpf->bpf_hlen <= bpf->bpf_size);

		r = sys_safecopyto(bpf->bpf_read.br_endpt,
		    bpf->bpf_read.br_grant, 0, (vir_bytes)bpf->bpf_hbuf,
		    bpf->bpf_hlen);

		if (r == OK) {
			r = (ssize_t)bpf->bpf_hlen;

			bpf->bpf_hlen = 0;

			assert(bpf->bpf_slen != bpf->bpf_size);

			/*
			 * Allow readers to get the last packets after the
			 * interface has disappeared, before getting errors.
			 */
			if (bpf->bpf_ifdev == NULL)
				bpfdev_rotate(bpf);
		}
	} else
		r = (is_timeout) ? EAGAIN : EIO;

	chardriver_reply_task(bpf->bpf_read.br_endpt, bpf->bpf_read.br_id, r);

	bpf->bpf_read.br_endpt = NONE;

	/* Was there still a timer running?  Then cancel it now. */
	if (bpf->bpf_timeout > 0 && !is_timeout)
		cancel_timer(&bpf->bpf_read.br_timer);
}

/*
 * A read timeout has triggered for the BPF device.  Wake up the pending read
 * request.
 */
static void
bpfdev_timeout(int arg)
{
	struct bpfdev *bpf;

	assert(arg >= 0 && (unsigned int)arg < __arraycount(bpf_array));

	bpf = &bpf_array[arg];

	assert(bpf->bpf_read.br_endpt != NONE);

	bpfdev_resume_read(bpf, TRUE /*is_timeout*/);
}

/*
 * Read from a BPF device.
 */
static ssize_t
bpfdev_read(devminor_t minor, uint64_t position, endpoint_t endpt,
	cp_grant_id_t grant, size_t size, int flags, cdev_id_t id)
{
	struct bpfdev *bpf;
	ssize_t r;
	int suspend;

	if ((bpf = bpfdev_get_by_minor(minor)) == NULL)
		return EINVAL;

	/* Allow only one read call at a time. */
	if (bpf->bpf_read.br_endpt != NONE)
		return EIO;

	/* Has this BPF device been configured at all yet? */
	if (bpf->bpf_sbuf == NULL)
		return EINVAL;

	/*
	 * Does the read call size match the entire buffer size?  This is a
	 * ridiculous requirement but it makes our job quite a bit easier..
	 */
	if (size != bpf->bpf_size)
		return EINVAL;

	/*
	 * Following standard receive semantics, if the interface is gone,
	 * return all the packets that were pending before returning an error.
	 * This requires extra buffer rotations after read completion, too.
	 */
	if (bpf->bpf_ifdev == NULL && bpf->bpf_hlen == 0)
		return EIO;

	/*
	 * If immediate mode is not enabled, we should always suspend the read
	 * call if the hold buffer is empty.  If immediate mode is enabled, we
	 * should only suspend the read call if both buffers are empty, and
	 * return data from the hold buffer or otherwise the store buffer,
	 * whichever is not empty.  A non-blocking call behaves as though
	 * immediate mode is enabled, except it will return EAGAIN instead of
	 * suspending the read call if both buffers are empty.  Thus, we may
	 * have to rotate buffers for both immediate mode and non-blocking
	 * calls.  The latter is necessary for libpcap to behave correctly.
	 */
	if ((flags & CDEV_NONBLOCK) || (bpf->bpf_flags & BPFF_IMMEDIATE))
		suspend = (bpf->bpf_hlen == 0 && bpf->bpf_slen == 0);
	else
		suspend = (bpf->bpf_hlen == 0);

	if (suspend) {
		if (flags & CDEV_NONBLOCK)
			return EAGAIN;

		/* Suspend the read call for later. */
		bpf->bpf_read.br_endpt = endpt;
		bpf->bpf_read.br_grant = grant;
		bpf->bpf_read.br_id = id;

		/* Set a timer if requested. */
		if (bpf->bpf_timeout > 0)
			set_timer(&bpf->bpf_read.br_timer, bpf->bpf_timeout,
			    bpfdev_timeout, (int)(bpf - bpf_array));

		return EDONTREPLY;
	}

	/* If we get here, either buffer has data; rotate buffers if needed. */
	if (bpf->bpf_hlen == 0)
		bpfdev_rotate(bpf);
	assert(bpf->bpf_hlen > 0);

	if ((r = sys_safecopyto(endpt, grant, 0, (vir_bytes)bpf->bpf_hbuf,
	    bpf->bpf_hlen)) != OK)
		return r;

	r = (ssize_t)bpf->bpf_hlen;

	bpf->bpf_hlen = 0;

	/*
	 * If the store buffer is exactly full, rotate it now.  Also, if the
	 * interface has disappeared, the store buffer will never fill up.
	 * Rotate it so that the application will get any remaining data before
	 * getting errors about the interface being gone.
	 */
	if (bpf->bpf_slen == bpf->bpf_size || bpf->bpf_ifdev == NULL)
		bpfdev_rotate(bpf);

	return r;
}

/*
 * Write to a BPF device.
 */
static ssize_t
bpfdev_write(devminor_t minor, uint64_t position, endpoint_t endpt,
	cp_grant_id_t grant, size_t size, int flags, cdev_id_t id)
{
	struct bpfdev *bpf;
	struct pbuf *pbuf, *pptr, *pcopy;
	size_t off;
	err_t err;
	int r;

	if ((bpf = bpfdev_get_by_minor(minor)) == NULL)
		return EINVAL;

	if (bpf->bpf_ifdev == NULL)
		return EINVAL;

	/* VFS skips zero-sized I/O calls right now, but that may change. */
	if (size == 0)
		return 0;	/* nothing to do */

	if (size > ifdev_get_hdrlen(bpf->bpf_ifdev) +
	    ifdev_get_mtu(bpf->bpf_ifdev))
		return EMSGSIZE;

	if ((pbuf = pchain_alloc(PBUF_LINK, size)) == NULL)
		return ENOMEM;

	/* TODO: turn this into a series of vector copies. */
	off = 0;
	for (pptr = pbuf; pptr != NULL; pptr = pptr->next) {
		if ((r = sys_safecopyfrom(endpt, grant, off,
		    (vir_bytes)pptr->payload, pptr->len)) != OK) {
			pbuf_free(pbuf);

			return r;
		}
		off += pptr->len;
	}
	assert(off == size);

	/*
	 * In feedback mode, we cannot use the same packet buffers for both
	 * output and input, so make a copy.  We do this before calling the
	 * output function, which may change part of the buffers, because the
	 * BSDs take this approach as well.
	 */
	if (bpf->bpf_flags & BPFF_FEEDBACK) {
		if ((pcopy = pchain_alloc(PBUF_LINK, size)) == NULL) {
			pbuf_free(pbuf);

			return ENOMEM;
		}

		if (pbuf_copy(pcopy, pbuf) != ERR_OK)
			panic("unexpected pbuf copy failure");
	} else
		pcopy = NULL;

	/* Pass in the packet as output, and free it again. */
	err = ifdev_output(bpf->bpf_ifdev, pbuf, NULL /*netif*/,
	    TRUE /*to_bpf*/, !!(bpf->bpf_flags & BPFF_HDRCMPLT));

	pbuf_free(pbuf);

	/* In feedback mode, pass in the copy as input, if output succeeded. */
	if (err == ERR_OK && (bpf->bpf_flags & BPFF_FEEDBACK))
		ifdev_input(bpf->bpf_ifdev, pcopy, NULL /*netif*/,
		    FALSE /*to_bpf*/);
	else if (pcopy != NULL)
		pbuf_free(pcopy);

	return (err == ERR_OK) ? (ssize_t)size : util_convert_err(err);
}

/*
 * Attach a BPF device to a network interface, using the interface name given
 * in an ifreq structure.  As side effect, allocate hold and store buffers for
 * the device.  These buffers will stay allocated until the device is closed,
 * even though the interface may disappear before that.  Return OK if the BPF
 * device was successfully attached to the interface, or a negative error code
 * otherwise.
 */
static int
bpfdev_attach(struct bpfdev * bpf, struct ifreq * ifr)
{
	struct ifdev *ifdev;
	void *sbuf, *hbuf;

	/* Find the interface with the given name. */
	ifr->ifr_name[sizeof(ifr->ifr_name) - 1] = '\0';
	if ((ifdev = ifdev_find_by_name(ifr->ifr_name)) == NULL)
		return ENXIO;

	/*
	 * Allocate a store buffer and a hold buffer.  Preallocate the memory,
	 * or we might get killed later during low-memory conditions.
	 */
	if ((sbuf = (char *)mmap(NULL, bpf->bpf_size, PROT_READ | PROT_WRITE,
	    MAP_ANON | MAP_PRIVATE | MAP_PREALLOC, -1, 0)) == MAP_FAILED)
		return ENOMEM;

	if ((hbuf = (char *)mmap(NULL, bpf->bpf_size, PROT_READ | PROT_WRITE,
	    MAP_ANON | MAP_PRIVATE | MAP_PREALLOC, -1, 0)) == MAP_FAILED) {
		(void)munmap(sbuf, bpf->bpf_size);

		return ENOMEM;
	}

	bpf->bpf_ifdev = ifdev;
	bpf->bpf_sbuf = sbuf;
	bpf->bpf_hbuf = hbuf;
	assert(bpf->bpf_slen == 0);
	assert(bpf->bpf_hlen == 0);

	ifdev_attach_bpf(ifdev, &bpf->bpf_link);

	return OK;
}

/*
 * Detach the BPF device from its interface, which is about to disappear.
 */
void
bpfdev_detach(struct bpfdev_link * bpfl)
{
	struct bpfdev *bpf = (struct bpfdev *)bpfl;

	assert(bpf->bpf_flags & BPFF_IN_USE);
	assert(bpf->bpf_ifdev != NULL);

	/*
	 * We deliberately leave the buffers allocated here, for two reasons:
	 *
	 * 1) it lets applications to read any last packets in the buffers;
	 * 2) it prevents reattaching the BPF device to another interface.
	 */
	bpf->bpf_ifdev = NULL;

	/*
	 * Resume pending read and select requests, returning any data left,
	 * or an error if none.
	 */
	if (bpf->bpf_hlen == 0)
		bpfdev_rotate(bpf);

	if (bpf->bpf_read.br_endpt != NONE)
		bpfdev_resume_read(bpf, FALSE /*is_timeout*/);

	bpfdev_resume_select(bpf);
}

/*
 * Flush the given BPF device, resetting its buffer contents and statistics
 * counters.
 */
static void
bpfdev_flush(struct bpfdev * bpf)
{

	bpf->bpf_slen = 0;
	bpf->bpf_hlen = 0;

	bpf->bpf_stat.bs_recv = 0;
	bpf->bpf_stat.bs_drop = 0;
	bpf->bpf_stat.bs_capt = 0;
}

/*
 * Install a filter program on the BPF device.  A new filter replaces any old
 * one.  A zero-sized filter simply clears a previous filter.  On success,
 * perform a flush and return OK.  On failure, return a negative error code
 * without making any modifications to the current filter.
 */
static int
bpfdev_setfilter(struct bpfdev * bpf, endpoint_t endpt, cp_grant_id_t grant)
{
	struct bpf_insn *filter;
	unsigned int count;
	size_t len;
	int r;

	if ((r = sys_safecopyfrom(endpt, grant,
	    offsetof(struct minix_bpf_program, mbf_len), (vir_bytes)&count,
	    sizeof(count))) != OK)
		return r;

	if (count > BPF_MAXINSNS)
		return EINVAL;
	len = count * sizeof(struct bpf_insn);

	if (len > 0) {
		if ((filter = (struct bpf_insn *)mmap(NULL, len,
		    PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0)) ==
		    MAP_FAILED)
			return ENOMEM;

		if ((r = sys_safecopyfrom(endpt, grant,
		    offsetof(struct minix_bpf_program, mbf_insns),
		    (vir_bytes)filter, len)) != OK) {
			(void)munmap(filter, len);

			return r;
		}

		if (!bpf_validate(filter, count)) {
			(void)munmap(filter, len);

			return EINVAL;
		}
	} else
		filter = NULL;

	if (bpf->bpf_filter != NULL)
		(void)munmap(bpf->bpf_filter, bpf->bpf_filterlen);

	bpf->bpf_filter = filter;
	bpf->bpf_filterlen = len;

	bpfdev_flush(bpf);

	return OK;
}

/*
 * Process an I/O control request on the BPF device.
 */
static int
bpfdev_ioctl(devminor_t minor, unsigned long request, endpoint_t endpt,
	cp_grant_id_t grant, int flags, endpoint_t user_endpt, cdev_id_t id)
{
	struct bpfdev *bpf;
	struct bpf_stat bs;
	struct bpf_version bv;
	struct bpf_dltlist bfl;
	struct timeval tv;
	struct ifreq ifr;
	unsigned int uval;
	int r, val;

	if ((bpf = bpfdev_get_by_minor(minor)) == NULL)
		return EINVAL;

	/*
	 * We do not support multiple concurrent requests in this module.  That
	 * not only means that we forbid a read(2) call on a BPF device object
	 * while another read(2) is already pending: we also disallow IOCTL
	 * IOCTL calls while such a read(2) call is in progress.  This
	 * restriction should never be a problem for user programs, and allows
	 * us to rely on the fact that that no settings can change between the
	 * start and end of any read call.  As a side note, pending select(2)
	 * queries may be similarly affected, and will also not be fully
	 * accurate if any options are changed while pending.
	 */
	if (bpf->bpf_read.br_endpt != NONE)
		return EIO;

	bpf->bpf_pid = getnpid(user_endpt);

	/* These are in order of the NetBSD BIOC.. IOCTL numbers. */
	switch (request) {
	case BIOCGBLEN:
		uval = bpf->bpf_size;

		return sys_safecopyto(endpt, grant, 0, (vir_bytes)&uval,
		    sizeof(uval));

	case BIOCSBLEN:
		if (bpf->bpf_sbuf != NULL)
			return EINVAL;

		if ((r = sys_safecopyfrom(endpt, grant, 0, (vir_bytes)&uval,
		    sizeof(uval))) != OK)
			return r;

		if (uval < BPF_BUF_MIN)
			uval = BPF_BUF_MIN;
		else if (uval > BPF_BUF_MAX)
			uval = BPF_BUF_MAX;

		/* Is this the right thing to do?  It doesn't matter for us. */
		uval = BPF_WORDALIGN(uval);

		if ((r = sys_safecopyto(endpt, grant, 0, (vir_bytes)&uval,
		    sizeof(uval))) != OK)
			return r;

		bpf->bpf_size = uval;

		return OK;

	case MINIX_BIOCSETF:
		return bpfdev_setfilter(bpf, endpt, grant);

	case BIOCPROMISC:
		if (bpf->bpf_ifdev == NULL)
			return EINVAL;

		if (!(bpf->bpf_flags & BPFF_PROMISC)) {
			if (!ifdev_set_promisc(bpf->bpf_ifdev))
				return EINVAL;

			bpf->bpf_flags |= BPFF_PROMISC;
		}

		return OK;

	case BIOCFLUSH:
		bpfdev_flush(bpf);

		return OK;

	case BIOCGDLT:
		if (bpf->bpf_ifdev == NULL)
			return EINVAL;

		/* TODO: support for type configuration per BPF device. */
		uval = ifdev_get_dlt(bpf->bpf_ifdev);

		return sys_safecopyto(endpt, grant, 0, (vir_bytes)&uval,
		    sizeof(uval));

	case BIOCGETIF:
		if (bpf->bpf_ifdev == NULL)
			return EINVAL;

		memset(&ifr, 0, sizeof(ifr));
		strlcpy(ifr.ifr_name, ifdev_get_name(bpf->bpf_ifdev),
		    sizeof(ifr.ifr_name));

		return sys_safecopyto(endpt, grant, 0, (vir_bytes)&ifr,
		    sizeof(ifr));

	case BIOCSETIF:
		/*
		 * Test on the presence of a buffer rather than on an interface
		 * since the latter may disappear and thus be reset to NULL, in
		 * which case we do not want to allow rebinding to another.
		 */
		if (bpf->bpf_sbuf != NULL)
			return EINVAL;

		if ((r = sys_safecopyfrom(endpt, grant, 0, (vir_bytes)&ifr,
		    sizeof(ifr))) != OK)
			return r;

		return bpfdev_attach(bpf, &ifr);

	case BIOCGSTATS:
		/*
		 * Why do we not embed a bpf_stat structure directly in the
		 * BPF device structure?  Well, bpf_stat has massive padding..
		 */
		memset(&bs, 0, sizeof(bs));
		bs.bs_recv = bpf->bpf_stat.bs_recv;
		bs.bs_drop = bpf->bpf_stat.bs_drop;
		bs.bs_capt = bpf->bpf_stat.bs_capt;

		return sys_safecopyto(endpt, grant, 0, (vir_bytes)&bs,
		    sizeof(bs));

	case BIOCIMMEDIATE:
		if ((r = sys_safecopyfrom(endpt, grant, 0, (vir_bytes)&uval,
		    sizeof(uval))) != OK)
			return r;

		if (uval)
			bpf->bpf_flags |= BPFF_IMMEDIATE;
		else
			bpf->bpf_flags &= ~BPFF_IMMEDIATE;

		return OK;

	case BIOCVERSION:
		memset(&bv, 0, sizeof(bv));
		bv.bv_major = BPF_MAJOR_VERSION;
		bv.bv_minor = BPF_MINOR_VERSION;

		return sys_safecopyto(endpt, grant, 0, (vir_bytes)&bv,
		    sizeof(bv));

	case BIOCGHDRCMPLT:
		uval = !!(bpf->bpf_flags & BPFF_HDRCMPLT);

		return sys_safecopyto(endpt, grant, 0, (vir_bytes)&uval,
		    sizeof(uval));

	case BIOCSHDRCMPLT:
		if ((r = sys_safecopyfrom(endpt, grant, 0, (vir_bytes)&uval,
		    sizeof(uval))) != OK)
			return r;

		if (uval)
			bpf->bpf_flags |= BPFF_HDRCMPLT;
		else
			bpf->bpf_flags &= ~BPFF_HDRCMPLT;

		return OK;

	case BIOCSDLT:
		if (bpf->bpf_ifdev == NULL)
			return EINVAL;

		if ((r = sys_safecopyfrom(endpt, grant, 0, (vir_bytes)&uval,
		    sizeof(uval))) != OK)
			return r;

		/* TODO: support for type configuration per BPF device. */
		if (uval != ifdev_get_dlt(bpf->bpf_ifdev))
			return EINVAL;

		return OK;

	case MINIX_BIOCGDLTLIST:
		if (bpf->bpf_ifdev == NULL)
			return EINVAL;

		if ((r = sys_safecopyfrom(endpt, grant, 0, (vir_bytes)&bfl,
		    sizeof(bfl))) != OK)
			return r;

		if (bfl.bfl_list != NULL) {
			if (bfl.bfl_len < 1)
				return ENOMEM;

			/*
			 * Copy out the 'list', which consists of one entry.
			 * If we were to produce multiple entries, we would
			 * have to check against the MINIX_BPF_MAXDLT limit.
			 */
			uval = ifdev_get_dlt(bpf->bpf_ifdev);

			if ((r = sys_safecopyto(endpt, grant,
			    offsetof(struct minix_bpf_dltlist, mbfl_list),
			    (vir_bytes)&uval, sizeof(uval))) != OK)
				return r;
		}
		bfl.bfl_len = 1;

		return sys_safecopyto(endpt, grant, 0, (vir_bytes)&bfl,
		    sizeof(bfl));

	case BIOCGSEESENT:
		uval = !!(bpf->bpf_flags & BPFF_SEESENT);

		return sys_safecopyto(endpt, grant, 0, (vir_bytes)&uval,
		    sizeof(uval));

	case BIOCSSEESENT:
		if ((r = sys_safecopyfrom(endpt, grant, 0, (vir_bytes)&uval,
		    sizeof(uval))) != OK)
			return r;

		if (uval)
			bpf->bpf_flags |= BPFF_SEESENT;
		else
			bpf->bpf_flags &= ~BPFF_SEESENT;

		return OK;

	case BIOCSRTIMEOUT:
		if ((r = sys_safecopyfrom(endpt, grant, 0, (vir_bytes)&tv,
		    sizeof(tv))) != OK)
			return r;

		if ((r = util_timeval_to_ticks(&tv, &bpf->bpf_timeout)) != OK)
			return r;

		return OK;

	case BIOCGRTIMEOUT:
		util_ticks_to_timeval(bpf->bpf_timeout, &tv);

		return sys_safecopyto(endpt, grant, 0, (vir_bytes)&tv,
		    sizeof(tv));

	case BIOCGFEEDBACK:
		uval = !!(bpf->bpf_flags & BPFF_FEEDBACK);

		return sys_safecopyto(endpt, grant, 0, (vir_bytes)&uval,
		    sizeof(uval));

	case BIOCSFEEDBACK:
		if ((r = sys_safecopyfrom(endpt, grant, 0, (vir_bytes)&uval,
		    sizeof(uval))) != OK)
			return r;

		if (uval)
			bpf->bpf_flags |= BPFF_FEEDBACK;
		else
			bpf->bpf_flags &= ~BPFF_FEEDBACK;

		return OK;

	case FIONREAD:
		val = 0;
		if (bpf->bpf_hlen > 0)
			val = bpf->bpf_hlen;
		else if ((bpf->bpf_flags & BPFF_IMMEDIATE) &&
		    bpf->bpf_slen > 0)
			val = bpf->bpf_slen;
		else
			val = 0;

		return sys_safecopyto(endpt, grant, 0, (vir_bytes)&val,
		    sizeof(val));

	default:
		return ENOTTY;
	}
}

/*
 * Cancel a previously suspended request on a BPF device.  Since only read
 * requests may be suspended (select is handled differently), the cancel
 * request must be for a read request.  Note that character devices currently
 * (still) behave slightly differently from socket devices here: while socket
 * drivers are supposed to respond to the original request, character drivers
 * must respond to the original request from the cancel callback.
 */
static int
bpfdev_cancel(devminor_t minor, endpoint_t endpt, cdev_id_t id)
{
	struct bpfdev *bpf;

	if ((bpf = bpfdev_get_by_minor(minor)) == NULL)
		return EDONTREPLY;

	/* Is this a cancel request for the currently pending read request? */
	if (bpf->bpf_read.br_endpt != endpt || bpf->bpf_read.br_id != id)
		return EDONTREPLY;

	/* If so, cancel the read request. */
	if (bpf->bpf_timeout > 0)
		cancel_timer(&bpf->bpf_read.br_timer);

	bpf->bpf_read.br_endpt = NONE;

	return EINTR; /* the return value for the canceled read request */
}

/*
 * Perform a select query on a BPF device.
 */
static int
bpfdev_select(devminor_t minor, unsigned int ops, endpoint_t endpt)
{
	struct bpfdev *bpf;
	unsigned int r, notify;

	if ((bpf = bpfdev_get_by_minor(minor)) == NULL)
		return EINVAL;

	notify = (ops & CDEV_NOTIFY);
	ops &= (CDEV_OP_RD | CDEV_OP_WR | CDEV_OP_ERR);

	r = bpfdev_test_select(bpf, ops);

	/*
	 * For the operations that were not immediately ready, if requested,
	 * save the select request for later.
	 */
	ops &= ~r;

	if (ops != 0 && notify) {
		if (bpf->bpf_select.bs_endpt != NONE) {
			/* Merge in the operations with any earlier request. */
			if (bpf->bpf_select.bs_endpt != endpt)
				return EIO;
			bpf->bpf_select.bs_selops |= ops;
		} else {
			bpf->bpf_select.bs_endpt = endpt;
			bpf->bpf_select.bs_selops = ops;
		}
	}

	return r;
}

/*
 * Process an incoming packet on the interface to which the given BPF device is
 * attached.  If the packet passes the filter (if any), store as much as
 * requested of it in the store buffer, rotating buffers if needed and resuming
 * suspended read and select requests as appropriate.  This function is also
 * called through bpfdev_output() below.
 */
void
bpfdev_input(struct bpfdev_link * bpfl, const struct pbuf * pbuf)
{
	struct bpfdev *bpf = (struct bpfdev *)bpfl;
	struct timespec ts;
	struct bpf_hdr bh;
	const struct pbuf *pptr;
	size_t caplen, hdrlen, totlen, off, chunk;
	int hfull;

	/*
	 * Apparently bs_recv is the counter of packets that were run through
	 * the filter, not the number of packets that were or could be received
	 * by the user (which is what I got from the manual page.. oh well).
	 */
	bpf->bpf_stat.bs_recv++;
	bpf_stat.bs_recv++;

	/*
	 * Run the packet through the BPF device's filter to see whether the
	 * packet should be stored and if so, how much of it.  If no filter is
	 * set, all packets will be stored in their entirety.
	 */
	caplen = bpf_filter_ext(bpf->bpf_filter, pbuf, (u_char *)pbuf->payload,
	    pbuf->tot_len, pbuf->len);

	if (caplen == 0)
		return;		/* no match; ignore packet */

	if (caplen > pbuf->tot_len)
		caplen = pbuf->tot_len;

	/* Truncate packet entries to the full size of the buffers. */
	hdrlen = BPF_WORDALIGN(sizeof(bh));
	totlen = BPF_WORDALIGN(hdrlen + caplen);

	if (totlen > bpf->bpf_size) {
		totlen = bpf->bpf_size;
		caplen = totlen - hdrlen;
	}
	assert(totlen >= hdrlen);

	bpf->bpf_stat.bs_capt++;
	bpf_stat.bs_capt++;

	assert(bpf->bpf_sbuf != NULL);
	if (totlen > bpf->bpf_size - bpf->bpf_slen) {
		/*
		 * If the store buffer is full and the hold buffer is not
		 * empty, we cannot swap the two buffers, and so we must drop
		 * the current packet.
		 */
		if (bpf->bpf_hlen > 0) {
			bpf->bpf_stat.bs_drop++;
			bpf_stat.bs_drop++;

			return;
		}

		/*
		 * Rotate the buffers: the hold buffer will now be "full" and
		 * ready to be read - it may not actually be entirely full, but
		 * we could not fit this packet and we are not going to deliver
		 * packets out of order..
		 */
		bpfdev_rotate(bpf);

		hfull = TRUE;
	} else
		hfull = FALSE;

	/*
	 * Retrieve the capture time for the packet.  Ideally this would be
	 * done only once per accepted packet, but we do not expect many BPF
	 * devices to be receiving the same packets often enough to make that
	 * worth it.
	 */
	clock_time(&ts);

	/*
	 * Copy the packet into the store buffer, including a newly generated
	 * header.  Zero any padding areas, even if strictly not necessary.
	 */
	memset(&bh, 0, sizeof(bh));
	bh.bh_tstamp.tv_sec = ts.tv_sec;
	bh.bh_tstamp.tv_usec = ts.tv_nsec / 1000;
	bh.bh_caplen = caplen;
	bh.bh_datalen = pbuf->tot_len;
	bh.bh_hdrlen = hdrlen;

	assert(bpf->bpf_sbuf != NULL);
	off = bpf->bpf_slen;

	memcpy(&bpf->bpf_sbuf[off], &bh, sizeof(bh));
	if (hdrlen > sizeof(bh))
		memset(&bpf->bpf_sbuf[off + sizeof(bh)], 0,
		    hdrlen - sizeof(bh));
	off += hdrlen;

	for (pptr = pbuf; pptr != NULL && caplen > 0; pptr = pptr->next) {
		chunk = pptr->len;
		if (chunk > caplen)
			chunk = caplen;

		memcpy(&bpf->bpf_sbuf[off], pptr->payload, chunk);

		off += chunk;
		caplen -= chunk;
	}

	assert(off <= bpf->bpf_slen + totlen);
	if (bpf->bpf_slen + totlen > off)
		memset(&bpf->bpf_sbuf[off], 0, bpf->bpf_slen + totlen - off);

	bpf->bpf_slen += totlen;

	/*
	 * Edge case: if the hold buffer is empty and the store buffer is now
	 * exactly full, rotate buffers so that the packets can be read
	 * immediately, without waiting for the next packet to cause rotation.
	 */
	if (bpf->bpf_hlen == 0 && bpf->bpf_slen == bpf->bpf_size) {
		bpfdev_rotate(bpf);

		hfull = TRUE;
	}

	/*
	 * If the hold buffer is now full, or if immediate mode is enabled,
	 * then we now have data to deliver to userland.  See if we can wake up
	 * any read or select call (either but not both here).
	 */
	if (hfull || (bpf->bpf_flags & BPFF_IMMEDIATE)) {
		if (bpf->bpf_read.br_endpt != NONE)
			bpfdev_resume_read(bpf, FALSE /*is_timeout*/);
		else
			bpfdev_resume_select(bpf);
	}
}

/*
 * Process an outgoing packet on the interface to which the given BPF device is
 * attached.  If the BPF device is configured to capture outgoing packets as
 * well, attempt to capture the packet as per bpfdev_input().
 */
void
bpfdev_output(struct bpfdev_link * bpfl, const struct pbuf * pbuf)
{
	struct bpfdev *bpf = (struct bpfdev *)bpfl;

	if (bpf->bpf_flags & BPFF_SEESENT)
		bpfdev_input(bpfl, pbuf);
}

/*
 * Fill the given 'bde' structure with information about BPF device 'bpf'.
 */
static void
bpfdev_get_info(struct bpf_d_ext * bde, const struct bpfdev * bpf)
{

	bde->bde_bufsize = bpf->bpf_size;
	bde->bde_promisc = !!(bpf->bpf_flags & BPFF_PROMISC);
	bde->bde_state = BPF_IDLE;
	bde->bde_immediate = !!(bpf->bpf_flags & BPFF_IMMEDIATE);
	bde->bde_hdrcmplt = !!(bpf->bpf_flags & BPFF_HDRCMPLT);
	bde->bde_seesent = !!(bpf->bpf_flags & BPFF_SEESENT);
	/*
	 * NetBSD updates the process ID upon device open, close, ioctl, and
	 * poll.  From those, only open and ioctl make sense for us.  Sadly
	 * there is no way to indicate "no known PID" to netstat(1), so we
	 * cannot even save just the endpoint and look up the corresponding PID
	 * later, since the user process may be gone by then.
	 */
	bde->bde_pid = bpf->bpf_pid;
	bde->bde_rcount = bpf->bpf_stat.bs_recv;
	bde->bde_dcount = bpf->bpf_stat.bs_drop;
	bde->bde_ccount = bpf->bpf_stat.bs_capt;
	if (bpf->bpf_ifdev != NULL)
		strlcpy(bde->bde_ifname, ifdev_get_name(bpf->bpf_ifdev),
		    sizeof(bde->bde_ifname));
}

/*
 * Obtain statistics about open BPF devices ("peers").  This node may be
 * accessed by the superuser only.  Used by netstat(1).
 */
static ssize_t
bpfdev_peers(struct rmib_call * call, struct rmib_node * node __unused,
	struct rmib_oldp * oldp, struct rmib_newp * newp __unused)
{
	struct bpfdev *bpf;
	struct bpf_d_ext bde;
	unsigned int slot;
	ssize_t off;
	int r, size, max;

	if (!(call->call_flags & RMIB_FLAG_AUTH))
		return EPERM;

	if (call->call_namelen != 2)
		return EINVAL;

	size = call->call_name[0];
	if (size < 0 || (size_t)size > sizeof(bde))
		return EINVAL;
	if (size == 0)
		size = sizeof(bde);
	max = call->call_name[1];

	off = 0;

	for (slot = 0; slot < __arraycount(bpf_array); slot++) {
		bpf = &bpf_array[slot];

		if (!(bpf->bpf_flags & BPFF_IN_USE))
			continue;

		if (rmib_inrange(oldp, off)) {
			memset(&bde, 0, sizeof(bde));

			bpfdev_get_info(&bde, bpf);

			if ((r = rmib_copyout(oldp, off, &bde, size)) < 0)
				return r;
		}

		off += sizeof(bde);
		if (max > 0 && --max == 0)
			break;
	}

	/* No slack needed: netstat(1) resizes its buffer as needed. */
	return off;
}

static const struct chardriver bpfdev_tab = {
	.cdr_open		= bpfdev_open,
	.cdr_close		= bpfdev_close,
	.cdr_read		= bpfdev_read,
	.cdr_write		= bpfdev_write,
	.cdr_ioctl		= bpfdev_ioctl,
	.cdr_cancel		= bpfdev_cancel,
	.cdr_select		= bpfdev_select
};

/*
 * Process a character driver request.  Since the LWIP service offers character
 * devices for BPF only, it must be a request for a BPF device.
 */
void
bpfdev_process(message * m_ptr, int ipc_status)
{

	chardriver_process(&bpfdev_tab, m_ptr, ipc_status);
}
