/*	$NetBSD: kern_exec.c,v 1.418 2015/10/02 16:54:15 christos Exp $	*/

/*-
 * Copyright (c) 2008 The NetBSD Foundation, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*-
 * Copyright (C) 1993, 1994, 1996 Christopher G. Demetriou
 * Copyright (C) 1992 Wolfgang Solfrank.
 * Copyright (C) 1992 TooLs GmbH.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: kern_exec.c,v 1.418 2015/10/02 16:54:15 christos Exp $");

#include "opt_exec.h"
#include "opt_execfmt.h"
#include "opt_ktrace.h"
#include "opt_modular.h"
#include "opt_syscall_debug.h"
#include "veriexec.h"
#include "opt_pax.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/filedesc.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/kmem.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/acct.h>
#include <sys/atomic.h>
#include <sys/exec.h>
#include <sys/ktrace.h>
#include <sys/uidinfo.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/ras.h>
#include <sys/signalvar.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/kauth.h>
#include <sys/lwpctl.h>
#include <sys/pax.h>
#include <sys/cpu.h>
#include <sys/module.h>
#include <sys/syscallvar.h>
#include <sys/syscallargs.h>
#if NVERIEXEC > 0
#include <sys/verified_exec.h>
#endif /* NVERIEXEC > 0 */
#include <sys/sdt.h>
#include <sys/spawn.h>
#include <sys/prot.h>
#include <sys/cprng.h>

#include <uvm/uvm_extern.h>

#include <machine/reg.h>

#include <compat/common/compat_util.h>

#ifndef MD_TOPDOWN_INIT
#ifdef __USE_TOPDOWN_VM
#define	MD_TOPDOWN_INIT(epp)	(epp)->ep_flags |= EXEC_TOPDOWN_VM
#else
#define	MD_TOPDOWN_INIT(epp)
#endif
#endif

struct execve_data;

static size_t calcargs(struct execve_data * restrict, const size_t);
static size_t calcstack(struct execve_data * restrict, const size_t);
static int copyoutargs(struct execve_data * restrict, struct lwp *,
    char * const);
static int copyoutpsstrs(struct execve_data * restrict, struct proc *);
static int copyinargs(struct execve_data * restrict, char * const *,
    char * const *, execve_fetch_element_t, char **);
static int copyinargstrs(struct execve_data * restrict, char * const *,
    execve_fetch_element_t, char **, size_t *, void (*)(const void *, size_t));
static int exec_sigcode_map(struct proc *, const struct emul *);

#ifdef DEBUG_EXEC
#define DPRINTF(a) printf a
#define COPYPRINTF(s, a, b) printf("%s, %d: copyout%s @%p %zu\n", __func__, \
    __LINE__, (s), (a), (b))
static void dump_vmcmds(const struct exec_package * const, size_t, int);
#define DUMPVMCMDS(p, x, e) do { dump_vmcmds((p), (x), (e)); } while (0)
#else
#define DPRINTF(a)
#define COPYPRINTF(s, a, b)
#define DUMPVMCMDS(p, x, e) do {} while (0)
#endif /* DEBUG_EXEC */

/*
 * DTrace SDT provider definitions
 */
SDT_PROVIDER_DECLARE(proc);
SDT_PROBE_DEFINE1(proc, kernel, , exec, "char *");
SDT_PROBE_DEFINE1(proc, kernel, , exec__success, "char *");
SDT_PROBE_DEFINE1(proc, kernel, , exec__failure, "int");

/*
 * Exec function switch:
 *
 * Note that each makecmds function is responsible for loading the
 * exec package with the necessary functions for any exec-type-specific
 * handling.
 *
 * Functions for specific exec types should be defined in their own
 * header file.
 */
static const struct execsw	**execsw = NULL;
static int			nexecs;

u_int	exec_maxhdrsz;	 /* must not be static - used by netbsd32 */

/* list of dynamically loaded execsw entries */
static LIST_HEAD(execlist_head, exec_entry) ex_head =
    LIST_HEAD_INITIALIZER(ex_head);
struct exec_entry {
	LIST_ENTRY(exec_entry)	ex_list;
	SLIST_ENTRY(exec_entry)	ex_slist;
	const struct execsw	*ex_sw;
};

#ifndef __HAVE_SYSCALL_INTERN
void	syscall(void);
#endif

/* NetBSD emul struct */
struct emul emul_netbsd = {
	.e_name =		"netbsd",
#ifdef EMUL_NATIVEROOT
	.e_path =		EMUL_NATIVEROOT,
#else
	.e_path =		NULL,
#endif
#ifndef __HAVE_MINIMAL_EMUL
	.e_flags =		EMUL_HAS_SYS___syscall,
	.e_errno =		NULL,
	.e_nosys =		SYS_syscall,
	.e_nsysent =		SYS_NSYSENT,
#endif
	.e_sysent =		sysent,
#ifdef SYSCALL_DEBUG
	.e_syscallnames =	syscallnames,
#else
	.e_syscallnames =	NULL,
#endif
	.e_sendsig =		sendsig,
	.e_trapsignal =		trapsignal,
	.e_tracesig =		NULL,
	.e_sigcode =		NULL,
	.e_esigcode =		NULL,
	.e_sigobject =		NULL,
	.e_setregs =		setregs,
	.e_proc_exec =		NULL,
	.e_proc_fork =		NULL,
	.e_proc_exit =		NULL,
	.e_lwp_fork =		NULL,
	.e_lwp_exit =		NULL,
#ifdef __HAVE_SYSCALL_INTERN
	.e_syscall_intern =	syscall_intern,
#else
	.e_syscall =		syscall,
#endif
	.e_sysctlovly =		NULL,
	.e_fault =		NULL,
	.e_vm_default_addr =	uvm_default_mapaddr,
	.e_usertrap =		NULL,
	.e_ucsize =		sizeof(ucontext_t),
	.e_startlwp =		startlwp
};

/*
 * Exec lock. Used to control access to execsw[] structures.
 * This must not be static so that netbsd32 can access it, too.
 */
krwlock_t exec_lock;

static kmutex_t sigobject_lock;

/*
 * Data used between a loadvm and execve part of an "exec" operation
 */
struct execve_data {
	struct exec_package	ed_pack;
	struct pathbuf		*ed_pathbuf;
	struct vattr		ed_attr;
	struct ps_strings	ed_arginfo;
	char			*ed_argp;
	const char		*ed_pathstring;
	char			*ed_resolvedpathbuf;
	size_t			ed_ps_strings_sz;
	int			ed_szsigcode;
	size_t			ed_argslen;
	long			ed_argc;
	long			ed_envc;
};

/*
 * data passed from parent lwp to child during a posix_spawn()
 */
struct spawn_exec_data {
	struct execve_data	sed_exec;
	struct posix_spawn_file_actions
				*sed_actions;
	struct posix_spawnattr	*sed_attrs;
	struct proc		*sed_parent;
	kcondvar_t		sed_cv_child_ready;
	kmutex_t		sed_mtx_child;
	int			sed_error;
	volatile uint32_t	sed_refcnt;
};

static void *
exec_pool_alloc(struct pool *pp, int flags)
{

	return (void *)uvm_km_alloc(kernel_map, NCARGS, 0,
	    UVM_KMF_PAGEABLE | UVM_KMF_WAITVA);
}

static void
exec_pool_free(struct pool *pp, void *addr)
{

	uvm_km_free(kernel_map, (vaddr_t)addr, NCARGS, UVM_KMF_PAGEABLE);
}

static struct pool exec_pool;

static struct pool_allocator exec_palloc = {
	.pa_alloc = exec_pool_alloc,
	.pa_free = exec_pool_free,
	.pa_pagesz = NCARGS
};

/*
 * check exec:
 * given an "executable" described in the exec package's namei info,
 * see what we can do with it.
 *
 * ON ENTRY:
 *	exec package with appropriate namei info
 *	lwp pointer of exec'ing lwp
 *	NO SELF-LOCKED VNODES
 *
 * ON EXIT:
 *	error:	nothing held, etc.  exec header still allocated.
 *	ok:	filled exec package, executable's vnode (unlocked).
 *
 * EXEC SWITCH ENTRY:
 * 	Locked vnode to check, exec package, proc.
 *
 * EXEC SWITCH EXIT:
 *	ok:	return 0, filled exec package, executable's vnode (unlocked).
 *	error:	destructive:
 *			everything deallocated execept exec header.
 *		non-destructive:
 *			error code, executable's vnode (unlocked),
 *			exec header unmodified.
 */
int
/*ARGSUSED*/
check_exec(struct lwp *l, struct exec_package *epp, struct pathbuf *pb)
{
	int		error, i;
	struct vnode	*vp;
	struct nameidata nd;
	size_t		resid;

	NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF | TRYEMULROOT, pb);

	/* first get the vnode */
	if ((error = namei(&nd)) != 0)
		return error;
	epp->ep_vp = vp = nd.ni_vp;
	/* normally this can't fail */
	error = copystr(nd.ni_pnbuf, epp->ep_resolvedname, PATH_MAX, NULL);
	KASSERT(error == 0);

#ifdef DIAGNOSTIC
	/* paranoia (take this out once namei stuff stabilizes) */
	memset(nd.ni_pnbuf, '~', PATH_MAX);
#endif

	/* check access and type */
	if (vp->v_type != VREG) {
		error = EACCES;
		goto bad1;
	}
	if ((error = VOP_ACCESS(vp, VEXEC, l->l_cred)) != 0)
		goto bad1;

	/* get attributes */
	if ((error = VOP_GETATTR(vp, epp->ep_vap, l->l_cred)) != 0)
		goto bad1;

	/* Check mount point */
	if (vp->v_mount->mnt_flag & MNT_NOEXEC) {
		error = EACCES;
		goto bad1;
	}
	if (vp->v_mount->mnt_flag & MNT_NOSUID)
		epp->ep_vap->va_mode &= ~(S_ISUID | S_ISGID);

	/* try to open it */
	if ((error = VOP_OPEN(vp, FREAD, l->l_cred)) != 0)
		goto bad1;

	/* unlock vp, since we need it unlocked from here on out. */
	VOP_UNLOCK(vp);

#if NVERIEXEC > 0
	error = veriexec_verify(l, vp, epp->ep_resolvedname,
	    epp->ep_flags & EXEC_INDIR ? VERIEXEC_INDIRECT : VERIEXEC_DIRECT,
	    NULL);
	if (error)
		goto bad2;
#endif /* NVERIEXEC > 0 */

#ifdef PAX_SEGVGUARD
	error = pax_segvguard(l, vp, epp->ep_resolvedname, false);
	if (error)
		goto bad2;
#endif /* PAX_SEGVGUARD */

	/* now we have the file, get the exec header */
	error = vn_rdwr(UIO_READ, vp, epp->ep_hdr, epp->ep_hdrlen, 0,
			UIO_SYSSPACE, 0, l->l_cred, &resid, NULL);
	if (error)
		goto bad2;
	epp->ep_hdrvalid = epp->ep_hdrlen - resid;

	/*
	 * Set up default address space limits.  Can be overridden
	 * by individual exec packages.
	 *
	 * XXX probably should be all done in the exec packages.
	 */
	epp->ep_vm_minaddr = VM_MIN_ADDRESS;
	epp->ep_vm_maxaddr = VM_MAXUSER_ADDRESS;
	/*
	 * set up the vmcmds for creation of the process
	 * address space
	 */
	error = ENOEXEC;
	for (i = 0; i < nexecs; i++) {
		int newerror;

		epp->ep_esch = execsw[i];
		newerror = (*execsw[i]->es_makecmds)(l, epp);

		if (!newerror) {
			/* Seems ok: check that entry point is not too high */
			if (epp->ep_entry > epp->ep_vm_maxaddr) {
#ifdef DIAGNOSTIC
				printf("%s: rejecting %p due to "
				    "too high entry address (> %p)\n",
					 __func__, (void *)epp->ep_entry,
					 (void *)epp->ep_vm_maxaddr);
#endif
				error = ENOEXEC;
				break;
			}
			/* Seems ok: check that entry point is not too low */
			if (epp->ep_entry < epp->ep_vm_minaddr) {
#ifdef DIAGNOSTIC
				printf("%s: rejecting %p due to "
				    "too low entry address (< %p)\n",
				     __func__, (void *)epp->ep_entry,
				     (void *)epp->ep_vm_minaddr);
#endif
				error = ENOEXEC;
				break;
			}

			/* check limits */
			if ((epp->ep_tsize > MAXTSIZ) ||
			    (epp->ep_dsize > (u_quad_t)l->l_proc->p_rlimit
						    [RLIMIT_DATA].rlim_cur)) {
#ifdef DIAGNOSTIC
				printf("%s: rejecting due to "
				    "limits (t=%llu > %llu || d=%llu > %llu)\n",
				    __func__,
				    (unsigned long long)epp->ep_tsize,
				    (unsigned long long)MAXTSIZ,
				    (unsigned long long)epp->ep_dsize,
				    (unsigned long long)
				    l->l_proc->p_rlimit[RLIMIT_DATA].rlim_cur);
#endif
				error = ENOMEM;
				break;
			}
			return 0;
		}

		if (epp->ep_emul_root != NULL) {
			vrele(epp->ep_emul_root);
			epp->ep_emul_root = NULL;
		}
		if (epp->ep_interp != NULL) {
			vrele(epp->ep_interp);
			epp->ep_interp = NULL;
		}

		/* make sure the first "interesting" error code is saved. */
		if (error == ENOEXEC)
			error = newerror;

		if (epp->ep_flags & EXEC_DESTR)
			/* Error from "#!" code, tidied up by recursive call */
			return error;
	}

	/* not found, error */

	/*
	 * free any vmspace-creation commands,
	 * and release their references
	 */
	kill_vmcmds(&epp->ep_vmcmds);

bad2:
	/*
	 * close and release the vnode, restore the old one, free the
	 * pathname buf, and punt.
	 */
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
	VOP_CLOSE(vp, FREAD, l->l_cred);
	vput(vp);
	return error;

bad1:
	/*
	 * free the namei pathname buffer, and put the vnode
	 * (which we don't yet have open).
	 */
	vput(vp);				/* was still locked */
	return error;
}

#ifdef __MACHINE_STACK_GROWS_UP
#define STACK_PTHREADSPACE NBPG
#else
#define STACK_PTHREADSPACE 0
#endif

static int
execve_fetch_element(char * const *array, size_t index, char **value)
{
	return copyin(array + index, value, sizeof(*value));
}

/*
 * exec system call
 */
int
sys_execve(struct lwp *l, const struct sys_execve_args *uap, register_t *retval)
{
	/* {
		syscallarg(const char *)	path;
		syscallarg(char * const *)	argp;
		syscallarg(char * const *)	envp;
	} */

	return execve1(l, SCARG(uap, path), SCARG(uap, argp),
	    SCARG(uap, envp), execve_fetch_element);
}

int
sys_fexecve(struct lwp *l, const struct sys_fexecve_args *uap,
    register_t *retval)
{
	/* {
		syscallarg(int)			fd;
		syscallarg(char * const *)	argp;
		syscallarg(char * const *)	envp;
	} */

	return ENOSYS;
}

/*
 * Load modules to try and execute an image that we do not understand.
 * If no execsw entries are present, we load those likely to be needed
 * in order to run native images only.  Otherwise, we autoload all
 * possible modules that could let us run the binary.  XXX lame
 */
static void
exec_autoload(void)
{
#ifdef MODULAR
	static const char * const native[] = {
		"exec_elf32",
		"exec_elf64",
		"exec_script",
		NULL
	};
	static const char * const compat[] = {
		"exec_elf32",
		"exec_elf64",
		"exec_script",
		"exec_aout",
		"exec_coff",
		"exec_ecoff",
		"compat_aoutm68k",
		"compat_freebsd",
		"compat_ibcs2",
		"compat_linux",
		"compat_linux32",
		"compat_netbsd32",
		"compat_sunos",
		"compat_sunos32",
		"compat_svr4",
		"compat_svr4_32",
		"compat_ultrix",
		NULL
	};
	char const * const *list;
	int i;

	list = (nexecs == 0 ? native : compat);
	for (i = 0; list[i] != NULL; i++) {
		if (module_autoload(list[i], MODULE_CLASS_EXEC) != 0) {
			continue;
		}
		yield();
	}
#endif
}

static int
makepathbuf(struct lwp *l, const char *upath, struct pathbuf **pbp,
    size_t *offs)
{
	char *path, *bp;
	size_t len, tlen;
	int error;
	struct cwdinfo *cwdi;

	path = PNBUF_GET();
	error = copyinstr(upath, path, MAXPATHLEN, &len);
	if (error) {
		PNBUF_PUT(path);
		DPRINTF(("%s: copyin path @%p %d\n", __func__, upath, error));
		return error;
	}

	if (path[0] == '/') {
		*offs = 0;
		goto out;
	}

	len++;
	if (len + 1 >= MAXPATHLEN)
		goto out;
	bp = path + MAXPATHLEN - len;
	memmove(bp, path, len);
	*(--bp) = '/';

	cwdi = l->l_proc->p_cwdi; 
	rw_enter(&cwdi->cwdi_lock, RW_READER);
	error = getcwd_common(cwdi->cwdi_cdir, NULL, &bp, path, MAXPATHLEN / 2,
	    GETCWD_CHECK_ACCESS, l);
	rw_exit(&cwdi->cwdi_lock);

	if (error) {
		DPRINTF(("%s: getcwd_common path %s %d\n", __func__, path,
		    error));
		goto out;
	}
	tlen = path + MAXPATHLEN - bp;

	memmove(path, bp, tlen);
	path[tlen] = '\0';
	*offs = tlen - len;
out:
	*pbp = pathbuf_assimilate(path);
	return 0;
}

static int
execve_loadvm(struct lwp *l, const char *path, char * const *args,
	char * const *envs, execve_fetch_element_t fetch_element,
	struct execve_data * restrict data)
{
	struct exec_package	* const epp = &data->ed_pack;
	int			error;
	struct proc		*p;
	char			*dp;
	u_int			modgen;
	size_t			offs = 0;	// XXX: GCC

	KASSERT(data != NULL);

	p = l->l_proc;
	modgen = 0;

	SDT_PROBE(proc, kernel, , exec, path, 0, 0, 0, 0);

	/*
	 * Check if we have exceeded our number of processes limit.
	 * This is so that we handle the case where a root daemon
	 * forked, ran setuid to become the desired user and is trying
	 * to exec. The obvious place to do the reference counting check
	 * is setuid(), but we don't do the reference counting check there
	 * like other OS's do because then all the programs that use setuid()
	 * must be modified to check the return code of setuid() and exit().
	 * It is dangerous to make setuid() fail, because it fails open and
	 * the program will continue to run as root. If we make it succeed
	 * and return an error code, again we are not enforcing the limit.
	 * The best place to enforce the limit is here, when the process tries
	 * to execute a new image, because eventually the process will need
	 * to call exec in order to do something useful.
	 */
 retry:
	if (p->p_flag & PK_SUGID) {
		if (kauth_authorize_process(l->l_cred, KAUTH_PROCESS_RLIMIT,
		     p, KAUTH_ARG(KAUTH_REQ_PROCESS_RLIMIT_BYPASS),
		     &p->p_rlimit[RLIMIT_NPROC],
		     KAUTH_ARG(RLIMIT_NPROC)) != 0 &&
		    chgproccnt(kauth_cred_getuid(l->l_cred), 0) >
		     p->p_rlimit[RLIMIT_NPROC].rlim_cur)
		return EAGAIN;
	}

	/*
	 * Drain existing references and forbid new ones.  The process
	 * should be left alone until we're done here.  This is necessary
	 * to avoid race conditions - e.g. in ptrace() - that might allow
	 * a local user to illicitly obtain elevated privileges.
	 */
	rw_enter(&p->p_reflock, RW_WRITER);

	/*
	 * Init the namei data to point the file user's program name.
	 * This is done here rather than in check_exec(), so that it's
	 * possible to override this settings if any of makecmd/probe
	 * functions call check_exec() recursively - for example,
	 * see exec_script_makecmds().
	 */
	if ((error = makepathbuf(l, path, &data->ed_pathbuf, &offs)) != 0)
		goto clrflg;
	data->ed_pathstring = pathbuf_stringcopy_get(data->ed_pathbuf);
	data->ed_resolvedpathbuf = PNBUF_GET();

	/*
	 * initialize the fields of the exec package.
	 */
	epp->ep_kname = data->ed_pathstring + offs;
	epp->ep_resolvedname = data->ed_resolvedpathbuf;
	epp->ep_hdr = kmem_alloc(exec_maxhdrsz, KM_SLEEP);
	epp->ep_hdrlen = exec_maxhdrsz;
	epp->ep_hdrvalid = 0;
	epp->ep_emul_arg = NULL;
	epp->ep_emul_arg_free = NULL;
	memset(&epp->ep_vmcmds, 0, sizeof(epp->ep_vmcmds));
	epp->ep_vap = &data->ed_attr;
	epp->ep_flags = (p->p_flag & PK_32) ? EXEC_FROM32 : 0;
	MD_TOPDOWN_INIT(epp);
	epp->ep_emul_root = NULL;
	epp->ep_interp = NULL;
	epp->ep_esch = NULL;
	epp->ep_pax_flags = 0;
	memset(epp->ep_machine_arch, 0, sizeof(epp->ep_machine_arch));

	rw_enter(&exec_lock, RW_READER);

	/* see if we can run it. */
	if ((error = check_exec(l, epp, data->ed_pathbuf)) != 0) {
		if (error != ENOENT) {
			DPRINTF(("%s: check exec failed %d\n",
			    __func__, error));
		}
		goto freehdr;
	}

	/* allocate an argument buffer */
	data->ed_argp = pool_get(&exec_pool, PR_WAITOK);
	KASSERT(data->ed_argp != NULL);
	dp = data->ed_argp;

	if ((error = copyinargs(data, args, envs, fetch_element, &dp)) != 0) {
		goto bad;
	}

	/*
	 * Calculate the new stack size.
	 */

#ifdef PAX_ASLR
#define	ASLR_GAP(epp)	(pax_aslr_epp_active(epp) ? (cprng_fast32() % PAGE_SIZE) : 0)
#else
#define	ASLR_GAP(epp)	0
#endif

#ifdef __MACHINE_STACK_GROWS_UP
/*
 * copyargs() fills argc/argv/envp from the lower address even on
 * __MACHINE_STACK_GROWS_UP machines.  Reserve a few words just below the SP
 * so that _rtld() use it.
 */
#define	RTLD_GAP	32
#else
#define	RTLD_GAP	0
#endif

	const size_t argenvstrlen = (char *)ALIGN(dp) - data->ed_argp;

	data->ed_argslen = calcargs(data, argenvstrlen);

	const size_t len = calcstack(data, ASLR_GAP(epp) + RTLD_GAP);

	if (len > epp->ep_ssize) {
		/* in effect, compare to initial limit */
		DPRINTF(("%s: stack limit exceeded %zu\n", __func__, len));
		error = ENOMEM;
		goto bad;
	}
	/* adjust "active stack depth" for process VSZ */
	epp->ep_ssize = len;

	return 0;

 bad:
	/* free the vmspace-creation commands, and release their references */
	kill_vmcmds(&epp->ep_vmcmds);
	/* kill any opened file descriptor, if necessary */
	if (epp->ep_flags & EXEC_HASFD) {
		epp->ep_flags &= ~EXEC_HASFD;
		fd_close(epp->ep_fd);
	}
	/* close and put the exec'd file */
	vn_lock(epp->ep_vp, LK_EXCLUSIVE | LK_RETRY);
	VOP_CLOSE(epp->ep_vp, FREAD, l->l_cred);
	vput(epp->ep_vp);
	pool_put(&exec_pool, data->ed_argp);

 freehdr:
	kmem_free(epp->ep_hdr, epp->ep_hdrlen);
	if (epp->ep_emul_root != NULL)
		vrele(epp->ep_emul_root);
	if (epp->ep_interp != NULL)
		vrele(epp->ep_interp);

	rw_exit(&exec_lock);

	pathbuf_stringcopy_put(data->ed_pathbuf, data->ed_pathstring);
	pathbuf_destroy(data->ed_pathbuf);
	PNBUF_PUT(data->ed_resolvedpathbuf);

 clrflg:
	rw_exit(&p->p_reflock);

	if (modgen != module_gen && error == ENOEXEC) {
		modgen = module_gen;
		exec_autoload();
		goto retry;
	}

	SDT_PROBE(proc, kernel, , exec__failure, error, 0, 0, 0, 0);
	return error;
}

static int
execve_dovmcmds(struct lwp *l, struct execve_data * restrict data)
{
	struct exec_package	* const epp = &data->ed_pack;
	struct proc		*p = l->l_proc;
	struct exec_vmcmd	*base_vcp;
	int			error = 0;
	size_t			i;

	/* record proc's vnode, for use by procfs and others */
	if (p->p_textvp)
		vrele(p->p_textvp);
	vref(epp->ep_vp);
	p->p_textvp = epp->ep_vp;

	/* create the new process's VM space by running the vmcmds */
	KASSERTMSG(epp->ep_vmcmds.evs_used != 0, "%s: no vmcmds", __func__);

	DUMPVMCMDS(epp, 0, 0);

	base_vcp = NULL;

	for (i = 0; i < epp->ep_vmcmds.evs_used && !error; i++) {
		struct exec_vmcmd *vcp;

		vcp = &epp->ep_vmcmds.evs_cmds[i];
		if (vcp->ev_flags & VMCMD_RELATIVE) {
			KASSERTMSG(base_vcp != NULL,
			    "%s: relative vmcmd with no base", __func__);
			KASSERTMSG((vcp->ev_flags & VMCMD_BASE) == 0,
			    "%s: illegal base & relative vmcmd", __func__);
			vcp->ev_addr += base_vcp->ev_addr;
		}
		error = (*vcp->ev_proc)(l, vcp);
		if (error)
			DUMPVMCMDS(epp, i, error);
		if (vcp->ev_flags & VMCMD_BASE)
			base_vcp = vcp;
	}

	/* free the vmspace-creation commands, and release their references */
	kill_vmcmds(&epp->ep_vmcmds);

	vn_lock(epp->ep_vp, LK_EXCLUSIVE | LK_RETRY);
	VOP_CLOSE(epp->ep_vp, FREAD, l->l_cred);
	vput(epp->ep_vp);

	/* if an error happened, deallocate and punt */
	if (error != 0) {
		DPRINTF(("%s: vmcmd %zu failed: %d\n", __func__, i - 1, error));
	}
	return error;
}

static void
execve_free_data(struct execve_data *data)
{
	struct exec_package	* const epp = &data->ed_pack;

	/* free the vmspace-creation commands, and release their references */
	kill_vmcmds(&epp->ep_vmcmds);
	/* kill any opened file descriptor, if necessary */
	if (epp->ep_flags & EXEC_HASFD) {
		epp->ep_flags &= ~EXEC_HASFD;
		fd_close(epp->ep_fd);
	}

	/* close and put the exec'd file */
	vn_lock(epp->ep_vp, LK_EXCLUSIVE | LK_RETRY);
	VOP_CLOSE(epp->ep_vp, FREAD, curlwp->l_cred);
	vput(epp->ep_vp);
	pool_put(&exec_pool, data->ed_argp);

	kmem_free(epp->ep_hdr, epp->ep_hdrlen);
	if (epp->ep_emul_root != NULL)
		vrele(epp->ep_emul_root);
	if (epp->ep_interp != NULL)
		vrele(epp->ep_interp);

	pathbuf_stringcopy_put(data->ed_pathbuf, data->ed_pathstring);
	pathbuf_destroy(data->ed_pathbuf);
	PNBUF_PUT(data->ed_resolvedpathbuf);
}

static void
pathexec(struct exec_package *epp, struct proc *p, const char *pathstring)
{
	const char		*commandname;
	size_t			commandlen;
	char			*path;

	/* set command name & other accounting info */
	commandname = strrchr(epp->ep_resolvedname, '/');
	if (commandname != NULL) {
		commandname++;
	} else {
		commandname = epp->ep_resolvedname;
	}
	commandlen = min(strlen(commandname), MAXCOMLEN);
	(void)memcpy(p->p_comm, commandname, commandlen);
	p->p_comm[commandlen] = '\0';


	/*
	 * If the path starts with /, we don't need to do any work.
	 * This handles the majority of the cases.
	 * In the future perhaps we could canonicalize it?
	 */
	if (pathstring[0] == '/') {
		path = PNBUF_GET();
		(void)strlcpy(path, pathstring, MAXPATHLEN);
		epp->ep_path = path;
	} else
		epp->ep_path = NULL;
}

/* XXX elsewhere */
static int
credexec(struct lwp *l, struct vattr *attr)
{
	struct proc *p = l->l_proc;
	int error;

	/*
	 * Deal with set[ug]id.  MNT_NOSUID has already been used to disable
	 * s[ug]id.  It's OK to check for PSL_TRACED here as we have blocked
	 * out additional references on the process for the moment.
	 */
	if ((p->p_slflag & PSL_TRACED) == 0 &&

	    (((attr->va_mode & S_ISUID) != 0 &&
	      kauth_cred_geteuid(l->l_cred) != attr->va_uid) ||

	     ((attr->va_mode & S_ISGID) != 0 &&
	      kauth_cred_getegid(l->l_cred) != attr->va_gid))) {
		/*
		 * Mark the process as SUGID before we do
		 * anything that might block.
		 */
		proc_crmod_enter();
		proc_crmod_leave(NULL, NULL, true);

		/* Make sure file descriptors 0..2 are in use. */
		if ((error = fd_checkstd()) != 0) {
			DPRINTF(("%s: fdcheckstd failed %d\n",
			    __func__, error));
			return error;
		}

		/*
		 * Copy the credential so other references don't see our
		 * changes.
		 */
		l->l_cred = kauth_cred_copy(l->l_cred);
#ifdef KTRACE
		/*
		 * If the persistent trace flag isn't set, turn off.
		 */
		if (p->p_tracep) {
			mutex_enter(&ktrace_lock);
			if (!(p->p_traceflag & KTRFAC_PERSISTENT))
				ktrderef(p);
			mutex_exit(&ktrace_lock);
		}
#endif
		if (attr->va_mode & S_ISUID)
			kauth_cred_seteuid(l->l_cred, attr->va_uid);
		if (attr->va_mode & S_ISGID)
			kauth_cred_setegid(l->l_cred, attr->va_gid);
	} else {
		if (kauth_cred_geteuid(l->l_cred) ==
		    kauth_cred_getuid(l->l_cred) &&
		    kauth_cred_getegid(l->l_cred) ==
		    kauth_cred_getgid(l->l_cred))
			p->p_flag &= ~PK_SUGID;
	}

	/*
	 * Copy the credential so other references don't see our changes.
	 * Test to see if this is necessary first, since in the common case
	 * we won't need a private reference.
	 */
	if (kauth_cred_geteuid(l->l_cred) != kauth_cred_getsvuid(l->l_cred) ||
	    kauth_cred_getegid(l->l_cred) != kauth_cred_getsvgid(l->l_cred)) {
		l->l_cred = kauth_cred_copy(l->l_cred);
		kauth_cred_setsvuid(l->l_cred, kauth_cred_geteuid(l->l_cred));
		kauth_cred_setsvgid(l->l_cred, kauth_cred_getegid(l->l_cred));
	}

	/* Update the master credentials. */
	if (l->l_cred != p->p_cred) {
		kauth_cred_t ocred;

		kauth_cred_hold(l->l_cred);
		mutex_enter(p->p_lock);
		ocred = p->p_cred;
		p->p_cred = l->l_cred;
		mutex_exit(p->p_lock);
		kauth_cred_free(ocred);
	}

	return 0;
}

static void
emulexec(struct lwp *l, struct exec_package *epp)
{
	struct proc		*p = l->l_proc;

	/* The emulation root will usually have been found when we looked
	 * for the elf interpreter (or similar), if not look now. */
	if (epp->ep_esch->es_emul->e_path != NULL &&
	    epp->ep_emul_root == NULL)
		emul_find_root(l, epp);

	/* Any old emulation root got removed by fdcloseexec */
	rw_enter(&p->p_cwdi->cwdi_lock, RW_WRITER);
	p->p_cwdi->cwdi_edir = epp->ep_emul_root;
	rw_exit(&p->p_cwdi->cwdi_lock);
	epp->ep_emul_root = NULL;
	if (epp->ep_interp != NULL)
		vrele(epp->ep_interp);

	/*
	 * Call emulation specific exec hook. This can setup per-process
	 * p->p_emuldata or do any other per-process stuff an emulation needs.
	 *
	 * If we are executing process of different emulation than the
	 * original forked process, call e_proc_exit() of the old emulation
	 * first, then e_proc_exec() of new emulation. If the emulation is
	 * same, the exec hook code should deallocate any old emulation
	 * resources held previously by this process.
	 */
	if (p->p_emul && p->p_emul->e_proc_exit
	    && p->p_emul != epp->ep_esch->es_emul)
		(*p->p_emul->e_proc_exit)(p);

	/*
	 * This is now LWP 1.
	 */
	/* XXX elsewhere */
	mutex_enter(p->p_lock);
	p->p_nlwpid = 1;
	l->l_lid = 1;
	mutex_exit(p->p_lock);

	/*
	 * Call exec hook. Emulation code may NOT store reference to anything
	 * from &pack.
	 */
	if (epp->ep_esch->es_emul->e_proc_exec)
		(*epp->ep_esch->es_emul->e_proc_exec)(p, epp);

	/* update p_emul, the old value is no longer needed */
	p->p_emul = epp->ep_esch->es_emul;

	/* ...and the same for p_execsw */
	p->p_execsw = epp->ep_esch;

#ifdef __HAVE_SYSCALL_INTERN
	(*p->p_emul->e_syscall_intern)(p);
#endif
	ktremul();
}

static int
execve_runproc(struct lwp *l, struct execve_data * restrict data,
	bool no_local_exec_lock, bool is_spawn)
{
	struct exec_package	* const epp = &data->ed_pack;
	int error = 0;
	struct proc		*p;

	/*
	 * In case of a posix_spawn operation, the child doing the exec
	 * might not hold the reader lock on exec_lock, but the parent
	 * will do this instead.
	 */
	KASSERT(no_local_exec_lock || rw_lock_held(&exec_lock));
	KASSERT(!no_local_exec_lock || is_spawn);
	KASSERT(data != NULL);

	p = l->l_proc;

	/* Get rid of other LWPs. */
	if (p->p_nlwps > 1) {
		mutex_enter(p->p_lock);
		exit_lwps(l);
		mutex_exit(p->p_lock);
	}
	KDASSERT(p->p_nlwps == 1);

	/* Destroy any lwpctl info. */
	if (p->p_lwpctl != NULL)
		lwp_ctl_exit();

	/* Remove POSIX timers */
	timers_free(p, TIMERS_POSIX);

	/* Set the PaX flags. */
	p->p_pax = epp->ep_pax_flags;

	/*
	 * Do whatever is necessary to prepare the address space
	 * for remapping.  Note that this might replace the current
	 * vmspace with another!
	 */
	if (is_spawn)
		uvmspace_spawn(l, epp->ep_vm_minaddr,
		    epp->ep_vm_maxaddr,
		    epp->ep_flags & EXEC_TOPDOWN_VM);
	else
		uvmspace_exec(l, epp->ep_vm_minaddr,
		    epp->ep_vm_maxaddr,
		    epp->ep_flags & EXEC_TOPDOWN_VM);

	struct vmspace		*vm;
	vm = p->p_vmspace;
	vm->vm_taddr = (void *)epp->ep_taddr;
	vm->vm_tsize = btoc(epp->ep_tsize);
	vm->vm_daddr = (void*)epp->ep_daddr;
	vm->vm_dsize = btoc(epp->ep_dsize);
	vm->vm_ssize = btoc(epp->ep_ssize);
	vm->vm_issize = 0;
	vm->vm_maxsaddr = (void *)epp->ep_maxsaddr;
	vm->vm_minsaddr = (void *)epp->ep_minsaddr;

#ifdef PAX_ASLR
	pax_aslr_init_vm(l, vm);
#endif /* PAX_ASLR */

	/* Now map address space. */
	error = execve_dovmcmds(l, data);
	if (error != 0)
		goto exec_abort;

	pathexec(epp, p, data->ed_pathstring);

	char * const newstack = STACK_GROW(vm->vm_minsaddr, epp->ep_ssize);

	error = copyoutargs(data, l, newstack);
	if (error != 0)
		goto exec_abort;

	cwdexec(p);
	fd_closeexec();		/* handle close on exec */

	if (__predict_false(ktrace_on))
		fd_ktrexecfd();

	execsigs(p);		/* reset catched signals */

	mutex_enter(p->p_lock);
	l->l_ctxlink = NULL;	/* reset ucontext link */
	p->p_acflag &= ~AFORK;
	p->p_flag |= PK_EXEC;
	mutex_exit(p->p_lock);

	/*
	 * Stop profiling.
	 */
	if ((p->p_stflag & PST_PROFIL) != 0) {
		mutex_spin_enter(&p->p_stmutex);
		stopprofclock(p);
		mutex_spin_exit(&p->p_stmutex);
	}

	/*
	 * It's OK to test PL_PPWAIT unlocked here, as other LWPs have
	 * exited and exec()/exit() are the only places it will be cleared.
	 */
	if ((p->p_lflag & PL_PPWAIT) != 0) {
#if 0
		lwp_t *lp;

		mutex_enter(proc_lock);
		lp = p->p_vforklwp;
		p->p_vforklwp = NULL;

		l->l_lwpctl = NULL; /* was on loan from blocked parent */
		p->p_lflag &= ~PL_PPWAIT;

		lp->l_pflag &= ~LP_VFORKWAIT; /* XXX */
		cv_broadcast(&lp->l_waitcv);
		mutex_exit(proc_lock);
#else
		mutex_enter(proc_lock);
		l->l_lwpctl = NULL; /* was on loan from blocked parent */
		p->p_lflag &= ~PL_PPWAIT;
		cv_broadcast(&p->p_pptr->p_waitcv);
		mutex_exit(proc_lock);
#endif
	}

	error = credexec(l, &data->ed_attr);
	if (error)
		goto exec_abort;

#if defined(__HAVE_RAS)
	/*
	 * Remove all RASs from the address space.
	 */
	ras_purgeall();
#endif

	doexechooks(p);

	/*
	 * Set initial SP at the top of the stack.
	 *
	 * Note that on machines where stack grows up (e.g. hppa), SP points to
	 * the end of arg/env strings.  Userland guesses the address of argc
	 * via ps_strings::ps_argvstr.
	 */

	/* Setup new registers and do misc. setup. */
	(*epp->ep_esch->es_emul->e_setregs)(l, epp, (vaddr_t)newstack);
	if (epp->ep_esch->es_setregs)
		(*epp->ep_esch->es_setregs)(l, epp, (vaddr_t)newstack);

	/* Provide a consistent LWP private setting */
	(void)lwp_setprivate(l, NULL);

	/* Discard all PCU state; need to start fresh */
	pcu_discard_all(l);

	/* map the process's signal trampoline code */
	if ((error = exec_sigcode_map(p, epp->ep_esch->es_emul)) != 0) {
		DPRINTF(("%s: map sigcode failed %d\n", __func__, error));
		goto exec_abort;
	}

	pool_put(&exec_pool, data->ed_argp);

	/* notify others that we exec'd */
	KNOTE(&p->p_klist, NOTE_EXEC);

	kmem_free(epp->ep_hdr, epp->ep_hdrlen);

	SDT_PROBE(proc, kernel, , exec__success, epp->ep_kname, 0, 0, 0, 0);

	emulexec(l, epp);

	/* Allow new references from the debugger/procfs. */
	rw_exit(&p->p_reflock);
	if (!no_local_exec_lock)
		rw_exit(&exec_lock);

	mutex_enter(proc_lock);

	if ((p->p_slflag & (PSL_TRACED|PSL_SYSCALL)) == PSL_TRACED) {
		ksiginfo_t ksi;

		KSI_INIT_EMPTY(&ksi);
		ksi.ksi_signo = SIGTRAP;
		ksi.ksi_lid = l->l_lid;
		kpsignal(p, &ksi, NULL);
	}

	if (p->p_sflag & PS_STOPEXEC) {
		ksiginfoq_t kq;

		KERNEL_UNLOCK_ALL(l, &l->l_biglocks);
		p->p_pptr->p_nstopchild++;
		p->p_pptr->p_waited = 0;
		mutex_enter(p->p_lock);
		ksiginfo_queue_init(&kq);
		sigclearall(p, &contsigmask, &kq);
		lwp_lock(l);
		l->l_stat = LSSTOP;
		p->p_stat = SSTOP;
		p->p_nrlwps--;
		lwp_unlock(l);
		mutex_exit(p->p_lock);
		mutex_exit(proc_lock);
		lwp_lock(l);
		mi_switch(l);
		ksiginfo_queue_drain(&kq);
		KERNEL_LOCK(l->l_biglocks, l);
	} else {
		mutex_exit(proc_lock);
	}

	pathbuf_stringcopy_put(data->ed_pathbuf, data->ed_pathstring);
	pathbuf_destroy(data->ed_pathbuf);
	PNBUF_PUT(data->ed_resolvedpathbuf);
	DPRINTF(("%s finished\n", __func__));
	return EJUSTRETURN;

 exec_abort:
	SDT_PROBE(proc, kernel, , exec__failure, error, 0, 0, 0, 0);
	rw_exit(&p->p_reflock);
	if (!no_local_exec_lock)
		rw_exit(&exec_lock);

	pathbuf_stringcopy_put(data->ed_pathbuf, data->ed_pathstring);
	pathbuf_destroy(data->ed_pathbuf);
	PNBUF_PUT(data->ed_resolvedpathbuf);

	/*
	 * the old process doesn't exist anymore.  exit gracefully.
	 * get rid of the (new) address space we have created, if any, get rid
	 * of our namei data and vnode, and exit noting failure
	 */
	uvm_deallocate(&vm->vm_map, VM_MIN_ADDRESS,
		VM_MAXUSER_ADDRESS - VM_MIN_ADDRESS);

	exec_free_emul_arg(epp);
	pool_put(&exec_pool, data->ed_argp);
	kmem_free(epp->ep_hdr, epp->ep_hdrlen);
	if (epp->ep_emul_root != NULL)
		vrele(epp->ep_emul_root);
	if (epp->ep_interp != NULL)
		vrele(epp->ep_interp);

	/* Acquire the sched-state mutex (exit1() will release it). */
	if (!is_spawn) {
		mutex_enter(p->p_lock);
		exit1(l, W_EXITCODE(error, SIGABRT));
	}

	return error;
}

int
execve1(struct lwp *l, const char *path, char * const *args,
    char * const *envs, execve_fetch_element_t fetch_element)
{
	struct execve_data data;
	int error;

	error = execve_loadvm(l, path, args, envs, fetch_element, &data);
	if (error)
		return error;
	error = execve_runproc(l, &data, false, false);
	return error;
}

static size_t
fromptrsz(const struct exec_package *epp)
{
	return (epp->ep_flags & EXEC_FROM32) ? sizeof(int) : sizeof(char *);
}

static size_t
ptrsz(const struct exec_package *epp)
{
	return (epp->ep_flags & EXEC_32) ? sizeof(int) : sizeof(char *);
}

static size_t
calcargs(struct execve_data * restrict data, const size_t argenvstrlen)
{
	struct exec_package	* const epp = &data->ed_pack;

	const size_t nargenvptrs =
	    1 +				/* long argc */
	    data->ed_argc +		/* char *argv[] */
	    1 +				/* \0 */
	    data->ed_envc +		/* char *env[] */
	    1 +				/* \0 */
	    epp->ep_esch->es_arglen;	/* auxinfo */

	return (nargenvptrs * ptrsz(epp)) + argenvstrlen;
}

static size_t
calcstack(struct execve_data * restrict data, const size_t gaplen)
{
	struct exec_package	* const epp = &data->ed_pack;

	data->ed_szsigcode = epp->ep_esch->es_emul->e_esigcode -
	    epp->ep_esch->es_emul->e_sigcode;

	data->ed_ps_strings_sz = (epp->ep_flags & EXEC_32) ?
	    sizeof(struct ps_strings32) : sizeof(struct ps_strings);

	const size_t sigcode_psstr_sz =
	    data->ed_szsigcode +	/* sigcode */
	    data->ed_ps_strings_sz +	/* ps_strings */
	    STACK_PTHREADSPACE;		/* pthread space */

	const size_t stacklen =
	    data->ed_argslen +
	    gaplen +
	    sigcode_psstr_sz;

	/* make the stack "safely" aligned */
	return STACK_LEN_ALIGN(stacklen, STACK_ALIGNBYTES);
}

static int
copyoutargs(struct execve_data * restrict data, struct lwp *l,
    char * const newstack)
{
	struct exec_package	* const epp = &data->ed_pack;
	struct proc		*p = l->l_proc;
	int			error;

	/* remember information about the process */
	data->ed_arginfo.ps_nargvstr = data->ed_argc;
	data->ed_arginfo.ps_nenvstr = data->ed_envc;

	/*
	 * Allocate the stack address passed to the newly execve()'ed process.
	 *
	 * The new stack address will be set to the SP (stack pointer) register
	 * in setregs().
	 */

	char *newargs = STACK_ALLOC(
	    STACK_SHRINK(newstack, data->ed_argslen), data->ed_argslen);

	error = (*epp->ep_esch->es_copyargs)(l, epp,
	    &data->ed_arginfo, &newargs, data->ed_argp);

	if (epp->ep_path) {
		PNBUF_PUT(epp->ep_path);
		epp->ep_path = NULL;
	}
	if (error) {
		DPRINTF(("%s: copyargs failed %d\n", __func__, error));
		return error;
	}

	error = copyoutpsstrs(data, p);
	if (error != 0)
		return error;

	return 0;
}

static int
copyoutpsstrs(struct execve_data * restrict data, struct proc *p)
{
	struct exec_package	* const epp = &data->ed_pack;
	struct ps_strings32	arginfo32;
	void			*aip;
	int			error;

	/* fill process ps_strings info */
	p->p_psstrp = (vaddr_t)STACK_ALLOC(STACK_GROW(epp->ep_minsaddr,
	    STACK_PTHREADSPACE), data->ed_ps_strings_sz);

	if (epp->ep_flags & EXEC_32) {
		aip = &arginfo32;
		arginfo32.ps_argvstr = (vaddr_t)data->ed_arginfo.ps_argvstr;
		arginfo32.ps_nargvstr = data->ed_arginfo.ps_nargvstr;
		arginfo32.ps_envstr = (vaddr_t)data->ed_arginfo.ps_envstr;
		arginfo32.ps_nenvstr = data->ed_arginfo.ps_nenvstr;
	} else
		aip = &data->ed_arginfo;

	/* copy out the process's ps_strings structure */
	if ((error = copyout(aip, (void *)p->p_psstrp, data->ed_ps_strings_sz))
	    != 0) {
		DPRINTF(("%s: ps_strings copyout %p->%p size %zu failed\n",
		    __func__, aip, (void *)p->p_psstrp, data->ed_ps_strings_sz));
		return error;
	}

	return 0;
}

static int
copyinargs(struct execve_data * restrict data, char * const *args,
    char * const *envs, execve_fetch_element_t fetch_element, char **dpp)
{
	struct exec_package	* const epp = &data->ed_pack;
	char			*dp;
	size_t			i;
	int			error;

	dp = *dpp;

	data->ed_argc = 0;

	/* copy the fake args list, if there's one, freeing it as we go */
	if (epp->ep_flags & EXEC_HASARGL) {
		struct exec_fakearg	*fa = epp->ep_fa;

		while (fa->fa_arg != NULL) {
			const size_t maxlen = ARG_MAX - (dp - data->ed_argp);
			size_t len;

			len = strlcpy(dp, fa->fa_arg, maxlen);
			/* Count NUL into len. */
			if (len < maxlen)
				len++;
			else {
				while (fa->fa_arg != NULL) {
					kmem_free(fa->fa_arg, fa->fa_len);
					fa++;
				}
				kmem_free(epp->ep_fa, epp->ep_fa_len);
				epp->ep_flags &= ~EXEC_HASARGL;
				return E2BIG;
			}
			ktrexecarg(fa->fa_arg, len - 1);
			dp += len;

			kmem_free(fa->fa_arg, fa->fa_len);
			fa++;
			data->ed_argc++;
		}
		kmem_free(epp->ep_fa, epp->ep_fa_len);
		epp->ep_flags &= ~EXEC_HASARGL;
	}

	/*
	 * Read and count argument strings from user.
	 */

	if (args == NULL) {
		DPRINTF(("%s: null args\n", __func__));
		return EINVAL;
	}
	if (epp->ep_flags & EXEC_SKIPARG)
		args = (const void *)((const char *)args + fromptrsz(epp));
	i = 0;
	error = copyinargstrs(data, args, fetch_element, &dp, &i, ktr_execarg);
	if (error != 0) {
		DPRINTF(("%s: copyin arg %d\n", __func__, error));
		return error;
	}
	data->ed_argc += i;

	/*
	 * Read and count environment strings from user.
	 */

	data->ed_envc = 0;
	/* environment need not be there */
	if (envs == NULL)
		goto done;
	i = 0;
	error = copyinargstrs(data, envs, fetch_element, &dp, &i, ktr_execenv);
	if (error != 0) {
		DPRINTF(("%s: copyin env %d\n", __func__, error));
		return error;
	}
	data->ed_envc += i;

done:
	*dpp = dp;

	return 0;
}

static int
copyinargstrs(struct execve_data * restrict data, char * const *strs,
    execve_fetch_element_t fetch_element, char **dpp, size_t *ip,
    void (*ktr)(const void *, size_t))
{
	char			*dp, *sp;
	size_t			i;
	int			error;

	dp = *dpp;

	i = 0;
	while (1) {
		const size_t maxlen = ARG_MAX - (dp - data->ed_argp);
		size_t len;

		if ((error = (*fetch_element)(strs, i, &sp)) != 0) {
			return error;
		}
		if (!sp)
			break;
		if ((error = copyinstr(sp, dp, maxlen, &len)) != 0) {
			if (error == ENAMETOOLONG)
				error = E2BIG;
			return error;
		}
		if (__predict_false(ktrace_on))
			(*ktr)(dp, len - 1);
		dp += len;
		i++;
	}

	*dpp = dp;
	*ip = i;

	return 0;
}

/*
 * Copy argv and env strings from kernel buffer (argp) to the new stack.
 * Those strings are located just after auxinfo.
 */
int
copyargs(struct lwp *l, struct exec_package *pack, struct ps_strings *arginfo,
    char **stackp, void *argp)
{
	char	**cpp, *dp, *sp;
	size_t	len;
	void	*nullp;
	long	argc, envc;
	int	error;

	cpp = (char **)*stackp;
	nullp = NULL;
	argc = arginfo->ps_nargvstr;
	envc = arginfo->ps_nenvstr;

	/* argc on stack is long */
	CTASSERT(sizeof(*cpp) == sizeof(argc));

	dp = (char *)(cpp +
	    1 +				/* long argc */
	    argc +			/* char *argv[] */
	    1 +				/* \0 */
	    envc +			/* char *env[] */
	    1 +				/* \0 */
	    /* XXX auxinfo multiplied by ptr size? */
	    pack->ep_esch->es_arglen);	/* auxinfo */
	sp = argp;

	if ((error = copyout(&argc, cpp++, sizeof(argc))) != 0) {
		COPYPRINTF("", cpp - 1, sizeof(argc));
		return error;
	}

	/* XXX don't copy them out, remap them! */
	arginfo->ps_argvstr = cpp; /* remember location of argv for later */

	for (; --argc >= 0; sp += len, dp += len) {
		if ((error = copyout(&dp, cpp++, sizeof(dp))) != 0) {
			COPYPRINTF("", cpp - 1, sizeof(dp));
			return error;
		}
		if ((error = copyoutstr(sp, dp, ARG_MAX, &len)) != 0) {
			COPYPRINTF("str", dp, (size_t)ARG_MAX);
			return error;
		}
	}

	if ((error = copyout(&nullp, cpp++, sizeof(nullp))) != 0) {
		COPYPRINTF("", cpp - 1, sizeof(nullp));
		return error;
	}

	arginfo->ps_envstr = cpp; /* remember location of envp for later */

	for (; --envc >= 0; sp += len, dp += len) {
		if ((error = copyout(&dp, cpp++, sizeof(dp))) != 0) {
			COPYPRINTF("", cpp - 1, sizeof(dp));
			return error;
		}
		if ((error = copyoutstr(sp, dp, ARG_MAX, &len)) != 0) {
			COPYPRINTF("str", dp, (size_t)ARG_MAX);
			return error;
		}

	}

	if ((error = copyout(&nullp, cpp++, sizeof(nullp))) != 0) {
		COPYPRINTF("", cpp - 1, sizeof(nullp));
		return error;
	}

	*stackp = (char *)cpp;
	return 0;
}


/*
 * Add execsw[] entries.
 */
int
exec_add(struct execsw *esp, int count)
{
	struct exec_entry	*it;
	int			i;

	if (count == 0) {
		return 0;
	}

	/* Check for duplicates. */
	rw_enter(&exec_lock, RW_WRITER);
	for (i = 0; i < count; i++) {
		LIST_FOREACH(it, &ex_head, ex_list) {
			/* assume unique (makecmds, probe_func, emulation) */
			if (it->ex_sw->es_makecmds == esp[i].es_makecmds &&
			    it->ex_sw->u.elf_probe_func ==
			    esp[i].u.elf_probe_func &&
			    it->ex_sw->es_emul == esp[i].es_emul) {
				rw_exit(&exec_lock);
				return EEXIST;
			}
		}
	}

	/* Allocate new entries. */
	for (i = 0; i < count; i++) {
		it = kmem_alloc(sizeof(*it), KM_SLEEP);
		it->ex_sw = &esp[i];
		LIST_INSERT_HEAD(&ex_head, it, ex_list);
	}

	/* update execsw[] */
	exec_init(0);
	rw_exit(&exec_lock);
	return 0;
}

/*
 * Remove execsw[] entry.
 */
int
exec_remove(struct execsw *esp, int count)
{
	struct exec_entry	*it, *next;
	int			i;
	const struct proclist_desc *pd;
	proc_t			*p;

	if (count == 0) {
		return 0;
	}

	/* Abort if any are busy. */
	rw_enter(&exec_lock, RW_WRITER);
	for (i = 0; i < count; i++) {
		mutex_enter(proc_lock);
		for (pd = proclists; pd->pd_list != NULL; pd++) {
			PROCLIST_FOREACH(p, pd->pd_list) {
				if (p->p_execsw == &esp[i]) {
					mutex_exit(proc_lock);
					rw_exit(&exec_lock);
					return EBUSY;
				}
			}
		}
		mutex_exit(proc_lock);
	}

	/* None are busy, so remove them all. */
	for (i = 0; i < count; i++) {
		for (it = LIST_FIRST(&ex_head); it != NULL; it = next) {
			next = LIST_NEXT(it, ex_list);
			if (it->ex_sw == &esp[i]) {
				LIST_REMOVE(it, ex_list);
				kmem_free(it, sizeof(*it));
				break;
			}
		}
	}

	/* update execsw[] */
	exec_init(0);
	rw_exit(&exec_lock);
	return 0;
}

/*
 * Initialize exec structures. If init_boot is true, also does necessary
 * one-time initialization (it's called from main() that way).
 * Once system is multiuser, this should be called with exec_lock held,
 * i.e. via exec_{add|remove}().
 */
int
exec_init(int init_boot)
{
	const struct execsw 	**sw;
	struct exec_entry	*ex;
	SLIST_HEAD(,exec_entry)	first;
	SLIST_HEAD(,exec_entry)	any;
	SLIST_HEAD(,exec_entry)	last;
	int			i, sz;

	if (init_boot) {
		/* do one-time initializations */
		rw_init(&exec_lock);
		mutex_init(&sigobject_lock, MUTEX_DEFAULT, IPL_NONE);
		pool_init(&exec_pool, NCARGS, 0, 0, PR_NOALIGN|PR_NOTOUCH,
		    "execargs", &exec_palloc, IPL_NONE);
		pool_sethardlimit(&exec_pool, maxexec, "should not happen", 0);
	} else {
		KASSERT(rw_write_held(&exec_lock));
	}

	/* Sort each entry onto the appropriate queue. */
	SLIST_INIT(&first);
	SLIST_INIT(&any);
	SLIST_INIT(&last);
	sz = 0;
	LIST_FOREACH(ex, &ex_head, ex_list) {
		switch(ex->ex_sw->es_prio) {
		case EXECSW_PRIO_FIRST:
			SLIST_INSERT_HEAD(&first, ex, ex_slist);
			break;
		case EXECSW_PRIO_ANY:
			SLIST_INSERT_HEAD(&any, ex, ex_slist);
			break;
		case EXECSW_PRIO_LAST:
			SLIST_INSERT_HEAD(&last, ex, ex_slist);
			break;
		default:
			panic("%s", __func__);
			break;
		}
		sz++;
	}

	/*
	 * Create new execsw[].  Ensure we do not try a zero-sized
	 * allocation.
	 */
	sw = kmem_alloc(sz * sizeof(struct execsw *) + 1, KM_SLEEP);
	i = 0;
	SLIST_FOREACH(ex, &first, ex_slist) {
		sw[i++] = ex->ex_sw;
	}
	SLIST_FOREACH(ex, &any, ex_slist) {
		sw[i++] = ex->ex_sw;
	}
	SLIST_FOREACH(ex, &last, ex_slist) {
		sw[i++] = ex->ex_sw;
	}

	/* Replace old execsw[] and free used memory. */
	if (execsw != NULL) {
		kmem_free(__UNCONST(execsw),
		    nexecs * sizeof(struct execsw *) + 1);
	}
	execsw = sw;
	nexecs = sz;

	/* Figure out the maximum size of an exec header. */
	exec_maxhdrsz = sizeof(int);
	for (i = 0; i < nexecs; i++) {
		if (execsw[i]->es_hdrsz > exec_maxhdrsz)
			exec_maxhdrsz = execsw[i]->es_hdrsz;
	}

	return 0;
}

static int
exec_sigcode_map(struct proc *p, const struct emul *e)
{
	vaddr_t va;
	vsize_t sz;
	int error;
	struct uvm_object *uobj;

	sz = (vaddr_t)e->e_esigcode - (vaddr_t)e->e_sigcode;

	if (e->e_sigobject == NULL || sz == 0) {
		return 0;
	}

	/*
	 * If we don't have a sigobject for this emulation, create one.
	 *
	 * sigobject is an anonymous memory object (just like SYSV shared
	 * memory) that we keep a permanent reference to and that we map
	 * in all processes that need this sigcode. The creation is simple,
	 * we create an object, add a permanent reference to it, map it in
	 * kernel space, copy out the sigcode to it and unmap it.
	 * We map it with PROT_READ|PROT_EXEC into the process just
	 * the way sys_mmap() would map it.
	 */

	uobj = *e->e_sigobject;
	if (uobj == NULL) {
		mutex_enter(&sigobject_lock);
		if ((uobj = *e->e_sigobject) == NULL) {
			uobj = uao_create(sz, 0);
			(*uobj->pgops->pgo_reference)(uobj);
			va = vm_map_min(kernel_map);
			if ((error = uvm_map(kernel_map, &va, round_page(sz),
			    uobj, 0, 0,
			    UVM_MAPFLAG(UVM_PROT_RW, UVM_PROT_RW,
			    UVM_INH_SHARE, UVM_ADV_RANDOM, 0)))) {
				printf("kernel mapping failed %d\n", error);
				(*uobj->pgops->pgo_detach)(uobj);
				mutex_exit(&sigobject_lock);
				return error;
			}
			memcpy((void *)va, e->e_sigcode, sz);
#ifdef PMAP_NEED_PROCWR
			pmap_procwr(&proc0, va, sz);
#endif
			uvm_unmap(kernel_map, va, va + round_page(sz));
			*e->e_sigobject = uobj;
		}
		mutex_exit(&sigobject_lock);
	}

	/* Just a hint to uvm_map where to put it. */
	va = e->e_vm_default_addr(p, (vaddr_t)p->p_vmspace->vm_daddr,
	    round_page(sz));

#ifdef __alpha__
	/*
	 * Tru64 puts /sbin/loader at the end of user virtual memory,
	 * which causes the above calculation to put the sigcode at
	 * an invalid address.  Put it just below the text instead.
	 */
	if (va == (vaddr_t)vm_map_max(&p->p_vmspace->vm_map)) {
		va = (vaddr_t)p->p_vmspace->vm_taddr - round_page(sz);
	}
#endif

	(*uobj->pgops->pgo_reference)(uobj);
	error = uvm_map(&p->p_vmspace->vm_map, &va, round_page(sz),
			uobj, 0, 0,
			UVM_MAPFLAG(UVM_PROT_RX, UVM_PROT_RX, UVM_INH_SHARE,
				    UVM_ADV_RANDOM, 0));
	if (error) {
		DPRINTF(("%s, %d: map %p "
		    "uvm_map %#"PRIxVSIZE"@%#"PRIxVADDR" failed %d\n",
		    __func__, __LINE__, &p->p_vmspace->vm_map, round_page(sz),
		    va, error));
		(*uobj->pgops->pgo_detach)(uobj);
		return error;
	}
	p->p_sigctx.ps_sigcode = (void *)va;
	return 0;
}

/*
 * Release a refcount on spawn_exec_data and destroy memory, if this
 * was the last one.
 */
static void
spawn_exec_data_release(struct spawn_exec_data *data)
{
	if (atomic_dec_32_nv(&data->sed_refcnt) != 0)
		return;

	cv_destroy(&data->sed_cv_child_ready);
	mutex_destroy(&data->sed_mtx_child);

	if (data->sed_actions)
		posix_spawn_fa_free(data->sed_actions,
		    data->sed_actions->len);
	if (data->sed_attrs)
		kmem_free(data->sed_attrs,
		    sizeof(*data->sed_attrs));
	kmem_free(data, sizeof(*data));
}

/*
 * A child lwp of a posix_spawn operation starts here and ends up in
 * cpu_spawn_return, dealing with all filedescriptor and scheduler
 * manipulations in between.
 * The parent waits for the child, as it is not clear whether the child
 * will be able to acquire its own exec_lock. If it can, the parent can
 * be released early and continue running in parallel. If not (or if the
 * magic debug flag is passed in the scheduler attribute struct), the
 * child rides on the parent's exec lock until it is ready to return to
 * to userland - and only then releases the parent. This method loses
 * concurrency, but improves error reporting.
 */
static void
spawn_return(void *arg)
{
	struct spawn_exec_data *spawn_data = arg;
	struct lwp *l = curlwp;
	int error, newfd;
	size_t i;
	const struct posix_spawn_file_actions_entry *fae;
	pid_t ppid;
	register_t retval;
	bool have_reflock;
	bool parent_is_waiting = true;

	/*
	 * Check if we can release parent early.
	 * We either need to have no sed_attrs, or sed_attrs does not
	 * have POSIX_SPAWN_RETURNERROR or one of the flags, that require
	 * safe access to the parent proc (passed in sed_parent).
	 * We then try to get the exec_lock, and only if that works, we can
	 * release the parent here already.
	 */
	ppid = spawn_data->sed_parent->p_pid;
	if ((!spawn_data->sed_attrs
	    || (spawn_data->sed_attrs->sa_flags
	        & (POSIX_SPAWN_RETURNERROR|POSIX_SPAWN_SETPGROUP)) == 0)
	    && rw_tryenter(&exec_lock, RW_READER)) {
		parent_is_waiting = false;
		mutex_enter(&spawn_data->sed_mtx_child);
		cv_signal(&spawn_data->sed_cv_child_ready);
		mutex_exit(&spawn_data->sed_mtx_child);
	}

	/* don't allow debugger access yet */
	rw_enter(&l->l_proc->p_reflock, RW_WRITER);
	have_reflock = true;

	error = 0;
	/* handle posix_spawn_file_actions */
	if (spawn_data->sed_actions != NULL) {
		for (i = 0; i < spawn_data->sed_actions->len; i++) {
			fae = &spawn_data->sed_actions->fae[i];
			switch (fae->fae_action) {
			case FAE_OPEN:
				if (fd_getfile(fae->fae_fildes) != NULL) {
					error = fd_close(fae->fae_fildes);
					if (error)
						break;
				}
				error = fd_open(fae->fae_path, fae->fae_oflag,
				    fae->fae_mode, &newfd);
				if (error)
					break;
				if (newfd != fae->fae_fildes) {
					error = dodup(l, newfd,
					    fae->fae_fildes, 0, &retval);
					if (fd_getfile(newfd) != NULL)
						fd_close(newfd);
				}
				break;
			case FAE_DUP2:
				error = dodup(l, fae->fae_fildes,
				    fae->fae_newfildes, 0, &retval);
				break;
			case FAE_CLOSE:
				if (fd_getfile(fae->fae_fildes) == NULL) {
					error = EBADF;
					break;
				}
				error = fd_close(fae->fae_fildes);
				break;
			}
			if (error)
				goto report_error;
		}
	}

	/* handle posix_spawnattr */
	if (spawn_data->sed_attrs != NULL) {
		int ostat;
		struct sigaction sigact;
		sigact._sa_u._sa_handler = SIG_DFL;
		sigact.sa_flags = 0;

		/* 
		 * set state to SSTOP so that this proc can be found by pid.
		 * see proc_enterprp, do_sched_setparam below
		 */
		ostat = l->l_proc->p_stat;
		l->l_proc->p_stat = SSTOP;

		/* Set process group */
		if (spawn_data->sed_attrs->sa_flags & POSIX_SPAWN_SETPGROUP) {
			pid_t mypid = l->l_proc->p_pid,
			     pgrp = spawn_data->sed_attrs->sa_pgroup;

			if (pgrp == 0)
				pgrp = mypid;

			error = proc_enterpgrp(spawn_data->sed_parent,
			    mypid, pgrp, false);
			if (error)
				goto report_error;
		}

		/* Set scheduler policy */
		if (spawn_data->sed_attrs->sa_flags & POSIX_SPAWN_SETSCHEDULER)
			error = do_sched_setparam(l->l_proc->p_pid, 0,
			    spawn_data->sed_attrs->sa_schedpolicy,
			    &spawn_data->sed_attrs->sa_schedparam);
		else if (spawn_data->sed_attrs->sa_flags
		    & POSIX_SPAWN_SETSCHEDPARAM) {
			error = do_sched_setparam(ppid, 0,
			    SCHED_NONE, &spawn_data->sed_attrs->sa_schedparam);
		}
		if (error)
			goto report_error;

		/* Reset user ID's */
		if (spawn_data->sed_attrs->sa_flags & POSIX_SPAWN_RESETIDS) {
			error = do_setresuid(l, -1,
			     kauth_cred_getgid(l->l_cred), -1,
			     ID_E_EQ_R | ID_E_EQ_S);
			if (error)
				goto report_error;
			error = do_setresuid(l, -1,
			    kauth_cred_getuid(l->l_cred), -1,
			    ID_E_EQ_R | ID_E_EQ_S);
			if (error)
				goto report_error;
		}

		/* Set signal masks/defaults */
		if (spawn_data->sed_attrs->sa_flags & POSIX_SPAWN_SETSIGMASK) {
			mutex_enter(l->l_proc->p_lock);
			error = sigprocmask1(l, SIG_SETMASK,
			    &spawn_data->sed_attrs->sa_sigmask, NULL);
			mutex_exit(l->l_proc->p_lock);
			if (error)
				goto report_error;
		}

		if (spawn_data->sed_attrs->sa_flags & POSIX_SPAWN_SETSIGDEF) {
			/*
			 * The following sigaction call is using a sigaction
			 * version 0 trampoline which is in the compatibility
			 * code only. This is not a problem because for SIG_DFL
			 * and SIG_IGN, the trampolines are now ignored. If they
			 * were not, this would be a problem because we are
			 * holding the exec_lock, and the compat code needs
			 * to do the same in order to replace the trampoline
			 * code of the process.
			 */
			for (i = 1; i <= NSIG; i++) {
				if (sigismember(
				    &spawn_data->sed_attrs->sa_sigdefault, i))
					sigaction1(l, i, &sigact, NULL, NULL,
					    0);
			}
		}
		l->l_proc->p_stat = ostat;
	}

	/* now do the real exec */
	error = execve_runproc(l, &spawn_data->sed_exec, parent_is_waiting,
	    true);
	have_reflock = false;
	if (error == EJUSTRETURN)
		error = 0;
	else if (error)
		goto report_error;

	if (parent_is_waiting) {
		mutex_enter(&spawn_data->sed_mtx_child);
		cv_signal(&spawn_data->sed_cv_child_ready);
		mutex_exit(&spawn_data->sed_mtx_child);
	}

	/* release our refcount on the data */
	spawn_exec_data_release(spawn_data);

	/* and finally: leave to userland for the first time */
	cpu_spawn_return(l);

	/* NOTREACHED */
	return;

 report_error:
	if (have_reflock) {
		/*
		 * We have not passed through execve_runproc(),
		 * which would have released the p_reflock and also
		 * taken ownership of the sed_exec part of spawn_data,
		 * so release/free both here.
		 */
		rw_exit(&l->l_proc->p_reflock);
		execve_free_data(&spawn_data->sed_exec);
	}

	if (parent_is_waiting) {
		/* pass error to parent */
		mutex_enter(&spawn_data->sed_mtx_child);
		spawn_data->sed_error = error;
		cv_signal(&spawn_data->sed_cv_child_ready);
		mutex_exit(&spawn_data->sed_mtx_child);
	} else {
		rw_exit(&exec_lock);
	}

	/* release our refcount on the data */
	spawn_exec_data_release(spawn_data);

	/* done, exit */
	mutex_enter(l->l_proc->p_lock);
	/*
	 * Posix explicitly asks for an exit code of 127 if we report
	 * errors from the child process - so, unfortunately, there
	 * is no way to report a more exact error code.
	 * A NetBSD specific workaround is POSIX_SPAWN_RETURNERROR as
	 * flag bit in the attrp argument to posix_spawn(2), see above.
	 */
	exit1(l, W_EXITCODE(127, 0));
}

void
posix_spawn_fa_free(struct posix_spawn_file_actions *fa, size_t len)
{

	for (size_t i = 0; i < len; i++) {
		struct posix_spawn_file_actions_entry *fae = &fa->fae[i];
		if (fae->fae_action != FAE_OPEN)
			continue;
		kmem_free(fae->fae_path, strlen(fae->fae_path) + 1);
	}
	if (fa->len > 0)
		kmem_free(fa->fae, sizeof(*fa->fae) * fa->len);
	kmem_free(fa, sizeof(*fa));
}

static int
posix_spawn_fa_alloc(struct posix_spawn_file_actions **fap,
    const struct posix_spawn_file_actions *ufa, rlim_t lim)
{
	struct posix_spawn_file_actions *fa;
	struct posix_spawn_file_actions_entry *fae;
	char *pbuf = NULL;
	int error;
	size_t i = 0;

	fa = kmem_alloc(sizeof(*fa), KM_SLEEP);
	error = copyin(ufa, fa, sizeof(*fa));
	if (error || fa->len == 0) {
		kmem_free(fa, sizeof(*fa));
		return error;	/* 0 if not an error, and len == 0 */
	}

	if (fa->len > lim) {
		kmem_free(fa, sizeof(*fa));
		return EINVAL;
	}

	fa->size = fa->len;
	size_t fal = fa->len * sizeof(*fae);
	fae = fa->fae;
	fa->fae = kmem_alloc(fal, KM_SLEEP);
	error = copyin(fae, fa->fae, fal);
	if (error)
		goto out;

	pbuf = PNBUF_GET();
	for (; i < fa->len; i++) {
		fae = &fa->fae[i];
		if (fae->fae_action != FAE_OPEN)
			continue;
		error = copyinstr(fae->fae_path, pbuf, MAXPATHLEN, &fal);
		if (error)
			goto out;
		fae->fae_path = kmem_alloc(fal, KM_SLEEP);
		memcpy(fae->fae_path, pbuf, fal);
	}
	PNBUF_PUT(pbuf);

	*fap = fa;
	return 0;
out:
	if (pbuf)
		PNBUF_PUT(pbuf);
	posix_spawn_fa_free(fa, i);
	return error;
}

int
check_posix_spawn(struct lwp *l1)
{
	int error, tnprocs, count;
	uid_t uid;
	struct proc *p1;

	p1 = l1->l_proc;
	uid = kauth_cred_getuid(l1->l_cred);
	tnprocs = atomic_inc_uint_nv(&nprocs);

	/*
	 * Although process entries are dynamically created, we still keep
	 * a global limit on the maximum number we will create.
	 */
	if (__predict_false(tnprocs >= maxproc))
		error = -1;
	else
		error = kauth_authorize_process(l1->l_cred,
		    KAUTH_PROCESS_FORK, p1, KAUTH_ARG(tnprocs), NULL, NULL);

	if (error) {
		atomic_dec_uint(&nprocs);
		return EAGAIN;
	}

	/*
	 * Enforce limits.
	 */
	count = chgproccnt(uid, 1);
	if (kauth_authorize_process(l1->l_cred, KAUTH_PROCESS_RLIMIT,
	     p1, KAUTH_ARG(KAUTH_REQ_PROCESS_RLIMIT_BYPASS),
	     &p1->p_rlimit[RLIMIT_NPROC], KAUTH_ARG(RLIMIT_NPROC)) != 0 &&
	    __predict_false(count > p1->p_rlimit[RLIMIT_NPROC].rlim_cur)) {
		(void)chgproccnt(uid, -1);
		atomic_dec_uint(&nprocs);
		return EAGAIN;
	}

	return 0;
}

int
do_posix_spawn(struct lwp *l1, pid_t *pid_res, bool *child_ok, const char *path,
	struct posix_spawn_file_actions *fa,
	struct posix_spawnattr *sa,
	char *const *argv, char *const *envp,
	execve_fetch_element_t fetch)
{

	struct proc *p1, *p2;
	struct lwp *l2;
	int error;
	struct spawn_exec_data *spawn_data;
	vaddr_t uaddr;
	pid_t pid;
	bool have_exec_lock = false;

	p1 = l1->l_proc;

	/* Allocate and init spawn_data */
	spawn_data = kmem_zalloc(sizeof(*spawn_data), KM_SLEEP);
	spawn_data->sed_refcnt = 1; /* only parent so far */
	cv_init(&spawn_data->sed_cv_child_ready, "pspawn");
	mutex_init(&spawn_data->sed_mtx_child, MUTEX_DEFAULT, IPL_NONE);
	mutex_enter(&spawn_data->sed_mtx_child);

	/*
	 * Do the first part of the exec now, collect state
	 * in spawn_data.
	 */
	error = execve_loadvm(l1, path, argv,
	    envp, fetch, &spawn_data->sed_exec);
	if (error == EJUSTRETURN)
		error = 0;
	else if (error)
		goto error_exit;

	have_exec_lock = true;

	/*
	 * Allocate virtual address space for the U-area now, while it
	 * is still easy to abort the fork operation if we're out of
	 * kernel virtual address space.
	 */
	uaddr = uvm_uarea_alloc();
	if (__predict_false(uaddr == 0)) {
		error = ENOMEM;
		goto error_exit;
	}
	
	/*
	 * Allocate new proc. Borrow proc0 vmspace for it, we will
	 * replace it with its own before returning to userland
	 * in the child.
	 * This is a point of no return, we will have to go through
	 * the child proc to properly clean it up past this point.
	 */
	p2 = proc_alloc();
	pid = p2->p_pid;

	/*
	 * Make a proc table entry for the new process.
	 * Start by zeroing the section of proc that is zero-initialized,
	 * then copy the section that is copied directly from the parent.
	 */
	memset(&p2->p_startzero, 0,
	    (unsigned) ((char *)&p2->p_endzero - (char *)&p2->p_startzero));
	memcpy(&p2->p_startcopy, &p1->p_startcopy,
	    (unsigned) ((char *)&p2->p_endcopy - (char *)&p2->p_startcopy));
	p2->p_vmspace = proc0.p_vmspace;

	TAILQ_INIT(&p2->p_sigpend.sp_info);

	LIST_INIT(&p2->p_lwps);
	LIST_INIT(&p2->p_sigwaiters);

	/*
	 * Duplicate sub-structures as needed.
	 * Increase reference counts on shared objects.
	 * Inherit flags we want to keep.  The flags related to SIGCHLD
	 * handling are important in order to keep a consistent behaviour
	 * for the child after the fork.  If we are a 32-bit process, the
	 * child will be too.
	 */
	p2->p_flag =
	    p1->p_flag & (PK_SUGID | PK_NOCLDWAIT | PK_CLDSIGIGN | PK_32);
	p2->p_emul = p1->p_emul;
	p2->p_execsw = p1->p_execsw;

	mutex_init(&p2->p_stmutex, MUTEX_DEFAULT, IPL_HIGH);
	mutex_init(&p2->p_auxlock, MUTEX_DEFAULT, IPL_NONE);
	rw_init(&p2->p_reflock);
	cv_init(&p2->p_waitcv, "wait");
	cv_init(&p2->p_lwpcv, "lwpwait");

	p2->p_lock = mutex_obj_alloc(MUTEX_DEFAULT, IPL_NONE);

	kauth_proc_fork(p1, p2);

	p2->p_raslist = NULL;
	p2->p_fd = fd_copy();

	/* XXX racy */
	p2->p_mqueue_cnt = p1->p_mqueue_cnt;

	p2->p_cwdi = cwdinit();

	/*
	 * Note: p_limit (rlimit stuff) is copy-on-write, so normally
	 * we just need increase pl_refcnt.
	 */
	if (!p1->p_limit->pl_writeable) {
		lim_addref(p1->p_limit);
		p2->p_limit = p1->p_limit;
	} else {
		p2->p_limit = lim_copy(p1->p_limit);
	}

	p2->p_lflag = 0;
	p2->p_sflag = 0;
	p2->p_slflag = 0;
	p2->p_pptr = p1;
	p2->p_ppid = p1->p_pid;
	LIST_INIT(&p2->p_children);

	p2->p_aio = NULL;

#ifdef KTRACE
	/*
	 * Copy traceflag and tracefile if enabled.
	 * If not inherited, these were zeroed above.
	 */
	if (p1->p_traceflag & KTRFAC_INHERIT) {
		mutex_enter(&ktrace_lock);
		p2->p_traceflag = p1->p_traceflag;
		if ((p2->p_tracep = p1->p_tracep) != NULL)
			ktradref(p2);
		mutex_exit(&ktrace_lock);
	}
#endif

	/*
	 * Create signal actions for the child process.
	 */
	p2->p_sigacts = sigactsinit(p1, 0);
	mutex_enter(p1->p_lock);
	p2->p_sflag |=
	    (p1->p_sflag & (PS_STOPFORK | PS_STOPEXEC | PS_NOCLDSTOP));
	sched_proc_fork(p1, p2);
	mutex_exit(p1->p_lock);

	p2->p_stflag = p1->p_stflag;

	/*
	 * p_stats.
	 * Copy parts of p_stats, and zero out the rest.
	 */
	p2->p_stats = pstatscopy(p1->p_stats);

	/* copy over machdep flags to the new proc */
	cpu_proc_fork(p1, p2);

	/*
	 * Prepare remaining parts of spawn data
	 */
	spawn_data->sed_actions = fa;
	spawn_data->sed_attrs = sa;

	spawn_data->sed_parent = p1;

	/* create LWP */
	lwp_create(l1, p2, uaddr, 0, NULL, 0, spawn_return, spawn_data,
	    &l2, l1->l_class);
	l2->l_ctxlink = NULL;	/* reset ucontext link */

	/*
	 * Copy the credential so other references don't see our changes.
	 * Test to see if this is necessary first, since in the common case
	 * we won't need a private reference.
	 */
	if (kauth_cred_geteuid(l2->l_cred) != kauth_cred_getsvuid(l2->l_cred) ||
	    kauth_cred_getegid(l2->l_cred) != kauth_cred_getsvgid(l2->l_cred)) {
		l2->l_cred = kauth_cred_copy(l2->l_cred);
		kauth_cred_setsvuid(l2->l_cred, kauth_cred_geteuid(l2->l_cred));
		kauth_cred_setsvgid(l2->l_cred, kauth_cred_getegid(l2->l_cred));
	}

	/* Update the master credentials. */
	if (l2->l_cred != p2->p_cred) {
		kauth_cred_t ocred;

		kauth_cred_hold(l2->l_cred);
		mutex_enter(p2->p_lock);
		ocred = p2->p_cred;
		p2->p_cred = l2->l_cred;
		mutex_exit(p2->p_lock);
		kauth_cred_free(ocred);
	}

	*child_ok = true;
	spawn_data->sed_refcnt = 2;	/* child gets it as well */
#if 0
	l2->l_nopreempt = 1; /* start it non-preemptable */
#endif

	/*
	 * It's now safe for the scheduler and other processes to see the
	 * child process.
	 */
	mutex_enter(proc_lock);

	if (p1->p_session->s_ttyvp != NULL && p1->p_lflag & PL_CONTROLT)
		p2->p_lflag |= PL_CONTROLT;

	LIST_INSERT_HEAD(&p1->p_children, p2, p_sibling);
	p2->p_exitsig = SIGCHLD;	/* signal for parent on exit */

	LIST_INSERT_AFTER(p1, p2, p_pglist);
	LIST_INSERT_HEAD(&allproc, p2, p_list);

	p2->p_trace_enabled = trace_is_enabled(p2);
#ifdef __HAVE_SYSCALL_INTERN
	(*p2->p_emul->e_syscall_intern)(p2);
#endif

	/*
	 * Make child runnable, set start time, and add to run queue except
	 * if the parent requested the child to start in SSTOP state.
	 */
	mutex_enter(p2->p_lock);

	getmicrotime(&p2->p_stats->p_start);

	lwp_lock(l2);
	KASSERT(p2->p_nrlwps == 1);
	p2->p_nrlwps = 1;
	p2->p_stat = SACTIVE;
	l2->l_stat = LSRUN;
	sched_enqueue(l2, false);
	lwp_unlock(l2);

	mutex_exit(p2->p_lock);
	mutex_exit(proc_lock);

	cv_wait(&spawn_data->sed_cv_child_ready, &spawn_data->sed_mtx_child);
	error = spawn_data->sed_error;
	mutex_exit(&spawn_data->sed_mtx_child);
	spawn_exec_data_release(spawn_data);

	rw_exit(&p1->p_reflock);
	rw_exit(&exec_lock);
	have_exec_lock = false;

	*pid_res = pid;
	return error;

 error_exit:
	if (have_exec_lock) {
		execve_free_data(&spawn_data->sed_exec);
		rw_exit(&p1->p_reflock);
		rw_exit(&exec_lock);
	}
	mutex_exit(&spawn_data->sed_mtx_child);
	spawn_exec_data_release(spawn_data);

	return error;
}

int
sys_posix_spawn(struct lwp *l1, const struct sys_posix_spawn_args *uap,
    register_t *retval)
{
	/* {
		syscallarg(pid_t *) pid;
		syscallarg(const char *) path;
		syscallarg(const struct posix_spawn_file_actions *) file_actions;
		syscallarg(const struct posix_spawnattr *) attrp;
		syscallarg(char *const *) argv;
		syscallarg(char *const *) envp;
	} */	

	int error;
	struct posix_spawn_file_actions *fa = NULL;
	struct posix_spawnattr *sa = NULL;
	pid_t pid;
	bool child_ok = false;
	rlim_t max_fileactions;
	proc_t *p = l1->l_proc;

	error = check_posix_spawn(l1);
	if (error) {
		*retval = error;
		return 0;
	}

	/* copy in file_actions struct */
	if (SCARG(uap, file_actions) != NULL) {
		max_fileactions = 2 * min(p->p_rlimit[RLIMIT_NOFILE].rlim_cur,
		    maxfiles);
		error = posix_spawn_fa_alloc(&fa, SCARG(uap, file_actions),
		    max_fileactions);
		if (error)
			goto error_exit;
	}

	/* copyin posix_spawnattr struct */
	if (SCARG(uap, attrp) != NULL) {
		sa = kmem_alloc(sizeof(*sa), KM_SLEEP);
		error = copyin(SCARG(uap, attrp), sa, sizeof(*sa));
		if (error)
			goto error_exit;
	}

	/*
	 * Do the spawn
	 */
	error = do_posix_spawn(l1, &pid, &child_ok, SCARG(uap, path), fa, sa,
	    SCARG(uap, argv), SCARG(uap, envp), execve_fetch_element);
	if (error)
		goto error_exit;

	if (error == 0 && SCARG(uap, pid) != NULL)
		error = copyout(&pid, SCARG(uap, pid), sizeof(pid));

	*retval = error;
	return 0;

 error_exit:
	if (!child_ok) {
		(void)chgproccnt(kauth_cred_getuid(l1->l_cred), -1);
		atomic_dec_uint(&nprocs);

		if (sa)
			kmem_free(sa, sizeof(*sa));
		if (fa)
			posix_spawn_fa_free(fa, fa->len);
	}

	*retval = error;
	return 0;
}

void
exec_free_emul_arg(struct exec_package *epp)
{
	if (epp->ep_emul_arg_free != NULL) {
		KASSERT(epp->ep_emul_arg != NULL);
		(*epp->ep_emul_arg_free)(epp->ep_emul_arg);
		epp->ep_emul_arg_free = NULL;
		epp->ep_emul_arg = NULL;
	} else {
		KASSERT(epp->ep_emul_arg == NULL);
	}
}

#ifdef DEBUG_EXEC
static void
dump_vmcmds(const struct exec_package * const epp, size_t x, int error)
{
	struct exec_vmcmd *vp = &epp->ep_vmcmds.evs_cmds[0];
	size_t j;

	if (error == 0)
		DPRINTF(("vmcmds %u\n", epp->ep_vmcmds.evs_used));
	else
		DPRINTF(("vmcmds %zu/%u, error %d\n", x, 
		    epp->ep_vmcmds.evs_used, error));

	for (j = 0; j < epp->ep_vmcmds.evs_used; j++) {
		DPRINTF(("vmcmd[%zu] = vmcmd_map_%s %#"
		    PRIxVADDR"/%#"PRIxVSIZE" fd@%#"
		    PRIxVSIZE" prot=0%o flags=%d\n", j,
		    vp[j].ev_proc == vmcmd_map_pagedvn ?
		    "pagedvn" :
		    vp[j].ev_proc == vmcmd_map_readvn ?
		    "readvn" :
		    vp[j].ev_proc == vmcmd_map_zero ?
		    "zero" : "*unknown*",
		    vp[j].ev_addr, vp[j].ev_len,
		    vp[j].ev_offset, vp[j].ev_prot,
		    vp[j].ev_flags));
		if (error != 0 && j == x)
			DPRINTF(("     ^--- failed\n"));
	}
}
#endif
