/*	$NetBSD: db_machdep.h,v 1.22 2014/09/13 18:08:38 matt Exp $	*/

/*
 * Copyright (c) 1996 Scott K Stevens
 * 
 * Mach Operating System
 * Copyright (c) 1991,1990 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */

#ifndef	_ARM_DB_MACHDEP_H_
#define	_ARM_DB_MACHDEP_H_

/*
 * Machine-dependent defines for new kernel debugger.
 */
#include <sys/types.h>
#include <uvm/uvm_extern.h>
#include <arm/armreg.h>
#include <machine/frame.h>
#include <machine/trap.h>

/* end of mangling */

typedef	vaddr_t		db_addr_t;	/* address - unsigned */
#define	DDB_EXPR_FMT	"l"		/* expression is long */
typedef	long		db_expr_t;	/* expression - signed */

typedef trapframe_t db_regs_t;

#ifndef MULTIPROCESSOR
extern db_regs_t ddb_regs;	/* register state */
#define	DDB_REGS	(&ddb_regs)
#else
extern db_regs_t *ddb_regp;
#define DDB_REGS	(ddb_regp)
#define ddb_regs	(*ddb_regp)
#endif

#ifdef __PROG26
#define	PC_REGS(regs)	((regs)->tf_r15 & R15_PC)
#define PC_ADVANCE(regs) ((regs)->tf_r15 += BKPT_SIZE)
#else
#define	PC_REGS(regs)	((regs)->tf_pc)
#define PC_ADVANCE(r)   ((r)->tf_r15 += BKPT_SIZE)
#endif

#define	BKPT_ADDR(addr)	(addr)			/* breakpoint address */
#if defined(DDB)
#define	BKPT_INST	(KERNEL_BREAKPOINT)	/* breakpoint instruction */
#else
/* breakpoint instruction if we use KGDB, this is used in db_set_temp_breakpoint() */
#define BKPT_INST	(GDB5_BREAKPOINT)
#endif
#define	BKPT_SIZE	(INSN_SIZE)		/* size of breakpoint inst */
#define	BKPT_SET(inst, addr)	(BKPT_INST)

/*#define FIXUP_PC_AFTER_BREAK(regs)	((regs)->tf_pc -= BKPT_SIZE)*/

#define	T_FAULT				(0)
#define T_BREAKPOINT			(1)

#define	IS_BREAKPOINT_TRAP(type, code)	((type) == T_BREAKPOINT)
#define IS_WATCHPOINT_TRAP(type, code)	(0)

#define	inst_trap_return(ins)	(0)
/* ldmxx reg, {..., pc}
					    01800000  stack mode
					    000f0000  register
					    0000ffff  register list */
/* mov pc, reg
					    0000000f  register */
#define	inst_return(ins)	(((ins) & 0x0e108000) == 0x08108000 || \
				 ((ins) & 0x0ff0fff0) == 0x01a0f000)
/* bl ...
					    00ffffff  offset>>2 */
#define	inst_call(ins)		(((ins) & 0x0f000000) == 0x0b000000)
/* b ...
					    00ffffff  offset>>2 */
/* ldr pc, [pc, reg, lsl #2]
					    0000000f  register */
#define	inst_branch(ins)	(((ins) & 0x0f000000) == 0x0a000000 || \
				 ((ins) & 0x0fdffff0) == 0x079ff100 || \
				 ((ins) & 0x0ff0f000) == 0x0590f000)
#define inst_load(ins)		(0)
#define inst_store(ins)		(0)
#define inst_unconditional_flow_transfer(ins)	\
	(__SHIFTOUT((ins), INSN_COND_MASK) == INSN_COND_AL \
	 && (inst_branch(ins) || inst_call(ins) || inst_return(ins)))

#define getreg_val			(0)
#define next_instr_address(pc, bd)	((bd) ? (pc) : ((pc) + INSN_SIZE))

#define DB_MACHINE_COMMANDS

#define SOFTWARE_SSTEP

u_int branch_taken(u_int insn, u_int pc, db_regs_t *db_regs);
int kdb_trap(int, db_regs_t *);
void db_machine_init(void);
int db_validate_address(vaddr_t addr);

#define DB_ELF_SYMBOLS
#define DB_ELFSIZE 32

/*
 * kgdb
 */
typedef register_t	kgdb_reg_t;
#define KGDB_NUMREGS	(16 + 8*3 + 2)	/* r0..r15, f0..f7, fps, cpsr
					 * fp-registers are 12 bytes wide */
#define KGDB_REGNUM_R0		0
#define KGDB_REGNUM_SPSR	16 + 8*3 + 1
#define KGDB_BUFLEN		1024

/*
 * MP stuff
 */
extern volatile struct cpu_info *db_onproc;
extern volatile struct cpu_info *db_newcpu;
#endif	/* _ARM_DB_MACHDEP_H_ */
