/* Virtual mount table related routines.
 *
 */

#include "fs.h"
#include "vmnt.h"


/*===========================================================================*
 *                             get_free_vmnt				     *
 *===========================================================================*/
PUBLIC struct vmnt *get_free_vmnt(short *index)
{
  struct vmnt *vp;
  *index = 0;
  for (vp = &vmnt[0]; vp < &vmnt[NR_MNTS]; ++vp, ++(*index)) 
      if (vp->m_dev == NO_DEV) return(vp);

  return(NIL_VMNT);
}


/*===========================================================================*
 *                             find_vmnt				     *
 *===========================================================================*/
PUBLIC struct vmnt *find_vmnt(int fs_e) 
{
  struct vmnt *vp;
  for (vp = &vmnt[0]; vp < &vmnt[NR_MNTS]; ++vp) 
      if (vp->m_fs_e == fs_e) return(vp);

  return(NIL_VMNT);
}


