#include "sysutil.h" 

/*===========================================================================*
 *				    report					     *
 *===========================================================================*/
PUBLIC void report(who, mess, num)
char *who;				/* server identification */
char *mess;				/* message format to print */
int num;				/* number to go with the message */
{
/* Display a message for a server. */ 

  if (num != NO_NUM) {
      printf("%s: %s %d\n", who, mess, num);
  } else {
      printf("%s: %s\n", who, mess);
  }
}


