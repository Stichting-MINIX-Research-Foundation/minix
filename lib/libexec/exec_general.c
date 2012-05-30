#define _SYSTEM 1

#include <minix/type.h>
#include <minix/const.h>
#include <sys/param.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <libexec.h>
#include <string.h>
#include <assert.h>
#include <minix/ipc.h>
#include <minix/com.h>
#include <minix/callnr.h>
#include <machine/elf.h>

void libexec_patch_ptr(char stack[ARG_MAX], vir_bytes base)
{
/* When doing an exec(name, argv, envp) call, the user builds up a stack
 * image with arg and env pointers relative to the start of the stack.  Now
 * these pointers must be relocated, since the stack is not positioned at
 * address 0 in the user's address space.
 */

  char **ap, flag;
  vir_bytes v;

  flag = 0;                     /* counts number of 0-pointers seen */
  ap = (char **) stack;         /* points initially to 'nargs' */
  ap++;                         /* now points to argv[0] */
  while (flag < 2) {
        if (ap >= (char **) &stack[ARG_MAX]) return;    /* too bad */
        if (*ap != NULL) {
                v = (vir_bytes) *ap;    /* v is relative pointer */
                v += base;              /* relocate it */
                *ap = (char *) v;       /* put it back */
        } else {
                flag++;
        }
        ap++;
  }
}

int libexec_pm_newexec(endpoint_t proc_e, struct exec_info *e)
{
  int r;
  message m;

  m.m_type = PM_NEWEXEC;
  m.EXC_NM_PROC = proc_e;
  m.EXC_NM_PTR = (char *)e;
  if ((r = sendrec(PM_PROC_NR, &m)) != OK) return(r);

  e->allow_setuid = !!(m.m1_i2 & EXC_NM_RF_ALLOW_SETUID);

  return(m.m_type);
}
