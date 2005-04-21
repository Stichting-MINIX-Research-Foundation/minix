#include "syslib.h"

/*===========================================================================*
 *                               sys_enable_irq				     *    
 *===========================================================================*/
PUBLIC int sys_enable_irq(int irq_nr)
{
    message m_irq;
    m_irq.IRQ_NR = irq_nr;
    m_irq.IRQ_CTL_OP = IRQ_ENABLE;
    return _taskcall(SYSTASK, SYS_IRQCTL, &m_irq);
}


