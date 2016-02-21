/* UNIX Domain Sockets - uds.c - socket management */

#include "uds.h"

static struct udssock uds_array[NR_UDSSOCK];
static TAILQ_HEAD(uds_freelist, udssock) uds_freelist;
static unsigned int uds_in_use;
static int uds_running;

static const struct sockevent_ops uds_ops;

static SLIST_HEAD(udshash, udssock) udshash[UDSHASH_SLOTS];

/*
 * Initialize file-to-socket hash table.
 */
static void
udshash_init(void)
{
	unsigned int slot;

	for (slot = 0; slot < __arraycount(udshash); slot++)
		SLIST_INIT(&udshash[slot]);
}

/*
 * Return a hash table slot number for the given <dev,ino> pair.
 */
static unsigned int
udshash_slot(dev_t dev, ino_t ino)
{

	assert(dev != NO_DEV);
	assert(ino != 0);

	/*
	 * Effectively combining two 64-bit numbers into a single 6-or-so-bit
	 * hash is not too easy.  This hash function is probably among the
	 * worst options.  Then again it is not all that critical as we are not
	 * expecting that many bound UDS sockets in the system anyway.
	 */
	return (unsigned int)(dev ^ ino) % UDSHASH_SLOTS;
}

/*
 * Look for a socket that is bound to the given <dev,ino> pair.  Return a
 * pointer to the socket if found, or NULL otherwise.
 */
static struct udssock *
udshash_get(dev_t dev, ino_t ino)
{
	struct udssock *uds;
	unsigned int slot;

	slot = udshash_slot(dev, ino);

	SLIST_FOREACH(uds, &udshash[slot], uds_hash) {
		if (uds->uds_dev == dev && uds->uds_ino == ino)
			return uds;
	}

	return NULL;
}

/*
 * Add a socket to the file-to-socket hash table.  The socket must have its
 * device and inode fields set, and must not be in the hash table already.
 */
static void
udshash_add(struct udssock * uds)
{
	unsigned int slot;

	slot = udshash_slot(uds->uds_dev, uds->uds_ino);

	SLIST_INSERT_HEAD(&udshash[slot], uds, uds_hash);
}

/*
 * Remove a socket from the file-to-socket hash table.  The socket must be in
 * the hash table.
 */
static void
udshash_del(struct udssock * uds)
{
	unsigned int slot;

	slot = udshash_slot(uds->uds_dev, uds->uds_ino);

	/* This macro is O(n). */
	SLIST_REMOVE(&udshash[slot], uds, udssock, uds_hash);
}

/*
 * Return the socket identifier for the given UDS socket object.
 */
sockid_t
uds_get_id(struct udssock * uds)
{

	return (sockid_t)(uds - uds_array);
}

/*
 * Given either NULL or a previously returned socket, return the next in-use
 * UDS socket of the given socket type, or NULL if there are no more matches.
 * The sockets are returned in random order, but each matching socket is
 * returned exactly once (until any socket is allocated or freed).
 */
struct udssock *
uds_enum(struct udssock * prev, int type)
{
	sockid_t id;

	if (prev != NULL)
		id = uds_get_id(prev) + 1;
	else
		id = 0;

	for (; id < NR_UDSSOCK; id++)
		if ((uds_array[id].uds_flags & UDSF_IN_USE) &&
		    uds_get_type(&uds_array[id]) == type)
			return &uds_array[id];

	return NULL;
}

/*
 * Invalidate credentials on the socket.
 */
static void
uds_clear_cred(struct udssock * uds)
{

	uds->uds_cred.unp_pid = -1;
	uds->uds_cred.unp_euid = -1;
	uds->uds_cred.unp_egid = -1;
}

/*
 * Obtain the credentials (process, user, and group ID) of the given user
 * endpoint and associate them with the socket for later retrieval.  It is
 * important to note that this information is obtained once at connect time,
 * and never updated later.  The party receiving the credentials must take this
 * into account.
 */
static void
uds_get_cred(struct udssock * uds, endpoint_t user_endpt)
{
	int r;

	if ((uds->uds_cred.unp_pid = r = getepinfo(user_endpt,
	    &uds->uds_cred.unp_euid, &uds->uds_cred.unp_egid)) < 0) {
		printf("UDS: failed obtaining credentials of %d (%d)\n",
		    user_endpt, r);

		uds_clear_cred(uds);
	}
}

/*
 * Allocate and initialize a UDS socket.  On succes, return OK with a pointer
 * to the new socket in 'udsp'.  On failure, return a negative error code.
 */
static int
uds_alloc(struct udssock ** udsp)
{
	struct udssock *uds;
	int r;

	/* Allocate, initialize, and return a UNIX domain socket object. */
	if (TAILQ_EMPTY(&uds_freelist))
		return ENOBUFS;

	uds = TAILQ_FIRST(&uds_freelist);

	uds->uds_conn = NULL;		/* not connected */
	uds->uds_link = NULL;		/* not connecting or linked */
	uds->uds_queued = 0;
	uds->uds_flags = UDSF_IN_USE;	/* may be found through enumeration */
	uds->uds_pathlen = 0;		/* not bound: no path */
	uds->uds_dev = NO_DEV;		/* not hashed: no socket file device */
	uds->uds_ino = 0;		/* not hashed: no socket file inode */
	uds_clear_cred(uds);		/* no bind/connect-time credentials */
	TAILQ_INIT(&uds->uds_queue);	/* an empty queue */

	if ((r = uds_io_setup(uds)) != OK)
		return r;

	TAILQ_REMOVE(&uds_freelist, uds, uds_next);

	assert(uds_in_use < NR_UDSSOCK);
	uds_in_use++;

	*udsp = uds;
	return OK;
}

/*
 * Free a previously allocated socket.
 */
static void
uds_free(struct sock * sock)
{
	struct udssock *uds = (struct udssock *)sock;

	uds_io_cleanup(uds);

	uds->uds_flags = 0;		/* no longer in use */

	TAILQ_INSERT_HEAD(&uds_freelist, uds, uds_next);

	assert(uds_in_use > 0);
	if (--uds_in_use == 0 && uds_running == FALSE)
		sef_cancel();
}

/*
 * Create a new socket.
 */
static sockid_t
uds_socket(int domain, int type, int protocol, endpoint_t user_endpt __unused,
	struct sock ** sockp, const struct sockevent_ops ** ops)
{
	struct udssock *uds;
	int r;

	dprintf(("UDS: socket(%d,%d,%d)\n", domain, type, protocol));

	if (domain != PF_UNIX) {
		/* This means the service was configured incorrectly. */
		printf("UDS: got request for domain %d\n", domain);

		return EAFNOSUPPORT;
	}

	/* We support the following three socket types. */
	switch (type) {
	case SOCK_STREAM:
	case SOCK_SEQPACKET:
	case SOCK_DGRAM:
		break;
	default:
		return EPROTOTYPE;
	}

	/*
	 * The PF_UNIX domain does not support particular protocols, so the
	 * given protocol must be zero (= anything that matches).
	 */
	if (protocol != UDSPROTO_UDS)
		return EPROTONOSUPPORT;

	if ((r = uds_alloc(&uds)) != OK)
		return r;

	dprintf(("UDS: socket returns %d\n", uds_get_id(uds)));

	*sockp = &uds->uds_sock;
	*ops = &uds_ops;
	return uds_get_id(uds);
}

/*
 * Connect a pair of sockets.
 */
static int
uds_pair(struct sock * sock1, struct sock * sock2, endpoint_t user_endpt)
{
	struct udssock *uds1 = (struct udssock *)sock1;
	struct udssock *uds2 = (struct udssock *)sock2;

	dprintf(("UDS: pair(%d,%d)\n", uds_get_id(uds1), uds_get_id(uds2)));

	/* Only connection-oriented types are acceptable. */
	if (uds_get_type(uds1) == SOCK_DGRAM)
		return EOPNOTSUPP;

	/* Connect the sockets. */
	uds1->uds_conn = uds2;
	uds2->uds_conn = uds1;
	uds1->uds_flags |= UDSF_CONNECTED;
	uds2->uds_flags |= UDSF_CONNECTED;

	/* Obtain the (same) credentials for both sides of the connection. */
	uds_get_cred(uds1, user_endpt);
	memcpy(&uds2->uds_cred, &uds1->uds_cred, sizeof(uds2->uds_cred));

	return OK;
}

/*
 * Disconnect a UDS socket, notifying or freeing up the other end of the
 * connection depending on whether the socket was linked, that is, on the
 * accept queue of a listening socket.
 */
static void
uds_disconnect(struct udssock * uds, int was_linked)
{
	struct udssock *conn;

	assert(uds_is_connected(uds));
	assert(uds_has_conn(uds));

	conn = uds->uds_conn;

	assert(uds_is_connected(conn));
	assert(uds_has_conn(conn));
	assert(!uds_has_link(conn));
	assert(conn->uds_conn == uds);

	/* Disconnect the sockets. */
	uds->uds_conn = NULL;
	conn->uds_conn = NULL;

	/*
	 * If the given socket is linked, then it is a connected socket for
	 * which the other end has been created but not yet accepted.  In that
	 * case, the other end ('conn') will have to be freed up.  Otherwise,
	 * it is a regular user-created socket and we must properly transition
	 * it into disconnected state.
	 */
	if (!was_linked) {
		sockevent_raise(&conn->uds_sock, SEV_SEND | SEV_RECV);

		/*
		 * Clear the peer credentials so that they will not be mistaken
		 * for having been obtained at bind time.
		 */
		uds_clear_cred(conn);
	} else
		sockevent_raise(&conn->uds_sock, SEV_CLOSE);
}

/*
 * Add the socket 'link' to the queue of the socket 'uds'.  This also implies
 * that 'link's link socket is set to 'uds'.
 */
static void
uds_add_queue(struct udssock * uds, struct udssock * link)
{

	dprintf(("UDS: add_queue(%d,%d)\n",
	    uds_get_id(uds), uds_get_id(link)));

	TAILQ_INSERT_TAIL(&uds->uds_queue, link, uds_next);

	uds->uds_queued++;
	assert(uds->uds_queued != 0);

	link->uds_link = uds;
}

/*
 * Remove the socket 'link' from the queue of the socket 'uds'.  This also
 * reset 'link's link to NULL.
 */
static void
uds_del_queue(struct udssock * uds, struct udssock * link)
{

	dprintf(("UDS: del_queue(%d,%d)\n",
	    uds_get_id(uds), uds_get_id(link)));

	assert(link->uds_link == uds);

	TAILQ_REMOVE(&uds->uds_queue, link, uds_next);

	assert(uds->uds_queued > 0);
	uds->uds_queued--;

	link->uds_link = NULL;
}

/*
 * Remove all sockets from the queue of the socket 'uds', with the exception of
 * 'except' if non-NULL.  Raise an ECONNRESET error on all removed sockets that
 * are not equal to 'uds'.
 */
static void
uds_clear_queue(struct udssock * uds, struct udssock * except)
{
	struct udssock *link, *tmp;
	int found;

	dprintf(("UDS: clear_queue(%d,%d)\n",
	    uds_get_id(uds), (except != NULL) ? uds_get_id(except) : -1));

	found = 0;

	/*
	 * Abort all connecting sockets queued on this socket, except for the
	 * given exception, which may be NULL.
	 */
	TAILQ_FOREACH_SAFE(link, &uds->uds_queue, uds_next, tmp) {
		if (link == except) {
			found++;

			continue;
		}

		dprintf(("UDS: clear_queue removes %d\n", uds_get_id(link)));

		assert(uds_get_type(link) == SOCK_DGRAM ||
		    uds_is_connecting(link) || uds_is_connected(link));

		uds_del_queue(uds, link);

		/*
		 * Generate an error only if the socket was not linked to
		 * itself (only datagram sockets can be linked to themselves).
		 * The error is not helpful for applications in that case.
		 */
		if (uds != link)
			sockevent_set_error(&link->uds_sock, ECONNRESET);

		/*
		 * If this is a listening socket, disconnect the connecting or
		 * connected end.  If a connected peer was already created for
		 * the queued socket, dispose of that peer.
		 *
		 * Clear credentials obtained when starting to connect (in
		 * which case the socket is always a connection-oriented
		 * socket), so that they will not be mistaken for credentials
		 * obtained at bind time.
		 */
		if (uds_get_type(link) != SOCK_DGRAM) {
			if (uds_is_connected(link))
				uds_disconnect(link, TRUE /*was_linked*/);
			else
				uds_clear_cred(link);
		}
	}

	assert(uds->uds_queued == found);
}

/*
 * Check whether the socket address given in 'addr', with length 'addr_len', is
 * a valid UNIX domain socket address (including a path to a socket file).  On
 * success, return the (non-zero) length of the socket file's path, minus the
 * null terminator which may in fact not be present.  The caller is responsible
 * for copying and terminating the path as needed.  A pointer to the path as
 * stored in 'addr' is returned in 'pathp'.  On failure, return an error code.
 */
static int
uds_check_addr(const struct sockaddr * addr, socklen_t addr_len,
	const char ** pathp)
{
	const char *p;
	size_t len;

	/*
	 * We could cast to a sockaddr_un structure pointer first, but that
	 * would not provide any benefits here.  Instead, we use sa_data as the
	 * generic equivalent of sun_path.
	 */
	if (addr_len < offsetof(struct sockaddr, sa_data))
		return EINVAL;

	if (addr->sa_family != AF_UNIX)
		return EAFNOSUPPORT;

	len = (size_t)addr_len - offsetof(struct sockaddr, sa_data);
	if (len > 0 && (p = memchr(addr->sa_data, '\0', len)) != NULL)
		len = (size_t)(p - addr->sa_data);

	/* The given path name must not be an empty string. */
	if (len == 0)
		return ENOENT;

	/* This check should be redundant but better safe than sorry. */
	if (len >= UDS_PATH_MAX)
		return EINVAL;

	*pathp = (const char *)addr->sa_data;
	return len;
}

/*
 * Given the socket file path given as 'path' with length 'path_len' (not
 * necessarily null terminated), store a socket address with the path in
 * 'addr', and return the socket address length in 'addr_len'.  The calling
 * libraries (libsockdriver, libsockevent) and the static assert in uds.h
 * guarantee that 'addr' is sufficiently large to store any address we generate
 * here.  The libraries may subsequently copy out only a part of it to the user
 * process.  This function always succeeds.
 */
void
uds_make_addr(const char * path, size_t len, struct sockaddr * addr,
	socklen_t * addr_len)
{

	/*
	 * Generate the address.  The stored length (sa_len/sun_len) does not
	 * include a null terminator.  The entire structure does include a null
	 * terminator, but only if the socket is bound.
	 */
	addr->sa_len = offsetof(struct sockaddr, sa_data) + len;
	addr->sa_family = AF_UNIX;
	if (len > 0) {
		/* This call may (intentionally) overrun the sa_data size. */
		memcpy((char *)addr->sa_data, path, len);
		((char *)addr->sa_data)[len] = '\0';

		/* The socket is bound, so include the null terminator. */
		len++;
		assert(len <= UDS_PATH_MAX);
	}

	/* Note that this length may be different from sa_len/sun_len now. */
	*addr_len = offsetof(struct sockaddr, sa_data) + len;
}

/*
 * Bind a socket to a local address.
 */
static int
uds_bind(struct sock * sock, const struct sockaddr * addr, socklen_t addr_len,
	endpoint_t user_endpt)
{
	struct udssock *uds = (struct udssock *)sock;
	struct udssock *uds2;
	const char *path;
	size_t len;
	dev_t dev;
	ino_t ino;
	int r;

	dprintf(("UDS: bind(%d)\n", uds_get_id(uds)));

	/* A socket may be bound at any time, but only once. */
	if (uds_is_bound(uds))
		return EINVAL;

	/* Verify that the user gave us an acceptable address. */
	if ((r = uds_check_addr(addr, addr_len, &path)) < 0)
		return r;
	len = (size_t)r;

	/* Attempt to create the socket file on the file system. */
	r = socketpath(user_endpt, path, len, SPATH_CREATE, &dev, &ino);
	if (r != OK)
		return r;
	assert(dev != NO_DEV && ino != 0);

	/*
	 * It is possible that a socket file of a previously bound socket was
	 * unlinked, and due to inode number reuse, a new socket file has now
	 * been created with the same <dev,ino> pair.  In that case, we must
	 * unbind the old socket, because it must no longer be found.  The old
	 * socket will still have a path (and behave as though it is bound) but
	 * no longer be found through hash lookups.
	 */
	if ((uds2 = udshash_get(dev, ino)) != NULL) {
		udshash_del(uds2);

		uds2->uds_dev = NO_DEV;
		uds2->uds_ino = 0;
	}

	/*
	 * Obtain credentials for the socket, unless the socket is already
	 * connecting or connected, in which case we must not replace the
	 * credentials we obtained already.  We later clear those credentials
	 * upon a connection failure or disconnect, so that if the socket is
	 * then put in listening mode, we know there are no bind-time
	 * credentials.  Not ideal, but we really need two separate sets of
	 * credentials if we want to get this right, which is a waste of memory
	 * as no sane application writer would ever rely on credential passing
	 * after recycling a socket..
	 */
	if (uds_get_type(uds) != SOCK_DGRAM && !uds_is_connecting(uds) &&
	    !uds_is_connected(uds))
		uds_get_cred(uds, user_endpt);

	/* Asssign the address to the socket. */
	uds->uds_pathlen = len;
	memcpy(&uds->uds_path, path, len);
	uds->uds_dev = dev;
	uds->uds_ino = ino;

	udshash_add(uds);

	return OK;
}

/*
 * Look up a UDS socket based on a user-given address.  If a socket exists for
 * the address, check if it is type-compatible with the given UDS socket.
 * On succes, return OK, with 'peerp' set to the socket that was found.  On
 * failure, return a negative error code.
 */
int
uds_lookup(struct udssock * uds, const struct sockaddr * addr,
	socklen_t addr_len, endpoint_t user_endpt, struct udssock ** peerp)
{
	struct udssock *peer;
	const char *path;
	size_t len;
	dev_t dev;
	ino_t ino;
	int r;

	/* Verify that the user gave us an acceptable address. */
	if ((r = uds_check_addr(addr, addr_len, &path)) < 0)
		return r;
	len = (size_t)r;

	/* Attempt to look up the socket file on the file system. */
	r = socketpath(user_endpt, path, len, SPATH_CHECK, &dev, &ino);
	if (r != OK)
		return r;
	assert(dev != NO_DEV && ino != 0);

	if ((peer = udshash_get(dev, ino)) == NULL)
		return ECONNREFUSED;
	if (uds_get_type(peer) != uds_get_type(uds))
		return EPROTOTYPE;

	*peerp = peer;
	return OK;
}

/*
 * Given the listening socket 'uds', and the socket 'link' that is calling or
 * has called connect(2) and is or will be linked to the listening socket's
 * queue, create a new socket and connect it to 'link', putting both sockets in
 * the connected state.  The given link socket may be in unconnected,
 * connecting, or disconnected state prior to the call.  Return OK or an error
 * code.  The link state of the link socket remains unchanged in any case.
 */
static int
uds_attach(struct udssock * uds, struct udssock * link)
{
	struct udssock *conn;
	int r;

	/*
	 * Allocate a new socket to use as peer socket for the connection that
	 * is about to be established.  The new socket is not yet known by
	 * libsockevent.
	 */
	if ((r = uds_alloc(&conn)) != OK)
		return r;

	/*
	 * Ask libsockevent to clone the sock object in the new UDS socket from
	 * the listening socket.  This adds the sock object to libsockevent's
	 * data structures and ensures that we can safely use the socket
	 * despite the fact that it has not yet been accepted (and thus
	 * returned to libsockevent).  From this moment on, we must either
	 * return the socket's ID (but not a pointer to it!) from uds_accept()
	 * or raise SEV_CLOSE on it.
	 */
	sockevent_clone(&uds->uds_sock, &conn->uds_sock, uds_get_id(conn));

	/* Connect the link socket to the new socket. */
	link->uds_conn = conn;
	link->uds_flags |= UDSF_CONNECTED;

	/*
	 * Connect the new socket to the link socket as well.  The child
	 * socket should also inherit pretty much all settings from the
	 * listening socket, including the bind path and the listening socket's
	 * bind-time credentials.
	 */
	conn->uds_conn = link;
	conn->uds_flags = uds->uds_flags & (UDSF_PASSCRED | UDSF_CONNWAIT);
	conn->uds_flags |= UDSF_CONNECTED;
	conn->uds_pathlen = uds->uds_pathlen;
	memcpy(conn->uds_path, uds->uds_path, (size_t)uds->uds_pathlen);
	memcpy(&conn->uds_cred, &uds->uds_cred, sizeof(conn->uds_cred));

	return OK;
}

/*
 * Connect a socket to a remote address.
 */
static int
uds_connect(struct sock * sock, const struct sockaddr * addr,
	socklen_t addr_len, endpoint_t user_endpt)
{
	struct udssock *uds = (struct udssock *)sock;
	struct udssock *link;
	int r;

	dprintf(("UDS: connect(%d)\n", uds_get_id(uds)));

	/* For connection-oriented sockets, several state checks apply. */
	if (uds_get_type(uds) != SOCK_DGRAM) {
		if (uds_is_listening(uds))
			return EOPNOTSUPP;
		if (uds_is_connecting(uds))
			return EALREADY;
		if (uds_is_connected(uds))
			return EISCONN;
		/* Disconnected sockets may be reconnected, see below. */
	} else {
		/*
		 * Connectionless sockets may be unconnected by providing an
		 * address with family AF_UNSPEC.  Handle this case first here.
		 */
		if (addr_len >= offsetof(struct sockaddr, sa_data) &&
		    addr->sa_family == AF_UNSPEC) {
			/*
			 * Reset this socket's previous connection to another
			 * socket, if any.  Unconnecting has no effect on other
			 * sockets connected to this socket, though.
			 */
			if (uds_has_link(uds))
				uds_del_queue(uds->uds_link, uds);

			return OK;
		}
	}

	/*
	 * Find the socket identified by the given address.  If it exists at
	 * all, see if it is a proper match.
	 */
	if ((r = uds_lookup(uds, addr, addr_len, user_endpt, &link)) != OK)
		return r;

	/*
	 * Handle connectionless sockets first, in which case a connect links
	 * the socket to a send target and limits receipt to datagrams from
	 * that target.  We actually point the socket to the peer socket,
	 * through uds_link.  That also means that if the target socket
	 * disappears, we have to reset any sockets connected to it, in which
	 * case we return them to the unconnected state.  In order to allow
	 * finding all sockets connected to a particular socket, we put all
	 * those sockets on their target's queue, hence why we use uds_link and
	 * not uds_conn.  As mentioned before, we allow reconnecting without
	 * restrictions.
	 * TODO: see if reconnecting should clear a pending ECONNRESET.
	 *
	 * An important note: 'uds' and 'link' may actually be the same socket,
	 * if the caller chooses to connect a socket with itself!
	 */
	if (uds_get_type(uds) == SOCK_DGRAM) {
		/* Reconnecting to the same socket has no effect. */
		if (uds_has_link(uds) && uds->uds_link == link)
			return OK;

		/*
		 * If the intended target is linked to another socket, we
		 * refuse linking to it.  Sending or receiving would never work
		 * anyway.  Do allow a socket to link to itself after being
		 * linked to another socket.  The error code is the same as in
		 * the sending code, borrowed from Linux.
		 */
		if (uds != link && uds_has_link(link) && link->uds_link != uds)
			return EPERM;

		/*
		 * Reset this socket's previous link to another socket, if any.
		 */
		if (uds_has_link(uds))
			uds_del_queue(uds->uds_link, uds);

		/*
		 * Reset any links to this socket, except for the one by
		 * the intended target.  Sending or receiving would no longer
		 * work anyway.  If the socket was linked to itself, clear its
		 * self-link without generating an ECONNRESET.  If the socket
		 * is relinking to itself, reestablish the link after first
		 * clearing it.
		 */
		uds_clear_queue(uds, (uds != link) ? link : NULL);

		uds_add_queue(link, uds);

		return OK;
	}

	/*
	 * For connection-oriented sockets there is more to do.  First, make
	 * sure that the peer is a listening socket, that it has not been shut
	 * down, and that its backlog is not already at the configured maximum.
	 */
	if (!uds_is_listening(link))
		return ECONNREFUSED;

	if (uds_is_shutdown(link, SFL_SHUT_RD | SFL_SHUT_WR))
		return ECONNREFUSED;

	if (link->uds_queued >= link->uds_backlog)
		return ECONNREFUSED;

	/*
	 * The behavior of connect(2) now depends on whether LOCAL_CONNWAIT is
	 * set on either the connecting or the listening socket.  If it is not,
	 * the socket will be connected to a new as-yet invisible socket, which
	 * will be the one returned from accept(2) later.  If it was, the
	 * socket will be put in the connecting state.
	 */
	if (!((uds->uds_flags | link->uds_flags) & UDSF_CONNWAIT)) {
		if ((r = uds_attach(link, uds)) != OK)
			return r;

		assert(uds_is_connected(uds));
	} else {
		/*
		 * Disconnected sockets now stop being connected.  Any pending
		 * data can still be received, though.
		 */
		uds->uds_flags &= ~UDSF_CONNECTED;

		r = SUSPEND;
	}

	/* Obtain credentials for the socket. */
	uds_get_cred(uds, user_endpt);

	/* Add the socket at the end of the listening socket's queue. */
	uds_add_queue(link, uds);

	assert(r != SUSPEND || uds_is_connecting(uds));

	/*
	 * Let an accept call handle the rest, which will in turn resume this
	 * connect call.  The sockevent library ensures that this works even if
	 * the call is non-blocking.
	 */
	sockevent_raise(&link->uds_sock, SEV_ACCEPT);

	return r;
}

/*
 * Put a socket in listening mode.
 */
static int
uds_listen(struct sock * sock, int backlog)
{
	struct udssock *uds = (struct udssock *)sock;

	/* The maximum backlog value must not exceed its field size. */
	assert(SOMAXCONN <= USHRT_MAX);

	dprintf(("UDS: listen(%d)\n", uds_get_id(uds)));

	/* Only connection-oriented types may be put in listening mode. */
	if (uds_get_type(uds) == SOCK_DGRAM)
		return EOPNOTSUPP;

	/* A connecting or connected socket may not listen. */
	if (uds_is_connecting(uds) || uds_is_connected(uds))
		return EINVAL;

	/* POSIX says that this is now the appropriate error code here. */
	if (!uds_is_bound(uds))
		return EDESTADDRREQ;

	/*
	 * The socket is now entering the listening state.  If it was
	 * previously disconnected, clear the connection flag.
	 */
	uds->uds_flags &= ~UDSF_CONNECTED;

	/*
	 * We do not remove sockets from the backlog if it is now being dropped
	 * below the current number of queued sockets.  We only refuse newly
	 * connecting sockets beyond the backlog size.
	 */
	uds->uds_backlog = backlog;

	return OK;
}

/*
 * Test whether an accept request would block.  Return OK if a socket could be
 * accepted, an appropriate error code if an accept call would fail instantly,
 * or SUSPEND if the accept request would block waiting for a connection.
 */
static int
uds_test_accept(struct sock * sock)
{
	struct udssock *uds = (struct udssock *)sock;

	/*
	 * Ensure that the socket is in listening mode.  If not, we must return
	 * the error code that is appropriate for this socket type.
	 */
	if (uds_get_type(uds) == SOCK_DGRAM)
		return EOPNOTSUPP;
	if (!uds_is_listening(uds))
		return EINVAL;

	/*
	 * If the socket has been shut down, new connections are no longer
	 * accepted and accept calls no longer block.  This is not a POSIX
	 * requirement, but rather an application convenience feature.
	 */
	if (uds->uds_queued == 0) {
		if (uds_is_shutdown(uds, SFL_SHUT_RD | SFL_SHUT_WR))
			return ECONNABORTED;

		return SUSPEND;
	}

	return OK;
}

/*
 * Accept a connection on a listening socket, creating a new socket.  On
 * success, return the new socket identifier, with the new socket stored in
 * 'newsockp'.  Otherwise, return an error code.
 */
static sockid_t
uds_accept(struct sock * sock, struct sockaddr * addr, socklen_t * addr_len,
	endpoint_t user_endpt __unused, struct sock ** newsockp)
{
	struct udssock *uds = (struct udssock *)sock;
	struct udssock *link, *conn;
	sockid_t r;

	dprintf(("UDS: accept(%d)\n", uds_get_id(uds)));

	if ((r = uds_test_accept(sock)) != OK)
		return r;

	/*
	 * Take the first connecting socket off the listening queue.
	 */
	assert(!TAILQ_EMPTY(&uds->uds_queue));

	link = TAILQ_FIRST(&uds->uds_queue);

	/*
	 * Depending on the LOCAL_CONNWAIT setting at the time of connect(2),
	 * the socket may be connecting or connected.  In the latter case, its
	 * attached socket is the socket we will return now.  Otherwise we have
	 * to attach a socket first.
	 */
	assert(uds_is_connecting(link) || uds_is_connected(link));

	if (uds_is_connecting(link)) {
		/*
		 * Attach a new socket.  If this fails, return the error but
		 * leave the connecting socket on the listening queue.
		 */
		if ((r = uds_attach(uds, link)) != OK)
			return r;

		assert(uds_is_connected(link));

		/*
		 * Wake up blocked (connect, send, select) calls on the peer
		 * socket.
		 */
		sockevent_raise(&link->uds_sock, SEV_CONNECT);
	}

	uds_del_queue(uds, link);

	/* Return the peer socket's address to the caller. */
	uds_make_addr(link->uds_path, link->uds_pathlen, addr, addr_len);

	conn = link->uds_conn;

	dprintf(("UDS: accept returns %d\n", uds_get_id(conn)));

	/*
	 * We already cloned the sock object, so return its ID but not a
	 * pointer to it.  That tells libsockevent not to reinitialize it.
	 */
	*newsockp = NULL;
	return uds_get_id(conn);
}

/*
 * Set socket options.
 */
static int
uds_setsockopt(struct sock * sock, int level, int name,
	const struct sockdriver_data * data, socklen_t len)
{
	struct udssock *uds = (struct udssock *)sock;
	int r, val;

	dprintf(("UDS: setsockopt(%d,%d,%d)\n", uds_get_id(uds), level, name));

	switch (level) {
	case SOL_SOCKET:
		switch (name) {
		case SO_SNDBUF:
		case SO_RCVBUF:
			/*
			 * The send buffer size may not be changed because the
			 * buffer is the same as the other side's receive
			 * buffer, and what the other side is may vary from
			 * send call to send call.  Changing the receive buffer
			 * size would disallow us from even accurately guessing
			 * the send buffer size in getsockopt calls.  Therefore
			 * both are hardcoded and cannot actually be changed.
			 * In order to support applications that want at least
			 * a certain minimum, we do accept requests to shrink
			 * either buffer, but we ignore the given size.
			 */
			if ((r = sockdriver_copyin_opt(data, &val, sizeof(val),
			    len)) != OK)
				return r;

			if (val <= 0 || (size_t)val > uds_io_buflen())
				return EINVAL;

			return OK; /* ignore new value */
		}

		break;

	case UDSPROTO_UDS:
		switch (name) {
		case LOCAL_CREDS:
			if ((r = sockdriver_copyin_opt(data, &val, sizeof(val),
			    len)) != OK)
				return r;

			if (val)
				uds->uds_flags |= UDSF_PASSCRED;
			else
				uds->uds_flags &= ~UDSF_PASSCRED;

			/*
			 * In incredibly rare cases, disabling this flag may
			 * allow blocked sends to be resumed, because suddenly
			 * no room for the credentials is needed in the receive
			 * buffer anymore.
			 */
			if (!val)
				sockevent_raise(&uds->uds_sock, SEV_SEND);

			return OK;

		case LOCAL_CONNWAIT:
			if ((r = sockdriver_copyin_opt(data, &val, sizeof(val),
			    len)) != OK)
				return r;

			if (val)
				uds->uds_flags |= UDSF_CONNWAIT;
			else
				uds->uds_flags &= ~UDSF_CONNWAIT;

			/*
			 * Changing the setting does not affect sockets that
			 * are currently pending to be accepted.  Therefore,
			 * uds_accept() may have to deal with either case on a
			 * socket-by-socket basis.
			 */
			return OK;

		case LOCAL_PEEREID:
			/* This option may be retrieved but not set. */
			return ENOPROTOOPT;
		}

		break;
	}

	return ENOPROTOOPT;
}

/*
 * Retrieve socket options.
 */
static int
uds_getsockopt(struct sock * sock, int level, int name,
	const struct sockdriver_data * data, socklen_t * len)
{
	struct udssock *uds = (struct udssock *)sock;
	int val;

	dprintf(("UDS: getsockopt(%d,%d,%d)\n", uds_get_id(uds), level, name));

	switch (level) {
	case SOL_SOCKET:
		switch (name) {
		case SO_SNDBUF:
		case SO_RCVBUF:
			/* See uds_setsockopt() for why this is static. */
			val = (int)uds_io_buflen();

			return sockdriver_copyout_opt(data, &val, sizeof(val),
			    len);
		}

		break;

	case UDSPROTO_UDS:
		switch (name) {
		case LOCAL_CREDS:
			val = !!(uds->uds_flags & UDSF_PASSCRED);

			return sockdriver_copyout_opt(data, &val, sizeof(val),
			    len);

		case LOCAL_CONNWAIT:
			val = !!(uds->uds_flags & UDSF_CONNWAIT);

			return sockdriver_copyout_opt(data, &val, sizeof(val),
			    len);

		case LOCAL_PEEREID:
			/* getpeereid(3) documents these error codes. */
			if (uds_get_type(uds) == SOCK_DGRAM)
				return EINVAL;
			if (!uds_is_connected(uds))
				return ENOTCONN;

			/*
			 * This is a custom MINIX3 error, indicating that there
			 * are no credentials to return.  This could be due to
			 * a failure to obtain them (which *should* not happen)
			 * but also if the socket was bound while connected,
			 * disconnected, and then reused as listening socket.
			 */
			if (uds->uds_conn->uds_cred.unp_pid == -1)
				return EINVAL;

			return sockdriver_copyout_opt(data,
			    &uds->uds_conn->uds_cred,
			    sizeof(uds->uds_conn->uds_cred), len);
		}

		break;
	}

	return ENOPROTOOPT;
}

/*
 * Retrieve a socket's local address.
 */
static int
uds_getsockname(struct sock * sock, struct sockaddr * addr,
	socklen_t * addr_len)
{
	struct udssock *uds = (struct udssock *)sock;

	dprintf(("UDS: getsockname(%d)\n", uds_get_id(uds)));

	uds_make_addr(uds->uds_path, uds->uds_pathlen, addr, addr_len);

	return OK;
}

/*
 * Retrieve a socket's remote address.
 */
static int
uds_getpeername(struct sock * sock, struct sockaddr * addr,
	socklen_t * addr_len)
{
	struct udssock *uds = (struct udssock *)sock;
	struct udssock *peer;

	dprintf(("UDS: getpeername(%d)\n", uds_get_id(uds)));

	/*
	 * For disconnected sockets, we no longer have a peer socket and thus
	 * also no peer address.  Too bad, but NetBSD does the same.
	 *
	 * For connecting sockets we could in fact return a peer address, but
	 * POSIX says (and other platforms agree) that we should deny the call.
	 */
	peer = uds_get_peer(uds);

	if (peer == NULL || uds_is_connecting(uds))
		return ENOTCONN;

	uds_make_addr(peer->uds_path, peer->uds_pathlen, addr, addr_len);

	return OK;
}

/*
 * Shut down socket send and receive operations.  Note that 'flags' is a
 * bitwise mask with libsockevent's SFL_SHUT_{RD,WR} flags rather than the set
 * of SHUT_{RD,WR,RDWR} values from userland.
 */
static int
uds_shutdown(struct sock * sock, unsigned int flags)
{
	struct udssock *uds = (struct udssock *)sock;
	struct udssock *conn;
	unsigned int mask;

	dprintf(("UDS: shutdown(%d,0x%x)\n", uds_get_id(uds), flags));

	/*
	 * If we are shutting down the socket for reading, we can already close
	 * any in-flight file descriptors associated with this socket.
	 */
	if (flags & SFL_SHUT_RD)
		uds_io_reset(uds);

	/*
	 * A shutdown on this side of a connection may have an effect on
	 * ongoing operations on the other side.  Fire appropriate events.
	 */
	if (uds_is_connected(uds)) {
		assert(uds_get_type(uds) != SOCK_DGRAM);

		conn = uds->uds_conn;

		mask = 0;
		if (flags & SFL_SHUT_RD)
			mask |= SEV_SEND;
		if (flags & SFL_SHUT_WR)
			mask |= SEV_RECV;

		sockevent_raise(&conn->uds_sock, mask);
	}

	return OK;
}

/*
 * Close a socket.
 *
 * The 'force' flag is unused because we need never wait for data to be sent,
 * since we keep all in-flight data on the receiver side.
 */
static int
uds_close(struct sock * sock, int force __unused)
{
	struct udssock *uds = (struct udssock *)sock;

	dprintf(("UDS: close(%d)\n", uds_get_id(uds)));

	if (uds_get_type(uds) == SOCK_DGRAM) {
		/* If this socket is linked to a target, disconnect it. */
		if (uds_has_link(uds))
			uds_del_queue(uds->uds_link, uds);

		/* Reset all sockets linked to this socket as a target. */
		uds_clear_queue(uds, NULL);
	} else if (uds_is_listening(uds)) {
		/*
		 * Abort all connecting sockets queued on this socket, and
		 * break all connections for connected sockets queued on this
		 * socket, freeing their peers.
		 */
		uds_clear_queue(uds, NULL);
	} else if (uds_has_link(uds)) {
		/*
		 * This socket is connecting or connected while the other side
		 * has not been accepted yet.  Remove the socket from the
		 * listening socket's queue, and if it was connected, get rid
		 * of its peer socket altogether.
		 */
		assert(uds_is_listening(uds->uds_link));

		uds_del_queue(uds->uds_link, uds);

		if (uds_is_connected(uds))
			uds_disconnect(uds, TRUE /*was_linked*/);
	} else if (uds_is_connected(uds)) {
		/*
		 * Decouple the peer socket from this socket, and possibly wake
		 * up any pending operations on it.  The socket remains marked
		 * as connected, but will now be disconnected.
		 */
		uds_disconnect(uds, FALSE /*was_linked*/);
	}

	if (uds_is_hashed(uds))
		udshash_del(uds);

	return OK;
}

static const struct sockevent_ops uds_ops = {
	.sop_pair		= uds_pair,
	.sop_bind		= uds_bind,
	.sop_connect		= uds_connect,
	.sop_listen		= uds_listen,
	.sop_accept		= uds_accept,
	.sop_test_accept	= uds_test_accept,
	.sop_pre_send		= uds_pre_send,
	.sop_send		= uds_send,
	.sop_test_send		= uds_test_send,
	.sop_pre_recv		= uds_pre_recv,
	.sop_recv		= uds_recv,
	.sop_test_recv		= uds_test_recv,
	.sop_setsockopt		= uds_setsockopt,
	.sop_getsockopt		= uds_getsockopt,
	.sop_getsockname	= uds_getsockname,
	.sop_getpeername	= uds_getpeername,
	.sop_shutdown		= uds_shutdown,
	.sop_close		= uds_close,
	.sop_free		= uds_free
};

/*
 * Initialize the service.
 */
static int
uds_init(int type __unused, sef_init_info_t * info __unused)
{
	unsigned int i;

	/* Initialize the list of free sockets. */
	TAILQ_INIT(&uds_freelist);

	for (i = 0; i < __arraycount(uds_array); i++) {
		uds_array[i].uds_flags = 0;

		TAILQ_INSERT_TAIL(&uds_freelist, &uds_array[i], uds_next);
	}

	/* Initialize the file-to-socket hash table. */
	udshash_init();

	/* Initialize the input/output module. */
	uds_io_init();

	/* Initialize the status module. */
	uds_stat_init();

	/* Initialize the sockevent library. */
	sockevent_init(uds_socket);

	uds_in_use = 0;
	uds_running = TRUE;

	return OK;
}

/*
 * Clean up before shutdown.
 */
static void
uds_cleanup(void)
{

	/* Tell the status module to clean up. */
	uds_stat_cleanup();
}

/*
 * The service has received a signal.
 */
static void
uds_signal(int signo)
{

	/* Only check for the termination signal.  Ignore anything else. */
	if (signo != SIGTERM)
		return;

	/* Exit only once all sockets have been closed. */
	uds_running = FALSE;

	if (uds_in_use == 0)
		sef_cancel();
}

/*
 * Perform initialization using the System Event Framework (SEF).
 */
static void
uds_startup(void)
{

	/* Register initialization callbacks. */
	sef_setcb_init_fresh(uds_init);

	/* Register signal callback. */
	sef_setcb_signal_handler(uds_signal);

	/* Let SEF perform startup. */
	sef_startup();
}

/*
 * The UNIX Domain Sockets driver.
 */
int
main(void)
{
	message m;
	int r, ipc_status;

	/* Initialize the service. */
	uds_startup();

	/* Loop receiving and processing messages until instructed to stop. */
	while (uds_running || uds_in_use > 0) {
		if ((r = sef_receive_status(ANY, &m, &ipc_status)) != OK) {
			if (r == EINTR)
				continue;	/* sef_cancel() was called */

			panic("UDS: sef_receive_status failed: %d", r);
		}

		/*
		 * Messages from the MIB service are (ultimately) for the
		 * status module.  Everything else is assumed to be a socket
		 * request and passed to libsockevent, which will ignore
		 * anything it does not recognize.
		 */
		if (m.m_source == MIB_PROC_NR)
			rmib_process(&m, ipc_status);
		else
			sockevent_process(&m, ipc_status);
	}

	/* Clean up before graceful shutdown. */
	uds_cleanup();

	return EXIT_SUCCESS;
}
