#ifndef __HW_INTR_X86_H__
#define __HW_INTR_X86_H__

#include "../..//kernel.h"

/* legacy PIC */

_PROTOTYPE(void irq_8259_unmask,(int irq));
_PROTOTYPE(void irq_8259_mask,(int irq));
_PROTOTYPE(void irq_handle,(int irq));

#define hw_intr_mask(irq)	irq_8259_mask(irq)
#define hw_intr_unmask(irq)	irq_8259_unmask(irq)

#endif /* __HW_INTR_X86_H__ */
