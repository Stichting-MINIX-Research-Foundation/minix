#include "inc.h"
#include <sys/stat.h>
#include <string.h>
#include <minix/com.h>
#include <minix/callnr.h>
#include <minix/vfsif.h>

/*===========================================================================*
 *				do_noop					     *
 *===========================================================================*/
int do_noop(void)
{
/* Do not do anything. */
  return(OK);
}

/*===========================================================================*
 *				no_sys					     *
 *===========================================================================*/
int no_sys(void)
{
/* Somebody has used an illegal system call number */
  return(EINVAL);
}

