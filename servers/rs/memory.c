/* This file contains memory management routines for RS.
 *
 * Changes:
 *   Nov 22, 2009: Created    (Cristiano Giuffrida)
 */

#include "inc.h"

#define munmap _munmap
#define munmap_text _munmap_text
#include <sys/mman.h>
#undef munmap
#undef munmap_text

PUBLIC int unmap_ok = 0;

/*===========================================================================*
 *				    munmap	            		     *
 *===========================================================================*/
PUBLIC int munmap(void *addrstart, vir_bytes len)
{
  if(!unmap_ok) 
      return ENOSYS;

  return _munmap(addrstart, len);
}

/*===========================================================================*
 *			         munmap_text	            		     *
 *===========================================================================*/
PUBLIC int munmap_text(void *addrstart, vir_bytes len)
{
  if(!unmap_ok)
      return ENOSYS;

  return _munmap_text(addrstart, len);
}
