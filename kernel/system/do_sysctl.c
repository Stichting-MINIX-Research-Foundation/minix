/* The kernel call implemented in this file:
 *   m_type:	SYS_SYSCTL
 *
 * The parameters for this kernel call are:
 *  	SYSCTL_CODE	request
 * and then request-specific arguments in SYSCTL_ARG1 and SYSCTL_ARG2.
 */

#include "kernel/system.h"


/*===========================================================================*
 *			        do_sysctl				     *
 *===========================================================================*/
int do_sysctl(struct proc * caller, message * m_ptr)
{
  vir_bytes len, buf;
  static char mybuf[DIAG_BUFSIZE];
  int s, i, proc_nr;

  switch (m_ptr->SYSCTL_CODE) {
    case SYSCTL_CODE_DIAG:
        buf = (vir_bytes) m_ptr->SYSCTL_ARG1;
        len = (vir_bytes) m_ptr->SYSCTL_ARG2;
	if(len < 1 || len > DIAG_BUFSIZE) {
		printf("do_sysctl: diag for %d: len %d out of range\n",
			caller->p_endpoint, len);
		return EINVAL;
	}
	if((s=data_copy_vmcheck(caller, caller->p_endpoint, buf, KERNEL,
					(vir_bytes) mybuf, len)) != OK) {
		printf("do_sysctl: diag for %d: len %d: copy failed: %d\n",
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
	printf("do_sysctl: invalid request %d\n", m_ptr->SYSCTL_CODE);
        return(EINVAL);
  }
}

