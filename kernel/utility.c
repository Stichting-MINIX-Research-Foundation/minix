/* This file contains a collection of miscellaneous procedures:
 *   panic:	    abort MINIX due to a fatal error
 */

#include "kernel.h"
#include <unistd.h>

/*===========================================================================*
 *				panic                                        *
 *===========================================================================*/
PUBLIC void panic(mess,nr)
_CONST char *mess;
int nr;
{
/* The system has run aground of a fatal kernel error. Terminate execution. */
  static int panicking = 0;
  if (panicking ++) return;		/* prevent recursive panics */

  if (mess != NULL) {
	kprintf("\nKernel panic: %s", mess);
	if (nr != NO_NUM) kprintf(" %d", nr);
	kprintf("\n",NO_NUM);
  }

  /* Abort MINIX. */
  prepare_shutdown(RBT_PANIC);
}
