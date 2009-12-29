/* The kernel call implemented in this file:
 *   m_type:	SYS_TRACE
 *
 * The parameters for this kernel call are:
 *    m2_i1:	CTL_ENDPT	process that is traced
 *    m2_i2:    CTL_REQUEST	trace request
 *    m2_l1:    CTL_ADDRESS     address at traced process' space
 *    m2_l2:    CTL_DATA        data to be written or returned here
 */

#include "../system.h"
#include <sys/ptrace.h>

#if USE_TRACE

/*==========================================================================*
 *				do_trace				    *
 *==========================================================================*/
PUBLIC int do_trace(m_ptr)
register message *m_ptr;
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
  vir_bytes tr_addr = (vir_bytes) m_ptr->CTL_ADDRESS;
  long tr_data = m_ptr->CTL_DATA;
  int tr_request = m_ptr->CTL_REQUEST;
  int tr_proc_nr_e = m_ptr->CTL_ENDPT, tr_proc_nr;
  unsigned char ub;
  int i;

#define COPYTOPROC(seg, addr, myaddr, length) {		\
	struct vir_addr fromaddr, toaddr;		\
	int r;	\
	fromaddr.proc_nr_e = SYSTEM;			\
	toaddr.proc_nr_e = tr_proc_nr_e;		\
	fromaddr.offset = (myaddr);			\
	toaddr.offset = (addr);				\
	fromaddr.segment = D;				\
	toaddr.segment = (seg);				\
	if((r=virtual_copy_vmcheck(&fromaddr, &toaddr, length)) != OK) { \
		printf("Can't copy in sys_trace: %d\n", r);\
		return r;\
	}  \
}

#define COPYFROMPROC(seg, addr, myaddr, length) {	\
	struct vir_addr fromaddr, toaddr;		\
	int r;	\
	fromaddr.proc_nr_e = tr_proc_nr_e;		\
	toaddr.proc_nr_e = SYSTEM;			\
	fromaddr.offset = (addr);			\
	toaddr.offset = (myaddr);			\
	fromaddr.segment = (seg);			\
	toaddr.segment = D;				\
	if((r=virtual_copy_vmcheck(&fromaddr, &toaddr, length)) != OK) { \
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
	RTS_LOCK_SET(rp, RTS_P_STOP);
	rp->p_reg.psw &= ~TRACEBIT;	/* clear trace bit */
	rp->p_misc_flags &= ~MF_SC_TRACE;	/* clear syscall trace flag */
	return(OK);

  case T_GETINS:		/* return value from instruction space */
	COPYFROMPROC(T, tr_addr, (vir_bytes) &tr_data, sizeof(long));
	m_ptr->CTL_DATA = tr_data;
	break;

  case T_GETDATA:		/* return value from data space */
	COPYFROMPROC(D, tr_addr, (vir_bytes) &tr_data, sizeof(long));
	m_ptr->CTL_DATA= tr_data;
	break;

  case T_GETUSER:		/* return value from process table */
	if ((tr_addr & (sizeof(long) - 1)) != 0) return(EFAULT);

	if (tr_addr <= sizeof(struct proc) - sizeof(long)) {
		m_ptr->CTL_DATA = *(long *) ((char *) rp + (int) tr_addr);
		break;
	}

	/* The process's proc struct is followed by its priv struct.
	 * The alignment here should be unnecessary, but better safe..
	 */
	i = sizeof(long) - 1;
	tr_addr -= (sizeof(struct proc) + i) & ~i;

	if (tr_addr > sizeof(struct priv) - sizeof(long)) return(EFAULT);

	m_ptr->CTL_DATA = *(long *) ((char *) rp->p_priv + (int) tr_addr);
	break;

  case T_SETINS:		/* set value in instruction space */
	COPYTOPROC(T, tr_addr, (vir_bytes) &tr_data, sizeof(long));
	m_ptr->CTL_DATA = 0;
	break;

  case T_SETDATA:			/* set value in data space */
	COPYTOPROC(D, tr_addr, (vir_bytes) &tr_data, sizeof(long));
	m_ptr->CTL_DATA = 0;
	break;

  case T_SETUSER:			/* set value in process table */
	if ((tr_addr & (sizeof(reg_t) - 1)) != 0 ||
	     tr_addr > sizeof(struct stackframe_s) - sizeof(reg_t))
		return(EFAULT);
	i = (int) tr_addr;
#if (_MINIX_CHIP == _CHIP_INTEL)
	/* Altering segment registers might crash the kernel when it
	 * tries to load them prior to restarting a process, so do
	 * not allow it.
	 */
	if (i == (int) &((struct proc *) 0)->p_reg.cs ||
	    i == (int) &((struct proc *) 0)->p_reg.ds ||
	    i == (int) &((struct proc *) 0)->p_reg.es ||
#if _WORD_SIZE == 4
	    i == (int) &((struct proc *) 0)->p_reg.gs ||
	    i == (int) &((struct proc *) 0)->p_reg.fs ||
#endif
	    i == (int) &((struct proc *) 0)->p_reg.ss)
		return(EFAULT);
#endif
	if (i == (int) &((struct proc *) 0)->p_reg.psw)
		/* only selected bits are changeable */
		SETPSW(rp, tr_data);
	else
		*(reg_t *) ((char *) &rp->p_reg + i) = (reg_t) tr_data;
	m_ptr->CTL_DATA = 0;
	break;

  case T_DETACH:		/* detach tracer */
	rp->p_misc_flags &= ~MF_SC_ACTIVE;

	/* fall through */
  case T_RESUME:		/* resume execution */
	RTS_LOCK_UNSET(rp, RTS_P_STOP);
	m_ptr->CTL_DATA = 0;
	break;

  case T_STEP:			/* set trace bit */
	rp->p_reg.psw |= TRACEBIT;
	RTS_LOCK_UNSET(rp, RTS_P_STOP);
	m_ptr->CTL_DATA = 0;
	break;

  case T_SYSCALL:		/* trace system call */
	rp->p_misc_flags |= MF_SC_TRACE;
	RTS_LOCK_UNSET(rp, RTS_P_STOP);
	m_ptr->CTL_DATA = 0;
	break;

  case T_READB_INS:		/* get value from instruction space */
	COPYFROMPROC(T, tr_addr, (vir_bytes) &ub, 1);
	m_ptr->CTL_DATA = ub;
	break;

  case T_WRITEB_INS:		/* set value in instruction space */
	ub = (unsigned char) (tr_data & 0xff);
	COPYTOPROC(T, tr_addr, (vir_bytes) &ub, 1);
	m_ptr->CTL_DATA = 0;
	break;

  default:
	return(EINVAL);
  }
  return(OK);
}

#endif /* USE_TRACE */
