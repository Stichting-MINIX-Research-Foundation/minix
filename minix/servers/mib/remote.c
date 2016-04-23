/* MIB service - remote.c - remote service management and communication */

#include "mib.h"

/*
 * TODO: the main feature that is missing here is a more active way to
 * determine that a particular service has died, so that its mount points can
 * be removed proactively.  Without this, there is a (small) risk that we end
 * up talking to a recycled endpoint with a service that ignores our request,
 * resulting in a deadlock of the MIB service.  Right now, the problem is that
 * there is no proper DS API to subscribe to generic service-down events.
 *
 * In the long term, communication to other services should be made
 * asynchronous, so that the MIB service does not block if there are problems
 * with the other service.  The protocol should already support this, and some
 * simplifications are the result of preparing for future asynchrony support
 * (such as not dynamically querying the remote root node for its properties,
 * which would be very hard to implement in a nonblocking way).  However,
 * actual support is missing.  For now we assume that the remote service either
 * answers the request, or crashes (causing the sendrec to abort), which is
 * mostly good enough.
 */

/* This is the maximum number of remote services that may register subtrees. */
#define MIB_ENDPTS	(1U << MIB_EID_BITS)

/* This is the maximum service label size, including '\0'. */
#define MIB_LABEL_MAX	16

/* Table of remote endpoints, indexed by mount point nodes' node_eid fields. */
static struct {
	endpoint_t endpt;		/* remote endpoint or NONE */
	struct mib_node *nodes;		/* head of list of mount point nodes */
	char label[MIB_LABEL_MAX];	/* label of the remote endpoint */
} endpts[MIB_ENDPTS];

/*
 * Initialize the table of remote endpoints.
 */
void
mib_remote_init(void)
{
	unsigned int i;

	for (i = 0; i < __arraycount(endpts); i++) {
		endpts[i].endpt = NONE;
		endpts[i].nodes = NULL;
	}
}

/*
 * The remote endpoint with the given table index has been determined to have
 * died.  Clean up all its mount points.
 */
static void
mib_down(unsigned int eid)
{
	struct mib_node *node, *next_node;

	assert(endpts[eid].endpt != NONE);
	assert(endpts[eid].nodes != NULL);

	/* Unmount each of the remote endpoint's mount points. */
	for (node = endpts[eid].nodes; node != NULL; node = next_node) {
		/* The unmount call may deallocate the node object. */
		next_node = node->node_next;

		mib_unmount(node);
	}

	/* Mark the entry itself as no longer in use. */
	endpts[eid].endpt = NONE;
	endpts[eid].nodes = NULL;
}

/*
 * Obtain the label for the given endpoint.  On success, return OK and store
 * the label in the given buffer.  If the label cannot be retrieved or does not
 * fit in the given buffer, return a negative error code.
 */
static int
mib_get_label(endpoint_t endpt, char * label, size_t labelsize)
{
	char key[DS_MAX_KEYLEN];
	int r;

	/* TODO: init has a label, so this is not a proper is-service test! */
	if ((r = ds_retrieve_label_name(key, endpt)) != OK) {
		printf("MIB: unable to obtain label for %d\n", endpt);

		return r;
	}

	key[sizeof(key) - 1] = 0;
	if (strlen(key) >= labelsize) {
		/* This should really never happen. */
		printf("MIB: service %d label '%s' is too long\n", endpt, key);

		return ENAMETOOLONG;
	}

	strlcpy(label, key, labelsize);
	return OK;
}

/*
 * Register a remote subtree, mounting it in the local tree as requested.
 */
static void
mib_do_register(endpoint_t endpt, const char * label, uint32_t rid,
	uint32_t flags, unsigned int csize, unsigned int clen, const int * mib,
	unsigned int miblen)
{
	struct mib_node *node;
	unsigned int eid;
	int r, free_eid;

	/*
	 * See if we already have a remote endpoint for the service's label.
	 * If so, we can safely assume that the old endpoint has died and we
	 * have to unmount any previous entries.  Also find a free entry for
	 * the remote endpoint if it is new.
	 */
	free_eid = -1;
	for (eid = 0; eid < __arraycount(endpts); eid++) {
		if (endpts[eid].endpt == endpt)
			break;
		else if (endpts[eid].endpt != NONE &&
		    !strcmp(endpts[eid].label, label)) {
			mib_down(eid);

			assert(endpts[eid].endpt == NONE);
			assert(endpts[eid].nodes == NULL);

			break;
		} else if (endpts[eid].endpt == NONE && free_eid < 0)
			free_eid = eid;
	}

	if (eid == __arraycount(endpts)) {
		if (free_eid < 0) {
			printf("MIB: remote endpoints table is full!\n");

			return;
		}

		eid = free_eid;
	}

	/*
	 * Make sure that the caller does not introduce two mount points with
	 * the same ID.  Right now we refuse such requests; instead, we could
	 * also choose to first deregister the old mount point with this ID.
	 */
	for (node = endpts[eid].nodes; node != NULL; node = node->node_next) {
		if (node->node_rid == rid)
			break;
	}

	if (node != NULL) {
		MIB_DEBUG_MOUNT(("MIB: service %d tried to reuse ID %"PRIu32
		    "\n", endpt, rid));

		return;
	}

	/*
	 * If we did not already have an entry for this endpoint, add one now,
	 * because the mib_mount() call will expect it to be there.  If the
	 * mount call fails, we may have to invalidate the entry again.
	 */
	if (endpts[eid].endpt == NONE) {
		endpts[eid].endpt = endpt;
		endpts[eid].nodes = NULL;
		strlcpy(endpts[eid].label, label, sizeof(endpts[eid].label));
	}

	/* Attempt to mount the remote subtree in the tree. */
	r = mib_mount(mib, miblen, eid, rid, flags, csize, clen, &node);

	if (r != OK) {
		/* If the entry has no other mount points, invalidate it. */
		if (endpts[eid].nodes == NULL)
			endpts[eid].endpt = NONE;

		return;
	}

	/* Add the new node to the list of mount points of the endpoint. */
	node->node_next = endpts[eid].nodes;
	endpts[eid].nodes = node;
}

/*
 * Process a mount point registration request from another service.
 */
int
mib_register(const message * m_in, int ipc_status)
{
	char label[DS_MAX_KEYLEN];

	/*
	 * Registration messages must be one-way, or they may cause a deadlock
	 * if crossed by a request coming from us.  This case also effectively
	 * eliminates the possibility for userland to register nodes.  The
	 * return value of ENOSYS effectively tells userland that this call
	 * number is not in use, which allows us to repurpose call numbers
	 * later.
	 */
	if (IPC_STATUS_CALL(ipc_status) == SENDREC)
		return ENOSYS;

	MIB_DEBUG_MOUNT(("MIB: got register request from %d\n",
	    m_in->m_source));

	/* Double-check if the caller is a service by obtaining its label. */
	if (mib_get_label(m_in->m_source, label, sizeof(label)) != OK)
		return EDONTREPLY;

	/* Perform one message-level bounds check here. */
	if (m_in->m_lsys_mib_register.miblen >
	    __arraycount(m_in->m_lsys_mib_register.mib))
		return EDONTREPLY;

	/* The rest of the work is handled by a message-agnostic function. */
	mib_do_register(m_in->m_source, label,
	    m_in->m_lsys_mib_register.root_id, m_in->m_lsys_mib_register.flags,
	    m_in->m_lsys_mib_register.csize, m_in->m_lsys_mib_register.clen,
	    m_in->m_lsys_mib_register.mib, m_in->m_lsys_mib_register.miblen);

	/* Never reply to this message. */
	return EDONTREPLY;
}

/*
 * Deregister a previously registered remote subtree, unmounting it from the
 * local tree.
 */
static void
mib_do_deregister(endpoint_t endpt, uint32_t rid)
{
	struct mib_node *node, **nodep;
	unsigned int eid;

	for (eid = 0; eid < __arraycount(endpts); eid++) {
		if (endpts[eid].endpt == endpt)
			break;
	}

	if (eid == __arraycount(endpts)) {
		MIB_DEBUG_MOUNT(("MIB: deregister request from unknown "
		    "endpoint %d\n", endpt));

		return;
	}

	for (nodep = &endpts[eid].nodes; *nodep != NULL;
	    nodep = &node->node_next) {
		node = *nodep;

		if (node->node_rid == rid)
			break;
	}

	if (*nodep == NULL) {
		MIB_DEBUG_MOUNT(("MIB: deregister request from %d for unknown "
		    "ID %"PRIu32"\n", endpt, rid));

		return;
	}

	/*
	 * The unmount function may or may not deallocate the node object, so
	 * remove it from the linked list first.  If this leaves an empty
	 * linked list, also mark the remote endpoint entry itself as free.
	 */
	*nodep = node->node_next;

	if (endpts[eid].nodes == NULL) {
		endpts[eid].endpt = NONE;
		endpts[eid].nodes = NULL;
	}

	/* Finally, unmount the remote subtree. */
	mib_unmount(node);
}

/*
 * Process a mount point deregistration request from another service.
 */
int
mib_deregister(const message * m_in, int ipc_status)
{

	/* Same as for registration messages. */
	if (IPC_STATUS_CALL(ipc_status) == SENDREC)
		return ENOSYS;

	MIB_DEBUG_MOUNT(("MIB: got deregister request from %d\n",
	    m_in->m_source));

	/* The rest of the work is handled by a message-agnostic function. */
	mib_do_deregister(m_in->m_source, m_in->m_lsys_mib_register.root_id);

	/* Never reply to this message. */
	return EDONTREPLY;
}

/*
 * Retrieve information about the root of a remote subtree, specifically its
 * name and description.  This is done only when there was no corresponding
 * local node and one has to be created temporarily.  On success, return OK
 * with the name and description stored in the given buffers.  Otherwise,
 * return a negative error code.
 */
int
mib_remote_info(unsigned int eid, uint32_t rid, char * name, size_t namesize,
	char * desc, size_t descsize)
{
	endpoint_t endpt;
	cp_grant_id_t name_grant, desc_grant;
	message m;
	int r;

	if (eid >= __arraycount(endpts) || endpts[eid].endpt == NONE)
		return EINVAL;

	endpt = endpts[eid].endpt;

	if ((name_grant = cpf_grant_direct(endpt, (vir_bytes)name, namesize,
	    CPF_WRITE)) == GRANT_INVALID)
		return EINVAL;

	if ((desc_grant = cpf_grant_direct(endpt, (vir_bytes)desc, descsize,
	    CPF_WRITE)) == GRANT_INVALID) {
		cpf_revoke(name_grant);

		return EINVAL;
	}

	memset(&m, 0, sizeof(m));

	m.m_type = COMMON_MIB_INFO;
	m.m_mib_lsys_info.req_id = 0; /* reserved for future async support */
	m.m_mib_lsys_info.root_id = rid;
	m.m_mib_lsys_info.name_grant = name_grant;
	m.m_mib_lsys_info.name_size = namesize;
	m.m_mib_lsys_info.desc_grant = desc_grant;
	m.m_mib_lsys_info.desc_size = descsize;

	r = ipc_sendrec(endpt, &m);

	cpf_revoke(desc_grant);
	cpf_revoke(name_grant);

	if (r != OK)
		return r;

	if (m.m_type != COMMON_MIB_REPLY)
		return EINVAL;
	if (m.m_lsys_mib_reply.req_id != 0)
		return EINVAL;

	return m.m_lsys_mib_reply.status;
}

/*
 * Relay a sysctl(2) call from a user process to a remote service, because the
 * call reached a mount point into a remote subtree.  Return the result code
 * from the remote service.  Alternatively, return ERESTART if it has been
 * determined that the remote service is dead, in which case its mount points
 * will have been removed (possibly including the entire given node), and the
 * caller should continue the call on the underlying local subtree if there is
 * any.  Note that the remote service may also return ERESTART to indicate that
 * the remote subtree does not exist, either because it is being deregistered
 * or because the remote service was restarted with loss of state.
 */
ssize_t
mib_remote_call(struct mib_call * call, struct mib_node * node,
	struct mib_oldp * oldp, struct mib_newp * newp)
{
	cp_grant_id_t name_grant, oldp_grant, newp_grant;
	size_t oldp_len, newp_len;
	endpoint_t endpt;
	message m;
	int r;

	endpt = endpts[node->node_eid].endpt;
	assert(endpt != NONE);

	/*
	 * Allocate grants.  Since ENOMEM has a special meaning for sysctl(2),
	 * never return that code even if it is the most appropriate one.
	 * The remainder of the name may be empty; the callee should check.
	 */
	name_grant = cpf_grant_direct(endpt, (vir_bytes)call->call_name,
	    call->call_namelen * sizeof(call->call_name[0]), CPF_READ);
	if (!GRANT_VALID(name_grant))
		return EINVAL;

	if ((r = mib_relay_oldp(endpt, oldp, &oldp_grant, &oldp_len)) != OK) {
		cpf_revoke(name_grant);

		return r;
	}

	if ((r = mib_relay_newp(endpt, newp, &newp_grant, &newp_len)) != OK) {
		if (GRANT_VALID(oldp_grant))
			cpf_revoke(oldp_grant);
		cpf_revoke(name_grant);

		return r;
	}

	/*
	 * Construct the request message.  We have not optimized this flow for
	 * performance.  In particular, we never embed even short names in the
	 * message, and we supply a flag indicating whether the caller is root
	 * regardless of whether the callee is interested in this.  This is
	 * more convenient for the callee, but also more costly.
	 */
	memset(&m, 0, sizeof(m));

	m.m_type = COMMON_MIB_CALL;
	m.m_mib_lsys_call.req_id = 0; /* reserved for future async support */
	m.m_mib_lsys_call.root_id = node->node_rid;
	m.m_mib_lsys_call.name_grant = name_grant;
	m.m_mib_lsys_call.name_len = call->call_namelen;
	m.m_mib_lsys_call.oldp_grant = oldp_grant;
	m.m_mib_lsys_call.oldp_len = oldp_len;
	m.m_mib_lsys_call.newp_grant = newp_grant;
	m.m_mib_lsys_call.newp_len = newp_len;
	m.m_mib_lsys_call.user_endpt = call->call_endpt;
	m.m_mib_lsys_call.flags = !!mib_authed(call); /* TODO: define flags */
	m.m_mib_lsys_call.root_ver = node->node_ver;
	m.m_mib_lsys_call.tree_ver = mib_root.node_ver;

	/* Issue a synchronous call to the remove service. */
	r = ipc_sendrec(endpt, &m);

	/* Then first clean up. */
	if (GRANT_VALID(newp_grant))
		cpf_revoke(newp_grant);
	if (GRANT_VALID(oldp_grant))
		cpf_revoke(oldp_grant);
	cpf_revoke(name_grant);

	/*
	 * Treat any IPC-level error as an indication that there is a problem
	 * with the remote service.  Declare it dead, remove all its mount
	 * points, and return ERESTART to indicate to the caller that it should
	 * (carefully) try to continue the request on a local subtree instead.
	 * Again: mib_down() may actually deallocate the given 'node' object.
	 */
	if (r != OK) {
		mib_down(node->node_eid);

		return ERESTART;
	}

	if (m.m_type != COMMON_MIB_REPLY)
		return EINVAL;
	if (m.m_lsys_mib_reply.req_id != 0)
		return EINVAL;

	/*
	 * If a deregister message from the service crosses our call, we'll get
	 * the response before we get the deregister request.  In that case,
	 * the remote service should return ERESTART to indicate that the mount
	 * point does not exist as far as it is concerned, so that we can try
	 * the local version of the tree instead.
	 */
	if (m.m_lsys_mib_reply.status == ERESTART)
		mib_do_deregister(endpt, node->node_rid);

	return m.m_lsys_mib_reply.status;
}
