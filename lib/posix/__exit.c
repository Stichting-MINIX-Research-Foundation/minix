#define _exit	__exit
#include <lib.h>
#include <unistd.h>

PUBLIC void _exit(status)
int status;
{
  message m;

  m.m1_i1 = status;
  _syscall(MM, EXIT, &m);
}
