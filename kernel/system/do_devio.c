/* The kernel call implemented in this file:
 *   m_type:	SYS_DEVIO
 *
 * The parameters for this kernel call are:
 *   m2_i3:	DIO_REQUEST	(request input or output)	
 *   m2_l1:	DIO_PORT	(port to read/ write)	
 *   m2_l2:	DIO_VALUE	(value to write/ return value read)	
 */

#include "kernel/system.h"
#include <minix/devio.h>
#include <minix/endpoint.h>
#include <minix/portio.h>

#if USE_DEVIO

/*===========================================================================*
 *			        do_devio                                     *
 *===========================================================================*/
int do_devio(struct proc * caller, message * m_ptr)
{
    struct priv *privp;
    port_t port;
    struct io_range *iorp;
    int i, size, nr_io_range;
    int io_type, io_dir;

    io_type = m_ptr->DIO_REQUEST & _DIO_TYPEMASK;
    io_dir  = m_ptr->DIO_REQUEST & _DIO_DIRMASK;

    switch (io_type)
    {
	case _DIO_BYTE: size= 1; break;
	case _DIO_WORD: size= 2; break;
	case _DIO_LONG: size= 4; break;
	default: size= 4; break;	/* Be conservative */
    }

    privp= priv(caller);
    if (!privp)
    {
	printf("no priv structure!\n");
	goto doit;
    }
    if (privp->s_flags & CHECK_IO_PORT)
    {
	port= m_ptr->DIO_PORT;
	nr_io_range= privp->s_nr_io_range;
	for (i= 0, iorp= privp->s_io_tab; i<nr_io_range; i++, iorp++)
	{
		if (port >= iorp->ior_base && port+size-1 <= iorp->ior_limit)
			break;
	}
	if (i >= nr_io_range)
	{
			printf("do_devio: port 0x%x (size %d) not allowed\n",
				m_ptr->DIO_PORT, size);
		return EPERM;
	}
    }

doit:
    if (m_ptr->DIO_PORT & (size-1))
    {
		printf("do_devio: unaligned port 0x%x (size %d)\n",
			m_ptr->DIO_PORT, size);
	return EPERM;
    }

/* Process a single I/O request for byte, word, and long values. */
    if (io_dir == _DIO_INPUT) { 
      switch (io_type) {
	/* maybe "it" should not be called ports */
        case _DIO_BYTE: m_ptr->DIO_VALUE = inb(m_ptr->DIO_PORT); break; 
        case _DIO_WORD: m_ptr->DIO_VALUE = inw(m_ptr->DIO_PORT); break; 
        case _DIO_LONG: m_ptr->DIO_VALUE = inl(m_ptr->DIO_PORT); break;
    	default: return(EINVAL);
      } 
    } else { 
      switch (io_type) {
	case _DIO_BYTE: outb(m_ptr->DIO_PORT, m_ptr->DIO_VALUE); break;
	case _DIO_WORD: outw(m_ptr->DIO_PORT, m_ptr->DIO_VALUE); break;
	case _DIO_LONG: outl(m_ptr->DIO_PORT, m_ptr->DIO_VALUE); break;
    	default: return(EINVAL);
      } 
    }
    return(OK);
}

#endif /* USE_DEVIO */
