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

/* utility.c */
_PROTOTYPE( int kprintf, (const char *fmt, ...)				);
_PROTOTYPE( void minix_panic, (char *s, int n)				);

/* proc.c */
_PROTOTYPE( int sys_call, (int call_nr, int src_dst, 
					message *m_ptr, long bit_map)	);
_PROTOTYPE( int lock_notify, (int src, int dst)				);
_PROTOTYPE( int mini_notify, (struct proc *src, endpoint_t dst)		);
_PROTOTYPE( int lock_send, (int dst, message *m_ptr)			);
_PROTOTYPE( void enqueue, (struct proc *rp)				);
_PROTOTYPE( void dequeue, (struct proc *rp)				);
_PROTOTYPE( void balance_queues, (struct timer *tp)			);
_PROTOTYPE( struct proc * schedcheck, (void)				);
_PROTOTYPE( struct proc * arch_finish_schedcheck, (void)		);
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
_PROTOTYPE( void set_sendto_bit, (struct proc *rc, int id)		);
_PROTOTYPE( void unset_sendto_bit, (struct proc *rc, int id)		);
_PROTOTYPE( void send_sig, (int proc_nr, int sig_nr)			);
_PROTOTYPE( void cause_sig, (proc_nr_t proc_nr, int sig_nr)			);
_PROTOTYPE( void sig_delay_done, (struct proc *rp)			);
_PROTOTYPE( void sys_task, (void)					);
#define numap_local(proc_nr, vir_addr, bytes) \
	umap_local(proc_addr(proc_nr), D, (vir_addr), (bytes))
_PROTOTYPE( phys_bytes umap_grant, (struct proc *, cp_grant_id_t,
	vir_bytes));
_PROTOTYPE( void clear_endpoint, (struct proc *rc)			);
_PROTOTYPE( phys_bytes umap_bios, (vir_bytes vir_addr, vir_bytes bytes));

/* system/do_newmap.c */
_PROTOTYPE( int newmap, (struct proc *rp, struct mem_map *map_ptr)	);

/* system/do_vtimer.c */
_PROTOTYPE( void vtimer_check, (struct proc *rp)			);

/* interrupt.c */
_PROTOTYPE( void put_irq_handler, (irq_hook_t *hook, int irq,
						 irq_handler_t handler)  );
_PROTOTYPE( void rm_irq_handler, (irq_hook_t *hook)                      );
_PROTOTYPE( void enable_irq, (irq_hook_t *hook)                        	);
_PROTOTYPE( int disable_irq, (irq_hook_t *hook)                        );

/* debug.c */
#if DEBUG_SCHED_CHECK
#define CHECK_RUNQUEUES check_runqueues_f(__FILE__, __LINE__)
_PROTOTYPE( void check_runqueues_f, (char *file, int line) );
#endif
_PROTOTYPE( char *rtsflagstr, (int flags) );
_PROTOTYPE( char *miscflagstr, (int flags) );

/* system/do_safemap.c */
_PROTOTYPE( int map_invoke_vm, (int req_type,
		endpoint_t end_d, int seg_d, vir_bytes off_d,
		endpoint_t end_s, int seg_s, vir_bytes off_s,
		size_t size, int flag));

/* system/do_safecopy.c */
_PROTOTYPE( int verify_grant, (endpoint_t, endpoint_t, cp_grant_id_t, vir_bytes,
	int, vir_bytes, vir_bytes *, endpoint_t *));

/* system/do_sysctl.c */
_PROTOTYPE( int do_sysctl, (message *m));

#if SPROFILE
/* profile.c */
_PROTOTYPE( void init_profile_clock, (u32_t)				);
_PROTOTYPE( void stop_profile_clock, (void)				);
#endif

/* functions defined in architecture-dependent files. */
_PROTOTYPE( void prot_init, (void)                     			);
_PROTOTYPE( phys_bytes phys_copy, (phys_bytes source, phys_bytes dest,
                phys_bytes count)                                       );
_PROTOTYPE( void phys_copy_fault, (void));
_PROTOTYPE( void phys_copy_fault_in_kernel, (void));
#define virtual_copy(src, dst, bytes) virtual_copy_f(src, dst, bytes, 0)
#define virtual_copy_vmcheck(src, dst, bytes) virtual_copy_f(src, dst, bytes, 1)
_PROTOTYPE( int virtual_copy_f, (struct vir_addr *src, struct vir_addr *dst, 
				vir_bytes bytes, int vmcheck)		);
_PROTOTYPE( int data_copy, (endpoint_t from, vir_bytes from_addr,
	endpoint_t to, vir_bytes to_addr, size_t bytes));
_PROTOTYPE( int data_copy_vmcheck, (endpoint_t from, vir_bytes from_addr,
	endpoint_t to, vir_bytes to_addr, size_t bytes));
_PROTOTYPE( void alloc_segments, (struct proc *rp)                      );
_PROTOTYPE( void vm_init, (struct proc *first)        			);
_PROTOTYPE( phys_bytes umap_local, (register struct proc *rp, int seg,
	vir_bytes vir_addr, vir_bytes bytes));
_PROTOTYPE( void cp_mess, (int src,phys_clicks src_clicks,
        vir_bytes src_offset, phys_clicks dst_clicks, vir_bytes dst_offset));
_PROTOTYPE( phys_bytes umap_remote, (struct proc* rp, int seg,
        vir_bytes vir_addr, vir_bytes bytes)				);
_PROTOTYPE( phys_bytes umap_virtual, (struct proc* rp, int seg,
        vir_bytes vir_addr, vir_bytes bytes)				);
_PROTOTYPE( phys_bytes seg2phys, (U16_t)                                );
_PROTOTYPE( int vm_phys_memset, (phys_bytes source, u8_t pattern,
                phys_bytes count)                                       );
_PROTOTYPE( vir_bytes alloc_remote_segment, (u32_t *, segframe_t *,
        int, phys_bytes, vir_bytes, int));
_PROTOTYPE( int intr_init, (int, int)					);
_PROTOTYPE( int intr_disabled, (void)					);
_PROTOTYPE( void halt_cpu, (void)                                	);
_PROTOTYPE( void arch_init, (void)                                     );
_PROTOTYPE( void ser_putc, (char)						);
_PROTOTYPE( void arch_shutdown, (int)					);
_PROTOTYPE( void arch_monitor, (void)					);
_PROTOTYPE( void arch_get_aout_headers, (int i, struct exec *h)		);
_PROTOTYPE( void restart, (void)                                        );
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
_PROTOTYPE( void proc_stacktrace, (struct proc *proc)	         );
_PROTOTYPE( int vm_lookup, (struct proc *proc, vir_bytes virtual, vir_bytes *result, u32_t *ptent));
_PROTOTYPE( int delivermsg, (struct proc *target));
_PROTOTYPE( void arch_do_syscall, (struct proc *proc)			);
_PROTOTYPE( int arch_phys_map, (int index, phys_bytes *addr,
	phys_bytes *len, int *flags));
_PROTOTYPE( int arch_phys_map_reply, (int index, vir_bytes addr));
_PROTOTYPE( int arch_enable_paging, (void));

#endif /* PROTO_H */
