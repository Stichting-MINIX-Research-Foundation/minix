/* MIB service - tree.c - tree access and management */

#include "mib.h"

/*
 * Does the given identifier fall within the range of static identifiers in the
 * given parent?  This check can be used to enumerate all static array entries
 * in the given parent, starting from zero.  The check does not guarantee that
 * the entry is actually for a valid node, nor does it guarantee that there is
 * not a dynamic node with this identifier.
 */
#define IS_STATIC_ID(parent, id) ((unsigned int)(id) < (parent)->node_size)

/*
 * Scratch buffer, used for various cases of temporary data storage.  It must
 * be large enough to fit a sysctldesc structure followed by the longest
 * supported description.  It must also be large enough to serve as temporary
 * storage for data being written in the majority of cases.  Finally, it must
 * be large enough to contain an entire page, for mib_copyin_str().
 */
#define MAXDESCLEN	1024	/* from NetBSD */
#define SCRATCH_SIZE	MAX(PAGE_SIZE, sizeof(struct sysctldesc) + MAXDESCLEN)
static char scratch[SCRATCH_SIZE] __aligned(sizeof(int32_t));

unsigned int mib_nodes;		/* how many nodes are there in the tree? */
unsigned int mib_objects;	/* how many memory objects are allocated? */
unsigned int mib_remotes;	/* how many remote subtrees are there? */

/*
 * Find a node through its parent node and identifier.  Return the node if it
 * was found, and optionally store a pointer to the pointer to its dynode
 * superstructure (for removal).  If no matching node was found, return NULL.
 */
static struct mib_node *
mib_find(struct mib_node * parent, int id, struct mib_dynode *** prevpp)
{
	struct mib_node *node;
	struct mib_dynode **dynp;

	if (id < 0)
		return NULL;

	/*
	 * Is there a static node with this identifier?  The static nodes are
	 * all in a single array, so lookup is O(1) for these nodes.  We use
	 * the node flags field to see whether the array entry is valid.
	 */
	if (IS_STATIC_ID(parent, id)) {
		node = &parent->node_scptr[id];

		if (node->node_flags != 0) {
			/* Found a matching static node. */
			if (prevpp != NULL)
				*prevpp = NULL;
			return node;
		}
	}

	/*
	 * Is there a dynamic node with this identifier?  The dynamic nodes
	 * form a linked list.  This is predominantly because userland may pick
	 * the identifier number at creation time, so we cannot rely on all
	 * dynamically created nodes falling into a small identifier range.
	 * That eliminates the option of a dynamic array indexed by identifier,
	 * and a linked list is the simplest next option.  Thus, dynamic node
	 * lookup is O(n).  However, since the list is sorted by identifier,
	 * we may be able to stop the search early.
	 */
	for (dynp = &parent->node_dcptr; *dynp != NULL;
	    dynp = &((*dynp)->dynode_next)) {
		if ((*dynp)->dynode_id == id) {
			/* Found a matching dynamic node. */
			if (prevpp != NULL)
				*prevpp = dynp;
			return &(*dynp)->dynode_node;
		} else if ((*dynp)->dynode_id > id)
			break; /* no need to look further */
	}

	return NULL;
}

/*
 * Copy out a node to userland, using the exchange format for nodes (namely,
 * a sysctlnode structure).  Return the size of the object that is (or, if the
 * node falls outside the requested data range, would be) copied out on
 * success, or a negative error code on failure.  The function may return 0
 * to indicate that nothing was copied out after all (this is unused here).
 */
static ssize_t
mib_copyout_node(struct mib_call * call, struct mib_oldp * oldp, size_t off,
	int id, const struct mib_node * node)
{
	struct sysctlnode scn;
	int visible;

	if (!mib_inrange(oldp, off))
		return sizeof(scn); /* nothing to do */

	memset(&scn, 0, sizeof(scn));

	/*
	 * We use CTLFLAG_PARENT, CTLFLAG_VERIFY, and CTLFLAG_REMOTE internally
	 * only.  NetBSD uses the values of these flags for different purposes.
	 * Either way, do not expose them to userland.
	 */
	scn.sysctl_flags = SYSCTL_VERSION | (node->node_flags &
	    ~(CTLFLAG_PARENT | CTLFLAG_VERIFY | CTLFLAG_REMOTE));
	scn.sysctl_num = id;
	strlcpy(scn.sysctl_name, node->node_name, sizeof(scn.sysctl_name));
	scn.sysctl_ver = node->node_ver;
	scn.sysctl_size = node->node_size;

	/* Some information is only visible if the user can access the node. */
	visible = (!(node->node_flags & CTLFLAG_PRIVATE) || mib_authed(call));

	/*
	 * For immediate types, store the immediate value in the resulting
	 * structure, unless the caller is not authorized to obtain the value.
	 */
	if ((node->node_flags & CTLFLAG_IMMEDIATE) && visible) {
		switch (SYSCTL_TYPE(node->node_flags)) {
		case CTLTYPE_BOOL:
			scn.sysctl_bdata = node->node_bool;
			break;
		case CTLTYPE_INT:
			scn.sysctl_idata = node->node_int;
			break;
		case CTLTYPE_QUAD:
			scn.sysctl_qdata = node->node_quad;
		}
	}

	/* Special rules apply to parent nodes. */
	if (SYSCTL_TYPE(node->node_flags) == CTLTYPE_NODE) {
		/* Report the node size the way NetBSD does, just in case. */
		scn.sysctl_size = sizeof(scn);

		/*
		 * If this is a remote node, use the values we have of the root
		 * of the remote subtree.  If we did not have these values, we
		 * would have to call into the remote service here, which for
		 * reliability purposes is a bad idea.
		 *
		 * If this is a real parent node, report child information.  In
		 * both these cases, expose child information only if the node
		 * itself is accessible by the caller.
		 *
		 * If this is a function-driven node, indicate this by setting
		 * a nonzero function address.  This allows trace(1) to
		 * determine that it should not attempt to descend into this
		 * part of the tree as usual, because a) accessing subnodes may
		 * have side effects, and b) meta-identifiers may not work as
		 * expected in these parts of the tree.  Do not return the real
		 * function pointer, as this would leak anti-ASR information.
		 */
		if (node->node_flags & CTLFLAG_REMOTE) {
			if (visible) {
				scn.sysctl_csize = node->node_rcsize;
				scn.sysctl_clen = node->node_rclen;
			}
		} else if (node->node_flags & CTLFLAG_PARENT) {
			if (visible) {
				scn.sysctl_csize = node->node_csize;
				scn.sysctl_clen = node->node_clen;
			}
		} else
			scn.sysctl_func = SYSCTL_NODE_FN;
	}

	/* Copy out the resulting node. */
	return mib_copyout(oldp, off, &scn, sizeof(scn));
}

/*
 * Given a query on a non-leaf (parent) node, provide the user with an array of
 * this node's children.
 */
static ssize_t
mib_query(struct mib_call * call, struct mib_node * parent,
	struct mib_oldp * oldp, struct mib_newp * newp)
{
	struct sysctlnode scn;
	struct mib_node *node;
	struct mib_dynode *dynode;
	size_t off;
	int r, id;

	/* If the user passed in version numbers, check them. */
	if (newp != NULL) {
		if ((r = mib_copyin(newp, &scn, sizeof(scn))) != OK)
			return r;

		if (SYSCTL_VERS(scn.sysctl_flags) != SYSCTL_VERSION)
			return EINVAL;

		/*
		 * If a node version number is given, it must match the version
		 * of the parent or the root.
		 */
		if (scn.sysctl_ver != 0 &&
		    scn.sysctl_ver != mib_root.node_ver &&
		    scn.sysctl_ver != parent->node_ver)
			return EINVAL;
	}

	/*
	 * We need not return the nodes strictly in ascending order of
	 * identifiers, as this is not expected by userland.  For example,
	 * sysctlgetmibinfo(3) performs its own sorting after a query.
	 * Thus, we can go through the static and dynamic nodes separately.
	 */
	off = 0;

	/* First enumerate the static nodes. */
	for (id = 0; IS_STATIC_ID(parent, id); id++) {
		node = &parent->node_scptr[id];

		if (node->node_flags == 0)
			continue;

		if ((r = mib_copyout_node(call, oldp, off, id, node)) < 0)
			return r;
		off += r;
	}

	/* Then enumerate the dynamic nodes. */
	for (dynode = parent->node_dcptr; dynode != NULL;
	    dynode = dynode->dynode_next) {
		node = &dynode->dynode_node;

		if ((r = mib_copyout_node(call, oldp, off, dynode->dynode_id,
		    node)) < 0)
			return r;
		off += r;
	}

	return off;
}

/*
 * Check whether the given name buffer contains a valid node name string.  If
 * the name is nonempty, properly terminated, and contains only acceptable
 * characters, return the length of the string excluding null terminator.
 * Otherwise, return zero to indicate failure.
 */
static size_t
mib_check_name(const char * name, size_t namesize)
{
	size_t namelen;
	char c;

	/* Names must be nonempty, null terminated, C symbol style strings. */
	for (namelen = 0; namelen < namesize; namelen++) {
		if ((c = name[namelen]) == '\0')
			break;
		/* A-Z, a-z, 0-9, _ only, and no digit as first character. */
		if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
		    c == '_' || (c >= '0' && c <= '9' && namelen > 0)))
			return 0;
	}
	if (namelen == 0 || namelen == namesize)
		return 0;

	return namelen;
}

/*
 * Scan a parent node's children, as part of new node creation.  Search for
 * either a free node identifier (if given_id < 0) or collisions with the node
 * identifier to use (if given_id >= 0).  Also check for name collisions.  Upon
 * success, return OK, with the resulting node identifier stored in 'idp' and a
 * pointer to the pointer for the new dynamic node stored in 'prevpp'.  Upon
 * failure, return an error code.  If the failure is EEXIST, 'idp' will contain
 * the ID of the conflicting node, and 'nodep' will point to the node.
 */
static int
mib_scan(struct mib_node * parent, int given_id, const char * name, int * idp,
	struct mib_dynode *** prevpp, struct mib_node ** nodep)
{
	struct mib_dynode **prevp, **dynp;
	struct mib_node *node;
	int id;

	/*
	 * We must verify that no entry already exists with the given name.  In
	 * addition, if a nonnegative identifier is given, we should use that
	 * identifier and make sure it does not already exist.  Otherwise, we
	 * must find a free identifier.  Finally, we sort the dynamic nodes in
	 * ascending identifier order, so we must find the right place at which
	 * to insert the new node.
	 *
	 * For finding a free identifier, choose an ID that falls (well)
	 * outside the static range, both to avoid accidental retrieval by an
	 * application that uses a static ID, and to simplify verifying that
	 * the ID is indeed free.  The sorting of dynamic nodes by identifier
	 * ensures that searching for a free identifier is O(n).
	 *
	 * At this time, we do not support some NetBSD features.  We do not
	 * force success if the new node is sufficiently like an existing one.
	 * Also, we do not use global autoincrement for dynamic identifiers,
	 * although that could easily be changed.
	 */

	/* First check the static node array, just for collisions. */
	for (id = 0; IS_STATIC_ID(parent, id); id++) {
		node = &parent->node_scptr[id];
		if (node->node_flags == 0)
			continue;
		if (id == given_id || !strcmp(name, node->node_name)) {
			*idp = id;
			*nodep = node;
			return EEXIST;
		}
	}

	/*
	 * Then try to find the place to insert a new dynamic node.  At the
	 * same time, check for both identifier and name collisions.
	 */
	if (given_id >= 0)
		id = given_id;
	else
		id = MAX(CREATE_BASE, parent->node_size);

	for (prevp = &parent->node_dcptr; *prevp != NULL;
	    prevp = &((*prevp)->dynode_next)) {
		if ((*prevp)->dynode_id > id)
			break;
		if ((*prevp)->dynode_id == id) {
			if (given_id >= 0) {
				*idp = id;
				*nodep = &(*prevp)->dynode_node;
				return EEXIST;
			} else
				id++;
		}
		if (!strcmp(name, (*prevp)->dynode_node.node_name)) {
			*idp = (*prevp)->dynode_id;
			*nodep = &(*prevp)->dynode_node;
			return EEXIST;
		}
	}

	/* Finally, check the rest of the dynamic nodes for name collisions. */
	for (dynp = prevp; *dynp != NULL; dynp = &((*dynp)->dynode_next)) {
		assert((*dynp)->dynode_id > id);

		if (!strcmp(name, (*dynp)->dynode_node.node_name)) {
			*idp = (*dynp)->dynode_id;
			*nodep = &(*dynp)->dynode_node;
			return EEXIST;
		}
	}

	*idp = id;
	*prevpp = prevp;
	return OK;
}

/*
 * Copy in a string from the user process, located at the given remote address,
 * into the given local buffer.  If no buffer is given, just compute the length
 * of the string.  On success, return OK.  If 'sizep' is not NULL, it will be
 * filled with the string size, including the null terminator.  If a non-NULL
 * buffer was given, the string will be copied into the provided buffer (also
 * including null terminator).  Return an error code on failure, which includes
 * the case that no null terminator was found within the range of bytes that
 * would fit in the given buffer.
 */
static int
mib_copyin_str(struct mib_newp * __restrict newp, vir_bytes addr,
	char * __restrict buf, size_t bufsize, size_t * __restrict sizep)
{
	char *ptr, *endp;
	size_t chunk, len;
	int r;

	assert(newp != NULL);
	assert(bufsize <= SSIZE_MAX);

	if (addr == 0)
		return EINVAL;

	/*
	 * NetBSD has a kernel routine for copying in a string from userland.
	 * MINIX3 does not, since its system call interface has always relied
	 * on userland passing in string lengths.  The sysctl(2) API does not
	 * provide the string length, and thus, we have to do a bit of guess
	 * work.  If we copy too little at once, performance suffers.  If we
	 * copy too much at once, we may trigger an unneeded page fault.  Make
	 * use of page boundaries to strike a balance between those two.  If we
	 * are requested to just get the string length, use the scratch buffer.
	 */
	len = 0;

	while (bufsize > 0) {
		chunk = PAGE_SIZE - (addr % PAGE_SIZE);
		if (chunk > bufsize)
			chunk = bufsize;

		ptr = (buf != NULL) ? &buf[len] : scratch;
		if ((r = mib_copyin_aux(newp, addr, ptr, chunk)) != OK)
			return r;

		if ((endp = memchr(ptr, '\0', chunk)) != NULL) {
			/* A null terminator was found - success. */
			if (sizep != NULL)
				*sizep = len + (size_t)(endp - ptr) + 1;
			return OK;
		}

		addr += chunk;
		len += chunk;
		bufsize -= chunk;
	}

	/* No null terminator found. */
	return EINVAL;
}

/*
 * Increase the version of the root node, and copy this new version to all
 * nodes on the path to the given node, including that node itself.
 */
static void
mib_upgrade(struct mib_node * node)
{
	uint32_t ver;

	assert(node != NULL);

	/*
	 * The root node determines the version of the entire tree.  Do not use
	 * version number 0, as a zero version number indicates no interest in
	 * versions elsewhere.
	 */

	ver = mib_root.node_ver + 1;
	if (ver == 0)
		ver = 1;

	/* Copy the new version to all the nodes on the path. */
	do {
		node->node_ver = ver;

		node = node->node_parent;
	} while (node != NULL);
}

/*
 * Add a new dynamically allocated node into the tree, inserting it into the
 * linked-list position of the parent tree as given by 'prevp'.  Also update
 * versions and counters accordingly.  This function never fails.
 */
static void
mib_add(struct mib_dynode * dynode, struct mib_dynode ** prevp)
{
	struct mib_node *parent;

	parent = dynode->dynode_node.node_parent;
	assert(parent != NULL);

	/* Link the dynamic node into the list, in the right place. */
	assert(prevp != NULL);
	dynode->dynode_next = *prevp;
	*prevp = dynode;

	/* The parent node now has one more child. */
	parent->node_csize++;
	parent->node_clen++;

	/* There is now one more node in the tree. */
	mib_nodes++;

	/*
	 * Bump the version of all nodes on the path to the new node, including
	 * the node itself.
	 */
	mib_upgrade(&dynode->dynode_node);
}

/*
 * Create a node.
 */
static ssize_t
mib_create(struct mib_call * call, struct mib_node * parent,
	struct mib_oldp * oldp, struct mib_newp * newp)
{
	struct mib_dynode *dynode, **prevp;
	struct mib_node *node;
	struct sysctlnode scn;
	size_t namelen, size;
	ssize_t len;
	bool b;
	char c;
	int r, id;

	/* This is a privileged operation. */
	if (!mib_authed(call))
		return EPERM;

	/*
	 * The parent must not be a remote node, but this is already implied by
	 * the fact that we got here at all.
	 */
	assert(SYSCTL_TYPE(parent->node_flags) == CTLTYPE_NODE);
	assert(!(parent->node_flags & CTLFLAG_REMOTE));

	/* The parent node must not be marked as read-only. */
	if (!(parent->node_flags & CTLFLAG_READWRITE))
		return EPERM;

	/*
	 * Has the parent reached its child node limit?  This check is entirely
	 * theoretical as long as we support only 32-bit virtual memory.
	 */
	if (parent->node_csize == INT_MAX)
		return EINVAL;
	assert(parent->node_clen <= parent->node_csize);

	/* The caller must supply information on the child node to create. */
	if (newp == NULL)
		return EINVAL;

	if ((r = mib_copyin(newp, &scn, sizeof(scn))) != OK)
		return r;

	/*
	 * We perform as many checks as possible before we start allocating
	 * memory.  Then again, after allocation, copying in data may still
	 * fail.  Unlike when setting values, we do not first copy data into a
	 * temporary buffer here, because we do not need to: if the copy fails,
	 * the entire create operation fails, so atomicity is not an issue.
	 */
	if (SYSCTL_VERS(scn.sysctl_flags) != SYSCTL_VERSION)
		return EINVAL;

	/*
	 * If a node version number is given, it must match the version of
	 * either the parent or the root node.  The given version number is
	 * *not* used for the node being created.
	 */
	if (scn.sysctl_ver != 0 && scn.sysctl_ver != mib_root.node_ver &&
	    scn.sysctl_ver != parent->node_ver)
		return EINVAL;

	/*
	 * Validate the node flags.  In addition to the NetBSD-allowed flags,
	 * we also allow UNSIGNED, and leave its interpretation to userland.
	 */
	if (SYSCTL_FLAGS(scn.sysctl_flags) &
	    ~(SYSCTL_USERFLAGS | CTLFLAG_UNSIGNED))
		return EINVAL;

	if (!(scn.sysctl_flags & CTLFLAG_IMMEDIATE)) {
		/*
		 * Without either IMMEDIATE or OWNDATA, data pointers are
		 * actually kernel addresses--a concept we do not support.
		 * Otherwise, if IMMEDIATE is not set, we are going to have to
		 * allocate extra memory for the data, so force OWNDATA to be.
		 * set.  Node-type nodes have no data, though.
		 */
		if (SYSCTL_TYPE(scn.sysctl_flags) != CTLTYPE_NODE) {
			if (!(scn.sysctl_flags & CTLFLAG_OWNDATA) &&
			    scn.sysctl_data != NULL)
				return EINVAL; /* not meaningful on MINIX3 */

			scn.sysctl_flags |= CTLFLAG_OWNDATA;
		}
	} else if (scn.sysctl_flags & CTLFLAG_OWNDATA)
		return EINVAL;

	/* The READWRITE flag consists of multiple bits.  Sanitize. */
	if (scn.sysctl_flags & CTLFLAG_READWRITE)
		scn.sysctl_flags |= CTLFLAG_READWRITE;

	/* Validate the node type and size, and do some additional checks. */
	switch (SYSCTL_TYPE(scn.sysctl_flags)) {
	case CTLTYPE_BOOL:
		if (scn.sysctl_size != sizeof(bool))
			return EINVAL;
		break;
	case CTLTYPE_INT:
		if (scn.sysctl_size != sizeof(int))
			return EINVAL;
		break;
	case CTLTYPE_QUAD:
		if (scn.sysctl_size != sizeof(u_quad_t))
			return EINVAL;
		break;
	case CTLTYPE_STRING:
		/*
		 * For strings, a zero length means that we are supposed to
		 * allocate a buffer size based on the given string size.
		 */
		if (scn.sysctl_size == 0 && scn.sysctl_data != NULL) {
			if ((r = mib_copyin_str(newp,
			    (vir_bytes)scn.sysctl_data, NULL, SSIZE_MAX,
			    &size)) != OK)
				return r;
			scn.sysctl_size = size;
		}
		/* FALLTHROUGH */
	case CTLTYPE_STRUCT:
		/*
		 * We do not set an upper size on the data size, since it would
		 * still be possible to create a large number of nodes, and
		 * this is a privileged operation ayway.
		 */
		if (scn.sysctl_size == 0 || scn.sysctl_size > SSIZE_MAX)
			return EINVAL;
		if (scn.sysctl_flags & CTLFLAG_IMMEDIATE)
			return EINVAL;
		break;
	case CTLTYPE_NODE:
		/*
		 * The zero size is an API requirement, but we also rely on the
		 * zero value internally, as the node has no static children.
		 */
		if (scn.sysctl_size != 0)
			return EINVAL;
		if (scn.sysctl_flags & (CTLFLAG_IMMEDIATE | CTLFLAG_OWNDATA))
			return EINVAL;
		if (scn.sysctl_csize != 0 || scn.sysctl_clen != 0 ||
		    scn.sysctl_child != NULL)
			return EINVAL;
		break;
	default:
		return EINVAL;
	}

	if (scn.sysctl_func != NULL || scn.sysctl_parent != NULL)
		return EINVAL;

	/* Verify that the given name is valid, and get its string length. */
	namelen = mib_check_name(scn.sysctl_name, sizeof(scn.sysctl_name));

	if (namelen == 0)
		return EINVAL;

	/*
	 * Find a free identifier, or check for ID collisions if a specific
	 * identifier was given.  At the same time, scan for name collisions,
	 * and find the location at which to insert the new node in the list.
	 */
	r = mib_scan(parent, scn.sysctl_num, scn.sysctl_name, &id, &prevp,
	    &node);

	if (r != OK) {
		/*
		 * On collisions, if requested, copy out the existing node.
		 * This does not quite fit the general interaction model, as
		 * the service must now return a nonzero old length from a call
		 * that actually failed (in contrast to ENOMEM failures).
		 */
		if (r == EEXIST && oldp != NULL) {
			len = mib_copyout_node(call, oldp, 0, id, node);

			if (len > 0)
				mib_setoldlen(call, len);
		}

		return r;
	}

	/*
	 * All checks so far have passed.  "id" now contains the new node
	 * identifier, and "prevp" points to the pointer at which to insert the
	 * new node in its parent's linked list of dynamic nodes.
	 *
	 * We can now attempt to create and initialize a new dynamic node.
	 * Allocating nodes this way may cause heavy memory fragmentation over
	 * time, but we do not expect the tree to see heavy modification at run
	 * time, and the superuser has easier ways to get the MIB service in
	 * trouble.  We note that even in low-memory conditions, the MIB
	 * service is always able to provide basic functionality.
	 */
	size = sizeof(*dynode) + namelen;
	if (!(scn.sysctl_flags & CTLFLAG_IMMEDIATE))
		size += scn.sysctl_size;

	if ((dynode = malloc(size)) == NULL)
		return EINVAL; /* do not return ENOMEM */
	mib_objects++;

	/* From here on, we have to free "dynode" before returning an error. */
	r = OK;

	memset(dynode, 0, sizeof(*dynode)); /* no need to zero all of "size" */
	dynode->dynode_id = id;
	strlcpy(dynode->dynode_name, scn.sysctl_name, namelen + 1);

	node = &dynode->dynode_node;
	node->node_flags = scn.sysctl_flags & ~SYSCTL_VERS_MASK;
	if (SYSCTL_TYPE(scn.sysctl_flags) == CTLTYPE_NODE)
		node->node_flags |= CTLFLAG_PARENT;
	node->node_size = scn.sysctl_size;
	node->node_parent = parent;
	node->node_name = dynode->dynode_name;

	/* Initialize the node value. */
	if (scn.sysctl_flags & CTLFLAG_IMMEDIATE) {
		switch (SYSCTL_TYPE(scn.sysctl_flags)) {
		case CTLTYPE_BOOL:
			/* Sanitize booleans.  See the C99 _Bool comment. */
			memcpy(&c, &scn.sysctl_bdata, sizeof(c));
			node->node_bool = (bool)c;
			break;
		case CTLTYPE_INT:
			node->node_int = scn.sysctl_idata;
			break;
		case CTLTYPE_QUAD:
			node->node_quad = scn.sysctl_qdata;
			break;
		default:
			assert(0);
		}
	} else if (SYSCTL_TYPE(scn.sysctl_flags) != CTLTYPE_NODE) {
		node->node_data = dynode->dynode_name + namelen + 1;

		/* Did the user supply initial data?  If not, use zeroes. */
		if (scn.sysctl_data != NULL) {
			/*
			 * For strings, do not copy in more than needed.  This
			 * is just a nice feature which allows initialization
			 * of large string buffers with short strings.
			 */
			if (SYSCTL_TYPE(scn.sysctl_flags) == CTLTYPE_STRING)
				r = mib_copyin_str(newp,
				    (vir_bytes)scn.sysctl_data,
				    node->node_data, scn.sysctl_size, NULL);
			else
				r = mib_copyin_aux(newp,
				    (vir_bytes)scn.sysctl_data,
				    node->node_data, scn.sysctl_size);
		} else
			memset(node->node_data, 0, scn.sysctl_size);

		/*
		 * Sanitize booleans.  See the C99 _Bool comment elsewhere.
		 * In this case it is not as big of a deal, as we will not be
		 * accessing the boolean value directly ourselves.
		 */
		if (r == OK && SYSCTL_TYPE(scn.sysctl_flags) == CTLTYPE_BOOL) {
			b = (bool)*(char *)node->node_data;
			memcpy(node->node_data, &b, sizeof(b));
		}
	}

	/*
	 * Even though it would be entirely possible to set a description right
	 * away as well, this does not seem to be supported on NetBSD at all.
	 */

	/* Deal with earlier failures now. */
	if (r != OK) {
		free(dynode);
		mib_objects--;

		return r;
	}

	/*
	 * At this point, actual creation can no longer fail.  Add the node
	 * into the tree, and update versions and counters.
	 */
	mib_add(dynode, prevp);

	/*
	 * Copy out the newly created node as resulting ("old") data.  Do not
	 * undo the creation if this fails, though.
	 */
	return mib_copyout_node(call, oldp, 0, id, node);
}

/*
 * Remove the given node from the tree.  If 'prevp' is NULL, the node is a
 * static node which should be zeroed out.  If 'prevp' is not NULL, the node is
 * a dynamic node which should be freed; 'prevp' will then point to the pointer
 * to its dynode container.  Also update versions and counters as appropriate.
 * This function never fails.
 */
static void
mib_remove(struct mib_node * node, struct mib_dynode ** prevp)
{
	struct mib_dynode *dynode;
	struct mib_node *parent;

	parent = node->node_parent;
	assert(parent != NULL);

	/* If the description was allocated, free it. */
	if (node->node_flags & CTLFLAG_OWNDESC) {
		free(__UNCONST(node->node_desc));
		mib_objects--;
	}

	/*
	 * Static nodes only use static memory, and dynamic nodes have the data
	 * area embedded in the dynode object.  In neither case is data memory
	 * allocated separately, and thus, it need never be freed separately.
	 * Therefore we *must not* check CTLFLAG_OWNDATA here.
	 */

	assert(parent->node_csize > 0);
	assert(parent->node_clen > 0);

	/*
	 * Dynamic nodes must be freed.  Freeing the dynode object also frees
	 * the node name and any associated data.  Static nodes are zeroed out,
	 * and the static memory they referenced will become inaccessible.
	 */
	if (prevp != NULL) {
		dynode = *prevp;
		*prevp = dynode->dynode_next;

		assert(node == &dynode->dynode_node);

		free(dynode);
		mib_objects--;

		parent->node_csize--;
	} else
		memset(node, 0, sizeof(*node));

	parent->node_clen--;

	mib_nodes--;

	/* Bump the version of all nodes on the path to the destroyed node. */
	mib_upgrade(parent);
}

/*
 * Destroy a node.
 */
static ssize_t
mib_destroy(struct mib_call * call, struct mib_node * parent,
	struct mib_oldp * oldp, struct mib_newp * newp)
{
	struct mib_dynode **prevp;
	struct mib_node *node;
	struct sysctlnode scn;
	ssize_t r;

	/* This is a privileged operation. */
	if (!mib_authed(call))
		return EPERM;

	/* The parent node must not be marked as read-only. */
	if (!(parent->node_flags & CTLFLAG_READWRITE))
		return EPERM;

	/* The caller must specify which child node to destroy. */
	if (newp == NULL)
		return EINVAL;

	if ((r = mib_copyin(newp, &scn, sizeof(scn))) != OK)
		return r;

	if (SYSCTL_VERS(scn.sysctl_flags) != SYSCTL_VERSION)
		return EINVAL;

	/* Locate the child node. */
	if ((node = mib_find(parent, scn.sysctl_num, &prevp)) == NULL)
		return ENOENT;

	/* The node must not be marked as permanent. */
	if (node->node_flags & CTLFLAG_PERMANENT)
		return EPERM;

	/* For node-type nodes, extra rules apply. */
	if (SYSCTL_TYPE(node->node_flags) == CTLTYPE_NODE) {
		/* The node must not be a mount point. */
		if (node->node_flags & CTLFLAG_REMOTE)
			return EBUSY;

		/* The node must not have an associated function. */
		if (!(node->node_flags & CTLFLAG_PARENT))
			return EPERM;

		/* The target node must itself not have child nodes. */
		if (node->node_clen != 0)
			return ENOTEMPTY;
	}

	/* If the user supplied a version, it must match the node version. */
	if (scn.sysctl_ver != 0 && scn.sysctl_ver != node->node_ver)
		return EINVAL;	/* NetBSD inconsistently throws ENOENT here */

	/* If the user supplied a name, it must match the node name. */
	if (scn.sysctl_name[0] != '\0') {
		if (memchr(scn.sysctl_name, '\0',
		    sizeof(scn.sysctl_name)) == NULL)
			return EINVAL;
		if (strcmp(scn.sysctl_name, node->node_name))
			return EINVAL;	/* also ENOENT on NetBSD */
	}

	/*
	 * Copy out the old node if requested, otherwise return the length
	 * anyway.  The node will be destroyed even if this call fails,
	 * because that is how NetBSD behaves.
	 */
	r = mib_copyout_node(call, oldp, 0, scn.sysctl_num, node);

	/*
	 * Remove the node from the tree.  The procedure depends on whether the
	 * node is static (prevp == NULL) or dynamic (prevp != NULL).  Also
	 * update versions and counters.
	 */
	mib_remove(node, prevp);

	return r;
}

/*
 * Copy out a node description to userland, using the exchange format for node
 * descriptions (namely, a sysctldesc structure).  Return the size of the
 * object that is (or, if the description falls outside the requested data
 * range, would be) copied out on success, or a negative error code on failure.
 * The function may return 0 to indicate that nothing was copied out after all.
 */
static ssize_t
mib_copyout_desc(struct mib_call * call, struct mib_oldp * oldp, size_t off,
	int id, const struct mib_node * node)
{
	struct sysctldesc *scd;
	size_t size;
	int r;

	/* Descriptions of private nodes are considered private too. */
	if ((node->node_flags & CTLFLAG_PRIVATE) && !mib_authed(call))
		return 0;

	/* The description length includes the null terminator. */
	if (node->node_desc != NULL)
		size = strlen(node->node_desc) + 1;
	else
		size = 1;

	assert(sizeof(*scd) + size <= sizeof(scratch));

	scd = (struct sysctldesc *)scratch;
	memset(scd, 0, sizeof(*scd));
	scd->descr_num = id;
	scd->descr_ver = node->node_ver;
	scd->descr_len = size;
	if (node->node_desc != NULL)
		strlcpy(scd->descr_str, node->node_desc,
		    sizeof(scratch) - sizeof(*scd));
	else
		scd->descr_str[0] = '\0';

	size += offsetof(struct sysctldesc, descr_str);

	if ((r = mib_copyout(oldp, off, scratch, size)) < 0)
		return r;

	/*
	 * By aligning just the size, we may leave garbage between the entries
	 * copied out, which is fine because it is userland's own data.
	 */
	return roundup2(size, sizeof(int32_t));
}

/*
 * Retrieve node descriptions in bulk, or retrieve or assign a particular
 * node's description.
 */
static ssize_t
mib_describe(struct mib_call * call, struct mib_node * parent,
	struct mib_oldp * oldp, struct mib_newp * newp)
{
	struct sysctlnode scn;
	struct mib_node *node;
	struct mib_dynode *dynode;
	size_t off;
	int r, id;

	/* If new data are given, they identify a particular target node. */
	if (newp != NULL) {
		if ((r = mib_copyin(newp, &scn, sizeof(scn))) != OK)
			return r;

		if (SYSCTL_VERS(scn.sysctl_flags) != SYSCTL_VERSION)
			return EINVAL;

		/* Locate the child node. */
		if ((node = mib_find(parent, scn.sysctl_num, NULL)) == NULL)
			return ENOENT;

		/* Descriptions of private nodes are considered private too. */
		if ((node->node_flags & CTLFLAG_PRIVATE) && !mib_authed(call))
			return EPERM;

		/*
		 * If a description pointer was given, this is a request to
		 * set the node's description.
		 */
		if (scn.sysctl_desc != NULL) {
			/* Such a request requires superuser privileges. */
			if (!mib_authed(call))
				return EPERM;

			/*
			 * The node must not be a mount point.  Arguably this
			 * check is not necessary, since we use the description
			 * of the preexisting underlying node anyway.
			 */
			if (SYSCTL_TYPE(node->node_flags) == CTLTYPE_NODE &&
			    (node->node_flags & CTLFLAG_REMOTE))
				return EBUSY;

			/* The node must not already have a description. */
			if (node->node_desc != NULL)
				return EPERM;

			/* The node must not be marked as permanent. */
			if (node->node_flags & CTLFLAG_PERMANENT)
				return EPERM;

			/*
			 * If the user supplied a version, it must match.
			 * NetBSD performs this check only when setting
			 * descriptions, and thus, so do we..
			 */
			if (scn.sysctl_ver != 0 &&
			    scn.sysctl_ver != node->node_ver)
				return EINVAL;

			/*
			 * Copy in the description to the scratch buffer.
			 * The length of the description must be reasonable.
			 */
			if ((r = mib_copyin_str(newp,
			    (vir_bytes)scn.sysctl_desc, scratch, MAXDESCLEN,
			    NULL)) != OK)
				return r;

			/* Allocate memory and store the description. */
			if ((node->node_desc = strdup(scratch)) == NULL) {
				printf("MIB: out of memory!\n");

				return EINVAL; /* do not return ENOMEM */
			}
			mib_objects++;

			/* The description must now be freed with the node. */
			node->node_flags |= CTLFLAG_OWNDESC;
		}

		/*
		 * Either way, copy out the requested node's description, which
		 * should indeed be the new description if one was just set.
		 * Note that we have already performed the permission check
		 * that could make this call return zero, so here it will not.
		 */
		return mib_copyout_desc(call, oldp, 0, scn.sysctl_num, node);
	}

	/* See also the considerations laid out in mib_query(). */
	off = 0;

	/* First describe the static nodes. */
	for (id = 0; IS_STATIC_ID(parent, id); id++) {
		node = &parent->node_scptr[id];

		if (node->node_flags == 0)
			continue;

		if ((r = mib_copyout_desc(call, oldp, off, id, node)) < 0)
			return r;
		off += r;
	}

	/* Then describe the dynamic nodes. */
	for (dynode = parent->node_dcptr; dynode != NULL;
	    dynode = dynode->dynode_next) {
		node = &dynode->dynode_node;

		if ((r = mib_copyout_desc(call, oldp, off, dynode->dynode_id,
		    node)) < 0)
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
mib_getptr(struct mib_node * node)
{

	switch (SYSCTL_TYPE(node->node_flags)) {
	case CTLTYPE_BOOL:
		if (node->node_flags & CTLFLAG_IMMEDIATE)
			return &node->node_bool;
		break;
	case CTLTYPE_INT:
		if (node->node_flags & CTLFLAG_IMMEDIATE)
			return &node->node_int;
		break;
	case CTLTYPE_QUAD:
		if (node->node_flags & CTLFLAG_IMMEDIATE)
			return &node->node_quad;
		break;
	case CTLTYPE_STRING:
	case CTLTYPE_STRUCT:
		if (node->node_flags & CTLFLAG_IMMEDIATE)
			return NULL;
		break;
	default:
		return NULL;
	}

	return node->node_data;
}

/*
 * Read current (old) data from a regular data node, if requested.  Return the
 * old data length.
 */
static ssize_t
mib_read(struct mib_node * node, struct mib_oldp * oldp)
{
	void *ptr;
	size_t oldlen;
	int r;

	if ((ptr = mib_getptr(node)) == NULL)
		return EINVAL;

	if (SYSCTL_TYPE(node->node_flags) == CTLTYPE_STRING)
		oldlen = strlen(node->node_data) + 1;
	else
		oldlen = node->node_size;

	if (oldlen > SSIZE_MAX)
		return EINVAL;

	/* Copy out the current data, if requested at all. */
	if (oldp != NULL && (r = mib_copyout(oldp, 0, ptr, oldlen)) < 0)
		return r;

	/* Return the current length in any case. */
	return (ssize_t)oldlen;
}

/*
 * Write new data into a regular data node, if requested.
 */
static int
mib_write(struct mib_call * call, struct mib_node * node,
	struct mib_newp * newp, mib_verify_ptr verify)
{
	bool b[(sizeof(bool) == sizeof(char)) ? 1 : -1]; /* explained below */
	char *src, *dst;
	size_t newlen;
	int r;

	if (newp == NULL)
		return OK; /* nothing to do */

	/*
	 * When setting a new value, we cannot risk doing an in-place update:
	 * the copy from userland may fail halfway through, in which case an
	 * in-place update could leave the node value in a corrupted state.
	 * Thus, we must first fetch any new data into a temporary buffer.
	 *
	 * Given that we use intermediate data storage, we could support value
	 * swapping, where the user provides the same buffer for new and old
	 * data.  We choose not to: NetBSD does not support it, it would make
	 * trace(1)'s job a lot harder, and it would convolute the code here.
	 */
	newlen = mib_getnewlen(newp);

	if ((dst = mib_getptr(node)) == NULL)
		return EINVAL;

	switch (SYSCTL_TYPE(node->node_flags)) {
	case CTLTYPE_BOOL:
	case CTLTYPE_INT:
	case CTLTYPE_QUAD:
	case CTLTYPE_STRUCT:
		/* Non-string types must have an exact size match. */
		if (newlen != node->node_size)
			return EINVAL;
		break;
	case CTLTYPE_STRING:
		/*
		 * Strings must not exceed their buffer size.  There is a
		 * second check further below, because we allow userland to
		 * give us an unterminated string.  In that case we terminate
		 * it ourselves, but then the null terminator must fit as well.
		 */
		if (newlen > node->node_size)
			return EINVAL;
		break;
	default:
		return EINVAL;
	}

	/*
	 * If we cannot fit the data in the scratch buffer, then allocate a
	 * temporary buffer.  We add one extra byte so that we can add a null
	 * terminator at the end of strings in case userland did not supply
	 * one.  Either way, we must free the temporary buffer later!
	 *
	 * The alternative is to ensure that the given memory is accessible
	 * before starting the copy, but that would break if we ever add kernel
	 * threads or anything that allows asynchronous memory unmapping, etc.
	 */
	if (newlen + 1 > sizeof(scratch)) {
		/*
		 * In practice, the temporary buffer is at least an entire
		 * memory page, which is reasonable by any standard.  As a
		 * result, we can get away with refusing to perform dynamic
		 * allocation for unprivileged users.  This limits the impact
		 * that unprivileged users can have on our memory space.
		 */
		if (!mib_authed(call))
			return EPERM;

		/*
		 * Do not return ENOMEM on allocation failure, because ENOMEM
		 * implies that a valid old length was returned.
		 */
		if ((src = malloc(newlen + 1)) == NULL) {
			printf("MIB: out of memory!\n");

			return EINVAL;
		}
		mib_objects++;
	} else
		src = scratch;

	/* Copy in the data.  Note that newlen may be zero. */
	r = mib_copyin(newp, src, newlen);

	if (r == OK && verify != NULL && !verify(call, node, src, newlen))
		r = EINVAL;

	if (r == OK) {
		/* Check and, if acceptable, store the new value. */
		switch (SYSCTL_TYPE(node->node_flags)) {
		case CTLTYPE_BOOL:
			/*
			 * Due to the nature of the C99 _Bool type, we can not
			 * test directly whether the given boolean value is a
			 * value that is not "true" and not "false".  In the
			 * worst case, another value could invoke undefined
			 * behavior.  We try our best to sanitize the value
			 * without looking at it directly, which unfortunately
			 * requires us to test for the size of the bool type.
			 * We do that at compile time, hence the 'b' "array".
			 * Any size other than one byte is an ABI violation.
			 */
			b[0] = (bool)src[0];
			memcpy(dst, &b[0], sizeof(b[0]));
			break;
		case CTLTYPE_INT:
		case CTLTYPE_QUAD:
		case CTLTYPE_STRUCT:
			memcpy(dst, src, node->node_size);
			break;
		case CTLTYPE_STRING:
			if (newlen == node->node_size &&
			    src[newlen - 1] != '\0') {
				/* Our null terminator does not fit! */
				r = EINVAL;
				break;
			}
			/*
			 * We do not mind null characters in the middle.  In
			 * general, the buffer may contain garbage after the
			 * first null terminator, but such garbage will never
			 * end up being copied out.
			 */
			src[newlen] = '\0';
			strlcpy(dst, src, node->node_size);
			break;
		default:
			r = EINVAL;
		}
	}

	if (src != scratch) {
		free(src);
		mib_objects--;
	}

	return r;
}

/*
 * Read and/or write the value of a regular data node.  A regular data node is
 * a leaf node.  Typically, a leaf node has no associated function, in which
 * case this function will be used instead.  In addition, this function may be
 * used from handler functions as part of their functionality.
 */
ssize_t
mib_readwrite(struct mib_call * call, struct mib_node * node,
	struct mib_oldp * oldp, struct mib_newp * newp, mib_verify_ptr verify)
{
	ssize_t len;
	int r;

	/* Copy out old data, if requested.  Always get the old data length. */
	if ((r = len = mib_read(node, oldp)) < 0)
		return r;

	/* Copy in new data, if requested. */
	if ((r = mib_write(call, node, newp, verify)) != OK)
		return r;

	/* Return the old data length. */
	return len;
}

/*
 * Dispatch a sysctl call, by looking up the target node by its MIB name and
 * taking the appropriate action on the resulting node, if found.  Return the
 * old data length on success, or a negative error code on failure.
 */
ssize_t
mib_dispatch(struct mib_call * call, struct mib_oldp * oldp,
	struct mib_newp * newp)
{
	struct mib_node *parent, *node;
	ssize_t r;
	int id, is_leaf, can_restart, has_verify, has_func;

	assert(call->call_namelen <= CTL_MAXNAME);

	/*
	 * Resolve the name by descending into the node tree, level by level,
	 * starting at the MIB root.
	 */
	for (parent = &mib_root; call->call_namelen > 0; parent = node) {
		id = call->call_name[0];
		call->call_name++;
		call->call_namelen--;

		assert(SYSCTL_TYPE(parent->node_flags) == CTLTYPE_NODE);
		assert(parent->node_flags & CTLFLAG_PARENT);

		/*
		 * Check for meta-identifiers.  Regular identifiers are never
		 * negative, although node handler functions may take subpaths
		 * with negative identifiers that are not meta-identifiers
		 * (e.g., see KERN_PROC2).
		 */
		if (id < 0) {
			/*
			 * A meta-identifier must always be the last name
			 * component.
			 */
			if (call->call_namelen > 0)
				return EINVAL;

			switch (id) {
			case CTL_QUERY:
				return mib_query(call, parent, oldp, newp);
			case CTL_CREATE:
				return mib_create(call, parent, oldp, newp);
			case CTL_DESTROY:
				return mib_destroy(call, parent, oldp, newp);
			case CTL_DESCRIBE:
				return mib_describe(call, parent, oldp, newp);
			case CTL_CREATESYM:
			case CTL_MMAP:
			default:
				return EOPNOTSUPP;
			}
		}

		/* Locate the child node. */
		if ((node = mib_find(parent, id, NULL /*prevp*/)) == NULL)
			return ENOENT;

		/* Check if access is permitted at this level. */
		if ((node->node_flags & CTLFLAG_PRIVATE) && !mib_authed(call))
			return EPERM;

		/*
		 * Start by checking if the node is a remote node.  If so, let
		 * a remote service handle the remainder of this request.
		 * However, as part of attempting the remote call, we may
		 * discover that the remote service has died or that it is
		 * unmounting the subtree.  If the node was not a temporary
		 * mountpoint, we should (and do) continue with the request
		 * locally - if it was, it will already be deallocated and we
		 * must be very careful not to access 'node' again!
		 */
		is_leaf = (SYSCTL_TYPE(node->node_flags) != CTLTYPE_NODE);

		if (!is_leaf && (node->node_flags & CTLFLAG_REMOTE)) {
			/* Determine this before 'node' may disappear.. */
			can_restart = (node->node_flags & CTLFLAG_PARENT);

			r = mib_remote_call(call, node, oldp, newp);

			if (r != ERESTART || !can_restart)
				return (r != ERESTART) ? r : ENOENT;

			/* Service died, subtree is unmounted, keep going. */
			assert(SYSCTL_TYPE(node->node_flags) == CTLTYPE_NODE);
			assert(!(node->node_flags & CTLFLAG_REMOTE));
		}

		/*
		 * Is this a leaf node, and/or is this node handled by a
		 * function?  If either is true, resolution ends at this level.
		 * In order to save a few bytes of memory per node, we use
		 * different ways to determine whether there is a function
		 * depending on whether the node is a leaf or not.
		 */
		if (is_leaf) {
			has_verify = (node->node_flags & CTLFLAG_VERIFY);
			has_func = (!has_verify && node->node_func != NULL);
		} else {
			has_verify = FALSE;
			has_func = !(node->node_flags & CTLFLAG_PARENT);
		}

		/*
		 * The name may be longer only if the node is not a leaf.  That
		 * also applies to leaves with functions, so check this first.
		 */
		if (is_leaf && call->call_namelen > 0)
			return ENOTDIR;

		/*
		 * If resolution indeed ends here, and the user supplied new
		 * data, check if writing is allowed.  For functions, it is
		 * arguable whether we should do this check here already.
		 * However, for now, this approach covers all our use cases.
		 */
		if ((is_leaf || has_func) && newp != NULL) {
			if (!(node->node_flags & CTLFLAG_READWRITE))
				return EPERM;

			/*
			 * Unless nonprivileged users may write to this node,
			 * ensure that the user has superuser privileges.  The
			 * ANYWRITE flag does not override the READWRITE flag.
			 */
			if (!(node->node_flags & CTLFLAG_ANYWRITE) &&
			    !mib_authed(call))
				return EPERM;
		}

		/* If this node has a handler function, let it do the work. */
		if (has_func)
			return node->node_func(call, node, oldp, newp);

		/* For regular data leaf nodes, handle generic access. */
		if (is_leaf)
			return mib_readwrite(call, node, oldp, newp,
			    has_verify ? node->node_verify : NULL);

		/* No function and not a leaf?  Descend further. */
	}

	/* If we get here, the name refers to a node array. */
	return EISDIR;
}

/*
 * Recursively initialize the static tree at initialization time.
 */
static void
mib_tree_recurse(struct mib_node * parent)
{
	struct mib_node *node;
	int id;

	assert(SYSCTL_TYPE(parent->node_flags) == CTLTYPE_NODE);
	assert(parent->node_flags & CTLFLAG_PARENT);

	/*
	 * Later on, node_csize and node_clen will also include dynamically
	 * created nodes.  This means that we cannot use node_csize to iterate
	 * over the static nodes.
	 */
	parent->node_csize = parent->node_size;

	node = parent->node_scptr;

	for (id = 0; IS_STATIC_ID(parent, id); id++, node++) {
		if (node->node_flags == 0)
			continue;

		mib_nodes++;

		parent->node_clen++;

		node->node_ver = parent->node_ver;
		node->node_parent = parent;

		/* Recursively apply this function to all node children. */
		if (SYSCTL_TYPE(node->node_flags) == CTLTYPE_NODE &&
		    (node->node_flags & CTLFLAG_PARENT))
			mib_tree_recurse(node);
	}
}

/*
 * Go through the entire static tree, recursively, initializing some values
 * that could not be assigned at compile time.
 */
void
mib_tree_init(void)
{

	/* Initialize some variables. */
	mib_nodes = 1; /* the root node itself */
	mib_objects = 0;

	/*
	 * The entire tree starts with the same, nonzero node version.
	 * The root node is the only node without a parent.
	 */
	mib_root.node_ver = 1;
	mib_root.node_parent = NULL;

	/* Recursively initialize the static tree. */
	mib_tree_recurse(&mib_root);
}

/*
 * Process a subtree mount request from a remote service.  Return OK on
 * success, with a pointer to the resulting static-node structure stored in
 * 'nodep'.  Return a negative error code on failure.
 */
int
mib_mount(const int * mib, unsigned int miblen, unsigned int eid, uint32_t rid,
	uint32_t flags, unsigned int csize, unsigned int clen,
	struct mib_node ** nodep)
{
	struct mib_dynode *dynode, **prevp;
	struct mib_node *parent, *node;
	char name[SYSCTL_NAMELEN], *desc;
	size_t size, namelen, desclen;
	unsigned int n;
	int r, id;

	/*
	 * Perform initial verification of the given parameters.  Even stricter
	 * checks may be performed later.
	 */
	/*
	 * By policy, we forbid mounting top-level nodes.  This is in effect
	 * also the only security-like restriction: a service should not be
	 * able to just take over, say, the entire "kern" subtree.  There is
	 * currently little in the way of a service taking over an important
	 * set of second-level nodes, though.
	 *
	 * TODO: allow mounting of predefined mount points only, for example by
	 * having an internal node flag that permits mounting the subtree or
	 * any node in it.  As an even better alternative, allow this to be
	 * controlled through a policy specification; unfortunately, this would
	 * also add a substantial amount of infrastructure.
	 */
	if (miblen < 2) {
		MIB_DEBUG_MOUNT(("MIB: mounting failed, path too short\n"));

		return EPERM;
	}

	/*
	 * The flags field is highly restricted right now.  Only a few flags
	 * may be given at all, and then when using an existing node as mount
	 * point, the flag must exactly match the existing node's flags.
	 */
	if (SYSCTL_VERS(flags) != SYSCTL_VERSION ||
	    SYSCTL_TYPE(flags) != CTLTYPE_NODE ||
	    (SYSCTL_FLAGS(flags) & ~(CTLFLAG_READONLY | CTLFLAG_READWRITE |
	    CTLFLAG_PERMANENT | CTLFLAG_HIDDEN)) != 0) {
		MIB_DEBUG_MOUNT(("MIB: mounting failed, invalid flags %"PRIx32
		    "\n", flags));

		return EINVAL;
	}

	if (csize > (1U << MIB_RC_BITS) || clen > csize) {
		MIB_DEBUG_MOUNT(("MIB: mounting failed, invalid child size or "
		    "length (%u, %u)\n", csize, clen));

		return EINVAL;
	}

	/*
	 * Look up the parent node of the mount point.  This parent node must
	 * exist - we don't want to create more than one temporary node in any
	 * case.  All the nodes leading up to and including the parent node
	 * must be real, local, non-private, node-type nodes.  The path may not
	 * be private, because that would allow an unprivileged service to
	 * intercept writes to privileged nodes--currently a total nonissue in
	 * practice, but still.  Note that the service may itself restrict
	 * access to nodes in its own mounted subtree in any way it wishes.
	 */
	parent = &mib_root;

	for (n = 0; n < miblen - 1; n++) {
		/* Meta-identifiers are obviously not allowed in the path. */
		if ((id = mib[n]) < 0) {
			MIB_DEBUG_MOUNT(("MIB: mounting failed, meta-ID in "
			    "path\n"));

			return EINVAL;
		}

		/* Locate the child node. */
		if ((node = mib_find(parent, id, NULL /*prevp*/)) == NULL) {
			MIB_DEBUG_MOUNT(("MIB: mounting failed, path not "
			    "found\n"));

			return ENOENT;
		}

		/* Make sure it is a regular node-type node. */
		if (SYSCTL_TYPE(node->node_flags) != CTLTYPE_NODE ||
		    !(node->node_flags & CTLFLAG_PARENT) ||
		    (node->node_flags & (CTLFLAG_REMOTE | CTLFLAG_PRIVATE))) {
			MIB_DEBUG_MOUNT(("MIB: mounting failed, unacceptable "
			    "node on path\n"));

			return EPERM;
		}

		parent = node;
	}

	/* Now see if the mount point itself exists. */
	if ((id = mib[miblen - 1]) < 0) {
		MIB_DEBUG_MOUNT(("MIB: mounting failed, meta-ID in path\n"));

		return EINVAL;
	}

	/*
	 * If the target node exists and passes all tests, it will simply be
	 * converted to a mount point.  If the target node does not exist, we
	 * have to allocate a temporary node as mount point.
	 */
	if ((node = mib_find(parent, id, NULL /*prevp*/)) != NULL) {
		/*
		 * We are about to mount on an existing node.  As stated above,
		 * the node flags must match the given flags exactly.
		 */
		if (SYSCTL_TYPE(node->node_flags) != CTLTYPE_NODE ||
		    SYSCTL_FLAGS(node->node_flags) !=
		    (SYSCTL_FLAGS(flags) | CTLFLAG_PARENT)) {
			MIB_DEBUG_MOUNT(("MIB: mounting failed, target node "
			    "mismatch (%"PRIx32", %"PRIx32")\n",
			    node->node_flags, flags));

			return EPERM;
		}

		/*
		 * If the node has dynamically added children, we will not be
		 * able to restore the node to its old state when unmounting.
		 */
		if (node->node_size != node->node_csize) {
			MIB_DEBUG_MOUNT(("MIB: mounting failed, node has "
			    "dynamic children\n"));

			return EBUSY;
		}

		mib_upgrade(node);
	} else {
		/*
		 * We are going to create a temporary mount point.  Much of the
		 * procedure that follows is a rather selective extract from
		 * mib_create().  Start with a check for the impossible.
		 */
		if (parent->node_csize == INT_MAX) {
			MIB_DEBUG_MOUNT(("MIB: mounting failed, parent node "
			    "full\n"));

			return EINVAL;
		}

		/*
		 * In order to create the new node, we also need the node's
		 * name and description; those did not fit in the request
		 * message.  Ask the caller to copy these strings to us.
		 */
		name[0] = '\0';
		scratch[0] = '\0';

		if ((r = mib_remote_info(eid, rid, name, sizeof(name), scratch,
		    MAXDESCLEN)) != OK) {
			MIB_DEBUG_MOUNT(("MIB: mounting failed, node info "
			    "request yielded %d\n", r));

			return r;
		}

		/* Make sure the name is valid. */
		if ((namelen = mib_check_name(name, sizeof(name))) == 0) {
			printf("MIB: mounting failed, bad name\n");

			return EINVAL;
		}

		/* Just forcefully terminate the description. */
		scratch[MAXDESCLEN - 1] = '\0';
		desclen = strlen(scratch);

		/*
		 * We know the identifier is not in use yet; make sure that the
		 * name is not, either.  As a side effect, find out where the
		 * new node should be inserted upon success.
		 */
		if (mib_scan(parent, id, name, &id /*unused*/, &prevp,
		    &node /*unused*/) != OK) {
			MIB_DEBUG_MOUNT(("MIB: mounting failed, name "
			    "conflict\n"));

			return EEXIST;
		}

		/*
		 * Allocate a dynamic node.  Unlike for user-created dynamic
		 * nodes, temporary mount points also include the description
		 * in the dynode object.
		 */
		size = sizeof(*dynode) + namelen + desclen + 1;

		if ((dynode = malloc(size)) == NULL) {
			printf("MIB: out of memory!\n");

			return ENOMEM;
		}
		mib_objects++;

		/* Initialize the dynamic node. */
		memset(dynode, 0, sizeof(*dynode));
		dynode->dynode_id = id;
		strlcpy(dynode->dynode_name, name, namelen + 1);
		desc = &dynode->dynode_name[namelen + 1];
		strlcpy(desc, scratch, desclen + 1);

		node = &dynode->dynode_node;
		node->node_flags = flags & ~SYSCTL_VERS_MASK;
		node->node_size = 0;
		node->node_parent = parent;
		node->node_name = dynode->dynode_name;
		node->node_desc = desc;

		/*
		 * Add the new dynamic node into the tree, and adjust versions
		 * and counters.
		 */
		mib_add(dynode, prevp);
	}

	/* Success!  Perform the actual mount, and return the target node. */
	node->node_flags |= CTLFLAG_REMOTE;
	node->node_eid = eid;
	node->node_rcsize = csize;
	node->node_rclen = clen;
	node->node_rid = rid;

	mib_remotes++;

	*nodep = node;
	return OK;
}

/*
 * Unmount the remote subtree identified by the given node.  Release the mount
 * point by reversing the action performed while mounting.  Also bump the
 * version numbers on the path, so that userland knows that it is to expect a
 * change of contents in the subtree.  This function always succeeds, and may
 * deallocate the given node.
 */
void
mib_unmount(struct mib_node * node)
{
	struct mib_dynode **prevp;
	struct mib_node *child;
	int id;

	assert(SYSCTL_TYPE(node->node_flags) == CTLTYPE_NODE);
	assert(node->node_flags & CTLFLAG_REMOTE);

	/*
	 * Given that the node has the CTLFLAG_REMOTE flag set, we can now tell
	 * whether the remote subtree obscured a preexisting node or we created
	 * a temporary mount point, by checking its CTLFLAG_PARENT flag.
	 */
	if (node->node_flags & CTLFLAG_PARENT) {
		/*
		 * Return the node to its former pre-mount state.  Restore the
		 * original node_clen field by recomputing it.
		 */
		node->node_flags &= ~CTLFLAG_REMOTE;
		node->node_csize = node->node_size;
		node->node_clen = 0;

		for (id = 0; IS_STATIC_ID(node, id); id++) {
			child = &node->node_scptr[id];

			if (child->node_flags != 0)
				node->node_clen++;
		}

		node->node_dcptr = NULL;

		/* Increase version numbers on the path to the node. */
		mib_upgrade(node);
	} else {
		/*
		 * We know that we dynamically allocated this node; find its
		 * parent's pointer to it.
		 */
		for (prevp = &node->node_parent->node_dcptr; *prevp != NULL;
		    prevp = &(*prevp)->dynode_next) {
			if (&(*prevp)->dynode_node == node)
				break;
		}
		assert(*prevp != NULL);

		/* Free the node, and adjust counts and versions. */
		mib_remove(node, prevp);
	}

	assert(mib_remotes > 0);
	mib_remotes--;
}
