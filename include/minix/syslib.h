/* Prototypes for system library functions. */

#ifndef _SYSLIB_H
#define _SYSLIB_H

#include <sys/types.h>
#include <sys/sigtypes.h>

#include <minix/ipc.h>
#include <minix/u64.h>
#include <minix/devio.h>

#include <minix/safecopies.h>
#include <minix/sef.h>
#include <machine/mcontext.h>

/* Forward declaration */
struct reg86u;
struct rs_pci;
struct rusage;

#define SYSTASK SYSTEM

/*==========================================================================* 
 * Minix system library. 						    *
 *==========================================================================*/ 
int _taskcall(endpoint_t who, int syscallnr, message *msgptr);
int _kernel_call(int syscallnr, message *msgptr);

int sys_abort(int how);
int sys_enable_iop(endpoint_t proc_ep);
int sys_exec(endpoint_t proc_ep, vir_bytes stack_ptr, vir_bytes progname,
	vir_bytes pc, vir_bytes ps_str);
int sys_fork(endpoint_t parent, endpoint_t child, endpoint_t *, 
	u32_t vm, vir_bytes *);
int sys_clear(endpoint_t proc_ep);
int sys_exit(void);
int sys_trace(int req, endpoint_t proc_ep, long addr, long *data_p);

int sys_schedule(endpoint_t proc_ep, int priority, int quantum, int
	cpu);
int sys_schedctl(unsigned flags, endpoint_t proc_ep, int priority, int
	quantum, int cpu);

/* Shorthands for sys_runctl() system call. */
#define sys_stop(proc_ep) sys_runctl(proc_ep, RC_STOP, 0)
#define sys_delay_stop(proc_ep) sys_runctl(proc_ep, RC_STOP, RC_DELAY)
#define sys_resume(proc_ep) sys_runctl(proc_ep, RC_RESUME, 0)
int sys_runctl(endpoint_t proc_ep, int action, int flags);

int sys_update(endpoint_t src_ep, endpoint_t dst_ep);
int sys_statectl(int request);
int sys_privctl(endpoint_t proc_ep, int req, void *p);
int sys_privquery_mem(endpoint_t proc_ep, phys_bytes physstart,
	phys_bytes physlen);
int sys_setgrant(cp_grant_t *grants, int ngrants);

int sys_int86(struct reg86u *reg86p);
int sys_vm_setbuf(phys_bytes base, phys_bytes size, phys_bytes high);
int sys_vm_map(endpoint_t proc_ep, int do_map, phys_bytes base,
	phys_bytes size, phys_bytes offset);
int sys_vmctl(endpoint_t who, int param, u32_t value);
int sys_vmctl_get_pdbr(endpoint_t who, u32_t *pdbr);
int sys_vmctl_get_memreq(endpoint_t *who, vir_bytes *mem, vir_bytes
	*len, int *wrflag, endpoint_t *who_s, vir_bytes *mem_s, endpoint_t *);
int sys_vmctl_enable_paging(void * data);

int sys_readbios(phys_bytes address, void *buf, size_t size);
int sys_settime(int now, clockid_t clk_id, time_t sec, long nsec);
int sys_stime(time_t boottime);
int sys_vmctl_get_mapping(int index, phys_bytes *addr, phys_bytes *len,
	int *flags);
int sys_vmctl_reply_mapping(int index, vir_bytes addr);
int sys_vmctl_set_addrspace(endpoint_t who, phys_bytes ptroot, void
	*ptroot_v);


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
int sys_sdevio(int req, long port, endpoint_t proc_ep, void *buffer, int
	count, vir_bytes offset);
void *alloc_contig(size_t len, int flags, phys_bytes *phys);
int free_contig(void *addr, size_t len);

#define AC_ALIGN4K	0x01
#define AC_LOWER16M	0x02
#define AC_ALIGN64K	0x04
#define AC_LOWER1M	0x08

/* Clock functionality: get system times, (un)schedule an alarm call, or
 * retrieve/set a process-virtual timer.
 */
int sys_times(endpoint_t proc_ep, clock_t *user_time, clock_t *sys_time,
	clock_t *uptime, time_t *boottime);
int sys_setalarm(clock_t exp_time, int abs_time);
int sys_vtimer(endpoint_t proc_nr, int which, clock_t *newval, clock_t
	*oldval);

/* Shorthands for sys_irqctl() system call. */
#define sys_irqdisable(hook_id) \
    sys_irqctl(IRQ_DISABLE, 0, 0, hook_id) 
#define sys_irqenable(hook_id) \
    sys_irqctl(IRQ_ENABLE, 0, 0, hook_id) 
#define sys_irqsetpolicy(irq_vec, policy, hook_id) \
    sys_irqctl(IRQ_SETPOLICY, irq_vec, policy, hook_id)
#define sys_irqrmpolicy(hook_id) \
    sys_irqctl(IRQ_RMPOLICY, 0, 0, hook_id)
int sys_irqctl(int request, int irq_vec, int policy, int *irq_hook_id);

/* Shorthands for sys_vircopy() and sys_physcopy() system calls. */
#define sys_datacopy(p1, v1, p2, v2, len) sys_vircopy(p1, v1, p2, v2, len, 0)
#define sys_datacopy_try(p1, v1, p2, v2, len) sys_vircopy(p1, v1, p2, v2, len, CP_FLAG_TRY)
int sys_vircopy(endpoint_t src_proc, vir_bytes src_v,
	endpoint_t dst_proc, vir_bytes dst_vir, phys_bytes bytes, int flags);

#define sys_abscopy(src_phys, dst_phys, bytes) \
	sys_physcopy(NONE, src_phys, NONE, dst_phys, bytes, 0)
int sys_physcopy(endpoint_t src_proc, vir_bytes src_vir,
	endpoint_t dst_proc, vir_bytes dst_vir, phys_bytes bytes, int flags);


/* Grant-based copy functions. */
int sys_safecopyfrom(endpoint_t source, cp_grant_id_t grant, vir_bytes
	grant_offset, vir_bytes my_address, size_t bytes);
int sys_safecopyto(endpoint_t dest, cp_grant_id_t grant, vir_bytes
	grant_offset, vir_bytes my_address, size_t bytes);
int sys_vsafecopy(struct vscp_vec *copyvec, int elements);

int sys_safememset(endpoint_t source, cp_grant_id_t grant, vir_bytes
	grant_offset, int pattern, size_t bytes);

int sys_memset(endpoint_t who, unsigned long pattern,
	phys_bytes base, phys_bytes bytes);

int sys_vumap(endpoint_t endpt, struct vumap_vir *vvec,
	int vcount, size_t offset, int access, struct vumap_phys *pvec,
	int *pcount);
int sys_umap(endpoint_t proc_ep, int seg, vir_bytes vir_addr, vir_bytes
	bytes, phys_bytes *phys_addr);
int sys_umap_data_fb(endpoint_t proc_ep, vir_bytes vir_addr, vir_bytes
	bytes, phys_bytes *phys_addr);
int sys_umap_remote(endpoint_t proc_ep, endpoint_t grantee, int seg,
	vir_bytes vir_addr, vir_bytes bytes, phys_bytes *phys_addr);

/* Shorthands for sys_diagctl() system call. */
#define sys_diagctl_diag(buf,len) \
	sys_diagctl(DIAGCTL_CODE_DIAG, buf, len)
#define sys_diagctl_stacktrace(ep) \
	sys_diagctl(DIAGCTL_CODE_STACKTRACE, NULL, ep)
#define sys_diagctl_register()	\
	sys_diagctl(DIAGCTL_CODE_REGISTER, NULL, 0)
#define sys_diagctl_unregister() \
	sys_diagctl(DIAGCTL_CODE_UNREGISTER, NULL, 0)
int sys_diagctl(int ctl, char *arg1, int arg2);

/* Shorthands for sys_getinfo() system call. */
#define sys_getkinfo(dst)	sys_getinfo(GET_KINFO, dst, 0,0,0)
#define sys_getloadinfo(dst)	sys_getinfo(GET_LOADINFO, dst, 0,0,0)
#define sys_getmachine(dst)	sys_getinfo(GET_MACHINE, dst, 0,0,0)
#define sys_getcpuinfo(dst)     sys_getinfo(GET_CPUINFO, dst, 0,0,0)
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
#define sys_getpriv(dst, nr)	sys_getinfo(GET_PRIV, dst, 0,0, nr)
#define sys_getidletsc(dst)	sys_getinfo(GET_IDLETSC, dst, 0,0,0)
#define sys_getregs(dst,nr)	sys_getinfo(GET_REGS, dst, 0,0, nr)
#define sys_getrusage(dst, nr)  sys_getinfo(GET_RUSAGE, dst, 0,0, nr)
int sys_getinfo(int request, void *val_ptr, int val_len, void *val_ptr2,
	int val_len2);
int sys_whoami(endpoint_t *ep, char *name, int namelen, int
	*priv_flags);

/* Signal control. */
int sys_kill(endpoint_t proc_ep, int sig);
int sys_sigsend(endpoint_t proc_ep, struct sigmsg *sig_ctxt);
int sys_sigreturn(endpoint_t proc_ep, struct sigmsg *sig_ctxt);
int sys_getksig(endpoint_t *proc_ep, sigset_t *k_sig_map);
int sys_endksig(endpoint_t proc_ep);

/* NOTE: two different approaches were used to distinguish the device I/O
 * types 'byte', 'word', 'long': the latter uses #define and results in a
 * smaller implementation, but looses the static type checking.
 */
int sys_voutb(pvb_pair_t *pvb_pairs, int nr_ports);
int sys_voutw(pvw_pair_t *pvw_pairs, int nr_ports);
int sys_voutl(pvl_pair_t *pvl_pairs, int nr_ports);
int sys_vinb(pvb_pair_t *pvb_pairs, int nr_ports);
int sys_vinw(pvw_pair_t *pvw_pairs, int nr_ports);
int sys_vinl(pvl_pair_t *pvl_pairs, int nr_ports);

/* Shorthands for sys_out() system call. */
#define sys_outb(p,v)	sys_out((p), (u32_t) (v), _DIO_BYTE)
#define sys_outw(p,v)	sys_out((p), (u32_t) (v), _DIO_WORD)
#define sys_outl(p,v)	sys_out((p), (u32_t) (v), _DIO_LONG)
int sys_out(int port, u32_t value, int type);

/* Shorthands for sys_in() system call. */
#define sys_inb(p,v)	sys_in((p), (v), _DIO_BYTE)
#define sys_inw(p,v)	sys_in((p), (v), _DIO_WORD)
#define sys_inl(p,v)	sys_in((p), (v), _DIO_LONG)
int sys_in(int port, u32_t *value, int type);

/* arm pinmux */
int sys_padconf(u32_t padconf, u32_t mask, u32_t value);

/* pci.c */
void pci_init(void);
int pci_first_dev(int *devindp, u16_t *vidp, u16_t *didp);
int pci_next_dev(int *devindp, u16_t *vidp, u16_t *didp);
int pci_find_dev(u8_t bus, u8_t dev, u8_t func, int *devindp);
void pci_reserve(int devind);
int pci_reserve_ok(int devind);
void pci_ids(int devind, u16_t *vidp, u16_t *didp);
void pci_rescan_bus(u8_t busnr);
u8_t pci_attr_r8(int devind, int port);
u16_t pci_attr_r16(int devind, int port);
u32_t pci_attr_r32(int devind, int port);
void pci_attr_w8(int devind, int port, u8_t value);
void pci_attr_w16(int devind, int port, u16_t value);
void pci_attr_w32(int devind, int port, u32_t value);
char *pci_dev_name(u16_t vid, u16_t did);
char *pci_slot_name(int devind);
int pci_set_acl(struct rs_pci *rs_pci);
int pci_del_acl(endpoint_t proc_ep);
int pci_get_bar(int devind, int port, u32_t *base, u32_t *size, int
	*ioflag);

/* Profiling. */
int sys_sprof(int action, int size, int freq, int type, endpoint_t
	endpt, void *ctl_ptr, void *mem_ptr);
int sys_cprof(int action, int size, endpoint_t endpt, void *ctl_ptr,
	void *mem_ptr);
int sys_profbuf(void *ctl_ptr, void *mem_ptr);

/* machine context */
int sys_getmcontext(endpoint_t proc, vir_bytes mcp);
int sys_setmcontext(endpoint_t proc, vir_bytes mcp);

/* input */
int tty_input_inject(int type, int code, int val);

/* Miscellaneous calls from servers and drivers. */
pid_t srv_fork(uid_t reuid, gid_t regid);
int srv_kill(pid_t pid, int sig);
int getprocnr(pid_t pid, endpoint_t *proc_ep);
int mapdriver(char *label, devmajor_t major);
pid_t getnpid(endpoint_t proc_ep);
uid_t getnuid(endpoint_t proc_ep);
gid_t getngid(endpoint_t proc_ep);
int checkperms(endpoint_t endpt, char *path, size_t size);
int copyfd(endpoint_t endpt, int fd, int what);
#define COPYFD_FROM	0	/* copy file descriptor from remote process */
#define COPYFD_TO	1	/* copy file descriptor to remote process */
#define COPYFD_CLOSE	2	/* close file descriptor in remote process */

#endif /* _SYSLIB_H */

