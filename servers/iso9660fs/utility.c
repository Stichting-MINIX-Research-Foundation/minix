#include "inc.h"
#include <sys/stat.h>
#include <string.h>
#include <minix/com.h>
#include <minix/callnr.h>
#include <minix/vfsif.h>

static int panicking;

/*===========================================================================*
 *				no_sys					     *
 *===========================================================================*/
PUBLIC int no_sys()
{
/* Somebody has used an illegal system call number */
  return(EINVAL);
}

/*===========================================================================*
 *				panic					     *
 *===========================================================================*/
PUBLIC void panic(who, mess, num)
char *who;			/* who caused the panic */
char *mess;			/* panic message string */
int num;			/* number to go with it */
{
/* Something awful has happened.  Panics are caused when an internal
 * inconsistency is detected, e.g., a programming error or illegal value of a
 * defined constant.
 */
  if (panicking) return;	/* do not panic during a sync */
  panicking = TRUE;		/* prevent another panic during the sync */

  printf("FS panic (%s): %s ", who, mess);
  if (num != NO_NUM) printf("%d",num);
  exit(1);
}
