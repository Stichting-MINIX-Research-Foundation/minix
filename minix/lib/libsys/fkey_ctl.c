#include "sysutil.h" 
#include <string.h>

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
    memset(&m, 0, sizeof(m));
    m.m_lsys_tty_fkey_ctl.request = request;
    m.m_lsys_tty_fkey_ctl.fkeys = (fkeys) ? *fkeys : 0;
    m.m_lsys_tty_fkey_ctl.sfkeys = (sfkeys) ? *sfkeys : 0;
    s = _taskcall(TTY_PROC_NR, TTY_FKEY_CONTROL, &m);
    if (fkeys) *fkeys = m.m_tty_lsys_fkey_ctl.fkeys;
    if (sfkeys) *sfkeys = m.m_tty_lsys_fkey_ctl.sfkeys;
    return(s);
}


