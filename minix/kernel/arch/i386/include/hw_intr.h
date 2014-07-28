#ifndef __HW_INTR_X86_H__
#define __HW_INTR_X86_H__

#include "kernel/kernel.h"
void irq_8259_unmask(int irq);
void irq_8259_mask(int irq);
void irq_8259_eoi(int irq);
void irq_handle(int irq);
void i8259_disable(void);
void eoi_8259_master(void);
void eoi_8259_slave(void);

/* 
 * we don't use IO APIC if not configured for SMP as we cannot read any info
 * about it unless we use MPS which is not present on all single CPU
 * configurations. ACPI would be another option, however we don't support it
 * either
 */
#if defined(USE_APIC)
#include "kernel/arch/i386/apic.h"

#define hw_intr_mask(irq)	ioapic_mask_irq(irq)
#define hw_intr_unmask(irq)	ioapic_unmask_irq(irq)
#define hw_intr_ack(irq)	ioapic_eoi(irq)
#define hw_intr_used(irq)	do {					\
					if (ioapic_enabled)		\
						ioapic_set_irq(irq);	\
				} while (0)
#define hw_intr_not_used(irq)	do {					\
					if (ioapic_enabled)		\
						ioapic_unset_irq(irq);	\
				} while (0)
#define hw_intr_disable_all() do {					\
					ioapic_disable_all();		\
					ioapic_reset_pic();		\
					lapic_disable();		\
				} while (0)
#ifdef CONFIG_SMP
#define ipi_ack			apic_eoi
#endif

#else
/* legacy PIC */

#define hw_intr_mask(irq)	irq_8259_mask(irq)
#define hw_intr_unmask(irq)	irq_8259_unmask(irq)
#define hw_intr_ack(irq)	irq_8259_eoi(irq)
#define hw_intr_used(irq)
#define hw_intr_not_used(irq)
#define hw_intr_disable_all()

#endif

#endif /* __HW_INTR_X86_H__ */
