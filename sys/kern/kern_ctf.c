/*	$NetBSD: kern_ctf.c,v 1.5 2014/10/18 08:33:29 snj Exp $	*/
/*-
 * Copyright (c) 2008 John Birrell <jb@freebsd.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/kern/kern_ctf.c,v 1.1.4.1 2009/08/03 08:13:06 kensmith Exp $
 */

#define ELFSIZE ARCH_ELFSIZE
#include <sys/module.h>
#include <sys/exec.h>
#include <sys/exec_elf.h>
#include <sys/kmem.h>
#include <sys/malloc.h>
#include <sys/kobj_impl.h>
#include <sys/kobj.h>
#include <sys/kern_ctf.h>

#define _KSYMS_PRIVATE
#include <sys/ksyms.h>

#include <net/zlib.h>

/*
 * Note this file is included by both link_elf.c and link_elf_obj.c.
 *
 * The CTF header structure definition can't be used here because it's
 * (annoyingly) covered by the CDDL. We will just use a few bytes from
 * it as an integer array where we 'know' what they mean.
 */
#define CTF_HDR_SIZE		36
#define CTF_HDR_STRTAB_U32	7
#define CTF_HDR_STRLEN_U32	8

static void *
z_alloc(void *nil, u_int items, u_int size)
{
	void *ptr;

	ptr = malloc(items * size, M_TEMP, M_NOWAIT);
	return ptr;
}

static void
z_free(void *nil, void *ptr)
{
	free(ptr, M_TEMP);
}

int
mod_ctf_get(struct module *mod, mod_ctf_t *mc)
{
	mod_ctf_t *cmc;
	struct ksyms_symtab *st; 
	void * ctftab = NULL;
	size_t sz;
	int error = 0;
	int compressed = 0;

	void *ctfbuf = NULL;
	uint8_t *ctfaddr;
	size_t ctfsize;

	if (mc == NULL) {
		return EINVAL;
	}

	/* Set the defaults for no CTF present. That's not a crime! */
	memset(mc, 0, sizeof(*mc));

	/* cached mc? */
	if (mod->mod_ctf != NULL) {
		cmc = mod->mod_ctf;
		*mc = *cmc;
		return (0);
	}

	st = ksyms_get_mod(mod->mod_info->mi_name);

	if (st != NULL) {
		mc->nmap     = st->sd_nmap;
		mc->nmapsize = st->sd_nmapsize;
	}

	if (mod->mod_kobj == NULL) {
	    	/* no kobj entry, try building from ksyms list */
		if (st == NULL) {
			return ENOENT;
		}

		ctfaddr = st->sd_ctfstart;
		ctfsize = st->sd_ctfsize;

		mc->symtab = st->sd_symstart;
		mc->strtab = st->sd_strstart;
		mc->strcnt = 0;		/* XXX TBD */
		mc->nsym   = st->sd_symsize / sizeof(Elf_Sym);
	} else {
		if (kobj_find_section(mod->mod_kobj, ".SUNW_ctf", (void **)&ctfaddr, &ctfsize)) {
			return ENOENT;
		}

		mc->symtab = mod->mod_kobj->ko_symtab;
		mc->strtab = mod->mod_kobj->ko_strtab;
		mc->strcnt = 0;		/* XXX TBD */
		mc->nsym   = mod->mod_kobj->ko_symcnt;
	}

	if (ctfaddr == NULL) {
	    	error = ENOENT;
		goto out;
	}

	/* Check the CTF magic number. (XXX check for big endian!) */
	if (ctfaddr[0] != 0xf1 || ctfaddr[1] != 0xcf) {
	    	error = EINVAL;
		goto out;
	}

	/* Check if version 2. */
	if (ctfaddr[2] != 2) {
	    	error = EINVAL;
		goto out;
	}

	/* Check if the data is compressed. */
	if ((ctfaddr[3] & 0x1) != 0) {
		uint32_t *u32 = (uint32_t *) ctfaddr;

		/*
		 * The last two fields in the CTF header are the offset
		 * from the end of the header to the start of the string
		 * data and the length of that string data. se this
		 * information to determine the decompressed CTF data
		 * buffer required.
		 */
		sz = u32[CTF_HDR_STRTAB_U32] + u32[CTF_HDR_STRLEN_U32] +
		    CTF_HDR_SIZE;

		compressed = 1;
	} else {
		/*
		 * The CTF data is not compressed, so the ELF section
		 * size is the same as the buffer size required.
		 */
		sz = ctfsize;
	}

	/*
	 * Allocate memory to buffer the CTF data in its decompressed
	 * form.
	 */
	if (compressed) {
		if ((ctfbuf = malloc(sz, M_TEMP, M_WAITOK)) == NULL) {
			error = ENOMEM;
			goto out;
		}
		ctftab = ctfbuf;
		mc->ctfalloc = 1;
	} else {
		ctftab = (void *)ctfaddr;
	}

	/* Check if decompression is required. */
	if (compressed) {
		z_stream zs;
		int ret;

		/*
		 * The header isn't compressed, so copy that into the
		 * CTF buffer first.
		 */
		memcpy(ctftab, ctfaddr, CTF_HDR_SIZE);

		/* Initialise the zlib structure. */
		memset(&zs, 0, sizeof(zs));
		zs.zalloc = z_alloc;
		zs.zfree = z_free;

		if (inflateInit2(&zs, MAX_WBITS) != Z_OK) {
			error = EIO;
			goto out;
		}

		zs.avail_in = ctfsize - CTF_HDR_SIZE;
		zs.next_in = ctfaddr + CTF_HDR_SIZE;
		zs.avail_out = sz - CTF_HDR_SIZE;
		zs.next_out = ((uint8_t *) ctftab) + CTF_HDR_SIZE;
		inflateReset(&zs);
		if ((ret = inflate(&zs, Z_FINISH)) != Z_STREAM_END) {
			printf("%s(%d): zlib inflate returned %d\n", __func__, __LINE__, ret);
			error = EIO;
			goto out;
		}
	}

	/* Got the CTF data! */
	mc->ctftab = ctftab;
	mc->ctfcnt = ctfsize;

	/* cache it */
	cmc = kmem_alloc(sizeof(mod_ctf_t), KM_SLEEP);

	*cmc = *mc;
	mod->mod_ctf = cmc;

	/* We'll retain the memory allocated for the CTF data. */
	ctfbuf = NULL;

out:
	if (ctfbuf != NULL)
		free(ctfbuf, M_TEMP);

	return (error);
}
