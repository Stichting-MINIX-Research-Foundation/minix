#include "syslib.h"

/*===========================================================================*
 *                                sys_getinfo				     *
 *===========================================================================*/
PUBLIC int sys_getinfo(request, val_ptr, val_len, key_ptr, key_len)
int request; 				/* system info requested */
void *val_ptr;				/* pointer where to store it */
int val_len;				/* max length of value to get */
void *key_ptr;				/* pointer to key requested */
int key_len;				/* length of key */ 
{
    message m;

    m.I_REQUEST = request;
    m.I_PROC_NR = SELF;			/* always store values at caller */
    m.I_VAL_PTR = val_ptr;
    m.I_VAL_LEN = val_len;
    m.I_KEY_PTR = key_ptr;
    m.I_KEY_LEN = key_len;

    return(_taskcall(SYSTASK, SYS_GETINFO, &m));
}


