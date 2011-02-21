#ifndef _SYS_UCONTEXT_H_
#define _SYS_UCONTEXT_H_

#include <sys/sigtypes.h>
#include <machine/mcontext.h>

typedef struct __ucontext ucontext_t;

struct __ucontext {
  unsigned int uc_flags;  /* Properties of ucontext */
  ucontext_t *uc_link;    /* Next context to resume when current is finished */
  mcontext_t uc_mcontext; /* Machine state */
  sigset_t uc_sigmask;    /* Signals blocked in this context */
  stack_t uc_stack;       /* The stack used by this context */
};

#ifndef _UC_UCONTEXT_ALIGN
#define _UC_UCONTEXT_ALIGN (~0)
#endif

#define UCF_SWAPPED	001 /* Context has been swapped in by swapcontext(3) */
#define UCF_IGNFPU	002 /* Ignore FPU context by get or setcontext(3) */
#define UCF_IGNSIGM	004 /* Ignore signal mask by get or setcontext(3) */

#define NCARGS 6

#ifdef __minix
__BEGIN_DECLS
void resumecontext(ucontext_t *ucp);

/* These functions get and set ucontext structure through PM/kernel. They don't
 * manipulate the stack. */
int getuctx(ucontext_t *ucp);
int setuctx(const ucontext_t *ucp);
__END_DECLS
#endif /* __minix */

#endif /* !_SYS_UCONTEXT_H_ */
