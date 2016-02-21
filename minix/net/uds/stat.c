/* UNIX Domain Sockets - stat.c - network status */

#include "uds.h"
#include <sys/socketvar.h>
#include <sys/unpcb.h>

/*
 * Fill the given 'ki' structure with information about the socket 'uds'.
 */
static void
uds_get_info(struct kinfo_pcb * ki, const struct udssock * uds)
{
	struct udssock *peer;
	socklen_t len;
	int type;

	type = uds_get_type(uds);
	peer = uds_get_peer(uds);

	ki->ki_pcbaddr = (uint64_t)(uintptr_t)uds;
	ki->ki_ppcbaddr = (uint64_t)(uintptr_t)uds;
	ki->ki_sockaddr = (uint64_t)(uintptr_t)&uds->uds_sock;
	ki->ki_family = AF_UNIX;
	ki->ki_type = type;
	ki->ki_protocol = UDSPROTO_UDS;
	ki->ki_pflags = 0;
	if (uds->uds_flags & UDSF_CONNWAIT)
		ki->ki_pflags |= UNP_CONNWAIT;
	if (uds->uds_flags & UDSF_PASSCRED)
		ki->ki_pflags |= UNP_WANTCRED;
	if (type != SOCK_DGRAM && uds->uds_cred.unp_pid != -1) {
		if (uds_is_listening(uds))
			ki->ki_pflags |= UNP_EIDSBIND;
		else if (uds_is_connecting(uds) || uds_is_connected(uds))
			ki->ki_pflags |= UNP_EIDSVALID;
	}
	/* Not sure about NetBSD connection states.  First attempt here. */
	if (uds_is_connecting(uds))
		ki->ki_sostate = SS_ISCONNECTING;
	else if (uds_is_connected(uds))
		ki->ki_sostate = SS_ISCONNECTED;
	else if (uds_is_disconnected(uds))
		ki->ki_sostate = SS_ISDISCONNECTED;
	ki->ki_rcvq = uds->uds_len;
	/* We currently mirror the peer's receive queue size when connected. */
	if (uds_is_connected(uds))
		ki->ki_sndq = peer->uds_len;
	/* The source is not set for bound connection-type sockets here. */
	if (type == SOCK_DGRAM || uds_is_listening(uds))
		uds_make_addr(uds->uds_path, (size_t)uds->uds_pathlen,
		    &ki->ki_src, &len);
	if (peer != NULL)
		uds_make_addr(peer->uds_path, (size_t)peer->uds_pathlen,
		    &ki->ki_dst, &len);
	/* TODO: we should set ki_inode and ki_vnode, but to what? */
	ki->ki_conn = (uint64_t)(uintptr_t)peer;
	if (!TAILQ_EMPTY(&uds->uds_queue))
		ki->ki_refs =
		    (uint64_t)(uintptr_t)TAILQ_FIRST(&uds->uds_queue);
	if (uds_has_link(uds))
		ki->ki_nextref =
		    (uint64_t)(uintptr_t)TAILQ_NEXT(uds, uds_next);
}

/*
 * Remote MIB implementation of CTL_NET PF_LOCAL {SOCK_STREAM,SOCK_DGRAM,
 * SOCK_SEQPACKET} 0.  This function handles all queries on the
 * "net.local.{stream,dgram,seqpacket}.pcblist" sysctl(7) nodes.
 *
 * The 0 for "pcblist" is a MINIXism: we use it to keep our arrays small.
 * NetBSD numbers these nodes dynamically and so they have numbers above
 * CREATE_BASE.  That also means that no userland application can possibly
 * hardcode their numbers, and must perform lookups by name.  In turn, that
 * means that we can safely change the 0 to another number if NetBSD ever
 * introduces statically numbered nodes in these subtrees.
 */
static ssize_t
net_local_pcblist(struct rmib_call * call, struct rmib_node * node __unused,
	struct rmib_oldp * oldp, struct rmib_newp * newp __unused)
{
	struct udssock *uds;
	struct kinfo_pcb ki;
	ssize_t off;
	int r, type, size, max;

	if (call->call_namelen != 4)
		return EINVAL;

	/* The first two added name fields are not used. */

	size = call->call_name[2];
	if (size < 0 || (size_t)size > sizeof(ki))
		return EINVAL;
	if (size == 0)
		size = sizeof(ki);
	max = call->call_name[3];

	type = call->call_oname[2];

	off = 0;

	for (uds = uds_enum(NULL, type); uds != NULL;
	    uds = uds_enum(uds, type)) {
		if (rmib_inrange(oldp, off)) {
			memset(&ki, 0, sizeof(ki));

			uds_get_info(&ki, uds);

			if ((r = rmib_copyout(oldp, off, &ki, size)) < 0)
				return r;
		}

		off += size;
		if (max > 0 && --max == 0)
			break;
	}

	/*
	 * Margin to limit the possible effects of the inherent race condition
	 * between receiving just the data size and receiving the actual data.
	 */
	if (oldp == NULL)
		off += PCB_SLOP * size;

	return off;
}

/* The CTL_NET PF_LOCAL SOCK_STREAM subtree. */
static struct rmib_node net_local_stream_table[] = {
	[0]	= RMIB_FUNC(RMIB_RO | CTLTYPE_NODE, 0, net_local_pcblist,
		    "pcblist", "SOCK_STREAM protocol control block list"),
};

/* The CTL_NET PF_LOCAL SOCK_DGRAM subtree. */
static struct rmib_node net_local_dgram_table[] = {
	[0]	= RMIB_FUNC(RMIB_RO | CTLTYPE_NODE, 0, net_local_pcblist,
		    "pcblist", "SOCK_DGRAM protocol control block list"),
};

/* The CTL_NET PF_LOCAL SOCK_SEQPACKET subtree. */
static struct rmib_node net_local_seqpacket_table[] = {
	[0]	= RMIB_FUNC(RMIB_RO | CTLTYPE_NODE, 0, net_local_pcblist,
		    "pcblist", "SOCK_SEQPACKET protocol control block list"),
};

/* The CTL_NET PF_LOCAL subtree. */
static struct rmib_node net_local_table[] = {
/* 1*/	[SOCK_STREAM]		= RMIB_NODE(RMIB_RO, net_local_stream_table,
				    "stream", "SOCK_STREAM settings"),
/* 2*/	[SOCK_DGRAM]		= RMIB_NODE(RMIB_RO, net_local_dgram_table,
				    "dgram", "SOCK_DGRAM settings"),
/* 5*/	[SOCK_SEQPACKET]	= RMIB_NODE(RMIB_RO, net_local_seqpacket_table,
				    "seqpacket", "SOCK_SEQPACKET settings"),
};

static struct rmib_node net_local_node =
    RMIB_NODE(RMIB_RO, net_local_table, "local", "PF_LOCAL related settings");

/*
 * Initialize the status module.
 */
void
uds_stat_init(void)
{
	const int mib[] = { CTL_NET, PF_LOCAL };
	int r;

	/*
	 * Register our own "net.local" subtree with the MIB service.
	 *
	 * This call only returns local failures.  Remote failures (in the MIB
	 * service) are silently ignored.  So, we can safely panic on failure.
	 */
	if ((r = rmib_register(mib, __arraycount(mib), &net_local_node)) != OK)
		panic("UDS: unable to register remote MIB tree: %d", r);
}

/*
 * Clean up the status module.
 */
void
uds_stat_cleanup(void)
{

	rmib_deregister(&net_local_node);
}
