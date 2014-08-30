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
#include <sys/ptrace.h>

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
		printf("Can't copy in sys_trace: %d\n", r);\
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
		printf("Can't copy in sys_trace: %d\n", r);\
		return r;\
	}  \
}

  if(!isokendpt(tr_proc_nr_e, &tr_proc_nr)) return(EINVAL);
  if (iskerneln(tr_proc_nr)) return(EPERM);

  rp = proc_addr(tr_proc_nr);
  if (isemptyp(rp)) return(EINVAL);
  switch (tr_request) {
  case T_STOP:			/* stop process */
	RTS_SET(rp, RTS_P_STOP);
	/* clear syscall trace and single step flags */
	rp->p_misc_flags &= ~(MF_SC_TRACE | MF_STEP);
	return(OK);

  case T_GETINS:		/* return value from instruction space */
	COPYFROMPROC(tr_addr, (vir_bytes) &tr_data, sizeof(long));
	m_ptr->m_krn_lsys_sys_trace.data = tr_data;
	break;

  case T_GETDATA:		/* return value from data space */
	COPYFROMPROC(tr_addr, (vir_bytes) &tr_data, sizeof(long));
	m_ptr->m_krn_lsys_sys_trace.data= tr_data;
	break;

  case T_GETUSER:		/* return value from process table */
	if ((tr_addr & (sizeof(long) - 1)) != 0) return(EFAULT);

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

	if (tr_addr > sizeof(struct priv) - sizeof(long)) return(EFAULT);

	m_ptr->m_krn_lsys_sys_trace.data =
	    *(long *) ((char *) rp->p_priv + (int) tr_addr);
	break;

  case T_SETINS:		/* set value in instruction space */
	COPYTOPROC(tr_addr, (vir_bytes) &tr_data, sizeof(long));
	m_ptr->m_krn_lsys_sys_trace.data = 0;
	break;

  case T_SETDATA:			/* set value in data space */
	COPYTOPROC(tr_addr, (vir_bytes) &tr_data, sizeof(long));
	m_ptr->m_krn_lsys_sys_trace.data = 0;
	break;

  case T_SETUSER:			/* set value in process table */
	if ((tr_addr & (sizeof(reg_t) - 1)) != 0 ||
	     tr_addr > sizeof(struct stackframe_s) - sizeof(reg_t))
		return(EFAULT);
	i = (int) tr_addr;
#if defined(__i386__)
	/* Altering segment registers might crash the kernel when it
	 * tries to load them prior to restarting a process, so do
	 * not allow it.
	 */
	if (i == (int) &((struct proc *) 0)->p_reg.cs ||
	    i == (int) &((struct proc *) 0)->p_reg.ds ||
	    i == (int) &((struct proc *) 0)->p_reg.es ||
	    i == (int) &((struct proc *) 0)->p_reg.gs ||
	    i == (int) &((struct proc *) 0)->p_reg.fs ||
	    i == (int) &((struct proc *) 0)->p_reg.ss)
		return(EFAULT);

	if (i == (int) &((struct proc *) 0)->p_reg.psw)
		/* only selected bits are changeable */
		SETPSW(rp, tr_data);
	else
		*(reg_t *) ((char *) &rp->p_reg + i) = (reg_t) tr_data;
#elif defined(__arm__)
	if (i == (int) &((struct proc *) 0)->p_reg.psr) {
		/* only selected bits are changeable */
		SET_USR_PSR(rp, tr_data);
	} else {
		*(reg_t *) ((char *) &rp->p_reg + i) = (reg_t) tr_data;
	}
#endif
	m_ptr->m_krn_lsys_sys_trace.data = 0;
	break;

  case T_DETACH:		/* detach tracer */
	rp->p_misc_flags &= ~MF_SC_ACTIVE;

	/* fall through */
  case T_RESUME:		/* resume execution */
	RTS_UNSET(rp, RTS_P_STOP);
	m_ptr->m_krn_lsys_sys_trace.data = 0;
	break;

  case T_STEP:			/* set trace bit */
	rp->p_misc_flags |= MF_STEP;
	RTS_UNSET(rp, RTS_P_STOP);
	m_ptr->m_krn_lsys_sys_trace.data = 0;
	break;

  case T_SYSCALL:		/* trace system call */
	rp->p_misc_flags |= MF_SC_TRACE;
	RTS_UNSET(rp, RTS_P_STOP);
	m_ptr->m_krn_lsys_sys_trace.data = 0;
	break;

  case T_READB_INS:		/* get value from instruction space */
	COPYFROMPROC(tr_addr, (vir_bytes) &ub, 1);
	m_ptr->m_krn_lsys_sys_trace.data = ub;
	break;

  case T_WRITEB_INS:		/* set value in instruction space */
	ub = (unsigned char) (tr_data & 0xff);
	COPYTOPROC(tr_addr, (vir_bytes) &ub, 1);
	m_ptr->m_krn_lsys_sys_trace.data = 0;
	break;

  default:
	return(EINVAL);
  }
  return(OK);
}

#endif /* USE_TRACE */
