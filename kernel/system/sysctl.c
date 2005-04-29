
/* The system call implemented in this file:
 *   m_type:	SYS_EXIT
 *
 * The parameters for this system call are:
 *    m1_i1:	EXIT_STATUS	(exit status, 0 if normal exit)
 *
 * Author:
 *    Jorrit N. Herder <jnherder@cs.vu.nl>
 */

#include "../kernel.h"
#include "../system.h"
#include "../protect.h"
#include <sys/svrctl.h>
#include "../sendmask.h"

/*===========================================================================*
 * 				   do_exit				     *
 *===========================================================================*/
PUBLIC int do_exit(m_ptr)
message *m_ptr;			/* pointer to request message */
{
/* Handle sys_exit. A server or driver wants to exit. This may happen
 * on a panic, but also is done when MINIX is shutdown.
 */
  register struct proc *rp;
  int proc_nr = m_ptr->m_source;	/* can only exit own process */

  if (m_ptr->EXIT_STATUS != 0) {
      kprintf("WARNING: system process %d exited with an error.\n", proc_nr );
  }

  /* Now call the routine to clean up of the process table slot. This cancels
   * outstanding timers, possibly removes the process from the message queues,
   * and reset important process table fields.
   */
  clear_proc(proc_nr);

  /* If the shutdown sequence is active, see if it was awaiting the shutdown
   * of this system service. If so, directly continue the stop sequence. 
   */
  if (shutting_down && shutdown_process == proc_addr(proc_nr)) {
      stop_sequence(&shutdown_timer);
  }
  return(EDONTREPLY);			/* no reply is sent */
}



/* The system call implemented in this file:
 *   m_type:	SYS_SVRCTL
 *
 * The parameters for this system call are:
 *    m2_i1:	CTL_PROC_NR 	(process number of caller)	
 *    m2_i2:	CTL_REQUEST	(request type)	
 *    m2_i3:	CTL_MM_PRIV 	(privilege)
 *    m2_l1:    CTL_SEND_MASK   (new send mask to be installed)
 *    m2_l2:    CTL_PROC_TYPE   (new process type)
 *    m2_p1:	CTL_ARG_PTR 	(argument pointer)
 */


/*===========================================================================*
 *				do_svrctl				     *
 *===========================================================================*/
PUBLIC int do_svrctl(m_ptr)
message *m_ptr;			/* pointer to request message */
{
  register struct proc *rp;
  int proc_nr, priv;
  int request;
  vir_bytes argp;

  /* Extract message parameters. */
  proc_nr = m_ptr->CTL_PROC_NR;
  if (proc_nr == SELF) proc_nr = m_ptr->m_source;
  if (! isokprocn(proc_nr)) return(EINVAL);

  request = m_ptr->CTL_REQUEST;
  priv = m_ptr->CTL_MM_PRIV;
  argp = (vir_bytes) m_ptr->CTL_ARG_PTR;
  rp = proc_addr(proc_nr);

  /* Check if the PM privileges are super user. */
  if (!priv || !isuserp(rp)) 
      return(EPERM);

  /* See what is requested and handle the request. */
  switch (request) {
  case SYSSIGNON: {
	/* Make this process a server. The system processes should be able
	 * to communicate with this new server, so update their send masks
	 * as well.
	 */
  	/* fall through */
  }
  case SYSSENDMASK: {
	rp->p_type = P_SERVER;
	rp->p_sendmask = ALLOW_ALL_MASK;
	send_mask_allow(proc_addr(RTL8139)->p_sendmask, proc_nr);
	send_mask_allow(proc_addr(PM_PROC_NR)->p_sendmask, proc_nr);
	send_mask_allow(proc_addr(FS_PROC_NR)->p_sendmask, proc_nr);
	send_mask_allow(proc_addr(IS_PROC_NR)->p_sendmask, proc_nr);
	send_mask_allow(proc_addr(CLOCK)->p_sendmask, proc_nr);
	send_mask_allow(proc_addr(SYSTASK)->p_sendmask, proc_nr);
	return(OK); 
  }
  default:
	return(EINVAL);
  }
}

/* The system call implemented in this file:
 *   m_type:	SYS_SEGCTL
 *
 * The parameters for this system call are:
 *    m4_l3:	SEG_PHYS	(physical base address)
 *    m4_l4:	SEG_SIZE	(size of segment)
 *    m4_l1:	SEG_SELECT	(return segment selector here)
 *    m4_l2:	SEG_OFFSET	(return offset within segment here)
 *    m4_l5:	SEG_INDEX	(return index into remote memory map here)
 *
 * Author:
 *    Jorrit N. Herder <jnherder@cs.vu.nl>
 */


/*===========================================================================*
 *			        do_segctl				     *
 *===========================================================================*/
PUBLIC int do_segctl(m_ptr)
register message *m_ptr;	/* pointer to request message */
{
/* Return a segment selector and offset that can be used to reach a physical
 * address, for use by a driver doing memory I/O in the A0000 - DFFFF range.
 */
  u16_t selector;
  vir_bytes offset;
  int i, index;
  register struct proc *rp;
  phys_bytes phys = (phys_bytes) m_ptr->SEG_PHYS;
  vir_bytes size = (vir_bytes) m_ptr->SEG_SIZE;
  int result;

  /* First check if there is a slot available for this segment. */
  rp = proc_addr(m_ptr->m_source);
  index = -1;
  for (i=0; i < NR_REMOTE_SEGS; i++) {
  	if (! rp->p_farmem[i].in_use) {
  		index = i; 
  		rp->p_farmem[i].in_use = TRUE;
  		rp->p_farmem[i].mem_phys = phys;
  		rp->p_farmem[i].mem_len = size;
  		break;
  	}
  }
  if (index < 0) return(ENOSPC);


  if (! machine.protected) {
      selector = phys / HCLICK_SIZE;
      offset = phys % HCLICK_SIZE;
      result = OK;
  } else {
      /* Check if the segment size can be recorded in bytes, that is, check
       * if descriptor's limit field can delimited the allowed memory region
       * precisely. This works up to 1MB. If the size is larger, 4K pages
       * instead of bytes are used.
       */
      if (size < BYTE_GRAN_MAX) {
          init_dataseg(&rp->p_ldt[EXTRA_LDT_INDEX+i], phys, size, 
          	USER_PRIVILEGE);
          selector = ((EXTRA_LDT_INDEX+i)*0x08) | (1*0x04) | USER_PRIVILEGE;
          offset = 0;
          result = OK;			
      } else {
          init_dataseg(&rp->p_ldt[EXTRA_LDT_INDEX+i], phys & ~0xFFFF, 0, 
          	USER_PRIVILEGE);
          selector = ((EXTRA_LDT_INDEX+i)*0x08) | (1*0x04) | USER_PRIVILEGE;
          offset = phys & 0xFFFF;
          result = OK;
      }
  }

  /* Request successfully done. Now return the result. */
  m_ptr->SEG_INDEX = index | REMOTE_SEG;
  m_ptr->SEG_SELECT = selector;
  m_ptr->SEG_OFFSET = offset;
  return(result);
}


/* The system call implemented in this file:
 *   m_type:	SYS_IOPENABLE
 *
 * The parameters for this system call are:
 *    m2_i2:	PROC_NR		(process to give I/O Protection Level bits)
 *
 * Author:
 *    Jorrit N. Herder <jnherder@cs.vu.nl>
 */

/*===========================================================================*
 *			        do_iopenable				     *
 *===========================================================================*/
PUBLIC int do_iopenable(m_ptr)
register message *m_ptr;	/* pointer to request message */
{
#if ENABLE_USERPRIV && ENABLE_USERIOPL
  enable_iop(proc_addr(m_ptr->PROC_NR)); 
  return(OK);
#else
  return(EPERM);
#endif
}


/* The system call implemented in this file:
 *   m_type:	SYS_KMALLOC
 *
 * The parameters for this system call are:
 *    m4_l2:	MEM_CHUNK_SIZE	(request a buffer of this size)
 *    m4_l1:	MEM_CHUNK_BASE 	(return physical address on success)	
 *
 * Author:
 *    Jorrit N. Herder <jnherder@cs.vu.nl>
 */

/*===========================================================================*
 *			        do_kmalloc				     *
 *===========================================================================*/
PUBLIC int do_kmalloc(m_ptr)
register message *m_ptr;	/* pointer to request message */
{
/* Request a (DMA) buffer to be allocated in one of the memory chunks. */
  phys_clicks tot_clicks;
  struct memory *memp;
  
  tot_clicks = (m_ptr->MEM_CHUNK_SIZE + CLICK_SIZE-1) >> CLICK_SHIFT;
  memp = &mem[NR_MEMS];
  while ((--memp)->size < tot_clicks) {
      if (memp == mem) {
          return(ENOMEM);
      }
  }
  memp->size -= tot_clicks;
  m_ptr->MEM_CHUNK_BASE = (memp->base + memp->size) << CLICK_SHIFT; 
  return(OK);
}

