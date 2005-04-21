#include "syslib.h"

/*===========================================================================*
 *                               sys_phys2seg				     *    
 *===========================================================================*/
PUBLIC int sys_phys2seg(seg, off, phys, size)
u16_t *seg;				/* return segment selector here */
vir_bytes *off;				/* return offset in segment here */
phys_bytes phys;			/* physical address to convert */
vir_bytes size;				/* size of segment */
{
    message m;
    int s;
    m.SEG_PHYS = phys;
    m.SEG_SIZE = size;
    s = _taskcall(SYSTASK, SYS_PHYS2SEG, &m);
    *seg = (u16_t) m.SEG_SELECT;
    *off = (vir_bytes) m.SEG_OFFSET;
    return s;
}


