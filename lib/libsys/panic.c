#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <stdarg.h>
#include <minix/sysutil.h>

#include "syslib.h"

/*===========================================================================*
 *				panic				     *
 *===========================================================================*/
PUBLIC void panic(const char *fmt, ...)
{
/* Something awful has happened. Panics are caused when an internal
 * inconsistency is detected, e.g., a programming error or illegal 
 * value of a defined constant.
 */
  message m;
  endpoint_t me = NONE;
  char name[20];
  void (*suicide)(void);
  static int panicing= 0;
  va_list args;

  if(panicing) return;
  panicing= 1;

  if(sys_whoami(&me, name, sizeof(name)) == OK && me != NONE)
	printf("%s(%d): panic: ", name, me);
  else
	printf("(sys_whoami failed): panic: ");

  if(fmt) {
	va_start(args, fmt);
	vprintf(fmt, args);
	va_end(fmt);
  } else {
	printf("no message\n");
  }
  printf("\n");

  printf("syslib:panic.c: stacktrace: ");
  util_stacktrace();

  /* Try exit */
  _exit(1);

  /* Try to signal ourself */
  abort();

  /* If exiting nicely through PM fails for some reason, try to
   * commit suicide. E.g., message to PM might fail due to deadlock.
   */
  suicide = (void (*)(void)) -1;
  suicide();

  /* If committing suicide fails for some reason, hang. */
  for(;;) { }
}

