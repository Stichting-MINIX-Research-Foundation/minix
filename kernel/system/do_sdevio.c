/* The kernel call implemented in this file:
 *   m_type:	SYS_SDEVIO
 *
 * The parameters for this kernel call are:
 *    m2_i3:	DIO_REQUEST	(request input or output)	
 *    m2_i1:	DIO_TYPE	(flag indicating byte, word, or long)
 *    m2_l1:	DIO_PORT	(port to read/ write)	
 *    m2_p1:	DIO_VEC_ADDR	(virtual address of buffer)	
 *    m2_l2:	DIO_VEC_SIZE	(number of elements)	
 *    m2_i2:	DIO_VEC_PROC	(process where buffer is)	
 */

#include "../system.h"
#include <minix/devio.h>
#include <minix/endpoint.h>

#if USE_SDEVIO

/*===========================================================================*
 *			        do_sdevio                                    *
 *===========================================================================*/
PUBLIC int do_sdevio(m_ptr)
register message *m_ptr;	/* pointer to request message */
{
  int proc_nr, proc_nr_e = m_ptr->DIO_VEC_ENDPT;
  int count = m_ptr->DIO_VEC_SIZE;
  long port = m_ptr->DIO_PORT;
  phys_bytes phys_buf;
  int req_type, req_dir;

  /* Check if process endpoint is OK. 
   * A driver may directly provide a pointer to a buffer at the user-process
   * that initiated the device I/O. Kernel processes, of course, are denied.
   */
  if (proc_nr_e == SELF)
	proc_nr = who_p;
  else
	if(!isokendpt(proc_nr_e, &proc_nr))
		return(EINVAL);
  if (iskerneln(proc_nr)) return(EPERM);

  /* Extract direction (in or out) and type (size). */
  req_dir = m_ptr->DIO_REQUEST & _DIO_DIRMASK;
  req_type = m_ptr->DIO_REQUEST & _DIO_TYPEMASK;

  /* Check for 'safe' variants. */
  if((m_ptr->DIO_REQUEST & _DIO_SAFEMASK) == _DIO_SAFE) {
     /* Map grant address to physical address. */
     if ((phys_buf = umap_verify_grant(proc_addr(proc_nr), who_e,
	(vir_bytes) m_ptr->DIO_VEC_ADDR,
	(vir_bytes) m_ptr->DIO_OFFSET, count,
	req_dir == _DIO_INPUT ? CPF_WRITE : CPF_READ)) == 0)
         return(EPERM);
  } else {
     if(proc_nr != who_p)
	kprintf("unsafe sdevio by %d in %d\n", who_e, proc_nr_e);
     /* Get and check physical address. */
     if ((phys_buf = numap_local(proc_nr,
	 (vir_bytes) m_ptr->DIO_VEC_ADDR, count)) == 0)
         return(EFAULT);
  }

  /* Perform device I/O for bytes and words. Longs are not supported. */
  if (req_dir == _DIO_INPUT) { 
      switch (req_type) {
      case _DIO_BYTE: phys_insb(port, phys_buf, count); break; 
      case _DIO_WORD: phys_insw(port, phys_buf, count); break; 
      default: return(EINVAL);
      } 
  } else if (req_dir == _DIO_OUTPUT) { 
      switch (req_type) {
      case _DIO_BYTE: phys_outsb(port, phys_buf, count); break; 
      case _DIO_WORD: phys_outsw(port, phys_buf, count); break; 
      default: return(EINVAL);
      } 
  }
  else {
      return(EINVAL);
  }
  return(OK);
}

#endif /* USE_SDEVIO */

