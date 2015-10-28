/*	$NetBSD: kvm_private.h,v 1.20 2011/09/12 21:11:32 christos Exp $	*/

/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software developed by the Computer Systems
 * Engineering group at Lawrence Berkeley Laboratory under DARPA contract
 * BG 91-66 and contributed to Berkeley.
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
 *	@(#)kvm_private.h	8.1 (Berkeley) 6/4/93
 */

struct __kvm {
	/*
	 * a string to be prepended to error messages
	 * provided for compatibility with sun's interface
	 * if this value is null, errors are saved in errbuf[]
	 */
	const char *program;
	char	*errp;		/* XXX this can probably go away */
	char	errbuf[_POSIX2_LINE_MAX];
	int	pmfd;		/* physical memory file (or crash dump) */
	int	vmfd;		/* virtual memory file (-1 if crash dump) */
	int	swfd;		/* swap file (e.g., /dev/drum) */
	int	nlfd;		/* namelist file (e.g., /vmunix) */
	char	alive;		/* live kernel? */
	struct kinfo_proc *procbase;
	struct kinfo_proc2 *procbase2;
	struct kinfo_lwp *lwpbase;
	size_t  procbase_len;
	size_t  procbase2_len;
	size_t  lwpbase_len;
	u_long	usrstack;		/* address of end of user stack */
	u_long	min_uva, max_uva;	/* min/max user virtual address */
	int	nbpg;		/* page size */
	char	*swapspc;	/* (dynamic) storage for swapped pages */
	char	*argspc, *argbuf; /* (dynamic) storage for argv strings */
	size_t	argspc_len;	/* length of the above */
	char	**argv;		/* (dynamic) storage for argv pointers */
	int	argc;		/* length of above (not actual # present) */

	/*
	 * Header structures for kernel dumps. Only gets filled in for
	 * dead kernels.
	 */
	struct kcore_hdr	*kcore_hdr;
	size_t	cpu_dsize;
	void	*cpu_data;
	off_t	dump_off;	/* Where the actual dump starts	*/

	/*
	 * Kernel virtual address translation state.  This only gets filled
	 * in for dead kernels; otherwise, the running kernel (i.e. kmem)
	 * will do the translations for us.  It could be big, so we
	 * only allocate it if necessary.
	 */
	struct vmstate *vmst; /* XXX: should become obsoleted */
	/*
	 * These kernel variables are used for looking up user addresses,
	 * and are cached for efficiency.
	 */
	struct pglist *vm_page_buckets;
	int vm_page_hash_mask;
	/* Buffer for raw disk I/O. */
	size_t fdalign;
	uint8_t *iobuf;
	size_t iobufsz;
	char kernelname[MAXPATHLEN];
};

/* Levels of aliveness */
#define	KVM_ALIVE_DEAD		0	/* dead, working from core file */
#define	KVM_ALIVE_FILES		1	/* alive, working from open kmem/drum */
#define	KVM_ALIVE_SYSCTL	2	/* alive, sysctl-type calls only */

#define	ISALIVE(kd)	((kd)->alive != KVM_ALIVE_DEAD)
#define	ISKMEM(kd)	((kd)->alive == KVM_ALIVE_FILES)
#define	ISSYSCTL(kd)	((kd)->alive == KVM_ALIVE_SYSCTL || ISKMEM(kd))

/*
 * Functions used internally by kvm, but across kvm modules.
 */
void	 _kvm_err(kvm_t *, const char *, const char *, ...)
	    __attribute__((__format__(__printf__, 3, 4)));
int	 _kvm_dump_mkheader(kvm_t *, kvm_t *);
void	 _kvm_freeprocs(kvm_t *);
void	 _kvm_freevtop(kvm_t *);
int	 _kvm_mdopen(kvm_t *);
int	 _kvm_initvtop(kvm_t *);
int	 _kvm_kvatop(kvm_t *, vaddr_t, paddr_t *);
void	*_kvm_malloc(kvm_t *, size_t);
off_t	 _kvm_pa2off(kvm_t *, paddr_t);
void	*_kvm_realloc(kvm_t *, void *, size_t);
void	 _kvm_syserr(kvm_t *, const char *, const char *, ...)
	    __attribute__((__format__(__printf__, 3, 4)));
ssize_t	_kvm_pread(kvm_t *, int, void *, size_t, off_t);

#define KREAD(kd, addr, obj) \
	(kvm_read(kd, addr, (obj), sizeof(*obj)) != sizeof(*obj))

#define KVM_ALLOC(kd, member, size) \
    do { \
	if (kd->member == NULL)	\
		kd->member = _kvm_malloc(kd, kd->member ## _len = size); \
	else if (kd->member ## _len < size) \
		kd->member = _kvm_realloc(kd, kd->member, \
		    kd->member ## _len = size); \
	if (kd->member == NULL) { \
		kd->member ## _len = 0; \
		return (NULL); \
	} \
    } while (/*CONSTCOND*/0)
