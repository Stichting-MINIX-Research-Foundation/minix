/* The kernel call implemented in this file:
 *   m_type:	SYS_DEVIO
 *
 * The parameters for this kernel call are:
 *   m2_i3:	DIO_REQUEST	(request input or output)	
 *   m2_i1:	DIO_TYPE	(flag indicating byte, word, or long)
 *   m2_l1:	DIO_PORT	(port to read/ write)	
 *   m2_l2:	DIO_VALUE	(value to write/ return value read)	
 */

#include "../system.h"
#include <minix/devio.h>

#if USE_DEVIO

/*===========================================================================*
 *			        do_devio                                     *
 *===========================================================================*/
PUBLIC int do_devio(m_ptr)
register message *m_ptr;	/* pointer to request message */
{
/* Process a single I/O request for byte, word, and long values. */
    if (m_ptr->DIO_REQUEST == DIO_INPUT) { 
      switch (m_ptr->DIO_TYPE) {
        case DIO_BYTE: m_ptr->DIO_VALUE = inb(m_ptr->DIO_PORT); break; 
        case DIO_WORD: m_ptr->DIO_VALUE = inw(m_ptr->DIO_PORT); break; 
        case DIO_LONG: m_ptr->DIO_VALUE = inl(m_ptr->DIO_PORT); break; 
    	default: return(EINVAL);
      } 
    } else { 
      switch (m_ptr->DIO_TYPE) {
        case DIO_BYTE: outb(m_ptr->DIO_PORT, m_ptr->DIO_VALUE); break;  
        case DIO_WORD: outw(m_ptr->DIO_PORT, m_ptr->DIO_VALUE); break;  
        case DIO_LONG: outl(m_ptr->DIO_PORT, m_ptr->DIO_VALUE); break;  
    	default: return(EINVAL);
      } 
    }
    return(OK);
}

#endif /* USE_DEVIO */
