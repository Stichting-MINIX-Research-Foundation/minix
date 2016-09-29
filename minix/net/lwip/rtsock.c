/* LWIP service - rtsock.c - routing sockets and route sysctl support */
/*
 * In a nutshell, the intended abstraction is that only this module deals with
 * route messages, message headers, and RTA arrays, whereas other modules
 * (ifaddr, route) are responsible for parsing and providing sockaddr_* type
 * addresses, with the exception of compression and expansion which is
 * particular to routing sockets.  Concretely, there should be no reference to
 * (e.g.) rt_msghdr outside this module, and no mention of ip_addr_t inside it.
 */

#include "lwip.h"
#include "ifaddr.h"
#include "rtsock.h"
#include "route.h"
#include "lldata.h"

/* The number of routing sockets. */
#define NR_RTSOCK		8

/*
 * The send buffer maximum determines the maximum size of requests.  The
 * maximum possible request size is the size of the routing message header plus
 * RTAX_MAX times the maximum socket address size, including alignment.  That
 * currently works out to a number in the low 400s, so 512 should be fine for
 * now.  At this time we do not support changing the send buffer size, because
 * there really is no point in doing so.  Hence also no RT_SNDBUF_{MIN,DEF}.
 */
#define RT_SNDBUF_MAX		512	/* maximum RT send buffer size */

#define RT_RCVBUF_MIN		0	/* minimum RT receive buffer size */
#define RT_RCVBUF_DEF		16384	/* default RT receive buffer size */
#define RT_RCVBUF_MAX		65536	/* maximum RT receive buffer size */

/* Address length of routing socket address structures; two bytes only. */
#define RTSOCK_ADDR_LEN		offsetof(struct sockaddr, sa_data)

struct rtsock_rta {
	const void *rta_ptr[RTAX_MAX];
	socklen_t rta_len[RTAX_MAX];
};

static const char rtsock_padbuf[RT_ROUNDUP(0)];

static struct rtsock {
	struct sock rt_sock;		/* socket object, MUST be first */
	int rt_family;			/* address family filter if not zero */
	unsigned int rt_flags;		/* routing socket flags (RTF_) */
	struct pbuf *rt_rcvhead;	/* receive buffer, first packet */
	struct pbuf **rt_rcvtailp;	/* receive buffer, last ptr-ptr */
	size_t rt_rcvlen;		/* receive buffer, length in bytes */
	size_t rt_rcvbuf;		/* receive buffer, maximum size */
	TAILQ_ENTRY(rtsock) rt_next;	/* next in active or free list */
} rt_array[NR_RTSOCK];

#define RTF_NOLOOPBACK		0x1	/* suppress reply messages */

static TAILQ_HEAD(, rtsock) rt_freelist;	/* free routing sockets */
static TAILQ_HEAD(, rtsock) rt_activelist;	/* active routing sockets */

struct rtsock_request {
	struct rtsock *rtr_src;		/* source socket of the request */
	pid_t rtr_pid;			/* process ID of requesting process */
	int rtr_seq;			/* sequence number from the request */
	int rtr_getif;			/* RTM_GET only: get interface info */
};

static const struct sockevent_ops rtsock_ops;

static ssize_t rtsock_info(struct rmib_call *, struct rmib_node *,
	struct rmib_oldp *, struct rmib_newp *);

/* The CTL_NET PF_ROUTE subtree. */
static struct rmib_node net_route_table[] = {
	[0]	= RMIB_FUNC(RMIB_RO | CTLTYPE_NODE, 0, rtsock_info,
		    "rtable", "Routing table information"),
};

/* The CTL_NET PF_ROUTE node. */
static struct rmib_node net_route_node =
    RMIB_NODE(RMIB_RO, net_route_table, "route", "PF_ROUTE information");

/*
 * Initialize the routing sockets module.
 */
void
rtsock_init(void)
{
	const int mib[] = { CTL_NET, PF_ROUTE };
	unsigned int slot;
	int r;

	/* Initialize the list of free routing sockets. */
	TAILQ_INIT(&rt_freelist);

	for (slot = 0; slot < __arraycount(rt_array); slot++)
		TAILQ_INSERT_TAIL(&rt_freelist, &rt_array[slot], rt_next);

	/* Initialize the list of acive routing sockets. */
	TAILQ_INIT(&rt_activelist);

	/* Register the "net.route" subtree with the MIB service. */
	if ((r = rmib_register(mib, __arraycount(mib), &net_route_node)) != OK)
		panic("unable to register net.route RMIB tree: %d", r);
}

/*
 * Allocate a pbuf suitable for storing a routing message of 'size' bytes.
 * Return the allocated pbuf on success, or NULL on memory allocation failure.
 */
static struct pbuf *
rtsock_alloc(size_t size)
{
	struct pbuf *pbuf;

	/*
	 * The data will currently always fit in a single pool buffer.  Just in
	 * case this changes in the future, warn and fail cleanly.  The rest of
	 * the code is not able to deal with buffer chains as it is, although
	 * that can be changed if necessary.
	 */
	if (size > MEMPOOL_BUFSIZE) {
		printf("LWIP: routing socket packet too large (%zu)\n", size);

		return NULL;
	}

	pbuf = pbuf_alloc(PBUF_RAW, size, PBUF_RAM);

	assert(pbuf == NULL || pbuf->tot_len == pbuf->len);

	return pbuf;
}

/*
 * Initialize a routing addresses map.
 */
static void
rtsock_rta_init(struct rtsock_rta * rta)
{

	memset(rta, 0, sizeof(*rta));
}

/*
 * Set an entry in a routing addresses map.  When computing sizes, 'ptr' may be
 * NULL.
 */
static void
rtsock_rta_set(struct rtsock_rta * rta, unsigned int rtax, const void * ptr,
	socklen_t len)
{

	assert(rtax < RTAX_MAX);

	rta->rta_ptr[rtax] = ptr;
	rta->rta_len[rtax] = len;
}

/*
 * Copy out a message with a header and any entries in a routing addresses map,
 * either into a pbuf allocated for this purpose, or to a RMIB (sysctl) caller,
 * at the given offset.  If no destination is given ('pbuf ' and 'oldp' are
 * both NULL), compute just the size of the resulting data.  Otherwise, set the
 * length and address mask fields in the header as a side effect.  Return the
 * number of bytes copied on success, and if 'pbuf' is not NULL, it is filled
 * with a pointer to the newly allocated pbuf.  Return a negative error code on
 * failure.  Note that when computing the size only, any actual data pointers
 * ('hdr', 'msglen', 'addrs', and the pointers in 'rta') may be NULL or even
 * invalid, even though the corresponding sizes should still be supplied.
 */
static ssize_t
rtsock_rta_finalize(void * hdr, size_t hdrlen, u_short * msglen, int * addrs,
	const struct rtsock_rta * rta, struct pbuf ** pbuf,
	struct rmib_oldp * oldp, ssize_t off)
{
	iovec_t iov[1 + RTAX_MAX * 2];
	size_t len, padlen, totallen;
	unsigned int i, iovcnt;
	int mask;

	assert(pbuf == NULL || oldp == NULL);
	assert(pbuf == NULL || off == 0);
	assert(RT_ROUNDUP(hdrlen) == hdrlen);

	iov[0].iov_addr = (vir_bytes)hdr;
	iov[0].iov_size = hdrlen;
	iovcnt = 1;

	totallen = hdrlen;
	mask = 0;

	/*
	 * The addresses in the given RTA map, as present, should be stored in
	 * the numbering order of the map.
	 */
	for (i = 0; i < RTAX_MAX; i++) {
		if (rta->rta_ptr[i] == NULL)
			continue;

		if ((len = rta->rta_len[i]) > 0) {
			assert(iovcnt < __arraycount(iov));
			iov[iovcnt].iov_addr = (vir_bytes)rta->rta_ptr[i];
			iov[iovcnt++].iov_size = len;
		}

		/* Note that RT_ROUNDUP(0) is not 0.. */
		if ((padlen = RT_ROUNDUP(len) - len) > 0) {
			assert(iovcnt < __arraycount(iov));
			iov[iovcnt].iov_addr = (vir_bytes)rtsock_padbuf;
			iov[iovcnt++].iov_size = padlen;
		}

		totallen += len + padlen;
		mask |= (1 << i);	/* convert RTAX_ to RTA_ */
	}

	/* If only the length was requested, return it now. */
	if (pbuf == NULL && oldp == NULL)
		return totallen;

	/*
	 * Casting 'hdr' would violate C99 strict aliasing rules, but the
	 * address mask is not always at the same location anyway.
	 */
	*msglen = totallen;
	*addrs = mask;

	if (pbuf != NULL) {
		if ((*pbuf = rtsock_alloc(totallen)) == NULL)
			return ENOMEM;

		return util_coalesce((char *)(*pbuf)->payload, totallen, iov,
		    iovcnt);
	} else
		return rmib_vcopyout(oldp, off, iov, iovcnt);
}

/*
 * Reduce the size of a network mask to the bytes actually used.  It is highly
 * doubtful that this extra complexity pays off in any form, but it is what the
 * BSDs historically do.  We currently implement compression for IPv4 only.
 */
static void
rtsock_compress_netmask(struct sockaddr * sa)
{
	struct sockaddr_in sin;
	uint32_t addr;

	if (sa->sa_family != AF_INET)
		return; /* nothing to do */

	memcpy(&sin, sa, sizeof(sin));	/* no type punning.. (sigh) */

	addr = htonl(sin.sin_addr.s_addr);

	if (addr & 0x000000ff)
		sa->sa_len = 8;
	else if (addr & 0x0000ffff)
		sa->sa_len = 7;
	else if (addr & 0x00ffffff)
		sa->sa_len = 6;
	else if (addr != 0)
		sa->sa_len = 5;
	else
		sa->sa_len = 0;
}

/*
 * Expand a possibly compressed IPv4 or IPv6 network mask, given as 'sa', into
 * 'mask'.  Return TRUE if expansion succeeded.  In that case, the resulting
 * mask must have sa.sa_len and sa.sa_family filled in correctly, and have the
 * appropriate size for its address family.  Return FALSE if expansion failed
 * and an error should be returned to the caller.
 */
static int
rtsock_expand_netmask(union sockaddr_any * mask, const struct sockaddr * sa)
{

	if (sa->sa_len > sizeof(*mask))
		return FALSE;

	memset(mask, 0, sizeof(*mask));
	memcpy(mask, sa, sa->sa_len);

	/*
	 * Amazingly, even the address family may be chopped off, in which case
	 * an IPv4 address is implied.
	 */
	if (sa->sa_len >= offsetof(struct sockaddr, sa_data) &&
	    sa->sa_family == AF_INET6) {
		if (sa->sa_len > sizeof(struct sockaddr_in6))
			return FALSE;

		mask->sa.sa_len = sizeof(struct sockaddr_in6);
		mask->sa.sa_family = AF_INET6;
	} else {
		if (sa->sa_len > sizeof(struct sockaddr_in))
			return FALSE;

		mask->sa.sa_len = sizeof(struct sockaddr_in);
		mask->sa.sa_family = AF_INET;
	}

	return TRUE;
}

/*
 * Create a routing socket.
 */
sockid_t
rtsock_socket(int type, int protocol, struct sock ** sockp,
	const struct sockevent_ops ** ops)
{
	struct rtsock *rt;

	/*
	 * There is no superuser check here: regular users are allowed to issue
	 * (only) RTM_GET requests on routing sockets.
	 */
	if (type != SOCK_RAW)
		return EPROTOTYPE;

	/* We could accept only the protocols we know, but this is fine too. */
	if (protocol < 0 || protocol >= AF_MAX)
		return EPROTONOSUPPORT;

	if (TAILQ_EMPTY(&rt_freelist))
		return ENOBUFS;

	rt = TAILQ_FIRST(&rt_freelist);
	TAILQ_REMOVE(&rt_freelist, rt, rt_next);

	rt->rt_flags = 0;
	rt->rt_family = protocol;
	rt->rt_rcvhead = NULL;
	rt->rt_rcvtailp = &rt->rt_rcvhead;
	rt->rt_rcvlen = 0;
	rt->rt_rcvbuf = RT_RCVBUF_DEF;

	TAILQ_INSERT_HEAD(&rt_activelist, rt, rt_next);

	*sockp = &rt->rt_sock;
	*ops = &rtsock_ops;
	return SOCKID_RT | (sockid_t)(rt - rt_array);
}

/*
 * Enqueue data on the receive queue of a routing socket.  The caller must have
 * checked whether the receive buffer size allows for the receipt of the data.
 */
static void
rtsock_enqueue(struct rtsock * rt, struct pbuf * pbuf)
{

	*rt->rt_rcvtailp = pbuf;
	rt->rt_rcvtailp = pchain_end(pbuf);
	rt->rt_rcvlen += pchain_size(pbuf);

	sockevent_raise(&rt->rt_sock, SEV_RECV);
}

/*
 * Determine whether a routing message for address family 'family', originated
 * from routing socket 'rtsrc' if not NULL, should be sent to routing socket
 * 'rt'.  Return TRUE if the message should be sent to this socket, or FALSE
 * if it should not.
 */
static int
rtsock_can_send(struct rtsock *rt, struct rtsock *rtsrc, int family)
{

	/* Do not send anything on sockets shut down for reading. */
	if (sockevent_is_shutdown(&rt->rt_sock, SFL_SHUT_RD))
		return FALSE;

	/*
	 * Do not send a reply message to the source of the request if the
	 * source is not interested in replies to its own requests.
	 */
	if (rt == rtsrc && (rt->rt_flags & RTF_NOLOOPBACK))
		return FALSE;

	/*
	 * For address family specific messages, make sure the routing socket
	 * is interested in that family.  Make an exception if the socket was
	 * the source of the request, though: we currently do not prevent user
	 * processes from issuing commands for the "wrong" family.
	 */
	if (rt->rt_family != AF_UNSPEC && family != AF_UNSPEC &&
	    rt->rt_family != family && rt != rtsrc)
		return FALSE;

	/*
	 * See whether the receive queue of the socket is already full.  We do
	 * not consider the size of the current request, in order to not drop
	 * larger messages and then enqueue smaller ones.
	 */
	if (rt->rt_rcvlen >= rt->rt_rcvbuf)
		return FALSE;

	/* All is well: go on and deliver the message. */
	return TRUE;
}

/*
 * Send the routing message in 'pbuf' to the given routing socket if possible,
 * or check whether such a message could be sent to that socket if 'pbuf' is
 * NULL.  In the former case, the function takes ownership of 'pbuf'.  The
 * given routing socket is assumed to be the source of the routing request that
 * generated this message.  In the latter case, the function returns TRUE if
 * the socket would take the message or FALSE if not.  If 'family' is not
 * AF_UNSPEC, it is to be the address family of the message.
 */
static int
rtsock_msg_one(struct rtsock * rt, int family, struct pbuf * pbuf)
{

	if (rtsock_can_send(rt, rt, family)) {
		if (pbuf != NULL)
			rtsock_enqueue(rt, pbuf);

		return TRUE;
	} else {
		if (pbuf != NULL)
			pbuf_free(pbuf);

		return FALSE;
	}
}

/*
 * Send the routing message in 'pbuf' to all matching routing sockets, or check
 * whether there are any such matching routing sockets if 'pbuf' is NULL.  In
 * the former case, the function takes ownership of 'pbuf'.  In the latter
 * case, the function returns TRUE if there are any matching sockets or FALSE
 * if there are none.  If 'rtsrc' is not NULL, it is to be the routing socket
 * that is the source of the message.  If 'family' is not AF_UNSPEC, it is to
 * be the address family of the message.
 */
static int
rtsock_msg_match(struct rtsock * rtsrc, int family, struct pbuf * pbuf)
{
	struct rtsock *rt, *rtprev;
	struct pbuf *pcopy;

	rtprev = NULL;

	TAILQ_FOREACH(rt, &rt_activelist, rt_next) {
		if (!rtsock_can_send(rt, rtsrc, family))
			continue;

		/*
		 * There is at least one routing socket that is interested in
		 * receiving this message, and able to receive it.
		 */
		if (pbuf == NULL)
			return TRUE;

		/*
		 * We need to make copies of the generated message for all but
		 * the last matching socket, which gets the original.  If we're
		 * out of memory, free the original and stop: there are more
		 * important things to spend memory on than routing sockets.
		 */
		if (rtprev != NULL) {
			if ((pcopy = rtsock_alloc(pbuf->tot_len)) == NULL) {
				pbuf_free(pbuf);

				return TRUE;
			}

			if (pbuf_copy(pcopy, pbuf) != ERR_OK)
				panic("unexpected pbuf copy failure");

			rtsock_enqueue(rtprev, pcopy);
		}

		rtprev = rt;
	}

	if (rtprev != NULL)
		rtsock_enqueue(rtprev, pbuf);
	else if (pbuf != NULL)
		pbuf_free(pbuf);

	return (rtprev != NULL);
}

/*
 * Dequeue and free the head of the receive queue of a routing socket.
 */
static void
rtsock_dequeue(struct rtsock * rt)
{
	struct pbuf *pbuf, **pnext;
	size_t size;

	pbuf = rt->rt_rcvhead;
	assert(pbuf != NULL);

	pnext = pchain_end(pbuf);
	size = pchain_size(pbuf);

	if ((rt->rt_rcvhead = *pnext) == NULL)
		rt->rt_rcvtailp = &rt->rt_rcvhead;

	assert(rt->rt_rcvlen >= size);
	rt->rt_rcvlen -= size;

	*pnext = NULL;
	pbuf_free(pbuf);
}

/*
 * Process a routing message sent on a socket.  Return OK on success, in which
 * case the caller assumes that the processing routine has sent a reply to the
 * user and possibly other routing sockets.  Return a negative error code on
 * failure, in which case the caller will send the reply to the user instead.
 */
static int
rtsock_process(struct rtsock *rt, struct rt_msghdr * rtm, char * buf,
	size_t len, int is_root)
{
	struct rtsock_request rtr;
	struct rtsock_rta rta;
	const struct sockaddr *netmask;
	struct sockaddr sa;
	union sockaddr_any mask;
	size_t off;
	int i;

	if (rtm->rtm_msglen != len)
		return EINVAL;

	if (rtm->rtm_version != RTM_VERSION) {
		printf("LWIP: PID %d uses routing sockets version %u\n",
			rtm->rtm_pid, rtm->rtm_version);

		return EPROTONOSUPPORT;
	}

	/*
	 * Make sure that we won't misinterpret the rest of the message.  While
	 * looking at the message type, also make sure non-root users can only
	 * ever issue RTM_GET requests.
	 */
	switch (rtm->rtm_type) {
	case RTM_ADD:
	case RTM_DELETE:
	case RTM_CHANGE:
	case RTM_LOCK:
		if (!is_root)
			return EPERM;

		/* FALLTHROUGH */
	case RTM_GET:
		break;

	default:
		return EOPNOTSUPP;
	}

	/*
	 * Extract all given addresses.  We do not actually support all types
	 * of entries, but we cannot skip the ones we do not need either.
	 */
	rtsock_rta_init(&rta);

	off = sizeof(*rtm);
	assert(off == RT_ROUNDUP(off));

	for (i = 0; i < RTAX_MAX; i++) {
		if (!(rtm->rtm_addrs & (1 << i)))
			continue;

		if (off + offsetof(struct sockaddr, sa_data) > len)
			return EINVAL;

		/*
		 * It is safe to access sa_len and even sa_family in all cases,
		 * in particular even when the structure is of size zero.
		 */
		assert(offsetof(struct sockaddr, sa_data) <= RT_ROUNDUP(0));

		memcpy(&sa, &buf[off], offsetof(struct sockaddr, sa_data));

		if (off + sa.sa_len > len)
			return EINVAL;

		rtsock_rta_set(&rta, i, &buf[off], sa.sa_len);

		off += RT_ROUNDUP((size_t)sa.sa_len);
	}

	/*
	 * Expand the given netmask if it is in compressed IPv4 form.  We do
	 * this here because it is particular to routing sockets; we also do
	 * the compression in this module.  Note how the compression may even
	 * strip off the address family; really, who came up with this ****?
	 */
	netmask = (const struct sockaddr *)rta.rta_ptr[RTAX_NETMASK];

	if (netmask != NULL) {
		if (!rtsock_expand_netmask(&mask, netmask))
			return EINVAL;

		rtsock_rta_set(&rta, RTAX_NETMASK, &mask, mask.sa.sa_len);
	}

	/*
	 * Actually process the command.  Pass on enough information so that a
	 * reply can be generated on success.  The abstraction as sketched at
	 * the top of the file imposes that we pass quite a few parameters.
	 */
	rtr.rtr_src = rt;
	rtr.rtr_pid = rtm->rtm_pid;
	rtr.rtr_seq = rtm->rtm_seq;
	rtr.rtr_getif = (rtm->rtm_type == RTM_GET &&
	    (rta.rta_ptr[RTAX_IFP] != NULL || rta.rta_ptr[RTAX_IFA] != NULL));

	return route_process(rtm->rtm_type,
	    (const struct sockaddr *)rta.rta_ptr[RTAX_DST],
	    (const struct sockaddr *)rta.rta_ptr[RTAX_NETMASK],
	    (const struct sockaddr *)rta.rta_ptr[RTAX_GATEWAY],
	    (const struct sockaddr *)rta.rta_ptr[RTAX_IFP],
	    (const struct sockaddr *)rta.rta_ptr[RTAX_IFA],
	    rtm->rtm_flags, rtm->rtm_inits, &rtm->rtm_rmx, &rtr);
}

/*
 * Perform preliminary checks on a send request.
 */
static int
rtsock_pre_send(struct sock * sock __unused, size_t len,
	socklen_t ctl_len __unused, const struct sockaddr * addr,
	socklen_t addr_len __unused, endpoint_t user_endpt __unused, int flags)
{

	if (flags != 0)
		return EOPNOTSUPP;

	if (addr != NULL)
		return EISCONN;

	/*
	 * For the most basic failures - that is, we cannot even manage to
	 * receive the request - we do not generate a reply message.
	 */
	if (len < sizeof(struct rt_msghdr))
		return ENOBUFS;
	if (len > RT_SNDBUF_MAX)
		return EMSGSIZE;

	return OK;
}

/*
 * Send data on a routing socket.
 */
static int
rtsock_send(struct sock * sock, const struct sockdriver_data * data,
	size_t len, size_t * offp, const struct sockdriver_data * ctl __unused,
	socklen_t ctl_len __unused, socklen_t * ctl_off __unused,
	const struct sockaddr * addr __unused, socklen_t addr_len __unused,
	endpoint_t user_endpt, int flags __unused, size_t min __unused)
{
	struct rtsock *rt = (struct rtsock *)sock;
	char buf[RT_SNDBUF_MAX] __aligned(4);
	struct rt_msghdr rtm;
	struct pbuf *pbuf;
	uid_t euid;
	int r, is_root;

	/* Copy in the request, and adjust some fields right away. */
	assert(len >= sizeof(rtm));
	assert(len <= sizeof(buf));

	if ((r = sockdriver_copyin(data, 0, buf, len)) != OK)
		return r;

	memcpy(&rtm, buf, sizeof(rtm));
	rtm.rtm_errno = 0;
	rtm.rtm_flags &= ~RTF_DONE;
	rtm.rtm_pid = getepinfo(user_endpt, &euid, NULL /*gid*/);

	is_root = (euid == ROOT_EUID);

	/* Process the request. */
	r = rtsock_process(rt, &rtm, buf, len, is_root);

	/*
	 * If the request has been processed successfully, a reply has been
	 * sent already, possibly also to other routing sockets.  Here, we
	 * handle the case that the request has resulted in failure, in which
	 * case we send a reply to the caller only.  This behavior is different
	 * from the traditional BSD behavior, which also sends failure replies
	 * to other sockets.  Our motivation is that while other parties are
	 * never going to be interested in failures anyway, it is in fact easy
	 * for an unprivileged user process to abuse the failure-reply system
	 * in order to fake other types of routing messages (e.g., RTM_IFINFO)
	 * to other parties.  By sending failure replies only to the requestor,
	 * we eliminate the need for security-sensitive request validation.
	 */
	if (r != OK && rtsock_can_send(rt, rt, AF_UNSPEC)) {
		rtm.rtm_errno = -r;

		if ((pbuf = rtsock_alloc(len)) == NULL)
			return ENOMEM;

		/* For the reply, reuse the request message largely as is. */
		memcpy(pbuf->payload, &rtm, sizeof(rtm));
		if (len > sizeof(rtm))
			memcpy((uint8_t *)pbuf->payload + sizeof(rtm),
			    buf + sizeof(rtm), len - sizeof(rtm));

		rtsock_enqueue(rt, pbuf);
	} else if (r == OK)
		*offp = len;

	return r;
}

/*
 * Perform preliminary checks on a receive request.
 */
static int
rtsock_pre_recv(struct sock * sock __unused, endpoint_t user_endpt __unused,
	int flags)
{

	/*
	 * We accept the same flags across all socket types in LWIP, and then
	 * simply ignore the ones we do not support for routing sockets.
	 */
	if ((flags & ~(MSG_PEEK | MSG_WAITALL)) != 0)
		return EOPNOTSUPP;

	return OK;
}

/*
 * Receive data on a routing socket.
 */
static int
rtsock_recv(struct sock * sock, const struct sockdriver_data * data,
	size_t len, size_t * off, const struct sockdriver_data * ctl __unused,
	socklen_t ctl_len __unused, socklen_t * ctl_off __unused,
	struct sockaddr * addr, socklen_t * addr_len,
	endpoint_t user_endpt __unused, int flags, size_t min __unused,
	int * rflags)
{
	struct rtsock *rt = (struct rtsock *)sock;
	struct pbuf *pbuf;
	int r;

	if ((pbuf = rt->rt_rcvhead) == NULL)
		return SUSPEND;

	/* Copy out the data to the calling user process. */
	if (len >= pbuf->tot_len)
		len = pbuf->tot_len;
	else
		*rflags |= MSG_TRUNC;

	r = util_copy_data(data, len, 0, pbuf, 0, FALSE /*copy_in*/);

	if (r != OK)
		return r;

	/* Generate a dummy source address. */
	addr->sa_len = RTSOCK_ADDR_LEN;
	addr->sa_family = AF_ROUTE;
	*addr_len = RTSOCK_ADDR_LEN;

	/* Discard the data now, unless we were instructed to peek only. */
	if (!(flags & MSG_PEEK))
		rtsock_dequeue(rt);

	/* Return the received part of the data length. */
	*off = len;
	return OK;
}

/*
 * Test whether data can be received on a routing socket, and if so, how many
 * bytes of data.
 */
static int
rtsock_test_recv(struct sock * sock, size_t min __unused, size_t * size)
{
	struct rtsock *rt = (struct rtsock *)sock;

	if (rt->rt_rcvhead == NULL)
		return SUSPEND;

	if (size != NULL)
		*size = rt->rt_rcvhead->tot_len;
	return OK;
}

/*
 * Set socket options on a routing socket.
 */
static int
rtsock_setsockopt(struct sock * sock, int level, int name,
	const struct sockdriver_data * data, socklen_t len)
{
	struct rtsock *rt = (struct rtsock *)sock;
	int r, val;

	if (level == SOL_SOCKET) {
		switch (name) {
		case SO_USELOOPBACK:
			if ((r = sockdriver_copyin_opt(data, &val, sizeof(val),
			    len)) != OK)
				return r;

			if (!val)
				rt->rt_flags |= RTF_NOLOOPBACK;
			else
				rt->rt_flags &= ~RTF_NOLOOPBACK;

			return OK;

		case SO_RCVBUF:
			if ((r = sockdriver_copyin_opt(data, &val, sizeof(val),
			    len)) != OK)
				return r;

			if (val < RT_RCVBUF_MIN || val > RT_RCVBUF_MAX)
				return EINVAL;

			rt->rt_rcvbuf = (size_t)val;

			return OK;
		}
	}

	return ENOPROTOOPT;
}

/*
 * Retrieve socket options on a routing socket.
 */
static int
rtsock_getsockopt(struct sock * sock, int level, int name,
	const struct sockdriver_data * data, socklen_t * len)
{
	struct rtsock *rt = (struct rtsock *)sock;
	int val;

	if (level == SOL_SOCKET) {
		switch (name) {
		case SO_USELOOPBACK:
			val = !(rt->rt_flags & RTF_NOLOOPBACK);

			return sockdriver_copyout_opt(data, &val, sizeof(val),
			    len);

		case SO_RCVBUF:
			val = rt->rt_rcvbuf;

			return sockdriver_copyout_opt(data, &val, sizeof(val),
			    len);
		}
	}

	return ENOPROTOOPT;
}

/*
 * Retrieve the local or remote socket address of a routing socket.
 */
static int
rtsock_getname(struct sock * sock __unused, struct sockaddr * addr,
	socklen_t * addr_len)
{

	/* This is entirely useless but apparently common between OSes. */
	addr->sa_len = RTSOCK_ADDR_LEN;
	addr->sa_family = AF_ROUTE;
	*addr_len = RTSOCK_ADDR_LEN;

	return OK;
}

/*
 * Drain the receive queue of a routing socket.
 */
static void
rtsock_drain(struct rtsock * rt)
{

	while (rt->rt_rcvhead != NULL)
		rtsock_dequeue(rt);
}

/*
 * Shut down a routing socket for reading and/or writing.
 */
static int
rtsock_shutdown(struct sock * sock, unsigned int mask)
{
	struct rtsock *rt = (struct rtsock *)sock;

	if (mask & SFL_SHUT_RD)
		rtsock_drain(rt);

	return OK;
}

/*
 * Close a routing socket.
 */
static int
rtsock_close(struct sock * sock, int force __unused)
{
	struct rtsock *rt = (struct rtsock *)sock;

	rtsock_drain(rt);

	return OK;
}

/*
 * Free up a closed routing socket.
 */
static void
rtsock_free(struct sock * sock)
{
	struct rtsock *rt = (struct rtsock *)sock;

	TAILQ_REMOVE(&rt_activelist, rt, rt_next);

	TAILQ_INSERT_HEAD(&rt_freelist, rt, rt_next);
}

static const struct sockevent_ops rtsock_ops = {
	.sop_pre_send		= rtsock_pre_send,
	.sop_send		= rtsock_send,
	.sop_pre_recv		= rtsock_pre_recv,
	.sop_recv		= rtsock_recv,
	.sop_test_recv		= rtsock_test_recv,
	.sop_setsockopt		= rtsock_setsockopt,
	.sop_getsockopt		= rtsock_getsockopt,
	.sop_getsockname	= rtsock_getname,
	.sop_getpeername	= rtsock_getname,
	.sop_shutdown		= rtsock_shutdown,
	.sop_close		= rtsock_close,
	.sop_free		= rtsock_free
};

/*
 * Send an interface announcement message about the given interface.  If
 * 'arrival' is set, the interface has just been created; otherwise, the
 * interface is about to be destroyed.
 */
void
rtsock_msg_ifannounce(struct ifdev * ifdev, int arrival)
{
	struct if_announcemsghdr ifan;
	struct pbuf *pbuf;

	if (!rtsock_msg_match(NULL /*rtsrc*/, AF_UNSPEC, NULL /*pbuf*/))
		return;

	memset(&ifan, 0, sizeof(ifan));
	ifan.ifan_msglen = sizeof(ifan);
	ifan.ifan_version = RTM_VERSION;
	ifan.ifan_type = RTM_IFANNOUNCE;
	ifan.ifan_index = ifdev_get_index(ifdev);
	strlcpy(ifan.ifan_name, ifdev_get_name(ifdev), sizeof(ifan.ifan_name));
	ifan.ifan_what = (arrival) ? IFAN_ARRIVAL : IFAN_DEPARTURE;

	if ((pbuf = rtsock_alloc(sizeof(ifan))) == NULL)
		return;
	memcpy(pbuf->payload, &ifan, sizeof(ifan));

	rtsock_msg_match(NULL /*rtsrc*/, AF_UNSPEC, pbuf);
}

/*
 * Send an interface information routing message.
 */
void
rtsock_msg_ifinfo(struct ifdev * ifdev)
{
	struct if_msghdr ifm;
	struct pbuf *pbuf;

	if (!rtsock_msg_match(NULL /*rtsrc*/, AF_UNSPEC, NULL /*pbuf*/))
		return;

	memset(&ifm, 0, sizeof(ifm));
	ifm.ifm_msglen = sizeof(ifm);
	ifm.ifm_version = RTM_VERSION;
	ifm.ifm_type = RTM_IFINFO;
	ifm.ifm_addrs = 0;
	ifm.ifm_flags = ifdev_get_ifflags(ifdev);
	ifm.ifm_index = ifdev_get_index(ifdev);
	memcpy(&ifm.ifm_data, ifdev_get_ifdata(ifdev), sizeof(ifm.ifm_data));

	if ((pbuf = rtsock_alloc(sizeof(ifm))) == NULL)
		return;
	memcpy(pbuf->payload, &ifm, sizeof(ifm));

	rtsock_msg_match(NULL /*rtsrc*/, AF_UNSPEC, pbuf);
}

/*
 * Set up a RTA map and an interface address structure for use in a RTM_xxxADDR
 * routing message.
 */
static void
rtsock_rta_init_ifam(struct rtsock_rta * rta, struct ifa_msghdr * ifam,
	struct ifdev * ifdev, unsigned int type, struct sockaddr_dlx * sdlx)
{

	memset(ifam, 0, sizeof(*ifam));
	ifam->ifam_version = RTM_VERSION;
	ifam->ifam_type = type;
	ifam->ifam_flags = 0;
	ifam->ifam_index = ifdev_get_index(ifdev);
	ifam->ifam_metric = ifdev_get_metric(ifdev);

	rtsock_rta_init(rta);

	ifaddr_dl_get(ifdev, (ifaddr_dl_num_t)0, sdlx);

	rtsock_rta_set(rta, RTAX_IFP, sdlx, sdlx->sdlx_len);
}

/*
 * Add a specific link-layer address for an interface to the given RTA map.
 */
static void
rtsock_rta_add_dl(struct rtsock_rta * rta, struct ifdev * ifdev,
	ifaddr_dl_num_t num, struct sockaddr_dlx * sdlx)
{

	/* Obtain the address data. */
	ifaddr_dl_get(ifdev, num, sdlx);

	/* Add the interface address. */
	rtsock_rta_set(rta, RTAX_IFA, sdlx, sdlx->sdlx_len);

	/*
	 * NetBSD also adds a RTAX_NETMASK entry here.  At this moment it is
	 * not clear to me why, and it is a pain to make, so for now we do not.
	 */
}

/*
 * Send a routing message about a new, changed, or deleted datalink address for
 * the given interface.
 */
void
rtsock_msg_addr_dl(struct ifdev * ifdev, unsigned int type,
	ifaddr_dl_num_t num)
{
	struct rtsock_rta rta;
	struct ifa_msghdr ifam;
	struct sockaddr_dlx name, addr;
	struct pbuf *pbuf;

	if (!rtsock_msg_match(NULL /*rtsrc*/, AF_LINK, NULL /*pbuf*/))
		return;

	rtsock_rta_init_ifam(&rta, &ifam, ifdev, type, &name);

	rtsock_rta_add_dl(&rta, ifdev, num, &addr);

	if (rtsock_rta_finalize(&ifam, sizeof(ifam), &ifam.ifam_msglen,
	    &ifam.ifam_addrs, &rta, &pbuf, NULL, 0) > 0)
		rtsock_msg_match(NULL /*rtsrc*/, AF_LINK, pbuf);
}

/*
 * Add a specific IPv4 address for an interface to the given RTA map.
 */
static void
rtsock_rta_add_v4(struct rtsock_rta * rta, struct ifdev * ifdev,
	ifaddr_v4_num_t num, struct sockaddr_in sin[4])
{

	/* Obtain the address data. */
	(void)ifaddr_v4_get(ifdev, num, &sin[0], &sin[1], &sin[2], &sin[3]);

	/* Add the interface address. */
	rtsock_rta_set(rta, RTAX_IFA, &sin[0], sin[0].sin_len);

	/* Add the netmask, after compressing it. */
	rtsock_compress_netmask((struct sockaddr *)&sin[1]);

	rtsock_rta_set(rta, RTAX_NETMASK, &sin[1], sin[1].sin_len);

	/* Possibly add a broadcast or destination address. */
	if (sin[2].sin_len != 0)
		rtsock_rta_set(rta, RTAX_BRD, &sin[2], sin[2].sin_len);
	else if (sin[3].sin_len != 0)
		rtsock_rta_set(rta, RTAX_DST, &sin[3], sin[3].sin_len);
}

/*
 * Send a routing message about a new or deleted IPv4 address for the given
 * interface.
 */
void
rtsock_msg_addr_v4(struct ifdev * ifdev, unsigned int type,
	ifaddr_v4_num_t num)
{
	struct rtsock_rta rta;
	struct ifa_msghdr ifam;
	struct sockaddr_dlx name;
	struct sockaddr_in sin[4];
	struct pbuf *pbuf;

	if (!rtsock_msg_match(NULL /*rtsrc*/, AF_INET, NULL /*pbuf*/))
		return;

	rtsock_rta_init_ifam(&rta, &ifam, ifdev, type, &name);

	rtsock_rta_add_v4(&rta, ifdev, num, sin);

	if (rtsock_rta_finalize(&ifam, sizeof(ifam), &ifam.ifam_msglen,
	    &ifam.ifam_addrs, &rta, &pbuf, NULL, 0) > 0)
		rtsock_msg_match(NULL /*rtsrc*/, AF_INET, pbuf);
}

/*
 * Add a specific IPv6 address for an interface to the given RTA map.
 */
static void
rtsock_rta_add_v6(struct rtsock_rta * rta, struct ifdev * ifdev,
	ifaddr_v6_num_t num, struct sockaddr_in6 sin6[3])
{

	/* Obtain the address data. */
	ifaddr_v6_get(ifdev, num, &sin6[0], &sin6[1], &sin6[2]);

	/* Add the interface address. */
	rtsock_rta_set(rta, RTAX_IFA, &sin6[0], sin6[0].sin6_len);

	/* Add the netmask, after compressing it (a no-op at the moment). */
	rtsock_compress_netmask((struct sockaddr *)&sin6[1]);

	rtsock_rta_set(rta, RTAX_NETMASK, &sin6[1], sin6[1].sin6_len);

	/* Possibly add a destination address. */
	if (sin6[2].sin6_len != 0)
		rtsock_rta_set(rta, RTAX_DST, &sin6[2], sin6[2].sin6_len);
}

/*
 * Send a routing message about a new or deleted IPv6 address for the given
 * interface.
 */
void
rtsock_msg_addr_v6(struct ifdev * ifdev, unsigned int type,
	ifaddr_v6_num_t num)
{
	struct rtsock_rta rta;
	struct ifa_msghdr ifam;
	struct sockaddr_dlx name;
	struct sockaddr_in6 sin6[3];
	struct pbuf *pbuf;

	if (!rtsock_msg_match(NULL /*rtsrc*/, AF_INET6, NULL /*pbuf*/))
		return;

	rtsock_rta_init_ifam(&rta, &ifam, ifdev, type, &name);

	rtsock_rta_add_v6(&rta, ifdev, num, sin6);

	if (rtsock_rta_finalize(&ifam, sizeof(ifam), &ifam.ifam_msglen,
	    &ifam.ifam_addrs, &rta, &pbuf, NULL, 0) > 0)
		rtsock_msg_match(NULL /*rtsrc*/, AF_INET6, pbuf);
}

/*
 * Send an RTM_MISS routing message about an address for which no route was
 * found.  The caller must provide the address in the appropriate form and
 * perform any per-address rate limiting.
 */
void
rtsock_msg_miss(const struct sockaddr * addr)
{
	struct rt_msghdr rtm;
	struct rtsock_rta rta;
	struct pbuf *pbuf;

	/*
	 * Unfortunately the destination address has already been generated (as
	 * 'addr'), which is a big part of the work.  Still, skip the rest if
	 * there is no routing socket to deliver the message to.
	 */
	if (!rtsock_msg_match(NULL /*rtsrc*/, addr->sa_family, NULL /*pbuf*/))
		return;

	memset(&rtm, 0, sizeof(rtm));
	rtm.rtm_version = RTM_VERSION;
	rtm.rtm_type = RTM_MISS;

	rtsock_rta_init(&rta);

	rtsock_rta_set(&rta, RTAX_DST, addr, addr->sa_len);

	if (rtsock_rta_finalize(&rtm, sizeof(rtm), &rtm.rtm_msglen,
	    &rtm.rtm_addrs, &rta, &pbuf, NULL, 0) > 0)
		rtsock_msg_match(NULL /*rtsrc*/, addr->sa_family, pbuf);
}

/*
 * Generate routing socket data for a route, for either routing socket
 * broadcasting or a sysctl(7) request.  The route is given as 'route'.  The
 * type of the message (RTM_) is given as 'type'.  The resulting routing
 * message header is stored in 'rtm' and an address vector is stored in 'rta'.
 * The latter may point to addresses generated in 'addr', 'mask', 'gateway',
 * and optionally (if not NULL) 'ifp' and 'ifa'.  The caller is responsible for
 * combining the results into an appropriate routing message.
 */
static void
rtsock_get_route(struct rt_msghdr * rtm, struct rtsock_rta * rta,
	union sockaddr_any * addr, union sockaddr_any * mask,
	union sockaddr_any * gateway, union sockaddr_any * ifp,
	union sockaddr_any * ifa, const struct route_entry * route,
	unsigned int type)
{
	struct ifdev *ifdev;
	unsigned int flags, use;

	route_get(route, addr, mask, gateway, ifp, ifa, &ifdev, &flags, &use);

	memset(rtm, 0, sizeof(*rtm));
	rtm->rtm_version = RTM_VERSION;
	rtm->rtm_type = type;
	rtm->rtm_flags = flags;
	rtm->rtm_index = ifdev_get_index(ifdev);
	rtm->rtm_use = use;

	rtsock_rta_init(rta);

	rtsock_rta_set(rta, RTAX_DST, addr, addr->sa.sa_len);

	if (!(flags & RTF_HOST)) {
		rtsock_compress_netmask(&mask->sa);

		rtsock_rta_set(rta, RTAX_NETMASK, mask, mask->sa.sa_len);
	}

	rtsock_rta_set(rta, RTAX_GATEWAY, gateway, gateway->sa.sa_len);

	if (ifp != NULL)
		rtsock_rta_set(rta, RTAX_IFP, ifp, ifp->sa.sa_len);

	if (ifa != NULL)
		rtsock_rta_set(rta, RTAX_IFA, ifa, ifa->sa.sa_len);
}

/*
 * Send a routing message about a route, with the given type which may be one
 * of RTM_ADD, RTM_CHANGE, RTM_DELETE, RTM_LOCK, and RTM_GET.  The routing
 * socket request information 'rtr', if not NULL, provides additional
 * information about the routing socket that was the source of the request (if
 * any), various fields that should be echoed, and (for RTM_GET) whether to
 * add interface information to the output.
 */
void
rtsock_msg_route(const struct route_entry * route, unsigned int type,
	const struct rtsock_request * rtr)
{
	union sockaddr_any addr, mask, gateway, ifp, ifa;
	struct rt_msghdr rtm;
	struct rtsock_rta rta;
	struct rtsock *rtsrc;
	struct pbuf *pbuf;
	int family, getif;

	rtsrc = (rtr != NULL) ? rtr->rtr_src : NULL;
	family = (route_is_ipv6(route)) ? AF_INET6 : AF_INET;

	if (!rtsock_msg_match(rtsrc, family, NULL /*pbuf*/))
		return;

	getif = (rtr != NULL && rtr->rtr_getif);

	rtsock_get_route(&rtm, &rta, &addr, &mask, &gateway,
	    (getif) ? &ifp : NULL, (getif) ? &ifa : NULL, route, type);

	if (rtr != NULL) {
		rtm.rtm_flags |= RTF_DONE;
		rtm.rtm_pid = rtr->rtr_pid;
		rtm.rtm_seq = rtr->rtr_seq;
	}

	if (rtsock_rta_finalize(&rtm, sizeof(rtm), &rtm.rtm_msglen,
	    &rtm.rtm_addrs, &rta, &pbuf, NULL, 0) > 0)
		rtsock_msg_match(rtsrc, family, pbuf);
}

/*
 * Generate sysctl(7) output or length for the given routing table entry
 * 'route', provided that the route passes the flags filter 'filter'.  The
 * address length 'addr_len' is used to compute a cheap length estimate.  On
 * success, return the byte size of the output.  If the route was not a match
 * for the filter, return zero.  On failure, return a negative error code.
 */
static ssize_t
rtsock_info_rtable_entry(const struct route_entry * route, unsigned int filter,
	socklen_t addr_len, struct rmib_oldp * oldp, size_t off)
{
	union sockaddr_any addr, mask, gateway;
	struct rt_msghdr rtm;
	struct rtsock_rta rta;
	unsigned int flags;
	ssize_t len;

	flags = route_get_flags(route);

	/* Apparently, matching any of the flags (if given) is sufficient. */
	if (filter != 0 && (filter & flags) != 0)
		return 0;

	/* Size (over)estimation shortcut. */
	if (oldp == NULL) {
		len = sizeof(rtm) + RT_ROUNDUP(addr_len) +
		    RT_ROUNDUP(sizeof(gateway));

		if (!(flags & RTF_HOST))
			len += RT_ROUNDUP(addr_len);

		return len;
	}

	rtsock_get_route(&rtm, &rta, &addr, &mask, &gateway, NULL /*ifp*/,
	    NULL /*ifa*/, route, RTM_GET);

	return rtsock_rta_finalize(&rtm, sizeof(rtm), &rtm.rtm_msglen,
	    &rtm.rtm_addrs, &rta, NULL /*pbuf*/, oldp, off);
}

/*
 * Obtain routing table entries.
 */
static ssize_t
rtsock_info_rtable(struct rmib_oldp * oldp, int family, int filter)
{
	struct route_entry *route;
	ssize_t r, off;

	off = 0;

	if (family == AF_UNSPEC || family == AF_INET) {
		for (route = NULL; (route = route_enum_v4(route)) != NULL; ) {
			if ((r = rtsock_info_rtable_entry(route,
			    (unsigned int)filter, sizeof(struct sockaddr_in),
			    oldp, off)) < 0)
				return r;
			off += r;
		}
	}

	if (family == AF_UNSPEC || family == AF_INET6) {
		for (route = NULL; (route = route_enum_v6(route)) != NULL; ) {
			if ((r = rtsock_info_rtable_entry(route,
			    (unsigned int)filter, sizeof(struct sockaddr_in6),
			    oldp, off)) < 0)
				return r;
			off += r;
		}
	}

	/* TODO: should we add slack here? */
	return off;
}

/*
 * Generate routing socket data for an ARP table entry, for either routing
 * socket broadcasting or a sysctl(7) request.  The ARP table entry number is
 * given as 'num'.  The type of the message (RTM_) is given as 'type'.  The
 * resulting routing message header is stored in 'rtm' and an address vector is
 * stored in 'rta'.  The latter may point to addresses generated in 'addr' and
 * 'gateway'.  The caller is responsible for combining the results into an
 * appropriate routing message.
 */
static void
rtsock_get_arp(struct rt_msghdr * rtm, struct rtsock_rta * rta,
	struct sockaddr_in * addr, struct sockaddr_dlx * gateway,
	lldata_arp_num_t num, unsigned int type)
{
	struct ifdev *ifdev;
	unsigned int flags;

	lldata_arp_get(num, addr, gateway, &ifdev, &flags);

	memset(rtm, 0, sizeof(*rtm));
	rtm->rtm_version = RTM_VERSION;
	rtm->rtm_type = type;
	rtm->rtm_flags = flags;
	rtm->rtm_index = ifdev_get_index(ifdev);

	/* TODO: obtaining and reporting the proper expiry time, if any. */
	if (!(flags & RTF_STATIC))
		rtm->rtm_rmx.rmx_expire = (time_t)-1;

	rtsock_rta_init(rta);

	rtsock_rta_set(rta, RTAX_DST, addr, addr->sin_len);

	rtsock_rta_set(rta, RTAX_GATEWAY, gateway, gateway->sdlx_len);
}

/*
 * Send a routing message about an ARP table entry, with the given type which
 * may be one of RTM_ADD, RTM_CHANGE, RTM_DELETE, RTM_LOCK, and RTM_GET.  The
 * routing socket request information 'rtr', if not NULL, provides additional
 * information about the routing socket that was the source of the request (if
 * any) and various fields that should be echoed.
 */
void
rtsock_msg_arp(lldata_arp_num_t num, unsigned int type,
	const struct rtsock_request * rtr)
{
	struct sockaddr_in addr;
	struct sockaddr_dlx gateway;
	struct rt_msghdr rtm;
	struct rtsock_rta rta;
	struct pbuf *pbuf;

	assert(rtr != NULL);

	/*
	 * We do not maintain the link-local tables ourselves, and thus, we do
	 * not have a complete view of modifications to them.  In order not to
	 * confuse userland with inconsistent updates (e.g., deletion of
	 * previously unreported entries), send these routing messages to the
	 * source of the routing request only.
	 */
	if (!rtsock_msg_one(rtr->rtr_src, AF_INET, NULL /*pbuf*/))
		return;

	rtsock_get_arp(&rtm, &rta, &addr, &gateway, num, type);

	if (rtr != NULL) {
		rtm.rtm_flags |= RTF_DONE;
		rtm.rtm_pid = rtr->rtr_pid;
		rtm.rtm_seq = rtr->rtr_seq;
	}

	if (rtsock_rta_finalize(&rtm, sizeof(rtm), &rtm.rtm_msglen,
	    &rtm.rtm_addrs, &rta, &pbuf, NULL, 0) > 0)
		rtsock_msg_one(rtr->rtr_src, AF_INET, pbuf);
}

/*
 * Obtain ARP table entries.
 */
static ssize_t
rtsock_info_lltable_arp(struct rmib_oldp * oldp)
{
	struct sockaddr_in addr;
	struct sockaddr_dlx gateway;
	struct rt_msghdr rtm;
	struct rtsock_rta rta;
	lldata_arp_num_t num;
	ssize_t r, off;

	off = 0;

	for (num = 0; lldata_arp_enum(&num); num++) {
		/* Size (over)estimation shortcut. */
		if (oldp == NULL) {
			off += sizeof(struct rt_msghdr) +
			    RT_ROUNDUP(sizeof(addr)) +
			    RT_ROUNDUP(sizeof(gateway));

			continue;
		}

		rtsock_get_arp(&rtm, &rta, &addr, &gateway, num, RTM_GET);

		if ((r = rtsock_rta_finalize(&rtm, sizeof(rtm),
		    &rtm.rtm_msglen, &rtm.rtm_addrs, &rta, NULL /*pbuf*/, oldp,
		    off)) < 0)
			return r;
		off += r;
	}

	/* TODO: should we add slack here? */
	return off;
}

/*
 * Generate routing socket data for an NDP table entry, for either routing
 * socket broadcasting or a sysctl(7) request.  The NDP table entry number is
 * given as 'num'.  The type of the message (RTM_) is given as 'type'.  The
 * resulting routing message header is stored in 'rtm' and an address vector is
 * stored in 'rta'.  The latter may point to addresses generated in 'addr' and
 * 'gateway'.  The caller is responsible for combining the results into an
 * appropriate routing message.
 */
static void
rtsock_get_ndp(struct rt_msghdr * rtm, struct rtsock_rta * rta,
	struct sockaddr_in6 * addr, struct sockaddr_dlx * gateway,
	lldata_ndp_num_t num, unsigned int type)
{
	struct ifdev *ifdev;
	unsigned int flags;

	lldata_ndp_get(num, addr, gateway, &ifdev, &flags);

	memset(rtm, 0, sizeof(*rtm));
	rtm->rtm_version = RTM_VERSION;
	rtm->rtm_type = type;
	rtm->rtm_flags = flags;
	rtm->rtm_index = ifdev_get_index(ifdev);

	rtsock_rta_init(rta);

	rtsock_rta_set(rta, RTAX_DST, addr, addr->sin6_len);

	rtsock_rta_set(rta, RTAX_GATEWAY, gateway, gateway->sdlx_len);
}

/*
 * Send a routing message about an NDP table entry, with the given type which
 * may be one of RTM_ADD, RTM_CHANGE, RTM_DELETE, RTM_LOCK, and RTM_GET.  The
 * routing socket request information 'rtr', if not NULL, provides additional
 * information about the routing socket that was the source of the request (if
 * any) and various fields that should be echoed.
 */
void
rtsock_msg_ndp(lldata_ndp_num_t num, unsigned int type,
	const struct rtsock_request * rtr)
{
	struct sockaddr_in6 addr;
	struct sockaddr_dlx gateway;
	struct rt_msghdr rtm;
	struct rtsock_rta rta;
	struct pbuf *pbuf;

	assert(rtr != NULL);

	/*
	 * We do not maintain the link-local tables ourselves, and thus, we do
	 * not have a complete view of modifications to them.  In order not to
	 * confuse userland with inconsistent updates (e.g., deletion of
	 * previously unreported entries), send these routing messages to the
	 * source of the routing request only.
	 */
	if (!rtsock_msg_one(rtr->rtr_src, AF_INET6, NULL /*pbuf*/))
		return;

	rtsock_get_ndp(&rtm, &rta, &addr, &gateway, num, type);

	if (rtr != NULL) {
		rtm.rtm_flags |= RTF_DONE;
		rtm.rtm_pid = rtr->rtr_pid;
		rtm.rtm_seq = rtr->rtr_seq;
	}

	if (rtsock_rta_finalize(&rtm, sizeof(rtm), &rtm.rtm_msglen,
	    &rtm.rtm_addrs, &rta, &pbuf, NULL, 0) > 0)
		rtsock_msg_one(rtr->rtr_src, AF_INET6, pbuf);
}

/*
 * Obtain NDP table entries.
 */
static ssize_t
rtsock_info_lltable_ndp(struct rmib_oldp * oldp)
{
	struct rt_msghdr rtm;
	struct rtsock_rta rta;
	struct sockaddr_in6 addr;
	struct sockaddr_dlx gateway;
	lldata_ndp_num_t num;
	ssize_t r, off;

	off = 0;

	for (num = 0; lldata_ndp_enum(&num); num++) {
		/* Size (over)estimation shortcut. */
		if (oldp == NULL) {
			off += sizeof(struct rt_msghdr) +
			    RT_ROUNDUP(sizeof(addr)) +
			    RT_ROUNDUP(sizeof(gateway));

			continue;
		}

		rtsock_get_ndp(&rtm, &rta, &addr, &gateway, num, RTM_GET);

		if ((r = rtsock_rta_finalize(&rtm, sizeof(rtm),
		    &rtm.rtm_msglen, &rtm.rtm_addrs, &rta, NULL /*pbuf*/, oldp,
		    off)) < 0)
			return r;
		off += r;
	}

	/* TODO: should we add slack here? */
	return off;
}

/*
 * Obtain link-layer (ARP, NDP) table entries.
 */
static ssize_t
rtsock_info_lltable(struct rmib_oldp * oldp, int family)
{

	switch (family) {
	case AF_INET:
		return rtsock_info_lltable_arp(oldp);

	case AF_INET6:
		return rtsock_info_lltable_ndp(oldp);

	default:
		return 0;
	}
}

/*
 * Obtain link-layer address information for one specific interface.
 */
static ssize_t
rtsock_info_if_dl(struct ifdev * ifdev, struct ifa_msghdr * ifam,
	struct rmib_oldp * oldp, ssize_t off)
{
	struct rtsock_rta rta;
	struct sockaddr_dlx sdlx;
	ifaddr_dl_num_t num;
	ssize_t r, len;

	len = 0;

	for (num = 0; ifaddr_dl_enum(ifdev, &num); num++) {
		if (oldp == NULL) {
			len += sizeof(*ifam) + RT_ROUNDUP(sizeof(sdlx));

			continue;
		}

		rtsock_rta_init(&rta);

		rtsock_rta_add_dl(&rta, ifdev, num, &sdlx);

		if ((r = rtsock_rta_finalize(ifam, sizeof(*ifam),
		    &ifam->ifam_msglen, &ifam->ifam_addrs, &rta, NULL /*pbuf*/,
		    oldp, off + len)) < 0)
			return r;
		len += r;
	}

	return len;
}

/*
 * Obtain IPv4 address information for one specific interface.
 */
static ssize_t
rtsock_info_if_v4(struct ifdev * ifdev, struct ifa_msghdr * ifam,
	struct rmib_oldp * oldp, ssize_t off)
{
	struct sockaddr_in sin[4];
	struct rtsock_rta rta;
	ifaddr_v4_num_t num;
	ssize_t r, len;

	len = 0;

	/*
	 * Mostly for future compatibility, we support multiple IPv4 interface
	 * addresses here.  Every interface has an interface address and a
	 * netmask.  In addition, an interface may have either a broadcast or a
	 * destination address.
	 */
	for (num = 0; ifaddr_v4_enum(ifdev, &num); num++) {
		/* Size (over)estimation shortcut. */
		if (oldp == NULL) {
			len += sizeof(*ifam) + RT_ROUNDUP(sizeof(sin[0])) * 3;

			continue;
		}

		rtsock_rta_init(&rta);

		rtsock_rta_add_v4(&rta, ifdev, num, sin);

		if ((r = rtsock_rta_finalize(ifam, sizeof(*ifam),
		    &ifam->ifam_msglen, &ifam->ifam_addrs, &rta, NULL /*pbuf*/,
		    oldp, off + len)) < 0)
			return r;
		len += r;
	}

	return len;
}

/*
 * Obtain IPv6 address information for one specific interface.
 */
static ssize_t
rtsock_info_if_v6(struct ifdev * ifdev, struct ifa_msghdr * ifam,
	struct rmib_oldp * oldp, ssize_t off)
{
	struct sockaddr_in6 sin6[3];
	struct rtsock_rta rta;
	ifaddr_v6_num_t num;
	ssize_t r, len;

	len = 0;

	/* As with IPv4, except that IPv6 has no broadcast addresses. */
	for (num = 0; ifaddr_v6_enum(ifdev, &num); num++) {
		/* Size (over)estimation shortcut. */
		if (oldp == NULL) {
			len += sizeof(*ifam) + RT_ROUNDUP(sizeof(sin6[0])) * 3;

			continue;
		}

		rtsock_rta_init(&rta);

		rtsock_rta_add_v6(&rta, ifdev, num, sin6);

		if ((r = rtsock_rta_finalize(ifam, sizeof(*ifam),
		    &ifam->ifam_msglen, &ifam->ifam_addrs, &rta, NULL /*pbuf*/,
		    oldp, off + len)) < 0)
			return r;
		len += r;
	}

	return len;
}

/*
 * Obtain information for one specific interface.
 */
static ssize_t
rtsock_info_if(struct ifdev * ifdev, struct rmib_oldp * oldp, ssize_t off,
	int family)
{
	struct rtsock_rta rta;
	struct sockaddr_dlx sdlx;
	struct if_msghdr ifm;
	struct ifa_msghdr ifam;
	unsigned int ifflags;
	ssize_t r, len, sdlxsize;

	len = 0;

	ifflags = ifdev_get_ifflags(ifdev);

	/* Create an interface information entry. */
	rtsock_rta_init(&rta);

	if (oldp != NULL) {
		memset(&ifm, 0, sizeof(ifm));
		ifm.ifm_version = RTM_VERSION;
		ifm.ifm_type = RTM_IFINFO;
		ifm.ifm_flags = ifflags;
		ifm.ifm_index = ifdev_get_index(ifdev);
		memcpy(&ifm.ifm_data, ifdev_get_ifdata(ifdev),
		    sizeof(ifm.ifm_data));
	}

	/*
	 * Generate a datalink socket address structure.  TODO: see if it is
	 * worth obtaining just the length for the (oldp == NULL) case here.
	 */
	memset(&sdlx, 0, sizeof(sdlx));

	ifaddr_dl_get(ifdev, 0, &sdlx);

	sdlxsize = RT_ROUNDUP(sdlx.sdlx_len);

	rtsock_rta_set(&rta, RTAX_IFP, &sdlx, sdlxsize);

	if ((r = rtsock_rta_finalize(&ifm, sizeof(ifm), &ifm.ifm_msglen,
	    &ifm.ifm_addrs, &rta, NULL /*pbuf*/, oldp, off + len)) < 0)
		return r;
	len += r;

	/* Generate a header for all addresses once. */
	if (oldp != NULL) {
		memset(&ifam, 0, sizeof(ifam));
		ifam.ifam_version = RTM_VERSION;
		ifam.ifam_type = RTM_NEWADDR;
		ifam.ifam_flags = 0;
		ifam.ifam_index = ifdev_get_index(ifdev);
		ifam.ifam_metric = ifdev_get_metric(ifdev);
	}

	/* If requested and applicable, add any datalink addresses. */
	if (family == AF_UNSPEC || family == AF_LINK) {
		if ((r = rtsock_info_if_dl(ifdev, &ifam, oldp, off + len)) < 0)
			return r;
		len += r;
	}

	/* If requested and applicable, add any IPv4 addresses. */
	if (family == AF_UNSPEC || family == AF_INET) {
		if ((r = rtsock_info_if_v4(ifdev, &ifam, oldp, off + len)) < 0)
			return r;
		len += r;
	}

	/* If requested and applicable, add any IPv6 addresses. */
	if (family == AF_UNSPEC || family == AF_INET6) {
		if ((r = rtsock_info_if_v6(ifdev, &ifam, oldp, off + len)) < 0)
			return r;
		len += r;
	}

	return len;
}

/*
 * Obtain interface information.
 */
static ssize_t
rtsock_info_iflist(struct rmib_oldp * oldp, int family, uint32_t ifindex)
{
	struct ifdev *ifdev;
	ssize_t r, off;

	/*
	 * If information about a specific interface index is requested, then
	 * return information for just that interface.
	 */
	if (ifindex != 0) {
		if ((ifdev = ifdev_get_by_index(ifindex)) != NULL)
			return rtsock_info_if(ifdev, oldp, 0, family);
		else
			return 0;
	}

	/* Otherwise, iterate through the list of all interfaces. */
	off = 0;

	for (ifdev = ifdev_enum(NULL); ifdev != NULL;
	    ifdev = ifdev_enum(ifdev)) {

		/* Avoid generating results that are never copied out. */
		if (oldp != NULL && !rmib_inrange(oldp, off))
			oldp = NULL;

		if ((r = rtsock_info_if(ifdev, oldp, off, family)) < 0)
			return r;

		off += r;
	}

	/* TODO: should we add slack here? */
	return off;
}

/*
 * Obtain routing table, ARP cache, and interface information through
 * sysctl(7).  Return the (produced, or if oldp is NULL, estimated) byte size
 * of the output on success, or a negative error code on failure.
 */
static ssize_t
rtsock_info(struct rmib_call * call, struct rmib_node * node __unused,
	struct rmib_oldp * oldp, struct rmib_newp * newp __unused)
{
	int family, filter;

	if (call->call_namelen != 3)
		return EINVAL;

	family = call->call_name[0];
	filter = call->call_name[2];

	switch (call->call_name[1]) {
	case NET_RT_FLAGS:
		/*
		 * Preliminary support for changes as of NetBSD 8, where by
		 * default, the use of this subcall implies an ARP/NDP-only
		 * request.
		 */
		if (filter == 0)
			filter |= RTF_LLDATA;

		if (filter & RTF_LLDATA) {
			if (family == AF_UNSPEC)
				return EINVAL;

			/*
			 * Split off ARP/NDP handling from the normal routing
			 * table listing, as done since NetBSD 8.  We generate
			 * the ARP/NDP listing from here, and keep those
			 * entries out of the routing table dump below.  Since
			 * the filter is of a match-any type, and we have just
			 * matched a flag, no further filtering is needed here.
			 */
			return rtsock_info_lltable(oldp, family);
		}

		/* FALLTHROUGH */
	case NET_RT_DUMP:
		return rtsock_info_rtable(oldp, family, filter);

	case NET_RT_IFLIST:
		return rtsock_info_iflist(oldp, family, filter);

	default:
		return EINVAL;
	}
}
