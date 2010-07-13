/* Prototypes for system library functions. */

#ifndef _SYSLIB_H
#define _SYSLIB_H

#ifndef _TYPES_H
#include <minix/types.h>
#endif

#ifndef _IPC_H
#include <minix/ipc.h>
#endif

#include <minix/u64.h>

#ifndef _DEVIO_H
#include <minix/devio.h>
#endif

#include <minix/safecopies.h>
#include <minix/sef.h>
#include <machine/mcontext.h>

/* Forward declaration */
struct reg86u;
struct rs_pci;

#define SYSTASK SYSTEM

/*==========================================================================* 
 * Minix system library. 						    *
 *==========================================================================*/ 
_PROTOTYPE( int _taskcall, (endpoint_t who, int syscallnr, message *msgptr));
_PROTOTYPE( int _kernel_call, (int syscallnr, message *msgptr));

_PROTOTYPE( int sys_abort, (int how, ...));
_PROTOTYPE( int sys_enable_iop, (endpoint_t proc_ep));
_PROTOTYPE( int sys_exec, (endpoint_t proc_ep, char *ptr,  
				char *aout, vir_bytes initpc));
_PROTOTYPE( int sys_fork, (endpoint_t parent, endpoint_t child, endpoint_t *,
	struct mem_map *ptr, u32_t vm, vir_bytes *));
_PROTOTYPE( int sys_newmap, (endpoint_t proc_ep, struct mem_map *ptr));
_PROTOTYPE( int sys_clear, (endpoint_t proc_ep));
_PROTOTYPE( int sys_exit, (void));
_PROTOTYPE( int sys_trace, (int req, endpoint_t proc_ep, long addr, long *data_p));

_PROTOTYPE( int sys_schedule, (endpoint_t proc_ep, unsigned priority, unsigned quantum));
_PROTOTYPE( int sys_schedctl, (unsigned flags, endpoint_t proc_ep,
	unsigned priority, unsigned quantum));

/* Shorthands for sys_runctl() system call. */
#define sys_stop(proc_ep) sys_runctl(proc_ep, RC_STOP, 0)
#define sys_delay_stop(proc_ep) sys_runctl(proc_ep, RC_STOP, RC_DELAY)
#define sys_resume(proc_ep) sys_runctl(proc_ep, RC_RESUME, 0)
_PROTOTYPE( int sys_runctl, (endpoint_t proc_ep, int action, int flags));

_PROTOTYPE( int sys_update, (endpoint_t src_ep, endpoint_t dst_ep));
_PROTOTYPE( int sys_statectl, (int request));
_PROTOTYPE( int sys_privctl, (endpoint_t proc_ep, int req, void *p));
_PROTOTYPE( int sys_privquery_mem, (endpoint_t proc_ep,
	phys_bytes physstart, phys_bytes physlen));
_PROTOTYPE( int sys_setgrant, (cp_grant_t *grants, int ngrants));

_PROTOTYPE( int sys_int86, (struct reg86u *reg86p));
_PROTOTYPE( int sys_vm_setbuf, (phys_bytes base, phys_bytes size,
							phys_bytes high));
_PROTOTYPE( int sys_vm_map, (endpoint_t proc_ep, int do_map,
	phys_bytes base, phys_bytes size, phys_bytes offset));
_PROTOTYPE( int sys_vmctl, (endpoint_t who, int param, u32_t value));
_PROTOTYPE( int sys_vmctl_get_cr3_i386, (endpoint_t who, u32_t *cr3)  );
_PROTOTYPE( int sys_vmctl_get_memreq, (endpoint_t *who, vir_bytes *mem,
        vir_bytes *len, int *wrflag, endpoint_t *who_s, vir_bytes *mem_s,
        endpoint_t *) );
_PROTOTYPE( int sys_vmctl_enable_paging, (void * data));

_PROTOTYPE( int sys_readbios, (phys_bytes address, void *buf, size_t size));
_PROTOTYPE( int sys_stime, (time_t boottime));
_PROTOTYPE( int sys_sysctl, (int ctl, char *arg1, int arg2));
_PROTOTYPE( int sys_sysctl_stacktrace, (endpoint_t who));
_PROTOTYPE( int sys_vmctl_get_mapping, (int index, phys_bytes *addr,
	phys_bytes *len, int *flags));
_PROTOTYPE( int sys_vmctl_reply_mapping, (int index, vir_bytes addr));
_PROTOTYPE( int sys_vmctl_set_addrspace, (endpoint_t who,
        phys_bytes ptroot, void *ptroot_v));


/* Shorthands for sys_sdevio() system call. */
#define sys_insb(port, proc_ep, buffer, count) \
  sys_sdevio(DIO_INPUT_BYTE, port, proc_ep, buffer, count, 0)
#define sys_insw(port, proc_ep, buffer, count) \
  sys_sdevio(DIO_INPUT_WORD, port, proc_ep, buffer, count, 0)
#define sys_outsb(port, proc_ep, buffer, count) \
  sys_sdevio(DIO_OUTPUT_BYTE, port, proc_ep, buffer, count, 0)
#define sys_outsw(port, proc_ep, buffer, count) \
  sys_sdevio(DIO_OUTPUT_WORD, port, proc_ep, buffer, count, 0)
#define sys_safe_insb(port, ept, grant, offset, count) \
  sys_sdevio(DIO_SAFE_INPUT_BYTE, port, ept, (void*)grant, count, offset)
#define sys_safe_outsb(port, ept, grant, offset, count) \
  sys_sdevio(DIO_SAFE_OUTPUT_BYTE, port, ept, (void*)grant, count, offset)
#define sys_safe_insw(port, ept, grant, offset, count) \
  sys_sdevio(DIO_SAFE_INPUT_WORD, port, ept, (void*)grant, count, offset)
#define sys_safe_outsw(port, ept, grant, offset, count) \
  sys_sdevio(DIO_SAFE_OUTPUT_WORD, port, ept, (void*)grant, count, offset)
_PROTOTYPE( int sys_sdevio, (int req, long port, endpoint_t proc_ep,
	void *buffer, int count, vir_bytes offset));
_PROTOTYPE(void *alloc_contig, (size_t len, int flags, phys_bytes *phys));
_PROTOTYPE(int free_contig, (void *addr, size_t len));

#define AC_ALIGN4K	0x01
#define AC_LOWER16M	0x02
#define AC_ALIGN64K	0x04
#define AC_LOWER1M	0x08

/* Clock functionality: get system times, (un)schedule an alarm call, or
 * retrieve/set a process-virtual timer.
 */
_PROTOTYPE( int sys_times, (endpoint_t proc_ep, clock_t *user_time,
	clock_t *sys_time, clock_t *uptime, time_t *boottime));
_PROTOTYPE(int sys_setalarm, (clock_t exp_time, int abs_time));
_PROTOTYPE( int sys_vtimer, (endpoint_t proc_nr, int which, clock_t *newval,
	clock_t *oldval));

/* Shorthands for sys_irqctl() system call. */
#define sys_irqdisable(hook_id) \
    sys_irqctl(IRQ_DISABLE, 0, 0, hook_id) 
#define sys_irqenable(hook_id) \
    sys_irqctl(IRQ_ENABLE, 0, 0, hook_id) 
#define sys_irqsetpolicy(irq_vec, policy, hook_id) \
    sys_irqctl(IRQ_SETPOLICY, irq_vec, policy, hook_id)
#define sys_irqrmpolicy(hook_id) \
    sys_irqctl(IRQ_RMPOLICY, 0, 0, hook_id)
_PROTOTYPE ( int sys_irqctl, (int request, int irq_vec, int policy,
    int *irq_hook_id) );

/* Shorthands for sys_vircopy() and sys_physcopy() system calls. */
#define sys_biosin(bios_vir, dst_vir, bytes) \
	sys_vircopy(SELF, BIOS_SEG, bios_vir, SELF, D, dst_vir, bytes)
#define sys_biosout(src_vir, bios_vir, bytes) \
	sys_vircopy(SELF, D, src_vir, SELF, BIOS_SEG, bios_vir, bytes)
#define sys_datacopy(src_proc, src_vir, dst_proc, dst_vir, bytes) \
	sys_vircopy(src_proc, D, src_vir, dst_proc, D, dst_vir, bytes)
#define sys_textcopy(src_proc, src_vir, dst_proc, dst_vir, bytes) \
	sys_vircopy(src_proc, T, src_vir, dst_proc, T, dst_vir, bytes)
#define sys_stackcopy(src_proc, src_vir, dst_proc, dst_vir, bytes) \
	sys_vircopy(src_proc, S, src_vir, dst_proc, S, dst_vir, bytes)
_PROTOTYPE(int sys_vircopy, (endpoint_t src_proc, int src_s, vir_bytes src_v,
	endpoint_t dst_proc, int dst_seg, vir_bytes dst_vir, phys_bytes bytes));

#define sys_abscopy(src_phys, dst_phys, bytes) \
	sys_physcopy(NONE, PHYS_SEG, src_phys, NONE, PHYS_SEG, dst_phys, bytes)
_PROTOTYPE(int sys_physcopy, (endpoint_t src_proc, int src_seg, vir_bytes src_vir,
	endpoint_t dst_proc, int dst_seg, vir_bytes dst_vir, phys_bytes bytes));


/* Grant-based copy functions. */
_PROTOTYPE(int sys_safecopyfrom, (endpoint_t source, cp_grant_id_t grant,
	vir_bytes grant_offset, vir_bytes my_address, size_t bytes, int my_seg));
_PROTOTYPE(int sys_safecopyto, (endpoint_t dest, cp_grant_id_t grant,
	vir_bytes grant_offset, vir_bytes my_address, size_t bytes, int my_seg));
_PROTOTYPE(int sys_vsafecopy, (struct vscp_vec *copyvec, int elements));

_PROTOTYPE(int sys_memset, (unsigned long pattern, 
		phys_bytes base, phys_bytes bytes));

/* Grant-based map functions. */
_PROTOTYPE(int sys_safemap, (endpoint_t grantor, cp_grant_id_t grant,
	vir_bytes grant_offset, vir_bytes my_address, size_t bytes, int my_seg,
	int writable));
_PROTOTYPE(int sys_saferevmap_gid, (cp_grant_id_t grant));
_PROTOTYPE(int sys_saferevmap_addr, (vir_bytes addr));
_PROTOTYPE(int sys_safeunmap, (int my_seg, vir_bytes my_address));

_PROTOTYPE(int sys_umap, (endpoint_t proc_ep, int seg, vir_bytes vir_addr,
	 vir_bytes bytes, phys_bytes *phys_addr));
_PROTOTYPE(int sys_umap_data_fb, (endpoint_t proc_ep, vir_bytes vir_addr,
	 vir_bytes bytes, phys_bytes *phys_addr));
_PROTOTYPE(int sys_segctl, (int *index, u16_t *seg, vir_bytes *off,
	phys_bytes phys, vir_bytes size));

/* Shorthands for sys_getinfo() system call. */
#define sys_getkmessages(dst)	sys_getinfo(GET_KMESSAGES, dst, 0,0,0)
#define sys_getkinfo(dst)	sys_getinfo(GET_KINFO, dst, 0,0,0)
#define sys_getloadinfo(dst)	sys_getinfo(GET_LOADINFO, dst, 0,0,0)
#define sys_getmachine(dst)	sys_getinfo(GET_MACHINE, dst, 0,0,0)
#define sys_getproctab(dst)	sys_getinfo(GET_PROCTAB, dst, 0,0,0)
#define sys_getprivtab(dst)	sys_getinfo(GET_PRIVTAB, dst, 0,0,0)
#define sys_getproc(dst,nr)	sys_getinfo(GET_PROC, dst, 0,0, nr)
#define sys_getrandomness(dst)	sys_getinfo(GET_RANDOMNESS, dst, 0,0,0)
#define sys_getrandom_bin(d,b)	sys_getinfo(GET_RANDOMNESS_BIN, d, 0,0,b)
#define sys_getimage(dst)	sys_getinfo(GET_IMAGE, dst, 0,0,0)
#define sys_getirqhooks(dst)	sys_getinfo(GET_IRQHOOKS, dst, 0,0,0)
#define sys_getirqactids(dst)	sys_getinfo(GET_IRQACTIDS, dst, 0,0,0)
#define sys_getmonparams(v,vl)	sys_getinfo(GET_MONPARAMS, v,vl, 0,0)
#define sys_getschedinfo(v1,v2)	sys_getinfo(GET_SCHEDINFO, v1,0, v2,0)
#define sys_getlocktimings(dst)	sys_getinfo(GET_LOCKTIMING, dst, 0,0,0)
#define sys_getpriv(dst, nr)	sys_getinfo(GET_PRIV, dst, 0,0, nr)
#define sys_getidletsc(dst)	sys_getinfo(GET_IDLETSC, dst, 0,0,0)
#define sys_getaoutheader(dst,nr) sys_getinfo(GET_AOUTHEADER, dst, 0,0,nr)
_PROTOTYPE(int sys_getinfo, (int request, void *val_ptr, int val_len,
				 void *val_ptr2, int val_len2)		);
_PROTOTYPE(int sys_whoami, (endpoint_t *ep, char *name, int namelen,
				 int *priv_flags));

/* Signal control. */
_PROTOTYPE(int sys_kill, (endpoint_t proc_ep, int sig) );
_PROTOTYPE(int sys_sigsend, (endpoint_t proc_ep, struct sigmsg *sig_ctxt) ); 
_PROTOTYPE(int sys_sigreturn, (endpoint_t proc_ep, struct sigmsg *sig_ctxt) );
_PROTOTYPE(int sys_getksig, (endpoint_t *proc_ep, sigset_t *k_sig_map) ); 
_PROTOTYPE(int sys_endksig, (endpoint_t proc_ep) );

/* NOTE: two different approaches were used to distinguish the device I/O
 * types 'byte', 'word', 'long': the latter uses #define and results in a
 * smaller implementation, but looses the static type checking.
 */
_PROTOTYPE(int sys_voutb, (pvb_pair_t *pvb_pairs, int nr_ports)		);
_PROTOTYPE(int sys_voutw, (pvw_pair_t *pvw_pairs, int nr_ports)		);
_PROTOTYPE(int sys_voutl, (pvl_pair_t *pvl_pairs, int nr_ports)		);
_PROTOTYPE(int sys_vinb, (pvb_pair_t *pvb_pairs, int nr_ports)		);
_PROTOTYPE(int sys_vinw, (pvw_pair_t *pvw_pairs, int nr_ports)		);
_PROTOTYPE(int sys_vinl, (pvl_pair_t *pvl_pairs, int nr_ports)		);

/* Shorthands for sys_out() system call. */
#define sys_outb(p,v)	sys_out((p), (unsigned long) (v), _DIO_BYTE)
#define sys_outw(p,v)	sys_out((p), (unsigned long) (v), _DIO_WORD)
#define sys_outl(p,v)	sys_out((p), (unsigned long) (v), _DIO_LONG)
_PROTOTYPE(int sys_out, (int port, unsigned long value, int type)	); 

/* Shorthands for sys_in() system call. */
#define sys_inb(p,v)	sys_in((p), (v), _DIO_BYTE)
#define sys_inw(p,v)	sys_in((p), (v), _DIO_WORD)
#define sys_inl(p,v)	sys_in((p), (v), _DIO_LONG)
_PROTOTYPE(int sys_in, (int port, unsigned long *value, int type)	);

/* pci.c */
_PROTOTYPE( void pci_init, (void)					);
_PROTOTYPE( void pci_init1, (char *name)				);
_PROTOTYPE( int pci_first_dev, (int *devindp, u16_t *vidp, u16_t *didp)	);
_PROTOTYPE( int pci_next_dev, (int *devindp, u16_t *vidp, u16_t *didp)	);
_PROTOTYPE( int pci_find_dev, (u8_t bus, u8_t dev, u8_t func,
							int *devindp)	);
_PROTOTYPE( void pci_reserve, (int devind)				);
_PROTOTYPE( int pci_reserve_ok, (int devind)				);
_PROTOTYPE( void pci_ids, (int devind, u16_t *vidp, u16_t *didp)	);
_PROTOTYPE( void pci_rescan_bus, (u8_t busnr)				);
_PROTOTYPE( u8_t pci_attr_r8, (int devind, int port)			);
_PROTOTYPE( u16_t pci_attr_r16, (int devind, int port)			);
_PROTOTYPE( u32_t pci_attr_r32, (int devind, int port)			);
_PROTOTYPE( void pci_attr_w8, (int devind, int port, u8_t value)	);
_PROTOTYPE( void pci_attr_w16, (int devind, int port, u16_t value)	);
_PROTOTYPE( void pci_attr_w32, (int devind, int port, u32_t value)	);
_PROTOTYPE( char *pci_dev_name, (u16_t vid, u16_t did)			);
_PROTOTYPE( char *pci_slot_name, (int devind)				);
_PROTOTYPE( int pci_set_acl, (struct rs_pci *rs_pci)			);
_PROTOTYPE( int pci_del_acl, (endpoint_t proc_ep)			);
_PROTOTYPE( int pci_get_bar, (int devind, int port, u32_t *base,
					u32_t *size, int *ioflag)	);

/* Profiling. */
_PROTOTYPE( int sys_sprof, (int action, int size, int freq,
		endpoint_t endpt, void *ctl_ptr, void *mem_ptr)   );
_PROTOTYPE( int sys_cprof, (int action, int size, endpoint_t endpt,
                                       void *ctl_ptr, void *mem_ptr)   );
_PROTOTYPE( int sys_profbuf, (void *ctl_ptr, void *mem_ptr)            );

/* machine context */
_PROTOTYPE( int sys_getmcontext, (endpoint_t proc, mcontext_t *mcp)	);
_PROTOTYPE( int sys_setmcontext, (endpoint_t proc, mcontext_t *mcp)	);

#endif /* _SYSLIB_H */

