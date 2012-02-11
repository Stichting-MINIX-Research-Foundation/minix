#ifndef _MACHINE_MCONTEXT_H
#define _MACHINE_MCONTEXT_H 1

#include <machine/fpu.h>
#include <machine/stackframe.h>

#define MCF_MAGIC 0xc0ffee

/* Context to describe processor state */
typedef struct __mcontext {
  int mc_magic;  
  struct stackframe_s mc_p_reg;
  union fpu_state_u mc_fpu_state;
  short mc_fpu_flags;
} mcontext_t;

__BEGIN_DECLS
int setmcontext(const mcontext_t *mcp);
int getmcontext(mcontext_t *mcp);
__END_DECLS

#endif /* _MACHINE_MCONTEXT_H */
