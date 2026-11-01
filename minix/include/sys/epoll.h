/* sys/epoll.h - Linux-compatible epoll, implemented in MINIX libc.
 *
 * Emulated over poll(2): the epoll fd is a real descriptor (one end of a
 * pipe) but the interest list lives in the process (libc); epoll_wait()
 * polls the registered descriptors.  Semantics follow Linux for the common
 * level-triggered use (wayland, libinput, ...):
 *   - EPOLLIN/OUT/PRI/RDHUP/ERR/HUP, EPOLLONESHOT; data returned as given.
 *   - A registered descriptor that gets closed silently drops out of the
 *     set (on Linux this happens when its last reference is closed).
 *   - Regular files and directories: EPERM, as on Linux.
 * Not supported: EPOLLET (EINVAL - edge-triggered users would busy-loop),
 * EPOLLEXCLUSIVE/EPOLLWAKEUP are ignored, the interest list is not shared
 * with other processes (a fork child gets a copy), and an epoll fd is never
 * reported readable by poll/select/another epoll set.
 *
 * $MINIX$
 */
#ifndef _SYS_EPOLL_H_
#define _SYS_EPOLL_H_

#include <sys/cdefs.h>
#include <sys/types.h>
#include <signal.h>
#include <stdint.h>

#define EPOLLIN		0x001
#define EPOLLPRI	0x002
#define EPOLLOUT	0x004
#define EPOLLERR	0x008
#define EPOLLHUP	0x010
#define EPOLLRDNORM	0x040
#define EPOLLRDBAND	0x080
#define EPOLLWRNORM	0x100
#define EPOLLWRBAND	0x200
#define EPOLLMSG	0x400
#define EPOLLRDHUP	0x2000
#define EPOLLEXCLUSIVE	(1U << 28)
#define EPOLLWAKEUP	(1U << 29)
#define EPOLLONESHOT	(1U << 30)
#define EPOLLET		(1U << 31)

#define EPOLL_CTL_ADD	1
#define EPOLL_CTL_DEL	2
#define EPOLL_CTL_MOD	3

#define EPOLL_CLOEXEC	02000000	/* Linux value */

typedef union epoll_data {
	void		*ptr;
	int		 fd;
	uint32_t	 u32;
	uint64_t	 u64;
} epoll_data_t;

struct epoll_event {
	uint32_t	events;
	epoll_data_t	data;
} __packed;			/* as on Linux x86 */

__BEGIN_DECLS
int	epoll_create(int __size);
int	epoll_create1(int __flags);
int	epoll_ctl(int __epfd, int __op, int __fd, struct epoll_event *__event);
int	epoll_wait(int __epfd, struct epoll_event *__events, int __maxevents,
	    int __timeout);
int	epoll_pwait(int __epfd, struct epoll_event *__events, int __maxevents,
	    int __timeout, const sigset_t *__sigmask);
__END_DECLS

#endif /* _SYS_EPOLL_H_ */
