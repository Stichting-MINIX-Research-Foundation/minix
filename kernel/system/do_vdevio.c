/* The system call implemented in this file:
 *   m_type:	SYS_VDEVIO
 *
 * The parameters for this system call are:
 *    m2_i3:	DIO_REQUEST	(request input or output)	
 *    m2_i1:	DIO_TYPE	(flag indicating byte, word, or long)
 *    m2_p1:	DIO_VEC_ADDR	(pointer to port/ value pairs)	
 *    m2_i2:	DIO_VEC_SIZE	(number of ports to read or write) 
 */

#include "../system.h"
#include <minix/devio.h>

#if USE_VDEVIO


/* Buffer for SYS_VDEVIO to copy (port,value)-pairs from/ to user. */
PRIVATE char vdevio_pv_buf[VDEVIO_BUF_SIZE];      

/* SYS_VDEVIO sends a pointer to a (port,value)-pairs vector at the caller. 
 * Define the maximum number of (port,value)-pairs that can be handled in a 
 * in a single SYS_VDEVIO system call based on the struct definitions. 
 */
#define MAX_PVB_PAIRS ((VDEVIO_BUF_SIZE * sizeof(char)) / sizeof(pvb_pair_t))
#define MAX_PVW_PAIRS ((VDEVIO_BUF_SIZE * sizeof(char)) / sizeof(pvw_pair_t))
#define MAX_PVL_PAIRS ((VDEVIO_BUF_SIZE * sizeof(char)) / sizeof(pvl_pair_t))
	

/*===========================================================================*
 *			        do_vdevio                                    *
 *===========================================================================*/
PUBLIC int do_vdevio(m_ptr)
register message *m_ptr;	/* pointer to request message */
{
/* Perform a series of device I/O on behalf of a non-kernel process. The 
 * I/O addresses and I/O values are fetched from and returned to some buffer
 * in user space. The actual I/O is wrapped by lock() and unlock() to prevent
 * that I/O batch from being interrrupted.
 * This is the counterpart of do_devio, which performs a single device I/O. 
 */ 
    pvb_pair_t *pvb_pairs;      /* needed for byte values */
    pvw_pair_t *pvw_pairs;      /* needed for word values */
    pvl_pair_t *pvl_pairs;      /* needed for long values */
    int i;
    pid_t caller_pid;           /* process id of caller */
    size_t bytes;               /* # bytes to be copied */
    vir_bytes caller_vir;       /* virtual address at caller */
    phys_bytes caller_phys;     /* physical address at caller */
    phys_bytes kernel_phys;     /* physical address in kernel */


    /* Check if nr of ports is ok and get size of (port,value) data. */
    if (m_ptr->DIO_VEC_SIZE <= 0) return(EINVAL);
    switch(m_ptr->DIO_TYPE) {
    case DIO_BYTE:
        if (m_ptr->DIO_VEC_SIZE > MAX_PVB_PAIRS)  return(EINVAL);
        bytes = (size_t) (m_ptr->DIO_VEC_SIZE * sizeof(pvb_pair_t));
        break;
    case DIO_WORD:
        if (m_ptr->DIO_VEC_SIZE > MAX_PVW_PAIRS)  return(EINVAL);
        bytes = (size_t) (m_ptr->DIO_VEC_SIZE * sizeof(pvw_pair_t));
        break;
    case DIO_LONG:
        if (m_ptr->DIO_VEC_SIZE > MAX_PVL_PAIRS)  return(EINVAL);
        bytes = (size_t) (m_ptr->DIO_VEC_SIZE * sizeof(pvl_pair_t));
        break;
    default:	/* this once and for all checks for a correct type */
        return(EINVAL);
    }

    /* Calculate physical addresses and copy (port,value)-pairs from user. */
    caller_pid = (pid_t) m_ptr->m_source; 
    caller_vir = (vir_bytes) m_ptr->DIO_VEC_ADDR;
    caller_phys = umap_local(proc_addr(caller_pid), D, caller_vir, bytes);
    if (0 == caller_phys) return EFAULT;
    kernel_phys = vir2phys(vdevio_pv_buf);
    phys_copy(caller_phys, kernel_phys, (phys_bytes) bytes);

    /* Perform actual device I/O for byte, word, and long values. Note that 
    * the entire switch is wrapped in lock() and unlock() to prevent the I/O
    * batch from being interrupted. It may be cleaner to do this just around 
    * the for loops, but this results in rather lenghty code.
    */
    lock(13, "do_vdevio");
    switch (m_ptr->DIO_TYPE) {
        case DIO_BYTE: 					 /* byte values */
            pvb_pairs = (pvb_pair_t *) vdevio_pv_buf;
            if (DIO_INPUT == m_ptr->DIO_REQUEST) { 
                for (i=0; i < m_ptr->DIO_VEC_SIZE; i++) 
                    pvb_pairs[i].value = inb(pvb_pairs[i].port); 
            } else { 
                for (i=0; i < m_ptr->DIO_VEC_SIZE; i++) 
                    outb(pvb_pairs[i].port, pvb_pairs[i].value); 
            } 
            break; 
        case DIO_WORD:					  /* word values */
            pvw_pairs = (pvw_pair_t *) vdevio_pv_buf;
            if (DIO_INPUT == m_ptr->DIO_REQUEST) {
                for (i=0; i < m_ptr->DIO_VEC_SIZE; i++) 
                    pvw_pairs[i].value = inw(pvw_pairs[i].port);  
            } else {
                for (i=0; i < m_ptr->DIO_VEC_SIZE; i++) 
                    outw(pvw_pairs[i].port, pvw_pairs[i].value); 
            }
            break; 
	case DIO_LONG:			/* fall through: long values */
        default:  /* only DIO_LONG can arrive here, see above switch */
            pvl_pairs = (pvl_pair_t *) vdevio_pv_buf;
            if (DIO_INPUT == m_ptr->DIO_REQUEST) { 
                for (i=0; i < m_ptr->DIO_VEC_SIZE; i++) 
                    pvl_pairs[i].value = inl(pvl_pairs[i].port);  
            } else {
                for (i=0; i < m_ptr->DIO_VEC_SIZE; i++) 
                    outl(pvb_pairs[i].port, pvl_pairs[i].value); 
            }
    }
    unlock(13);
    
    /* Almost done, copy back results for input requests. */
    if (DIO_INPUT == m_ptr->REQUEST)
        phys_copy(kernel_phys, caller_phys, (phys_bytes) bytes);
    return(OK);
}

#endif /* USE_VDEVIO */

