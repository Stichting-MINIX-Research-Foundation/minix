/* Function prototypes. */

#ifndef PROTO_H
#define PROTO_H

#include <minix/safecopies.h>
#include <archtypes.h>
#include <a.out.h>

/* Struct declarations. */
struct proc;
struct timer;

/* clock.c */
_PROTOTYPE( void clock_task, (void)					);
_PROTOTYPE( clock_t get_uptime, (void)					);
_PROTOTYPE( void set_timer, (struct timer *tp, clock_t t, tmr_func_t f)	);
_PROTOTYPE( void reset_timer, (struct timer *tp)			);
_PROTOTYPE( void ser_dump_proc, (void)					);

/* main.c */
_PROTOTYPE( void main, (void)						);
_PROTOTYPE( void prepare_shutdown, (int how)				);
_PROTOTYPE( void minix_shutdown, (struct timer *tp)			);
_PROTOTYPE( void idle_task, (void)					);

/* utility.c */
_PROTOTYPE( int kprintf, (const char *fmt, ...)				);
_PROTOTYPE( void minix_panic, (char *s, int n)				);

/* proc.c */
_PROTOTYPE( int sys_call, (int call_nr, int src_dst, 
					message *m_ptr, long bit_map)	);
_PROTOTYPE( void sys_call_restart, (struct proc *caller)		);
_PROTOTYPE( int lock_notify, (int src, int dst)				);
_PROTOTYPE( int soft_notify, (int dst)					);
_PROTOTYPE( int lock_send, (int dst, message *m_ptr)			);
_PROTOTYPE( void lock_enqueue, (struct proc *rp)			);
_PROTOTYPE( void lock_dequeue, (struct proc *rp)			);
_PROTOTYPE( void enqueue, (struct proc *rp)				);
_PROTOTYPE( void dequeue, (struct proc *rp)				);
_PROTOTYPE( void balance_queues, (struct timer *tp)			);
_PROTOTYPE( struct proc *endpoint_lookup, (endpoint_t ep)		);
#if DEBUG_ENABLE_IPC_WARNINGS
_PROTOTYPE( int isokendpt_f, (char *file, int line, endpoint_t e, int *p, int f));
#define isokendpt_d(e, p, f) isokendpt_f(__FILE__, __LINE__, (e), (p), (f))
#else
_PROTOTYPE( int isokendpt_f, (endpoint_t e, int *p, int f)		);
#define isokendpt_d(e, p, f) isokendpt_f((e), (p), (f))
#endif

/* start.c */
_PROTOTYPE( void cstart, (U16_t cs, U16_t ds, U16_t mds,
				U16_t parmoff, U16_t parmsize)		);

/* system.c */
_PROTOTYPE( int get_priv, (register struct proc *rc, int proc_type)	);
_PROTOTYPE( void send_sig, (int proc_nr, int sig_nr)			);
_PROTOTYPE( void cause_sig, (int proc_nr, int sig_nr)			);
_PROTOTYPE( void sys_task, (void)					);
_PROTOTYPE( void get_randomness, (int source)				);
#define numap_local(proc_nr, vir_addr, bytes) \
	umap_local(proc_addr(proc_nr), D, (vir_addr), (bytes))
_PROTOTYPE( phys_bytes umap_grant, (struct proc *, cp_grant_id_t,
	vir_bytes));
_PROTOTYPE( vir_bytes vir_verify_grant, (struct proc *, endpoint_t,
	cp_grant_id_t, vir_bytes, vir_bytes, int, endpoint_t *));
_PROTOTYPE( void clear_endpoint, (struct proc *rc)			);
_PROTOTYPE( phys_bytes umap_bios, (vir_bytes vir_addr, vir_bytes bytes));
_PROTOTYPE( phys_bytes umap_verify_grant, (struct proc *rp, endpoint_t grantee,  cp_grant_id_t grant, vir_bytes offset, vir_bytes bytes, int access));


/* system/do_newmap.c */
_PROTOTYPE( int newmap, (struct proc *rp, struct mem_map *map_ptr)	);

/* interrupt.c */
_PROTOTYPE( void intr_handle,     (irq_hook_t *hook)		          );
_PROTOTYPE( void put_irq_handler, (irq_hook_t *hook, int irq,
						 irq_handler_t handler)  );
_PROTOTYPE( void rm_irq_handler, (irq_hook_t *hook)                      );
_PROTOTYPE( void enable_irq, (irq_hook_t *hook)                        	);
_PROTOTYPE( int disable_irq, (irq_hook_t *hook)                        );
_PROTOTYPE( void cons_setc, (int pos, int c)				);
_PROTOTYPE( void cons_seth, (int pos, int n)				);

/* debug.c */
#if DEBUG_SCHED_CHECK
#define CHECK_RUNQUEUES check_runqueues_f(__FILE__, __LINE__)
_PROTOTYPE( void check_runqueues_f, (char *file, int line) );
#endif
_PROTOTYPE( void timer_start, (int cat, char *name) );
_PROTOTYPE( void timer_end, (int cat) );

/* system/do_safecopy.c */
_PROTOTYPE( int verify_grant, (endpoint_t, endpoint_t, cp_grant_id_t, vir_bytes,
	int, vir_bytes, vir_bytes *, endpoint_t *));

#if SPROFILE
/* profile.c */
_PROTOTYPE( void init_profile_clock, (u32_t)				);
_PROTOTYPE( void stop_profile_clock, (void)				);
#endif

/* functions defined in architecture-dependent files. */
_PROTOTYPE( void phys_copy, (phys_bytes source, phys_bytes dest,
                phys_bytes count)                                       );
#define virtual_copy(src, dst, bytes) virtual_copy_f(src, dst, bytes, 0)
#define virtual_copy_vmcheck(src, dst, bytes) virtual_copy_f(src, dst, bytes, 1)
_PROTOTYPE( int virtual_copy_f, (struct vir_addr *src, struct vir_addr *dst, 
				vir_bytes bytes, int vmcheck)		);
_PROTOTYPE( int data_copy, (endpoint_t from, vir_bytes from_addr,
	endpoint_t to, vir_bytes to_addr, size_t bytes));
#define data_copy_to(d, p, v, n) data_copy(SYSTEM, (d), (p), (v), (n));
#define data_copy_from(d, p, v, n) data_copy((p), (v), SYSTEM, (d), (n));
_PROTOTYPE( void alloc_segments, (struct proc *rp)                      );
_PROTOTYPE( void vm_init, (void)                       			);
_PROTOTYPE( void vm_map_range, (u32_t base, u32_t size, u32_t offset)   );
_PROTOTYPE( int vm_copy, (vir_bytes src, struct proc *srcproc,
	vir_bytes dst, struct proc *dstproc, phys_bytes bytes));
_PROTOTYPE( phys_bytes umap_local, (register struct proc *rp, int seg,
	vir_bytes vir_addr, vir_bytes bytes));
_PROTOTYPE( void cp_mess, (int src,phys_clicks src_clicks,
        vir_bytes src_offset, phys_clicks dst_clicks, vir_bytes dst_offset));
_PROTOTYPE( phys_bytes umap_remote, (struct proc* rp, int seg,
        vir_bytes vir_addr, vir_bytes bytes)				);
_PROTOTYPE( phys_bytes umap_virtual, (struct proc* rp, int seg,
        vir_bytes vir_addr, vir_bytes bytes)				);
_PROTOTYPE( phys_bytes seg2phys, (U16_t)                                );
_PROTOTYPE( void phys_memset, (phys_bytes source, unsigned long pattern,
                phys_bytes count)                                       );
_PROTOTYPE( vir_bytes alloc_remote_segment, (u32_t *, segframe_t *,
        int, phys_bytes, vir_bytes, int));
_PROTOTYPE( int arch_init_clock, (void)					);
_PROTOTYPE( clock_t read_clock, (void)					);
_PROTOTYPE( void clock_stop, (void)    					);
_PROTOTYPE( int intr_init, (int)					);
_PROTOTYPE( int intr_disabled, (void)					);
_PROTOTYPE( int intr_unmask, (irq_hook_t* hook)                     );
_PROTOTYPE( int intr_mask, (irq_hook_t* hook)                    );
_PROTOTYPE( void idle_task, (void)                                     );
_PROTOTYPE( void system_init, (void)                                     );
_PROTOTYPE( void ser_putc, (char)						);
_PROTOTYPE( void arch_shutdown, (int)					);
_PROTOTYPE( void arch_get_aout_headers, (int i, struct exec *h)		);
_PROTOTYPE( void restart, (void)                                        );
_PROTOTYPE( void idle_task, (void)                                     );
_PROTOTYPE( void read_tsc, (unsigned long *high, unsigned long *low)    );
_PROTOTYPE( int arch_init_profile_clock, (u32_t freq)			);
_PROTOTYPE( void arch_stop_profile_clock, (void)			);
_PROTOTYPE( void arch_ack_profile_clock, (void)				);
_PROTOTYPE( void do_ser_debug, (void)					);
_PROTOTYPE( int arch_get_params, (char *parm, int max));
_PROTOTYPE( int arch_set_params, (char *parm, int max));
_PROTOTYPE( int arch_pre_exec, (struct proc *pr, u32_t, u32_t));
_PROTOTYPE( int arch_umap, (struct proc *pr, vir_bytes, vir_bytes,
	int, phys_bytes *));
_PROTOTYPE( int arch_do_vmctl, (message *m_ptr, struct proc *p)); 
_PROTOTYPE( int vm_contiguous, (struct proc *targetproc, u32_t vir_buf, size_t count));
_PROTOTYPE( int vm_checkrange, (struct proc *caller, struct proc *target, 
	vir_bytes start, vir_bytes length, int writeflag, int checkonly));
_PROTOTYPE( void proc_stacktrace, (struct proc *proc)	         );
_PROTOTYPE( int vm_lookup, (struct proc *proc, vir_bytes virtual, vir_bytes *result, u32_t *ptent));

#endif /* PROTO_H */
