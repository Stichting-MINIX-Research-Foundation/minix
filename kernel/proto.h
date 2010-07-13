/* Function prototypes. */

#ifndef PROTO_H
#define PROTO_H

#include <minix/safecopies.h>
#include <machine/archtypes.h>
#include <sys/sigcontext.h>
#include <a.out.h>

/* Struct declarations. */
struct proc;
struct timer;

/* clock.c */
_PROTOTYPE( clock_t get_uptime, (void)					);
_PROTOTYPE( void set_timer, (struct timer *tp, clock_t t, tmr_func_t f)	);
_PROTOTYPE( void reset_timer, (struct timer *tp)			);
_PROTOTYPE( void ser_dump_proc, (void)					);

_PROTOTYPE( void cycles_accounting_init, (void)				);
/*
 * This functions start and stop accounting for process, kernel or idle cycles.
 * It inherently have to account for some kernel cycles for process too,
 * therefore it should be called asap after trapping to kernel and as late as
 * possible before returning to userspace. These function is architecture
 * dependent
 */
_PROTOTYPE( void context_stop, (struct proc * p)			);
/* this is a wrapper to make calling it from assembly easier */
_PROTOTYPE( void context_stop_idle, (void)				);
_PROTOTYPE( void restore_fpu, (struct proc *)				);
_PROTOTYPE( void save_fpu, (struct proc *)				);
_PROTOTYPE( void fpu_sigcontext, (struct proc *, struct sigframe *fr, struct sigcontext *sc)	);

/* main.c */
_PROTOTYPE( int main, (void)						);
_PROTOTYPE( void prepare_shutdown, (int how)				);
_PROTOTYPE( __dead void minix_shutdown, (struct timer *tp)		);

/* proc.c */

_PROTOTYPE( int do_ipc, (reg_t r1, reg_t r2, reg_t r3)			);
_PROTOTYPE( int mini_notify, (const struct proc *src, endpoint_t dst)	);
_PROTOTYPE( void enqueue, (struct proc *rp)				);
_PROTOTYPE( void dequeue, (const struct proc *rp)			);
_PROTOTYPE( void switch_to_user, (void)					);
_PROTOTYPE( struct proc * arch_finish_switch_to_user, (void)		);
_PROTOTYPE( struct proc *endpoint_lookup, (endpoint_t ep)		);
#if DEBUG_ENABLE_IPC_WARNINGS
_PROTOTYPE( int isokendpt_f, (const char *file, int line, endpoint_t e, int *p, int f));
#define isokendpt_d(e, p, f) isokendpt_f(__FILE__, __LINE__, (e), (p), (f))
#else
_PROTOTYPE( int isokendpt_f, (endpoint_t e, int *p, int f)		);
#define isokendpt_d(e, p, f) isokendpt_f((e), (p), (f))
#endif
_PROTOTYPE( void proc_no_time, (struct proc *p));

/* start.c */
_PROTOTYPE( void cstart, (u16_t cs, u16_t ds, u16_t mds,
				u16_t parmoff, u16_t parmsize)		);
_PROTOTYPE( char *env_get, (const char *key));

/* system.c */
_PROTOTYPE( int get_priv, (register struct proc *rc, int proc_type)	);
_PROTOTYPE( void set_sendto_bit, (const struct proc *rc, int id)	);
_PROTOTYPE( void unset_sendto_bit, (const struct proc *rc, int id)	);
_PROTOTYPE( void fill_sendto_mask, (const struct proc *rc, int mask)	);
_PROTOTYPE( void send_sig, (endpoint_t proc_nr, int sig_nr)		);
_PROTOTYPE( void cause_sig, (proc_nr_t proc_nr, int sig_nr)			);
_PROTOTYPE( void sig_delay_done, (struct proc *rp)			);
_PROTOTYPE( void kernel_call, (message *m_user, struct proc * caller)	);
_PROTOTYPE( void system_init, (void)					);
#define numap_local(proc_nr, vir_addr, bytes) \
	umap_local(proc_addr(proc_nr), D, (vir_addr), (bytes))
_PROTOTYPE( phys_bytes umap_grant, (struct proc *, cp_grant_id_t, vir_bytes));
_PROTOTYPE( void clear_endpoint, (struct proc *rc)			);
_PROTOTYPE( void clear_ipc_refs, (struct proc *rc, int caller_ret)	);
_PROTOTYPE( phys_bytes umap_bios, (vir_bytes vir_addr, vir_bytes bytes));
_PROTOTYPE( void kernel_call_resume, (struct proc *p));
_PROTOTYPE( int sched_proc, (struct proc *rp,
	unsigned priority, unsigned quantum));

/* system/do_newmap.c */
_PROTOTYPE( int newmap, (struct proc * caller, struct proc *rp,
					struct mem_map *map_ptr));

/* system/do_vtimer.c */
_PROTOTYPE( void vtimer_check, (struct proc *rp)			);

/* interrupt.c */
_PROTOTYPE( void put_irq_handler, (irq_hook_t *hook, int irq,
						 irq_handler_t handler)  );
_PROTOTYPE( void rm_irq_handler, (const irq_hook_t *hook)		);
_PROTOTYPE( void enable_irq, (const irq_hook_t *hook)			);
_PROTOTYPE( int disable_irq, (const irq_hook_t *hook)			);

/* debug.c */
_PROTOTYPE( int runqueues_ok, (void) );
_PROTOTYPE( char *rtsflagstr, (int flags) );
_PROTOTYPE( char *miscflagstr, (int flags) );
_PROTOTYPE( char *schedulerstr, (struct proc *scheduler) );
/* prints process information */
_PROTOTYPE( void print_proc, (struct proc *pp));
/* prints the given process and recursively all processes it depends on */
_PROTOTYPE( void print_proc_recursive, (struct proc *pp));
#if DEBUG_DUMPIPC
_PROTOTYPE( void printmsgrecv, (message *msg, struct proc *src, 
						struct proc *dst)	);
_PROTOTYPE( void printmsgsend, (message *msg, struct proc *src, 
						struct proc *dst)	);
_PROTOTYPE( void printmsgkcall, (message *msg, struct proc *proc)	);
_PROTOTYPE( void printmsgkresult, (message *msg, struct proc *proc)	);
#endif

/* system/do_safemap.c */
_PROTOTYPE( int map_invoke_vm, (struct proc * caller, int req_type,
		endpoint_t end_d, int seg_d, vir_bytes off_d,
		endpoint_t end_s, int seg_s, vir_bytes off_s,
		size_t size, int flag));

/* system/do_safecopy.c */
_PROTOTYPE( int verify_grant, (endpoint_t, endpoint_t,
	cp_grant_id_t, vir_bytes, int,
	vir_bytes, vir_bytes *, endpoint_t *));

/* system/do_sysctl.c */
_PROTOTYPE( int do_sysctl, (struct proc * caller, message *m));

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
#define virtual_copy(src, dst, bytes) \
				virtual_copy_f(NULL, src, dst, bytes, 0)
#define virtual_copy_vmcheck(caller, src, dst, bytes) \
				virtual_copy_f(caller, src, dst, bytes, 1)
_PROTOTYPE( int virtual_copy_f, (struct proc * caller,
			struct vir_addr *src, struct vir_addr *dst,
			vir_bytes bytes, int vmcheck)		);
_PROTOTYPE( int data_copy, (endpoint_t from, vir_bytes from_addr,
	endpoint_t to, vir_bytes to_addr, size_t bytes));
_PROTOTYPE( int data_copy_vmcheck, (struct proc *,
	endpoint_t from, vir_bytes from_addr,
	endpoint_t to, vir_bytes to_addr, size_t bytes));
_PROTOTYPE( void alloc_segments, (struct proc *rp)                      );
_PROTOTYPE( void vm_init, (struct proc *first)        			);
_PROTOTYPE( void vm_stop, (void)        				);
_PROTOTYPE( phys_bytes umap_local, (register struct proc *rp, int seg,
	vir_bytes vir_addr, vir_bytes bytes));
_PROTOTYPE( phys_bytes umap_remote, (const struct proc* rp, int seg,
        vir_bytes vir_addr, vir_bytes bytes)				);
_PROTOTYPE( phys_bytes umap_virtual, (struct proc* rp,
			int seg, vir_bytes vir_addr, vir_bytes bytes)	);
_PROTOTYPE( phys_bytes seg2phys, (u16_t)                                );
_PROTOTYPE( int vm_phys_memset, (phys_bytes source, u8_t pattern,
                phys_bytes count)                                       );
_PROTOTYPE( vir_bytes alloc_remote_segment, (u32_t *, segframe_t *,
        int, phys_bytes, vir_bytes, int));
_PROTOTYPE( int intr_init, (int, int)					);
_PROTOTYPE( void halt_cpu, (void)                                	);
_PROTOTYPE( void arch_init, (void)                                     );
_PROTOTYPE( void ser_putc, (char)						);
_PROTOTYPE( __dead void arch_shutdown, (int)				);
_PROTOTYPE( __dead void arch_monitor, (void)				);
_PROTOTYPE( void arch_get_aout_headers, (int i, struct exec *h)		);
_PROTOTYPE( void restore_user_context, (struct proc * p)                );
_PROTOTYPE( void read_tsc, (unsigned long *high, unsigned long *low)    );
_PROTOTYPE( int arch_init_profile_clock, (u32_t freq)			);
_PROTOTYPE( void arch_stop_profile_clock, (void)			);
_PROTOTYPE( void arch_ack_profile_clock, (void)				);
_PROTOTYPE( void do_ser_debug, (void)					);
_PROTOTYPE( int arch_get_params, (char *parm, int max));
_PROTOTYPE( int arch_set_params, (char *parm, int max));
_PROTOTYPE( void arch_pre_exec, (struct proc *pr, u32_t, u32_t));
_PROTOTYPE( int arch_umap, (const struct proc *pr, vir_bytes, vir_bytes,
	int, phys_bytes *));
_PROTOTYPE( int arch_do_vmctl, (message *m_ptr, struct proc *p)); 
_PROTOTYPE( int vm_contiguous, (const struct proc *targetproc, vir_bytes vir_buf, size_t count));
_PROTOTYPE( void proc_stacktrace, (struct proc *proc)	         );
_PROTOTYPE( int vm_lookup, (const struct proc *proc, vir_bytes virtual, vir_bytes *result, u32_t *ptent));
_PROTOTYPE( void delivermsg, (struct proc *target));
_PROTOTYPE( void arch_do_syscall, (struct proc *proc)			);
_PROTOTYPE( int arch_phys_map, (int index, phys_bytes *addr,
	phys_bytes *len, int *flags));
_PROTOTYPE( int arch_phys_map_reply, (int index, vir_bytes addr));
_PROTOTYPE( int arch_enable_paging, (struct proc * caller, const message * m_ptr));

_PROTOTYPE( int copy_msg_from_user, (struct proc * p, message * user_mbuf,
							message * dst));
_PROTOTYPE( int copy_msg_to_user, (struct proc * p, message * src,
							message * user_mbuf));
_PROTOTYPE(void switch_address_space, (struct proc * p));
_PROTOTYPE(void release_address_space, (struct proc *pr));

_PROTOTYPE(void enable_fpu_exception, (void));
_PROTOTYPE(void disable_fpu_exception, (void));
_PROTOTYPE(void release_fpu, (void));

/* utility.c */
_PROTOTYPE( void cpu_print_freq, (unsigned cpu));
#endif /* PROTO_H */
