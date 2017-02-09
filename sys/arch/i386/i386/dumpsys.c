/*	$NetBSD: dumpsys.c,v 1.16 2011/12/12 19:03:09 mrg Exp $	*/

/*-
 * Copyright (c) 1996, 1997, 1998, 2000, 2004, 2006, 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum, by Jason R. Thorpe of the Numerical Aerospace
 * Simulation Facility, NASA Ames Research Center.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Coyote Point Systems, Inc. which was written under contract to Coyote
 * Point by Jed Davis and Devon O'Dell.
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
 * Copyright (c) 1982, 1987, 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
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
 *	@(#)machdep.c	7.4 (Berkeley) 6/3/91
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: dumpsys.c,v 1.16 2011/12/12 19:03:09 mrg Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/kcore.h>
#include <sys/core.h>
#include <sys/conf.h>
#include <sys/exec.h>
#include <sys/exec_aout.h>

#include <machine/kcore.h>

#include <uvm/uvm_extern.h>

/*
 * Exports, needed by savecore, the debugger or elsewhere in the kernel.
 */

void	dodumpsys(void);
void	dumpsys(void);

struct pcb	dumppcb;
uint32_t dumpmag = 0x8fca0101;	/* magic number */
int 	dumpsize;		/* pages */
long	dumplo; 		/* blocks */
int	sparse_dump = 1;

/*
 * Module private.
 */
 
#define dump_headerbuf_size	PAGE_SIZE
#define dump_headerbuf_end	(dump_headerbuf + dump_headerbuf_size)
#define dump_headerbuf_avail	(dump_headerbuf_end - dump_headerbuf_ptr)
#define BYTES_PER_DUMP		PAGE_SIZE /* must be a multiple of pagesize */

static vaddr_t	dumpspace;
static paddr_t	max_paddr;
static uint8_t	*sparse_dump_physmap;

static uint8_t	*dump_headerbuf;
static uint8_t	*dump_headerbuf_ptr;
static daddr_t	dump_header_blkno;

static size_t	dump_nmemsegs;
static size_t	dump_npages;
static size_t	dump_header_size;
static size_t	dump_totalbytesleft;

static int	cpu_dump(void);
static int	cpu_dumpsize(void);
static u_long	cpu_dump_mempagecnt(void);

static void	dump_misc_init(void);
static void	dump_seg_prep(void);
static int	dump_seg_iter(int (*)(paddr_t, paddr_t));

static void	sparse_dump_reset(void);
static void	sparse_dump_mark(vaddr_t, vaddr_t, int);
static void	cpu_dump_prep_sparse(void);

static void	dump_header_start(void);
static int	dump_header_flush(void);
static int	dump_header_addbytes(const void*, size_t);
static int	dump_header_addseg(paddr_t, paddr_t);
static int	dump_header_finish(void);

static int	dump_seg_count_range(paddr_t, paddr_t);
static int	dumpsys_seg(paddr_t, paddr_t);

/*
 * From machdep.c.
 */
 
extern phys_ram_seg_t	mem_clusters[VM_PHYSSEG_MAX];
extern int	mem_cluster_cnt;

void
dodumpsys(void)
{
	const struct bdevsw *bdev;
	int dumpend, psize;
	int error;

	if (dumpdev == NODEV)
		return;

	bdev = bdevsw_lookup(dumpdev);
	if (bdev == NULL || bdev->d_psize == NULL)
		return;

	/*
	 * For dumps during autoconfiguration,
	 * if dump device has already configured...
	 */
	if (dumpsize == 0)
		cpu_dumpconf();
	if (dumplo <= 0 || dumpsize == 0) {
		printf("\ndump to dev %llu,%llu not possible\n",
		    (unsigned long long)major(dumpdev),
		    (unsigned long long)minor(dumpdev));
		return;
	}
	printf("\ndumping to dev %llu,%llu offset %ld\n",
	    (unsigned long long)major(dumpdev),
	    (unsigned long long)minor(dumpdev), dumplo);

	psize = bdev_size(dumpdev);
	printf("dump ");
	if (psize == -1) {
		printf("area unavailable\n");
		return;
	}

#if 0	/* XXX this doesn't work.  grr. */
	/* toss any characters present prior to dump */
	while (sget() != NULL); /*syscons and pccons differ */
#endif

	dump_seg_prep();
	dumpend = dumplo + btodb(dump_header_size) + ctod(dump_npages);
	if (dumpend > psize) {
		printf("failed: insufficient space (%d < %d)\n",
		    psize, dumpend);
		goto failed;
	}

	dump_header_start();
	if ((error = cpu_dump()) != 0)
		goto err;
	if ((error = dump_header_finish()) != 0)
		goto err;

	if (dump_header_blkno != dumplo + btodb(dump_header_size)) {
		printf("BAD header size (%ld [written] != %ld [expected])\n",
		    (long)(dump_header_blkno - dumplo),
		    (long)btodb(dump_header_size));
		goto failed;
	}

	dump_totalbytesleft = roundup(ptoa(dump_npages), BYTES_PER_DUMP);
	error = dump_seg_iter(dumpsys_seg);

	if (error == 0 && dump_header_blkno != dumpend) {
		printf("BAD dump size (%ld [written] != %ld [expected])\n",
		    (long)(dumpend - dumplo),
		    (long)(dump_header_blkno - dumplo));
		goto failed;
	}

 err:
	switch (error) {

	case ENXIO:
		printf("device bad\n");
		break;

	case EFAULT:
		printf("device not ready\n");
		break;

	case EINVAL:
		printf("area improper\n");
		break;

	case EIO:
		printf("i/o error\n");
		break;

	case EINTR:
		printf("aborted from console\n");
		break;

	case 0:
		printf("succeeded\n");
		break;

	default:
		printf("error %d\n", error);
		break;
	}
failed:
	printf("\n\n");
	delay(5000000);		/* 5 seconds */
}

/*
 * This is called by main to set dumplo and dumpsize.
 * Dumps always skip the first PAGE_SIZE of disk space
 * in case there might be a disk label stored there.
 * If there is extra space, put dump at the end to
 * reduce the chance that swapping trashes it.
 *
 * Sparse dumps can't placed as close to the end as possible, because
 * savecore(8) has to know where to start reading in the dump device
 * before it has access to any of the crashed system's state.
 *
 * Note also that a sparse dump will never be larger than a full one:
 * in order to add a phys_ram_seg_t to the header, at least one page
 * must be removed.
 */
void
cpu_dumpconf(void)
{
	int nblks, dumpblks;	/* size of dump area */

	if (dumpdev == NODEV)
		goto bad;
	nblks = bdev_size(dumpdev);
	if (nblks <= ctod(1))
		goto bad;

	dumpblks = cpu_dumpsize();
	if (dumpblks < 0)
		goto bad;
	dumpblks += ctod(cpu_dump_mempagecnt());

	/* If dump won't fit (incl. room for possible label): */
	if (dumpblks > (nblks - ctod(1))) {
		/* A sparse dump might (and hopefully will) fit. */
		dumplo = ctod(1);
	} else {
		/* Put dump at end of partition */
		dumplo = nblks - dumpblks;
	}

	/* dumpsize is in page units, and doesn't include headers. */
	dumpsize = cpu_dump_mempagecnt();

	/* Now that we've decided this will work, init ancillary stuff. */
	dump_misc_init();
	return;

 bad:
	dumpsize = 0;
}

vaddr_t
reserve_dumppages(vaddr_t p)
{

	dumpspace = p;
	return (p + BYTES_PER_DUMP);
}

/*
 * Perform assorted dump-related initialization tasks.  Assumes that
 * the maximum physical memory address will not increase afterwards.
 */
static void
dump_misc_init(void)
{
	int i;

	if (dump_headerbuf != NULL)
		return; /* already called */

	for (i = 0; i < mem_cluster_cnt; ++i) {
		paddr_t top = mem_clusters[i].start + mem_clusters[i].size;
		if (max_paddr < top)
			max_paddr = top;
	}
#ifdef DUMP_DEBUG
	printf("dump_misc_init: max_paddr = %#" PRIxPADDR "\n", max_paddr);
#endif

	sparse_dump_physmap = (void*)uvm_km_alloc(kernel_map,
	    roundup(max_paddr / (PAGE_SIZE * NBBY), PAGE_SIZE),
	    PAGE_SIZE, UVM_KMF_WIRED|UVM_KMF_ZERO);
	dump_headerbuf = (void*)uvm_km_alloc(kernel_map,
	    dump_headerbuf_size,
	    PAGE_SIZE, UVM_KMF_WIRED|UVM_KMF_ZERO);
	/* XXXjld should check for failure here, disable dumps if so. */
}

/*
 * Clear the set of pages to include in a sparse dump.
 */
static void
sparse_dump_reset(void)
{

	memset(sparse_dump_physmap, 0,
	    roundup(max_paddr / (PAGE_SIZE * NBBY), PAGE_SIZE));
}

/*
 * Include or exclude pages in a sparse dump, by half-open virtual
 * address interval (which may wrap around the end of the space).
 */
static void
sparse_dump_mark(vaddr_t vbegin, vaddr_t vend, int includep)
{
	pmap_t pmap;
	paddr_t p;
	vaddr_t v;

	/*
	 * If a partial page is called for, the whole page must be included.
	 */
	if (includep) {
		vbegin = rounddown(vbegin, PAGE_SIZE);
		vend = roundup(vend, PAGE_SIZE);
	} else {
		vbegin = roundup(vbegin, PAGE_SIZE);
		vend = rounddown(vend, PAGE_SIZE);
	}

	pmap = pmap_kernel();
	for (v = vbegin; v != vend; v += PAGE_SIZE) {
		if (pmap_extract(pmap, v, &p)) {
			if (includep)
				setbit(sparse_dump_physmap, p/PAGE_SIZE);
			else
				clrbit(sparse_dump_physmap, p/PAGE_SIZE);
		}
	}
}

/*
 * Machine-dependently decides on the contents of a sparse dump, using
 * the above.
 */
static void
cpu_dump_prep_sparse(void)
{

	sparse_dump_reset();
	/* XXX could the alternate recursive page table be skipped? */
	sparse_dump_mark((vaddr_t)PTE_BASE, 0, 1);
	/* Memory for I/O buffers could be unmarked here, for example. */
	/* The kernel text could also be unmarked, but gdb would be upset. */
}

/*
 * Abstractly iterate over the collection of memory segments to be
 * dumped; the callback lacks the customary environment-pointer
 * argument because none of the current users really need one.
 *
 * To be used only after dump_seg_prep is called to set things up.
 */
static int
dump_seg_iter(int (*callback)(paddr_t, paddr_t))
{
	int error, i;

#define CALLBACK(start,size) do {     \
	error = callback(start,size); \
	if (error)                    \
		return error;         \
} while(0)

	for (i = 0; i < mem_cluster_cnt; ++i) {
		/*
		 * The bitmap is scanned within each memory segment,
		 * rather than over its entire domain, in case any
		 * pages outside of the memory proper have been mapped
		 * into kva; they might be devices that wouldn't
		 * appreciate being arbitrarily read, and including
		 * them could also break the assumption that a sparse
		 * dump will always be smaller than a full one.
		 */
		if (sparse_dump) {
			paddr_t p, start, end;
			int lastset;

			start = mem_clusters[i].start;
			end = start + mem_clusters[i].size;
			start = rounddown(start, PAGE_SIZE); /* unnecessary? */
			lastset = 0;
			for (p = start; p < end; p += PAGE_SIZE) {
				int thisset = isset(sparse_dump_physmap,
				    p/PAGE_SIZE);

				if (!lastset && thisset)
					start = p;
				if (lastset && !thisset)
					CALLBACK(start, p - start);
				lastset = thisset;
			}
			if (lastset)
				CALLBACK(start, p - start);
		} else
			CALLBACK(mem_clusters[i].start, mem_clusters[i].size);
	}
	return 0;
#undef CALLBACK
}

/*
 * Prepare for an impending core dump: decide what's being dumped and
 * how much space it will take up.
 */
static void
dump_seg_prep(void)
{

	if (sparse_dump)
		cpu_dump_prep_sparse();

	dump_nmemsegs = 0;
	dump_npages = 0;
	dump_seg_iter(dump_seg_count_range);

	dump_header_size = ALIGN(sizeof(kcore_seg_t)) +
	    ALIGN(sizeof(cpu_kcore_hdr_t)) +
	    ALIGN(dump_nmemsegs * sizeof(phys_ram_seg_t));
	dump_header_size = roundup(dump_header_size, dbtob(1));

	/*
	 * savecore(8) will read this to decide how many pages to
	 * copy, and cpu_dumpconf has already used the pessimistic
	 * value to set dumplo, so it's time to tell the truth.
	 */
	dumpsize = dump_npages; /* XXX could these just be one variable? */
}

static int
dump_seg_count_range(paddr_t start, paddr_t size)
{

	++dump_nmemsegs;
	dump_npages += size / PAGE_SIZE;
	return 0;
}

/*
 * A sparse dump's header may be rather large, due to the number of
 * "segments" emitted.  These routines manage a simple output buffer,
 * so that the header can be written to disk incrementally.
 */
static void
dump_header_start(void)
{

	dump_headerbuf_ptr = dump_headerbuf;
	dump_header_blkno = dumplo;
}

static int
dump_header_flush(void)
{
	const struct bdevsw *bdev;
	size_t to_write;
	int error;

	bdev = bdevsw_lookup(dumpdev);
	to_write = roundup(dump_headerbuf_ptr - dump_headerbuf, dbtob(1));
	error = bdev->d_dump(dumpdev, dump_header_blkno,
	    dump_headerbuf, to_write);
	dump_header_blkno += btodb(to_write);
	dump_headerbuf_ptr = dump_headerbuf;
	return error;
}

static int
dump_header_addbytes(const void* vptr, size_t n)
{
	const char *ptr = vptr;
	int error;

	while (n > dump_headerbuf_avail) {
		memcpy(dump_headerbuf_ptr, ptr, dump_headerbuf_avail);
		ptr += dump_headerbuf_avail;
		n -= dump_headerbuf_avail;
		dump_headerbuf_ptr = dump_headerbuf_end;
		error = dump_header_flush();
		if (error)
			return error;
	}
	memcpy(dump_headerbuf_ptr, ptr, n);
	dump_headerbuf_ptr += n;

	return 0;
}

static int
dump_header_addseg(paddr_t start, paddr_t size)
{
	phys_ram_seg_t seg = { start, size };

	return dump_header_addbytes(&seg, sizeof(seg));
}

static int
dump_header_finish(void)
{

	memset(dump_headerbuf_ptr, 0, dump_headerbuf_avail);
	return dump_header_flush();
}

/*
 * cpu_dumpsize: calculate size of machine-dependent kernel core dump headers
 * for a full (non-sparse) dump.
 */
static int
cpu_dumpsize(void)
{
	int size;

	size = ALIGN(sizeof(kcore_seg_t)) + ALIGN(sizeof(cpu_kcore_hdr_t)) +
	    ALIGN(mem_cluster_cnt * sizeof(phys_ram_seg_t));
	if (roundup(size, dbtob(1)) != dbtob(1))
		return (-1);

	return (1);
}

/*
 * cpu_dump_mempagecnt: calculate the size of RAM (in pages) to be dumped
 * for a full (non-sparse) dump.
 */
static u_long
cpu_dump_mempagecnt(void)
{
	u_long i, n;

	n = 0;
	for (i = 0; i < mem_cluster_cnt; i++)
		n += atop(mem_clusters[i].size);
	return (n);
}

/*
 * cpu_dump: dump the machine-dependent kernel core dump headers.
 */
static int
cpu_dump(void)
{
	kcore_seg_t seg;
	cpu_kcore_hdr_t cpuhdr;
	const struct bdevsw *bdev;

	bdev = bdevsw_lookup(dumpdev);
	if (bdev == NULL)
		return (ENXIO);

	/*
	 * Generate a segment header.
	 */
	CORE_SETMAGIC(seg, KCORE_MAGIC, MID_MACHINE, CORE_CPU);
	seg.c_size = dump_header_size - ALIGN(sizeof(seg));
	(void)dump_header_addbytes(&seg, ALIGN(sizeof(seg)));

	/*
	 * Add the machine-dependent header info.
	 */
	cpuhdr.pdppaddr = PDPpaddr;
	if (use_pae == 1)
		cpuhdr.pdppaddr |= I386_KCORE_PAE;
	cpuhdr.nmemsegs = dump_nmemsegs;
	(void)dump_header_addbytes(&cpuhdr, ALIGN(sizeof(cpuhdr)));

	/*
	 * Write out the memory segment descriptors.
	 */
	return dump_seg_iter(dump_header_addseg);
}

static int
dumpsys_seg(paddr_t maddr, paddr_t bytes)
{
	u_long i, m, n;
	daddr_t blkno;
	const struct bdevsw *bdev;
	int (*dump)(dev_t, daddr_t, void *, size_t);
	int error;

	bdev = bdevsw_lookup(dumpdev);
	dump = bdev->d_dump;

	blkno = dump_header_blkno;
	for (i = 0; i < bytes; i += n, dump_totalbytesleft -= n) {
		/* Print out how many MBs we have left to go. */
		if ((dump_totalbytesleft % (1024*1024)) == 0)
			printf_nolog("%lu ", (unsigned long)
			    (dump_totalbytesleft / (1024 * 1024)));

		/* Limit size for next transfer. */
		n = bytes - i;
		if (n > BYTES_PER_DUMP)
			n = BYTES_PER_DUMP;

		for (m = 0; m < n; m += NBPG)
			pmap_kenter_pa(dumpspace + m, maddr + m,
			    VM_PROT_READ, 0);
		pmap_update(pmap_kernel());

		error = (*dump)(dumpdev, blkno, (void *)dumpspace, n);
		if (error)
			return error;
		maddr += n;
		blkno += btodb(n);		/* XXX? */

#if 0	/* XXX this doesn't work.  grr. */
		/* operator aborting dump? */
		if (sget() != NULL)
			return EINTR;
#endif
	}
	dump_header_blkno = blkno;

	return 0;
}
