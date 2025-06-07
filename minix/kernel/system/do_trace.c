/* The kernel call implemented in this file:
 *   m_type:	SYS_TRACE
 *
 * The parameters for this kernel call are:
 *   m_lsys_krn_sys_trace.endpt		process that is traced
 *   m_lsys_krn_sys_trace.request	trace request
 *   m_lsys_krn_sys_trace.address	address at traced process' space
 *   m_lsys_krn_sys_trace.data		data to be written
 *   m_krn_lsys_sys_trace.data		data to be returned
 */

#include "kernel/system.h"
// #include <sys/ptrace.h> // Removed

// Added kernel headers
#include <minix/kernel_types.h> // For k_errno_t and potentially ptrace constants if moved
#include <klib/include/kprintf.h>
#include <klib/include/kstring.h>
#include <klib/include/kmemory.h>


#if USE_TRACE

/*==========================================================================*
 *				do_trace				    *
 *==========================================================================*/
int do_trace(struct proc * caller, message * m_ptr)
{
/* Handle the debugging commands supported by the ptrace system call
 * The commands are:
 * T_STOP	stop the process
 * T_OK		enable tracing by parent for this process
 * T_GETINS	return value from instruction space
 * T_GETDATA	return value from data space
 * T_GETUSER	return value from user process table
 * T_SETINS	set value in instruction space
 * T_SETDATA	set value in data space
 * T_SETUSER	set value in user process table
 * T_RESUME	resume execution
 * T_EXIT	exit
 * T_STEP	set trace bit
 * T_SYSCALL	trace system call
 * T_ATTACH	attach to an existing process
 * T_DETACH	detach from a traced process
 * T_SETOPT	set trace options
 * T_GETRANGE	get range of values
 * T_SETRANGE	set range of values
 *
 * The T_OK, T_ATTACH, T_EXIT, and T_SETOPT commands are handled completely by
 * the process manager. T_GETRANGE and T_SETRANGE use sys_vircopy(). All others
 * come here.
 */

  register struct proc *rp;
  vir_bytes tr_addr = m_ptr->m_lsys_krn_sys_trace.address;
  long tr_data = m_ptr->m_lsys_krn_sys_trace.data;
  int tr_request = m_ptr->m_lsys_krn_sys_trace.request;
  int tr_proc_nr_e = m_ptr->m_lsys_krn_sys_trace.endpt, tr_proc_nr;
  unsigned char ub;
  int i;

#define COPYTOPROC(addr, myaddr, length) {		\
	struct vir_addr fromaddr, toaddr;		\
	int r;	\
	fromaddr.proc_nr_e = KERNEL;			\
	toaddr.proc_nr_e = tr_proc_nr_e;		\
	fromaddr.offset = (myaddr);			\
	toaddr.offset = (addr);				\
	if((r=virtual_copy_vmcheck(caller, &fromaddr,	\
			&toaddr, length)) != OK) {	\
		kprintf_stub("Can't copy in sys_trace: %d\n", r); /* MODIFIED */ \
		return r;\
	}  \
}

#define COPYFROMPROC(addr, myaddr, length) {	\
	struct vir_addr fromaddr, toaddr;		\
	int r;	\
	fromaddr.proc_nr_e = tr_proc_nr_e;		\
	toaddr.proc_nr_e = KERNEL;			\
	fromaddr.offset = (addr);			\
	toaddr.offset = (myaddr);			\
	if((r=virtual_copy_vmcheck(caller, &fromaddr,	\
			&toaddr, length)) != OK) {	\
		kprintf_stub("Can't copy in sys_trace: %d\n", r); /* MODIFIED */ \
		return r;\
	}  \
}

  if(!isokendpt(tr_proc_nr_e, &tr_proc_nr)) return(EINVAL); // EINVAL might be undefined
  if (iskerneln(tr_proc_nr)) return(EPERM); // EPERM might be undefined

  rp = proc_addr(tr_proc_nr);
  if (isemptyp(rp)) return(EINVAL); // EINVAL might be undefined
  switch (tr_request) {
  // FIXME: T_* constants below will be undefined after removing <sys/ptrace.h>
  case 0: /* T_STOP placeholder */			/* stop process */
	RTS_SET(rp, RTS_P_STOP);
	/* clear syscall trace and single step flags */
	rp->p_misc_flags &= ~(MF_SC_TRACE | MF_STEP);
	return(OK);

  case 1: /* T_GETINS placeholder */		/* return value from instruction space */
	COPYFROMPROC(tr_addr, (vir_bytes) &tr_data, sizeof(long));
	m_ptr->m_krn_lsys_sys_trace.data = tr_data;
	break;

  case 2: /* T_GETDATA placeholder */		/* return value from data space */
	COPYFROMPROC(tr_addr, (vir_bytes) &tr_data, sizeof(long));
	m_ptr->m_krn_lsys_sys_trace.data= tr_data;
	break;

  case 3: /* T_GETUSER placeholder */		/* return value from process table */
	if ((tr_addr & (sizeof(long) - 1)) != 0) return(EFAULT); // EFAULT might be undefined

	if (tr_addr <= sizeof(struct proc) - sizeof(long)) {
		m_ptr->m_krn_lsys_sys_trace.data =
		    *(long *) ((char *) rp + (int) tr_addr);
		break;
	}

	/* The process's proc struct is followed by its priv struct.
	 * The alignment here should be unnecessary, but better safe..
	 */
	i = sizeof(long) - 1;
	tr_addr -= (sizeof(struct proc) + i) & ~i;

	if (tr_addr > sizeof(struct priv) - sizeof(long)) return(EFAULT); // EFAULT might be undefined

	m_ptr->m_krn_lsys_sys_trace.data =
	    *(long *) ((char *) rp->p_priv + (int) tr_addr);
	break;

  case 4: /* T_SETINS placeholder */		/* set value in instruction space */
	COPYTOPROC(tr_addr, (vir_bytes) &tr_data, sizeof(long));
	m_ptr->m_krn_lsys_sys_trace.data = 0;
	break;

  case 5: /* T_SETDATA placeholder */			/* set value in data space */
	COPYTOPROC(tr_addr, (vir_bytes) &tr_data, sizeof(long));
	m_ptr->m_krn_lsys_sys_trace.data = 0;
	break;

  case 6: /* T_SETUSER placeholder */			/* set value in process table */
	if ((tr_addr & (sizeof(reg_t) - 1)) != 0 ||
	     tr_addr > sizeof(struct stackframe_s) - sizeof(reg_t))
		return(EFAULT); // EFAULT might be undefined
	i = (int) tr_addr;
#if defined(__i386__)
	/* Altering segment registers might crash the kernel when it
	 * tries to load them prior to restarting a process, so do
	 * not allow it.
	 */
	if (i == K_OFFSETOF(struct proc, p_reg.cs) ||
	    i == K_OFFSETOF(struct proc, p_reg.ds) ||
	    i == K_OFFSETOF(struct proc, p_reg.es) ||
	    i == K_OFFSETOF(struct proc, p_reg.gs) ||
	    i == K_OFFSETOF(struct proc, p_reg.fs) ||
	    i == K_OFFSETOF(struct proc, p_reg.ss))
		return(EFAULT); // EFAULT might be undefined

	if (i == K_OFFSETOF(struct proc, p_reg.psw))
		/* only selected bits are changeable */
		SETPSW(rp, tr_data);
	else
		*(reg_t *) ((char *) &rp->p_reg + i) = (reg_t) tr_data;
#elif defined(__arm__)
	if (i == K_OFFSETOF(struct proc, p_reg.psr)) {
		/* only selected bits are changeable */
		SET_USR_PSR(rp, tr_data);
	} else {
		*(reg_t *) ((char *) &rp->p_reg + i) = (reg_t) tr_data;
	}
#endif
	m_ptr->m_krn_lsys_sys_trace.data = 0;
	break;

  case 7: /* T_DETACH / T_RESUME placeholder */		/* detach tracer / resume execution */
	rp->p_misc_flags &= ~MF_SC_ACTIVE;
	RTS_UNSET(rp, RTS_P_STOP);
	m_ptr->m_krn_lsys_sys_trace.data = 0;
	break;

  case 9: /* T_STEP placeholder */			/* set trace bit */
	rp->p_misc_flags |= MF_STEP;
	RTS_UNSET(rp, RTS_P_STOP);
	m_ptr->m_krn_lsys_sys_trace.data = 0;
	break;

  case 10: /* T_SYSCALL placeholder */		/* trace system call */
	rp->p_misc_flags |= MF_SC_TRACE;
	RTS_UNSET(rp, RTS_P_STOP);
	m_ptr->m_krn_lsys_sys_trace.data = 0;
	break;

  case 11: /* T_READB_INS placeholder */		/* get value from instruction space */
	COPYFROMPROC(tr_addr, (vir_bytes) &ub, 1);
	m_ptr->m_krn_lsys_sys_trace.data = ub;
	break;

  case 12: /* T_WRITEB_INS placeholder */		/* set value in instruction space */
	ub = (unsigned char) (tr_data & 0xff);
	COPYTOPROC(tr_addr, (vir_bytes) &ub, 1);
	m_ptr->m_krn_lsys_sys_trace.data = 0;
	break;

  default:
	return(EINVAL); // EINVAL might be undefined
  }
  return(OK);
}

#endif /* USE_TRACE */
