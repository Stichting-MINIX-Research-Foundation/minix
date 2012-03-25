/* The kernel call implemented in this file:
 *   m_type:	SYS_VDEVIO
 *
 * The parameters for this kernel call are:
 *    m2_i3:	DIO_REQUEST	(request input or output)	
 *    m2_p1:	DIO_VEC_ADDR	(pointer to port/ value pairs)	
 *    m2_i2:	DIO_VEC_SIZE	(number of ports to read or write) 
 */

#include "kernel/system.h"
#include <minix/devio.h>
#include <minix/endpoint.h>
#include <minix/portio.h>

#if USE_VDEVIO

/* Buffer for SYS_VDEVIO to copy (port,value)-pairs from/ to user. */
static char vdevio_buf[VDEVIO_BUF_SIZE];      
static pvb_pair_t * const pvb = (pvb_pair_t *) vdevio_buf;           
static pvw_pair_t * const pvw = (pvw_pair_t *) vdevio_buf;      
static pvl_pair_t * const pvl = (pvl_pair_t *) vdevio_buf;     

/*===========================================================================*
 *			        do_vdevio                                    *
 *===========================================================================*/
int do_vdevio(struct proc * caller, message * m_ptr)
{
/* Perform a series of device I/O on behalf of a non-kernel process. The 
 * I/O addresses and I/O values are fetched from and returned to some buffer
 * in user space. The actual I/O is wrapped by lock() and unlock() to prevent
 * that I/O batch from being interrrupted.
 * This is the counterpart of do_devio, which performs a single device I/O. 
 */ 
  int vec_size;               /* size of vector */
  int io_in;                  /* true if input */
  size_t bytes;               /* # bytes to be copied */
  port_t port;
  int i, j, io_size, nr_io_range;
  int io_dir, io_type;
  struct priv *privp;
  struct io_range *iorp;
  int r;
    
  /* Get the request, size of the request vector, and check the values. */
  io_dir = m_ptr->DIO_REQUEST & _DIO_DIRMASK;
  io_type = m_ptr->DIO_REQUEST & _DIO_TYPEMASK;
  if (io_dir == _DIO_INPUT) io_in = TRUE;
  else if (io_dir == _DIO_OUTPUT) io_in = FALSE;
  else return(EINVAL);
  if ((vec_size = m_ptr->DIO_VEC_SIZE) <= 0) return(EINVAL);
  switch (io_type) {
      case _DIO_BYTE:
	bytes = vec_size * sizeof(pvb_pair_t);
	io_size= sizeof(u8_t);
	break;
      case _DIO_WORD:
	bytes = vec_size * sizeof(pvw_pair_t);
	io_size= sizeof(u16_t);
	break;
      case _DIO_LONG:
	bytes = vec_size * sizeof(pvl_pair_t);
	io_size= sizeof(u32_t);
	break;
      default:  return(EINVAL);   /* check type once and for all */
  }
  if (bytes > sizeof(vdevio_buf))  return(E2BIG);

  /* Copy (port,value)-pairs from user. */
  if((r=data_copy(caller->p_endpoint, (vir_bytes) m_ptr->DIO_VEC_ADDR,
    KERNEL, (vir_bytes) vdevio_buf, bytes)) != OK)
	return r;

  privp= priv(caller);
  if (privp && (privp->s_flags & CHECK_IO_PORT))
  {
	/* Check whether the I/O is allowed */
	nr_io_range= privp->s_nr_io_range;
	for (i=0; i<vec_size; i++)
	{
		switch (io_type) {
		case _DIO_BYTE: port= pvb[i].port; break;
		case _DIO_WORD: port= pvw[i].port; break;
		default:	port= pvl[i].port; break;
		}
		for (j= 0, iorp= privp->s_io_tab; j<nr_io_range; j++, iorp++)
		{
			if (port >= iorp->ior_base &&
				port+io_size-1 <= iorp->ior_limit)
			{
				break;
			}
		}
		if (j >= nr_io_range)
		{
			printf(
		"do_vdevio: I/O port check failed for proc %d, port 0x%x\n",
				caller->p_endpoint, port);
			return EPERM;
		}
	}
  }

  /* Perform actual device I/O for byte, word, and long values */
  switch (io_type) {
  case _DIO_BYTE: 					 /* byte values */
      if (io_in) for (i=0; i<vec_size; i++) 
		pvb[i].value = inb( pvb[i].port); 
      else      for (i=0; i<vec_size; i++)
		outb( pvb[i].port, pvb[i].value); 
      break; 
  case _DIO_WORD:					  /* word values */
      if (io_in)
      {
	for (i=0; i<vec_size; i++)  
	{
		port= pvw[i].port;
		if (port & 1) goto bad;
		pvw[i].value = inw( pvw[i].port);  
	}
      }
      else
      {
	for (i=0; i<vec_size; i++) 
	{
		port= pvw[i].port;
		if (port & 1) goto bad;
		outw( pvw[i].port, pvw[i].value); 
	}
      }
      break; 
  default:            					  /* long values */
      if (io_in)
      {
	for (i=0; i<vec_size; i++)
	{
		port= pvl[i].port;
		if (port & 3) goto bad;
		pvl[i].value = inl(pvl[i].port);  
	}
      }
      else
      {
	for (i=0; i<vec_size; i++)
	{
		port= pvl[i].port;
		if (port & 3) goto bad;
		outl( pvb[i].port, pvl[i].value); 
	}
      }
  }
    
  /* Almost done, copy back results for input requests. */
  if (io_in) 
	if((r=data_copy(KERNEL, (vir_bytes) vdevio_buf,
	  caller->p_endpoint, (vir_bytes) m_ptr->DIO_VEC_ADDR,
	  (phys_bytes) bytes)) != OK)
		return r;
  return(OK);

bad:
	panic("do_vdevio: unaligned port: %d", port);
	return EPERM;
}

#endif /* USE_VDEVIO */

