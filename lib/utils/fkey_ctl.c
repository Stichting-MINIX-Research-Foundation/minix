#include "utils.h" 

/*===========================================================================*
 *				fkey_ctl				     *
 *===========================================================================*/
PUBLIC int fkey_ctl(fkey_code, enable_disable)
int fkey_code;				/* function key code it concerns */
int enable_disable;			/* enable or disable notifications */
{
/* Send a message to the TTY server to request notifications for function 
 * key presses or to disable notifications. Enabling succeeds unless the key
 * is already bound to another process. Disabling only succeeds if the key is
 * bound to the current process.   
 */ 
    message m;
    m.FKEY_CODE = fkey_code;
    m.FKEY_ENABLE = enable_disable;
    return(_taskcall(TTY, FKEY_CONTROL, &m));
}


