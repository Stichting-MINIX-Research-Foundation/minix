#include <machine/asm.h>

IMPORT(undefined_inst_entry)
IMPORT(svc_entry)
IMPORT(prefetch_abort_entry)
IMPORT(data_abort_entry)
IMPORT(irq_entry)

.text
.balign	4096
LABEL(exc_vector_table)
    ldr pc, =invalid_exc           /* Reset */
    ldr pc, =undefined_inst_entry  /* Undefined Instruction */
    ldr pc, =svc_entry             /* Supervisor Call */
    ldr pc, =prefetch_abort_entry  /* Prefetch Abort */
    ldr pc, =data_abort_entry      /* Data Abort */
    ldr pc, =invalid_exc           /* Hypervisor Call */
    ldr pc, =irq_entry             /* Interrupt */
    ldr pc, =invalid_exc           /* Fast Interrupt */

ENTRY(invalid_exc)
	b .
