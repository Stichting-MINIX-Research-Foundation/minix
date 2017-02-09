/*	$NetBSD: exec_elf.c,v 1.77 2015/09/26 16:12:24 maxv Exp $	*/

/*-
 * Copyright (c) 1994, 2000, 2005 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas.
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

/*
 * Copyright (c) 1996 Christopher G. Demetriou
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(1, "$NetBSD: exec_elf.c,v 1.77 2015/09/26 16:12:24 maxv Exp $");

#ifdef _KERNEL_OPT
#include "opt_pax.h"
#endif /* _KERNEL_OPT */

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/kmem.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/exec.h>
#include <sys/exec_elf.h>
#include <sys/syscall.h>
#include <sys/signalvar.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/kauth.h>
#include <sys/bitops.h>
#include <sys/cprng.h>

#include <sys/cpu.h>
#include <machine/reg.h>

#include <compat/common/compat_util.h>

#include <sys/pax.h>
#include <uvm/uvm_param.h>

extern struct emul emul_netbsd;

#define elf_check_header	ELFNAME(check_header)
#define elf_copyargs		ELFNAME(copyargs)
#define elf_load_interp		ELFNAME(load_interp)
#define elf_load_psection	ELFNAME(load_psection)
#define exec_elf_makecmds	ELFNAME2(exec,makecmds)
#define netbsd_elf_signature	ELFNAME2(netbsd,signature)
#define netbsd_elf_probe	ELFNAME2(netbsd,probe)
#define	coredump		ELFNAMEEND(coredump)
#define	elf_free_emul_arg	ELFNAME(free_emul_arg)

static int
elf_load_interp(struct lwp *, struct exec_package *, char *,
    struct exec_vmcmd_set *, u_long *, Elf_Addr *);
static void
elf_load_psection(struct exec_vmcmd_set *, struct vnode *, const Elf_Phdr *,
    Elf_Addr *, u_long *, int);

int	netbsd_elf_signature(struct lwp *, struct exec_package *, Elf_Ehdr *);
int	netbsd_elf_probe(struct lwp *, struct exec_package *, void *, char *,
	    vaddr_t *);

static void	elf_free_emul_arg(void *);

/* round up and down to page boundaries. */
#define	ELF_ROUND(a, b)		(((a) + (b) - 1) & ~((b) - 1))
#define	ELF_TRUNC(a, b)		((a) & ~((b) - 1))

static void
elf_placedynexec(struct exec_package *epp, Elf_Ehdr *eh, Elf_Phdr *ph)
{
	Elf_Addr align, offset;
	int i;

	for (align = i = 0; i < eh->e_phnum; i++)
		if (ph[i].p_type == PT_LOAD && ph[i].p_align > align)
			align = ph[i].p_align;

#ifdef PAX_ASLR
	if (pax_aslr_epp_active(epp)) {
		size_t pax_align, l2, delta;
		uint32_t r;

		pax_align = align;

		r = cprng_fast32();

		if (pax_align == 0)
			pax_align = PGSHIFT;
		l2 = ilog2(pax_align);
		delta = PAX_ASLR_DELTA(r, l2, PAX_ASLR_DELTA_EXEC_LEN);
		offset = ELF_TRUNC(delta, pax_align) + PAGE_SIZE;
#ifdef PAX_ASLR_DEBUG
		uprintf("r=0x%x l2=0x%zx PGSHIFT=0x%x Delta=0x%zx\n", r, l2,
		    PGSHIFT, delta);
		uprintf("pax offset=0x%llx entry=0x%llx\n",
		    (unsigned long long)offset,
		    (unsigned long long)eh->e_entry);
#endif /* PAX_ASLR_DEBUG */
	} else
#endif /* PAX_ASLR */
		offset = MAX(align, PAGE_SIZE);

	offset += epp->ep_vm_minaddr;

	for (i = 0; i < eh->e_phnum; i++)
		ph[i].p_vaddr += offset;
	epp->ep_entryoffset = offset;
	eh->e_entry += offset;
}

/*
 * Copy arguments onto the stack in the normal way, but add some
 * extra information in case of dynamic binding.
 */
int
elf_copyargs(struct lwp *l, struct exec_package *pack,
    struct ps_strings *arginfo, char **stackp, void *argp)
{
	size_t len, vlen;
	AuxInfo ai[ELF_AUX_ENTRIES], *a, *execname;
	struct elf_args *ap;
	int error;

	if ((error = copyargs(l, pack, arginfo, stackp, argp)) != 0)
		return error;

	a = ai;
	execname = NULL;

	memset(ai, 0, sizeof(ai));

	/*
	 * Push extra arguments on the stack needed by dynamically
	 * linked binaries
	 */
	if ((ap = (struct elf_args *)pack->ep_emul_arg)) {
		struct vattr *vap = pack->ep_vap;

		a->a_type = AT_PHDR;
		a->a_v = ap->arg_phaddr;
		a++;

		a->a_type = AT_PHENT;
		a->a_v = ap->arg_phentsize;
		a++;

		a->a_type = AT_PHNUM;
		a->a_v = ap->arg_phnum;
		a++;

		a->a_type = AT_PAGESZ;
		a->a_v = PAGE_SIZE;
		a++;

		a->a_type = AT_BASE;
		a->a_v = ap->arg_interp;
		a++;

		a->a_type = AT_FLAGS;
		a->a_v = 0;
		a++;

		a->a_type = AT_ENTRY;
		a->a_v = ap->arg_entry;
		a++;

		a->a_type = AT_EUID;
		if (vap->va_mode & S_ISUID)
			a->a_v = vap->va_uid;
		else
			a->a_v = kauth_cred_geteuid(l->l_cred);
		a++;

		a->a_type = AT_RUID;
		a->a_v = kauth_cred_getuid(l->l_cred);
		a++;

		a->a_type = AT_EGID;
		if (vap->va_mode & S_ISGID)
			a->a_v = vap->va_gid;
		else
			a->a_v = kauth_cred_getegid(l->l_cred);
		a++;

		a->a_type = AT_RGID;
		a->a_v = kauth_cred_getgid(l->l_cred);
		a++;

		a->a_type = AT_STACKBASE;
		a->a_v = l->l_proc->p_stackbase;
		a++;

		if (pack->ep_path) {
			execname = a;
			a->a_type = AT_SUN_EXECNAME;
			a++;
		}

		exec_free_emul_arg(pack);
	}

	a->a_type = AT_NULL;
	a->a_v = 0;
	a++;

	vlen = (a - ai) * sizeof(ai[0]);

	KASSERT(vlen <= sizeof(ai));

	if (execname) {
		char *path = pack->ep_path;
		execname->a_v = (uintptr_t)(*stackp + vlen);
		len = strlen(path) + 1;
		if ((error = copyout(path, (*stackp + vlen), len)) != 0)
			return error;
		len = ALIGN(len);
	} else
		len = 0;

	if ((error = copyout(ai, *stackp, vlen)) != 0)
		return error;
	*stackp += vlen + len;

	return 0;
}

/*
 * elf_check_header():
 *
 * Check header for validity; return 0 if ok, ENOEXEC if error
 */
int
elf_check_header(Elf_Ehdr *eh)
{

	if (memcmp(eh->e_ident, ELFMAG, SELFMAG) != 0 ||
	    eh->e_ident[EI_CLASS] != ELFCLASS)
		return ENOEXEC;

	switch (eh->e_machine) {

	ELFDEFNNAME(MACHDEP_ID_CASES)

	default:
		return ENOEXEC;
	}

	if (ELF_EHDR_FLAGS_OK(eh) == 0)
		return ENOEXEC;

	if (eh->e_shnum > ELF_MAXSHNUM || eh->e_phnum > ELF_MAXPHNUM)
		return ENOEXEC;

	return 0;
}

/*
 * elf_load_psection():
 *
 * Load a psection at the appropriate address
 */
static void
elf_load_psection(struct exec_vmcmd_set *vcset, struct vnode *vp,
    const Elf_Phdr *ph, Elf_Addr *addr, u_long *size, int flags)
{
	u_long msize, psize, rm, rf;
	long diff, offset;
	int vmprot = 0;

	/*
	 * If the user specified an address, then we load there.
	 */
	if (*addr == ELFDEFNNAME(NO_ADDR))
		*addr = ph->p_vaddr;

	if (ph->p_align > 1) {
		/*
		 * Make sure we are virtually aligned as we are supposed to be.
		 */
		diff = ph->p_vaddr - ELF_TRUNC(ph->p_vaddr, ph->p_align);
		KASSERT(*addr - diff == ELF_TRUNC(*addr, ph->p_align));
		/*
		 * But make sure to not map any pages before the start of the
		 * psection by limiting the difference to within a page.
		 */
		diff &= PAGE_MASK;
	} else
		diff = 0;

	vmprot |= (ph->p_flags & PF_R) ? VM_PROT_READ : 0;
	vmprot |= (ph->p_flags & PF_W) ? VM_PROT_WRITE : 0;
	vmprot |= (ph->p_flags & PF_X) ? VM_PROT_EXECUTE : 0;

	/*
	 * Adjust everything so it all starts on a page boundary.
	 */
	*addr -= diff;
	offset = ph->p_offset - diff;
	*size = ph->p_filesz + diff;
	msize = ph->p_memsz + diff;

	if (ph->p_align >= PAGE_SIZE) {
		if ((ph->p_flags & PF_W) != 0) {
			/*
			 * Because the pagedvn pager can't handle zero fill
			 * of the last data page if it's not page aligned we
			 * map the last page readvn.
			 */
			psize = trunc_page(*size);
		} else {
			psize = round_page(*size);
		}
	} else {
		psize = *size;
	}

	if (psize > 0) {
		NEW_VMCMD2(vcset, ph->p_align < PAGE_SIZE ?
		    vmcmd_map_readvn : vmcmd_map_pagedvn, psize, *addr, vp,
		    offset, vmprot, flags);
		flags &= VMCMD_RELATIVE;
	}
	if (psize < *size) {
		NEW_VMCMD2(vcset, vmcmd_map_readvn, *size - psize,
		    *addr + psize, vp, offset + psize, vmprot, flags);
	}

	/*
	 * Check if we need to extend the size of the segment (does
	 * bss extend page the next page boundary)?
	 */
	rm = round_page(*addr + msize);
	rf = round_page(*addr + *size);

	if (rm != rf) {
		NEW_VMCMD2(vcset, vmcmd_map_zero, rm - rf, rf, NULLVP,
		    0, vmprot, flags & VMCMD_RELATIVE);
		*size = msize;
	}
}

/*
 * elf_load_interp():
 *
 * Load an interpreter pointed to by path.
 */
static int
elf_load_interp(struct lwp *l, struct exec_package *epp, char *path,
    struct exec_vmcmd_set *vcset, u_long *entryoff, Elf_Addr *last)
{
	int error, i;
	struct vnode *vp;
	struct vattr attr;
	Elf_Ehdr eh;
	Elf_Phdr *ph = NULL;
	const Elf_Phdr *base_ph;
	const Elf_Phdr *last_ph;
	u_long phsize;
	Elf_Addr addr = *last;
	struct proc *p;
	bool use_topdown;

	p = l->l_proc;

	KASSERT(p->p_vmspace);
	if (__predict_true(p->p_vmspace != proc0.p_vmspace)) {
		use_topdown = p->p_vmspace->vm_map.flags & VM_MAP_TOPDOWN;
	} else {
#ifdef __USE_TOPDOWN_VM
		use_topdown = epp->ep_flags & EXEC_TOPDOWN_VM;
#else
		use_topdown = false;
#endif
	}

	/*
	 * 1. open file
	 * 2. read filehdr
	 * 3. map text, data, and bss out of it using VM_*
	 */
	vp = epp->ep_interp;
	if (vp == NULL) {
		error = emul_find_interp(l, epp, path);
		if (error != 0)
			return error;
		vp = epp->ep_interp;
	}
	/* We'll tidy this ourselves - otherwise we have locking issues */
	epp->ep_interp = NULL;
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);

	/*
	 * Similarly, if it's not marked as executable, or it's not a regular
	 * file, we don't allow it to be used.
	 */
	if (vp->v_type != VREG) {
		error = EACCES;
		goto badunlock;
	}
	if ((error = VOP_ACCESS(vp, VEXEC, l->l_cred)) != 0)
		goto badunlock;

	/* get attributes */
	if ((error = VOP_GETATTR(vp, &attr, l->l_cred)) != 0)
		goto badunlock;

	/*
	 * Check mount point.  Though we're not trying to exec this binary,
	 * we will be executing code from it, so if the mount point
	 * disallows execution or set-id-ness, we punt or kill the set-id.
	 */
	if (vp->v_mount->mnt_flag & MNT_NOEXEC) {
		error = EACCES;
		goto badunlock;
	}
	if (vp->v_mount->mnt_flag & MNT_NOSUID)
		epp->ep_vap->va_mode &= ~(S_ISUID | S_ISGID);

	error = vn_marktext(vp);
	if (error)
		goto badunlock;

	VOP_UNLOCK(vp);

	if ((error = exec_read_from(l, vp, 0, &eh, sizeof(eh))) != 0)
		goto bad;

	if ((error = elf_check_header(&eh)) != 0)
		goto bad;
	if (eh.e_type != ET_DYN || eh.e_phnum == 0) {
		error = ENOEXEC;
		goto bad;
	}

	phsize = eh.e_phnum * sizeof(Elf_Phdr);
	ph = kmem_alloc(phsize, KM_SLEEP);

	if ((error = exec_read_from(l, vp, eh.e_phoff, ph, phsize)) != 0)
		goto bad;

#ifdef ELF_INTERP_NON_RELOCATABLE
	/*
	 * Evil hack:  Only MIPS should be non-relocatable, and the
	 * psections should have a high address (typically 0x5ffe0000).
	 * If it's now relocatable, it should be linked at 0 and the
	 * psections should have zeros in the upper part of the address.
	 * Otherwise, force the load at the linked address.
	 */
	if (*last == ELF_LINK_ADDR && (ph->p_vaddr & 0xffff0000) == 0)
		*last = ELFDEFNNAME(NO_ADDR);
#endif

	/*
	 * If no position to load the interpreter was set by a probe
	 * function, pick the same address that a non-fixed mmap(0, ..)
	 * would (i.e. something safely out of the way).
	 */
	if (*last == ELFDEFNNAME(NO_ADDR)) {
		u_long limit = 0;
		/*
		 * Find the start and ending addresses of the psections to
		 * be loaded.  This will give us the size.
		 */
		for (i = 0, base_ph = NULL; i < eh.e_phnum; i++) {
			if (ph[i].p_type == PT_LOAD) {
				u_long psize = ph[i].p_vaddr + ph[i].p_memsz;
				if (base_ph == NULL)
					base_ph = &ph[i];
				if (psize > limit)
					limit = psize;
			}
		}

		if (base_ph == NULL) {
			error = ENOEXEC;
			goto bad;
		}

		/*
		 * Now compute the size and load address.
		 */
		addr = (*epp->ep_esch->es_emul->e_vm_default_addr)(p,
		    epp->ep_daddr,
		    round_page(limit) - trunc_page(base_ph->p_vaddr));
	} else
		addr = *last; /* may be ELF_LINK_ADDR */

	/*
	 * Load all the necessary sections
	 */
	for (i = 0, base_ph = NULL, last_ph = NULL; i < eh.e_phnum; i++) {
		switch (ph[i].p_type) {
		case PT_LOAD: {
			u_long size;
			int flags;

			if (base_ph == NULL) {
				/*
				 * First encountered psection is always the
				 * base psection.  Make sure it's aligned
				 * properly (align down for topdown and align
				 * upwards for not topdown).
				 */
				base_ph = &ph[i];
				flags = VMCMD_BASE;
				if (addr == ELF_LINK_ADDR)
					addr = ph[i].p_vaddr;
				if (use_topdown)
					addr = ELF_TRUNC(addr, ph[i].p_align);
				else
					addr = ELF_ROUND(addr, ph[i].p_align);
			} else {
				u_long limit = round_page(last_ph->p_vaddr
				    + last_ph->p_memsz);
				u_long base = trunc_page(ph[i].p_vaddr);

				/*
				 * If there is a gap in between the psections,
				 * map it as inaccessible so nothing else
				 * mmap'ed will be placed there.
				 */
				if (limit != base) {
					NEW_VMCMD2(vcset, vmcmd_map_zero,
					    base - limit,
					    limit - base_ph->p_vaddr, NULLVP,
					    0, VM_PROT_NONE, VMCMD_RELATIVE);
				}

				addr = ph[i].p_vaddr - base_ph->p_vaddr;
				flags = VMCMD_RELATIVE;
			}
			last_ph = &ph[i];
			elf_load_psection(vcset, vp, &ph[i], &addr,
			    &size, flags);
			/*
			 * If entry is within this psection then this
			 * must contain the .text section.  *entryoff is
			 * relative to the base psection.
			 */
			if (eh.e_entry >= ph[i].p_vaddr &&
			    eh.e_entry < (ph[i].p_vaddr + size)) {
				*entryoff = eh.e_entry - base_ph->p_vaddr;
			}
			addr += size;
			break;
		}

		default:
			break;
		}
	}

	kmem_free(ph, phsize);
	/*
	 * This value is ignored if TOPDOWN.
	 */
	*last = addr;
	vrele(vp);
	return 0;

badunlock:
	VOP_UNLOCK(vp);

bad:
	if (ph != NULL)
		kmem_free(ph, phsize);
	vrele(vp);
	return error;
}

/*
 * exec_elf_makecmds(): Prepare an Elf binary's exec package
 *
 * First, set of the various offsets/lengths in the exec package.
 *
 * Then, mark the text image busy (so it can be demand paged) or error
 * out if this is not possible.  Finally, set up vmcmds for the
 * text, data, bss, and stack segments.
 */
int
exec_elf_makecmds(struct lwp *l, struct exec_package *epp)
{
	Elf_Ehdr *eh = epp->ep_hdr;
	Elf_Phdr *ph, *pp;
	Elf_Addr phdr = 0, computed_phdr = 0, pos = 0, end_text = 0;
	int error, i;
	char *interp = NULL;
	u_long phsize;
	struct elf_args *ap;
	bool is_dyn = false;

	if (epp->ep_hdrvalid < sizeof(Elf_Ehdr))
		return ENOEXEC;
	if ((error = elf_check_header(eh)) != 0)
		return error;

	if (eh->e_type == ET_DYN)
		/* PIE, and some libs have an entry point */
		is_dyn = true;
	else if (eh->e_type != ET_EXEC)
		return ENOEXEC;

	if (eh->e_phnum == 0)
		return ENOEXEC;

	error = vn_marktext(epp->ep_vp);
	if (error)
		return error;

	/*
	 * Allocate space to hold all the program headers, and read them
	 * from the file
	 */
	phsize = eh->e_phnum * sizeof(Elf_Phdr);
	ph = kmem_alloc(phsize, KM_SLEEP);

	if ((error = exec_read_from(l, epp->ep_vp, eh->e_phoff, ph, phsize)) !=
	    0)
		goto bad;

	epp->ep_taddr = epp->ep_tsize = ELFDEFNNAME(NO_ADDR);
	epp->ep_daddr = epp->ep_dsize = ELFDEFNNAME(NO_ADDR);

	for (i = 0; i < eh->e_phnum; i++) {
		pp = &ph[i];
		if (pp->p_type == PT_INTERP) {
			if (pp->p_filesz < 2 || pp->p_filesz > MAXPATHLEN) {
				error = ENOEXEC;
				goto bad;
			}
			interp = PNBUF_GET();
			if ((error = exec_read_from(l, epp->ep_vp,
			    pp->p_offset, interp, pp->p_filesz)) != 0)
				goto bad;
			/* Ensure interp is NUL-terminated and of the expected length */
			if (strnlen(interp, pp->p_filesz) != pp->p_filesz - 1) {
				error = ENOEXEC;
				goto bad;
			}
			break;
		}
	}

	/*
	 * On the same architecture, we may be emulating different systems.
	 * See which one will accept this executable.
	 *
	 * Probe functions would normally see if the interpreter (if any)
	 * exists. Emulation packages may possibly replace the interpreter in
	 * interp with a changed path (/emul/xxx/<path>).
	 */
	pos = ELFDEFNNAME(NO_ADDR);
	if (epp->ep_esch->u.elf_probe_func) {
		vaddr_t startp = (vaddr_t)pos;

		error = (*epp->ep_esch->u.elf_probe_func)(l, epp, eh, interp,
							  &startp);
		if (error)
			goto bad;
		pos = (Elf_Addr)startp;
	}

	if (is_dyn)
		elf_placedynexec(epp, eh, ph);

	/*
	 * Load all the necessary sections
	 */
	for (i = 0; i < eh->e_phnum; i++) {
		Elf_Addr addr = ELFDEFNNAME(NO_ADDR);
		u_long size = 0;

		switch (ph[i].p_type) {
		case PT_LOAD:
			elf_load_psection(&epp->ep_vmcmds, epp->ep_vp,
			    &ph[i], &addr, &size, VMCMD_FIXED);

			/*
			 * Consider this as text segment, if it is executable.
			 * If there is more than one text segment, pick the
			 * largest.
			 */
			if (ph[i].p_flags & PF_X) {
				if (epp->ep_taddr == ELFDEFNNAME(NO_ADDR) ||
				    size > epp->ep_tsize) {
					epp->ep_taddr = addr;
					epp->ep_tsize = size;
				}
				end_text = addr + size;
			} else {
				epp->ep_daddr = addr;
				epp->ep_dsize = size;
			}
			if (ph[i].p_offset == 0) {
				computed_phdr = ph[i].p_vaddr + eh->e_phoff;
			}
			break;

		case PT_SHLIB:
			/* SCO has these sections. */
		case PT_INTERP:
			/* Already did this one. */
		case PT_DYNAMIC:
		case PT_NOTE:
			break;
		case PT_PHDR:
			/* Note address of program headers (in text segment) */
			phdr = ph[i].p_vaddr;
			break;

		default:
			/*
			 * Not fatal; we don't need to understand everything.
			 */
			break;
		}
	}

	if (epp->ep_vmcmds.evs_used == 0) {
		/* No VMCMD; there was no PT_LOAD section, or those
		 * sections were empty */
		error = ENOEXEC;
		goto bad;
	}

	if (epp->ep_daddr == ELFDEFNNAME(NO_ADDR)) {
		epp->ep_daddr = round_page(end_text);
		epp->ep_dsize = 0;
	}

	/*
	 * Check if we found a dynamically linked binary and arrange to load
	 * its interpreter
	 */
	if (interp) {
		u_int nused = epp->ep_vmcmds.evs_used;
		u_long interp_offset = 0;

		if ((error = elf_load_interp(l, epp, interp,
		    &epp->ep_vmcmds, &interp_offset, &pos)) != 0) {
			goto bad;
		}
		if (epp->ep_vmcmds.evs_used == nused) {
			/* elf_load_interp() has not set up any new VMCMD */
			error = ENOEXEC;
			goto bad;
		}

		ap = kmem_alloc(sizeof(*ap), KM_SLEEP);
		ap->arg_interp = epp->ep_vmcmds.evs_cmds[nused].ev_addr;
		epp->ep_entryoffset = interp_offset;
		epp->ep_entry = ap->arg_interp + interp_offset;
		PNBUF_PUT(interp);
	} else {
		epp->ep_entry = eh->e_entry;
		if (epp->ep_flags & EXEC_FORCEAUX) {
			ap = kmem_alloc(sizeof(*ap), KM_SLEEP);
			ap->arg_interp = (vaddr_t)NULL;
		} else
			ap = NULL;
	}

	if (ap) {
		ap->arg_phaddr = phdr ? phdr : computed_phdr;
		ap->arg_phentsize = eh->e_phentsize;
		ap->arg_phnum = eh->e_phnum;
		ap->arg_entry = eh->e_entry;
		epp->ep_emul_arg = ap;
		epp->ep_emul_arg_free = elf_free_emul_arg;
	}

#ifdef ELF_MAP_PAGE_ZERO
	/* Dell SVR4 maps page zero, yeuch! */
	NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_readvn, PAGE_SIZE, 0,
	    epp->ep_vp, 0, VM_PROT_READ);
#endif
	kmem_free(ph, phsize);
	return (*epp->ep_esch->es_setup_stack)(l, epp);

bad:
	if (interp)
		PNBUF_PUT(interp);
	exec_free_emul_arg(epp);
	kmem_free(ph, phsize);
	kill_vmcmds(&epp->ep_vmcmds);
	return error;
}

int
netbsd_elf_signature(struct lwp *l, struct exec_package *epp,
    Elf_Ehdr *eh)
{
	size_t i;
	Elf_Shdr *sh;
	Elf_Nhdr *np;
	size_t shsize, nsize;
	int error;
	int isnetbsd = 0;
	char *ndata, *ndesc;
	
#ifdef DIAGNOSTIC
	const char *badnote;
#define BADNOTE(n) badnote = (n)
#else
#define BADNOTE(n)
#endif

	epp->ep_pax_flags = 0;
	if (eh->e_shnum > ELF_MAXSHNUM || eh->e_shnum == 0)
		return ENOEXEC;

	shsize = eh->e_shnum * sizeof(Elf_Shdr);
	sh = kmem_alloc(shsize, KM_SLEEP);
	error = exec_read_from(l, epp->ep_vp, eh->e_shoff, sh, shsize);
	if (error)
		goto out;

	np = kmem_alloc(ELF_MAXNOTESIZE, KM_SLEEP);
	for (i = 0; i < eh->e_shnum; i++) {
		Elf_Shdr *shp = &sh[i];

		if (shp->sh_type != SHT_NOTE ||
		    shp->sh_size > ELF_MAXNOTESIZE ||
		    shp->sh_size < sizeof(Elf_Nhdr) + ELF_NOTE_NETBSD_NAMESZ)
			continue;

		error = exec_read_from(l, epp->ep_vp, shp->sh_offset, np,
		    shp->sh_size);
		if (error)
			continue;

		/* Point to the note, skip the header */
		ndata = (char *)(np + 1);

		/*
		 * Padding is present if necessary to ensure 4-byte alignment.
		 * The actual section size is therefore:
		 *    header size + 4-byte aligned name + 4-byte aligned desc
		 * Ensure this size is consistent with what is indicated
		 * in sh_size. The first check avoids integer overflows.
		 *
		 * Binaries from before NetBSD 1.6 have two notes in the same
		 * note section.  The second note was never used, so as long as
		 * the section is at least as big as it should be, it's ok.
		 * These binaries also have a second note section with a note of
		 * type ELF_NOTE_TYPE_NETBSD_TAG, which can be ignored as well.
		 */
		if (np->n_namesz > shp->sh_size || np->n_descsz > shp->sh_size) {
			BADNOTE("note size limit");
			goto bad;
		}
		nsize = sizeof(*np) + roundup(np->n_namesz, 4) +
		    roundup(np->n_descsz, 4);
		if (nsize > shp->sh_size) {
			BADNOTE("note size");
			goto bad;
		}
		ndesc = ndata + roundup(np->n_namesz, 4);

		switch (np->n_type) {
		case ELF_NOTE_TYPE_NETBSD_TAG:
			/* It is us */
			if (np->n_namesz == ELF_NOTE_NETBSD_NAMESZ &&
			    np->n_descsz == ELF_NOTE_NETBSD_DESCSZ &&
			    memcmp(ndata, ELF_NOTE_NETBSD_NAME,
			    ELF_NOTE_NETBSD_NAMESZ) == 0) {
				memcpy(&epp->ep_osversion, ndesc,
				    ELF_NOTE_NETBSD_DESCSZ);
				isnetbsd = 1;
				break;
			}

			/*
			 * Ignore SuSE tags; SuSE's n_type is the same as NetBSD's
			 * one.
			 */
			if (np->n_namesz == ELF_NOTE_SUSE_NAMESZ &&
			    memcmp(ndata, ELF_NOTE_SUSE_NAME,
			    ELF_NOTE_SUSE_NAMESZ) == 0)
				break;
			BADNOTE("NetBSD tag");
			goto bad;

		case ELF_NOTE_TYPE_PAX_TAG:
			if (np->n_namesz == ELF_NOTE_PAX_NAMESZ &&
			    np->n_descsz == ELF_NOTE_PAX_DESCSZ &&
			    memcmp(ndata, ELF_NOTE_PAX_NAME,
			    ELF_NOTE_PAX_NAMESZ) == 0) {
				uint32_t flags;
				memcpy(&flags, ndesc, sizeof(flags));
#if defined(PAX_MPROTECT) || defined(PAX_SEGVGUARD) || defined(PAX_ASLR)
				/* Convert the flags and insert them into
				 * the exec package. */
				pax_setup_elf_flags(epp, flags);
#else
				(void)flags; /* UNUSED */
#endif /* PAX_MPROTECT || PAX_SEGVGUARD || PAX_ASLR */
				break;
			}
			BADNOTE("PaX tag");
			goto bad;

		case ELF_NOTE_TYPE_MARCH_TAG:
			/* Copy the machine arch into the package. */
			if (np->n_namesz == ELF_NOTE_MARCH_NAMESZ
			    && memcmp(ndata, ELF_NOTE_MARCH_NAME,
				    ELF_NOTE_MARCH_NAMESZ) == 0) {
				/* Do not truncate the buffer */
				if (np->n_descsz > sizeof(epp->ep_machine_arch)) {
					BADNOTE("description size limit");
					goto bad;
				}
				/*
				 * Ensure ndesc is NUL-terminated and of the
				 * expected length.
				 */
				if (strnlen(ndesc, np->n_descsz) + 1 !=
				    np->n_descsz) {
					BADNOTE("description size");
					goto bad;
				}
				strlcpy(epp->ep_machine_arch, ndesc,
				    sizeof(epp->ep_machine_arch));
				break;
			}
			BADNOTE("march tag");
			goto bad;

		case ELF_NOTE_TYPE_MCMODEL_TAG:
			/* arch specific check for code model */
#ifdef ELF_MD_MCMODEL_CHECK
			if (np->n_namesz == ELF_NOTE_MCMODEL_NAMESZ
			    && memcmp(ndata, ELF_NOTE_MCMODEL_NAME,
				    ELF_NOTE_MCMODEL_NAMESZ) == 0) {
				ELF_MD_MCMODEL_CHECK(epp, ndesc, np->n_descsz);
				break;
			}
			BADNOTE("mcmodel tag");
			goto bad;
#endif
			break;

		case ELF_NOTE_TYPE_SUSE_VERSION_TAG:
			break;

		default:
			BADNOTE("unknown tag");
bad:
#ifdef DIAGNOSTIC
			/* Ignore GNU tags */
			if (np->n_namesz == ELF_NOTE_GNU_NAMESZ &&
			    memcmp(ndata, ELF_NOTE_GNU_NAME,
			    ELF_NOTE_GNU_NAMESZ) == 0)
			    break;

			int ns = MIN(np->n_namesz, shp->sh_size - sizeof(*np));
			printf("%s: Unknown elf note type %d (%s): "
			    "[namesz=%d, descsz=%d name=%-*.*s]\n",
			    epp->ep_kname, np->n_type, badnote, np->n_namesz,
			    np->n_descsz, ns, ns, ndata);
#endif
			break;
		}
	}
	kmem_free(np, ELF_MAXNOTESIZE);

	error = isnetbsd ? 0 : ENOEXEC;
out:
	kmem_free(sh, shsize);
	return error;
}

int
netbsd_elf_probe(struct lwp *l, struct exec_package *epp, void *eh, char *itp,
    vaddr_t *pos)
{
	int error;

	if ((error = netbsd_elf_signature(l, epp, eh)) != 0)
		return error;
#ifdef ELF_MD_PROBE_FUNC
	if ((error = ELF_MD_PROBE_FUNC(l, epp, eh, itp, pos)) != 0)
		return error;
#elif defined(ELF_INTERP_NON_RELOCATABLE)
	*pos = ELF_LINK_ADDR;
#endif
	epp->ep_flags |= EXEC_FORCEAUX;
	return 0;
}

void
elf_free_emul_arg(void *arg)
{
	struct elf_args *ap = arg;
	KASSERT(ap != NULL);
	kmem_free(ap, sizeof(*ap));
}
