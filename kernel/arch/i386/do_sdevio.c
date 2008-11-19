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

#include "../../system.h"
#include <minix/devio.h>
#include <minix/endpoint.h>

#include "proto.h"

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
  int i, req_type, req_dir, io_type, size, nr_io_range;
  struct proc *rp;
  struct priv *privp;
  struct io_range *iorp;

  /* Allow safe copies and accesses to SELF */
  if ((m_ptr->DIO_REQUEST & _DIO_SAFEMASK) != _DIO_SAFE &&
	proc_nr_e != SELF)
  {
	static int first= 1;
	if (first)
	{
		first= 0;
		kprintf("do_sdevio: for %d, req %d\n",
			m_ptr->m_source, m_ptr->DIO_REQUEST);
	}
  }

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
     {
	kprintf("do_sdevio: unsafe sdevio by %d in %d denied\n",
		who_e, proc_nr_e);
	return EPERM;
     }
     /* Get and check physical address. */
     if ((phys_buf = umap_virtual(proc_addr(proc_nr), D,
	 (vir_bytes) m_ptr->DIO_VEC_ADDR, count)) == 0)
         return(EFAULT);
  }

	switch (io_type)
	{
	case _DIO_BYTE: size= 1; break;
	case _DIO_WORD: size= 2; break;
	case _DIO_LONG: size= 4; break;
	default: size= 4; break;	/* Be conservative */
	}

  rp= proc_addr(who_p);
  privp= priv(rp);
  if (privp && privp->s_flags & CHECK_IO_PORT)
  {
	switch (io_type)
	{
	case _DIO_BYTE: size= 1; break;
	case _DIO_WORD: size= 2; break;
	case _DIO_LONG: size= 4; break;
	default: size= 4; break;	/* Be conservative */
	}
	port= m_ptr->DIO_PORT;
	nr_io_range= privp->s_nr_io_range;
	for (i= 0, iorp= privp->s_io_tab; i<nr_io_range; i++, iorp++)
	{
		if (port >= iorp->ior_base && port+size-1 <= iorp->ior_limit)
			break;
	}
	if (i >= nr_io_range)
	{
		kprintf(
		"do_sdevio: I/O port check failed for proc %d, port 0x%x\n",
			m_ptr->m_source, port);
		return EPERM;
	}
  }

  if (port & (size-1))
  {
	kprintf("do_devio: unaligned port 0x%x (size %d)\n", port, size);
	return EPERM;
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
