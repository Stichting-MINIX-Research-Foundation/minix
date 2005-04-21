#include "../kernel.h"
#include "../system.h"

PUBLIC int do_kill(m_ptr)
message *m_ptr;			/* pointer to request message */
{
/* Handle sys_kill(). Cause a signal to be sent to a process via MM.
 * Note that this has nothing to do with the kill (2) system call, this
 * is how the FS (and possibly other servers) get access to cause_sig. 
 */
  cause_sig(m_ptr->SIG_PROC, m_ptr->SIG_NUMBER);
  return(OK);
}


