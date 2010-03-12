/*
 * mcontext.c
 */
#include <lib.h>
#include <ucontext.h>
#include <unistd.h>

PUBLIC int setmcontext(const mcontext_t *mcp)
{
  message m;

  m.m1_p1 = (char *) mcp;

  return(_syscall(MM, SETMCONTEXT, &m));
}


PUBLIC int getmcontext(mcontext_t *mcp)
{
  message m;

  m.m1_p1 = (char *) mcp;

  return(_syscall(MM, GETMCONTEXT, &m));
}

