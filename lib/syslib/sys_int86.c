#include "syslib.h"

/*===========================================================================*
 *                                sys_int86				     *
 *===========================================================================*/
PUBLIC int sys_int86(reg86p)
struct reg86u *reg86p;
{
    message m;
    int result;

    m.m1_p1= (char *)reg86p;

    result = _taskcall(SYSTASK, SYS_INT86, &m);
    return(result);
}

