#include "syslib.h"

/*===========================================================================*
 *                               sys_segctl				     *    
 *===========================================================================*/
PUBLIC int sys_segctl(index, seg, off, phys, size)
int *index;				/* return index of remote segment */
u16_t *seg;				/* return segment selector here */
vir_bytes *off;				/* return offset in segment here */
phys_bytes phys;			/* physical address to convert */
vir_bytes size;				/* size of segment */
{
    message m;
    int s;
    m.SEG_PHYS = phys;
    m.SEG_SIZE = size;
    s = _taskcall(SYSTASK, SYS_SEGCTL, &m);
    *index = (int) m.SEG_INDEX;
    *seg = (u16_t) m.SEG_SELECT;
    *off = (vir_bytes) m.SEG_OFFSET;
    return s;
}


