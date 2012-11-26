#ifndef _MACHINE_MCONTEXT_H
#define _MACHINE_MCONTEXT_H

#include <machine/stackframe.h>

#define MCF_MAGIC 0xc0ffee

/* Context to describe processor state */
typedef struct __mcontext {
  int mc_magic;
  struct stackframe_s mc_p_reg;
} mcontext_t;

__BEGIN_DECLS
int setmcontext(const mcontext_t *mcp);
int getmcontext(mcontext_t *mcp);
__END_DECLS

#endif /* _MACHINE_MCONTEXT_H */
