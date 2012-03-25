/*
 * mcontext.c
*/
#include <sys/cdefs.h>
#include <lib.h>
#include <namespace.h>

#include <ucontext.h>
#include <unistd.h>

int setmcontext(const mcontext_t *mcp)
{
  message m;

  m.m1_p1 = (char *) __UNCONST(mcp);

  return(_syscall(PM_PROC_NR, SETMCONTEXT, &m));
}


int getmcontext(mcontext_t *mcp)
{
  message m;

  m.m1_p1 = (char *) mcp;

  return(_syscall(PM_PROC_NR, GETMCONTEXT, &m));
}

