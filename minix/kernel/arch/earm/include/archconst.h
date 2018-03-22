
#ifndef _ARM_ACONST_H
#define _ARM_ACONST_H

#include <machine/interrupt.h>
#include <machine/memory.h>
#include <machine/cpu.h>
#include <arm/armreg.h>

/* Program stack words and masks. */
#define INIT_PSR      (PSR_USR32_MODE | PSR_F)    /* initial psr */
#define INIT_TASK_PSR (PSR_SVC32_MODE | PSR_F)    /* initial psr for tasks */

/* Exception vector numbers */
#define RESET_VECTOR                  0
#define UNDEFINED_INST_VECTOR         1
#define SUPERVISOR_CALL_VECTOR        2
#define PREFETCH_ABORT_VECTOR         3
#define DATA_ABORT_VECTOR             4
#define HYPERVISOR_CALL_VECTOR        5
#define INTERRUPT_VECTOR              6
#define FAST_INTERRUPT_VECTOR         7

/* Data abort helper */
#define is_align_fault(fault_status) \
	((fault_status) == FAULT_ALIGN_0)

#define is_trans_fault(fault_status) \
	(((fault_status) == FAULT_TRANS_S) || ((fault_status) == FAULT_TRANS_P))

#define is_perm_fault(fault_status) \
	(((fault_status) == FAULT_PERM_S) || ((fault_status) == FAULT_PERM_P))

/*
 * defines how many bytes are reserved at the top of the kernel stack for global
 * information like currently scheduled process or current cpu id
 */
#define ARM_STACK_TOP_RESERVED	(2 * sizeof(reg_t))

/* only selected bits are changeable by user e.g.[31:9] and skip the 
 * mode bits. It is probably is a better idea to look at the current 
 * status to determine if one is allowed to write these values. This 
 * might allow debugging of privileged processes 
 */
#define SET_USR_PSR(rp, npsr) \
	rp->p_reg.psr = ( rp->p_reg.psr & 0x1F) | ( npsr & ~0x1F)


#define PG_ALLOCATEME ((phys_bytes)-1)

#endif /* _ARM_ACONST_H */
