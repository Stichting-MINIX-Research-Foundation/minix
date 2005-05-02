#include "syslib.h"

/*===========================================================================*
 *                               sys_irqctl				     *
 *===========================================================================*/
PUBLIC int sys_irqctl(req, irq_vec, policy, hook_id)
int req;				/* IRQ control request */
int irq_vec;				/* IRQ vector to control */
int policy;				/* bit mask for policy flags */
int *hook_id;				/* ID of IRQ hook at kernel */
{
    message m_irq;
    int s;
    
    m_irq.m_type = SYS_IRQCTL;
    m_irq.IRQ_REQUEST = req;
    m_irq.IRQ_VECTOR = irq_vec;
    m_irq.IRQ_POLICY = policy;
    m_irq.IRQ_HOOK_ID = *hook_id;
    
    s = _taskcall(SYSTASK, SYS_IRQCTL, &m_irq);
    if (req == IRQ_SETPOLICY) *hook_id = m_irq.IRQ_HOOK_ID;
    return(s);
}


