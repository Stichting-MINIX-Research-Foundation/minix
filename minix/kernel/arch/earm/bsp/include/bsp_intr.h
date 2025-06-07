#ifndef _BSP_INTR_H_
#define _BSP_INTR_H_

#ifndef __ASSEMBLY__

// Added kernel headers
#include <minix/kernel_types.h>
#include <klib/include/kprintf.h> // For KASSERT_PLACEHOLDER if used in headers this includes
#include <klib/include/kstring.h> // Precautionary
#include <klib/include/kmemory.h> // Precautionary

void bsp_irq_unmask(int irq);
void bsp_irq_mask(int irq);
void bsp_irq_handle(void);

#endif /* __ASSEMBLY__ */

#endif /* _BSP_INTR_H_ */
