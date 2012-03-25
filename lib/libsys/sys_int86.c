#include "syslib.h"

/*===========================================================================*
 *                                sys_int86				     *
 *===========================================================================*/
int sys_int86(reg86p)
struct reg86u *reg86p;
{
    message m;
    int result;

    m.m1_p1= (char *)reg86p;

    result = _kernel_call(SYS_INT86, &m);
    return(result);
}

