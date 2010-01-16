#ifndef __APIC_H__
#define __APIC_H__

#define APIC_ENABLE		0x100
#define APIC_FOCUS_DISABLED	(1 << 9)
#define APIC_SIV		0xFF

#define APIC_TDCR_2	0x00
#define APIC_TDCR_4	0x01
#define APIC_TDCR_8	0x02
#define APIC_TDCR_16	0x03
#define APIC_TDCR_32	0x08
#define APIC_TDCR_64	0x09
#define APIC_TDCR_128	0x0a
#define APIC_TDCR_1	0x0b

#define APIC_LVTT_VECTOR_MASK	0x000000FF
#define APIC_LVTT_DS_PENDING	(1 << 12)
#define APIC_LVTT_MASK		(1 << 16)
#define APIC_LVTT_TM		(1 << 17)

#define APIC_LVT_IIPP_MASK	0x00002000
#define APIC_LVT_IIPP_AH	0x00002000
#define APIC_LVT_IIPP_AL	0x00000000

#define IOAPIC_REGSEL		0x0
#define IOAPIC_RW		0x10

#define APIC_ICR_DM_MASK		0x00000700
#define APIC_ICR_VECTOR			APIC_LVTT_VECTOR_MASK
#define APIC_ICR_DM_FIXED		(0 << 8)
#define APIC_ICR_DM_LOWEST_PRIORITY	(1 << 8)
#define APIC_ICR_DM_SMI			(2 << 8)
#define APIC_ICR_DM_RESERVED		(3 << 8)
#define APIC_ICR_DM_NMI			(4 << 8)
#define APIC_ICR_DM_INIT		(5 << 8)
#define APIC_ICR_DM_STARTUP		(6 << 8)
#define APIC_ICR_DM_EXTINT		(7 << 8)

#define APIC_ICR_DM_PHYSICAL		(0 << 11)
#define APIC_ICR_DM_LOGICAL		(1 << 11)

#define APIC_ICR_DELIVERY_PENDING	(1 << 12)

#define APIC_ICR_INT_POLARITY		(1 << 13)

#define APIC_ICR_LEVEL_ASSERT		(1 << 14)
#define APIC_ICR_LEVEL_DEASSERT		(0 << 14)

#define APIC_ICR_TRIGGER		(1 << 15)

#define APIC_ICR_INT_MASK		(1 << 16)

#define APIC_ICR_DEST_FIELD		(0 << 18)
#define APIC_ICR_DEST_SELF		(1 << 18)
#define APIC_ICR_DEST_ALL		(2 << 18)
#define APIC_ICR_DEST_ALL_BUT_SELF	(3 << 18)

#define LOCAL_APIC_DEF_ADDR	0xfee00000 /* default local apic address */
#define IO_APIC_DEF_ADDR	0xfec00000 /* default i/o apic address */

#define LAPIC_ID	(lapic_addr + 0x020)
#define LAPIC_VERSION	(lapic_addr + 0x030)
#define LAPIC_TPR	(lapic_addr + 0x080)
#define LAPIC_EOI	(lapic_addr + 0x0b0)
#define LAPIC_LDR	(lapic_addr + 0x0d0)
#define LAPIC_DFR	(lapic_addr + 0x0e0)
#define LAPIC_SIVR	(lapic_addr + 0x0f0)
#define LAPIC_ESR	(lapic_addr + 0x280)
#define LAPIC_ICR1	(lapic_addr + 0x300)
#define LAPIC_ICR2	(lapic_addr + 0x310)
#define LAPIC_LVTTR	(lapic_addr + 0x320)
#define LAPIC_LVTTMR	(lapic_addr + 0x330)
#define LAPIC_LVTPCR	(lapic_addr + 0x340)
#define LAPIC_LINT0	(lapic_addr + 0x350)
#define LAPIC_LINT1	(lapic_addr + 0x360)
#define LAPIC_LVTER	(lapic_addr + 0x370)
#define LAPIC_TIMER_ICR	(lapic_addr + 0x380)
#define LAPIC_TIMER_CCR	(lapic_addr + 0x390)
#define LAPIC_TIMER_DCR	(lapic_addr + 0x3e0)

#define IOAPIC_ID			0x0
#define IOAPIC_VERSION			0x1
#define IOAPIC_ARB			0x2
#define IOAPIC_REDIR_TABLE		0x10

#define APIC_TIMER_INT_VECTOR		0xfe
#define APIC_SPURIOUS_INT_VECTOR	0xff


#ifndef __ASSEMBLY__

#include "../../kernel.h"

EXTERN int ioapic_enabled;
EXTERN u32_t lapic_addr;
EXTERN u32_t lapic_eoi_addr;
EXTERN u32_t lapic_taskpri_addr;
EXTERN int bsp_lapic_id;

#define MAX_NR_IOAPICS			32
#define MAX_NR_BUSES			32
#define MAX_NR_APICIDS			255
#define MAX_NR_LCLINTS			2

EXTERN u8_t apicid2cpuid[MAX_NR_APICIDS+1];
EXTERN unsigned apic_imcrp;
EXTERN unsigned nioapics;
EXTERN unsigned nbuses;
EXTERN unsigned nintrs;
EXTERN unsigned nlints;

EXTERN u32_t ioapic_id_mask[8];
EXTERN u32_t lapic_id_mask[8];
EXTERN u32_t lapic_addr_vaddr; /* we remember the virtual address here until we
				  switch to paging */
EXTERN u32_t lapic_addr;
EXTERN u32_t lapic_eoi_addr;
EXTERN u32_t lapic_taskpri_addr;

_PROTOTYPE (void calc_bus_clock, (void));
_PROTOTYPE (u32_t lapic_errstatus, (void));
/*
_PROTOTYPE (u32_t ioapic_read, (u32_t addr, u32_t offset));
_PROTOTYPE (void ioapic_write, (u32_t addr, u32_t offset, u32_t data));
_PROTOTYPE (void lapic_eoi, (void));
*/
_PROTOTYPE (void lapic_microsec_sleep, (unsigned count));
_PROTOTYPE (void smp_ioapic_unmask, (void));
_PROTOTYPE (void ioapic_disable_irqs, (u32_t irq));
_PROTOTYPE (void ioapic_enable_irqs, (u32_t irq));
_PROTOTYPE (u32_t ioapic_irqs_inuse, (void));
_PROTOTYPE (void smp_recv_ipi, (int arg));
_PROTOTYPE (void ioapic_config_pci_irq, (u32_t data));

_PROTOTYPE (int lapic_enable, (void));
_PROTOTYPE (void lapic_disable, (void));

_PROTOTYPE (void ioapic_disable_all, (void));
_PROTOTYPE (int ioapic_enable_all, (void));

_PROTOTYPE(void apic_idt_init, (int reset));

_PROTOTYPE(int apic_single_cpu_init, (void));

_PROTOTYPE(void lapic_set_timer_periodic, (unsigned freq));
_PROTOTYPE(void lapic_stop_timer, (void));

#include <minix/cpufeature.h>

#define cpu_feature_apic_on_chip() _cpufeature(_CPUF_I386_APIC_ON_CHIP)

#define lapic_read(what)	(*((u32_t *)((what))))
#define lapic_write(what, data)	do {			\
	(*((u32_t *)((what)))) = data;			\
} while(0)

#endif /* __ASSEMBLY__ */

#endif /* __APIC_H__ */
