/* Service support for remote MIB subtrees - by D.C. van Moolenbroek */
/*
 * In effect, this is a lightweight version of the MIB service's main and tree
 * code.  Some parts of the code have even been copied almost as is, even
 * though the copy here operates on slightly different data structures in order
 * to keep the implementation more lightweight.  For clarification on many
 * aspects of the source code here, see the source code of the MIB service.
 * One unique feature here is support for sparse nodes, which is needed for
 * net.inet/inet6 as those are using subtrees with protocol-based identifiers.
 *
 * There is no way for this module to get to know about MIB service deaths
 * without possibly interfering with the main code of the service this module
 * is a part of.  As a result, re-registration of mount points after a MIB
 * service restart is not automatic.  Instead, the main service code should
 * provide detection of MIB service restarts, and call rmib_reregister() after
 * such a restart in order to remount any previously mounted subtrees.
 */

#include <minix/drivers.h>
#include <minix/sysctl.h>
#include <minix/rmib.h>

/* Structures for outgoing and incoming data, deliberately distinctly named. */
struct rmib_oldp {
	cp_grant_id_t oldp_grant;
	size_t oldp_len;
};

struct rmib_newp {
	cp_grant_id_t newp_grant;
	size_t newp_len;
};

/*
 * The maximum field size, in bytes, for which updates (i.e., writes) to the
 * field do not require dynamic memory allocation.  By policy, non-root users
 * may not update fields exceeding this size at all.  For strings, this size
 * includes an extra byte for adding a null terminator if missing.  As the name
 * indicates, a buffer of this size is placed on the stack.
 */
#define RMIB_STACKBUF		257

/*
 * The maximum number of subtrees that this service can mount.  This value can
 * be increased without any problems, but it is already quite high in practice.
 */
#define RMIB_MAX_SUBTREES	16

/*
 * The array of subtree root nodes.  Each root node's array index is the root
 * identifier used in communication with the MIB service.
 */
static struct {
	struct rmib_node *rno_node;
	unsigned int rno_namelen;
	int rno_name[CTL_SHORTNAME];
} rnodes[RMIB_MAX_SUBTREES] = { { NULL, 0, { 0 } } };

/*
 * Return TRUE or FALSE indicating whether the given offset is within the range
 * of data that is to be copied out.  This call can be used to test whether
 * certain bits of data need to be prepared for copying at all.
 */
int
rmib_inrange(struct rmib_oldp * oldp, size_t off)
{

	if (oldp == NULL)
		return FALSE;

	return (off < oldp->oldp_len);
}

/*
 * Return the total length of the requested data.  This should not be used
 * directly except in highly unusual cases, such as particular node requests
 * where the request semantics blatantly violate overall sysctl(2) semantics.
 */
size_t
rmib_getoldlen(struct rmib_oldp * oldp)
{

	if (oldp == NULL)
		return 0;

	return oldp->oldp_len;
}

/*
 * Copy out (partial) data to the user.  The copy is automatically limited to
 * the range of data requested by the user.  Return the requested length on
 * success (for the caller's convenience) or an error code on failure.
 */
ssize_t
rmib_copyout(struct rmib_oldp * __restrict oldp, size_t off,
	const void * __restrict buf, size_t size)
{
	size_t len;
	int r;

	len = size;
	assert(len <= SSIZE_MAX);

	if (oldp == NULL || off >= oldp->oldp_len)
		return size; /* nothing to do */

	if (len > oldp->oldp_len - off)
		len = oldp->oldp_len - off;

	if ((r = sys_safecopyto(MIB_PROC_NR, oldp->oldp_grant, off,
	    (vir_bytes)buf, len)) != OK)
		return r;

	return size;
}

/*
 * Copy out (partial) data to the user, from a vector of up to RMIB_IOV_MAX
 * local buffers.  The copy is automatically limited to the range of data
 * requested by the user.  Return the total requested length on success or an
 * error code on failure.
 */
ssize_t
rmib_vcopyout(struct rmib_oldp * oldp, size_t off, const iovec_t * iov,
	unsigned int iovcnt)
{
	static struct vscp_vec vec[RMIB_IOV_MAX];
	size_t size, chunk;
	unsigned int i;
	ssize_t r;

	assert(iov != NULL);
	assert(iovcnt <= __arraycount(vec));

	/* Take a shortcut for single-vector elements, saving a kernel copy. */
	if (iovcnt == 1)
		return rmib_copyout(oldp, off, (const void *)iov->iov_addr,
		    iov->iov_size);

	/*
	 * Iterate through the full vector even if we cannot copy out all of
	 * it, because we need to compute the total length.
	 */
	for (size = i = 0; iovcnt > 0; iov++, iovcnt--) {
		if (oldp != NULL && off < oldp->oldp_len) {
			chunk = oldp->oldp_len - off;
			if (chunk > iov->iov_size)
				chunk = iov->iov_size;

			vec[i].v_from = SELF;
			vec[i].v_to = MIB_PROC_NR;
			vec[i].v_gid = oldp->oldp_grant;
			vec[i].v_offset = off;
			vec[i].v_addr = iov->iov_addr;
			vec[i].v_bytes = chunk;

			off += chunk;
			i++;
		}

		size += iov->iov_size;
	}

	/* Perform the copy, if there is anything to copy, that is. */
	if (i > 0 && (r = sys_vsafecopy(vec, i)) != OK)
		return r;

	return size;
}

/*
 * Copy in data from the user.  The given length must match exactly the length
 * given by the user.  Return OK or an error code.
 */
int
rmib_copyin(struct rmib_newp * __restrict newp, void * __restrict buf,
	size_t len)
{

	if (newp == NULL || len != newp->newp_len)
		return EINVAL;

	if (len == 0)
		return OK;

	return sys_safecopyfrom(MIB_PROC_NR, newp->newp_grant, 0,
	    (vir_bytes)buf, len);
}

/*
 * Copy out a node to userland, using the exchange format for nodes (namely,
 * a sysctlnode structure).  Return the size of the object that is (or, if the
 * node falls outside the requested data range, would be) copied out on
 * success, or a negative error code on failure.
 */
static ssize_t
rmib_copyout_node(struct rmib_call * call, struct rmib_oldp * oldp,
	ssize_t off, unsigned int id, const struct rmib_node * rnode)
{
	struct sysctlnode scn;
	int visible;

	if (!rmib_inrange(oldp, off))
		return sizeof(scn); /* nothing to do */

	memset(&scn, 0, sizeof(scn));

	/*
	 * We use CTLFLAG_SPARSE internally only.  NetBSD uses these flags for
	 * different purposes.  Either way, do not expose it to userland.
	 * hide any of them from the user.
	 */
	scn.sysctl_flags = SYSCTL_VERSION |
	    (rnode->rnode_flags & ~CTLFLAG_SPARSE);
	scn.sysctl_num = id;
	strlcpy(scn.sysctl_name, rnode->rnode_name, sizeof(scn.sysctl_name));
	scn.sysctl_ver = call->call_rootver;
	scn.sysctl_size = rnode->rnode_size;

	/* Some information is only visible if the user can access the node. */
	visible = (!(rnode->rnode_flags & CTLFLAG_PRIVATE) ||
	    (call->call_flags & RMIB_FLAG_AUTH));

	/*
	 * For immediate types, store the immediate value in the resulting
	 * structure, unless the caller is not authorized to obtain the value.
	 */
	if ((rnode->rnode_flags & CTLFLAG_IMMEDIATE) && visible) {
		switch (SYSCTL_TYPE(rnode->rnode_flags)) {
		case CTLTYPE_BOOL:
			scn.sysctl_bdata = rnode->rnode_bool;
			break;
		case CTLTYPE_INT:
			scn.sysctl_idata = rnode->rnode_int;
			break;
		case CTLTYPE_QUAD:
			scn.sysctl_qdata = rnode->rnode_quad;
			break;
		}
	}

	/* Special rules apply to parent nodes. */
	if (SYSCTL_TYPE(rnode->rnode_flags) == CTLTYPE_NODE) {
		/* Report the node size the way NetBSD does, just in case. */
		scn.sysctl_size = sizeof(scn);

		/*
		 * For real parent nodes, report child information, but only if
		 * the node itself is accessible by the caller.  For function-
		 * driven nodes, set a nonzero function address, for trace(1).
		 */
		if (rnode->rnode_func == NULL && visible) {
			scn.sysctl_csize = rnode->rnode_size;
			scn.sysctl_clen = rnode->rnode_clen;
		} else if (rnode->rnode_func != NULL)
			scn.sysctl_func = SYSCTL_NODE_FN;
	}

	/* Copy out the resulting node. */
	return rmib_copyout(oldp, off, &scn, sizeof(scn));
}

/*
 * Given a query on a non-leaf (parent) node, provide the user with an array of
 * this node's children.
 */
static ssize_t
rmib_query(struct rmib_call * call, struct rmib_node * rparent,
	struct rmib_oldp * oldp, struct rmib_newp * newp)
{
	struct sysctlnode scn;
	struct rmib_node *rnode;
	unsigned int i, id;
	ssize_t r, off;

	/* If the user passed in version numbers, check them. */
	if (newp != NULL) {
		if ((r = rmib_copyin(newp, &scn, sizeof(scn))) != OK)
			return r;

		if (SYSCTL_VERS(scn.sysctl_flags) != SYSCTL_VERSION)
			return EINVAL;

		/*
		 * If a node version number is given, it must match the version
		 * of the subtree or the root of the entire MIB version.
		 */
		if (scn.sysctl_ver != 0 &&
		    scn.sysctl_ver != call->call_rootver &&
		    scn.sysctl_ver != call->call_treever)
			return EINVAL;
	}

	/* Enumerate the child nodes of the given parent node. */
	off = 0;

	for (i = 0; i < rparent->rnode_size; i++) {
		if (rparent->rnode_flags & CTLFLAG_SPARSE) {
			id = rparent->rnode_icptr[i].rindir_id;
			rnode = rparent->rnode_icptr[i].rindir_node;
		} else {
			id = i;
			rnode = &rparent->rnode_cptr[i];

			if (rnode->rnode_flags == 0)
				continue;
		}

		if ((r = rmib_copyout_node(call, oldp, off, id, rnode)) < 0)
			return r;
		off += r;
	}

	return off;
}

/*
 * Copy out a node description to userland, using the exchange format for node
 * descriptions (namely, a sysctldesc structure).  Return the size of the
 * object that is (or, if the description falls outside the requested data
 * range, would be) copied out on success, or a negative error code on failure.
 * The function may return 0 to indicate that nothing was copied out after all.
 */
static ssize_t
rmib_copyout_desc(struct rmib_call * call, struct rmib_oldp * oldp,
	ssize_t off, unsigned int id, const struct rmib_node * rnode)
{
	struct sysctldesc scd;
	size_t len, size;
	ssize_t r;

	/* Descriptions of private nodes are considered private too. */
	if ((rnode->rnode_flags & CTLFLAG_PRIVATE) &&
	    !(call->call_flags & RMIB_FLAG_AUTH))
		return 0;

	/*
	 * Unfortunately, we do not have a scratch buffer here.  Instead, copy
	 * out the description structure and the actual description string
	 * separately.  This is more costly, but remote subtrees are already
	 * not going to give the best performance ever.  We do optimize for the
	 * case that there is no description, because that is relatively easy.
	 */
	/* The description length includes the null terminator. */
	if (rnode->rnode_desc != NULL)
		len = strlen(rnode->rnode_desc) + 1;
	else
		len = 1;

	memset(&scd, 0, sizeof(scd));
	scd.descr_num = id;
	scd.descr_ver = call->call_rootver;
	scd.descr_len = len;

	size = offsetof(struct sysctldesc, descr_str);

	if (len == 1) {
		scd.descr_str[0] = '\0'; /* superfluous */
		size++;
	}

	/* Copy out the structure, possibly including a null terminator. */
	if ((r = rmib_copyout(oldp, off, &scd, size)) < 0)
		return r;

	if (len > 1) {
		/* Copy out the description itself. */
		if ((r = rmib_copyout(oldp, off + size, rnode->rnode_desc,
		    len)) < 0)
			return r;

		size += len;
	}

	/*
	 * By aligning just the size, we may leave garbage between the entries
	 * copied out, which is fine because it is userland's own data.
	 */
	return roundup2(size, sizeof(int32_t));
}

/*
 * Look up a child node given a parent node and a child node identifier.
 * Return a pointer to the child node if found, or NULL otherwise.  The lookup
 * procedure differs based on whether the parent node is sparse or not.
 */
static struct rmib_node *
rmib_lookup(struct rmib_node * rparent, unsigned int id)
{
	struct rmib_node *rnode;
	struct rmib_indir *rindir;
	unsigned int i;

	if (rparent->rnode_flags & CTLFLAG_SPARSE) {
		rindir = rparent->rnode_icptr;
		for (i = 0; i < rparent->rnode_size; i++, rindir++)
			if (rindir->rindir_id == id)
				return rindir->rindir_node;
	} else {
		if (id >= rparent->rnode_size)
			return NULL;
		rnode = &rparent->rnode_cptr[id];
		if (rnode->rnode_flags != 0)
			return rnode;
	}

	return NULL;
}

/*
 * Retrieve node descriptions in bulk, or retrieve a particular node's
 * description.
 */
static ssize_t
rmib_describe(struct rmib_call * call, struct rmib_node * rparent,
	struct rmib_oldp * oldp, struct rmib_newp * newp)
{
	struct sysctlnode scn;
	struct rmib_node *rnode;
	unsigned int i, id;
	ssize_t r, off;

	if (newp != NULL) {
		if ((r = rmib_copyin(newp, &scn, sizeof(scn))) != OK)
			return r;

		if (SYSCTL_VERS(scn.sysctl_flags) != SYSCTL_VERSION)
			return EINVAL;

		/* Locate the child node. */
		if ((rnode = rmib_lookup(rparent, scn.sysctl_num)) == NULL)
			return ENOENT;

		/* Descriptions of private nodes are considered private too. */
		if ((rnode->rnode_flags & CTLFLAG_PRIVATE) &&
		    !(call->call_flags & RMIB_FLAG_AUTH))
			return EPERM;

		/*
		 * If a description pointer was given, this is a request to
		 * set the node's description.  We do not allow this, nor would
		 * we be able to support it, since we cannot access the data.
		 */
		if (scn.sysctl_desc != NULL)
			return EPERM;

		/*
		 * Copy out the requested node's description.  At this point we
		 * should be sure that this call does not return zero.
		 */
		return rmib_copyout_desc(call, oldp, 0, scn.sysctl_num, rnode);
	}

	/* Describe the child nodes of the given parent node. */
	off = 0;

	for (i = 0; i < rparent->rnode_size; i++) {
		if (rparent->rnode_flags & CTLFLAG_SPARSE) {
			id = rparent->rnode_icptr[i].rindir_id;
			rnode = rparent->rnode_icptr[i].rindir_node;
		} else {
			id = i;
			rnode = &rparent->rnode_cptr[i];

			if (rnode->rnode_flags == 0)
				continue;
		}

		if ((r = rmib_copyout_desc(call, oldp, off, id, rnode)) < 0)
			return r;
		off += r;
	}

	return off;
}

/*
 * Return a pointer to the data associated with the given node, or NULL if the
 * node has no associated data.  Actual calls to this function should never
 * result in NULL - as long as the proper rules are followed elsewhere.
 */
static void *
rmib_getptr(struct rmib_node * rnode)
{

	switch (SYSCTL_TYPE(rnode->rnode_flags)) {
	case CTLTYPE_BOOL:
		if (rnode->rnode_flags & CTLFLAG_IMMEDIATE)
			return &rnode->rnode_bool;
		break;
	case CTLTYPE_INT:
		if (rnode->rnode_flags & CTLFLAG_IMMEDIATE)
			return &rnode->rnode_int;
		break;
	case CTLTYPE_QUAD:
		if (rnode->rnode_flags & CTLFLAG_IMMEDIATE)
			return &rnode->rnode_quad;
		break;
	case CTLTYPE_STRING:
	case CTLTYPE_STRUCT:
		if (rnode->rnode_flags & CTLFLAG_IMMEDIATE)
			return NULL;
		break;
	default:
		return NULL;
	}

	return rnode->rnode_data;
}

/*
 * Read current (old) data from a regular data node, if requested.  Return the
 * old data length.
 */
static ssize_t
rmib_read(struct rmib_node * rnode, struct rmib_oldp * oldp)
{
	void *ptr;
	size_t oldlen;
	int r;

	if ((ptr = rmib_getptr(rnode)) == NULL)
		return EINVAL;

	if (SYSCTL_TYPE(rnode->rnode_flags) == CTLTYPE_STRING)
		oldlen = strlen(rnode->rnode_data) + 1;
	else
		oldlen = rnode->rnode_size;

	if (oldlen > SSIZE_MAX)
		return EINVAL;

	/* Copy out the current data, if requested at all. */
	if (oldp != NULL && (r = rmib_copyout(oldp, 0, ptr, oldlen)) < 0)
		return r;

	/* Return the current length in any case. */
	return (ssize_t)oldlen;
}

/*
 * Write new data into a regular data node, if requested.
 */
static int
rmib_write(struct rmib_call * call, struct rmib_node * rnode,
	struct rmib_newp * newp)
{
	bool b[(sizeof(bool) == sizeof(char)) ? 1 : -1]; /* for sanitizing */
	char *src, *dst, buf[RMIB_STACKBUF];
	size_t newlen;
	int r;

	if (newp == NULL)
		return OK; /* nothing to do */

	/*
	 * When setting a new value, we cannot risk doing an in-place update:
	 * the copy from userland may fail halfway through, in which case an
	 * in-place update could leave the node value in a corrupted state.
	 * Thus, we must first fetch any new data into a temporary buffer.
	 */
	newlen = newp->newp_len;

	if ((dst = rmib_getptr(rnode)) == NULL)
		return EINVAL;

	switch (SYSCTL_TYPE(rnode->rnode_flags)) {
	case CTLTYPE_BOOL:
	case CTLTYPE_INT:
	case CTLTYPE_QUAD:
	case CTLTYPE_STRUCT:
		/* Non-string types must have an exact size match. */
		if (newlen != rnode->rnode_size)
			return EINVAL;
		break;
	case CTLTYPE_STRING:
		/*
		 * Strings must not exceed their buffer size.  There is a
		 * second check further below, because we allow userland to
		 * give us an unterminated string.  In that case we terminate
		 * it ourselves, but then the null terminator must fit as well.
		 */
		if (newlen > rnode->rnode_size)
			return EINVAL;
		break;
	default:
		return EINVAL;
	}

	/*
	 * If we cannot fit the data in the small stack buffer, then allocate a
	 * temporary buffer.  We add one extra byte so that we can add a null
	 * terminator at the end of strings in case userland did not supply
	 * one.  Either way, we must free the temporary buffer later!
	 */
	if (newlen + 1 > sizeof(buf)) {
		/*
		 * For regular users, we do not want to perform dynamic memory
		 * allocation.  Thus, for CTLTYPE_ANYWRITE nodes, only the
		 * superuser may set values exceeding the small buffer in size.
		 */
		if (!(call->call_flags & RMIB_FLAG_AUTH))
			return EPERM;

		/* Do not return ENOMEM on allocation failure. */
		if ((src = malloc(newlen + 1)) == NULL)
			return EINVAL;
	} else
		src = buf;

	/* Copy in the data.  Note that the given new length may be zero. */
	if ((r = rmib_copyin(newp, src, newlen)) == OK) {
		/* Check and, if acceptable, store the new value. */
		switch (SYSCTL_TYPE(rnode->rnode_flags)) {
		case CTLTYPE_BOOL:
			/* Sanitize booleans.  See the MIB code for details. */
			b[0] = (bool)src[0];
			memcpy(dst, &b[0], sizeof(b[0]));
			break;
		case CTLTYPE_INT:
		case CTLTYPE_QUAD:
		case CTLTYPE_STRUCT:
			memcpy(dst, src, rnode->rnode_size);
			break;
		case CTLTYPE_STRING:
			if (newlen == rnode->rnode_size &&
			    src[newlen - 1] != '\0') {
				/* Our null terminator does not fit! */
				r = EINVAL;
				break;
			}
			src[newlen] = '\0';
			strlcpy(dst, src, rnode->rnode_size);
			break;
		default:
			r = EINVAL;
		}
	}

	if (src != buf)
		free(src);

	return r;
}

/*
 * Read and/or write the value of a regular data node.  A regular data node is
 * a leaf node.  Typically, a leaf node has no associated function, in which
 * case this function will be used instead.  In addition, this function may be
 * used from handler functions as part of their functionality.
 */
ssize_t
rmib_readwrite(struct rmib_call * call, struct rmib_node * rnode,
	struct rmib_oldp * oldp, struct rmib_newp * newp)
{
	ssize_t len;
	int r;

	/* Copy out old data, if requested.  Always get the old data length. */
	if ((r = len = rmib_read(rnode, oldp)) < 0)
		return r;

	/* Copy in new data, if requested. */
	if ((r = rmib_write(call, rnode, newp)) != OK)
		return r;

	/* Return the old data length. */
	return len;
}

/*
 * Handle a sysctl(2) call from a user process, relayed by the MIB service to
 * us.  If the call succeeds, return the old length.  The MIB service will
 * perform a check against the given old length and return ENOMEM to the caller
 * when applicable, so we do not have to do that here.  If the call fails,
 * return a negative error code.
 */
static ssize_t
rmib_call(const message * m_in)
{
	struct rmib_node *rnode, *rparent;
	struct rmib_call call;
	struct rmib_oldp oldp_data, *oldp;
	struct rmib_newp newp_data, *newp;
	unsigned int root_id, prefixlen, namelen;
	int r, id, is_leaf, has_func, name[CTL_MAXNAME];

	/*
	 * Look up the root of the subtree that is the subject of the call.  If
	 * the call is for a subtree that is not registered, return ERESTART to
	 * indicate to the MIB service that it should deregister the subtree it
	 * thinks we have.  This case may occur in practice if a deregistration
	 * request from us crosses a sysctl call request from the MIB service.
	 */
	root_id = m_in->m_mib_lsys_call.root_id;
	if (root_id >= __arraycount(rnodes) ||
	    (rnode = rnodes[root_id].rno_node) == NULL)
		return ERESTART;

	/*
	 * Use the name of the mounted subtree as prefix to the given name, so
	 * that call_oname will point to the complete name of the node.  This
	 * is necessary for the few queries that make use of call_oname.
	 */
	prefixlen = rnodes[root_id].rno_namelen;
	memcpy(name, rnodes[root_id].rno_name, prefixlen * sizeof(name[0]));

	/*
	 * Set up all data structures that we need to use while handling the
	 * call processing.  Start by copying in the remainder of the MIB name.
	 */
	/* A zero name length is valid and should always yield EISDIR. */
	namelen = m_in->m_mib_lsys_call.name_len;
	if (prefixlen + namelen > __arraycount(name))
		return EINVAL;

	if (namelen > 0) {
		r = sys_safecopyfrom(m_in->m_source,
		    m_in->m_mib_lsys_call.name_grant, 0,
		    (vir_bytes)&name[prefixlen], sizeof(name[0]) * namelen);
		if (r != OK)
			return r;
	}

	oldp_data.oldp_grant = m_in->m_mib_lsys_call.oldp_grant;
	oldp_data.oldp_len = m_in->m_mib_lsys_call.oldp_len;
	oldp = (GRANT_VALID(oldp_data.oldp_grant)) ? &oldp_data : NULL;

	newp_data.newp_grant = m_in->m_mib_lsys_call.newp_grant;
	newp_data.newp_len = m_in->m_mib_lsys_call.newp_len;
	newp = (GRANT_VALID(newp_data.newp_grant)) ? &newp_data : NULL;

	call.call_endpt = m_in->m_mib_lsys_call.user_endpt;
	call.call_oname = name;
	call.call_name = &name[prefixlen];
	call.call_namelen = namelen;
	call.call_flags = m_in->m_mib_lsys_call.flags;
	call.call_rootver = m_in->m_mib_lsys_call.root_ver;
	call.call_treever = m_in->m_mib_lsys_call.tree_ver;

	/*
	 * Dispatch the call.
	 */
	for (rparent = rnode; call.call_namelen > 0; rparent = rnode) {
		id = call.call_name[0];
		call.call_name++;
		call.call_namelen--;

		assert(SYSCTL_TYPE(rparent->rnode_flags) == CTLTYPE_NODE);

		/* Check for meta-identifiers. */
		if (id < 0) {
			/*
			 * A meta-identifier must always be the last name
			 * component.
			 */
			if (call.call_namelen > 0)
				return EINVAL;

			switch (id) {
			case CTL_QUERY:
				return rmib_query(&call, rparent, oldp, newp);
			case CTL_DESCRIBE:
				return rmib_describe(&call, rparent, oldp,
				    newp);
			case CTL_CREATE:
			case CTL_DESTROY:
				/* We support fully static subtrees only. */
				return EPERM;
			default:
				return EOPNOTSUPP;
			}
		}

		/* Locate the child node. */
		if ((rnode = rmib_lookup(rparent, id)) == NULL)
			return ENOENT;

		/* Check if access is permitted at this level. */
		if ((rnode->rnode_flags & CTLFLAG_PRIVATE) &&
		    !(call.call_flags & RMIB_FLAG_AUTH))
			return EPERM;

		/*
		 * Is this a leaf node, and/or is this node handled by a
		 * function?  If either is true, resolution ends at this level.
		 */
		is_leaf = (SYSCTL_TYPE(rnode->rnode_flags) != CTLTYPE_NODE);
		has_func = (rnode->rnode_func != NULL);

		/*
		 * The name may be longer only if the node is not a leaf.  That
		 * also applies to leaves with functions, so check this first.
		 */
		if (is_leaf && call.call_namelen > 0)
			return ENOTDIR;

		/*
		 * If resolution indeed ends here, and the user supplied new
		 * data, check if writing is allowed.
		 */
		if ((is_leaf || has_func) && newp != NULL) {
			if (!(rnode->rnode_flags & CTLFLAG_READWRITE))
				return EPERM;

			if (!(rnode->rnode_flags & CTLFLAG_ANYWRITE) &&
			    !(call.call_flags & RMIB_FLAG_AUTH))
				return EPERM;
		}

		/* If this node has a handler function, let it do the work. */
		if (has_func)
			return rnode->rnode_func(&call, rnode, oldp, newp);

		/* For regular data leaf nodes, handle generic access. */
		if (is_leaf)
			return rmib_readwrite(&call, rnode, oldp, newp);

		/* No function and not a leaf?  Descend further. */
	}

	/* If we get here, the name refers to a node array. */
	return EISDIR;
}

/*
 * Initialize the given node and recursively all its node-type children,
 * assigning the proper child length value to each of them.
 */
static void
rmib_init(struct rmib_node * rparent)
{
	struct rmib_node *rnode;
	unsigned int i;

	for (i = 0; i < rparent->rnode_size; i++) {
		if (rparent->rnode_flags & CTLFLAG_SPARSE) {
			/* Indirect lists must be sorted ascending by ID. */
			assert(i == 0 || rparent->rnode_icptr[i].rindir_id >
			    rparent->rnode_icptr[i - 1].rindir_id);

			rnode = rparent->rnode_icptr[i].rindir_node;
		} else {
			rnode = &rparent->rnode_cptr[i];

			if (rnode->rnode_flags == 0)
				continue;
		}

		rparent->rnode_clen++;

		if (SYSCTL_TYPE(rnode->rnode_flags) == CTLTYPE_NODE)
			rmib_init(rnode); /* recurse */
	}
}

/*
 * Request that the MIB service (re)mount the subtree identified by the given
 * identifier.  This is a one-way request, so we never hear whether mounting
 * succeeds.  There is not that much we can do if it fails anyway though.
 */
static void
rmib_send_reg(int id)
{
	message m;
	int r;

	memset(&m, 0, sizeof(m));

	m.m_type = MIB_REGISTER;
	m.m_lsys_mib_register.root_id = id;
	m.m_lsys_mib_register.flags = SYSCTL_VERSION |
	    (rnodes[id].rno_node->rnode_flags & ~CTLFLAG_SPARSE);
	m.m_lsys_mib_register.csize = rnodes[id].rno_node->rnode_size;
	m.m_lsys_mib_register.clen = rnodes[id].rno_node->rnode_clen;
	m.m_lsys_mib_register.miblen = rnodes[id].rno_namelen;
	memcpy(m.m_lsys_mib_register.mib, rnodes[id].rno_name,
	    sizeof(rnodes[id].rno_name[0]) * rnodes[id].rno_namelen);

	if ((r = asynsend3(MIB_PROC_NR, &m, AMF_NOREPLY)) != OK)
		panic("asynsend3 call to MIB service failed: %d", r);
}

/*
 * Register a MIB subtree.  Initialize the subtree, add it to the local set,
 * and send a registration request for it to the MIB service.
 */
int
rmib_register(const int * name, unsigned int namelen, struct rmib_node * rnode)
{
	unsigned int id, free_id;

	/* A few basic sanity checks. */
	if (namelen == 0 || namelen >= __arraycount(rnodes[0].rno_name))
		return EINVAL;
	if (SYSCTL_TYPE(rnode->rnode_flags) != CTLTYPE_NODE)
		return EINVAL;

	/* Make sure this is a new subtree, and find a free slot for it. */
	for (id = free_id = 0; id < __arraycount(rnodes); id++) {
		if (rnodes[id].rno_node == rnode)
			return EEXIST;
		else if (rnodes[id].rno_node == NULL &&
		    rnodes[free_id].rno_node != NULL)
			free_id = id;
	}

	if (rnodes[free_id].rno_node != NULL)
		return ENOMEM;

	rnodes[free_id].rno_node = rnode;
	rnodes[free_id].rno_namelen = namelen;
	memcpy(rnodes[free_id].rno_name, name, sizeof(name[0]) * namelen);

	/*
	 * Initialize the entire subtree.  This will also compute rnode_clen
	 * for the given rnode, so do this before sending the message.
	 */
	rmib_init(rnode);

	/* Send the registration request to the MIB service. */
	rmib_send_reg(free_id);

	return OK;
}

/*
 * Deregister a previously registered subtree, both internally and with the MIB
 * service.  Return OK if the deregistration procedure has been started, in
 * which case the given subtree is guaranteed to no longer be accessed.  Return
 * a negative error code on failure.
 */
int
rmib_deregister(struct rmib_node * rnode)
{
	message m;
	unsigned int id;

	for (id = 0; id < __arraycount(rnodes); id++)
		if (rnodes[id].rno_node == rnode)
			break;

	if (id == __arraycount(rnodes))
		return ENOENT;

	rnodes[id].rno_node = NULL;

	/*
	 * Request that the MIB service unmount the subtree.  We completely
	 * ignore failure here, because the caller would not be able to do
	 * anything about it anyway.  We may also still receive sysctl call
	 * requests for the node we just deregistered, but this is caught
	 * during request processing.  Reuse of the rnodes[] slot could be a
	 * potential problem though.  We could use sequence numbers in the root
	 * identifiers to resolve that problem if it ever occurs in reality.
	 */
	memset(&m, 0, sizeof(m));

	m.m_type = MIB_DEREGISTER;
	m.m_lsys_mib_register.root_id = id;

	(void)asynsend3(MIB_PROC_NR, &m, AMF_NOREPLY);

	return OK;
}

/*
 * Reregister all previously registered subtrees.  This routine should be
 * called after the main program has determined that the MIB service has been
 * restarted.
 */
void
rmib_reregister(void)
{
	unsigned int id;

	for (id = 0; id < __arraycount(rnodes); id++)
		if (rnodes[id].rno_node != NULL)
			rmib_send_reg(id);
}

/*
 * Reset all registrations, without involving MIB communication.  This routine
 * exists for testing purposes only, and may disappear in the future.
 */
void
rmib_reset(void)
{

	memset(rnodes, 0, sizeof(rnodes));
}

/*
 * Process a request from the MIB service for information about the root node
 * of a subtree, specifically its name and description.
 */
static int
rmib_info(const message * m_in)
{
	struct rmib_node *rnode;
	unsigned int id;
	const char *ptr;
	size_t size;
	int r;

	id = m_in->m_mib_lsys_info.root_id;
	if (id >= __arraycount(rnodes) || rnodes[id].rno_node == NULL)
		return ENOENT;
	rnode = rnodes[id].rno_node;

	/* The name must fit.  If it does not, the service writer messed up. */
	size = strlen(rnode->rnode_name) + 1;
	if (size > m_in->m_mib_lsys_info.name_size)
		return ENAMETOOLONG;

	r = sys_safecopyto(m_in->m_source, m_in->m_mib_lsys_info.name_grant, 0,
	    (vir_bytes)rnode->rnode_name, size);
	if (r != OK)
		return r;

	/* If there is no (optional) description, copy out an empty string. */
	ptr = (rnode->rnode_desc != NULL) ? rnode->rnode_desc : "";
	size = strlen(ptr) + 1;

	if (size > m_in->m_mib_lsys_info.desc_size)
		size = m_in->m_mib_lsys_info.desc_size;

	return sys_safecopyto(m_in->m_source, m_in->m_mib_lsys_info.desc_grant,
	    0, (vir_bytes)ptr, size);
}

/*
 * Process a request from the MIB service.  The given message should originate
 * from the MIB service and have one of the COMMON_MIB_ requests as type.
 */
void
rmib_process(const message * m_in, int ipc_status)
{
	message m_out;
	uint32_t req_id;
	ssize_t r;

	/* Only the MIB service may issue these requests. */
	if (m_in->m_source != MIB_PROC_NR)
		return;

	/* Process the actual request. */
	switch (m_in->m_type) {
	case COMMON_MIB_INFO:
		req_id = m_in->m_mib_lsys_info.req_id;

		r = rmib_info(m_in);

		break;

	case COMMON_MIB_CALL:
		req_id = m_in->m_mib_lsys_call.req_id;

		r = rmib_call(m_in);

		break;

	default:
		/*
		 * HACK: assume that for all current and future requests, the
		 * request ID field is in the same place.  We could create a
		 * m_mib_lsys_unknown pseudo message type for this, but, eh.
		 */
		req_id = m_in->m_mib_lsys_info.req_id;

		r = ENOSYS;
	}

	/* Construct and send a reply message to the MIB service. */
	memset(&m_out, 0, sizeof(m_out));

	m_out.m_type = COMMON_MIB_REPLY;
	m_out.m_lsys_mib_reply.req_id = req_id;
	m_out.m_lsys_mib_reply.status = r;

	if (IPC_STATUS_CALL(ipc_status) == SENDREC)
		r = ipc_sendnb(m_in->m_source, &m_out);
	else
		r = asynsend3(m_in->m_source, &m_out, AMF_NOREPLY);

	if (r != OK)
		printf("lsys:rmib: unable to send reply to %d: %zd\n",
		    m_in->m_source, r);
}
