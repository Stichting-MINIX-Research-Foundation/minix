#include "sysutil.h"

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
  if (NULL != who && NULL != mess) {
      if (num != NO_NUM) {
          printf("Panic in %s: %s: %d\n", who, mess, num); 
      } else {
          printf("Panic in %s: %s\n", who, mess); 
      }
  }

  m.PR_PROC_NR = SELF;
  _taskcall(SYSTASK, SYS_EXIT, &m);
  /* never reached */
}

