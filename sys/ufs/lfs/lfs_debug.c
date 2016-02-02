/*	$NetBSD: lfs_debug.c,v 1.54 2015/09/01 06:12:04 dholland Exp $	*/

/*-
 * Copyright (c) 1999, 2000, 2001, 2002, 2003 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Konrad E. Schroder <perseant@hhhh.org>.
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
 * Copyright (c) 1991, 1993
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
 *	@(#)lfs_debug.c	8.1 (Berkeley) 6/11/93
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: lfs_debug.c,v 1.54 2015/09/01 06:12:04 dholland Exp $");

#ifdef DEBUG

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/buf.h>
#include <sys/syslog.h>
#include <sys/proc.h>

#include <ufs/lfs/ulfs_inode.h>
#include <ufs/lfs/lfs.h>
#include <ufs/lfs/lfs_accessors.h>
#include <ufs/lfs/lfs_extern.h>

int lfs_lognum;
struct lfs_log_entry lfs_log[LFS_LOGLENGTH];

int
lfs_bwrite_log(struct buf *bp, const char *file, int line)
{
	struct vop_bwrite_args a;

	a.a_desc = VDESC(vop_bwrite);
	a.a_bp = bp;

	if (!(bp->b_flags & B_GATHERED) && !(bp->b_oflags & BO_DELWRI)) {
		LFS_ENTER_LOG("write", file, line, bp->b_lblkno, bp->b_flags,
			curproc->p_pid);
	}
	return (VCALL(bp->b_vp, VOFFSET(vop_bwrite), &a));
}

void
lfs_dumplog(void)
{
	int i;
	const char *cp;

	for (i = lfs_lognum; i != (lfs_lognum - 1) % LFS_LOGLENGTH;
	     i = (i + 1) % LFS_LOGLENGTH)
		if (lfs_log[i].file) {
			/* Only print out basename, for readability */
			cp = lfs_log[i].file;
			while(*cp)
				++cp;
			while(*cp != '/' && cp > lfs_log[i].file)
				--cp;

			printf("lbn %" PRId64 " %s %lx %d, %d %s\n",
				lfs_log[i].block,
				lfs_log[i].op,
				lfs_log[i].flags,
				lfs_log[i].pid,
				lfs_log[i].line,
				cp);
		}
}

void
lfs_dump_super(struct lfs *lfsp)
{
	int i;

	printf("%s%x\t%s%x\t%s%ju\t%s%d\n",
	       "magic	 ", lfsp->lfs_is64 ?
			lfsp->lfs_dlfs_u.u_64.dlfs_magic :
			lfsp->lfs_dlfs_u.u_32.dlfs_magic,
	       "version	 ", lfs_sb_getversion(lfsp),
	       "size	 ", (uintmax_t)lfs_sb_getsize(lfsp),
	       "ssize	 ", lfs_sb_getssize(lfsp));
	printf("%s%ju\t%s%d\t%s%d\t%s%d\n",
	       "dsize	 ", (uintmax_t)lfs_sb_getdsize(lfsp),
	       "bsize	 ", lfs_sb_getbsize(lfsp),
	       "fsize	 ", lfs_sb_getfsize(lfsp),
	       "frag	 ", lfs_sb_getfrag(lfsp));

	printf("%s%d\t%s%d\t%s%d\t%s%d\n",
	       "minfree	 ", lfs_sb_getminfree(lfsp),
	       "inopb	 ", lfs_sb_getinopb(lfsp),
	       "ifpb	 ", lfs_sb_getifpb(lfsp),
	       "nindir	 ", lfs_sb_getnindir(lfsp));

	printf("%s%d\t%s%d\t%s%d\t%s%d\n",
	       "nseg	 ", lfs_sb_getnseg(lfsp),
	       "nspf	 ", lfs_sb_getnspf(lfsp),
	       "cleansz	 ", lfs_sb_getcleansz(lfsp),
	       "segtabsz ", lfs_sb_getsegtabsz(lfsp));

	printf("%s%x\t%s%d\t%s%lx\t%s%d\n",
	       "segmask	 ", lfs_sb_getsegmask(lfsp),
	       "segshift ", lfs_sb_getsegshift(lfsp),
	       "bmask	 ", (unsigned long)lfs_sb_getbmask(lfsp),
	       "bshift	 ", lfs_sb_getbshift(lfsp));

	printf("%s%lu\t%s%d\t%s%lx\t%s%u\n",
	       "ffmask	 ", (unsigned long)lfs_sb_getffmask(lfsp),
	       "ffshift	 ", lfs_sb_getffshift(lfsp),
	       "fbmask	 ", (unsigned long)lfs_sb_getfbmask(lfsp),
	       "fbshift	 ", lfs_sb_getfbshift(lfsp));

	printf("%s%d\t%s%d\t%s%x\t%s%jx\n",
	       "sushift	 ", lfs_sb_getsushift(lfsp),
	       "fsbtodb	 ", lfs_sb_getfsbtodb(lfsp),
	       "cksum	 ", lfs_sb_getcksum(lfsp),
	       "maxfilesize ", (uintmax_t)lfs_sb_getmaxfilesize(lfsp));

	printf("Superblock disk addresses:");
	for (i = 0; i < LFS_MAXNUMSB; i++)
		printf(" %jx", (intmax_t)lfs_sb_getsboff(lfsp, i));
	printf("\n");

	printf("Checkpoint Info\n");
	printf("%s%ju\t%s%jx\n",
	       "freehd	 ", (uintmax_t)lfs_sb_getfreehd(lfsp),
	       "idaddr	 ", (intmax_t)lfs_sb_getidaddr(lfsp));
	printf("%s%jx\t%s%ju\t%s%jx\t%s%jx\t%s%jx\t%s%jx\n",
	       "bfree	 ", (intmax_t)lfs_sb_getbfree(lfsp),
	       "nfiles	 ", (uintmax_t)lfs_sb_getnfiles(lfsp),
	       "lastseg	 ", (intmax_t)lfs_sb_getlastseg(lfsp),
	       "nextseg	 ", (intmax_t)lfs_sb_getnextseg(lfsp),
	       "curseg	 ", (intmax_t)lfs_sb_getcurseg(lfsp),
	       "offset	 ", (intmax_t)lfs_sb_getoffset(lfsp));
	printf("tstamp	 %llx\n", (long long)lfs_sb_gettstamp(lfsp));

	if (!lfsp->lfs_is64) {
		printf("32-bit only derived or constant fields\n");
		printf("%s%u\n",
		       "ifile	 ", lfs_sb_getifile(lfsp));
	}
}

void
lfs_dump_dinode(struct lfs *fs, union lfs_dinode *dip)
{
	int i;

	printf("%s%u\t%s%d\t%s%u\t%s%u\t%s%ju\t%s%ju\n",
	       "mode   ", lfs_dino_getmode(fs, dip),
	       "nlink  ", lfs_dino_getnlink(fs, dip),
	       "uid    ", lfs_dino_getuid(fs, dip),
	       "gid    ", lfs_dino_getgid(fs, dip),
	       "size   ", (uintmax_t)lfs_dino_getsize(fs, dip),
	       "blocks ", (uintmax_t)lfs_dino_getblocks(fs, dip));
	printf("inum  %ju\n", (uintmax_t)lfs_dino_getinumber(fs, dip));
	printf("Direct Addresses\n");
	for (i = 0; i < ULFS_NDADDR; i++) {
		printf("\t%jx", (intmax_t)lfs_dino_getdb(fs, dip, i));
		if ((i % 6) == 5)
			printf("\n");
	}
	for (i = 0; i < ULFS_NIADDR; i++)
		printf("\t%jx", (intmax_t)lfs_dino_getib(fs, dip, i));
	printf("\n");
}

void
lfs_check_segsum(struct lfs *fs, struct segment *sp, char *file, int line)
{
	int actual;
#if 0
	static int offset;
#endif

	if ((actual = 1) == 1)
		return; /* XXXX not checking this anymore, really */

	if (sp->sum_bytes_left >= FINFOSIZE(fs)
	   && lfs_fi_getnblocks(fs, sp->fip) > 512) {
		printf("%s:%d: fi_nblocks = %d\n", file, line,
		       lfs_fi_getnblocks(fs, sp->fip));
#ifdef DDB
		Debugger();
#endif
	}

	if (sp->sum_bytes_left > 484) {
		printf("%s:%d: bad value (%d = -%d) for sum_bytes_left\n",
		       file, line, sp->sum_bytes_left, lfs_sb_getsumsize(fs)-sp->sum_bytes_left);
		panic("too many bytes");
	}

	actual = lfs_sb_getsumsize(fs)
		/* amount taken up by FINFOs */
		- ((char *)NEXT_FINFO(fs, sp->fip) - (char *)(sp->segsum))
			/* amount taken up by inode blocks */
			/* XXX should this be INUMSIZE or BLKPTRSIZE? */
			- LFS_INUMSIZE(fs)*((sp->ninodes+LFS_INOPB(fs)-1) / LFS_INOPB(fs));
#if 0
	if (actual - sp->sum_bytes_left < offset)
	{
		printf("%s:%d: offset changed %d -> %d\n", file, line,
		       offset, actual-sp->sum_bytes_left);
		offset = actual - sp->sum_bytes_left;
		/* panic("byte mismatch"); */
	}
#endif
#if 0
	if (actual != sp->sum_bytes_left)
		printf("%s:%d: warning: segsum miscalc at %d (-%d => %d)\n",
		       file, line, sp->sum_bytes_left,
		       lfs_sb_getsumsize(fs)-sp->sum_bytes_left,
		       actual);
#endif
	if (sp->sum_bytes_left > 0
	   && ((char *)(sp->segsum))[lfs_sb_getsumsize(fs)
				     - sizeof(int32_t) * ((sp->ninodes+LFS_INOPB(fs)-1) / LFS_INOPB(fs))
				     - sp->sum_bytes_left] != '\0') {
		printf("%s:%d: warning: segsum overwrite at %d (-%d => %d)\n",
		       file, line, sp->sum_bytes_left,
		       lfs_sb_getsumsize(fs)-sp->sum_bytes_left,
		       actual);
#ifdef DDB
		Debugger();
#endif
	}
}

void
lfs_check_bpp(struct lfs *fs, struct segment *sp, char *file, int line)
{
	daddr_t blkno;
	struct buf **bpp;
	struct vnode *devvp;

	devvp = VTOI(fs->lfs_ivnode)->i_devvp;
	blkno = (*(sp->bpp))->b_blkno;
	for (bpp = sp->bpp; bpp < sp->cbpp; bpp++) {
		if ((*bpp)->b_blkno != blkno) {
			if ((*bpp)->b_vp == devvp) {
				printf("Oops, would misplace raw block "
				       "0x%" PRIx64 " at 0x%" PRIx64 "\n",
				       (*bpp)->b_blkno,
				       blkno);
			} else {
				printf("%s:%d: misplace ino %llu lbn %" PRId64
				       " at 0x%" PRIx64 " instead of "
				       "0x%" PRIx64 "\n",
				       file, line,
				       (unsigned long long)
				       VTOI((*bpp)->b_vp)->i_number,
				       (*bpp)->b_lblkno,
				       blkno,
				       (*bpp)->b_blkno);
			}
		}
		blkno += LFS_FSBTODB(fs, lfs_btofsb(fs, (*bpp)->b_bcount));
	}
}

int lfs_debug_log_subsys[DLOG_MAX];

/*
 * Log events from various debugging areas of LFS, depending on what
 * the user has enabled.
 */
void
lfs_debug_log(int subsys, const char *fmt, ...)
{
	va_list ap;

	/* If not debugging this subsys, exit */
	if (lfs_debug_log_subsys[subsys] == 0)
		return;

	va_start(ap, fmt);
	vlog(LOG_DEBUG, fmt, ap);
	va_end(ap);
}
#endif /* DEBUG */
