/*	$NetBSD: kvm.c,v 1.101 2014/02/19 20:21:22 dsl Exp $	*/

/*-
 * Copyright (c) 1989, 1992, 1993
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
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
#if 0
static char sccsid[] = "@(#)kvm.c	8.2 (Berkeley) 2/13/94";
#else
__RCSID("$NetBSD: kvm.c,v 1.101 2014/02/19 20:21:22 dsl Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#include <sys/param.h>
#include <sys/lwp.h>
#include <sys/proc.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/sysctl.h>

#include <sys/core.h>
#include <sys/exec.h>
#include <sys/kcore.h>
#include <sys/ksyms.h>
#include <sys/types.h>

#include <uvm/uvm_extern.h>

#include <machine/cpu.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <nlist.h>
#include <paths.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <kvm.h>

#include "kvm_private.h"

static int	_kvm_get_header(kvm_t *);
static kvm_t	*_kvm_open(kvm_t *, const char *, const char *,
		    const char *, int, char *);
static int	clear_gap(kvm_t *, bool (*)(void *, const void *, size_t),
		    void *, size_t);
static off_t	Lseek(kvm_t *, int, off_t, int);
static ssize_t	Pread(kvm_t *, int, void *, size_t, off_t);

char *
kvm_geterr(kvm_t *kd)
{
	return (kd->errbuf);
}

const char *
kvm_getkernelname(kvm_t *kd)
{
	return kd->kernelname;
}

/*
 * Report an error using printf style arguments.  "program" is kd->program
 * on hard errors, and 0 on soft errors, so that under sun error emulation,
 * only hard errors are printed out (otherwise, programs like gdb will
 * generate tons of error messages when trying to access bogus pointers).
 */
void
_kvm_err(kvm_t *kd, const char *program, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	if (program != NULL) {
		(void)fprintf(stderr, "%s: ", program);
		(void)vfprintf(stderr, fmt, ap);
		(void)fputc('\n', stderr);
	} else
		(void)vsnprintf(kd->errbuf,
		    sizeof(kd->errbuf), fmt, ap);

	va_end(ap);
}

void
_kvm_syserr(kvm_t *kd, const char *program, const char *fmt, ...)
{
	va_list ap;
	size_t n;

	va_start(ap, fmt);
	if (program != NULL) {
		(void)fprintf(stderr, "%s: ", program);
		(void)vfprintf(stderr, fmt, ap);
		(void)fprintf(stderr, ": %s\n", strerror(errno));
	} else {
		char *cp = kd->errbuf;

		(void)vsnprintf(cp, sizeof(kd->errbuf), fmt, ap);
		n = strlen(cp);
		(void)snprintf(&cp[n], sizeof(kd->errbuf) - n, ": %s",
		    strerror(errno));
	}
	va_end(ap);
}

void *
_kvm_malloc(kvm_t *kd, size_t n)
{
	void *p;

	if ((p = malloc(n)) == NULL)
		_kvm_err(kd, kd->program, "%s", strerror(errno));
	return (p);
}

/*
 * Wrapper around the lseek(2) system call; calls _kvm_syserr() for us
 * in the event of emergency.
 */
static off_t
Lseek(kvm_t *kd, int fd, off_t offset, int whence)
{
	off_t off;

	errno = 0;

	if ((off = lseek(fd, offset, whence)) == -1 && errno != 0) {
		_kvm_syserr(kd, kd->program, "Lseek");
		return ((off_t)-1);
	}
	return (off);
}

ssize_t
_kvm_pread(kvm_t *kd, int fd, void *buf, size_t size, off_t off)
{
	ptrdiff_t moff;
	void *newbuf;
	size_t dsize;
	ssize_t rv;
	off_t doff;

	/* If aligned nothing to do. */
 	if (((off % kd->fdalign) | (size % kd->fdalign)) == 0) {
		return pread(fd, buf, size, off);
 	}

	/*
	 * Otherwise must buffer.  We can't tolerate short reads in this
	 * case (lazy bum).
	 */
	moff = (ptrdiff_t)off % kd->fdalign;
	doff = off - moff;
	dsize = moff + size + kd->fdalign - 1;
	dsize -= dsize % kd->fdalign;
	if (kd->iobufsz < dsize) {
		newbuf = realloc(kd->iobuf, dsize);
		if (newbuf == NULL) {
			_kvm_syserr(kd, 0, "cannot allocate I/O buffer");
			return (-1);
		}
		kd->iobuf = newbuf;
		kd->iobufsz = dsize;
	}
	rv = pread(fd, kd->iobuf, dsize, doff);
	if (rv < size + moff)
		return -1;
	memcpy(buf, kd->iobuf + moff, size);
	return size;
}

/*
 * Wrapper around the pread(2) system call; calls _kvm_syserr() for us
 * in the event of emergency.
 */
static ssize_t
Pread(kvm_t *kd, int fd, void *buf, size_t nbytes, off_t offset)
{
	ssize_t rv;

	errno = 0;

	if ((rv = _kvm_pread(kd, fd, buf, nbytes, offset)) != nbytes &&
	    errno != 0)
		_kvm_syserr(kd, kd->program, "Pread");
	return (rv);
}

static kvm_t *
_kvm_open(kvm_t *kd, const char *uf, const char *mf, const char *sf, int flag,
    char *errout)
{
	struct stat st;
	int ufgiven;

	kd->pmfd = -1;
	kd->vmfd = -1;
	kd->swfd = -1;
	kd->nlfd = -1;
	kd->alive = KVM_ALIVE_DEAD;
	kd->procbase = NULL;
	kd->procbase_len = 0;
	kd->procbase2 = NULL;
	kd->procbase2_len = 0;
	kd->lwpbase = NULL;
	kd->lwpbase_len = 0;
	kd->nbpg = getpagesize();
	kd->swapspc = NULL;
	kd->argspc = NULL;
	kd->argspc_len = 0;
	kd->argbuf = NULL;
	kd->argv = NULL;
	kd->vmst = NULL;
	kd->vm_page_buckets = NULL;
	kd->kcore_hdr = NULL;
	kd->cpu_dsize = 0;
	kd->cpu_data = NULL;
	kd->dump_off = 0;
	kd->fdalign = 1;
	kd->iobuf = NULL;
	kd->iobufsz = 0;

	if (flag & KVM_NO_FILES) {
		kd->alive = KVM_ALIVE_SYSCTL;
		return(kd);
	}

	/*
	 * Call the MD open hook.  This sets:
	 *	usrstack, min_uva, max_uva
	 */
	if (_kvm_mdopen(kd)) {
		_kvm_err(kd, kd->program, "md init failed");
		goto failed;
	}

	ufgiven = (uf != NULL);
	if (!ufgiven) {
#ifdef CPU_BOOTED_KERNEL
		/* 130 is 128 + '/' + '\0' */
		static char booted_kernel[130];
		int mib[2], rc;
		size_t len;

		mib[0] = CTL_MACHDEP;
		mib[1] = CPU_BOOTED_KERNEL;
		booted_kernel[0] = '/';
		booted_kernel[1] = '\0';
		len = sizeof(booted_kernel) - 2;
		rc = sysctl(&mib[0], 2, &booted_kernel[1], &len, NULL, 0);
		booted_kernel[sizeof(booted_kernel) - 1] = '\0';
		uf = (booted_kernel[1] == '/') ?
		    &booted_kernel[1] : &booted_kernel[0];
		if (rc != -1)
			rc = stat(uf, &st);
		if (rc != -1 && !S_ISREG(st.st_mode))
			rc = -1;
		if (rc == -1)
#endif /* CPU_BOOTED_KERNEL */
			uf = _PATH_UNIX;
	}
	else if (strlen(uf) >= MAXPATHLEN) {
		_kvm_err(kd, kd->program, "exec file name too long");
		goto failed;
	}
	if (flag & ~O_RDWR) {
		_kvm_err(kd, kd->program, "bad flags arg");
		goto failed;
	}
	if (mf == 0)
		mf = _PATH_MEM;
	if (sf == 0)
		sf = _PATH_DRUM;

	/*
	 * Open the kernel namelist.  If /dev/ksyms doesn't
	 * exist, open the current kernel.
	 */
	if (ufgiven == 0)
		kd->nlfd = open(_PATH_KSYMS, O_RDONLY | O_CLOEXEC, 0);
	if (kd->nlfd < 0) {
		if ((kd->nlfd = open(uf, O_RDONLY | O_CLOEXEC, 0)) < 0) {
			_kvm_syserr(kd, kd->program, "%s", uf);
			goto failed;
		}
		strlcpy(kd->kernelname, uf, sizeof(kd->kernelname));
	} else {
		strlcpy(kd->kernelname, _PATH_KSYMS, sizeof(kd->kernelname));
		/*
		 * We're here because /dev/ksyms was opened
		 * successfully.  However, we don't want to keep it
		 * open, so we close it now.  Later, we will open
		 * it again, since it will be the only case where
		 * kd->nlfd is negative.
		 */
		close(kd->nlfd);
		kd->nlfd = -1;
	}

	if ((kd->pmfd = open(mf, flag | O_CLOEXEC, 0)) < 0) {
		_kvm_syserr(kd, kd->program, "%s", mf);
		goto failed;
	}
	if (fstat(kd->pmfd, &st) < 0) {
		_kvm_syserr(kd, kd->program, "%s", mf);
		goto failed;
	}
	if (S_ISCHR(st.st_mode) && strcmp(mf, _PATH_MEM) == 0) {
		/*
		 * If this is /dev/mem, open kmem too.  (Maybe we should
		 * make it work for either /dev/mem or /dev/kmem -- in either
		 * case you're working with a live kernel.)
		 */
		if ((kd->vmfd = open(_PATH_KMEM, flag | O_CLOEXEC, 0)) < 0) {
			_kvm_syserr(kd, kd->program, "%s", _PATH_KMEM);
			goto failed;
		}
		kd->alive = KVM_ALIVE_FILES;
		if ((kd->swfd = open(sf, flag | O_CLOEXEC, 0)) < 0) {
			if (errno != ENXIO) {
				_kvm_syserr(kd, kd->program, "%s", sf);
				goto failed;
			}
			/* swap is not configured?  not fatal */
		}
	} else {
		kd->fdalign = DEV_BSIZE;	/* XXX */
		/*
		 * This is a crash dump.
		 * Initialize the virtual address translation machinery.
		 *
		 * If there is no valid core header, fail silently here.
		 * The address translations however will fail without
		 * header. Things can be made to run by calling
		 * kvm_dump_mkheader() before doing any translation.
		 */
		if (_kvm_get_header(kd) == 0) {
			if (_kvm_initvtop(kd) < 0)
				goto failed;
		}
	}
	return (kd);
failed:
	/*
	 * Copy out the error if doing sane error semantics.
	 */
	if (errout != 0)
		(void)strlcpy(errout, kd->errbuf, _POSIX2_LINE_MAX);
	(void)kvm_close(kd);
	return (0);
}

/*
 * The kernel dump file (from savecore) contains:
 *    kcore_hdr_t kcore_hdr;
 *    kcore_seg_t cpu_hdr;
 *    (opaque)    cpu_data; (size is cpu_hdr.c_size)
 *	  kcore_seg_t mem_hdr;
 *    (memory)    mem_data; (size is mem_hdr.c_size)
 *
 * Note: khdr is padded to khdr.c_hdrsize;
 * cpu_hdr and mem_hdr are padded to khdr.c_seghdrsize
 */
static int
_kvm_get_header(kvm_t *kd)
{
	kcore_hdr_t	kcore_hdr;
	kcore_seg_t	cpu_hdr;
	kcore_seg_t	mem_hdr;
	size_t		offset;
	ssize_t		sz;

	/*
	 * Read the kcore_hdr_t
	 */
	sz = Pread(kd, kd->pmfd, &kcore_hdr, sizeof(kcore_hdr), (off_t)0);
	if (sz != sizeof(kcore_hdr))
		return (-1);

	/*
	 * Currently, we only support dump-files made by the current
	 * architecture...
	 */
	if ((CORE_GETMAGIC(kcore_hdr) != KCORE_MAGIC) ||
	    (CORE_GETMID(kcore_hdr) != MID_MACHINE))
		return (-1);

	/*
	 * Currently, we only support exactly 2 segments: cpu-segment
	 * and data-segment in exactly that order.
	 */
	if (kcore_hdr.c_nseg != 2)
		return (-1);

	/*
	 * Save away the kcore_hdr.  All errors after this
	 * should do a to "goto fail" to deallocate things.
	 */
	kd->kcore_hdr = _kvm_malloc(kd, sizeof(kcore_hdr));
	memcpy(kd->kcore_hdr, &kcore_hdr, sizeof(kcore_hdr));
	offset = kcore_hdr.c_hdrsize;

	/*
	 * Read the CPU segment header
	 */
	sz = Pread(kd, kd->pmfd, &cpu_hdr, sizeof(cpu_hdr), (off_t)offset);
	if (sz != sizeof(cpu_hdr))
		goto fail;
	if ((CORE_GETMAGIC(cpu_hdr) != KCORESEG_MAGIC) ||
	    (CORE_GETFLAG(cpu_hdr) != CORE_CPU))
		goto fail;
	offset += kcore_hdr.c_seghdrsize;

	/*
	 * Read the CPU segment DATA.
	 */
	kd->cpu_dsize = cpu_hdr.c_size;
	kd->cpu_data = _kvm_malloc(kd, cpu_hdr.c_size);
	if (kd->cpu_data == NULL)
		goto fail;
	sz = Pread(kd, kd->pmfd, kd->cpu_data, cpu_hdr.c_size, (off_t)offset);
	if (sz != cpu_hdr.c_size)
		goto fail;
	offset += cpu_hdr.c_size;

	/*
	 * Read the next segment header: data segment
	 */
	sz = Pread(kd, kd->pmfd, &mem_hdr, sizeof(mem_hdr), (off_t)offset);
	if (sz != sizeof(mem_hdr))
		goto fail;
	offset += kcore_hdr.c_seghdrsize;

	if ((CORE_GETMAGIC(mem_hdr) != KCORESEG_MAGIC) ||
	    (CORE_GETFLAG(mem_hdr) != CORE_DATA))
		goto fail;

	kd->dump_off = offset;
	return (0);

fail:
	if (kd->kcore_hdr != NULL) {
		free(kd->kcore_hdr);
		kd->kcore_hdr = NULL;
	}
	if (kd->cpu_data != NULL) {
		free(kd->cpu_data);
		kd->cpu_data = NULL;
		kd->cpu_dsize = 0;
	}
	return (-1);
}

/*
 * The format while on the dump device is: (new format)
 *	kcore_seg_t cpu_hdr;
 *	(opaque)    cpu_data; (size is cpu_hdr.c_size)
 *	kcore_seg_t mem_hdr;
 *	(memory)    mem_data; (size is mem_hdr.c_size)
 */
int
kvm_dump_mkheader(kvm_t *kd, off_t dump_off)
{
	kcore_seg_t	cpu_hdr;
	size_t hdr_size;
	ssize_t sz;

	if (kd->kcore_hdr != NULL) {
	    _kvm_err(kd, kd->program, "already has a dump header");
	    return (-1);
	}
	if (ISALIVE(kd)) {
		_kvm_err(kd, kd->program, "don't use on live kernel");
		return (-1);
	}

	/*
	 * Validate new format crash dump
	 */
	sz = Pread(kd, kd->pmfd, &cpu_hdr, sizeof(cpu_hdr), dump_off);
	if (sz != sizeof(cpu_hdr)) {
		_kvm_err(kd, 0, "read %zx bytes at offset %"PRIx64
		    " for cpu_hdr instead of requested %zu",
		    sz, dump_off, sizeof(cpu_hdr));
		return (-1);
	}
	if ((CORE_GETMAGIC(cpu_hdr) != KCORE_MAGIC)
		|| (CORE_GETMID(cpu_hdr) != MID_MACHINE)) {
		_kvm_err(kd, 0, "invalid magic in cpu_hdr");
		return (0);
	}
	hdr_size = ALIGN(sizeof(cpu_hdr));

	/*
	 * Read the CPU segment.
	 */
	kd->cpu_dsize = cpu_hdr.c_size;
	kd->cpu_data = _kvm_malloc(kd, kd->cpu_dsize);
	if (kd->cpu_data == NULL) {
		_kvm_err(kd, kd->program, "no cpu_data");
		goto fail;
	}
	sz = Pread(kd, kd->pmfd, kd->cpu_data, cpu_hdr.c_size,
	    dump_off + hdr_size);
	if (sz != cpu_hdr.c_size) {
		_kvm_err(kd, kd->program, "size %zu != cpu_hdr.csize %"PRIu32,
		    sz, cpu_hdr.c_size);
		goto fail;
	}
	hdr_size += kd->cpu_dsize;

	/*
	 * Leave phys mem pointer at beginning of memory data
	 */
	kd->dump_off = dump_off + hdr_size;
	if (Lseek(kd, kd->pmfd, kd->dump_off, SEEK_SET) == -1) {
		_kvm_err(kd, kd->program, "failed to seek to %" PRId64,
		    (int64_t)kd->dump_off);
		goto fail;
	}

	/*
	 * Create a kcore_hdr.
	 */
	kd->kcore_hdr = _kvm_malloc(kd, sizeof(kcore_hdr_t));
	if (kd->kcore_hdr == NULL) {
		_kvm_err(kd, kd->program, "failed to allocate header");
		goto fail;
	}

	kd->kcore_hdr->c_hdrsize    = ALIGN(sizeof(kcore_hdr_t));
	kd->kcore_hdr->c_seghdrsize = ALIGN(sizeof(kcore_seg_t));
	kd->kcore_hdr->c_nseg       = 2;
	CORE_SETMAGIC(*(kd->kcore_hdr), KCORE_MAGIC, MID_MACHINE,0);

	/*
	 * Now that we have a valid header, enable translations.
	 */
	if (_kvm_initvtop(kd) == 0)
		/* Success */
		return (hdr_size);

fail:
	if (kd->kcore_hdr != NULL) {
		free(kd->kcore_hdr);
		kd->kcore_hdr = NULL;
	}
	if (kd->cpu_data != NULL) {
		free(kd->cpu_data);
		kd->cpu_data = NULL;
		kd->cpu_dsize = 0;
	}
	return (-1);
}

static int
clear_gap(kvm_t *kd, bool (*write_buf)(void *, const void *, size_t),
    void *cookie, size_t size)
{
	char buf[1024];
	size_t len;

	(void)memset(buf, 0, size > sizeof(buf) ? sizeof(buf) : size);

	while (size > 0) {
		len = size > sizeof(buf) ? sizeof(buf) : size;
		if (!(*write_buf)(cookie, buf, len)) {
			_kvm_syserr(kd, kd->program, "clear_gap");
			return -1;
		}
		size -= len;
	} 

	return 0;
}

/*
 * Write the dump header by calling write_buf with cookie as first argument.
 */
int
kvm_dump_header(kvm_t *kd, bool (*write_buf)(void *, const void *, size_t),
    void *cookie, int dumpsize)
{
	kcore_seg_t	seghdr;
	long		offset;
	size_t		gap;

	if (kd->kcore_hdr == NULL || kd->cpu_data == NULL) {
		_kvm_err(kd, kd->program, "no valid dump header(s)");
		return (-1);
	}

	/*
	 * Write the generic header
	 */
	offset = 0;
	if (!(*write_buf)(cookie, kd->kcore_hdr, sizeof(kcore_hdr_t))) {
		_kvm_syserr(kd, kd->program, "kvm_dump_header");
		return (-1);
	}
	offset += kd->kcore_hdr->c_hdrsize;
	gap     = kd->kcore_hdr->c_hdrsize - sizeof(kcore_hdr_t);
	if (clear_gap(kd, write_buf, cookie, gap) == -1)
		return (-1);

	/*
	 * Write the CPU header
	 */
	CORE_SETMAGIC(seghdr, KCORESEG_MAGIC, 0, CORE_CPU);
	seghdr.c_size = ALIGN(kd->cpu_dsize);
	if (!(*write_buf)(cookie, &seghdr, sizeof(seghdr))) {
		_kvm_syserr(kd, kd->program, "kvm_dump_header");
		return (-1);
	}
	offset += kd->kcore_hdr->c_seghdrsize;
	gap     = kd->kcore_hdr->c_seghdrsize - sizeof(seghdr);
	if (clear_gap(kd, write_buf, cookie, gap) == -1)
		return (-1);

	if (!(*write_buf)(cookie, kd->cpu_data, kd->cpu_dsize)) {
		_kvm_syserr(kd, kd->program, "kvm_dump_header");
		return (-1);
	}
	offset += seghdr.c_size;
	gap     = seghdr.c_size - kd->cpu_dsize;
	if (clear_gap(kd, write_buf, cookie, gap) == -1)
		return (-1);

	/*
	 * Write the actual dump data segment header
	 */
	CORE_SETMAGIC(seghdr, KCORESEG_MAGIC, 0, CORE_DATA);
	seghdr.c_size = dumpsize;
	if (!(*write_buf)(cookie, &seghdr, sizeof(seghdr))) {
		_kvm_syserr(kd, kd->program, "kvm_dump_header");
		return (-1);
	}
	offset += kd->kcore_hdr->c_seghdrsize;
	gap     = kd->kcore_hdr->c_seghdrsize - sizeof(seghdr);
	if (clear_gap(kd, write_buf, cookie, gap) == -1)
		return (-1);

	return (int)offset;
}

static bool
kvm_dump_header_stdio(void *cookie, const void *buf, size_t len)
{
	return fwrite(buf, len, 1, (FILE *)cookie) == 1;
}

int
kvm_dump_wrtheader(kvm_t *kd, FILE *fp, int dumpsize)
{
	return kvm_dump_header(kd, kvm_dump_header_stdio, fp, dumpsize);
}

kvm_t *
kvm_openfiles(const char *uf, const char *mf, const char *sf,
    int flag, char *errout)
{
	kvm_t *kd;

	if ((kd = malloc(sizeof(*kd))) == NULL) {
		(void)strlcpy(errout, strerror(errno), _POSIX2_LINE_MAX);
		return (0);
	}
	kd->program = 0;
	return (_kvm_open(kd, uf, mf, sf, flag, errout));
}

kvm_t *
kvm_open(const char *uf, const char *mf, const char *sf, int flag,
    const char *program)
{
	kvm_t *kd;

	if ((kd = malloc(sizeof(*kd))) == NULL) {
		(void)fprintf(stderr, "%s: %s\n",
		    program ? program : getprogname(), strerror(errno));
		return (0);
	}
	kd->program = program;
	return (_kvm_open(kd, uf, mf, sf, flag, NULL));
}

int
kvm_close(kvm_t *kd)
{
	int error = 0;

	if (kd->pmfd >= 0)
		error |= close(kd->pmfd);
	if (kd->vmfd >= 0)
		error |= close(kd->vmfd);
	if (kd->nlfd >= 0)
		error |= close(kd->nlfd);
	if (kd->swfd >= 0)
		error |= close(kd->swfd);
	if (kd->vmst)
		_kvm_freevtop(kd);
	kd->cpu_dsize = 0;
	if (kd->cpu_data != NULL)
		free(kd->cpu_data);
	if (kd->kcore_hdr != NULL)
		free(kd->kcore_hdr);
	if (kd->procbase != 0)
		free(kd->procbase);
	if (kd->procbase2 != 0)
		free(kd->procbase2);
	if (kd->lwpbase != 0)
		free(kd->lwpbase);
	if (kd->swapspc != 0)
		free(kd->swapspc);
	if (kd->argspc != 0)
		free(kd->argspc);
	if (kd->argbuf != 0)
		free(kd->argbuf);
	if (kd->argv != 0)
		free(kd->argv);
	if (kd->iobuf != 0)
		free(kd->iobuf);
	free(kd);

	return (error);
}

int
kvm_nlist(kvm_t *kd, struct nlist *nl)
{
	int rv, nlfd;

	/*
	 * kd->nlfd might be negative when we get here, and in that
	 * case that means that we're using /dev/ksyms.
	 * So open it again, just for the time we retrieve the list.
	 */
	if (kd->nlfd < 0) {
		nlfd = open(_PATH_KSYMS, O_RDONLY | O_CLOEXEC, 0);
		if (nlfd < 0) {
			_kvm_err(kd, 0, "failed to open %s", _PATH_KSYMS);
			return (nlfd);
		}
	} else
		nlfd = kd->nlfd;

	/*
	 * Call the nlist(3) routines to retrieve the given namelist.
	 */
	rv = __fdnlist(nlfd, nl);

	if (rv == -1)
		_kvm_err(kd, 0, "bad namelist");

	if (kd->nlfd < 0)
		close(nlfd);

	return (rv);
}

int
kvm_dump_inval(kvm_t *kd)
{
	struct nlist	nl[2];
	paddr_t		pa;
	size_t		dsize;
	off_t		doff;
	void		*newbuf;

	if (ISALIVE(kd)) {
		_kvm_err(kd, kd->program, "clearing dump on live kernel");
		return (-1);
	}
	nl[0].n_name = "_dumpmag";
	nl[1].n_name = NULL;

	if (kvm_nlist(kd, nl) == -1) {
		_kvm_err(kd, 0, "bad namelist");
		return (-1);
	}
	if (_kvm_kvatop(kd, (vaddr_t)nl[0].n_value, &pa) == 0)
		return (-1);

	errno = 0;
	dsize = MAX(kd->fdalign, sizeof(u_long));
	if (kd->iobufsz < dsize) {
		newbuf = realloc(kd->iobuf, dsize);
		if (newbuf == NULL) {
			_kvm_syserr(kd, 0, "cannot allocate I/O buffer");
			return (-1);
		}
		kd->iobuf = newbuf;
		kd->iobufsz = dsize;
	}
	memset(kd->iobuf, 0, dsize);
	doff = _kvm_pa2off(kd, pa);
	doff -= doff % kd->fdalign;
	if (pwrite(kd->pmfd, kd->iobuf, dsize, doff) == -1) {
		_kvm_syserr(kd, 0, "cannot invalidate dump - pwrite");
		return (-1);
	}
	return (0);
}

ssize_t
kvm_read(kvm_t *kd, u_long kva, void *buf, size_t len)
{
	int cc;
	void *cp;

	if (ISKMEM(kd)) {
		/*
		 * We're using /dev/kmem.  Just read straight from the
		 * device and let the active kernel do the address translation.
		 */
		errno = 0;
		cc = _kvm_pread(kd, kd->vmfd, buf, len, (off_t)kva);
		if (cc < 0) {
			_kvm_syserr(kd, 0, "kvm_read");
			return (-1);
		} else if (cc < len)
			_kvm_err(kd, kd->program, "short read");
		return (cc);
	} else if (ISSYSCTL(kd)) {
		_kvm_err(kd, kd->program, "kvm_open called with KVM_NO_FILES, "
		    "can't use kvm_read");
		return (-1);
	} else {
		if ((kd->kcore_hdr == NULL) || (kd->cpu_data == NULL)) {
			_kvm_err(kd, kd->program, "no valid dump header");
			return (-1);
		}
		cp = buf;
		while (len > 0) {
			paddr_t	pa;
			off_t	foff;

			cc = _kvm_kvatop(kd, (vaddr_t)kva, &pa);
			if (cc == 0)
				return (-1);
			if (cc > len)
				cc = len;
			foff = _kvm_pa2off(kd, pa);
			errno = 0;
			cc = _kvm_pread(kd, kd->pmfd, cp, (size_t)cc, foff);
			if (cc < 0) {
				_kvm_syserr(kd, kd->program, "kvm_read");
				break;
			}
			/*
			 * If kvm_kvatop returns a bogus value or our core
			 * file is truncated, we might wind up seeking beyond
			 * the end of the core file in which case the read will
			 * return 0 (EOF).
			 */
			if (cc == 0)
				break;
			cp = (char *)cp + cc;
			kva += cc;
			len -= cc;
		}
		return ((char *)cp - (char *)buf);
	}
	/* NOTREACHED */
}

ssize_t
kvm_write(kvm_t *kd, u_long kva, const void *buf, size_t len)
{
	int cc;

	if (ISKMEM(kd)) {
		/*
		 * Just like kvm_read, only we write.
		 */
		errno = 0;
		cc = pwrite(kd->vmfd, buf, len, (off_t)kva);
		if (cc < 0) {
			_kvm_syserr(kd, 0, "kvm_write");
			return (-1);
		} else if (cc < len)
			_kvm_err(kd, kd->program, "short write");
		return (cc);
	} else if (ISSYSCTL(kd)) {
		_kvm_err(kd, kd->program, "kvm_open called with KVM_NO_FILES, "
		    "can't use kvm_write");
		return (-1);
	} else {
		_kvm_err(kd, kd->program,
		    "kvm_write not implemented for dead kernels");
		return (-1);
	}
	/* NOTREACHED */
}
