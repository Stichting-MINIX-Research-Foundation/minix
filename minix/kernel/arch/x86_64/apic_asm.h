#ifndef __APIC_ASM_H__
#define __APIC_ASM_H__


#ifndef __ASSEMBLY__
#include "kernel/kernel.h"

void apic_hwint0(void);
void apic_hwint1(void);
void apic_hwint2(void);
void apic_hwint3(void);
void apic_hwint4(void);
void apic_hwint5(void);
void apic_hwint6(void);
void apic_hwint7(void);
void apic_hwint8(void);
void apic_hwint9(void);
void apic_hwint10(void);
void apic_hwint11(void);
void apic_hwint12(void);
void apic_hwint13(void);
void apic_hwint14(void);
void apic_hwint15(void);
void apic_hwint16(void);
void apic_hwint17(void);
void apic_hwint18(void);
void apic_hwint19(void);
void apic_hwint20(void);
void apic_hwint21(void);
void apic_hwint22(void);
void apic_hwint23(void);
void apic_hwint24(void);
void apic_hwint25(void);
void apic_hwint26(void);
void apic_hwint27(void);
void apic_hwint28(void);
void apic_hwint29(void);
void apic_hwint30(void);
void apic_hwint31(void);
void apic_hwint32(void);
void apic_hwint33(void);
void apic_hwint34(void);
void apic_hwint35(void);
void apic_hwint36(void);
void apic_hwint37(void);
void apic_hwint38(void);
void apic_hwint39(void);
void apic_hwint40(void);
void apic_hwint41(void);
void apic_hwint42(void);
void apic_hwint43(void);
void apic_hwint44(void);
void apic_hwint45(void);
void apic_hwint46(void);
void apic_hwint47(void);
void apic_hwint48(void);
void apic_hwint49(void);
void apic_hwint50(void);
void apic_hwint51(void);
void apic_hwint52(void);
void apic_hwint53(void);
void apic_hwint54(void);
void apic_hwint55(void);
void apic_hwint56(void);
void apic_hwint57(void);
void apic_hwint58(void);
void apic_hwint59(void);
void apic_hwint60(void);
void apic_hwint61(void);
void apic_hwint62(void);
void apic_hwint63(void);

/* The local APIC timer tick handlers */
void lapic_timer_int_handler(void);
void apic_spurios_intr(void);
void apic_error_intr(void);

#endif

#define APIC_DEBUG

#ifdef APIC_DEBUG

#define LAPIC_INTR_DUMMY_HANDLER_SIZE	32

#ifndef __ASSEMBLY__
EXTERN char lapic_intr_dummy_handles_start;
EXTERN char lapic_intr_dummy_handles_end;
#endif

#endif /* APIC_DEBUG */

#endif /* __APIC_ASM_H__ */
