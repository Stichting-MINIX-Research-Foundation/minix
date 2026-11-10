/*
 * __libc_thr_yield() - sched_yield() for programs without threads.
 *
 * <sched.h> renames sched_yield() to __libc_thr_yield(), as NetBSD does,
 * but MINIX's libc does not build NetBSD's thread stubs, so the name was
 * not defined anywhere: sched_yield() did not link, and weak references
 * (libstdc++'s std::this_thread::yield) called address 0.  Without threads
 * there is nothing to yield to.  libmthread overrides this weak definition
 * with one that switches threads.
 */

#include <sys/cdefs.h>
#include <sched.h>

int __libc_thr_yield_stub(void);

int
__libc_thr_yield_stub(void)
{
	return 0;
}

__weak_alias(__libc_thr_yield, __libc_thr_yield_stub)
