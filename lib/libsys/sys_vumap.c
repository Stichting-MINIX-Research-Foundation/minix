#include "syslib.h"

/*===========================================================================*
 *				sys_vumap				     *
 *===========================================================================*/
int sys_vumap(
  endpoint_t endpt,			/* source process endpoint, or SELF */
  struct vumap_vir *vvec,		/* virtual (input) vector */
  int vcount,				/* number of elements in vvec */
  size_t offset,			/* offset into first vvec element */
  int access,				/* requested safecopy access flags */
  struct vumap_phys *pvec,		/* physical (output) vector */
  int *pcount				/* (max, returned) nr of els in pvec */
)
{
  message m;
  int r;

  m.VUMAP_ENDPT = endpt;
  m.VUMAP_VADDR = (vir_bytes) vvec;
  m.VUMAP_VCOUNT = vcount;
  m.VUMAP_OFFSET = offset;
  m.VUMAP_ACCESS = access;
  m.VUMAP_PADDR = (vir_bytes) pvec;
  m.VUMAP_PMAX = *pcount;

  r = _kernel_call(SYS_VUMAP, &m);

  if (r != OK)
	return r;

  *pcount = m.VUMAP_PCOUNT;
  return OK;
}
