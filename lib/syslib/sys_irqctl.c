#include "syslib.h"

/*===========================================================================*
 *                               sys_irqctl				     *
 *===========================================================================*/
PUBLIC int sys_irqctl(req, irq_vec, policy, proc_nr, port, val_ptr, mask_val)
int req;				/* IRQ control request */
int irq_vec;				/* IRQ vector to control */
int policy;				/* bit mask for policy flags */
int proc_nr;				/* process number to notify */
long port;				/* port to read or write */
void *val_ptr;				/* address store value read */
long mask_val;				/* strobe mask or value to write */
{
    message m_irq;
    int s;
    
    m_irq.m_type = SYS_IRQCTL;
    m_irq.IRQ_REQUEST = req;
    m_irq.IRQ_VECTOR = irq_vec;
    m_irq.IRQ_POLICY = policy;
    m_irq.IRQ_PROC_NR = proc_nr;
    m_irq.IRQ_PORT = port;
    m_irq.IRQ_VIR_ADDR = (vir_bytes) val_ptr;
    m_irq.IRQ_MASK_VAL = mask_val;
    
    return _taskcall(SYSTASK, SYS_IRQCTL, &m_irq);
}


