/*
 * This file implements the upper socket layer of VFS: the BSD socket system
 * calls, and any associated file descriptor, file pointer, vnode, and file
 * system processing.  In most cases, this layer will call into the lower
 * socket layer in order to send the request to a socket driver.  Generic file
 * calls (e.g., read, write, ioctl, and select) are not implemented here, and
 * will directly call into the lower socket layer as well.
 *
 * The following table shows the system call numbers implemented in this file,
 * along with their request and reply message types.  Each request layout
 * message type is prefixed with "m_lc_vfs_".  Each reply layout message type
 * is prefixed with "m_vfs_lc_".  For requests without a specific reply layout,
 * only the "m_type" message field is used in the reply message.
 *
 * Type			Request layout		Reply layout
 * ----			--------------		------------
 * VFS_SOCKET		socket
 * VFS_SOCKETPAIR	socket			fdpair
 * VFS_BIND		sockaddr
 * VFS_CONNECT		sockaddr
 * VFS_LISTEN		listen
 * VFS_ACCEPT		sockaddr		socklen
 * VFS_SENDTO		sendrecv
 * VFS_RECVFROM		sendrecv		socklen
 * VFS_SENDMSG		sockmsg
 * VFS_RECVMSG		sockmsg
 * VFS_SETSOCKOPT	sockopt
 * VFS_GETSOCKOPT	sockopt			socklen
 * VFS_GETSOCKNAME	sockaddr		socklen
 * VFS_GETPEERNAME	sockaddr		socklen
 * VFS_SHUTDOWN		shutdown
 */

#include "fs.h"
#include "vnode.h"
#include "file.h"

#include <sys/socket.h>

/*
 * Convert any SOCK_xx open flags to O_xx open flags.
 */
static int
get_sock_flags(int type)
{
	int flags;

	flags = 0;
	if (type & SOCK_CLOEXEC)
		flags |= O_CLOEXEC;
	if (type & SOCK_NONBLOCK)
		flags |= O_NONBLOCK;
	if (type & SOCK_NOSIGPIPE)
		flags |= O_NOSIGPIPE;

	return flags;
}

/*
 * Perform cheap pre-call checks to ensure that the given number of socket FDs
 * can be created for the current process.
 */
static int
check_sock_fds(int nfds)
{

	/*
	 * For now, we simply check if there are enough file descriptor slots
	 * free in the process.  Since the process is blocked on a socket call,
	 * this aspect will not change.  Availability of file pointers, vnodes,
	 * and PFS nodes may vary, and is therefore less interesting to check
	 * here - it will have to be checked again upon completion anyway.
	 */
	return check_fds(fp, nfds);
}

/*
 * Create a new file descriptor, including supporting objects, for the open
 * socket identified by 'dev', in the current process, using the O_xx open
 * flags 'flags'.  On success, return the file descriptor number.  The results
 * of a successful call can be undone with close_fd(), which will also close
 * the socket itself.  On failure, return a negative error code.  In this case,
 * the socket will be left open.
 */
static int
make_sock_fd(dev_t dev, int flags)
{
	struct vmnt *vmp;
	struct vnode *vp;
	struct filp *filp;
	struct node_details res;
	int r, fd;

	assert((flags & ~(O_CLOEXEC | O_NONBLOCK | O_NOSIGPIPE)) == 0);

#if !NDEBUG
	/*
	 * Check whether there is a socket object for the new device already.
	 * This is an expensive check, but if the socket driver sends us a new
	 * socket ID that is already in use, this is a sure sign of driver
	 * misbehavior.  So far it does seem like nothing would go wrong within
	 * VFS in this case though, which is why this is a debug-only check.
	 */
	if (find_filp_by_sock_dev(dev) != NULL) {
		printf("VFS: socket driver %d generated in-use socket ID!\n",
		    get_smap_by_dev(dev, NULL)->smap_endpt);
		return EIO;
	}
#endif /* !NDEBUG */

	/*
	 * Get a lock on PFS.  TODO: it is not clear whether locking PFS is
	 * needed at all, let alone which lock: map_vnode() uses a write lock,
	 * create_pipe() uses a read lock, and cdev_clone() uses no lock at
	 * all.  As is, the README prescribes VMNT_READ, so that's what we use
	 * here.  The code below largely copies the create_pipe() code anyway.
	 */
	if ((vmp = find_vmnt(PFS_PROC_NR)) == NULL)
		panic("PFS gone");
	if ((r = lock_vmnt(vmp, VMNT_READ)) != OK)
		return r;

	/* Obtain a free vnode. */
	if ((vp = get_free_vnode()) == NULL) {
		unlock_vmnt(vmp);
		return err_code;
	}
	lock_vnode(vp, VNODE_OPCL);

	/* Acquire a file descriptor. */
	if ((r = get_fd(fp, 0, R_BIT | W_BIT, &fd, &filp)) != OK) {
		unlock_vnode(vp);
		unlock_vmnt(vmp);
		return r;
	}

	/* Create a PFS node for the socket. */
	if ((r = req_newnode(PFS_PROC_NR, fp->fp_effuid, fp->fp_effgid,
	    S_IFSOCK | ACCESSPERMS, dev, &res)) != OK) {
		unlock_filp(filp);
		unlock_vnode(vp);
		unlock_vmnt(vmp);
		return r;
	}

	/* Fill in the objects, and link them together. */
	vp->v_fs_e = res.fs_e;
	vp->v_inode_nr = res.inode_nr;
	vp->v_mode = res.fmode;
	vp->v_sdev = dev;
	vp->v_fs_count = 1;
	vp->v_ref_count = 1;
	vp->v_vmnt = NULL;
	vp->v_dev = NO_DEV;
	vp->v_size = 0;

	filp->filp_vno = vp;
	filp->filp_flags = O_RDWR | flags;
	filp->filp_count = 1;

	fp->fp_filp[fd] = filp;
	if (flags & O_CLOEXEC)
		FD_SET(fd, &fp->fp_cloexec_set);

	/* Release locks, and return the new file descriptor. */
	unlock_filp(filp); /* this also unlocks the vnode now! */
	unlock_vmnt(vmp);

	return fd;
}

/*
 * Create a socket.
 */
int
do_socket(void)
{
	int domain, type, sock_type, protocol;
	dev_t dev;
	int r, flags;

	domain = job_m_in.m_lc_vfs_socket.domain;
	type = job_m_in.m_lc_vfs_socket.type;
	protocol = job_m_in.m_lc_vfs_socket.protocol;

	/* Is there a socket driver for this domain at all? */
	if (get_smap_by_domain(domain) == NULL)
		return EAFNOSUPPORT;

	/*
	 * Ensure that it is at least likely that after creating a socket, we
	 * will be able to create a file descriptor for it, along with all the
	 * necessary supporting objects.  While it would be slightly neater to
	 * allocate these objects before trying to create the socket, this is
	 * offset by the fact that that approach results in a downright mess in
	 * do_socketpair() below, and with the current approach we can reuse
	 * the same code for accepting sockets as well.  For newly created
	 * sockets, it is no big deal to close them right after creation; for
	 * newly accepted sockets, we have no choice but to do that anyway.
	 * Moreover, object creation failures should be rare and our approach
	 * does not cause significantly more overhead anyway, so the entire
	 * issue is largely philosophical anyway.  For now, this will do.
	 */
	if ((r = check_sock_fds(1)) != OK)
		return r;

	sock_type = type & ~SOCK_FLAGS_MASK;
	flags = get_sock_flags(type);

	if ((r = sdev_socket(domain, sock_type, protocol, &dev,
	    FALSE /*pair*/)) != OK)
		return r;

	if ((r = make_sock_fd(dev, flags)) < 0)
		(void)sdev_close(dev, FALSE /*may_suspend*/);

	return r;
}

/*
 * Create a pair of connected sockets.
 */
int
do_socketpair(void)
{
	int domain, type, sock_type, protocol;
	dev_t dev[2];
	int r, fd0, fd1, flags;

	domain = job_m_in.m_lc_vfs_socket.domain;
	type = job_m_in.m_lc_vfs_socket.type;
	protocol = job_m_in.m_lc_vfs_socket.protocol;

	/* Is there a socket driver for this domain at all? */
	if (get_smap_by_domain(domain) == NULL)
		return EAFNOSUPPORT;

	/*
	 * See the lengthy comment in do_socket().  This time we need two of
	 * everything, though.
	 */
	if ((r = check_sock_fds(2)) != OK)
		return r;

	sock_type = type & ~SOCK_FLAGS_MASK;
	flags = get_sock_flags(type);

	if ((r = sdev_socket(domain, sock_type, protocol, dev,
	    TRUE /*pair*/)) != OK)
		return r;

	if ((fd0 = make_sock_fd(dev[0], flags)) < 0) {
		(void)sdev_close(dev[0], FALSE /*may_suspend*/);
		(void)sdev_close(dev[1], FALSE /*may_suspend*/);
		return fd0;
	}

	if ((fd1 = make_sock_fd(dev[1], flags)) < 0) {
		close_fd(fp, fd0, FALSE /*may_suspend*/);
		(void)sdev_close(dev[1], FALSE /*may_suspend*/);
		return fd1;
	}

	job_m_out.m_vfs_lc_fdpair.fd0 = fd0;
	job_m_out.m_vfs_lc_fdpair.fd1 = fd1;
	return OK;
}

/*
 * Check whether the given file descriptor identifies an open socket in the
 * current process.  If so, return OK, with the socket device number stored in
 * 'dev' and its file pointer flags stored in 'flags' (if not NULL).  If not,
 * return an appropriate error code.
 */
static int
get_sock(int fd, dev_t * dev, int * flags)
{
	struct filp *filp;

	if ((filp = get_filp(fd, VNODE_READ)) == NULL)
		return err_code;

	if (!S_ISSOCK(filp->filp_vno->v_mode)) {
		unlock_filp(filp);
		return ENOTSOCK;
	}

	*dev = filp->filp_vno->v_sdev;
	if (flags != NULL)
		*flags = filp->filp_flags;

	/*
	 * It is safe to leave the file pointer object unlocked during the
	 * actual call.  Since the current process is blocked for the duration
	 * of the socket call, we know the socket's file descriptor, and thus
	 * its file pointer, can not possibly be freed.  In addition, we will
	 * not be accessing the file pointer anymore later, with the exception
	 * of accept calls, which reacquire the lock when the reply comes in.
	 */
	unlock_filp(filp);
	return OK;
}

/*
 * Bind a socket to a local address.
 */
int
do_bind(void)
{
	dev_t dev;
	int r, fd, flags;

	fd = job_m_in.m_lc_vfs_sockaddr.fd;

	if ((r = get_sock(fd, &dev, &flags)) != OK)
		return r;

	return sdev_bind(dev, job_m_in.m_lc_vfs_sockaddr.addr,
	    job_m_in.m_lc_vfs_sockaddr.addr_len, flags);
}

/*
 * Connect a socket to a remote address.
 */
int
do_connect(void)
{
	dev_t dev;
	int r, fd, flags;

	fd = job_m_in.m_lc_vfs_sockaddr.fd;

	if ((r = get_sock(fd, &dev, &flags)) != OK)
		return r;

	return sdev_connect(dev, job_m_in.m_lc_vfs_sockaddr.addr,
	    job_m_in.m_lc_vfs_sockaddr.addr_len, flags);
}

/*
 * Put a socket in listening mode.
 */
int
do_listen(void)
{
	dev_t dev;
	int r, fd, backlog;

	fd = job_m_in.m_lc_vfs_listen.fd;
	backlog = job_m_in.m_lc_vfs_listen.backlog;

	if ((r = get_sock(fd, &dev, NULL)) != OK)
		return r;

	if (backlog < 0)
		backlog = 0;

	return sdev_listen(dev, backlog);
}

/*
 * Accept a connection on a listening socket, creating a new socket.
 */
int
do_accept(void)
{
	dev_t dev;
	int r, fd, flags;

	fd = job_m_in.m_lc_vfs_sockaddr.fd;

	if ((r = get_sock(fd, &dev, &flags)) != OK)
		return r;

	if ((r = check_sock_fds(1)) != OK)
		return r;

	return sdev_accept(dev, job_m_in.m_lc_vfs_sockaddr.addr,
	    job_m_in.m_lc_vfs_sockaddr.addr_len, flags, fd);
}

/*
 * Resume a previously suspended accept(2) system call.  This routine must
 * cover three distinct cases, depending on the 'status' and 'dev' values:
 *
 * #1. If the 'status' parameter is set to OK, the accept call succeeded.  In
 *     that case, the function is guaranteed to be called from a worker thread,
 *     with 'fp' set to the user process that made the system call.  In that
 *     case, this function may block its calling thread.  The 'dev' parameter
 *     will contain the device number of the newly accepted socket.
 * #2. If the 'status' parameter contains a negative error code, but 'dev' is
 *     *not* set to NO_DEV, then the same as above applies, except that the new
 *     socket must be closed immediately.
 * #3. If 'status' is a negative error code and 'dev' is set to NO_DEV, then
 *     the accept call has failed and no new socket was ever created.  In this
 *     case, the function MUST NOT block its calling thread.
 */
void
resume_accept(struct fproc * rfp, int status, dev_t dev, unsigned int addr_len,
	int listen_fd)
{
	message m;
	dev_t ldev;
	int r, flags;

	/*
	 * If the call did not succeed and no socket was created (case #3), we
	 * cannot and should not do more than send the error to the user
	 * process.
	 */
	if (status != OK && dev == NO_DEV) {
		replycode(rfp->fp_endpoint, status);

		return;
	}

	/*
	 * The call succeeded.  The lower socket layer (sdev.c) ensures that in
	 * that case, we are called from a worker thread which is associated
	 * with the original user process.  Thus, we can block the current
	 * thread.  Start by verifying that the listening socket is still
	 * around.  If it is not, it must have been invalidated as a result of
	 * a socket driver death, in which case we must report an error but
	 * need not close the new socket.  As a side effect, obtain the
	 * listening socket's flags, which on BSD systems are inherited by the
	 * accepted socket.
	 */
	assert(fp == rfp); /* needed for get_sock() and make_sock_fd() */

	if (get_sock(listen_fd, &ldev, &flags) != OK) {
		replycode(rfp->fp_endpoint, EIO);

		return;
	}

	/* The same socket driver must host both sockets, obviously. */
	assert(get_smap_by_dev(ldev, NULL) == get_smap_by_dev(dev, NULL));

	/*
	 * If an error status was returned (case #2), we must now close the
	 * newly accepted socket.  Effectively, this allows socket drivers to
	 * handle address copy failures in the cleanest possible way.
	 */
	if (status != OK) {
		(void)sdev_close(dev, FALSE /*may_suspend*/);

		replycode(rfp->fp_endpoint, status);

		return;
	}

	/*
	 * A new socket has been successfully accepted (case #1).  Try to
	 * create a file descriptor for the new socket.  If this fails, we have
	 * to close the new socket after all.  That is not great, but we have
	 * no way to prevent this except by preallocating all objects for the
	 * duration of the accept call, which is not exactly great either.
	 */
	flags &= O_CLOEXEC | O_NONBLOCK | O_NOSIGPIPE;

	if ((r = make_sock_fd(dev, flags)) < 0) {
		(void)sdev_close(dev, FALSE /*may_suspend*/);

		replycode(rfp->fp_endpoint, r);

		return;
	}

	/*
	 * The accept call has succeeded.  Send a reply message with the new
	 * file descriptor and an address length (which may be zero).
	 */
	memset(&m, 0, sizeof(m));
	m.m_vfs_lc_socklen.len = addr_len;

	reply(&m, rfp->fp_endpoint, r);
}

/*
 * Send a message on a socket.
 */
int
do_sendto(void)
{
	dev_t dev;
	int r, fd, flags;

	fd = job_m_in.m_lc_vfs_sendrecv.fd;

	if ((r = get_sock(fd, &dev, &flags)) != OK)
		return r;

	return sdev_readwrite(dev, job_m_in.m_lc_vfs_sendrecv.buf,
	    job_m_in.m_lc_vfs_sendrecv.len, 0, 0,
	    job_m_in.m_lc_vfs_sendrecv.addr,
	    job_m_in.m_lc_vfs_sendrecv.addr_len,
	    job_m_in.m_lc_vfs_sendrecv.flags, WRITING, flags, 0);
}

/*
 * Receive a message from a socket.
 */
int
do_recvfrom(void)
{
	dev_t dev;
	int r, fd, flags;

	fd = job_m_in.m_lc_vfs_sendrecv.fd;

	if ((r = get_sock(fd, &dev, &flags)) != OK)
		return r;

	return sdev_readwrite(dev, job_m_in.m_lc_vfs_sendrecv.buf,
	    job_m_in.m_lc_vfs_sendrecv.len, 0, 0,
	    job_m_in.m_lc_vfs_sendrecv.addr,
	    job_m_in.m_lc_vfs_sendrecv.addr_len,
	    job_m_in.m_lc_vfs_sendrecv.flags, READING, flags, 0);
}

/*
 * Resume a previously suspended recvfrom(2) system call.  This function MUST
 * NOT block its calling thread.
 */
void
resume_recvfrom(struct fproc * rfp, int status, unsigned int addr_len)
{
	message m;

	if (status >= 0) {
		memset(&m, 0, sizeof(m));
		m.m_vfs_lc_socklen.len = addr_len;

		reply(&m, rfp->fp_endpoint, status);
	} else
		replycode(rfp->fp_endpoint, status);
}

/*
 * Send or receive a message on a socket using a message structure.
 */
int
do_sockmsg(void)
{
	struct msghdr msg;
	struct iovec iov;
	vir_bytes msg_buf, data_buf;
	size_t data_len;
	dev_t dev;
	int r, fd, flags;

	assert(job_call_nr == VFS_SENDMSG || job_call_nr == VFS_RECVMSG);

	fd = job_m_in.m_lc_vfs_sockmsg.fd;
	msg_buf = job_m_in.m_lc_vfs_sockmsg.msgbuf;

	if ((r = get_sock(fd, &dev, &flags)) != OK)
		return r;

	if ((r = sys_datacopy_wrapper(who_e, msg_buf, SELF, (vir_bytes)&msg,
	    sizeof(msg))) != OK)
		return r;

	data_buf = 0;
	data_len = 0;
	if (msg.msg_iovlen > 0) {
		/*
		 * We do not yet support vectors with more than one element;
		 * for this reason, libc is currently expected to consolidate
		 * the entire vector into a single element.  Once we do add
		 * proper vector support, the ABI itself need not be changed.
		 */
		if (msg.msg_iovlen > 1)
			return EMSGSIZE;

		if ((r = sys_datacopy_wrapper(who_e, (vir_bytes)msg.msg_iov,
		    SELF, (vir_bytes)&iov, sizeof(iov))) != OK)
			return r;

		if (iov.iov_len > SSIZE_MAX)
			return EINVAL;

		if (iov.iov_len > 0) {
			data_buf = (vir_bytes)iov.iov_base;
			data_len = iov.iov_len;
		}
	}

	return sdev_readwrite(dev, data_buf, data_len,
	    (vir_bytes)msg.msg_control, msg.msg_controllen,
	    (vir_bytes)msg.msg_name, msg.msg_namelen,
	    job_m_in.m_lc_vfs_sockmsg.flags,
	    (job_call_nr == VFS_RECVMSG) ? READING : WRITING, flags,
	    (job_call_nr == VFS_RECVMSG) ? msg_buf : 0);
}

/*
 * Resume a previously suspended recvmsg(2) system call.  The 'status'
 * parameter contains either the number of data bytes received or a negative
 * error code.  The 'msg_buf' parameter contains the user address of the msghdr
 * structure.  If a failure occurs in this function, the received data
 * (including, in the worst case, references to received file descriptors) will
 * be lost - while seriously ugly, this is always the calling process's fault,
 * extremely hard to deal with, and on par with current behavior in other
 * operating systems.  This function MUST NOT block its calling thread.
 */
void
resume_recvmsg(struct fproc * rfp, int status, unsigned int ctl_len,
	unsigned int addr_len, int flags, vir_bytes msg_buf)
{
	struct msghdr msg;
	int r;

	if (status < 0) {
		replycode(rfp->fp_endpoint, status);

		return;
	}

	/*
	 * Unfortunately, we now need to update a subset of the fields of the
	 * msghdr structure.  We can 1) copy in the entire structure for the
	 * second time, modify some fields, and copy it out in its entirety
	 * again, 2) copy out individual fields that have been changed, 3) save
	 * a copy of the original structure somewhere.  The third option is the
	 * most efficient, but would increase the fproc structure size by quite
	 * a bit.  The main difference between the first and second options is
	 * the number of kernel calls; we choose to use the first option.
	 */
	if ((r = sys_datacopy_wrapper(rfp->fp_endpoint, msg_buf, SELF,
	    (vir_bytes)&msg, sizeof(msg))) != OK) {
		/* We copied it in before, how could it fail now? */
		printf("VFS: resume_recvmsg cannot copy in msghdr? (%d)\n", r);

		replycode(rfp->fp_endpoint, r);

		return;
	}

	/* Modify and copy out the structure, and wake up the caller. */
	msg.msg_controllen = ctl_len;
	msg.msg_flags = flags;
	if (addr_len > 0)
		msg.msg_namelen = addr_len;

	if ((r = sys_datacopy_wrapper(SELF, (vir_bytes)&msg, rfp->fp_endpoint,
	    msg_buf, sizeof(msg))) != OK)
		status = r;

	replycode(rfp->fp_endpoint, status);
}

/*
 * Set socket options.
 */
int
do_setsockopt(void)
{
	dev_t dev;
	int r, fd;

	fd = job_m_in.m_lc_vfs_sockopt.fd;

	if ((r = get_sock(fd, &dev, NULL)) != OK)
		return r;

	return sdev_setsockopt(dev, job_m_in.m_lc_vfs_sockopt.level,
	    job_m_in.m_lc_vfs_sockopt.name, job_m_in.m_lc_vfs_sockopt.buf,
	    job_m_in.m_lc_vfs_sockopt.len);
}

/*
 * Get socket options.
 */
int
do_getsockopt(void)
{
	unsigned int len;
	dev_t dev;
	int r, fd;

	fd = job_m_in.m_lc_vfs_sockopt.fd;
	len = job_m_in.m_lc_vfs_sockopt.len;

	if ((r = get_sock(fd, &dev, NULL)) != OK)
		return r;

	r = sdev_getsockopt(dev, job_m_in.m_lc_vfs_sockopt.level,
	    job_m_in.m_lc_vfs_sockopt.name, job_m_in.m_lc_vfs_sockopt.buf,
	    &len);

	if (r == OK)
		job_m_out.m_vfs_lc_socklen.len = len;
	return r;
}

/*
 * Get the local address of a socket.
 */
int
do_getsockname(void)
{
	unsigned int len;
	dev_t dev;
	int r, fd;

	fd = job_m_in.m_lc_vfs_sockaddr.fd;
	len = job_m_in.m_lc_vfs_sockaddr.addr_len;

	if ((r = get_sock(fd, &dev, NULL)) != OK)
		return r;

	r = sdev_getsockname(dev, job_m_in.m_lc_vfs_sockaddr.addr, &len);

	if (r == OK)
		job_m_out.m_vfs_lc_socklen.len = len;
	return r;
}

/*
 * Get the remote address of a socket.
 */
int
do_getpeername(void)
{
	unsigned int len;
	dev_t dev;
	int r, fd;

	fd = job_m_in.m_lc_vfs_sockaddr.fd;
	len = job_m_in.m_lc_vfs_sockaddr.addr_len;

	if ((r = get_sock(fd, &dev, NULL)) != OK)
		return r;

	r = sdev_getpeername(dev, job_m_in.m_lc_vfs_sockaddr.addr, &len);

	if (r == OK)
		job_m_out.m_vfs_lc_socklen.len = len;
	return r;
}

/*
 * Shut down socket send and receive operations.
 */
int
do_shutdown(void)
{
	dev_t dev;
	int r, fd, how;

	fd = job_m_in.m_lc_vfs_shutdown.fd;
	how = job_m_in.m_lc_vfs_shutdown.how;

	if ((r = get_sock(fd, &dev, NULL)) != OK)
		return r;

	if (how != SHUT_RD && how != SHUT_WR && how != SHUT_RDWR)
		return EINVAL;

	return sdev_shutdown(dev, how);
}
