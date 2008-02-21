#include "syslib.h"

/*===========================================================================*
 *                                sys_mapdma				     *
 *===========================================================================*/
PUBLIC int sys_mapdma(vir_addr, bytes)
vir_bytes vir_addr;			/* address in bytes with segment*/
vir_bytes bytes;			/* number of bytes to be copied */
{
    message m;
    int result;

    m.CP_SRC_ADDR = vir_addr;
    m.CP_NR_BYTES = bytes;

    result = _taskcall(SYSTASK, SYS_MAPDMA, &m);
    return(result);
}

