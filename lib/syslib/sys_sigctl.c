#include "syslib.h"

/*===========================================================================*
 *                                sys_sigctl				     *
 *===========================================================================*/
PUBLIC int sys_sigctl(request, proc_nr, sig_ctxt, flags, k_proc_nr, k_sig_map)
int request;				/* control operation requested */
int proc_nr;				/* for which process */
struct sigmsg *sig_ctxt;		/* POSIX style handling */
int flags;				/* flags for POSIX handling */
int *k_proc_nr;				/* return process number here */
sigset_t *k_sig_map;			/* return signal map here */
{
    message m;
    int result;

    m.m_type = SYS_SIGCTL;
    m.SIG_REQUEST = request;
    m.SIG_PROC = proc_nr;
    m.SIG_FLAGS = flags;
    m.SIG_CTXT_PTR = (char *) sig_ctxt;
    result = _taskcall(SYSTASK, SYS_SIGCTL, &m);
    if (request == S_GETSIG && k_proc_nr != 0 && k_sig_map != 0) {
    	*k_proc_nr = m.SIG_PROC;
    	*k_sig_map = (sigset_t) m.SIG_MAP;
    }
    return(result);
}

