/* This file contains memory management routines for RS.
 *
 * Changes:
 *   Nov 22, 2009: Created    (Cristiano Giuffrida)
 */

#include "inc.h"

#define munmap _munmap
#include <sys/mman.h>
#undef munmap

int unmap_ok = 0;

/*===========================================================================*
 *				    munmap            		     	*
 *===========================================================================*/
int munmap(void *addrstart, vir_bytes len)
{
  if(!unmap_ok) 
      return ENOSYS;

  return _munmap(addrstart, len);
}
