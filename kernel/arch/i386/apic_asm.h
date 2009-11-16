#ifndef __APIC_ASM_H__
#define __APIC_ASM_H__


#ifndef __ASSEMBLY__
#include "../../kernel.h"

_PROTOTYPE( void apic_hwint00, (void) );
_PROTOTYPE( void apic_hwint01, (void) );
_PROTOTYPE( void apic_hwint02, (void) );
_PROTOTYPE( void apic_hwint03, (void) );
_PROTOTYPE( void apic_hwint04, (void) );
_PROTOTYPE( void apic_hwint05, (void) );
_PROTOTYPE( void apic_hwint06, (void) );
_PROTOTYPE( void apic_hwint07, (void) );
_PROTOTYPE( void apic_hwint08, (void) );
_PROTOTYPE( void apic_hwint09, (void) );
_PROTOTYPE( void apic_hwint10, (void) );
_PROTOTYPE( void apic_hwint11, (void) );
_PROTOTYPE( void apic_hwint12, (void) );
_PROTOTYPE( void apic_hwint13, (void) );
_PROTOTYPE( void apic_hwint14, (void) );
_PROTOTYPE( void apic_hwint15, (void) );

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
