/*	$NetBSD: lfs_debug.c,v 1.39 2011/07/17 20:54:54 joerg Exp $	*/

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
__KERNEL_RCSID(0, "$NetBSD: lfs_debug.c,v 1.39 2011/07/17 20:54:54 joerg Exp $");

#ifdef DEBUG

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/buf.h>
#include <sys/syslog.h>
#include <sys/proc.h>

#include <ufs/ufs/inode.h>
#include <ufs/lfs/lfs.h>
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

	printf("%s%x\t%s%x\t%s%d\t%s%d\n",
	       "magic	 ", lfsp->lfs_magic,
	       "version	 ", lfsp->lfs_version,
	       "size	 ", lfsp->lfs_size,
	       "ssize	 ", lfsp->lfs_ssize);
	printf("%s%d\t%s%d\t%s%d\t%s%d\n",
	       "dsize	 ", lfsp->lfs_dsize,
	       "bsize	 ", lfsp->lfs_bsize,
	       "fsize	 ", lfsp->lfs_fsize,
	       "frag	 ", lfsp->lfs_frag);

	printf("%s%d\t%s%d\t%s%d\t%s%d\n",
	       "minfree	 ", lfsp->lfs_minfree,
	       "inopb	 ", lfsp->lfs_inopb,
	       "ifpb	 ", lfsp->lfs_ifpb,
	       "nindir	 ", lfsp->lfs_nindir);

	printf("%s%d\t%s%d\t%s%d\t%s%d\n",
	       "nseg	 ", lfsp->lfs_nseg,
	       "nspf	 ", lfsp->lfs_nspf,
	       "cleansz	 ", lfsp->lfs_cleansz,
	       "segtabsz ", lfsp->lfs_segtabsz);

	printf("%s%x\t%s%d\t%s%lx\t%s%d\n",
	       "segmask	 ", lfsp->lfs_segmask,
	       "segshift ", lfsp->lfs_segshift,
	       "bmask	 ", (unsigned long)lfsp->lfs_bmask,
	       "bshift	 ", lfsp->lfs_bshift);

	printf("%s%lu\t%s%d\t%s%lx\t%s%u\n",
	       "ffmask	 ", (unsigned long)lfsp->lfs_ffmask,
	       "ffshift	 ", lfsp->lfs_ffshift,
	       "fbmask	 ", (unsigned long)lfsp->lfs_fbmask,
	       "fbshift	 ", lfsp->lfs_fbshift);

	printf("%s%d\t%s%d\t%s%x\t%s%qx\n",
	       "sushift	 ", lfsp->lfs_sushift,
	       "fsbtodb	 ", lfsp->lfs_fsbtodb,
	       "cksum	 ", lfsp->lfs_cksum,
	       "maxfilesize ", (long long)lfsp->lfs_maxfilesize);

	printf("Superblock disk addresses:");
	for (i = 0; i < LFS_MAXNUMSB; i++)
		printf(" %x", lfsp->lfs_sboffs[i]);
	printf("\n");

	printf("Checkpoint Info\n");
	printf("%s%d\t%s%x\t%s%d\n",
	       "freehd	 ", lfsp->lfs_freehd,
	       "idaddr	 ", lfsp->lfs_idaddr,
	       "ifile	 ", lfsp->lfs_ifile);
	printf("%s%x\t%s%d\t%s%x\t%s%x\t%s%x\t%s%x\n",
	       "bfree	 ", lfsp->lfs_bfree,
	       "nfiles	 ", lfsp->lfs_nfiles,
	       "lastseg	 ", lfsp->lfs_lastseg,
	       "nextseg	 ", lfsp->lfs_nextseg,
	       "curseg	 ", lfsp->lfs_curseg,
	       "offset	 ", lfsp->lfs_offset);
	printf("tstamp	 %llx\n", (long long)lfsp->lfs_tstamp);
}

void
lfs_dump_dinode(struct ufs1_dinode *dip)
{
	int i;

	printf("%s%u\t%s%d\t%s%u\t%s%u\t%s%qu\t%s%d\n",
	       "mode   ", dip->di_mode,
	       "nlink  ", dip->di_nlink,
	       "uid    ", dip->di_uid,
	       "gid    ", dip->di_gid,
	       "size   ", (long long)dip->di_size,
	       "blocks ", dip->di_blocks);
	printf("inum  %d\n", dip->di_inumber);
	printf("Direct Addresses\n");
	for (i = 0; i < NDADDR; i++) {
		printf("\t%x", dip->di_db[i]);
		if ((i % 6) == 5)
			printf("\n");
	}
	for (i = 0; i < NIADDR; i++)
		printf("\t%x", dip->di_ib[i]);
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

	if (sp->sum_bytes_left >= FINFOSIZE
	   && sp->fip->fi_nblocks > 512) {
		printf("%s:%d: fi_nblocks = %d\n",file,line,sp->fip->fi_nblocks);
#ifdef DDB
		Debugger();
#endif
	}

	if (sp->sum_bytes_left > 484) {
		printf("%s:%d: bad value (%d = -%d) for sum_bytes_left\n",
		       file, line, sp->sum_bytes_left, fs->lfs_sumsize-sp->sum_bytes_left);
		panic("too many bytes");
	}

	actual = fs->lfs_sumsize
		/* amount taken up by FINFOs */
		- ((char *)&(sp->fip->fi_blocks[sp->fip->fi_nblocks]) - (char *)(sp->segsum))
			/* amount taken up by inode blocks */
			- sizeof(int32_t)*((sp->ninodes+INOPB(fs)-1) / INOPB(fs));
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
		       fs->lfs_sumsize-sp->sum_bytes_left,
		       actual);
#endif
	if (sp->sum_bytes_left > 0
	   && ((char *)(sp->segsum))[fs->lfs_sumsize
				     - sizeof(int32_t) * ((sp->ninodes+INOPB(fs)-1) / INOPB(fs))
				     - sp->sum_bytes_left] != '\0') {
		printf("%s:%d: warning: segsum overwrite at %d (-%d => %d)\n",
		       file, line, sp->sum_bytes_left,
		       fs->lfs_sumsize-sp->sum_bytes_left,
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
		blkno += fsbtodb(fs, btofsb(fs, (*bpp)->b_bcount));
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
