#ifndef __APIC_ASM_H__
#define __APIC_ASM_H__


#ifndef __ASSEMBLY__
#include "kernel/kernel.h"

_PROTOTYPE( void apic_hwint0, (void) );
_PROTOTYPE( void apic_hwint1, (void) );
_PROTOTYPE( void apic_hwint2, (void) );
_PROTOTYPE( void apic_hwint3, (void) );
_PROTOTYPE( void apic_hwint4, (void) );
_PROTOTYPE( void apic_hwint5, (void) );
_PROTOTYPE( void apic_hwint6, (void) );
_PROTOTYPE( void apic_hwint7, (void) );
_PROTOTYPE( void apic_hwint8, (void) );
_PROTOTYPE( void apic_hwint9, (void) );
_PROTOTYPE( void apic_hwint10, (void) );
_PROTOTYPE( void apic_hwint11, (void) );
_PROTOTYPE( void apic_hwint12, (void) );
_PROTOTYPE( void apic_hwint13, (void) );
_PROTOTYPE( void apic_hwint14, (void) );
_PROTOTYPE( void apic_hwint15, (void) );
_PROTOTYPE( void apic_hwint16, (void) );
_PROTOTYPE( void apic_hwint17, (void) );
_PROTOTYPE( void apic_hwint18, (void) );
_PROTOTYPE( void apic_hwint19, (void) );
_PROTOTYPE( void apic_hwint20, (void) );
_PROTOTYPE( void apic_hwint21, (void) );
_PROTOTYPE( void apic_hwint22, (void) );
_PROTOTYPE( void apic_hwint23, (void) );
_PROTOTYPE( void apic_hwint24, (void) );
_PROTOTYPE( void apic_hwint25, (void) );
_PROTOTYPE( void apic_hwint26, (void) );
_PROTOTYPE( void apic_hwint27, (void) );
_PROTOTYPE( void apic_hwint28, (void) );
_PROTOTYPE( void apic_hwint29, (void) );
_PROTOTYPE( void apic_hwint30, (void) );
_PROTOTYPE( void apic_hwint31, (void) );
_PROTOTYPE( void apic_hwint32, (void) );
_PROTOTYPE( void apic_hwint33, (void) );
_PROTOTYPE( void apic_hwint34, (void) );
_PROTOTYPE( void apic_hwint35, (void) );
_PROTOTYPE( void apic_hwint36, (void) );
_PROTOTYPE( void apic_hwint37, (void) );
_PROTOTYPE( void apic_hwint38, (void) );
_PROTOTYPE( void apic_hwint39, (void) );
_PROTOTYPE( void apic_hwint40, (void) );
_PROTOTYPE( void apic_hwint41, (void) );
_PROTOTYPE( void apic_hwint42, (void) );
_PROTOTYPE( void apic_hwint43, (void) );
_PROTOTYPE( void apic_hwint44, (void) );
_PROTOTYPE( void apic_hwint45, (void) );
_PROTOTYPE( void apic_hwint46, (void) );
_PROTOTYPE( void apic_hwint47, (void) );
_PROTOTYPE( void apic_hwint48, (void) );
_PROTOTYPE( void apic_hwint49, (void) );
_PROTOTYPE( void apic_hwint50, (void) );
_PROTOTYPE( void apic_hwint51, (void) );
_PROTOTYPE( void apic_hwint52, (void) );
_PROTOTYPE( void apic_hwint53, (void) );
_PROTOTYPE( void apic_hwint54, (void) );
_PROTOTYPE( void apic_hwint55, (void) );
_PROTOTYPE( void apic_hwint56, (void) );
_PROTOTYPE( void apic_hwint57, (void) );
_PROTOTYPE( void apic_hwint58, (void) );
_PROTOTYPE( void apic_hwint59, (void) );
_PROTOTYPE( void apic_hwint60, (void) );
_PROTOTYPE( void apic_hwint61, (void) );
_PROTOTYPE( void apic_hwint62, (void) );
_PROTOTYPE( void apic_hwint63, (void) );

/* The local APIC timer tick handlers */
_PROTOTYPE(void lapic_bsp_timer_int_handler, (void));
_PROTOTYPE(void lapic_ap_timer_int_handler, (void));

#endif

#define CONFIG_APIC_DEBUG

#ifdef CONFIG_APIC_DEBUG

#define LAPIC_INTR_DUMMY_HANDLER_SIZE	32

#ifndef __ASSEMBLY__
EXTERN char lapic_intr_dummy_handles_start;
EXTERN char lapic_intr_dummy_handles_end;
#endif

#endif /* CONFIG_APIC_DEBUG */

#endif /* __APIC_ASM_H__ */
