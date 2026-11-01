/* epoll.c - Linux-compatible epoll for MINIX, emulated over poll(2).
 *
 * Each epoll instance is the read end of a pipe (so the fd is real: it can
 * be closed, inherited, marked close-on-exec) plus an interest list kept
 * here.  epoll_wait() turns the list into one poll(2) call.  To detect a
 * closed-and-reused epoll fd number, the pipe's inode is remembered and
 * checked on every call.  See <sys/epoll.h> for the semantics.
 */

#include <sys/cdefs.h>
#include "namespace.h"

#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct epitem {
	int		fd;
	uint32_t	events;		/* 0: disabled (fired ONESHOT) */
	epoll_data_t	data;
	int		stream;		/* connected-type socket: EOF -> HUP */
};

struct epinst {
	int		epfd;		/* -1: free slot */
	int		wfd;		/* libc-held other end of the pipe */
	dev_t		dev;		/* identity of the pipe */
	ino_t		ino;
	struct epitem	*items;
	int		nitems, cap;
	int		rotor;		/* fairness when maxevents is small */
};

static struct epinst *eps;
static int neps;

#define EP_SUPPORTED	(EPOLLIN | EPOLLPRI | EPOLLOUT | EPOLLERR | EPOLLHUP | \
	EPOLLRDNORM | EPOLLRDBAND | EPOLLWRNORM | EPOLLWRBAND | EPOLLMSG | \
	EPOLLRDHUP | EPOLLEXCLUSIVE | EPOLLWAKEUP | EPOLLONESHOT)

static void
ep_free(struct epinst *ep)
{
	if (ep->wfd >= 0)
		close(ep->wfd);
	free(ep->items);
	memset(ep, 0, sizeof(*ep));
	ep->epfd = -1;
	ep->wfd = -1;
}

/* Find the live instance behind 'epfd'; set errno and return NULL if none. */
static struct epinst *
ep_lookup(int epfd)
{
	struct stat st;
	int i;

	if (fstat(epfd, &st) != 0)
		return NULL;		/* EBADF */
	for (i = 0; i < neps; i++) {
		struct epinst *ep = &eps[i];
		if (ep->epfd != epfd)
			continue;
		if (ep->dev == st.st_dev && ep->ino == st.st_ino)
			return ep;
		ep_free(ep);		/* closed and reused: stale */
		break;
	}
	errno = EINVAL;
	return NULL;
}

int
epoll_create1(int flags)
{
	struct epinst *ep = NULL;
	struct stat st;
	int p[2], i;

	if (flags & ~EPOLL_CLOEXEC) {
		errno = EINVAL;
		return -1;
	}
	if (pipe(p) != 0)
		return -1;

	for (i = 0; i < neps; i++) {
		if (eps[i].epfd == p[0])
			ep_free(&eps[i]);	/* stale entry, fd reused */
		if (eps[i].epfd == -1 && ep == NULL)
			ep = &eps[i];
	}
	if (ep == NULL) {
		struct epinst *n = realloc(eps, (neps + 4) * sizeof(*eps));
		if (n == NULL) {
			close(p[0]);
			close(p[1]);
			errno = ENOMEM;
			return -1;
		}
		eps = n;
		for (i = neps; i < neps + 4; i++) {
			memset(&eps[i], 0, sizeof(eps[i]));
			eps[i].epfd = eps[i].wfd = -1;
		}
		ep = &eps[neps];
		neps += 4;
	}

	if (fstat(p[0], &st) != 0) {
		close(p[0]);
		close(p[1]);
		return -1;
	}
	ep->epfd = p[0];
	ep->wfd = p[1];
	ep->dev = st.st_dev;
	ep->ino = st.st_ino;
	ep->items = NULL;
	ep->nitems = ep->cap = ep->rotor = 0;

	/* The private end never leaks into exec'd programs. */
	fcntl(p[1], F_SETFD, FD_CLOEXEC);
	if (flags & EPOLL_CLOEXEC)
		fcntl(p[0], F_SETFD, FD_CLOEXEC);
	return p[0];
}

int
epoll_create(int size)
{
	if (size <= 0) {
		errno = EINVAL;
		return -1;
	}
	return epoll_create1(0);
}

static int
ep_find(struct epinst *ep, int fd)
{
	int i;

	for (i = 0; i < ep->nitems; i++)
		if (ep->items[i].fd == fd)
			return i;
	return -1;
}

int
epoll_ctl(int epfd, int op, int fd, struct epoll_event *event)
{
	struct epinst *ep;
	struct stat st;
	int i;

	if ((ep = ep_lookup(epfd)) == NULL)
		return -1;
	if (fstat(fd, &st) != 0)
		return -1;		/* EBADF */
	if (fd == epfd) {
		errno = EINVAL;
		return -1;
	}
	if (S_ISREG(st.st_mode) || S_ISDIR(st.st_mode)) {
		errno = EPERM;		/* always "ready": Linux refuses */
		return -1;
	}
	if (op != EPOLL_CTL_DEL) {
		if (event == NULL) {
			errno = EFAULT;
			return -1;
		}
		if (event->events & EPOLLET) {
			errno = EINVAL;	/* not emulated, see <sys/epoll.h> */
			return -1;
		}
	}

	i = ep_find(ep, fd);
	switch (op) {
	case EPOLL_CTL_ADD:
		if (i >= 0) {
			errno = EEXIST;
			return -1;
		}
		if (ep->nitems == ep->cap) {
			int cap = ep->cap ? ep->cap * 2 : 16;
			struct epitem *n = realloc(ep->items,
			    cap * sizeof(*n));
			if (n == NULL) {
				errno = ENOMEM;
				return -1;
			}
			ep->items = n;
			ep->cap = cap;
		}
		i = ep->nitems++;
		ep->items[i].fd = fd;
		ep->items[i].stream = 0;
		if (S_ISSOCK(st.st_mode)) {
			int type;
			socklen_t len = sizeof(type);
			if (getsockopt(fd, SOL_SOCKET, SO_TYPE, &type,
			    &len) == 0 && (type == SOCK_STREAM ||
			    type == SOCK_SEQPACKET))
				ep->items[i].stream = 1;
		}
		/* FALLTHROUGH */
	case EPOLL_CTL_MOD:
		if (i < 0) {
			errno = ENOENT;
			return -1;
		}
		ep->items[i].events = (event->events & EP_SUPPORTED) |
		    EPOLLERR | EPOLLHUP;
		ep->items[i].data = event->data;
		return 0;
	case EPOLL_CTL_DEL:
		if (i < 0) {
			errno = ENOENT;
			return -1;
		}
		ep->items[i] = ep->items[--ep->nitems];
		return 0;
	default:
		errno = EINVAL;
		return -1;
	}
}

static short
ep_to_poll(uint32_t ev)
{
	short p = 0;

	if (ev & (EPOLLIN | EPOLLRDNORM | EPOLLRDHUP)) p |= POLLIN;
	if (ev & EPOLLRDBAND) p |= POLLRDBAND;
	if (ev & EPOLLPRI) p |= POLLPRI;
	if (ev & (EPOLLOUT | EPOLLWRNORM)) p |= POLLOUT;
	if (ev & EPOLLWRBAND) p |= POLLWRBAND;
	return p;
}

static uint32_t
poll_to_ep(short re, uint32_t want)
{
	uint32_t ev = 0;

	if (re & (POLLIN | POLLRDNORM)) ev |= EPOLLIN | EPOLLRDNORM;
	if (re & POLLRDBAND) ev |= EPOLLRDBAND;
	if (re & POLLPRI) ev |= EPOLLPRI;
	if (re & (POLLOUT | POLLWRNORM)) ev |= EPOLLOUT | EPOLLWRNORM;
	if (re & POLLWRBAND) ev |= EPOLLWRBAND;
	if (re & POLLERR) ev |= EPOLLERR;
	if (re & POLLHUP) ev |= EPOLLHUP | EPOLLRDHUP;
	return ev & (want | EPOLLERR | EPOLLHUP);
}

int
epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout)
{
	struct epinst *ep;
	struct pollfd *pfd;
	int *idx;
	int i, j, n, r, out, start;

	if (maxevents <= 0 || events == NULL) {
		errno = EINVAL;
		return -1;
	}
	if ((ep = ep_lookup(epfd)) == NULL)
		return -1;

	pfd = malloc((ep->nitems + 1) * sizeof(*pfd));
	idx = malloc((ep->nitems + 1) * sizeof(*idx));
	if (pfd == NULL || idx == NULL) {
		free(pfd);
		free(idx);
		errno = ENOMEM;
		return -1;
	}
	for (n = 0, i = 0; i < ep->nitems; i++) {
		if (ep->items[i].events == 0)
			continue;	/* fired EPOLLONESHOT */
		pfd[n].fd = ep->items[i].fd;
		pfd[n].events = ep_to_poll(ep->items[i].events);
		pfd[n].revents = 0;
		idx[n++] = ep->items[i].fd;
	}

	r = poll(pfd, n, timeout);
	if (r < 0 && errno == EBADF) {
		/* MINIX poll() fails as a whole on a closed descriptor
		 * (rather than POLLNVAL): drop closed members, as Linux does
		 * when the last reference goes away, and try again. */
		int dropped = 0;
		for (i = 0; i < ep->nitems; ) {
			if (fcntl(ep->items[i].fd, F_GETFD) == -1) {
				ep->items[i] = ep->items[--ep->nitems];
				dropped = 1;
			} else
				i++;
		}
		free(pfd);
		free(idx);
		if (!dropped) {
			errno = EBADF;
			return -1;
		}
		return epoll_wait(epfd, events, maxevents, timeout);
	}
	if (r <= 0) {
		free(pfd);
		free(idx);
		return r;	/* 0: timeout; -1: errno set (EINTR...) */
	}

	/* The interest list cannot have changed during poll() (single
	 * threaded use), but look items up by fd to be safe. */
	out = 0;
	start = n > 0 ? ep->rotor % n : 0;
	for (j = 0; j < n && out < maxevents; j++) {
		int k = (start + j) % n, it;
		uint32_t ev;

		if (pfd[k].revents == 0)
			continue;
		if ((it = ep_find(ep, idx[k])) < 0)
			continue;
		if (pfd[k].revents & POLLNVAL) {
			/* Closed without EPOLL_CTL_DEL: drop it, as Linux
			 * does when the last reference goes away. */
			ep->items[it] = ep->items[--ep->nitems];
			continue;
		}
		ev = poll_to_ep(pfd[k].revents, ep->items[it].events);
		/* MINIX poll() reports a stream socket whose peer has gone as
		 * merely readable; Linux says EPOLLHUP (wayland-server and
		 * others rely on that to tell a disconnect from an error). */
		if ((pfd[k].revents & POLLIN) && ep->items[it].stream) {
			char c;
			if (recv(idx[k], &c, 1, MSG_PEEK | MSG_DONTWAIT) == 0)
				ev |= EPOLLHUP | (ep->items[it].events &
				    EPOLLRDHUP);
		}
		if (ev == 0)
			continue;
		events[out].events = ev;
		events[out].data = ep->items[it].data;
		out++;
		if (ep->items[it].events & EPOLLONESHOT)
			ep->items[it].events = 0;
	}
	ep->rotor = n > 0 ? (start + j) % n : 0;

	free(pfd);
	free(idx);
	if (out == 0 && timeout != 0) {
		/* Only closed fds fired: wait again (rare). */
		return epoll_wait(epfd, events, maxevents, timeout);
	}
	return out;
}

int
epoll_pwait(int epfd, struct epoll_event *events, int maxevents, int timeout,
	const sigset_t *sigmask)
{
	sigset_t old;
	int r, e;

	/* Not atomic like Linux: a signal unblocked here may be delivered
	 * just before poll() starts and not interrupt it. */
	if (sigmask != NULL && sigprocmask(SIG_SETMASK, sigmask, &old) != 0)
		return -1;
	r = epoll_wait(epfd, events, maxevents, timeout);
	e = errno;
	if (sigmask != NULL)
		sigprocmask(SIG_SETMASK, &old, NULL);
	errno = e;
	return r;
}
