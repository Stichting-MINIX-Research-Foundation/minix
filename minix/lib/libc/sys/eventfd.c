/* eventfd.c - Linux-compatible eventfd for MINIX.
 *
 * Substrate: an AF_UNIX/SOCK_STREAM socketpair (the FIFO O_RDWR trick does
 * not exist on MINIX).  The public fd is the read end; eventfd_write writes
 * into the libc-held other end.  poll/select/read/write on the public fd
 * work; the counter is accumulated by summing pending 8-byte units (written
 * one per eventfd_write call), giving Linux read semantics (drain-and-sum),
 * and EFD_SEMAPHORE yields one unit per read.
 *
 * Note: raw write(2) on the public fd goes nowhere visible (the stream
 * direction is one-way per end) - always use eventfd_write().
 */

#include <sys/cdefs.h>
#include "namespace.h"

#include <sys/eventfd.h>
#include <sys/socket.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>

#define MAX_EVENTFDS 64

static struct { int pub; int priv; int semaphore; } efds[MAX_EVENTFDS];
static int nefds;

static int
_efd_find(int fd)
{
	int i;
	for (i = 0; i < nefds; i++)
		if (efds[i].pub == fd) return i;
	return -1;
}

int
eventfd(unsigned int initval, int flags)
{
	int fds[2], i;

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) != 0)
		return -1;

	/* fd values get recycled by VFS after close(2): purge a stale entry
	 * for this public fd (and close its libc-held peer), or the peer end
	 * would be a dead socket.  The stale peer is still open, so it cannot
	 * collide with the fresh pair. */
	{
		int j = _efd_find(fds[0]);
		if (j >= 0) {
			close(efds[j].priv);
			efds[j] = efds[--nefds];
		}
	}

	if (nefds >= MAX_EVENTFDS) {
		close(fds[0]);
		close(fds[1]);
		errno = ENFILE;
		return -1;
	}

	i = nefds++;
	efds[i].pub = fds[0];
	efds[i].priv = fds[1];
	efds[i].semaphore = (flags & EFD_SEMAPHORE) != 0;

	if (flags & EFD_NONBLOCK) {
		fcntl(fds[0], F_SETFL, fcntl(fds[0], F_GETFL) | O_NONBLOCK);
		fcntl(fds[1], F_SETFL, fcntl(fds[1], F_GETFL) | O_NONBLOCK);
	}
	if (flags & EFD_CLOEXEC)
		fcntl(fds[0], F_SETFD, fcntl(fds[0], F_GETFD) | FD_CLOEXEC);
	/* the libc-held end must never leak into exec'd programs, and
	 * writing it after the public end was closed must not raise SIGPIPE */
	fcntl(fds[1], F_SETFD, fcntl(fds[1], F_GETFD) | FD_CLOEXEC);
	fcntl(fds[1], F_SETNOSIGPIPE, 1);

	while (initval-- > 0) {
		if (eventfd_write(fds[0], 1) != 0) {
			int e = errno;
			nefds--;	/* entry i is the last one */
			close(fds[0]);
			close(fds[1]);
			errno = e;
			return -1;
		}
	}
	return fds[0];
}

int
eventfd_write(int fd, eventfd_t val)
{
	static const eventfd_t one = 1;
	int i;
	ssize_t r;

	if ((i = _efd_find(fd)) < 0) { errno = EBADF; return -1; }
	/* one 8-byte unit per write call, regardless of magnitude: reads
	 * sum pending units; this models the counter closely enough */
	/* If the public end was closed, the write fails with EPIPE (the
	 * private end is F_SETNOSIGPIPE, so no signal); Linux says EIO. */
	for (; val > 0; val--) {
		r = write(efds[i].priv, &one, 8);
		if (r != 8) {
			if (errno == EPIPE) errno = EIO;
			return -1;
		}
	}
	return 0;
}

int
eventfd_read(int fd, eventfd_t *val)
{
	eventfd_t v = 0, t;
	ssize_t r;
	int sem = 0;

	r = _efd_find(fd);
	if (r >= 0) sem = efds[r].semaphore;

	r = read(fd, &t, 8);
	if (r != 8) {
		if (r < 0) return -1;	/* EAGAIN when empty + nonblock */
		errno = EBADF;		/* cannot happen: units are 8B */
		return -1;
	}
	v = 1;		/* each pending 8-byte unit counts as one */

	if (!sem) {
		/*
		 * Drain everything pending and sum.  This must never block,
		 * even on a blocking eventfd (a plain read(2) here would hang
		 * once the stream is empty), hence MSG_DONTWAIT.  A unit could
		 * in theory be split by a short stream transfer: finish any
		 * partial unit with a blocking read, its remainder is coming.
		 */
		char buf[64 * 8];
		uint64_t total = 0;
		int serrno = errno;

		while ((r = recv(fd, buf, sizeof(buf), MSG_DONTWAIT)) > 0)
			total += r;
		if (r < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
			return -1;
		if (total % 8 != 0) {
			size_t need = 8 - total % 8;
			while (need > 0) {
				r = read(fd, buf, need);
				if (r <= 0)
					return -1;
				need -= r;
			}
			total += 8 - total % 8;
		}
		v += total / 8;
		errno = serrno;
	}
	*val = v;
	return 0;
}
