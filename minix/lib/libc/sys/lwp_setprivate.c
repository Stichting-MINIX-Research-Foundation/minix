#include <sys/types.h>

/*
 * MINIX stub: _lwp_setprivate sets the TLS base (%fs on x86_64).
 * Full implementation requires kernel FSGSBASE support (CR4.FSGSBASE)
 * or a sysarch syscall, neither of which MINIX currently provides.
 */
void
_lwp_setprivate(void *ptr)
{
}
