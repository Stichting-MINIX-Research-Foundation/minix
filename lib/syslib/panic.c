#include <stdlib.h>
#include <signal.h>
#include <minix/sysutil.h>

#include "syslib.h"

int panicing= 0;

/*===========================================================================*
 *				panic					     *
 *===========================================================================*/
PUBLIC void panic(who, mess, num)
char *who;			/* server identification */
char *mess;			/* message format string */
int num;			/* number to go with format string */
{
/* Something awful has happened. Panics are caused when an internal
 * inconsistency is detected, e.g., a programming error or illegal 
 * value of a defined constant.
 */
  message m;
  void (*suicide)(void);

  panicing= 1;
  if (NULL != who && NULL != mess) {
      if (num != NO_NUM) {
          printf("Panic in %s: %s: %d\n", who, mess, num); 
      } else {
          printf("Panic in %s: %s\n", who, mess); 
      }
  }

  /* Try to signal ourself */
  sys_kill(SELF, SIGKILL);

  /* If exiting nicely through PM fails for some reason, try to
   * commit suicide. E.g., message to PM might fail due to deadlock.
   */
  suicide = (void (*)(void)) -1;
  suicide();

  /* If committing suicide fails for some reason, hang. */
  for(;;) { }
}

