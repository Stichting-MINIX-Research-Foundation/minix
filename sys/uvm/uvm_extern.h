/*	$NetBSD: uvm_extern.h,v 1.184 2012/09/01 00:26:37 matt Exp $	*/

/*
 * Copyright (c) 1997 Charles D. Cranor and Washington University.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * from: Id: uvm_extern.h,v 1.1.2.21 1998/02/07 01:16:53 chs Exp
 */

/*-
 * Copyright (c) 1991, 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)vm_extern.h	8.5 (Berkeley) 5/3/95
 */

#ifndef _UVM_UVM_EXTERN_H_
#define _UVM_UVM_EXTERN_H_

/*
 * uvm_extern.h: this file defines the external interface to the VM system.
 *
 * this should be the only file included by non-VM parts of the kernel
 * which need access to VM services.   if you want to know the interface
 * to the MI VM layer without knowing the details, this is the file to
 * learn.
 *
 * NOTE: vm system calls are prototyped in syscallargs.h
 */

/*
 * defines
 */

/*
 * the following defines are for uvm_map and functions which call it.
 */

/* protections bits */
#define UVM_PROT_MASK	0x07	/* protection mask */
#define UVM_PROT_NONE	0x00	/* protection none */
#define UVM_PROT_ALL	0x07	/* everything */
#define UVM_PROT_READ	0x01	/* read */
#define UVM_PROT_WRITE  0x02	/* write */
#define UVM_PROT_EXEC	0x04	/* exec */

/* protection short codes */
#define UVM_PROT_R	0x01	/* read */
#define UVM_PROT_W	0x02	/* write */
#define UVM_PROT_RW	0x03    /* read-write */
#define UVM_PROT_X	0x04	/* exec */
#define UVM_PROT_RX	0x05	/* read-exec */
#define UVM_PROT_WX	0x06	/* write-exec */
#define UVM_PROT_RWX	0x07	/* read-write-exec */

/* 0x08: not used */

/* inherit codes */
#define UVM_INH_MASK	0x30	/* inherit mask */
#define UVM_INH_SHARE	0x00	/* "share" */
#define UVM_INH_COPY	0x10	/* "copy" */
#define UVM_INH_NONE	0x20	/* "none" */
#define UVM_INH_DONATE	0x30	/* "donate" << not used */

/* 0x40, 0x80: not used */

/* bits 0x700: max protection, 0x800: not used */

/* bits 0x7000: advice, 0x8000: not used */
/* advice: matches MADV_* from sys/mman.h and POSIX_FADV_* from sys/fcntl.h */
#define UVM_ADV_NORMAL	0x0	/* 'normal' */
#define UVM_ADV_RANDOM	0x1	/* 'random' */
#define UVM_ADV_SEQUENTIAL 0x2	/* 'sequential' */
#define UVM_ADV_WILLNEED 0x3	/* pages will be needed */
#define UVM_ADV_DONTNEED 0x4	/* pages won't be needed */
#define UVM_ADV_NOREUSE	0x5	/* pages will be used only once */
#define UVM_ADV_MASK	0x7	/* mask */

/* bits 0xffff0000: mapping flags */
#define UVM_FLAG_FIXED   0x010000 /* find space */
#define UVM_FLAG_OVERLAY 0x020000 /* establish overlay */
#define UVM_FLAG_NOMERGE 0x040000 /* don't merge map entries */
#define UVM_FLAG_COPYONW 0x080000 /* set copy_on_write flag */
#define UVM_FLAG_AMAPPAD 0x100000 /* for bss: pad amap to reduce allocations */
#define UVM_FLAG_TRYLOCK 0x200000 /* fail if we can not lock map */
#define UVM_FLAG_NOWAIT  0x400000 /* not allowed to sleep */
#define UVM_FLAG_WAITVA  0x800000 /* wait for va */
#define UVM_FLAG_VAONLY  0x2000000 /* unmap: no pages are mapped */
#define UVM_FLAG_COLORMATCH 0x4000000 /* match color given in off */

/* macros to extract info */
#define UVM_PROTECTION(X)	((X) & UVM_PROT_MASK)
#define UVM_INHERIT(X)		(((X) & UVM_INH_MASK) >> 4)
#define UVM_MAXPROTECTION(X)	(((X) >> 8) & UVM_PROT_MASK)
#define UVM_ADVICE(X)		(((X) >> 12) & UVM_ADV_MASK)

#define UVM_MAPFLAG(PROT,MAXPROT,INH,ADVICE,FLAGS) \
	(((MAXPROT) << 8)|(PROT)|(INH)|((ADVICE) << 12)|(FLAGS))

/* magic offset value: offset not known(obj) or don't care(!obj) */
#define UVM_UNKNOWN_OFFSET ((voff_t) -1)

/*
 * the following defines are for uvm_km_alloc/free's flags
 */
#define UVM_KMF_WIRED	0x1			/* allocation type: wired */
#define UVM_KMF_PAGEABLE 0x2			/* allocation type: pageable */
#define UVM_KMF_VAONLY	0x4			/* allocation type: VA only */
#define	UVM_KMF_TYPEMASK (UVM_KMF_VAONLY | UVM_KMF_PAGEABLE | UVM_KMF_WIRED)
#define UVM_KMF_CANFAIL	0x8			/* caller handles failure */
#define UVM_KMF_ZERO	0x10			/* want zero filled memory */
#define UVM_KMF_EXEC	0x20			/* need executable mapping */
#define UVM_KMF_TRYLOCK	UVM_FLAG_TRYLOCK	/* try locking only */
#define UVM_KMF_NOWAIT	UVM_FLAG_NOWAIT		/* not allowed to sleep */
#define UVM_KMF_WAITVA	UVM_FLAG_WAITVA		/* sleep for va */
#define UVM_KMF_COLORMATCH UVM_FLAG_COLORMATCH	/* start at color in align */

/*
 * the following defines the strategies for uvm_pagealloc_strat()
 */
#define	UVM_PGA_STRAT_NORMAL	0	/* priority (low id to high) walk */
#define	UVM_PGA_STRAT_ONLY	1	/* only specified free list */
#define	UVM_PGA_STRAT_FALLBACK	2	/* ONLY falls back on NORMAL */

/*
 * flags for uvm_pagealloc_strat()
 */
#define UVM_PGA_USERESERVE	0x0001	/* ok to use reserve pages */
#define	UVM_PGA_ZERO		0x0002	/* returned page must be zero'd */

/*
 * flags for ubc_alloc()
 */
#define UBC_READ	0x001
#define UBC_WRITE	0x002
#define UBC_FAULTBUSY	0x004

/*
 * flags for ubc_release()
 */
#define UBC_UNMAP	0x010

/*
 * flags for ubc_uiomve()
 */
#define	UBC_PARTIALOK	0x100

/*
 * flags for uvn_findpages().
 */
#define UFP_ALL		0x00
#define UFP_NOWAIT	0x01
#define UFP_NOALLOC	0x02
#define UFP_NOCACHE	0x04
#define UFP_NORDONLY	0x08
#define UFP_DIRTYONLY	0x10
#define UFP_BACKWARD	0x20

/*
 * lockflags that control the locking behavior of various functions.
 */
#define	UVM_LK_ENTER	0x00000001	/* map locked on entry */
#define	UVM_LK_EXIT	0x00000002	/* leave map locked on exit */

/*
 * Default number of pages to allocate on the stack
 */
#define	UBC_MAX_PAGES	8

/*
 * Value representing inactive emap.
 */
#define	UVM_EMAP_INACTIVE	(0)

/*
 * structures
 */

struct buf;
struct core;
struct loadavg;
struct mount;
struct pglist;
struct proc;
struct uio;
struct uvm_object;
struct vm_anon;
struct vmspace;
struct pmap;
struct vnode;
struct simplelock;
struct vm_map_entry;
struct vm_map;
struct vm_page;
struct vmtotal;

/*
 * uvm_pctparam: parameter to be shown as percentage to user.
 */

#define	UVM_PCTPARAM_SHIFT	8
#define	UVM_PCTPARAM_SCALE	(1 << UVM_PCTPARAM_SHIFT)
#define	UVM_PCTPARAM_APPLY(pct, x) \
	(((x) * (pct)->pct_scaled) >> UVM_PCTPARAM_SHIFT)
struct uvm_pctparam {
	int pct_pct;	/* percent [0, 100] */ /* should be the first member */
	int pct_scaled;
	int (*pct_check)(struct uvm_pctparam *, int);
};

/*
 * uvmexp: global data structures that are exported to parts of the kernel
 * other than the vm system.
 */

struct uvmexp {
	/* vm_page constants */
	int pagesize;   /* size of a page (PAGE_SIZE): must be power of 2 */
	int pagemask;   /* page mask */
	int pageshift;  /* page shift */

	/* vm_page counters */
	int npages;     /* number of pages we manage */
	int free;       /* number of free pages */
	int paging;	/* number of pages in the process of being paged out */
	int wired;      /* number of wired pages */

	/*
	 * Adding anything before this line will break binary compatibility
	 * with top(1) on NetBSD 1.5.
	 */

	int ncolors;	/* number of page color buckets: must be p-o-2 */
	int colormask;	/* color bucket mask */

	int zeropages;		/* number of zero'd pages */
	int reserve_pagedaemon; /* number of pages reserved for pagedaemon */
	int reserve_kernel;	/* number of pages reserved for kernel */
	unsigned anonpages;	/* number of pages used by anon mappings */
	unsigned filepages;	/* number of pages used by cached file data */
	unsigned execpages;	/* number of pages used by cached exec data */

	/* pageout params */
	int freemin;    /* min number of free pages */
	int freetarg;   /* target number of free pages */
	int wiredmax;   /* max number of wired pages */

	/* swap */
	int nswapdev;	/* number of configured swap devices in system */
	int swpages;	/* number of PAGE_SIZE'ed swap pages */
	int swpgavail;	/* number of swap pages currently available */
	int swpginuse;	/* number of swap pages in use */
	int swpgonly;	/* number of swap pages in use, not also in RAM */
	int nswget;	/* number of times fault calls uvm_swap_get() */

	/* stat counters.  XXX: should be 64-bit counters */
	int _unused_faults;	/* page fault count */
	int _unused_traps;	/* trap count */
	int _unused_intrs;	/* interrupt count */
	int _unused_swtch;	/* context switch count */
	int _unused_softs;	/* software interrupt count */
	int _unused_syscalls;	/* system calls */
	int pageins;		/* pagein operation count */
				/* pageouts are in pdpageouts below */
	int _unused1;
	int _unused2;
	int pgswapin;		/* pages swapped in */
	int pgswapout;		/* pages swapped out */
	int forks;  		/* forks */
	int forks_ppwait;	/* forks where parent waits */
	int forks_sharevm;	/* forks where vmspace is shared */
	int pga_zerohit;	/* pagealloc where zero wanted and zero
				   was available */
	int pga_zeromiss;	/* pagealloc where zero wanted and zero
				   not available */
	int zeroaborts;		/* number of times page zeroing was
				   aborted */
	int colorhit;		/* pagealloc where we got optimal color */
	int colormiss;		/* pagealloc where we didn't */
	int cpuhit;		/* pagealloc where we allocated locally */
	int cpumiss;		/* pagealloc where we didn't */

	/* fault subcounters.  XXX: should be 64-bit counters */
	int fltnoram;	/* number of times fault was out of ram */
	int fltnoanon;	/* number of times fault was out of anons */
	int fltpgwait;	/* number of times fault had to wait on a page */
	int fltpgrele;	/* number of times fault found a released page */
	int fltrelck;	/* number of times fault relock called */
	int fltrelckok;	/* number of times fault relock is a success */
	int fltanget;	/* number of times fault gets anon page */
	int fltanretry;	/* number of times fault retrys an anon get */
	int fltamcopy;	/* number of times fault clears "needs copy" */
	int fltnamap;	/* number of times fault maps a neighbor anon page */
	int fltnomap;	/* number of times fault maps a neighbor obj page */
	int fltlget;	/* number of times fault does a locked pgo_get */
	int fltget;	/* number of times fault does an unlocked get */
	int flt_anon;	/* number of times fault anon (case 1a) */
	int flt_acow;	/* number of times fault anon cow (case 1b) */
	int flt_obj;	/* number of times fault is on object page (2a) */
	int flt_prcopy;	/* number of times fault promotes with copy (2b) */
	int flt_przero;	/* number of times fault promotes with zerofill (2b) */

	/* daemon counters.  XXX: should be 64-bit counters */
	int pdwoke;	/* number of times daemon woke up */
	int pdrevs;	/* number of times daemon rev'd clock hand */
	int _unused3;
	int pdfreed;	/* number of pages daemon freed since boot */
	int pdscans;	/* number of pages daemon scanned since boot */
	int pdanscan;	/* number of anonymous pages scanned by daemon */
	int pdobscan;	/* number of object pages scanned by daemon */
	int pdreact;	/* number of pages daemon reactivated since boot */
	int pdbusy;	/* number of times daemon found a busy page */
	int pdpageouts;	/* number of times daemon started a pageout */
	int pdpending;	/* number of times daemon got a pending pagout */
	int pddeact;	/* number of pages daemon deactivates */
	int pdreanon;	/* anon pages reactivated due to thresholds */
	int pdrefile;	/* file pages reactivated due to thresholds */
	int pdreexec;	/* executable pages reactivated due to thresholds */
};

/*
 * The following structure is 64-bit alignment safe.  New elements
 * should only be added to the end of this structure so binary
 * compatibility can be preserved.
 */
struct uvmexp_sysctl {
	int64_t	pagesize;
	int64_t	pagemask;
	int64_t	pageshift;
	int64_t	npages;
	int64_t	free;
	int64_t	active;
	int64_t	inactive;
	int64_t	paging;
	int64_t	wired;
	int64_t	zeropages;
	int64_t	reserve_pagedaemon;
	int64_t	reserve_kernel;
	int64_t	freemin;
	int64_t	freetarg;
	int64_t	inactarg;		/* unused */
	int64_t	wiredmax;
	int64_t	nswapdev;
	int64_t	swpages;
	int64_t	swpginuse;
	int64_t	swpgonly;
	int64_t	nswget;
	int64_t	unused1;		/* unused; was nanon */
	int64_t cpuhit;
	int64_t cpumiss;
	int64_t	faults;
	int64_t	traps;
	int64_t	intrs;
	int64_t	swtch;
	int64_t	softs;
	int64_t	syscalls;
	int64_t	pageins;
	int64_t	swapins;		/* unused */
	int64_t	swapouts;		/* unused */
	int64_t	pgswapin;
	int64_t	pgswapout;
	int64_t	forks;
	int64_t	forks_ppwait;
	int64_t	forks_sharevm;
	int64_t	pga_zerohit;
	int64_t	pga_zeromiss;
	int64_t	zeroaborts;
	int64_t	fltnoram;
	int64_t	fltnoanon;
	int64_t	fltpgwait;
	int64_t	fltpgrele;
	int64_t	fltrelck;
	int64_t	fltrelckok;
	int64_t	fltanget;
	int64_t	fltanretry;
	int64_t	fltamcopy;
	int64_t	fltnamap;
	int64_t	fltnomap;
	int64_t	fltlget;
	int64_t	fltget;
	int64_t	flt_anon;
	int64_t	flt_acow;
	int64_t	flt_obj;
	int64_t	flt_prcopy;
	int64_t	flt_przero;
	int64_t	pdwoke;
	int64_t	pdrevs;
	int64_t	unused4;
	int64_t	pdfreed;
	int64_t	pdscans;
	int64_t	pdanscan;
	int64_t	pdobscan;
	int64_t	pdreact;
	int64_t	pdbusy;
	int64_t	pdpageouts;
	int64_t	pdpending;
	int64_t	pddeact;
	int64_t	anonpages;
	int64_t	filepages;
	int64_t	execpages;
	int64_t colorhit;
	int64_t colormiss;
	int64_t ncolors;
};

#ifdef _KERNEL
/* we need this before including uvm_page.h on some platforms */
extern struct uvmexp uvmexp;
/* MD code needs this without including <uvm/uvm.h> */
extern bool vm_page_zero_enable;
#endif

/*
 * Finally, bring in standard UVM headers.
 */
#include <sys/vmmeter.h>
#include <sys/queue.h>
#include <sys/lock.h>
#ifdef _KERNEL
#include <sys/vmem.h>
#endif
#include <uvm/uvm_param.h>
#include <uvm/uvm_prot.h>
#include <uvm/uvm_pmap.h>
#include <uvm/uvm_map.h>
#include <uvm/uvm_pager.h>

/*
 * helpers for calling ubc_release()
 */
#ifdef PMAP_CACHE_VIVT
#define UBC_WANT_UNMAP(vp) (((vp)->v_iflag & VI_TEXT) != 0)
#else
#define UBC_WANT_UNMAP(vp) false
#endif
#define UBC_UNMAP_FLAG(vp) (UBC_WANT_UNMAP(vp) ? UBC_UNMAP : 0)

/*
 * Shareable process virtual address space.
 * May eventually be merged with vm_map.
 * Several fields are temporary (text, data stuff).
 */
struct vmspace {
	struct	vm_map vm_map;	/* VM address map */
	int	vm_refcnt;	/* number of references *
				 * note: protected by vm_map.ref_lock */
	void *	vm_shm;		/* SYS5 shared memory private data XXX */
/* we copy from vm_startcopy to the end of the structure on fork */
#define vm_startcopy vm_rssize
	segsz_t vm_rssize;	/* current resident set size in pages */
	segsz_t vm_swrss;	/* resident set size before last swap */
	segsz_t vm_tsize;	/* text size (pages) XXX */
	segsz_t vm_dsize;	/* data size (pages) XXX */
	segsz_t vm_ssize;	/* stack size (pages) */
	segsz_t vm_issize;	/* initial unmapped stack size (pages) */
	void *	vm_taddr;	/* user virtual address of text XXX */
	void *	vm_daddr;	/* user virtual address of data XXX */
	void *vm_maxsaddr;	/* user VA at max stack growth */
	void *vm_minsaddr;	/* user VA at top of stack */
	size_t vm_aslr_delta_mmap;	/* mmap() random delta for ASLR */
};
#define	VMSPACE_IS_KERNEL_P(vm)	VM_MAP_IS_KERNEL(&(vm)->vm_map)

#ifdef _KERNEL

/*
 * used to keep state while iterating over the map for a core dump.
 */
struct uvm_coredump_state {
	void *cookie;		/* opaque for the caller */
	vaddr_t start;		/* start of region */
	vaddr_t realend;	/* real end of region */
	vaddr_t end;		/* virtual end of region */
	vm_prot_t prot;		/* protection of region */
	int flags;		/* flags; see below */
};

#define	UVM_COREDUMP_STACK	0x01	/* region is user stack */

/*
 * the various kernel maps, owned by MD code
 */
extern struct vm_map *kernel_map;
extern struct vm_map *phys_map;

/*
 * macros
 */

#define vm_resident_count(vm) (pmap_resident_count((vm)->vm_map.pmap))


/* vm_machdep.c */
int		vmapbuf(struct buf *, vsize_t);
void		vunmapbuf(struct buf *, vsize_t);

/* uvm_aobj.c */
struct uvm_object	*uao_create(vsize_t, int);
void			uao_detach(struct uvm_object *);
void			uao_reference(struct uvm_object *);

/* uvm_bio.c */
void			ubc_init(void);
void *			ubc_alloc(struct uvm_object *, voff_t, vsize_t *, int,
			    int);
void			ubc_release(void *, int);
int			ubc_uiomove(struct uvm_object *, struct uio *, vsize_t,
			    int, int);
void			ubc_zerorange(struct uvm_object *, off_t, size_t, int);
void			ubc_purge(struct uvm_object *);

/* uvm_emap.c */
void			uvm_emap_sysinit(void);
#ifdef __HAVE_PMAP_EMAP
void			uvm_emap_switch(lwp_t *);
#else
#define			uvm_emap_switch(l)
#endif

u_int			uvm_emap_gen_return(void);
void			uvm_emap_update(u_int);

vaddr_t			uvm_emap_alloc(vsize_t, bool);
void			uvm_emap_free(vaddr_t, size_t);

void			uvm_emap_enter(vaddr_t, struct vm_page **, u_int);
void			uvm_emap_remove(vaddr_t, vsize_t);

#ifdef __HAVE_PMAP_EMAP
void			uvm_emap_consume(u_int);
u_int			uvm_emap_produce(void);
#else
#define			uvm_emap_consume(x)
#define			uvm_emap_produce()	UVM_EMAP_INACTIVE
#endif

/* uvm_fault.c */
#define uvm_fault(m, a, p) uvm_fault_internal(m, a, p, 0)
int		uvm_fault_internal(struct vm_map *, vaddr_t, vm_prot_t, int);
			/* handle a page fault */

/* uvm_glue.c */
#if defined(KGDB)
void			uvm_chgkprot(void *, size_t, int);
#endif
void			uvm_proc_fork(struct proc *, struct proc *, bool);
void			uvm_lwp_fork(struct lwp *, struct lwp *,
			    void *, size_t, void (*)(void *), void *);
int			uvm_coredump_walkmap(struct proc *,
			    void *,
			    int (*)(struct proc *, void *,
				    struct uvm_coredump_state *), void *);
void			uvm_proc_exit(struct proc *);
void			uvm_lwp_exit(struct lwp *);
void			uvm_init_limits(struct proc *);
bool			uvm_kernacc(void *, size_t, vm_prot_t);
__dead void		uvm_scheduler(void);
vaddr_t			uvm_uarea_alloc(void);
void			uvm_uarea_free(vaddr_t);
vaddr_t			uvm_uarea_system_alloc(struct cpu_info *);
void			uvm_uarea_system_free(vaddr_t);
vaddr_t			uvm_lwp_getuarea(lwp_t *);
void			uvm_lwp_setuarea(lwp_t *, vaddr_t);
int			uvm_vslock(struct vmspace *, void *, size_t, vm_prot_t);
void			uvm_vsunlock(struct vmspace *, void *, size_t);
void			uvm_cpu_attach(struct cpu_info *);


/* uvm_init.c */
void			uvm_init(void);

/* uvm_io.c */
int			uvm_io(struct vm_map *, struct uio *);

/* uvm_km.c */
vaddr_t			uvm_km_alloc(struct vm_map *, vsize_t, vsize_t,
			    uvm_flag_t);
void			uvm_km_free(struct vm_map *, vaddr_t, vsize_t,
			    uvm_flag_t);

struct vm_map		*uvm_km_suballoc(struct vm_map *, vaddr_t *,
			    vaddr_t *, vsize_t, int, bool,
			    struct vm_map *);
#ifdef _KERNEL
int			uvm_km_kmem_alloc(vmem_t *, vmem_size_t, vm_flag_t,
			    vmem_addr_t *);
void			uvm_km_kmem_free(vmem_t *, vmem_addr_t, vmem_size_t);
bool			uvm_km_va_starved_p(void);
#endif

/* uvm_map.c */
int			uvm_map(struct vm_map *, vaddr_t *, vsize_t,
			    struct uvm_object *, voff_t, vsize_t,
			    uvm_flag_t);
int			uvm_map_pageable(struct vm_map *, vaddr_t,
			    vaddr_t, bool, int);
int			uvm_map_pageable_all(struct vm_map *, int, vsize_t);
bool			uvm_map_checkprot(struct vm_map *, vaddr_t,
			    vaddr_t, vm_prot_t);
int			uvm_map_protect(struct vm_map *, vaddr_t,
			    vaddr_t, vm_prot_t, bool);
struct vmspace		*uvmspace_alloc(vaddr_t, vaddr_t);
void			uvmspace_init(struct vmspace *, struct pmap *,
			    vaddr_t, vaddr_t);
void			uvmspace_exec(struct lwp *, vaddr_t, vaddr_t);
void			uvmspace_spawn(struct lwp *, vaddr_t, vaddr_t);
struct vmspace		*uvmspace_fork(struct vmspace *);
void			uvmspace_addref(struct vmspace *);
void			uvmspace_free(struct vmspace *);
void			uvmspace_share(struct proc *, struct proc *);
void			uvmspace_unshare(struct lwp *);

void			uvm_whatis(uintptr_t, void (*)(const char *, ...));

/* uvm_meter.c */
int			uvm_sysctl(int *, u_int, void *, size_t *,
			    void *, size_t, struct proc *);
int			uvm_pctparam_check(struct uvm_pctparam *, int);
void			uvm_pctparam_set(struct uvm_pctparam *, int);
int			uvm_pctparam_get(struct uvm_pctparam *);
void			uvm_pctparam_init(struct uvm_pctparam *, int,
			    int (*)(struct uvm_pctparam *, int));
int			uvm_pctparam_createsysctlnode(struct uvm_pctparam *,
			    const char *, const char *);

/* uvm_mmap.c */
int			uvm_mmap(struct vm_map *, vaddr_t *, vsize_t,
			    vm_prot_t, vm_prot_t, int,
			    void *, voff_t, vsize_t);
vaddr_t			uvm_default_mapaddr(struct proc *, vaddr_t, vsize_t);

/* uvm_mremap.c */
int			uvm_mremap(struct vm_map *, vaddr_t, vsize_t,
			    struct vm_map *, vaddr_t *, vsize_t,
			    struct proc *, int);

/* uvm_object.c */
void			uvm_obj_init(struct uvm_object *,
			    const struct uvm_pagerops *, bool, u_int);
void			uvm_obj_setlock(struct uvm_object *, kmutex_t *);
void			uvm_obj_destroy(struct uvm_object *, bool);
int			uvm_obj_wirepages(struct uvm_object *, off_t, off_t,
			    struct pglist *);
void			uvm_obj_unwirepages(struct uvm_object *, off_t, off_t);

/* uvm_page.c */
struct vm_page		*uvm_pagealloc_strat(struct uvm_object *,
			    voff_t, struct vm_anon *, int, int, int);
#define	uvm_pagealloc(obj, off, anon, flags) \
	    uvm_pagealloc_strat((obj), (off), (anon), (flags), \
				UVM_PGA_STRAT_NORMAL, 0)
void			uvm_pagereplace(struct vm_page *,
			    struct vm_page *);
void			uvm_pagerealloc(struct vm_page *,
			    struct uvm_object *, voff_t);
/* Actually, uvm_page_physload takes PF#s which need their own type */
void			uvm_page_physload(paddr_t, paddr_t, paddr_t,
			    paddr_t, int);
void			uvm_setpagesize(void);

/* uvm_pager.c */
void			uvm_aio_biodone(struct buf *);
void			uvm_aio_aiodone(struct buf *);
void			uvm_aio_aiodone_pages(struct vm_page **, int, bool,
			    int);

/* uvm_pdaemon.c */
void			uvm_pageout(void *);
struct work;
void			uvm_aiodone_worker(struct work *, void *);
void			uvm_pageout_start(int);
void			uvm_pageout_done(int);
void			uvm_estimatepageable(int *, int *);

/* uvm_pglist.c */
int			uvm_pglistalloc(psize_t, paddr_t, paddr_t,
			    paddr_t, paddr_t, struct pglist *, int, int);
void			uvm_pglistfree(struct pglist *);

/* uvm_swap.c */
void			uvm_swap_init(void);

/* uvm_unix.c */
int			uvm_grow(struct proc *, vaddr_t);

/* uvm_user.c */
void			uvm_deallocate(struct vm_map *, vaddr_t, vsize_t);

/* uvm_vnode.c */
void			uvm_vnp_setsize(struct vnode *, voff_t);
void			uvm_vnp_setwritesize(struct vnode *, voff_t);
int			uvn_findpages(struct uvm_object *, voff_t,
			    int *, struct vm_page **, int);
bool			uvn_text_p(struct uvm_object *);
bool			uvn_clean_p(struct uvm_object *);
bool			uvn_needs_writefault_p(struct uvm_object *);

/* kern_malloc.c */
void			kmeminit_nkmempages(void);
void			kmeminit(void);
extern int		nkmempages;

#endif /* _KERNEL */

#endif /* _UVM_UVM_EXTERN_H_ */
