/* The device-independent network driver framework. */

#include <minix/drivers.h>
#include <minix/endpoint.h>
#include <minix/netdriver.h>
#include <minix/ds.h>
#include <assert.h>

#include "netdriver.h"

static const struct netdriver *netdriver_table = NULL;

static int running;

static int conf_expected;

static endpoint_t pending_endpt;
static struct netdriver_data pending_recv, pending_send;

static int defer_reply;
static unsigned int pending_flags;
static size_t pending_size;

static ether_addr_t hw_addr;

/*
 * Announce we are up after a fresh start or restart.
 */
void
netdriver_announce(void)
{
	const char *driver_prefix = "drv.net.";
	char label[DS_MAX_KEYLEN];
	char key[DS_MAX_KEYLEN];
	int r;

	/* Publish a driver up event. */
	if ((r = ds_retrieve_label_name(label, sef_self())) != OK)
		panic("netdriver: unable to get own label: %d", r);

	snprintf(key, sizeof(key), "%s%s", driver_prefix, label);
	if ((r = ds_publish_u32(key, DS_DRIVER_UP, DSF_OVERWRITE)) != OK)
		panic("netdriver: unable to publish driver up event: %d", r);
}

/*
 * Prepare for copying.  Given a flat offset, return the vector element index
 * and an offset into that element.  Panic if the request does not fall
 * entirely within the vector.
 */
size_t
netdriver_prepare_copy(struct netdriver_data * data, size_t off, size_t size,
	unsigned int * indexp)
{
	unsigned int i;

	assert(data->size > 0);

	/*
	 * In theory we could truncate when copying out, but this creates a
	 * problem for port-based I/O, where the size of the transfer is
	 * typically specified in advance.  We could do extra port-based I/O
	 * to discard the extra bytes, but the driver is better off doing such
	 * truncation itself.  Thus, we disallow copying (in and out) beyond
	 * the given data vector altogether.
	 */
	if (off + size > data->size)
		panic("netdriver: request to copy beyond data size");

	/*
	 * Find the starting offset in the vector.  If this turns out to be
	 * expensive, this can be adapted to store the last <element,offset>
	 * pair in the "data" structure (this is the reason it is not 'const').
	 */
	for (i = 0; i < data->count; i++) {
		assert(data->iovec[i].iov_size > 0);

		if (off >= data->iovec[i].iov_size)
			off -= data->iovec[i].iov_size;
		else
			break;
	}

	assert(i < data->count);

	*indexp = i;
	return off;
}

/*
 * Copy in or out packet data from/to a vector of grants.
 */
static void
netdriver_copy(struct netdriver_data * data, size_t off, vir_bytes addr,
	size_t size, int copyin)
{
	struct vscp_vec vec[SCPVEC_NR];
	size_t chunk;
	unsigned int i, v;
	int r;

	off = netdriver_prepare_copy(data, off, size, &i);

	/* Generate a new vector with all the individual copies to make. */
	for (v = 0; size > 0; v++) {
		chunk = data->iovec[i].iov_size - off;
		if (chunk > size)
			chunk = size;
		assert(chunk > 0);

		/*
		 * We should be able to fit the entire I/O request in a single
		 * copy vector.  If not, MINIX3 has been misconfigured.
		 */
		if (v >= SCPVEC_NR)
			panic("netdriver: invalid vector size constant");

		if (copyin) {
			vec[v].v_from = data->endpt;
			vec[v].v_to = SELF;
		} else {
			vec[v].v_from = SELF;
			vec[v].v_to = data->endpt;
		}
		vec[v].v_gid = data->iovec[i].iov_grant;
		vec[v].v_offset = off;
		vec[v].v_addr = addr;
		vec[v].v_bytes = chunk;

		i++;
		off = 0;
		addr += chunk;
		size -= chunk;
	}

	assert(v > 0 && v <= SCPVEC_NR);

	/*
	 * If only one vector element was generated, use a direct copy.  This
	 * saves the kernel from having to copy in the vector.
	 */
	if (v == 1) {
		if (copyin)
			r = sys_safecopyfrom(vec->v_from, vec->v_gid,
			    vec->v_offset, vec->v_addr, vec->v_bytes);
		else
			r = sys_safecopyto(vec->v_to, vec->v_gid,
			    vec->v_offset, vec->v_addr, vec->v_bytes);
	} else
		r = sys_vsafecopy(vec, v);

	if (r != OK)
		panic("netdriver: unable to copy data: %d", r);
}

/*
 * Copy in packet data.
 */
void
netdriver_copyin(struct netdriver_data * __restrict data, size_t off,
	void * __restrict ptr, size_t size)
{

	netdriver_copy(data, off, (vir_bytes)ptr, size, TRUE /*copyin*/);
}

/*
 * Copy out packet data.
 */
void
netdriver_copyout(struct netdriver_data * __restrict data, size_t off,
	const void * __restrict ptr, size_t size)
{

	netdriver_copy(data, off, (vir_bytes)ptr, size, FALSE /*copyin*/);
}

/*
 * Send a reply to a request.
 */
static void
send_reply(endpoint_t endpt, message * m_ptr)
{
	int r;

	if ((r = ipc_send(endpt, m_ptr)) != OK)
		panic("netdriver: unable to send to %d: %d", endpt, r);
}

/*
 * Defer sending any replies to task requests until the next call to
 * check_replies().  The purpose of this is aggregation of task replies to both
 * send and receive requests into a single reply message, which saves on
 * messages, in particular when processing interrupts.
 */
static void
defer_replies(void)
{

	assert(netdriver_table != NULL);
	assert(defer_reply == FALSE);

	defer_reply = TRUE;
}

/*
 * Check if we have to reply to earlier task (I/O) requests, and if so, send
 * the reply.  If deferred is FALSE and the call to this function was preceded
 * by a call to defer_replies(), do not send a reply yet.  If always_send is
 * TRUE, send a reply even if no tasks have completed yet.
 */
static void
check_replies(int deferred, int always_send)
{
	message m_reply;

	if (defer_reply && !deferred)
		return;

	defer_reply = FALSE;

	if (pending_flags == 0 && !always_send)
		return;

	assert(pending_endpt != NONE);

	memset(&m_reply, 0, sizeof(m_reply));
	m_reply.m_type = DL_TASK_REPLY;
	m_reply.m_netdrv_net_dl_task.flags = pending_flags;
	m_reply.m_netdrv_net_dl_task.count = pending_size;

	send_reply(pending_endpt, &m_reply);

	pending_flags = 0;
	pending_size = 0;
}

/*
 * Resume receiving packets.  In particular, if a receive request was pending,
 * call the driver's receive function.  If the call is successful, schedule
 * sending a reply to the requesting party.
 */
void
netdriver_recv(void)
{
	ssize_t r;

	if (pending_recv.size == 0)
		return;

	assert(netdriver_table != NULL);

	/*
	 * For convenience of driver writers: if the receive function returns
	 * zero, simply call it again, to simplify discarding invalid packets.
	 */
	do {
		r = netdriver_table->ndr_recv(&pending_recv,
		    pending_recv.size);

		/*
		 * The default policy is: drop undersized packets, panic on
		 * oversized packets.  The driver may implement any other
		 * policy (e.g., pad small packets, drop or truncate large
		 * packets), but it should at least test against the given
		 * 'max' value.  The reason that truncation should be
		 * implemented in the driver rather than here, is explained in
		 * an earlier comment about truncating copy operations.
		 */
		if (r >= 0 && r < ETH_MIN_PACK_SIZE)
			r = 0;
		else if (r > (ssize_t)pending_recv.size)
			panic("netdriver: oversized packet returned: %zd", r);
	} while (r == 0);

	if (r == SUSPEND)
		return;
	if (r < 0)
		panic("netdriver: driver reported receive failure: %d", r);

	assert(r >= ETH_MIN_PACK_SIZE && (size_t)r <= pending_recv.size);

	pending_flags |= DL_PACK_RECV;
	pending_size = r;

	pending_recv.size = 0;

	check_replies(FALSE /*deferred*/, FALSE /*always_send*/);
}

/*
 * Resume sending packets.  In particular, if a send request was pending, call
 * the driver's send function.  If the call is successful, schedule sending a
 * reply to the requesting party.  This function relies on being called
 * between init_pending() and check_pending().
 */
void
netdriver_send(void)
{
	int r;

	if (pending_send.size == 0)
		return;

	assert(netdriver_table != NULL);

	r = netdriver_table->ndr_send(&pending_send, pending_send.size);

	if (r == SUSPEND)
		return;
	if (r < 0)
		panic("netdriver: driver reported send failure: %d", r);

	pending_flags |= DL_PACK_SEND;

	pending_send.size = 0;

	check_replies(FALSE /*deferred*/, FALSE /*always_send*/);
}

/*
 * Process a request to receive or send a packet.
 */
static void
do_readwrite(const struct netdriver * __restrict ndp, endpoint_t endpt,
	cp_grant_id_t grant, unsigned int count, int write)
{
	struct netdriver_data *data;
	unsigned int i;
	int r;

	/* Copy in the I/O vector. */
	data = (write) ? &pending_send : &pending_recv;

	if (data->size != 0)
		panic("netdriver: multiple concurrent requests");

	if (count == 0 || count > NR_IOREQS)
		panic("netdriver: bad I/O vector count: %u", count);

	data->endpt = endpt;
	data->count = count;

	if ((r = sys_safecopyfrom(endpt, grant, 0, (vir_bytes)data->iovec,
	    sizeof(data->iovec[0]) * count)) != OK)
		panic("netdriver: unable to copy in I/O vector: %d", r);

	for (i = 0; i < count; i++)
		data->size += data->iovec[i].iov_size;

	if (data->size < ETH_MIN_PACK_SIZE ||
	    (!write && data->size < ETH_MAX_PACK_SIZE_TAGGED))
		panic("netdriver: invalid I/O vector size: %zu\n", data->size);

	/* Save the endpoint to which we should reply. */
	if (pending_endpt != NONE && pending_endpt != endpt)
		panic("netdriver: multiple request sources");
	pending_endpt = endpt;

	/* Resume sending or receiving. */
	defer_replies();

	if (write)
		netdriver_send();
	else
		netdriver_recv();

	/* Always send a reply in this case, even if no flags are set. */
	check_replies(TRUE /*deferred*/, TRUE /*always_send*/);
}

/*
 * Process a request to configure the driver, by setting its mode and obtaining
 * its ethernet hardware address.  We already have the latter as a result of
 * calling the ndr_init callback function.
 */
static void
do_conf(const struct netdriver * __restrict ndp,
	const message * __restrict m_ptr)
{
	message m_reply;

	if (ndp->ndr_mode != NULL)
		ndp->ndr_mode(m_ptr->m_net_netdrv_dl_conf.mode);

	memset(&m_reply, 0, sizeof(m_reply));
	m_reply.m_type = DL_CONF_REPLY;
	m_reply.m_netdrv_net_dl_conf.stat = OK; /* legacy */
	memcpy(&m_reply.m_netdrv_net_dl_conf.hw_addr, &hw_addr,
	    sizeof(m_reply.m_netdrv_net_dl_conf.hw_addr));

	send_reply(m_ptr->m_source, &m_reply);
}

/*
 * Process a request to obtain statistics from the driver.
 */
static void
do_getstat(const struct netdriver * __restrict ndp,
	const message * __restrict m_ptr)
{
	message m_reply;
	eth_stat_t stat;
	int r;

	memset(&stat, 0, sizeof(stat));

	if (ndp->ndr_stat != NULL)
		ndp->ndr_stat(&stat);

	if ((r = sys_safecopyto(m_ptr->m_source,
	    m_ptr->m_net_netdrv_dl_getstat_s.grant, 0, (vir_bytes)&stat,
	    sizeof(stat))) != OK)
		panic("netdriver: unable to copy out statistics: %d", r);

	memset(&m_reply, 0, sizeof(m_reply));
	m_reply.m_type = DL_STAT_REPLY;

	send_reply(m_ptr->m_source, &m_reply);
}

/*
 * Process an incoming message, and send a reply.
 */
void
netdriver_process(const struct netdriver * __restrict ndp,
	const message * __restrict m_ptr, int ipc_status)
{

	netdriver_table = ndp;

	/* Check for notifications first. */
	if (is_ipc_notify(ipc_status)) {
		defer_replies();

		switch (_ENDPOINT_P(m_ptr->m_source)) {
		case HARDWARE:
			if (ndp->ndr_intr != NULL)
				ndp->ndr_intr(m_ptr->m_notify.interrupts);
			break;

		case CLOCK:
			if (ndp->ndr_alarm != NULL)
				ndp->ndr_alarm(m_ptr->m_notify.timestamp);
			break;

		default:
			if (ndp->ndr_other != NULL)
				ndp->ndr_other(m_ptr, ipc_status);
		}

		/*
		 * Any of the above calls may end up invoking netdriver_send()
		 * and/or netdriver_recv(), which may in turn have deferred
		 * sending a reply to an earlier request.  See if we have to
		 * send the reply now.
		 */
		check_replies(TRUE /*deferred*/, FALSE /*always_send*/);
	}

	/*
	 * Discard datalink requests preceding a first DL_CONF request, so that
	 * after a driver restart, any in-flight request is discarded.  This is
	 * a rather blunt approach and must be revised if the protocol is ever
	 * made less inefficient (i.e. not strictly serialized).  Note that for
	 * correct driver operation it is important that non-datalink requests,
	 * interrupts in particular, do not go through this check.
	 */
	if (IS_DL_RQ(m_ptr->m_type) && conf_expected) {
		if (m_ptr->m_type != DL_CONF)
			return; /* do not send a reply */

		conf_expected = FALSE;
	}

	switch (m_ptr->m_type) {
	case DL_CONF:
		do_conf(ndp, m_ptr);
		break;

	case DL_GETSTAT_S:
		do_getstat(ndp, m_ptr);
		break;

	case DL_READV_S:
		do_readwrite(ndp, m_ptr->m_source,
		    m_ptr->m_net_netdrv_dl_readv_s.grant,
		    m_ptr->m_net_netdrv_dl_readv_s.count, FALSE /*write*/);
		break;

	case DL_WRITEV_S:
		do_readwrite(ndp, m_ptr->m_source,
		    m_ptr->m_net_netdrv_dl_writev_s.grant,
		    m_ptr->m_net_netdrv_dl_writev_s.count, TRUE /*write*/);
		break;

	default:
		defer_replies();

		if (ndp->ndr_other != NULL)
			ndp->ndr_other(m_ptr, ipc_status);

		/* As above: see if we have to send a reply now. */
		check_replies(TRUE /*deferred*/, FALSE /*always_send*/);
	}
}

/*
 * Perform initialization.  Return OK or an error code.
 */
int
netdriver_init(const struct netdriver * ndp)
{
	unsigned int instance;
	long v;
	int r;

	/* Initialize global variables. */
	pending_recv.size = 0;
	pending_send.size = 0;
	pending_endpt = NONE;
	defer_reply = FALSE;
	pending_flags = 0;
	pending_size = 0;
	conf_expected = TRUE;

	/* Get the card instance number. */
	v = 0;
	(void)env_parse("instance", "d", 0, &v, 0, 255);
	instance = (unsigned int)v;

	/* Call the initialization routine. */
	memset(&hw_addr, 0, sizeof(hw_addr));

	if (ndp->ndr_init != NULL &&
	    (r = ndp->ndr_init(instance, &hw_addr)) != OK)
		return r;

	/* Announce we are up! */
	netdriver_announce();

	return OK;
}

/*
 * SEF initialization function.
 */
static int
do_init(int __unused type, sef_init_info_t * __unused info)
{
	const struct netdriver *ndp;

	ndp = netdriver_table;
	assert(ndp != NULL);

	return netdriver_init(ndp);
}

/*
 * Break out of the main loop after finishing the current request.
 */
void
netdriver_terminate(void)
{

	if (netdriver_table != NULL && netdriver_table->ndr_stop != NULL)
		netdriver_table->ndr_stop();

	running = FALSE;

	sef_cancel();
}

/*
 * The process has received a signal.  See if we have to terminate.
 */
static void
got_signal(int sig)
{

	if (sig != SIGTERM)
		return;

	netdriver_terminate();
}

/*
 * Main program of any network driver.
 */
void
netdriver_task(const struct netdriver * ndp)
{
	message mess;
	int r, ipc_status;

	/* Perform SEF initialization. */
	sef_setcb_init_fresh(do_init);
	sef_setcb_init_restart(do_init);	/* TODO: revisit this */
	sef_setcb_signal_handler(got_signal);

	netdriver_table = ndp;

	sef_startup();

	netdriver_table = NULL;

	/* The main message loop. */
	running = TRUE;

	while (running) {
		if ((r = sef_receive_status(ANY, &mess, &ipc_status)) != OK) {
			if (r == EINTR)
				continue;	/* sef_cancel() was called */

			panic("netdriver: sef_receive_status failed: %d", r);
		}

		netdriver_process(ndp, &mess, ipc_status);
	}
}
