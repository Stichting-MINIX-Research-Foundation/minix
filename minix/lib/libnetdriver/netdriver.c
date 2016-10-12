/* The device-independent network driver framework. */

#include <minix/drivers.h>
#include <minix/netdriver.h>
#include <minix/ds.h>
#include <assert.h>

#include "netdriver.h"

/*
 * These maximum values should be at least somewhat synchronized with the
 * values in the LWIP service's ndev module.
 */
#define NETDRIVER_SENDQ_MAX	8
#define NETDRIVER_RECVQ_MAX	2

/*
 * Maximum number of multicast addresses that can be copied in from the TCP/IP
 * service and passed to the driver.  If the actual number from the service
 * exceeds this maximum, the driver will be told to receive all multicast
 * packets instead.
 */
#define NETDRIVER_MCAST_MAX	16

static const struct netdriver *netdriver_table = NULL;

static int running;

static int init_expected;

static int up;

static unsigned int ticks;

static struct netdriver_data pending_sendq[NETDRIVER_SENDQ_MAX];
static unsigned int pending_sends, pending_sendtail;

static struct netdriver_data pending_recvq[NETDRIVER_RECVQ_MAX];
static unsigned int pending_recvs, pending_recvtail;

static int pending_status;
static endpoint_t status_endpt;

static int pending_link, pending_stat;
static uint32_t stat_oerror, stat_coll, stat_ierror, stat_iqdrop;

static char device_name[NDEV_NAME_MAX];
static netdriver_addr_t device_hwaddr;
static uint32_t device_caps;

static unsigned int device_link;
static uint32_t device_media;

/*
 * Announce we are up after a fresh start or restart.
 */
static void
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

	if ((r = asynsend(endpt, m_ptr)) != OK)
		panic("netdriver: unable to send to %d: %d", endpt, r);
}

/*
 * A packet receive request has finished.  Send a reply and clean up.
 */
static void
finish_recv(int32_t result)
{
	struct netdriver_data *data;
	message m;

	assert(pending_recvs > 0);

	data = &pending_recvq[pending_recvtail];

	memset(&m, 0, sizeof(m));
	m.m_type = NDEV_RECV_REPLY;
	m.m_netdriver_ndev_reply.id = data->id;
	m.m_netdriver_ndev_reply.result = result;

	send_reply(data->endpt, &m);

	pending_recvtail = (pending_recvtail + 1) %
	    __arraycount(pending_recvq);
	pending_recvs--;
}

/*
 * Resume receiving packets.  In particular, if a receive request was pending,
 * call the driver's receive function.  If the call is successful, send a reply
 * to the requesting party.
 */
void
netdriver_recv(void)
{
	struct netdriver_data *data;
	ssize_t r;

	assert(netdriver_table != NULL);

	while (pending_recvs > 0) {
		data = &pending_recvq[pending_recvtail];

		/*
		 * For convenience of driver writers: if the receive function
		 * returns zero, simply call it again, to simplify discarding
		 * invalid packets.
		 */
		do {
			r = netdriver_table->ndr_recv(data, data->size);

			/*
			 * The default policy is: drop undersized packets,
			 * panic on oversized packets.  The driver may
			 * implement any other policy (e.g., pad small packets,
			 * drop or truncate large packets), but it should at
			 * least test against the given 'max' value.  The
			 * reason that truncation should be implemented in the
			 * driver rather than here, is explained in an earlier
			 * comment about truncating copy operations.
			 */
			if (r >= 0 && r < NDEV_ETH_PACKET_MIN)
				r = 0;
			else if (r > (ssize_t)data->size)
				panic("netdriver: oversized packet returned: "
				    "%zd", r);
		} while (r == 0);

		if (r == SUSPEND)
			break;

		if (r < 0)
			panic("netdriver: driver reported receive failure: %d",
			    r);

		assert(r >= NDEV_ETH_PACKET_MIN && (size_t)r <= data->size);

		finish_recv(r);
	}
}

/*
 * A packet send request has finished.  Send a reply and clean up.
 */
static void
finish_send(int32_t result)
{
	struct netdriver_data *data;
	message m;

	assert(pending_sends > 0);

	data = &pending_sendq[pending_sendtail];

	memset(&m, 0, sizeof(m));
	m.m_type = NDEV_SEND_REPLY;
	m.m_netdriver_ndev_reply.id = data->id;
	m.m_netdriver_ndev_reply.result = result;

	send_reply(data->endpt, &m);

	pending_sendtail = (pending_sendtail + 1) %
	    __arraycount(pending_sendq);
	pending_sends--;
}

/*
 * Resume sending packets.  In particular, if any send requests were pending,
 * call the driver's send function for each of them, until the driver can take
 * no more.  For each successful request is successful, send a reply to the
 * requesting party.
 */
void
netdriver_send(void)
{
	struct netdriver_data *data;
	int r;

	assert(netdriver_table != NULL);

	while (pending_sends > 0) {
		data = &pending_sendq[pending_sendtail];

		r = netdriver_table->ndr_send(data, data->size);

		if (r == SUSPEND)
			break;

		if (r < 0)
			panic("netdriver: driver reported send failure: %d",
			    r);

		finish_send(r);
	}
}

/*
 * Process a request to send or receive a packet.
 */
static void
do_transfer(const struct netdriver * __restrict ndp, const message * m_ptr,
	int do_write)
{
	struct netdriver_data *data;
	cp_grant_id_t grant;
	size_t size;
	unsigned int i;

	/* Prepare the local data structure. */
	if (do_write) {
		if (pending_sends == __arraycount(pending_sendq))
			panic("netdriver: too many concurrent send requests");

		data = &pending_sendq[(pending_sendtail + pending_sends) %
		    __arraycount(pending_sendq)];
	} else {
		if (pending_recvs == __arraycount(pending_recvq))
			panic("netdriver: too many concurrent receive "
			    "requests");

		data = &pending_recvq[(pending_recvtail + pending_recvs) %
		    __arraycount(pending_recvq)];
	}

	data->endpt = m_ptr->m_source;
	data->id = m_ptr->m_ndev_netdriver_transfer.id;
	data->count = m_ptr->m_ndev_netdriver_transfer.count;

	if (data->count == 0 || data->count > NDEV_IOV_MAX)
		panic("netdriver: bad I/O vector count: %u", data->count);

	data->size = 0;

	for (i = 0; i < data->count; i++) {
		grant = m_ptr->m_ndev_netdriver_transfer.grant[i];
		size = (size_t)m_ptr->m_ndev_netdriver_transfer.len[i];

		assert(size > 0);

		data->iovec[i].iov_grant = grant;
		data->iovec[i].iov_size = size;
		data->size += size;
	}

	if (data->size < NDEV_ETH_PACKET_MIN ||
	    (!do_write && data->size < NDEV_ETH_PACKET_MAX_TAGGED))
		panic("netdriver: invalid I/O vector size: %zu\n", data->size);

	if (do_write)
		pending_sends++;
	else
		pending_recvs++;

	/*
	 * If the driver is down, immediately abort the request again.  This
	 * is not a common case but does occur as part of queue draining by the
	 * TCP/IP stack, and is way easier to handle here than up there..
	 */
	if (!up) {
		if (do_write)
			finish_send(EINTR);
		else
			finish_recv(EINTR);

		return;
	}

	/* Otherwise, resume sending or receiving. */
	if (do_write)
		netdriver_send();
	else
		netdriver_recv();
}

/*
 * Process a request to (re)configure the driver.
 */
static void
do_conf(const struct netdriver * __restrict ndp,
	const message * __restrict m_ptr)
{
	netdriver_addr_t mcast_list[NETDRIVER_MCAST_MAX];
	uint32_t set, mode;
	unsigned int mcast_count;
	message m;
	int r;

	set = m_ptr->m_ndev_netdriver_conf.set;
	mode = m_ptr->m_ndev_netdriver_conf.mode;

	/*
	 * If the request includes taking down the interface, perform that step
	 * first: it is expected that in many cases, changing other settings
	 * requires stopping and restarting the device.
	 */
	if ((set & NDEV_SET_MODE) && mode == NDEV_MODE_DOWN &&
	    ndp->ndr_set_mode != NULL)
		ndp->ndr_set_mode(mode, NULL, 0);

	if ((set & NDEV_SET_CAPS) && ndp->ndr_set_caps != NULL)
		ndp->ndr_set_caps(m_ptr->m_ndev_netdriver_conf.caps);

	if ((set & NDEV_SET_FLAGS) && ndp->ndr_set_flags != NULL)
		ndp->ndr_set_flags(m_ptr->m_ndev_netdriver_conf.flags);

	if ((set & NDEV_SET_MEDIA) && ndp->ndr_set_media != NULL)
		ndp->ndr_set_media(m_ptr->m_ndev_netdriver_conf.media);

	if ((set & NDEV_SET_HWADDR) && ndp->ndr_set_hwaddr != NULL) {
		/* Save the new hardware address. */
		memcpy(&device_hwaddr, m_ptr->m_ndev_netdriver_conf.hwaddr,
		    sizeof(device_hwaddr));

		ndp->ndr_set_hwaddr(&device_hwaddr);
	}

	if ((set & NDEV_SET_MODE) && mode != NDEV_MODE_DOWN &&
	    ndp->ndr_set_mode != NULL) {
		/*
		 * If we have a multicast list, copy it in, unless it is too
		 * large: in that case, enable all-multicast receipt mode.
		 */
		if ((mode & NDEV_MODE_MCAST_LIST) &&
		    m_ptr->m_ndev_netdriver_conf.mcast_count >
		    __arraycount(mcast_list)) {
			mode &= ~NDEV_MODE_MCAST_LIST;
			mode |= NDEV_MODE_MCAST_ALL;
		}

		if (mode & NDEV_MODE_MCAST_LIST) {
			assert(m_ptr->m_ndev_netdriver_conf.mcast_grant !=
			    GRANT_INVALID);

			mcast_count = m_ptr->m_ndev_netdriver_conf.mcast_count;

			if ((r = sys_safecopyfrom(m_ptr->m_source,
			    m_ptr->m_ndev_netdriver_conf.mcast_grant, 0,
			    (vir_bytes)mcast_list,
			    mcast_count * sizeof(mcast_list[0]))) != OK)
				panic("netdriver: unable to copy data: %d", r);

			ndp->ndr_set_mode(mode, mcast_list, mcast_count);
		} else
			ndp->ndr_set_mode(mode, NULL, 0);
	}

	/* We always report OK: the caller cannot do anything upon failure. */
	memset(&m, 0, sizeof(m));
	m.m_type = NDEV_CONF_REPLY;
	m.m_netdriver_ndev_reply.id = m_ptr->m_ndev_netdriver_conf.id;
	m.m_netdriver_ndev_reply.result = OK;

	send_reply(m_ptr->m_source, &m);

	/*
	 * Finally, if the device has been taken down, abort pending send and
	 * receive requests.
	 */
	if (set & NDEV_SET_MODE) {
		if (mode == NDEV_MODE_DOWN) {
			while (pending_sends > 0)
				finish_send(EINTR);

			while (pending_recvs > 0)
				finish_recv(EINTR);

			up = FALSE;
		} else
			up = TRUE;
	}
}

/*
 * Request an update of the link state and active media of the device.  This
 * routine may be called both from the driver and internally.
 */
static void
update_link(void)
{

	if (netdriver_table->ndr_get_link != NULL)
		device_link = netdriver_table->ndr_get_link(&device_media);

	pending_link = FALSE;
}

/*
 * Attempt to send a status update to the endpoint registered to receive status
 * updates, if any.
 */
static void
send_status(void)
{
	message m;
	int r;

	assert(pending_link || pending_stat);

	if (status_endpt == NONE || pending_status)
		return;

	if (pending_link)
		update_link();

	memset(&m, 0, sizeof(m));
	m.m_type = NDEV_STATUS;
	m.m_netdriver_ndev_status.id = 0;	/* for now */
	m.m_netdriver_ndev_status.link = device_link;
	m.m_netdriver_ndev_status.media = device_media;
	m.m_netdriver_ndev_status.oerror = stat_oerror;
	m.m_netdriver_ndev_status.coll = stat_coll;
	m.m_netdriver_ndev_status.ierror = stat_ierror;
	m.m_netdriver_ndev_status.iqdrop = stat_iqdrop;

	if ((r = asynsend3(status_endpt, &m, AMF_NOREPLY)) != OK)
		panic("netdriver: unable to send status: %d", r);

	/*
	 * Do not send another status message until either the one we just sent
	 * gets acknowledged or we get a new initialization request.  This way
	 * we get "natural pacing" (i.e., we avoid overflowing the asynsend
	 * message queue by design) without using timers.
	 */
	pending_status = TRUE;

	/*
	 * The status message sends incremental updates for statistics.  This
	 * means that while a restart of the TCP/IP stack means the statistics
	 * are lost (not great), a restart of the driver leaves the statistics
	 * mostly intact (more important).
	 */
	stat_oerror = 0;
	stat_coll = 0;
	stat_ierror = 0;
	stat_iqdrop = 0;
	pending_stat = FALSE;
}

/*
 * Process a reply to a status update that we sent earlier on (supposedly).
 */
static void
do_status_reply(const struct netdriver * __restrict ndp __unused,
	const message * __restrict m_ptr)
{

	if (m_ptr->m_source != status_endpt)
		return;

	if (!pending_status || m_ptr->m_ndev_netdriver_status_reply.id != 0)
		panic("netdriver: unexpected status reply");

	pending_status = FALSE;

	/*
	 * If the local status has changed since our last status update,
	 * send a new one right away.
	 */
	if (pending_link || pending_stat)
		send_status();
}

/*
 * The driver reports that the link state and/or active media may have changed.
 * When convenient, request the new state from the driver and send a status
 * message to the TCP/IP stack.
 */
void
netdriver_link(void)
{

	pending_link = TRUE;

	send_status();
}

/*
 * The driver reports that a number of output errors have occurred.  Update
 * statistics accordingly.
 */
void
netdriver_stat_oerror(uint32_t count)
{

	if (count == 0)
		return;

	stat_oerror += count;
	pending_stat = TRUE;

	send_status();
}

/*
 * The driver reports that one or more packet collisions have occurred.  Update
 * statistics accordingly.
 */
void
netdriver_stat_coll(uint32_t count)
{

	if (count == 0)
		return;

	stat_coll += count;
	pending_stat = TRUE;

	send_status();
}

/*
 * The driver reports that a number of input errors have occurred.  Adjust
 * statistics accordingly.
 */
void
netdriver_stat_ierror(uint32_t count)
{

	if (count == 0)
		return;

	stat_ierror += count;
	pending_stat = TRUE;

	send_status();
}

/*
 * The driver reports that a number of input queue drops have occurred.  Update
 * statistics accordingly.
 */
void
netdriver_stat_iqdrop(uint32_t count)
{

	if (count == 0)
		return;

	stat_iqdrop += count;
	pending_stat = TRUE;

	send_status();
}

/*
 * Process an initialization request.  Actual initialization has already taken
 * place, so we simply report the information gathered at that time.  If the
 * caller (the TCP/IP stack) has crashed and restarted, we will get another
 * initialization request message, so keep the information up-to-date.
 */
static void
do_init(const struct netdriver * __restrict ndp,
	const message * __restrict m_ptr)
{
	message m;

	/*
	 * First of all, an initialization request is a sure indication that
	 * the caller does not have any send or receive requests pending, and
	 * will not acknowledge our previous status request, if any.  Forget
	 * any such previous requests and start sending status requests to the
	 * (new) endpoint.
	 */
	pending_sends = 0;
	pending_recvs = 0;
	pending_status = FALSE;

	status_endpt = m_ptr->m_source;

	/*
	 * Update link and media now, because we are about to send the initial
	 * values of those to the caller as well.
	 */
	update_link();

	memset(&m, 0, sizeof(m));
	m.m_type = NDEV_INIT_REPLY;
	m.m_netdriver_ndev_init_reply.id = m_ptr->m_ndev_netdriver_init.id;
	m.m_netdriver_ndev_init_reply.link = device_link;
	m.m_netdriver_ndev_init_reply.media = device_media;
	m.m_netdriver_ndev_init_reply.caps = device_caps;
	strlcpy(m.m_netdriver_ndev_init_reply.name, device_name,
	    sizeof(m.m_netdriver_ndev_init_reply.name));
	assert(sizeof(device_hwaddr) <=
	    sizeof(m.m_netdriver_ndev_init_reply.hwaddr));
	memcpy(m.m_netdriver_ndev_init_reply.hwaddr, &device_hwaddr,
	    sizeof(device_hwaddr));
	m.m_netdriver_ndev_init_reply.hwaddr_len = sizeof(device_hwaddr);

	m.m_netdriver_ndev_init_reply.max_send = __arraycount(pending_sendq);
	m.m_netdriver_ndev_init_reply.max_recv = __arraycount(pending_recvq);

	send_reply(m_ptr->m_source, &m);

	/*
	 * Also send the current status.  This is not required by the protocol
	 * and only serves to provide updated statistics to a new TCP/IP stack
	 * instance right away.
	 */
	if (pending_stat)
		send_status();
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
		switch (m_ptr->m_source) {
		case HARDWARE:
			if (ndp->ndr_intr != NULL)
				ndp->ndr_intr(m_ptr->m_notify.interrupts);
			break;

		case CLOCK:
			if (ndp->ndr_tick != NULL)
				ndp->ndr_tick();

			if (ticks > 0)
				(void)sys_setalarm(ticks, FALSE /*abs_time*/);
			break;

		default:
			if (ndp->ndr_other != NULL)
				ndp->ndr_other(m_ptr, ipc_status);
		}

		return;
	}

	/*
	 * Discard datalink requests preceding a first NDEV_INIT request, so
	 * that after a driver restart, any in-flight request is discarded.
	 * Note that for correct driver operation it is important that
	 * non-datalink requests, and interrupts in particular, do not go
	 * through this check.
	 */
	if (IS_NDEV_RQ(m_ptr->m_type) && init_expected) {
		if (m_ptr->m_type != NDEV_INIT)
			return; /* do not send a reply */

		init_expected = FALSE;
	}

	switch (m_ptr->m_type) {
	case NDEV_INIT:
		do_init(ndp, m_ptr);
		break;

	case NDEV_CONF:
		do_conf(ndp, m_ptr);
		break;

	case NDEV_SEND:
		do_transfer(ndp, m_ptr, TRUE /*do_write*/);
		break;

	case NDEV_RECV:
		do_transfer(ndp, m_ptr, FALSE /*do_write*/);
		break;

	case NDEV_STATUS_REPLY:
		do_status_reply(ndp, m_ptr);
		break;

	default:
		if (ndp->ndr_other != NULL)
			ndp->ndr_other(m_ptr, ipc_status);
	}
}

/*
 * Set a name for the device, based on the base name 'base' and the instance
 * number 'instance'.
 */
static void
netdriver_set_name(const char * base, unsigned int instance)
{
	size_t len;

	assert(instance <= 255);

	len = strlen(base);
	assert(len <= sizeof(device_name) - 4);

	memcpy(device_name, base, len);
	if (instance >= 100)
		device_name[len++] = '0' + instance / 100;
	if (instance >= 10)
		device_name[len++] = '0' + (instance % 100) / 10;
	device_name[len++] = '0' + instance % 10;
	device_name[len] = 0;
}

/*
 * Return the device name generated at driver initialization time.
 */
const char *
netdriver_name(void)
{

	return device_name;
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
	pending_sendtail = 0;
	pending_sends = 0;
	pending_recvtail = 0;
	pending_recvs = 0;

	memset(device_name, 0, sizeof(device_name));

	memset(&device_hwaddr, 0, sizeof(device_hwaddr));
	device_caps = 0;

	/* Use sensible defaults for the link state and active media. */
	device_link = NDEV_LINK_UNKNOWN;
	device_media = IFM_MAKEWORD(IFM_ETHER, IFM_AUTO, 0, 0);

	status_endpt = NONE;
	pending_status = FALSE;
	pending_link = FALSE;

	up = FALSE;

	ticks = 0;

	/* Get the device instance number. */
	v = 0;
	(void)env_parse("instance", "d", 0, &v, 0, 255);
	instance = (unsigned int)v;

	/* Generate the full driver name. */
	netdriver_set_name(ndp->ndr_name, instance);

	/* Call the initialization routine. */
	if ((r = ndp->ndr_init(instance, &device_hwaddr, &device_caps,
	    &ticks)) != OK)
		return r;

	/* Announce we are up! */
	netdriver_announce();

	init_expected = TRUE;
	running = TRUE;

	if (ticks > 0)
		(void)sys_setalarm(ticks, FALSE /*abs_time*/);

	return OK;
}

/*
 * Perform SEF initialization.
 */
static int
local_init(int type __unused, sef_init_info_t * info __unused)
{

	assert(netdriver_table != NULL);

	return netdriver_init(netdriver_table);
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
	sef_setcb_init_fresh(local_init);
	sef_setcb_signal_handler(got_signal);

	netdriver_table = ndp;

	sef_startup();

	/* The main message loop. */
	while (running) {
		if ((r = sef_receive_status(ANY, &mess, &ipc_status)) != OK) {
			if (r == EINTR)
				continue;	/* sef_cancel() was called */

			panic("netdriver: sef_receive_status failed: %d", r);
		}

		netdriver_process(ndp, &mess, ipc_status);
	}
}
