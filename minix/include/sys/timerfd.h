/* sys/timerfd.h - Linux-compatible timerfd, implemented in MINIX libc.
 * (Backed by one setitimer(SIGALRM) per process, multiplexed in libc over
 * all timerfds.  Note: applications must not use alarm()/setitimer()
 * themselves.  Expirations are counted; read(2) returns the 8-byte count.)
 *
 * $MINIX$
 */
#ifndef _SYS_TIMERFD_H_
#define _SYS_TIMERFD_H_

#include <sys/cdefs.h>
#include <sys/types.h>
#include <time.h>

#define TFD_CLOEXEC	02000000
#define TFD_NONBLOCK	04000

/* flags for timerfd_settime */
#define TFD_TIMER_ABSTIME	1
#define TFD_TIMER_CANCEL_SET	2	/* accepted, ignored */

__BEGIN_DECLS
int	timerfd_create(int __clockid, int __flags);
int	timerfd_settime(int __fd, int __flags,
	    const struct itimerspec *__new, struct itimerspec *__old);
int	timerfd_gettime(int __fd, struct itimerspec *__cur);
__END_DECLS

#endif /* _SYS_TIMERFD_H_ */
