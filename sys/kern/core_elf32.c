/*	$NetBSD: core_elf32.c,v 1.45 2014/04/02 17:19:49 matt Exp $	*/

/*
 * Copyright (c) 2001 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
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
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * core_elf32.c/core_elf64.c: Support for the Elf32/Elf64 core file format.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(1, "$NetBSD: core_elf32.c,v 1.45 2014/04/02 17:19:49 matt Exp $");

#ifdef _KERNEL_OPT
#include "opt_coredump.h"
#endif

#ifndef ELFSIZE
#define	ELFSIZE		32
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/exec.h>
#include <sys/exec_elf.h>
#include <sys/ptrace.h>
#include <sys/kmem.h>
#include <sys/kauth.h>

#include <machine/reg.h>

#include <uvm/uvm_extern.h>

#ifdef COREDUMP

struct writesegs_state {
	Elf_Phdr *psections;
	proc_t   *p;
	off_t	 secoff;
	size_t   npsections;
};

/*
 * We need to know how big the 'notes' are before we write the main header.
 * To avoid problems with double-processing we save the data.
 */
struct note_buf {
	struct note_buf  *nb_next;
	unsigned char    nb_data[4096 - sizeof (void *)];
};

struct note_state {
	struct note_buf  *ns_first;
	struct note_buf  *ns_last;
	unsigned int     ns_count;       /* Of full buffers */
	unsigned int     ns_offset;      /* Write point in last buffer */
};

static int	ELFNAMEEND(coredump_getseghdrs)(struct uvm_coredump_state *);

static int	ELFNAMEEND(coredump_notes)(struct lwp *, struct note_state *);
static int	ELFNAMEEND(coredump_note)(struct lwp *, struct note_state *);

/* The 'note' section names and data are always 4-byte aligned. */
#define	ELFROUNDSIZE	4	/* XXX Should it be sizeof(Elf_Word)? */

#define elf_process_read_regs	CONCAT(process_read_regs, ELFSIZE)
#define elf_process_read_fpregs	CONCAT(process_read_fpregs, ELFSIZE)
#define elf_reg			CONCAT(process_reg, ELFSIZE)
#define elf_fpreg		CONCAT(process_fpreg, ELFSIZE)

int
ELFNAMEEND(coredump)(struct lwp *l, struct coredump_iostate *cookie)
{
	Elf_Ehdr ehdr;
	Elf_Shdr shdr;
	Elf_Phdr *psections;
	size_t psectionssize;
	int npsections;
	struct writesegs_state ws;
	off_t notestart;
	size_t notesize;
	int error, i;

	struct note_state ns;
	struct note_buf *nb;

	psections = NULL;

	/* Get all of the notes (mostly all the registers). */
	ns.ns_first = kmem_alloc(sizeof *ns.ns_first, KM_SLEEP);
	ns.ns_last = ns.ns_first;
	ns.ns_count = 0;
	ns.ns_offset = 0;
	error = ELFNAMEEND(coredump_notes)(l, &ns);
	ns.ns_last->nb_next = NULL;
	if (error)
		goto out;
	notesize = ns.ns_count * sizeof nb->nb_data + ns.ns_offset;

	/*
	 * We have to make a total of 3 passes across the map:
	 *
	 *	1. Count the number of map entries (the number of
	 *	   PT_LOAD sections in the dump).
	 *
	 *	2. Write the P-section headers.
	 *
	 *	3. Write the P-sections.
	 */

	/* Pass 1: count the entries. */
	npsections = uvm_coredump_count_segs(l->l_proc);
	/* Allow for the PT_NOTE section. */
	npsections++;

	/* Build the main elf header */
	memset(&ehdr.e_ident[EI_PAD], 0, sizeof(ehdr.e_ident) - EI_PAD);
	memcpy(ehdr.e_ident, ELFMAG, SELFMAG);
#if ELFSIZE == 32
	ehdr.e_ident[EI_CLASS] = ELFCLASS32;
#elif ELFSIZE == 64
	ehdr.e_ident[EI_CLASS] = ELFCLASS64;
#endif
	ehdr.e_ident[EI_DATA] = ELFDEFNNAME(MACHDEP_ENDIANNESS);
	ehdr.e_ident[EI_VERSION] = EV_CURRENT;
	/* XXX Should be the OSABI/ABI version of the executable. */
	ehdr.e_ident[EI_OSABI] = ELFOSABI_SYSV;
	ehdr.e_ident[EI_ABIVERSION] = 0;

	ehdr.e_type = ET_CORE;
	/* XXX This should be the e_machine of the executable. */
	ehdr.e_machine = ELFDEFNNAME(MACHDEP_ID);
	ehdr.e_version = EV_CURRENT;
	ehdr.e_entry = 0;
	ehdr.e_flags = 0;
	ehdr.e_ehsize = sizeof(ehdr);
	ehdr.e_phentsize = sizeof(Elf_Phdr);
	if (npsections < PN_XNUM) {
		ehdr.e_phnum = npsections;
		ehdr.e_shentsize = 0;
		ehdr.e_shnum = 0;
		ehdr.e_shoff = 0;
		ehdr.e_phoff = sizeof(ehdr);
	} else {
		ehdr.e_phnum = PN_XNUM;
		ehdr.e_shentsize = sizeof(Elf_Shdr);
		ehdr.e_shnum = 1;
		ehdr.e_shoff = sizeof(ehdr);
		ehdr.e_phoff = sizeof(ehdr) + sizeof(shdr);
	}
	ehdr.e_shstrndx = 0;

#ifdef ELF_MD_COREDUMP_SETUP
	ELF_MD_COREDUMP_SETUP(l, &ehdr);
#endif

	/* Write out the ELF header. */
	error = coredump_write(cookie, UIO_SYSSPACE, &ehdr, sizeof(ehdr));
	if (error)
		goto out;

	/* Write out sections, if needed */
	if (npsections >= PN_XNUM) {
		memset(&shdr, 0, sizeof(shdr));
		shdr.sh_type = SHT_NULL;
		shdr.sh_info = npsections;
		error = coredump_write(cookie, UIO_SYSSPACE, &shdr,
		    sizeof(shdr));
		if (error)
			goto out;
	}

	psectionssize = npsections * sizeof(*psections);
	notestart = ehdr.e_phoff + psectionssize;

	psections = kmem_zalloc(psectionssize, KM_SLEEP);

	/* Pass 2: now find the P-section headers. */
	ws.secoff = notestart + notesize;
	ws.psections = psections;
	ws.npsections = npsections - 1;
	ws.p = l->l_proc;
	error = uvm_coredump_walkmap(l->l_proc, ELFNAMEEND(coredump_getseghdrs),
	    &ws);
	if (error)
		goto out;
	if (ws.npsections != 0) {
		/* A section went away */
		error = ENOMEM;
		goto out;
	}

	/* Add the PT_NOTE header after the P-section headers. */
	ws.psections->p_type = PT_NOTE;
	ws.psections->p_offset = notestart;
	ws.psections->p_vaddr = 0;
	ws.psections->p_paddr = 0;
	ws.psections->p_filesz = notesize;
	ws.psections->p_memsz = 0;
	ws.psections->p_flags = PF_R;
	ws.psections->p_align = ELFROUNDSIZE;

	/* Write the P-section headers followed by the PT_NOTE header */
	error = coredump_write(cookie, UIO_SYSSPACE, psections, psectionssize);
	if (error)
		goto out;

#ifdef DIAGNOSTIC
	if (coredump_offset(cookie) != notestart)
		panic("coredump: offset %lld != notestart %lld",
		    (long long) coredump_offset(cookie),
		    (long long) notestart);
#endif

	/* Write out the notes. */
	for (nb = ns.ns_first; nb != NULL; nb = nb->nb_next) {
		error = coredump_write(cookie, UIO_SYSSPACE, nb->nb_data,
		    nb->nb_next == NULL ? ns.ns_offset : sizeof nb->nb_data);
		if (error)
			goto out;
	}

	/* Finally, write the sections themselves. */
	for (i = 0; i < npsections - 1; i++) {
		if (psections[i].p_filesz == 0)
			continue;

#ifdef DIAGNOSTIC
		if (coredump_offset(cookie) != psections[i].p_offset)
			panic("coredump: offset %lld != p_offset[%d] %lld",
			    (long long) coredump_offset(cookie), i,
			    (long long) psections[i].p_filesz);
#endif

		error = coredump_write(cookie, UIO_USERSPACE,
		    (void *)(vaddr_t)psections[i].p_vaddr,
		    psections[i].p_filesz);
		if (error)
			goto out;
	}

  out:
	if (psections)
		kmem_free(psections, psectionssize);
	while ((nb = ns.ns_first) != NULL) {
		ns.ns_first = nb->nb_next;
		kmem_free(nb, sizeof *nb);
	}
	return (error);
}

static int
ELFNAMEEND(coredump_getseghdrs)(struct uvm_coredump_state *us)
{
	struct writesegs_state *ws = us->cookie;
	Elf_Phdr phdr;
	vsize_t size, realsize;
	vaddr_t end;
	int error;

	/* Don't overrun if there are more sections */
	if (ws->npsections == 0)
		return ENOMEM;
	ws->npsections--;

	size = us->end - us->start;
	realsize = us->realend - us->start;
	end = us->realend;

	/* Don't bother writing out trailing zeros */
	while (realsize > 0) {
		long buf[1024 / sizeof(long)];
		size_t slen = realsize > sizeof(buf) ? sizeof(buf) : realsize;
		const long *ep;
		int i;

		end -= slen;
		if ((error = copyin_proc(ws->p, (void *)end, buf, slen)) != 0)
			return error;

		ep = (const long *) &buf[slen / sizeof(buf[0])];
		for (i = 0, ep--; buf <= ep; ep--, i++) {
			if (*ep)
				break;
		}
		realsize -= i * sizeof(buf[0]);
		if (i * sizeof(buf[0]) < slen)
			break;
	}

	phdr.p_type = PT_LOAD;
	phdr.p_offset = ws->secoff;
	phdr.p_vaddr = us->start;
	phdr.p_paddr = 0;
	phdr.p_filesz = realsize;
	phdr.p_memsz = size;
	phdr.p_flags = 0;
	if (us->prot & VM_PROT_READ)
		phdr.p_flags |= PF_R;
	if (us->prot & VM_PROT_WRITE)
		phdr.p_flags |= PF_W;
	if (us->prot & VM_PROT_EXECUTE)
		phdr.p_flags |= PF_X;
	phdr.p_align = PAGE_SIZE;

	ws->secoff += phdr.p_filesz;
	*ws->psections++ = phdr;

	return (0);
}

static int
ELFNAMEEND(coredump_notes)(struct lwp *l, struct note_state *ns)
{
	struct proc *p;
	struct netbsd_elfcore_procinfo cpi;
	int error;
	struct lwp *l0;
	sigset_t ss1, ss2;

	p = l->l_proc;

	/* First, write an elfcore_procinfo. */
	cpi.cpi_version = NETBSD_ELFCORE_PROCINFO_VERSION;
	cpi.cpi_cpisize = sizeof(cpi);
	cpi.cpi_signo = p->p_sigctx.ps_signo;
	cpi.cpi_sigcode = p->p_sigctx.ps_code;
	cpi.cpi_siglwp = p->p_sigctx.ps_lwp;

	/*
	 * XXX This should be per-LWP.
	 */
	ss1 = p->p_sigpend.sp_set;
	sigemptyset(&ss2);
	LIST_FOREACH(l0, &p->p_lwps, l_sibling) {
		sigplusset(&l0->l_sigpend.sp_set, &ss1);
		sigplusset(&l0->l_sigmask, &ss2);
	}
	memcpy(&cpi.cpi_sigpend, &ss1, sizeof(cpi.cpi_sigpend));
	memcpy(&cpi.cpi_sigmask, &ss2, sizeof(cpi.cpi_sigmask));
	memcpy(&cpi.cpi_sigignore, &p->p_sigctx.ps_sigignore,
	    sizeof(cpi.cpi_sigignore));
	memcpy(&cpi.cpi_sigcatch, &p->p_sigctx.ps_sigcatch,
	    sizeof(cpi.cpi_sigcatch));

	cpi.cpi_pid = p->p_pid;
	mutex_enter(proc_lock);
	cpi.cpi_ppid = p->p_pptr->p_pid;
	cpi.cpi_pgrp = p->p_pgid;
	cpi.cpi_sid = p->p_session->s_sid;
	mutex_exit(proc_lock);

	cpi.cpi_ruid = kauth_cred_getuid(l->l_cred);
	cpi.cpi_euid = kauth_cred_geteuid(l->l_cred);
	cpi.cpi_svuid = kauth_cred_getsvuid(l->l_cred);

	cpi.cpi_rgid = kauth_cred_getgid(l->l_cred);
	cpi.cpi_egid = kauth_cred_getegid(l->l_cred);
	cpi.cpi_svgid = kauth_cred_getsvgid(l->l_cred);

	cpi.cpi_nlwps = p->p_nlwps;
	(void)strncpy(cpi.cpi_name, p->p_comm, sizeof(cpi.cpi_name));
	cpi.cpi_name[sizeof(cpi.cpi_name) - 1] = '\0';

	ELFNAMEEND(coredump_savenote)(ns, ELF_NOTE_NETBSD_CORE_PROCINFO,
	    ELF_NOTE_NETBSD_CORE_NAME, &cpi, sizeof(cpi));

	/* XXX Add hook for machdep per-proc notes. */

	/*
	 * Now write the register info for the thread that caused the
	 * coredump.
	 */
	error = ELFNAMEEND(coredump_note)(l, ns);
	if (error)
		return (error);

	/*
	 * Now, for each LWP, write the register info and any other
	 * per-LWP notes.
	 * Lock in case this is a gcore requested dump.
	 */
	mutex_enter(p->p_lock);
	LIST_FOREACH(l0, &p->p_lwps, l_sibling) {
		if (l0 == l)		/* we've taken care of this thread */
			continue;
		error = ELFNAMEEND(coredump_note)(l0, ns);
		if (error)
			break;
	}
	mutex_exit(p->p_lock);

	return error;
}

static int
ELFNAMEEND(coredump_note)(struct lwp *l, struct note_state *ns)
{
	int error;
	char name[64];
	elf_reg intreg;
#ifdef PT_GETFPREGS
	elf_fpreg freg;
	size_t freglen;
#endif

	snprintf(name, sizeof(name), "%s@%d",
	    ELF_NOTE_NETBSD_CORE_NAME, l->l_lid);

	error = elf_process_read_regs(l, &intreg);
	if (error)
		return (error);

	ELFNAMEEND(coredump_savenote)(ns, PT_GETREGS, name, &intreg,
	    sizeof(intreg));

#ifdef PT_GETFPREGS
	freglen = sizeof(freg);
	error = elf_process_read_fpregs(l, &freg, &freglen);
	if (error)
		return (error);

	ELFNAMEEND(coredump_savenote)(ns, PT_GETFPREGS, name, &freg, freglen);
#endif
	/* XXX Add hook for machdep per-LWP notes. */
	return (0);
}

static void
save_note_bytes(struct note_state *ns, const void *data, size_t len)
{
	struct note_buf *nb = ns->ns_last;
	size_t copylen;
	unsigned char *wp;

	/*
	 * Just copy the data into a buffer list.
	 * All but the last buffer is full.
	 */
	for (;;) {
		copylen = min(len, sizeof nb->nb_data - ns->ns_offset);
		wp = nb->nb_data + ns->ns_offset;
		memcpy(wp, data, copylen);
		if (copylen == len)
			break;
		nb->nb_next = kmem_alloc(sizeof *nb->nb_next, KM_SLEEP);
		nb = nb->nb_next;
		ns->ns_last = nb;
		ns->ns_count++;
		ns->ns_offset = 0;
		len -= copylen;
		data = (const unsigned char *)data + copylen;
	}

	while (copylen & (ELFROUNDSIZE - 1))
	    wp[copylen++] = 0;

	ns->ns_offset += copylen;
}

void
ELFNAMEEND(coredump_savenote)(struct note_state *ns, unsigned int type,
    const char *name, void *data, size_t data_len)
{
	Elf_Nhdr nhdr;

	nhdr.n_namesz = strlen(name) + 1;
	nhdr.n_descsz = data_len;
	nhdr.n_type = type;

	save_note_bytes(ns, &nhdr, sizeof (nhdr));
	save_note_bytes(ns, name, nhdr.n_namesz);
	save_note_bytes(ns, data, data_len);
}

#else	/* COREDUMP */

int
ELFNAMEEND(coredump)(struct lwp *l, void *cookie)
{

	return ENOSYS;
}

#endif	/* COREDUMP */
