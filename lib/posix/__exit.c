#define _exit	__exit
#include <lib.h>
#include <unistd.h>

PUBLIC void _exit(status)
int status;
{
  void (*suicide)(void);
  message m;

  m.m1_i1 = status;
  _syscall(MM, EXIT, &m);

  /* If exiting nicely through PM fails for some reason, try to
   * commit suicide. E.g., message to PM might fail due to deadlock.
   */
  suicide = (void (*)(void)) -1;
  suicide();

  /* If committing suicide fails for some reason, hang. */
  for(;;) { }
}

