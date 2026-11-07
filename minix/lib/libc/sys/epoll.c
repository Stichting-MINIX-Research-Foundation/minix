/* epoll.c - Linux-compatible epoll for MINIX, emulated over poll(2).
 *
 * Each epoll instance is the read end of a pipe (so the fd is real: it can
 * be closed, inherited, marked close-on-exec) plus an interest list kept
 * here.  epoll_wait() turns the list into one poll(2) call.  To detect a
 * closed-and-reused epoll fd number, the pipe's inode is remembered and
 * checked on every call.  See <sys/epoll.h> for the semantics.
 *
 * Nobody writes to that pipe, so on its own an epoll fd never becomes
 * readable.  poll(2) therefore asks __libc_epoll_poll() below to replace
 * epoll fds in its set by their members (recursively), and reports an
 * epoll fd readable when one of its members is ready - so an epoll fd can
 * be polled, and nested into another epoll instance (as Wayland
 * compositors do with libinput's fd).  select(2) on an epoll fd is not
 * covered.
 *
 * An instance is identified by its pipe (device and inode; libc keeps the
 * write end open, so the identity stays unique while the instance lives),
 * not by the descriptor number: dup()ed epoll fds - libwayland's
 * wl_event_loop_add_fd() dups the fd it is given - name the same instance.
 * Descriptors that epoll_ctl() found to be such duplicates are remembered
 * as aliases, so that poll() can spot epoll fds without system calls.  The
 * instance goes away when its original descriptor is closed (unlike Linux,
 * where the last duplicate counts).
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

static int *ep_alias;		/* fds found to be duplicates of epoll fds */
static int nalias, capalias;

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
/* The live instance whose pipe 'st' describes, or NULL. */
static struct epinst *
ep_by_identity(const struct stat *st)
{
	int i;

	if (!S_ISFIFO(st->st_mode))
		return NULL;
	for (i = 0; i < neps; i++)
		if (eps[i].epfd >= 0 && eps[i].dev == st->st_dev &&
		    eps[i].ino == st->st_ino)
			return &eps[i];
	return NULL;
}

static struct epinst *
ep_lookup(int epfd)
{
	struct epinst *ep;
	struct stat st;
	int i;

	if (fstat(epfd, &st) != 0)
		return NULL;		/* EBADF */
	for (i = 0; i < neps; i++) {
		ep = &eps[i];
		if (ep->epfd == epfd &&
		    (ep->dev != st.st_dev || ep->ino != st.st_ino))
			ep_free(ep);	/* closed and reused: stale */
	}
	if ((ep = ep_by_identity(&st)) != NULL)
		return ep;		/* the epoll fd itself, or a dup */
	errno = EINVAL;
	return NULL;
}

static void
ep_alias_add(int fd)
{
	int i, *n;

	for (i = 0; i < nalias; i++)
		if (ep_alias[i] == fd)
			return;
	if (nalias == capalias) {
		int cap = capalias ? capalias * 2 : 8;
		if ((n = realloc(ep_alias, cap * sizeof(*n))) == NULL)
			return;		/* only an optimization hint */
		ep_alias = n;
		capalias = cap;
	}
	ep_alias[nalias++] = fd;
}

static void
ep_alias_del(int fd)
{
	int i;

	for (i = 0; i < nalias; i++)
		if (ep_alias[i] == fd) {
			ep_alias[i] = ep_alias[--nalias];
			return;
		}
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

	/* A duplicate of an epoll fd (not the original): remember it. */
	if (op != EPOLL_CTL_DEL && ep_by_identity(&st) != NULL)
		ep_alias_add(fd);

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

/* Drop members whose fd was closed; return whether any were. */
static int
ep_purge_closed(struct epinst *ep)
{
	int i, dropped = 0;

	for (i = 0; i < ep->nitems; ) {
		if (fcntl(ep->items[i].fd, F_GETFD) == -1) {
			ep->items[i] = ep->items[--ep->nitems];
			dropped = 1;
		} else
			i++;
	}
	return dropped;
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
		int dropped = ep_purge_closed(ep);
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

/*
 * poll(2) support (see the top of this file).
 */
#define EP_NEST_MAX	8	/* nesting depth limit (Linux: 5) */

struct ep_pollset {
	struct pollfd	*pfd;
	nfds_t		*owner;		/* index into the caller's set */
	nfds_t		n, cap;
};

static int
ep_pollset_add(struct ep_pollset *ps, int fd, short events, nfds_t owner)
{
	if (ps->n == ps->cap) {
		nfds_t cap = ps->cap ? ps->cap * 2 : 32;
		struct pollfd *np = realloc(ps->pfd, cap * sizeof(*np));
		nfds_t *no;
		if (np == NULL)
			return -1;
		ps->pfd = np;
		if ((no = realloc(ps->owner, cap * sizeof(*no))) == NULL)
			return -1;
		ps->owner = no;
		ps->cap = cap;
	}
	ps->pfd[ps->n].fd = fd;
	ps->pfd[ps->n].events = events;
	ps->pfd[ps->n].revents = 0;
	ps->owner[ps->n++] = owner;
	return 0;
}

/* Cheap check, no system call: is 'fd' the number of an epoll instance? */
static struct epinst *
ep_candidate(int fd)
{
	int i;

	if (fd < 0)
		return NULL;
	for (i = 0; i < neps; i++)
		if (eps[i].epfd == fd)
			return &eps[i];
	for (i = 0; i < nalias; i++)
		if (ep_alias[i] == fd)
			return eps;	/* any non-NULL: verified by ep_lookup() */
	return NULL;
}

/*
 * Add the members of epoll instance 'epfd' (recursively) for 'owner'.
 * Return 0 when done, 1 if 'epfd' is not (or no longer) an epoll instance
 * - the caller polls it as an ordinary descriptor - and -1 on ENOMEM.
 */
static int
ep_pollset_expand(struct ep_pollset *ps, int epfd, nfds_t owner, int depth)
{
	struct epinst *ep;
	int i, fd, r;
	short ev;

	if (depth > EP_NEST_MAX)
		return 0;	/* too deep (a cycle?): never ready */
	if ((ep = ep_lookup(epfd)) == NULL) {
		ep_alias_del(epfd);	/* stale alias, or closed */
		return 1;
	}
	for (i = 0; i < ep->nitems; i++) {
		if (ep->items[i].events == 0)
			continue;	/* fired EPOLLONESHOT */
		fd = ep->items[i].fd;
		ev = ep_to_poll(ep->items[i].events);
		r = 1;
		if (ep_candidate(fd) != NULL &&
		    (r = ep_pollset_expand(ps, fd, owner, depth + 1)) < 0)
			return -1;
		if (r == 1 && ep_pollset_add(ps, fd, ev, owner) != 0)
			return -1;
	}
	return 0;
}

int __libc_poll_select(struct pollfd *, nfds_t, int);
int __libc_epoll_poll(struct pollfd *, nfds_t, int, int *);

int
__libc_epoll_poll(struct pollfd *p, nfds_t nfds, int timeout, int *handled)
{
	struct ep_pollset ps;
	nfds_t i, j;
	int r, any, retried;

	*handled = 0;
	for (any = 0, i = 0; i < nfds && !any; i++)
		if ((p[i].events & (POLLIN | POLLRDNORM)) &&
		    ep_candidate(p[i].fd) != NULL)
			any = 1;
	if (!any)
		return 0;	/* no epoll fds: plain poll() */

	*handled = 1;
	retried = 0;
again:
	memset(&ps, 0, sizeof(ps));
	for (i = 0; i < nfds; i++) {
		p[i].revents = 0;
		r = 1;
		if ((p[i].events & (POLLIN | POLLRDNORM)) &&
		    ep_candidate(p[i].fd) != NULL)
			r = ep_pollset_expand(&ps, p[i].fd, i, 1);
		if (r == 1)
			r = ep_pollset_add(&ps, p[i].fd, p[i].events, i);
		if (r != 0) {
			free(ps.pfd);
			free(ps.owner);
			errno = ENOMEM;
			return -1;
		}
	}

	r = __libc_poll_select(ps.pfd, ps.n, timeout);
	if (r < 0 && errno == EBADF && !retried) {
		/* A closed fd: drop closed members of all instances (as
		 * epoll_wait() does), then try once more; an fd closed in
		 * the caller's own set still fails with EBADF. */
		int k, dropped = 0;
		for (k = 0; k < neps; k++)
			if (eps[k].epfd >= 0 && ep_purge_closed(&eps[k]))
				dropped = 1;
		free(ps.pfd);
		free(ps.owner);
		if (!dropped) {
			errno = EBADF;
			return -1;
		}
		retried = 1;
		goto again;
	}
	if (r > 0) {
		for (j = 0; j < ps.n; j++) {
			if (ps.pfd[j].revents == 0)
				continue;
			i = ps.owner[j];
			if (ps.pfd[j].fd != p[i].fd)
				/* A member of epoll fd p[i]: it is readable. */
				p[i].revents |= p[i].events &
				    (POLLIN | POLLRDNORM);
			else
				p[i].revents |= ps.pfd[j].revents;
		}
		for (r = 0, i = 0; i < nfds; i++)
			if (p[i].revents != 0)
				r++;
	}
	free(ps.pfd);
	free(ps.owner);
	return r;
}
