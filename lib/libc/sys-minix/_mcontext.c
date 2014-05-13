/*
 * mcontext.c
*/
#include <sys/cdefs.h>
#include <lib.h>
#include <namespace.h>

#include <string.h>
#include <ucontext.h>
#include <unistd.h>

int setmcontext(const mcontext_t *mcp)
{
  message m;

  memset(&m, 0, sizeof(m));
  m.m_lc_pm_mcontext.ctx = (vir_bytes)mcp;

  return(_syscall(PM_PROC_NR, PM_SETMCONTEXT, &m));
}


int getmcontext(mcontext_t *mcp)
{
  message m;

  memset(&m, 0, sizeof(m));
  m.m_lc_pm_mcontext.ctx = (vir_bytes)mcp;

  return(_syscall(PM_PROC_NR, PM_GETMCONTEXT, &m));
}

