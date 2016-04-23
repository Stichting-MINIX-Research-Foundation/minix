/* MIB service - main.c - request abstraction and first-level tree */
/*
 * This is the Management Information Base (MIB) service.  Its one and only
 * task is to implement the sysctl(2) system call, which plays a fairly
 * important role in parts of *BSD userland.
 *
 * The sysctl(2) interface is used to access a variety of information.  In
 * order to obtain that information, and possibly modify it, the MIB service
 * calls into many other services.  The MIB service must therefore not be
 * called directly from other services, with the exception of ProcFS.  In fact,
 * ProcFS is currently the only service that is modeled as logically higher in
 * the MINIX3 service stack than MIB, something that itself is possible only
 * due to the nonblocking nature of VFS.  MIB may issue blocking calls to VFS.
 *
 * The MIB service is in the boot image because even init(8) makes use of
 * sysctl(2) during its own startup, so launching the MIB service at any later
 * time would make a proper implementation of sysctl(2) impossible.  Also, the
 * service needs superuser privileges because it may need to issue privileged
 * calls and obtain privileged information from other services.
 *
 * While most of the sysctl tree is maintained locally, the MIB service also
 * allows other services to register "remote" subtrees which are then handled
 * entirely by those services.  This feature, which works much like file system
 * mounting, allows 1) sysctl handling code to stay local to its corresponding
 * service, and 2) parts of the sysctl tree to adapt and expand dynamically as
 * optional services are started and stopped.  Compared to the MIB service's
 * local handling, remotely handled subtrees are subject to several additional
 * practical restrictions, hoever.  In the current implementation, the MIB
 * service makes blocking calls to remote services as needed; in the future,
 * these interactions could be made (more) asynchronous.
 *
 * The MIB service was created by David van Moolenbroek <david@minix3.org>.
 */

#include "mib.h"

/*
 * Most of these initially empty nodes are filled in by their corresponding
 * modules' _init calls; see mib_init below.  However, some subtrees are not
 * populated by the MIB service itself.  CTL_NET is expected to be populated
 * through registration of remote subtrees.  The libc sysctl(3) wrapper code
 * takes care of the CTL_USER subtree.  It must have an entry here though, or
 * sysctl(8) will not list it.  CTL_VENDOR is also empty, but writable, so that
 * it may be used by third parties.
 */
static struct mib_node mib_table[] = {
/* 1*/	[CTL_KERN]	= MIB_ENODE(_P | _RO, "kern", "High kernel"),
/* 2*/	[CTL_VM]	= MIB_ENODE(_P | _RO, "vm", "Virtual memory"),
/* 4*/	[CTL_NET]	= MIB_ENODE(_P | _RO, "net", "Networking"),
/* 6*/	[CTL_HW]	= MIB_ENODE(_P | _RO, "hw", "Generic CPU, I/O"),
/* 8*/	[CTL_USER]	= MIB_ENODE(_P | _RO, "user", "User-level"),
/*11*/	[CTL_VENDOR]	= MIB_ENODE(_P | _RW, "vendor", "Vendor specific"),
/*32*/	[CTL_MINIX]	= MIB_ENODE(_P | _RO, "minix", "MINIX3 specific"),
};

/*
 * The root node of the tree.  The root node is used internally only--it is
 * impossible to access the root node itself from userland in any way.  The
 * node is writable by default, so that programs such as init(8) may create
 * their own top-level entries.
 */
struct mib_node mib_root = MIB_NODE(_RW, mib_table, "", "");

/*
 * Structures describing old and new data as provided by userland.  The primary
 * advantage of these opaque structures is that we could in principle use them
 * to implement storage of small data results in the sysctl reply message, so
 * as to avoid the kernel copy, without changing any of the handler code.
 */
struct mib_oldp {
	endpoint_t oldp_endpt;
	vir_bytes oldp_addr;
	size_t oldp_len;
};
/*
 * Same structure, different type: prevent accidental mixups, and avoid the
 * need to use __restrict everywhere.
 */
struct mib_newp {
	endpoint_t newp_endpt;
	vir_bytes newp_addr;
	size_t newp_len;
};

/*
 * Return TRUE or FALSE indicating whether the given offset is within the range
 * of data that is to be copied out.  This call can be used to test whether
 * certain bits of data need to be prepared for copying at all.
 */
int
mib_inrange(struct mib_oldp * oldp, size_t off)
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
mib_getoldlen(struct mib_oldp * oldp)
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
mib_copyout(struct mib_oldp * __restrict oldp, size_t off,
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

	if ((r = sys_datacopy(SELF, (vir_bytes)buf, oldp->oldp_endpt,
	    oldp->oldp_addr + off, len)) != OK)
		return r;

	return size;
}

/*
 * Override the oldlen value returned from the call, in situations where an
 * error is thrown as well.
 */
void
mib_setoldlen(struct mib_call * call, size_t oldlen)
{

	call->call_reslen = oldlen;
}

/*
 * Return the new data length as provided by the user, or 0 if the user did not
 * supply new data.
 */
size_t
mib_getnewlen(struct mib_newp * newp)
{

	if (newp == NULL)
		return 0;

	return newp->newp_len;
}

/*
 * Copy in data from the user.  The given length must match exactly the length
 * given by the user.  Return OK or an error code.
 */
int
mib_copyin(struct mib_newp * __restrict newp, void * __restrict buf,
	size_t len)
{

	if (newp == NULL || len != newp->newp_len)
		return EINVAL;

	if (len == 0)
		return OK;

	return sys_datacopy(newp->newp_endpt, newp->newp_addr, SELF,
	    (vir_bytes)buf, len);
}

/*
 * Copy in auxiliary data from the user, based on a user pointer obtained from
 * data copied in earlier through mib_copyin().
 */
int
mib_copyin_aux(struct mib_newp * __restrict newp, vir_bytes addr,
	void * __restrict buf, size_t len)
{

	assert(newp != NULL);

	if (len == 0)
		return OK;

	return sys_datacopy(newp->newp_endpt, addr, SELF, (vir_bytes)buf, len);
}

/*
 * Create a grant for a call's old data region, if not NULL, for the given
 * endpoint.  On success, store the grant (or GRANT_INVALID) in grantp and the
 * length in lenp, and return OK.  On error, return an error code that must not
 * be ENOMEM.
 */
int
mib_relay_oldp(endpoint_t endpt, struct mib_oldp * __restrict oldp,
	cp_grant_id_t * grantp, size_t * __restrict lenp)
{

	if (oldp != NULL) {
		*grantp = cpf_grant_magic(endpt, oldp->oldp_endpt,
		    oldp->oldp_addr, oldp->oldp_len, CPF_WRITE);
		if (!GRANT_VALID(*grantp))
			return EINVAL;
		*lenp = oldp->oldp_len;
	} else {
		*grantp = GRANT_INVALID;
		*lenp = 0;
	}

	return OK;
}

/*
 * Create a grant for a call's new data region, if not NULL, for the given
 * endpoint.  On success, store the grant (or GRANT_INVALID) in grantp and the
 * length in lenp, and return OK.  On error, return an error code that must not
 * be ENOMEM.
 */
int
mib_relay_newp(endpoint_t endpt, struct mib_newp * __restrict newp,
	cp_grant_id_t * grantp, size_t * __restrict lenp)
{

	if (newp != NULL) {
		*grantp = cpf_grant_magic(endpt, newp->newp_endpt,
		    newp->newp_addr, newp->newp_len, CPF_READ);
		if (!GRANT_VALID(*grantp))
			return EINVAL;
		*lenp = newp->newp_len;
	} else {
		*grantp = GRANT_INVALID;
		*lenp = 0;
	}

	return OK;
}

/*
 * Check whether the user is allowed to perform privileged operations.  The
 * function returns a nonzero value if this is the case, and zero otherwise.
 * Authorization is performed only once per call.
 */
int
mib_authed(struct mib_call * call)
{

	if ((call->call_flags & (MIB_FLAG_AUTH | MIB_FLAG_NOAUTH)) == 0) {
		/* Ask PM if this endpoint has superuser privileges. */
		if (getnuid(call->call_endpt) == SUPER_USER)
			call->call_flags |= MIB_FLAG_AUTH;
		else
			call->call_flags |= MIB_FLAG_NOAUTH;
	}

	return (call->call_flags & MIB_FLAG_AUTH);
}

/*
 * Implement the sysctl(2) system call.
 */
static int
mib_sysctl(message * __restrict m_in, int ipc_status,
	message * __restrict m_out)
{
	vir_bytes oldaddr, newaddr;
	size_t oldlen, newlen;
	unsigned int namelen;
	int s, name[CTL_MAXNAME];
	endpoint_t endpt;
	struct mib_oldp oldp, *oldpp;
	struct mib_newp newp, *newpp;
	struct mib_call call;
	ssize_t r;

	/* Only handle blocking calls.  Ignore everything else. */
	if (IPC_STATUS_CALL(ipc_status) != SENDREC)
		return EDONTREPLY;

	endpt = m_in->m_source;
	oldaddr = m_in->m_lc_mib_sysctl.oldp;
	oldlen = m_in->m_lc_mib_sysctl.oldlen;
	newaddr = m_in->m_lc_mib_sysctl.newp;
	newlen = m_in->m_lc_mib_sysctl.newlen;
	namelen = m_in->m_lc_mib_sysctl.namelen;

	if (namelen == 0 || namelen > CTL_MAXNAME)
		return EINVAL;

	/*
	 * In most cases, the entire name fits in the request message, so we
	 * can avoid a kernel copy.
	 */
	if (namelen > CTL_SHORTNAME) {
		if ((s = sys_datacopy(endpt, m_in->m_lc_mib_sysctl.namep, SELF,
		    (vir_bytes)&name, sizeof(name[0]) * namelen)) != OK)
			return s;
	} else
		memcpy(name, m_in->m_lc_mib_sysctl.name,
		    sizeof(name[0]) * namelen);

	/*
	 * Set up a structure for the old data, if any.  When no old address is
	 * given, be forgiving if oldlen is not zero, as the user may simply
	 * not have initialized the variable before passing a pointer to it.
	 */
	if (oldaddr != 0) {
		oldp.oldp_endpt = endpt;
		oldp.oldp_addr = oldaddr;
		oldp.oldp_len = oldlen;
		oldpp = &oldp;
	} else
		oldpp = NULL;

	/*
	 * Set up a structure for the new data, if any.  If one of newaddr and
	 * newlen is zero but not the other, we (like NetBSD) disregard both.
	 */
	if (newaddr != 0 && newlen != 0) {
		newp.newp_endpt = endpt;
		newp.newp_addr = newaddr;
		newp.newp_len = newlen;
		newpp = &newp;
	} else
		newpp = NULL;

	/*
	 * Set up a structure for other call parameters.  Most of these should
	 * be used rarely, and we may want to add more later, so do not pass
	 * all of them around as actual function parameters all the time.
	 */
	call.call_endpt = endpt;
	call.call_name = name;
	call.call_namelen = namelen;
	call.call_flags = 0;
	call.call_reslen = 0;

	r = mib_dispatch(&call, oldpp, newpp);

	/*
	 * From NetBSD: we copy out as much as we can from the old data, while
	 * at the same time computing the full data length.  Then, here at the
	 * end, if the entire result did not fit in the destination buffer, we
	 * return ENOMEM instead of success, thus also returning a partial
	 * result and the full data length.
	 *
	 * It is also possible that data are copied out along with a "real"
	 * error.  In that case, we must report a nonzero resulting length
	 * along with that error code.  This is currently the case when node
	 * creation resulted in a collision, in which case the error code is
	 * EEXIST while the existing node is copied out as well.
	 */
	if (r >= 0) {
		m_out->m_mib_lc_sysctl.oldlen = (size_t)r;

		if (oldaddr != 0 && oldlen < (size_t)r)
			r = ENOMEM;
		else
			r = OK;
	} else
		m_out->m_mib_lc_sysctl.oldlen = call.call_reslen;

	return r;
}

/*
 * Initialize the service.
 */
static int
mib_init(int type __unused, sef_init_info_t * info __unused)
{

	/*
	 * Initialize pointers and sizes of subtrees in different modules.
	 * This is needed because we cannot use sizeof on external arrays.
	 * We do initialize the node entry (including any other fields)
	 * statically through MIB_ENODE because that forces the array to be
	 * large enough to store the entry.
	 */
	mib_kern_init(&mib_table[CTL_KERN]);
	mib_vm_init(&mib_table[CTL_VM]);
	mib_hw_init(&mib_table[CTL_HW]);
	mib_minix_init(&mib_table[CTL_MINIX]);

	/*
	 * Now that the static tree is complete, go through the entire tree,
	 * initializing miscellaneous fields.
	 */
	mib_tree_init();

	/* Prepare for requests to mount remote subtrees. */
	mib_remote_init();

	return OK;
}

/*
 * Perform SEF startup.
 */
static void
mib_startup(void)
{

	sef_setcb_init_fresh(mib_init);
	/*
	 * If we restart we lose all dynamic state, which means we lose all
	 * nodes that have been created at run time.  However, running with
	 * only the static node tree is still better than not running at all.
	 */
	sef_setcb_init_restart(mib_init);

	sef_startup();
}

/*
 * The Management Information Base (MIB) service.
 */
int
main(void)
{
	message m_in, m_out;
	int r, ipc_status;

	/* Perform initialization. */
	mib_startup();

	/* The main message loop. */
	for (;;) {
		/* Receive a request. */
		if ((r = sef_receive_status(ANY, &m_in, &ipc_status)) != OK)
			panic("sef_receive failed: %d", r);

		/* Process the request. */
		if (is_ipc_notify(ipc_status)) {
			/* We are not expecting any notifications. */
			printf("MIB: notification from %d\n", m_in.m_source);

			continue;
		}

		memset(&m_out, 0, sizeof(m_out));

		switch (m_in.m_type) {
		case MIB_SYSCTL:
			r = mib_sysctl(&m_in, ipc_status, &m_out);

			break;

		case MIB_REGISTER:
			r = mib_register(&m_in, ipc_status);

			break;

		case MIB_DEREGISTER:
			r = mib_deregister(&m_in, ipc_status);

			break;

		default:
			if (IPC_STATUS_CALL(ipc_status) == SENDREC)
				r = ENOSYS;
			else
				r = EDONTREPLY;
		}

		/* Send a reply, if applicable. */
		if (r != EDONTREPLY) {
			m_out.m_type = r;

			if ((r = ipc_sendnb(m_in.m_source, &m_out)) != OK)
				printf("MIB: ipc_sendnb failed (%d)\n", r);
		}
	}

	/* NOTREACHED */
	return 0;
}
