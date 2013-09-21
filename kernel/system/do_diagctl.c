/* The kernel call implemented in this file:
 *   m_type:	SYS_DIAGCTL
 *
 * The parameters for this kernel call are:
 *  	DIAGCTL_CODE	request
 * and then request-specific arguments in DIAGCTL_ARG1 and DIAGCTL_ARG2.
 */

#include "kernel/system.h"


/*===========================================================================*
 *			        do_diagctl				     *
 *===========================================================================*/
int do_diagctl(struct proc * caller, message * m_ptr)
{
  vir_bytes len, buf;
  static char mybuf[DIAG_BUFSIZE];
  int s, i, proc_nr;

  switch (m_ptr->DIAGCTL_CODE) {
    case DIAGCTL_CODE_DIAG:
        buf = (vir_bytes) m_ptr->DIAGCTL_ARG1;
        len = (vir_bytes) m_ptr->DIAGCTL_ARG2;
	if(len < 1 || len > DIAG_BUFSIZE) {
		printf("do_diagctl: diag for %d: len %d out of range\n",
			caller->p_endpoint, len);
		return EINVAL;
	}
	if((s=data_copy_vmcheck(caller, caller->p_endpoint, buf, KERNEL,
					(vir_bytes) mybuf, len)) != OK) {
		printf("do_diagctl: diag for %d: len %d: copy failed: %d\n",
			caller->p_endpoint, len, s);
		return s;
	}
	for(i = 0; i < len; i++)
		kputc(mybuf[i]);
	kputc(END_OF_KMESS);
	return OK;
    case DIAGCTL_CODE_STACKTRACE:
	if(!isokendpt(m_ptr->DIAGCTL_ARG2, &proc_nr))
		return EINVAL;
	proc_stacktrace(proc_addr(proc_nr));
	return OK;
    case DIAGCTL_CODE_REGISTER:
	if (!(priv(caller)->s_flags & SYS_PROC))
		return EPERM;
	priv(caller)->s_diag_sig = TRUE;
	/* If the message log is not empty, send a first notification
	 * immediately. After bootup the log is basically never empty.
	 */
	if (kmess.km_size > 0 && !kinfo.do_serial_debug)
		send_sig(caller->p_endpoint, SIGKMESS);
	return OK;
    case DIAGCTL_CODE_UNREGISTER:
	if (!(priv(caller)->s_flags & SYS_PROC))
		return EPERM;
	priv(caller)->s_diag_sig = FALSE;
	return OK;
    default:
	printf("do_diagctl: invalid request %d\n", m_ptr->DIAGCTL_CODE);
        return(EINVAL);
  }
}

