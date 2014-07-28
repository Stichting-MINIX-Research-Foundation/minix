#include "syslib.h"

/*===========================================================================*
 *                               sys_irqctl				     *
 *===========================================================================*/
int sys_irqctl(req, irq_vec, policy, hook_id)
int req;				/* IRQ control request */
int irq_vec;				/* IRQ vector to control */
int policy;				/* bit mask for policy flags */
int *hook_id;				/* ID of IRQ hook at kernel */
{
    message m_irq;
    int s;
    
    m_irq.m_type = SYS_IRQCTL;
    m_irq.m_lsys_krn_sys_irqctl.request = req;
    m_irq.m_lsys_krn_sys_irqctl.vector = irq_vec;
    m_irq.m_lsys_krn_sys_irqctl.policy = policy;
    m_irq.m_lsys_krn_sys_irqctl.hook_id = *hook_id;
    
    s = _kernel_call(SYS_IRQCTL, &m_irq);
    if (req == IRQ_SETPOLICY) *hook_id = m_irq.m_krn_lsys_sys_irqctl.hook_id;
    return(s);
}
