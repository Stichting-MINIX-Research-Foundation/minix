#include "syslib.h"

/*===========================================================================*
 *                                sys_kmalloc				     *
 *===========================================================================*/
PUBLIC int sys_kmalloc(size, phys_base)
size_t size; 				/* size in bytes */
phys_bytes *phys_base;			/* return physical base address */
{
    message m;
    int result;

    m.MEM_CHUNK_SIZE = size;

    if (OK == (result = _taskcall(SYSTASK, SYS_KMALLOC, &m)))
       *phys_base = (phys_bytes) m.MEM_CHUNK_BASE;
    return(result);
}

