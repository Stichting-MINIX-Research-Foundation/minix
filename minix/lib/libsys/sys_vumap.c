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

  m.m_lsys_krn_sys_vumap.endpt = endpt;
  m.m_lsys_krn_sys_vumap.vaddr = (vir_bytes) vvec;
  m.m_lsys_krn_sys_vumap.vcount = vcount;
  m.m_lsys_krn_sys_vumap.offset = offset;
  m.m_lsys_krn_sys_vumap.access = access;
  m.m_lsys_krn_sys_vumap.paddr = (vir_bytes) pvec;
  m.m_lsys_krn_sys_vumap.pmax = *pcount;

  r = _kernel_call(SYS_VUMAP, &m);

  if (r != OK)
	return r;

  *pcount = m.m_krn_lsys_sys_vumap.pcount;
  return OK;
}
