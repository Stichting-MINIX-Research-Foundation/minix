#include "fs.h"


/*===========================================================================*
 *				no_sys					     *
 *===========================================================================*/
int no_sys(message *pfs_m_in, message *pfs_m_out)
{
/* Somebody has used an illegal system call number */
  printf("no_sys: invalid call 0x%x to pfs\n", pfs_m_in->m_type);
  return(EINVAL);
}
