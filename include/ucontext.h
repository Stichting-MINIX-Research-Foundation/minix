#ifndef _UCONTEXT_H
#define _UCONTEXT_H 1

#include <sys/ucontext.h>

_PROTOTYPE( void makecontext, (ucontext_t *ucp, void (*func)(void),
				int argc, ...)				);
_PROTOTYPE( int swapcontext, (ucontext_t *oucp,
			      const ucontext_t *ucp)			);
_PROTOTYPE( int getcontext, (ucontext_t *ucp)			);
_PROTOTYPE( int setcontext, (const ucontext_t *ucp)		);

_PROTOTYPE( void resumecontext, (ucontext_t *ucp)		);

/* These functions get and set ucontext structure through PM/kernel. They don't
 * manipulate the stack. */
_PROTOTYPE( int getuctx, (ucontext_t *ucp)			);
_PROTOTYPE( int setuctx, (const ucontext_t *ucp)		);

#endif /* _UCONTEXT_H */

