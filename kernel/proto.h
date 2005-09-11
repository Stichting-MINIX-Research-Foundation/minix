/* Function prototypes. */

#ifndef PROTO_H
#define PROTO_H

/* Struct declarations. */
struct proc;
struct timer;

/* clock.c */
_PROTOTYPE( void clock_task, (void)					);
_PROTOTYPE( void clock_stop, (void)					);
_PROTOTYPE( clock_t get_uptime, (void)					);
_PROTOTYPE( unsigned long read_clock, (void)				);
_PROTOTYPE( void set_timer, (struct timer *tp, clock_t t, tmr_func_t f)	);
_PROTOTYPE( void reset_timer, (struct timer *tp)			);

/* main.c */
_PROTOTYPE( void main, (void)						);
_PROTOTYPE( void prepare_shutdown, (int how)				);

/* utility.c */
_PROTOTYPE( void kprintf, (const char *fmt, ...)			);
_PROTOTYPE( void panic, (_CONST char *s, int n)				);

/* proc.c */
_PROTOTYPE( int sys_call, (int function, int src_dest, message *m_ptr)	);
_PROTOTYPE( int lock_notify, (int src, int dst)				);
_PROTOTYPE( int lock_send, (int dst, message *m_ptr)			);
_PROTOTYPE( void lock_enqueue, (struct proc *rp)			);
_PROTOTYPE( void lock_dequeue, (struct proc *rp)			);

/* start.c */
_PROTOTYPE( void cstart, (U16_t cs, U16_t ds, U16_t mds,
				U16_t parmoff, U16_t parmsize)		);

/* system.c */
_PROTOTYPE( int get_priv, (register struct proc *rc, int proc_type)	);
_PROTOTYPE( void send_sig, (int proc_nr, int sig_nr)			);
_PROTOTYPE( void cause_sig, (int proc_nr, int sig_nr)			);
_PROTOTYPE( void sys_task, (void)					);
_PROTOTYPE( void get_randomness, (int source)				);
_PROTOTYPE( int virtual_copy, (struct vir_addr *src, struct vir_addr *dst, 
				vir_bytes bytes) 			);
#define numap_local(proc_nr, vir_addr, bytes) \
	umap_local(proc_addr(proc_nr), D, (vir_addr), (bytes))
_PROTOTYPE( phys_bytes umap_local, (struct proc *rp, int seg, 
		vir_bytes vir_addr, vir_bytes bytes)			);
_PROTOTYPE( phys_bytes umap_remote, (struct proc *rp, int seg, 
		vir_bytes vir_addr, vir_bytes bytes)			);
_PROTOTYPE( phys_bytes umap_bios, (struct proc *rp, vir_bytes vir_addr,
		vir_bytes bytes)					);

#if (CHIP == INTEL)

/* exception.c */
_PROTOTYPE( void exception, (unsigned vec_nr)				);

/* i8259.c */
_PROTOTYPE( void intr_init, (int mine)					);
_PROTOTYPE( void intr_handle, (irq_hook_t *hook)			);
_PROTOTYPE( void put_irq_handler, (irq_hook_t *hook, int irq,
						irq_handler_t handler)	);
_PROTOTYPE( void rm_irq_handler, (irq_hook_t *hook)			);

/* klib*.s */
_PROTOTYPE( void int86, (void)						);
_PROTOTYPE( void cp_mess, (int src,phys_clicks src_clicks,vir_bytes src_offset,
		phys_clicks dst_clicks, vir_bytes dst_offset)		);
_PROTOTYPE( void enable_irq, (irq_hook_t *hook)				);
_PROTOTYPE( int disable_irq, (irq_hook_t *hook)				);
_PROTOTYPE( u16_t mem_rdw, (U16_t segm, vir_bytes offset)		);
_PROTOTYPE( void phys_copy, (phys_bytes source, phys_bytes dest,
		phys_bytes count)					);
_PROTOTYPE( void phys_memset, (phys_bytes source, unsigned long pattern,
		phys_bytes count)					);
_PROTOTYPE( void phys_insb, (U16_t port, phys_bytes buf, size_t count)	);
_PROTOTYPE( void phys_insw, (U16_t port, phys_bytes buf, size_t count)	);
_PROTOTYPE( void phys_outsb, (U16_t port, phys_bytes buf, size_t count)	);
_PROTOTYPE( void phys_outsw, (U16_t port, phys_bytes buf, size_t count)	);
_PROTOTYPE( void reset, (void)						);
_PROTOTYPE( void level0, (void (*func)(void))				);
_PROTOTYPE( void monitor, (void)					);
_PROTOTYPE( void read_tsc, (unsigned long *high, unsigned long *low)	);
_PROTOTYPE( unsigned long read_cpu_flags, (void)			);

/* mpx*.s */
_PROTOTYPE( void idle_task, (void)					);
_PROTOTYPE( void restart, (void)					);

/* The following are never called from C (pure asm procs). */

/* Exception handlers (real or protected mode), in numerical order. */
void _PROTOTYPE( int00, (void) ), _PROTOTYPE( divide_error, (void) );
void _PROTOTYPE( int01, (void) ), _PROTOTYPE( single_step_exception, (void) );
void _PROTOTYPE( int02, (void) ), _PROTOTYPE( nmi, (void) );
void _PROTOTYPE( int03, (void) ), _PROTOTYPE( breakpoint_exception, (void) );
void _PROTOTYPE( int04, (void) ), _PROTOTYPE( overflow, (void) );
void _PROTOTYPE( int05, (void) ), _PROTOTYPE( bounds_check, (void) );
void _PROTOTYPE( int06, (void) ), _PROTOTYPE( inval_opcode, (void) );
void _PROTOTYPE( int07, (void) ), _PROTOTYPE( copr_not_available, (void) );
void				  _PROTOTYPE( double_fault, (void) );
void				  _PROTOTYPE( copr_seg_overrun, (void) );
void				  _PROTOTYPE( inval_tss, (void) );
void				  _PROTOTYPE( segment_not_present, (void) );
void				  _PROTOTYPE( stack_exception, (void) );
void				  _PROTOTYPE( general_protection, (void) );
void				  _PROTOTYPE( page_fault, (void) );
void				  _PROTOTYPE( copr_error, (void) );

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

/* Software interrupt handlers, in numerical order. */
_PROTOTYPE( void trp, (void) );
_PROTOTYPE( void s_call, (void) ), _PROTOTYPE( p_s_call, (void) );
_PROTOTYPE( void level0_call, (void) );

/* protect.c */
_PROTOTYPE( void prot_init, (void)					);
_PROTOTYPE( void init_codeseg, (struct segdesc_s *segdp, phys_bytes base,
		vir_bytes size, int privilege)				);
_PROTOTYPE( void init_dataseg, (struct segdesc_s *segdp, phys_bytes base,
		vir_bytes size, int privilege)				);
_PROTOTYPE( phys_bytes seg2phys, (U16_t seg)				);
_PROTOTYPE( void phys2seg, (u16_t *seg, vir_bytes *off, phys_bytes phys));
_PROTOTYPE( void enable_iop, (struct proc *pp)				);
_PROTOTYPE( void alloc_segments, (struct proc *rp)			);

#endif /* (CHIP == INTEL) */

#if (CHIP == M68000)
/* M68000 specific prototypes go here. */
#endif /* (CHIP == M68000) */

#endif /* PROTO_H */
