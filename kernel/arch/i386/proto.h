
#ifndef _I386_PROTO_H
#define _I386_PROTO_H

/* Hardware interrupt handlers. */
_PROTOTYPE( void hwint00, (void) );
_PROTOTYPE( void hwint01, (void) );
_PROTOTYPE( void hwint02, (void) );
_PROTOTYPE( void hwint03, (void) );
_PROTOTYPE( void hwint04, (void) );
_PROTOTYPE( void hwint05, (void) );
_PROTOTYPE( void hwint06, (void) );
_PROTOTYPE( void hwint07, (void) );
_PROTOTYPE( void hwint08, (void) );
_PROTOTYPE( void hwint09, (void) );
_PROTOTYPE( void hwint10, (void) );
_PROTOTYPE( void hwint11, (void) );
_PROTOTYPE( void hwint12, (void) );
_PROTOTYPE( void hwint13, (void) );
_PROTOTYPE( void hwint14, (void) );
_PROTOTYPE( void hwint15, (void) );


/* Exception handlers (real or protected mode), in numerical order. */
void _PROTOTYPE( int00, (void) ), _PROTOTYPE( divide_error, (void) );
void _PROTOTYPE( int01, (void) ), _PROTOTYPE( single_step_exception, (void) );
void _PROTOTYPE( int02, (void) ), _PROTOTYPE( nmi, (void) );
void _PROTOTYPE( int03, (void) ), _PROTOTYPE( breakpoint_exception, (void) );
void _PROTOTYPE( int04, (void) ), _PROTOTYPE( overflow, (void) );
void _PROTOTYPE( int05, (void) ), _PROTOTYPE( bounds_check, (void) );
void _PROTOTYPE( int06, (void) ), _PROTOTYPE( inval_opcode, (void) );
void _PROTOTYPE( int07, (void) ), _PROTOTYPE( copr_not_available, (void) );
void                              _PROTOTYPE( double_fault, (void) );
void                              _PROTOTYPE( copr_seg_overrun, (void) );
void                              _PROTOTYPE( inval_tss, (void) );
void                              _PROTOTYPE( segment_not_present, (void) );
void                              _PROTOTYPE( stack_exception, (void) );
void                              _PROTOTYPE( general_protection, (void) );
void                              _PROTOTYPE( page_fault, (void) );
void                              _PROTOTYPE( copr_error, (void) );

/* Software interrupt handlers, in numerical order. */
_PROTOTYPE( void trp, (void) );
_PROTOTYPE( void s_call, (void) ), _PROTOTYPE( p_s_call, (void) ); 
_PROTOTYPE( void level0_call, (void) );

/* memory.c */
_PROTOTYPE( void vir_insb, (u16_t port, struct proc *proc, u32_t vir, size_t count));
_PROTOTYPE( void vir_outsb, (u16_t port, struct proc *proc, u32_t vir, size_t count));
_PROTOTYPE( void vir_insw, (u16_t port, struct proc *proc, u32_t vir, size_t count));
_PROTOTYPE( void vir_outsw, (u16_t port, struct proc *proc, u32_t vir, size_t count));


/* exception.c */
_PROTOTYPE( void exception, (unsigned vec_nr, u32_t trap_errno,
	u32_t old_eip, U16_t old_cs, u32_t old_eflags)			);

/* klib386.s */
_PROTOTYPE( void level0, (void (*func)(void))                           );
_PROTOTYPE( void monitor, (void)                                        );
_PROTOTYPE( void reset, (void)                                          );
_PROTOTYPE( void int86, (void)                     			);
_PROTOTYPE( unsigned long read_cr0, (void)                              );
_PROTOTYPE( void write_cr0, (unsigned long value)                       );
_PROTOTYPE( void write_cr3, (unsigned long value)                       );
_PROTOTYPE( unsigned long read_cpu_flags, (void)                        );
_PROTOTYPE( void phys_insb, (U16_t port, phys_bytes buf, size_t count)  );
_PROTOTYPE( void phys_insw, (U16_t port, phys_bytes buf, size_t count)  );
_PROTOTYPE( void phys_outsb, (U16_t port, phys_bytes buf, size_t count) );
_PROTOTYPE( void phys_outsw, (U16_t port, phys_bytes buf, size_t count) );
_PROTOTYPE( void i386_invlpg, (U32_t addr) );

/* protect.c */
_PROTOTYPE( void prot_init, (void)                     			);
_PROTOTYPE( void init_codeseg, (struct segdesc_s *segdp, phys_bytes base,
                vir_bytes size, int privilege)                          );
_PROTOTYPE( void init_dataseg, (struct segdesc_s *segdp, phys_bytes base,
                vir_bytes size, int privilege)                          );
_PROTOTYPE( void enable_iop, (struct proc *pp)                          );

/* functions defined in architecture-independent kernel source. */
#include "../../proto.h"

#endif
