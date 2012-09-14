/* This file contains memory management routines for RS.
 *
 * Changes:
 *   Nov 22, 2009: Created    (Cristiano Giuffrida)
 */

#include "inc.h"

#define minix_munmap _minix_munmap
#include <sys/mman.h>
#undef minix_munmap

int unmap_ok = 0;

/*===========================================================================*
 *				    minix_munmap            		     *
 *===========================================================================*/
int minix_munmap(void *addrstart, vir_bytes len)
{
  if(!unmap_ok) 
      return ENOSYS;

  return _minix_munmap(addrstart, len);
}
