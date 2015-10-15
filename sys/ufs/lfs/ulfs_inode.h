/*	$NetBSD: ulfs_inode.h,v 1.16 2015/09/01 06:08:37 dholland Exp $	*/
/*  from NetBSD: inode.h,v 1.64 2012/11/19 00:36:21 jakllsch Exp  */

/*
 * Copyright (c) 1982, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 *	@(#)inode.h	8.9 (Berkeley) 5/14/95
 */

#ifndef _UFS_LFS_ULFS_INODE_H_
#define	_UFS_LFS_ULFS_INODE_H_

#include <sys/vnode.h>
#include <ufs/lfs/lfs_inode.h>
#include <ufs/lfs/ulfs_dinode.h>
#include <ufs/lfs/ulfs_quotacommon.h>

/*
 * These macros are used to bracket ULFS directory ops, so that we can
 * identify all the pages touched during directory ops which need to
 * be ordered and flushed atomically, so that they may be recovered.
 *
 * Because we have to mark nodes VU_DIROP in order to prevent
 * the cache from reclaiming them while a dirop is in progress, we must
 * also manage the number of nodes so marked (otherwise we can run out).
 * We do this by setting lfs_dirvcount to the number of marked vnodes; it
 * is decremented during segment write, when VU_DIROP is taken off.
 */
#define	MARK_VNODE(vp)			lfs_mark_vnode(vp)
#define	UNMARK_VNODE(vp)		lfs_unmark_vnode(vp)
int lfs_set_dirop(struct vnode *, struct vnode *);
void lfs_unset_dirop(struct lfs *, struct vnode *, const char *);

/* Misc. definitions */
#define BW_CLEAN	1		/* Flag for lfs_bwrite_ext() */
#define PG_DELWRI	PG_PAGER1	/* Local def for delayed pageout */

/* Resource limits */
#define	LFS_MAX_RESOURCE(x, u)	(((x) >> 2) - 10 * (u))
#define	LFS_WAIT_RESOURCE(x, u)	(((x) >> 1) - ((x) >> 3) - 10 * (u))
#define	LFS_INVERSE_MAX_RESOURCE(x, u)	(((x) + 10 * (u)) << 2)
#define LFS_MAX_BUFS	    LFS_MAX_RESOURCE(nbuf, 1)
#define LFS_WAIT_BUFS	    LFS_WAIT_RESOURCE(nbuf, 1)
#define LFS_INVERSE_MAX_BUFS(n)	LFS_INVERSE_MAX_RESOURCE(n, 1)
#define LFS_MAX_BYTES	    LFS_MAX_RESOURCE(bufmem_lowater, PAGE_SIZE)
#define LFS_INVERSE_MAX_BYTES(n) LFS_INVERSE_MAX_RESOURCE(n, PAGE_SIZE)
#define LFS_WAIT_BYTES	    LFS_WAIT_RESOURCE(bufmem_lowater, PAGE_SIZE)
#define LFS_MAX_DIROP	    ((desiredvnodes >> 2) + (desiredvnodes >> 3))
#define SIZEOF_DIROP(fs)	(2 * (lfs_sb_getbsize(fs) + DINOSIZE(fs)))
#define LFS_MAX_FSDIROP(fs)						\
	(lfs_sb_getnclean(fs) <= lfs_sb_getresvseg(fs) ? 0 :		\
	 ((lfs_sb_getnclean(fs) - lfs_sb_getresvseg(fs)) * lfs_sb_getssize(fs)) / \
          (2 * SIZEOF_DIROP(fs)))
#define LFS_MAX_PAGES	lfs_max_pages()
#define LFS_WAIT_PAGES	lfs_wait_pages()
#define LFS_BUFWAIT	    2	/* How long to wait if over *_WAIT_* */

#ifdef _KERNEL
extern u_long bufmem_lowater, bufmem_hiwater; /* XXX */

int lfs_wait_pages(void);
int lfs_max_pages(void);
#endif /* _KERNEL */

/* How starved can we be before we start holding back page writes */
#define LFS_STARVED_FOR_SEGS(fs) (lfs_sb_getnclean(fs) < lfs_sb_getresvseg(fs))

/*
 * Reserved blocks for lfs_malloc
 */

/* Structure to keep reserved blocks */
typedef struct lfs_res_blk {
	void *p;
	LIST_ENTRY(lfs_res_blk) res;
	int size;
	char inuse;
} res_t;

/* Types for lfs_newbuf and lfs_malloc */
#define LFS_NB_UNKNOWN -1
#define LFS_NB_SUMMARY	0
#define LFS_NB_SBLOCK	1
#define LFS_NB_IBLOCK	2
#define LFS_NB_CLUSTER	3
#define LFS_NB_CLEAN	4
#define LFS_NB_BLKIOV	5
#define LFS_NB_COUNT	6 /* always last */

/* Number of reserved memory blocks of each type */
#define LFS_N_SUMMARIES 2
#define LFS_N_SBLOCKS	1   /* Always 1, to throttle superblock writes */
#define LFS_N_IBLOCKS	16  /* In theory ssize/bsize; in practice around 2 */
#define LFS_N_CLUSTERS	16  /* In theory ssize/MAXPHYS */
#define LFS_N_CLEAN	0
#define LFS_N_BLKIOV	1

/* Total count of "large" (non-pool) types */
#define LFS_N_TOTAL (LFS_N_SUMMARIES + LFS_N_SBLOCKS + LFS_N_IBLOCKS +	\
		     LFS_N_CLUSTERS + LFS_N_CLEAN + LFS_N_BLKIOV)

/* Counts for pool types */
#define LFS_N_CL	LFS_N_CLUSTERS
#define LFS_N_BPP	2
#define LFS_N_SEG	2

/*
 * "struct buf" associated definitions
 */

/* Determine if a buffer belongs to the ifile */
#define IS_IFILE(bp)	(VTOI(bp->b_vp)->i_number == LFS_IFILE_INUM)

#ifdef _KERNEL
/* This overlays the fid structure (see fstypes.h). */
struct ulfs_ufid {
	u_int16_t ufid_len;	/* Length of structure. */
	u_int16_t ufid_pad;	/* Force 32-bit alignment. */
	u_int32_t ufid_ino;	/* File number (ino). */
	int32_t	  ufid_gen;	/* Generation number. */
};
/* Filehandle structure for exported LFSes */
struct lfid {
	struct ulfs_ufid lfid_ufid;
#define lfid_len lfid_ufid.ufid_len
#define lfid_ino lfid_ufid.ufid_ino
#define lfid_gen lfid_ufid.ufid_gen
	uint32_t lfid_ident;
};
#endif /* _KERNEL */

/* Address calculations for metadata located in the inode */
#define	S_INDIR(fs)	-ULFS_NDADDR
#define	D_INDIR(fs)	(S_INDIR(fs) - LFS_NINDIR(fs) - 1)
#define	T_INDIR(fs)	(D_INDIR(fs) - LFS_NINDIR(fs) * LFS_NINDIR(fs) - 1)

/*
 * "struct vnode" associated definitions
 */

/* Heuristic emptiness measure */
#define VPISEMPTY(vp)	 (LIST_EMPTY(&(vp)->v_dirtyblkhd) && 		\
			  !(vp->v_type == VREG && (vp)->v_iflag & VI_ONWORKLST) &&\
			  VTOI(vp)->i_lfs_nbtree == 0)

#define WRITEINPROG(vp) ((vp)->v_numoutput > 0 ||			\
	(!LIST_EMPTY(&(vp)->v_dirtyblkhd) &&				\
	 !(VTOI(vp)->i_flag & (IN_MODIFIED | IN_ACCESSED | IN_CLEANING))))





#if defined(_KERNEL)

/*
 * The DIP macro is used to access fields in the dinode that are
 * not cached in the inode itself.
 */
#define	DIP(ip, field) \
	(((ip)->i_ump->um_fstype == ULFS1) ? \
	(ip)->i_din->u_32.di_##field : (ip)->i_din->u_64.di_##field)

#define	DIP_ASSIGN(ip, field, value)					\
	do {								\
		if ((ip)->i_ump->um_fstype == ULFS1)			\
			(ip)->i_din->u_32.di_##field = (value);	\
		else							\
			(ip)->i_din->u_64.di_##field = (value);	\
	} while(0)

#define	DIP_ADD(ip, field, value)					\
	do {								\
		if ((ip)->i_ump->um_fstype == ULFS1)			\
			(ip)->i_din->u_32.di_##field += (value);	\
		else							\
			(ip)->i_din->u_64.di_##field += (value);	\
	} while(0)

/* XXX rework this better */
#define	 SHORTLINK(ip) \
	(((ip)->i_ump->um_fstype == ULFS1) ? \
	(void *)(ip)->i_din->u_32.di_db : (void *)(ip)->i_din->u_64.di_db)


/*
 * Structure used to pass around logical block paths generated by
 * ulfs_getlbns and used by truncate and bmap code.
 */
struct indir {
	daddr_t in_lbn;		/* Logical block number. */
	int	in_off;			/* Offset in buffer. */
	int	in_exists;		/* Flag if the block exists. */
};

/* Convert between inode pointers and vnode pointers. */
#define	VTOI(vp)	((struct inode *)(vp)->v_data)
#define	ITOV(ip)	((ip)->i_vnode)

#endif /* _KERNEL */

#endif /* !_UFS_LFS_ULFS_INODE_H_ */
