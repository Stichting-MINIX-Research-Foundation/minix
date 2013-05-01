#include "sysutil.h" 

/*===========================================================================*
 *				fkey_ctl				     *
 *===========================================================================*/
int fkey_ctl(request, fkeys, sfkeys)
int request;				/* request to perform */
int *fkeys;				/* bit masks for F1-F12 keys */
int *sfkeys;				/* bit masks for Shift F1-F12 keys */
{
/* Send a message to the TTY server to request notifications for function 
 * key presses or to disable notifications. Enabling succeeds unless the key
 * is already bound to another process. Disabling only succeeds if the key is
 * bound to the current process.   
 */ 
    message m;
    int s;
    m.FKEY_REQUEST = request;
    m.FKEY_FKEYS = (fkeys) ? *fkeys : 0;
    m.FKEY_SFKEYS = (sfkeys) ? *sfkeys : 0;
    s = _taskcall(TTY_PROC_NR, TTY_FKEY_CONTROL, &m);
    if (fkeys) *fkeys = m.FKEY_FKEYS;
    if (sfkeys) *sfkeys = m.FKEY_SFKEYS;
    return(s);
}


