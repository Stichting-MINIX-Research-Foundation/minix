#ifndef _SYS_UCONTEXT_H
#define _SYS_UCONTEXT_H 1

#include <signal.h>
#include <machine/mcontext.h>

#define NCARGS 6

#define UCF_SWAPPED	001 /* Context has been swapped in by swapcontext(3) */
#define UCF_IGNFPU	002 /* Ignore FPU context by get or setcontext(3) */
#define UCF_IGNSIGM	004 /* Ignore signal mask by get or setcontext(3) */
typedef struct __ucontext ucontext_t;
struct __ucontext {
  unsigned int uc_flags;  /* Properties of ucontext */
  ucontext_t *uc_link;    /* Next context to resume when current is finished */
  mcontext_t uc_mcontext; /* Machine state */
  sigset_t uc_sigmask;    /* Signals blocked in this context */
  stack_t uc_stack;       /* The stack used by this context */
};

#endif /* _SYS_UCONTEXT_H */

