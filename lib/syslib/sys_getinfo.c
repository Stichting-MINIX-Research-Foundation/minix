#include "syslib.h"

/*===========================================================================*
 *                                sys_getinfo				     *
 *===========================================================================*/
PUBLIC int sys_getinfo(request, ptr, len, ptr2, len2)
int request; 				/* system info requested */
void *ptr;				/* pointer where to store it */
int len;				/* max length of value to get */
void *ptr2;				/* second pointer */
int len2;				/* length or process nr */ 
{
    message m;

    m.I_REQUEST = request;
    m.I_PROC_NR = SELF;			/* always store values at caller */
    m.I_VAL_PTR = ptr;
    m.I_VAL_LEN = len;
    m.I_VAL_PTR2 = ptr2;
    m.I_VAL_LEN2 = len2;

    return(_taskcall(SYSTASK, SYS_GETINFO, &m));
}


