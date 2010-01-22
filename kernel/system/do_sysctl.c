/* The kernel call implemented in this file:
 *   m_type:	SYS_SYSCTL
 *
 * The parameters for this kernel call are:
 *  	SYSCTL_CODE	request
 * and then request-specific arguments in SYSCTL_ARG1 and SYSCTL_ARG2.
 */

#include "../system.h"


/*===========================================================================*
 *			        do_sysctl				     *
 *===========================================================================*/
PUBLIC int do_sysctl(m_ptr)
register message *m_ptr;	/* pointer to request message */
{
  vir_bytes len, buf;
  static char mybuf[DIAG_BUFSIZE];
  struct proc *caller;
  int s, i, proc_nr;

  caller = proc_addr(who_p);

  switch (m_ptr->SYSCTL_CODE) {
    case SYSCTL_CODE_DIAG:
        buf = (vir_bytes) m_ptr->SYSCTL_ARG1;
        len = (vir_bytes) m_ptr->SYSCTL_ARG2;
	if(len < 1 || len > DIAG_BUFSIZE) {
		kprintf("do_sysctl: diag for %d: len %d out of range\n",
			caller->p_endpoint, len);
		return EINVAL;
	}
	if((s=data_copy_vmcheck(who_e, buf, SYSTEM, (vir_bytes) mybuf, len)) != OK) {
		kprintf("do_sysctl: diag for %d: len %d: copy failed: %d\n",
			caller->p_endpoint, len, s);
		return s;
	}
	for(i = 0; i < len; i++)
		kputc(mybuf[i]);
	kputc(END_OF_KMESS);
	return OK;
    case SYSCTL_CODE_STACKTRACE:
	if(!isokendpt(m_ptr->SYSCTL_ARG2, &proc_nr))
		return EINVAL;
	proc_stacktrace(proc_addr(proc_nr));
	return OK;
    default:
	kprintf("do_sysctl: invalid request %d\n", m_ptr->SYSCTL_CODE);
        return(EINVAL);
  }

  minix_panic("do_sysctl: can't happen", NO_NUM);

  return(OK);
}

