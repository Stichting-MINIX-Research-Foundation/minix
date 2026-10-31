/* sys/eventfd.h - Linux-compatible eventfd, implemented in MINIX libc.
 * (The fd is backed by an AF_UNIX socketpair; poll/select/eventfd_read/
 * eventfd_write work as on Linux.  Prefer eventfd_read/eventfd_write
 * over raw read/write; a raw write(2) on the fd is silently lost.)
 *
 * $MINIX$
 */
#ifndef _SYS_EVENTFD_H_
#define _SYS_EVENTFD_H_

#include <sys/cdefs.h>
#include <sys/types.h>
#include <stdint.h>

#define EFD_SEMAPHORE	1	/* like Linux EFD_SEMAPHORE */
#define EFD_CLOEXEC	02000000
#define EFD_NONBLOCK	04000

typedef uint64_t eventfd_t;

__BEGIN_DECLS
int	eventfd(unsigned int __initval, int __flags);
int	eventfd_read(int __fd, eventfd_t *__val);
int	eventfd_write(int __fd, eventfd_t __val);
__END_DECLS

#endif /* _SYS_EVENTFD_H_ */
