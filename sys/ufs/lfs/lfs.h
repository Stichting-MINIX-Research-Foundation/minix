/*	$NetBSD: lfs.h,v 1.134 2011/07/11 08:27:40 hannken Exp $	*/

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
/*-
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
 *	@(#)lfs.h	8.9 (Berkeley) 5/8/95
 */

#ifndef _UFS_LFS_LFS_H_
#define _UFS_LFS_LFS_H_

#include <sys/rwlock.h>
#include <sys/mutex.h>
#include <sys/queue.h>
#include <sys/condvar.h>

/*
 * Compile-time options for LFS.
 */
#define LFS_IFIND_RETRIES  16
#define LFS_LOGLENGTH      1024 /* size of debugging log */
#define LFS_MAX_ACTIVE	   10	/* Dirty segments before ckp forced */

/*
 * Fixed filesystem layout parameters
 */
#define	LFS_LABELPAD	8192		/* LFS label size */
#define	LFS_SBPAD	8192		/* LFS superblock size */

#define	LFS_UNUSED_INUM	0		/* 0: out of band inode number */
#define	LFS_IFILE_INUM	1		/* 1: IFILE inode number */
					/* 2: Root inode number */
#define	LOSTFOUNDINO	3		/* 3: lost+found inode number */
#define	LFS_FIRST_INUM	4		/* 4: first free inode number */

#define	LFS_V1_SUMMARY_SIZE	512     /* V1 fixed summary size */
#define	LFS_DFL_SUMMARY_SIZE	512	/* Default summary size */

#define LFS_MAX_DADDR	0x7fffffff	/* Highest addressable fsb */

#define LFS_MAXNAMLEN	255		/* maximum name length in a dir */

/* Adjustable filesystem parameters */
#define MIN_FREE_SEGS	20
#define MIN_RESV_SEGS	15
#ifndef LFS_ATIME_IFILE
# define LFS_ATIME_IFILE 0 /* Store atime info in ifile (optional in LFSv1) */
#endif
#define LFS_MARKV_MAXBLKCNT	65536	/* Max block count for lfs_markv() */

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
#define SIZEOF_DIROP(fs)	(2 * ((fs)->lfs_bsize + DINODE1_SIZE))
#define LFS_MAX_FSDIROP(fs)						\
	((fs)->lfs_nclean <= (fs)->lfs_resvseg ? 0 :			\
	 (((fs)->lfs_nclean - (fs)->lfs_resvseg) * (fs)->lfs_ssize) /	\
          (2 * SIZEOF_DIROP(fs)))
#define LFS_MAX_PAGES	lfs_max_pages()
#define LFS_WAIT_PAGES	lfs_wait_pages()
#define LFS_BUFWAIT	    2	/* How long to wait if over *_WAIT_* */

#ifdef _KERNEL
int lfs_wait_pages(void);
int lfs_max_pages(void);
#endif /* _KERNEL */

/* How starved can we be before we start holding back page writes */
#define LFS_STARVED_FOR_SEGS(fs) ((fs)->lfs_nclean < (fs)->lfs_resvseg)

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

/* Unassigned disk addresses. */
#define	UNASSIGNED	-1
#define UNWRITTEN	-2

/* Unused logical block number */
#define LFS_UNUSED_LBN	-1

/* Determine if a buffer belongs to the ifile */
#define IS_IFILE(bp)	(VTOI(bp->b_vp)->i_number == LFS_IFILE_INUM)

# define LFS_LOCK_BUF(bp) do {						\
	if (((bp)->b_flags & B_LOCKED) == 0 && bp->b_iodone == NULL) {	\
		mutex_enter(&lfs_lock);					\
		++locked_queue_count;					\
		locked_queue_bytes += bp->b_bufsize;			\
		mutex_exit(&lfs_lock);					\
	}								\
	(bp)->b_flags |= B_LOCKED;					\
} while (0)

# define LFS_UNLOCK_BUF(bp) do {					\
	if (((bp)->b_flags & B_LOCKED) != 0 && bp->b_iodone == NULL) {	\
		mutex_enter(&lfs_lock);					\
		--locked_queue_count;					\
		locked_queue_bytes -= bp->b_bufsize;			\
		if (locked_queue_count < LFS_WAIT_BUFS &&		\
		    locked_queue_bytes < LFS_WAIT_BYTES)		\
			cv_broadcast(&locked_queue_cv);			\
		mutex_exit(&lfs_lock);					\
	}								\
	(bp)->b_flags &= ~B_LOCKED;					\
} while (0)

#ifdef _KERNEL

extern u_long bufmem_lowater, bufmem_hiwater; /* XXX */

# define LFS_IS_MALLOC_BUF(bp) ((bp)->b_iodone == lfs_callback)

# ifdef DEBUG
#  define LFS_DEBUG_COUNTLOCKED(m) do {					\
	if (lfs_debug_log_subsys[DLOG_LLIST]) {				\
		lfs_countlocked(&locked_queue_count, &locked_queue_bytes, (m)); \
		cv_broadcast(&locked_queue_cv);				\
	}								\
} while (0)
# else
#  define LFS_DEBUG_COUNTLOCKED(m)
# endif

/* log for debugging writes to the Ifile */
# ifdef DEBUG
struct lfs_log_entry {
	const char *op;
	const char *file;
	int pid;
	int line;
	daddr_t block;
	unsigned long flags;
};
extern int lfs_lognum;
extern struct lfs_log_entry lfs_log[LFS_LOGLENGTH];
#  define LFS_BWRITE_LOG(bp) lfs_bwrite_log((bp), __FILE__, __LINE__)
#  define LFS_ENTER_LOG(theop, thefile, theline, lbn, theflags, thepid) do {\
	int _s;								\
									\
	mutex_enter(&lfs_lock);						\
	_s = splbio();							\
	lfs_log[lfs_lognum].op = theop;					\
	lfs_log[lfs_lognum].file = thefile;				\
	lfs_log[lfs_lognum].line = (theline);				\
	lfs_log[lfs_lognum].pid = (thepid);				\
	lfs_log[lfs_lognum].block = (lbn);				\
	lfs_log[lfs_lognum].flags = (theflags);				\
	lfs_lognum = (lfs_lognum + 1) % LFS_LOGLENGTH;			\
	splx(_s);							\
	mutex_exit(&lfs_lock);						\
} while (0)

#  define LFS_BCLEAN_LOG(fs, bp) do {					\
	if ((bp)->b_vp == (fs)->lfs_ivnode)				\
		LFS_ENTER_LOG("clear", __FILE__, __LINE__,		\
			      bp->b_lblkno, bp->b_flags, curproc->p_pid);\
} while (0)

/* Must match list in lfs_vfsops.c ! */
#  define DLOG_RF     0  /* roll forward */
#  define DLOG_ALLOC  1  /* inode alloc */
#  define DLOG_AVAIL  2  /* lfs_{,r,f}avail */
#  define DLOG_FLUSH  3  /* flush */
#  define DLOG_LLIST  4  /* locked list accounting */
#  define DLOG_WVNODE 5  /* vflush/writevnodes verbose */
#  define DLOG_VNODE  6  /* vflush/writevnodes */
#  define DLOG_SEG    7  /* segwrite */
#  define DLOG_SU     8  /* seguse accounting */
#  define DLOG_CLEAN  9  /* cleaner routines */
#  define DLOG_MOUNT  10 /* mount/unmount */
#  define DLOG_PAGE   11 /* putpages/gop_write */
#  define DLOG_DIROP  12 /* dirop accounting */
#  define DLOG_MALLOC 13 /* lfs_malloc accounting */
#  define DLOG_MAX    14 /* The terminator */
#  define DLOG(a) lfs_debug_log a
# else /* ! DEBUG */
#  define LFS_BCLEAN_LOG(fs, bp)
#  define LFS_BWRITE_LOG(bp)		VOP_BWRITE((bp)->b_vp, (bp))
#  define DLOG(a)
# endif /* ! DEBUG */
#else /* ! _KERNEL */
# define LFS_BWRITE_LOG(bp)		VOP_BWRITE((bp))
#endif /* _KERNEL */

#ifdef _KERNEL
/* Filehandle structure for exported LFSes */
struct lfid {
	struct ufid lfid_ufid;
#define lfid_len lfid_ufid.ufid_len
#define lfid_ino lfid_ufid.ufid_ino
#define lfid_gen lfid_ufid.ufid_gen
	uint32_t lfid_ident;
};
#endif /* _KERNEL */

/*
 * "struct inode" associated definitions
 */

/* Address calculations for metadata located in the inode */
#define	S_INDIR(fs)	-NDADDR
#define	D_INDIR(fs)	(S_INDIR(fs) - NINDIR(fs) - 1)
#define	T_INDIR(fs)	(D_INDIR(fs) - NINDIR(fs) * NINDIR(fs) - 1)

/* For convenience */
#define IN_ALLMOD (IN_MODIFIED|IN_ACCESS|IN_CHANGE|IN_UPDATE|IN_MODIFY|IN_ACCESSED|IN_CLEANING)

#define LFS_SET_UINO(ip, flags) do {					\
	if (((flags) & IN_ACCESSED) && !((ip)->i_flag & IN_ACCESSED))	\
		++(ip)->i_lfs->lfs_uinodes;				\
	if (((flags) & IN_CLEANING) && !((ip)->i_flag & IN_CLEANING))	\
		++(ip)->i_lfs->lfs_uinodes;				\
	if (((flags) & IN_MODIFIED) && !((ip)->i_flag & IN_MODIFIED))	\
		++(ip)->i_lfs->lfs_uinodes;				\
	(ip)->i_flag |= (flags);					\
} while (0)

#define LFS_CLR_UINO(ip, flags) do {					\
	if (((flags) & IN_ACCESSED) && ((ip)->i_flag & IN_ACCESSED))	\
		--(ip)->i_lfs->lfs_uinodes;				\
	if (((flags) & IN_CLEANING) && ((ip)->i_flag & IN_CLEANING))	\
		--(ip)->i_lfs->lfs_uinodes;				\
	if (((flags) & IN_MODIFIED) && ((ip)->i_flag & IN_MODIFIED))	\
		--(ip)->i_lfs->lfs_uinodes;				\
	(ip)->i_flag &= ~(flags);					\
	if ((ip)->i_lfs->lfs_uinodes < 0) {				\
		panic("lfs_uinodes < 0");				\
	}								\
} while (0)

#define LFS_ITIMES(ip, acc, mod, cre) \
	while ((ip)->i_flag & (IN_ACCESS | IN_CHANGE | IN_UPDATE | IN_MODIFY)) \
		lfs_itimes(ip, acc, mod, cre)

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


/*
 * On-disk and in-memory checkpoint segment usage structure.
 */
typedef struct segusage SEGUSE;
struct segusage {
	u_int32_t su_nbytes;		/* 0: number of live bytes */
	u_int32_t su_olastmod;		/* 4: SEGUSE last modified timestamp */
	u_int16_t su_nsums;		/* 8: number of summaries in segment */
	u_int16_t su_ninos;		/* 10: number of inode blocks in seg */

#define	SEGUSE_ACTIVE		0x01	/*  segment currently being written */
#define	SEGUSE_DIRTY		0x02	/*  segment has data in it */
#define	SEGUSE_SUPERBLOCK	0x04	/*  segment contains a superblock */
#define SEGUSE_ERROR		0x08	/*  cleaner: do not clean segment */
#define SEGUSE_EMPTY		0x10	/*  segment is empty */
#define SEGUSE_INVAL		0x20	/*  segment is invalid */
	u_int32_t su_flags;		/* 12: segment flags */
	u_int64_t su_lastmod;		/* 16: last modified timestamp */
};

typedef struct segusage_v1 SEGUSE_V1;
struct segusage_v1 {
	u_int32_t su_nbytes;		/* 0: number of live bytes */
	u_int32_t su_lastmod;		/* 4: SEGUSE last modified timestamp */
	u_int16_t su_nsums;		/* 8: number of summaries in segment */
	u_int16_t su_ninos;		/* 10: number of inode blocks in seg */
	u_int32_t su_flags;		/* 12: segment flags  */
};

#define	SEGUPB(fs)	(fs->lfs_sepb)
#define	SEGTABSIZE_SU(fs)						\
	(((fs)->lfs_nseg + SEGUPB(fs) - 1) / (fs)->lfs_sepb)

#ifdef _KERNEL
# define SHARE_IFLOCK(F) 						\
  do {									\
	rw_enter(&(F)->lfs_iflock, RW_READER);				\
  } while(0)
# define UNSHARE_IFLOCK(F)						\
  do {									\
	rw_exit(&(F)->lfs_iflock);					\
  } while(0)
#else /* ! _KERNEL */
# define SHARE_IFLOCK(F)
# define UNSHARE_IFLOCK(F)
#endif /* ! _KERNEL */

/* Read in the block with a specific segment usage entry from the ifile. */
#define	LFS_SEGENTRY(SP, F, IN, BP) do {				\
	int _e;								\
	SHARE_IFLOCK(F);						\
	VTOI((F)->lfs_ivnode)->i_flag |= IN_ACCESS;			\
	if ((_e = bread((F)->lfs_ivnode,				\
	    ((IN) / (F)->lfs_sepb) + (F)->lfs_cleansz,			\
	    (F)->lfs_bsize, NOCRED, 0, &(BP))) != 0)			\
		panic("lfs: ifile read: %d", _e);			\
	if ((F)->lfs_version == 1)					\
		(SP) = (SEGUSE *)((SEGUSE_V1 *)(BP)->b_data +		\
			((IN) & ((F)->lfs_sepb - 1)));			\
	else								\
		(SP) = (SEGUSE *)(BP)->b_data + ((IN) % (F)->lfs_sepb);	\
	UNSHARE_IFLOCK(F);						\
} while (0)

#define LFS_WRITESEGENTRY(SP, F, IN, BP) do {				\
	if ((SP)->su_nbytes == 0)					\
		(SP)->su_flags |= SEGUSE_EMPTY;				\
	else								\
		(SP)->su_flags &= ~SEGUSE_EMPTY;			\
	(F)->lfs_suflags[(F)->lfs_activesb][(IN)] = (SP)->su_flags;	\
	LFS_BWRITE_LOG(BP);						\
} while (0)

/*
 * On-disk file information.  One per file with data blocks in the segment.
 */
typedef struct finfo FINFO;
struct finfo {
	u_int32_t fi_nblocks;		/* number of blocks */
	u_int32_t fi_version;		/* version number */
	u_int32_t fi_ino;		/* inode number */
	u_int32_t fi_lastlength;	/* length of last block in array */
	int32_t	  fi_blocks[1];		/* array of logical block numbers */
};
/* sizeof FINFO except fi_blocks */
#define	FINFOSIZE	(sizeof(FINFO) - sizeof(int32_t))

/*
 * Index file inode entries.
 */
typedef struct ifile IFILE;
struct ifile {
	u_int32_t if_version;		/* inode version number */
#define	LFS_UNUSED_DADDR	0	/* out-of-band daddr */
	int32_t	  if_daddr;		/* inode disk address */
#define LFS_ORPHAN_NEXTFREE	(~(u_int32_t)0) /* indicate orphaned file */
	u_int32_t if_nextfree;		/* next-unallocated inode */
	u_int32_t if_atime_sec;		/* Last access time, seconds */
	u_int32_t if_atime_nsec;	/* and nanoseconds */
};

typedef struct ifile_v1 IFILE_V1;
struct ifile_v1 {
	u_int32_t if_version;		/* inode version number */
	int32_t	  if_daddr;		/* inode disk address */
	u_int32_t if_nextfree;		/* next-unallocated inode */
#if LFS_ATIME_IFILE
	struct timespec if_atime;	/* Last access time */
#endif
};

/*
 * LFSv1 compatibility code is not allowed to touch if_atime, since it
 * may not be mapped!
 */
/* Read in the block with a specific inode from the ifile. */
#define	LFS_IENTRY(IP, F, IN, BP) do {					\
	int _e;								\
	SHARE_IFLOCK(F);						\
	VTOI((F)->lfs_ivnode)->i_flag |= IN_ACCESS;			\
	if ((_e = bread((F)->lfs_ivnode,				\
	(IN) / (F)->lfs_ifpb + (F)->lfs_cleansz + (F)->lfs_segtabsz,	\
	(F)->lfs_bsize, NOCRED, 0, &(BP))) != 0)			\
		panic("lfs: ifile ino %d read %d", (int)(IN), _e);	\
	if ((F)->lfs_version == 1)					\
		(IP) = (IFILE *)((IFILE_V1 *)(BP)->b_data +		\
				 (IN) % (F)->lfs_ifpb);			\
	else								\
		(IP) = (IFILE *)(BP)->b_data + (IN) % (F)->lfs_ifpb;	\
	UNSHARE_IFLOCK(F);						\
} while (0)

/*
 * Cleaner information structure.  This resides in the ifile and is used
 * to pass information from the kernel to the cleaner.
 */
typedef struct _cleanerinfo {
	u_int32_t clean;		/* number of clean segments */
	u_int32_t dirty;		/* number of dirty segments */
	int32_t   bfree;		/* disk blocks free */
	int32_t	  avail;		/* disk blocks available */
	u_int32_t free_head;		/* head of the inode free list */
	u_int32_t free_tail;		/* tail of the inode free list */
#define LFS_CLEANER_MUST_CLEAN	0x01
	u_int32_t flags;		/* status word from the kernel */
} CLEANERINFO;

#define	CLEANSIZE_SU(fs)						\
	((sizeof(CLEANERINFO) + (fs)->lfs_bsize - 1) >> (fs)->lfs_bshift)

/* Read in the block with the cleaner info from the ifile. */
#define LFS_CLEANERINFO(CP, F, BP) do {					\
	SHARE_IFLOCK(F);						\
	VTOI((F)->lfs_ivnode)->i_flag |= IN_ACCESS;			\
	if (bread((F)->lfs_ivnode,					\
	    (daddr_t)0, (F)->lfs_bsize, NOCRED, 0, &(BP)))		\
		panic("lfs: ifile read");				\
	(CP) = (CLEANERINFO *)(BP)->b_data;				\
	UNSHARE_IFLOCK(F);						\
} while (0)

/*
 * Synchronize the Ifile cleaner info with current avail and bfree.
 */
#define LFS_SYNC_CLEANERINFO(cip, fs, bp, w) do {		 	\
    mutex_enter(&lfs_lock);						\
    if ((w) || (cip)->bfree != (fs)->lfs_bfree ||		 	\
	(cip)->avail != (fs)->lfs_avail - (fs)->lfs_ravail - 		\
	(fs)->lfs_favail) {	 					\
	(cip)->bfree = (fs)->lfs_bfree;				 	\
	(cip)->avail = (fs)->lfs_avail - (fs)->lfs_ravail -		\
		(fs)->lfs_favail;				 	\
	if (((bp)->b_flags & B_GATHERED) == 0) {		 	\
		(fs)->lfs_flags |= LFS_IFDIRTY;			 	\
	}								\
	mutex_exit(&lfs_lock);						\
	(void) LFS_BWRITE_LOG(bp); /* Ifile */			 	\
    } else {							 	\
	mutex_exit(&lfs_lock);						\
	brelse(bp, 0);						 	\
    }									\
} while (0)

/*
 * Get the head of the inode free list.
 * Always called with the segment lock held.
 */
#define LFS_GET_HEADFREE(FS, CIP, BP, FREEP) do {			\
	if ((FS)->lfs_version > 1) {					\
		LFS_CLEANERINFO((CIP), (FS), (BP));			\
		(FS)->lfs_freehd = (CIP)->free_head;			\
		brelse(BP, 0);						\
	}								\
	*(FREEP) = (FS)->lfs_freehd;					\
} while (0)

#define LFS_PUT_HEADFREE(FS, CIP, BP, VAL) do {				\
	(FS)->lfs_freehd = (VAL);					\
	if ((FS)->lfs_version > 1) {					\
		LFS_CLEANERINFO((CIP), (FS), (BP));			\
		(CIP)->free_head = (VAL);				\
		LFS_BWRITE_LOG(BP);					\
		mutex_enter(&lfs_lock);					\
		(FS)->lfs_flags |= LFS_IFDIRTY;				\
		mutex_exit(&lfs_lock);					\
	}								\
} while (0)

#define LFS_GET_TAILFREE(FS, CIP, BP, FREEP) do {			\
	LFS_CLEANERINFO((CIP), (FS), (BP));				\
	*(FREEP) = (CIP)->free_tail;					\
	brelse(BP, 0);							\
} while (0)

#define LFS_PUT_TAILFREE(FS, CIP, BP, VAL) do {				\
	LFS_CLEANERINFO((CIP), (FS), (BP));				\
	(CIP)->free_tail = (VAL);					\
	LFS_BWRITE_LOG(BP);						\
	mutex_enter(&lfs_lock);						\
	(FS)->lfs_flags |= LFS_IFDIRTY;					\
	mutex_exit(&lfs_lock);						\
} while (0)

/*
 * On-disk segment summary information
 */
typedef struct segsum_v1 SEGSUM_V1;
struct segsum_v1 {
	u_int32_t ss_sumsum;		/* 0: check sum of summary block */
	u_int32_t ss_datasum;		/* 4: check sum of data */
	u_int32_t ss_magic;		/* 8: segment summary magic number */
#define SS_MAGIC	0x061561
	int32_t	  ss_next;		/* 12: next segment */
	u_int32_t ss_create;		/* 16: creation time stamp */
	u_int16_t ss_nfinfo;		/* 20: number of file info structures */
	u_int16_t ss_ninos;		/* 22: number of inodes in summary */

#define	SS_DIROP	0x01		/* segment begins a dirop */
#define	SS_CONT		0x02		/* more partials to finish this write*/
#define	SS_CLEAN	0x04		/* written by the cleaner */
#define	SS_RFW		0x08		/* written by the roll-forward agent */
	u_int16_t ss_flags;		/* 24: used for directory operations */
	u_int16_t ss_pad;		/* 26: extra space */
	/* FINFO's and inode daddr's... */
};

typedef struct segsum SEGSUM;
struct segsum {
	u_int32_t ss_sumsum;		/* 0: check sum of summary block */
	u_int32_t ss_datasum;		/* 4: check sum of data */
	u_int32_t ss_magic;		/* 8: segment summary magic number */
	int32_t	  ss_next;		/* 12: next segment */
	u_int32_t ss_ident;		/* 16: roll-forward fsid */
#define ss_ocreate ss_ident /* ident is where create was in v1 */
	u_int16_t ss_nfinfo;		/* 20: number of file info structures */
	u_int16_t ss_ninos;		/* 22: number of inodes in summary */
	u_int16_t ss_flags;		/* 24: used for directory operations */
	u_int8_t  ss_pad[6];		/* 26: extra space */
	u_int64_t ss_serial;		/* 32: serial number */
	u_int64_t ss_create;		/* 40: time stamp */
	/* FINFO's and inode daddr's... */
};

#define SEGSUM_SIZE(fs) ((fs)->lfs_version == 1 ? sizeof(SEGSUM_V1) : sizeof(SEGSUM))


/*
 * On-disk super block.
 */
struct dlfs {
#define	       LFS_MAGIC       0x070162
	u_int32_t dlfs_magic;	  /* 0: magic number */
#define	       LFS_VERSION     2
	u_int32_t dlfs_version;	  /* 4: version number */

	u_int32_t dlfs_size;	  /* 8: number of blocks in fs (v1) */
				  /*	number of frags in fs (v2) */
	u_int32_t dlfs_ssize;	  /* 12: number of blocks per segment (v1) */
				  /*	 number of bytes per segment (v2) */
	u_int32_t dlfs_dsize;	  /* 16: number of disk blocks in fs */
	u_int32_t dlfs_bsize;	  /* 20: file system block size */
	u_int32_t dlfs_fsize;	  /* 24: size of frag blocks in fs */
	u_int32_t dlfs_frag;	  /* 28: number of frags in a block in fs */

/* Checkpoint region. */
	u_int32_t dlfs_freehd;	  /* 32: start of the free list */
	int32_t   dlfs_bfree;	  /* 36: number of free disk blocks */
	u_int32_t dlfs_nfiles;	  /* 40: number of allocated inodes */
	int32_t	  dlfs_avail;	  /* 44: blocks available for writing */
	int32_t	  dlfs_uinodes;	  /* 48: inodes in cache not yet on disk */
	int32_t	  dlfs_idaddr;	  /* 52: inode file disk address */
	u_int32_t dlfs_ifile;	  /* 56: inode file inode number */
	int32_t	  dlfs_lastseg;	  /* 60: address of last segment written */
	int32_t	  dlfs_nextseg;	  /* 64: address of next segment to write */
	int32_t	  dlfs_curseg;	  /* 68: current segment being written */
	int32_t	  dlfs_offset;	  /* 72: offset in curseg for next partial */
	int32_t	  dlfs_lastpseg;  /* 76: address of last partial written */
	u_int32_t dlfs_inopf;	  /* 80: v1: time stamp; v2: inodes per frag */
#define dlfs_otstamp dlfs_inopf

/* These are configuration parameters. */
	u_int32_t dlfs_minfree;	  /* 84: minimum percentage of free blocks */

/* These fields can be computed from the others. */
	u_int64_t dlfs_maxfilesize; /* 88: maximum representable file size */
	u_int32_t dlfs_fsbpseg;	    /* 96: fsb per segment */
	u_int32_t dlfs_inopb;	  /* 100: inodes per block */
	u_int32_t dlfs_ifpb;	  /* 104: IFILE entries per block */
	u_int32_t dlfs_sepb;	  /* 108: SEGUSE entries per block */
	u_int32_t dlfs_nindir;	  /* 112: indirect pointers per block */
	u_int32_t dlfs_nseg;	  /* 116: number of segments */
	u_int32_t dlfs_nspf;	  /* 120: number of sectors per fragment */
	u_int32_t dlfs_cleansz;	  /* 124: cleaner info size in blocks */
	u_int32_t dlfs_segtabsz;  /* 128: segment table size in blocks */
	u_int32_t dlfs_segmask;	  /* 132: calculate offset within a segment */
	u_int32_t dlfs_segshift;  /* 136: fast mult/div for segments */
	u_int32_t dlfs_bshift;	  /* 140: calc block number from file offset */
	u_int32_t dlfs_ffshift;	  /* 144: fast mult/div for frag from file */
	u_int32_t dlfs_fbshift;	  /* 148: fast mult/div for frag from block */
	u_int64_t dlfs_bmask;	  /* 152: calc block offset from file offset */
	u_int64_t dlfs_ffmask;	  /* 160: calc frag offset from file offset */
	u_int64_t dlfs_fbmask;	  /* 168: calc frag offset from block offset */
	u_int32_t dlfs_blktodb;	  /* 176: blktodb and dbtoblk shift constant */
	u_int32_t dlfs_sushift;	  /* 180: fast mult/div for segusage table */

	int32_t	  dlfs_maxsymlinklen; /* 184: max length of an internal symlink */
#define LFS_MIN_SBINTERVAL     5  /* minimum superblock segment spacing */
#define LFS_MAXNUMSB	       10 /* 188: superblock disk offsets */
	int32_t	   dlfs_sboffs[LFS_MAXNUMSB];

	u_int32_t dlfs_nclean;	  /* 228: Number of clean segments */
	u_char	  dlfs_fsmnt[MNAMELEN];	 /* 232: name mounted on */
#define LFS_PF_CLEAN 0x1
	u_int16_t dlfs_pflags;	  /* 322: file system persistent flags */
	int32_t	  dlfs_dmeta;	  /* 324: total number of dirty summaries */
	u_int32_t dlfs_minfreeseg; /* 328: segments not counted in bfree */
	u_int32_t dlfs_sumsize;	  /* 332: size of summary blocks */
	u_int64_t dlfs_serial;	  /* 336: serial number */
	u_int32_t dlfs_ibsize;	  /* 344: size of inode blocks */
	int32_t	  dlfs_start;	  /* 348: start of segment 0 */
	u_int64_t dlfs_tstamp;	  /* 352: time stamp */
#define LFS_44INODEFMT 0
#define LFS_MAXINODEFMT 0
	u_int32_t dlfs_inodefmt;  /* 360: inode format version */
	u_int32_t dlfs_interleave; /* 364: segment interleave */
	u_int32_t dlfs_ident;	  /* 368: per-fs identifier */
	u_int32_t dlfs_fsbtodb;	  /* 372: fsbtodb abd dbtodsb shift constant */
	u_int32_t dlfs_resvseg;   /* 376: segments reserved for the cleaner */
	int8_t	  dlfs_pad[128];  /* 380: round to 512 bytes */
/* Checksum -- last valid disk field. */
	u_int32_t dlfs_cksum;	  /* 508: checksum for superblock checking */
};

/* Type used for the inode bitmap */
typedef u_int32_t lfs_bm_t;

/*
 * Linked list of segments whose byte count needs updating following a
 * file truncation.
 */
struct segdelta {
	long segnum;
	size_t num;
	LIST_ENTRY(segdelta) list;
};

/*
 * In-memory super block.
 */
struct lfs {
	struct dlfs lfs_dlfs;		/* on-disk parameters */
#define lfs_magic lfs_dlfs.dlfs_magic
#define lfs_version lfs_dlfs.dlfs_version
#define lfs_size lfs_dlfs.dlfs_size
#define lfs_ssize lfs_dlfs.dlfs_ssize
#define lfs_dsize lfs_dlfs.dlfs_dsize
#define lfs_bsize lfs_dlfs.dlfs_bsize
#define lfs_fsize lfs_dlfs.dlfs_fsize
#define lfs_frag lfs_dlfs.dlfs_frag
#define lfs_freehd lfs_dlfs.dlfs_freehd
#define lfs_bfree lfs_dlfs.dlfs_bfree
#define lfs_nfiles lfs_dlfs.dlfs_nfiles
#define lfs_avail lfs_dlfs.dlfs_avail
#define lfs_uinodes lfs_dlfs.dlfs_uinodes
#define lfs_idaddr lfs_dlfs.dlfs_idaddr
#define lfs_ifile lfs_dlfs.dlfs_ifile
#define lfs_lastseg lfs_dlfs.dlfs_lastseg
#define lfs_nextseg lfs_dlfs.dlfs_nextseg
#define lfs_curseg lfs_dlfs.dlfs_curseg
#define lfs_offset lfs_dlfs.dlfs_offset
#define lfs_lastpseg lfs_dlfs.dlfs_lastpseg
#define lfs_otstamp lfs_dlfs.dlfs_inopf
#define lfs_inopf lfs_dlfs.dlfs_inopf
#define lfs_minfree lfs_dlfs.dlfs_minfree
#define lfs_maxfilesize lfs_dlfs.dlfs_maxfilesize
#define lfs_fsbpseg lfs_dlfs.dlfs_fsbpseg
#define lfs_inopb lfs_dlfs.dlfs_inopb
#define lfs_ifpb lfs_dlfs.dlfs_ifpb
#define lfs_sepb lfs_dlfs.dlfs_sepb
#define lfs_nindir lfs_dlfs.dlfs_nindir
#define lfs_nseg lfs_dlfs.dlfs_nseg
#define lfs_nspf lfs_dlfs.dlfs_nspf
#define lfs_cleansz lfs_dlfs.dlfs_cleansz
#define lfs_segtabsz lfs_dlfs.dlfs_segtabsz
#define lfs_segmask lfs_dlfs.dlfs_segmask
#define lfs_segshift lfs_dlfs.dlfs_segshift
#define lfs_bmask lfs_dlfs.dlfs_bmask
#define lfs_bshift lfs_dlfs.dlfs_bshift
#define lfs_ffmask lfs_dlfs.dlfs_ffmask
#define lfs_ffshift lfs_dlfs.dlfs_ffshift
#define lfs_fbmask lfs_dlfs.dlfs_fbmask
#define lfs_fbshift lfs_dlfs.dlfs_fbshift
#define lfs_blktodb lfs_dlfs.dlfs_blktodb
#define lfs_fsbtodb lfs_dlfs.dlfs_fsbtodb
#define lfs_sushift lfs_dlfs.dlfs_sushift
#define lfs_maxsymlinklen lfs_dlfs.dlfs_maxsymlinklen
#define lfs_sboffs lfs_dlfs.dlfs_sboffs
#define lfs_cksum lfs_dlfs.dlfs_cksum
#define lfs_pflags lfs_dlfs.dlfs_pflags
#define lfs_fsmnt lfs_dlfs.dlfs_fsmnt
#define lfs_nclean lfs_dlfs.dlfs_nclean
#define lfs_dmeta lfs_dlfs.dlfs_dmeta
#define lfs_minfreeseg lfs_dlfs.dlfs_minfreeseg
#define lfs_sumsize lfs_dlfs.dlfs_sumsize
#define lfs_serial lfs_dlfs.dlfs_serial
#define lfs_ibsize lfs_dlfs.dlfs_ibsize
#define lfs_start lfs_dlfs.dlfs_start
#define lfs_tstamp lfs_dlfs.dlfs_tstamp
#define lfs_inodefmt lfs_dlfs.dlfs_inodefmt
#define lfs_interleave lfs_dlfs.dlfs_interleave
#define lfs_ident lfs_dlfs.dlfs_ident
#define lfs_resvseg lfs_dlfs.dlfs_resvseg

/* These fields are set at mount time and are meaningless on disk. */
	struct segment *lfs_sp;		/* current segment being written */
	struct vnode *lfs_ivnode;	/* vnode for the ifile */
	u_int32_t  lfs_seglock;		/* single-thread the segment writer */
	pid_t	  lfs_lockpid;		/* pid of lock holder */
	lwpid_t	  lfs_locklwp;		/* lwp of lock holder */
	u_int32_t lfs_iocount;		/* number of ios pending */
	u_int32_t lfs_writer;		/* don't allow any dirops to start */
	u_int32_t lfs_dirops;		/* count of active directory ops */
	u_int32_t lfs_dirvcount;	/* count of VDIROP nodes in this fs */
	u_int32_t lfs_doifile;		/* Write ifile blocks on next write */
	u_int32_t lfs_nactive;		/* Number of segments since last ckp */
	int8_t	  lfs_fmod;		/* super block modified flag */
	int8_t	  lfs_ronly;		/* mounted read-only flag */
#define LFS_NOTYET  0x01
#define LFS_IFDIRTY 0x02
#define LFS_WARNED  0x04
#define LFS_UNDIROP 0x08
	int8_t	  lfs_flags;		/* currently unused flag */
	u_int16_t lfs_activesb;		/* toggle between superblocks */
	daddr_t	  lfs_sbactive;		/* disk address of current sb write */
	struct vnode *lfs_flushvp;	/* vnode being flushed */
	int lfs_flushvp_fakevref;	/* fake vref count for flushvp */
	struct vnode *lfs_unlockvp;	/* being inactivated in lfs_segunlock */
	u_int32_t lfs_diropwait;	/* # procs waiting on dirop flush */
	size_t lfs_devbsize;		/* Device block size */
	size_t lfs_devbshift;		/* Device block shift */
	krwlock_t lfs_fraglock;
	krwlock_t lfs_iflock;		/* Ifile lock */
	kcondvar_t lfs_stopcv;		/* Wrap lock */
	struct lwp *lfs_stoplwp;
	pid_t lfs_rfpid;		/* Process ID of roll-forward agent */
	int	  lfs_nadirop;		/* number of active dirop nodes */
	long	  lfs_ravail;		/* blocks pre-reserved for writing */
	long	  lfs_favail;		/* blocks pre-reserved for writing */
	res_t *lfs_resblk;		/* Reserved memory for pageout */
	TAILQ_HEAD(, inode) lfs_dchainhd; /* dirop vnodes */
	TAILQ_HEAD(, inode) lfs_pchainhd; /* paging vnodes */
#define LFS_RESHASH_WIDTH 17
	LIST_HEAD(, lfs_res_blk) lfs_reshash[LFS_RESHASH_WIDTH];
	int	  lfs_pdflush;		 /* pagedaemon wants us to flush */
	u_int32_t **lfs_suflags;	/* Segment use flags */
#ifdef _KERNEL
	struct pool lfs_clpool;		/* Pool for struct lfs_cluster */
	struct pool lfs_bpppool;	/* Pool for bpp */
	struct pool lfs_segpool;	/* Pool for struct segment */
#endif /* _KERNEL */
#define LFS_MAX_CLEANIND 64
	int32_t  lfs_cleanint[LFS_MAX_CLEANIND]; /* Active cleaning intervals */
	int 	 lfs_cleanind;		/* Index into intervals */
	int lfs_sleepers;		/* # procs sleeping this fs */
	int lfs_pages;			/* dirty pages blaming this fs */
	lfs_bm_t *lfs_ino_bitmap;	/* Inuse inodes bitmap */
	int lfs_nowrap;			/* Suspend log wrap */
	int lfs_wrappass;		/* Allow first log wrap requester to pass */
	int lfs_wrapstatus;		/* Wrap status */
	LIST_HEAD(, segdelta) lfs_segdhd;	/* List of pending trunc accounting events */
};

/* NINDIR is the number of indirects in a file system block. */
#define	NINDIR(fs)	((fs)->lfs_nindir)

/* INOPB is the number of inodes in a secondary storage block. */
#define	INOPB(fs)	((fs)->lfs_inopb)
/* INOPF is the number of inodes in a fragment. */
#define INOPF(fs)	((fs)->lfs_inopf)

#define	blksize(fs, ip, lbn) \
	(((lbn) >= NDADDR || (ip)->i_ffs1_size >= ((lbn) + 1) << (fs)->lfs_bshift) \
	    ? (fs)->lfs_bsize \
	    : (fragroundup(fs, blkoff(fs, (ip)->i_ffs1_size))))
#define	blkoff(fs, loc)		((int)((loc) & (fs)->lfs_bmask))
#define fragoff(fs, loc)    /* calculates (loc % fs->lfs_fsize) */ \
    ((int)((loc) & (fs)->lfs_ffmask))

#if defined (_KERNEL)
#define	fsbtodb(fs, b)		((b) << ((fs)->lfs_ffshift - DEV_BSHIFT))
#define	dbtofsb(fs, b)		((b) >> ((fs)->lfs_ffshift - DEV_BSHIFT))
#else
#define	fsbtodb(fs, b)		((b) << (fs)->lfs_fsbtodb)
#define	dbtofsb(fs, b)		((b) >> (fs)->lfs_fsbtodb)
#endif

#define	lblkno(fs, loc)		((loc) >> (fs)->lfs_bshift)
#define	lblktosize(fs, blk)	((blk) << (fs)->lfs_bshift)

#define fsbtob(fs, b)		((b) << (fs)->lfs_ffshift)
#define btofsb(fs, b)		((b) >> (fs)->lfs_ffshift)

#define numfrags(fs, loc)	/* calculates (loc / fs->lfs_fsize) */	\
	((loc) >> (fs)->lfs_ffshift)
#define blkroundup(fs, size)	/* calculates roundup(size, fs->lfs_bsize) */ \
	((off_t)(((size) + (fs)->lfs_bmask) & (~(fs)->lfs_bmask)))
#define fragroundup(fs, size)	/* calculates roundup(size, fs->lfs_fsize) */ \
	((off_t)(((size) + (fs)->lfs_ffmask) & (~(fs)->lfs_ffmask)))
#define fragstoblks(fs, frags)/* calculates (frags / fs->fs_frag) */ \
	((frags) >> (fs)->lfs_fbshift)
#define blkstofrags(fs, blks)	/* calculates (blks * fs->fs_frag) */ \
	((blks) << (fs)->lfs_fbshift)
#define fragnum(fs, fsb)	/* calculates (fsb % fs->lfs_frag) */	\
	((fsb) & ((fs)->lfs_frag - 1))
#define blknum(fs, fsb)		/* calculates rounddown(fsb, fs->lfs_frag) */ \
	((fsb) &~ ((fs)->lfs_frag - 1))
#define dblksize(fs, dp, lbn) \
	(((lbn) >= NDADDR || (dp)->di_size >= ((lbn) + 1) << (fs)->lfs_bshift)\
	    ? (fs)->lfs_bsize \
	    : (fragroundup(fs, blkoff(fs, (dp)->di_size))))

#define	segsize(fs)	((fs)->lfs_version == 1 ?	     		\
			   lblktosize((fs), (fs)->lfs_ssize) :		\
			   (fs)->lfs_ssize)
#define segtod(fs, seg) (((fs)->lfs_version == 1     ?	     		\
			   (fs)->lfs_ssize << (fs)->lfs_blktodb :	\
			   btofsb((fs), (fs)->lfs_ssize)) * (seg))
#define	dtosn(fs, daddr)	/* block address to segment number */	\
	((uint32_t)(((daddr) - (fs)->lfs_start) / segtod((fs), 1)))
#define sntod(fs, sn)		/* segment number to disk address */	\
	((daddr_t)(segtod((fs), (sn)) + (fs)->lfs_start))

/*
 * Structures used by lfs_bmapv and lfs_markv to communicate information
 * about inodes and data blocks.
 */
typedef struct block_info {
	u_int32_t bi_inode;		/* inode # */
	int32_t	bi_lbn;			/* logical block w/in file */
	int32_t	bi_daddr;		/* disk address of block */
	u_int64_t bi_segcreate;		/* origin segment create time */
	int	bi_version;		/* file version number */
	void	*bi_bp;			/* data buffer */
	int	bi_size;		/* size of the block (if fragment) */
} BLOCK_INFO;

/* Compatibility for 1.5 binaries */
typedef struct block_info_15 {
	u_int32_t bi_inode;		/* inode # */
	int32_t	bi_lbn;			/* logical block w/in file */
	int32_t	bi_daddr;		/* disk address of block */
	u_int32_t bi_segcreate;		/* origin segment create time */
	int	bi_version;		/* file version number */
	void	*bi_bp;			/* data buffer */
	int	bi_size;		/* size of the block (if fragment) */
} BLOCK_INFO_15;

/* In-memory description of a segment about to be written. */
struct segment {
	struct lfs	 *fs;		/* file system pointer */
	struct buf	**bpp;		/* pointer to buffer array */
	struct buf	**cbpp;		/* pointer to next available bp */
	struct buf	**start_bpp;	/* pointer to first bp in this set */
	struct buf	 *ibp;		/* buffer pointer to inode page */
	struct ufs1_dinode    *idp;          /* pointer to ifile dinode */
	struct finfo	 *fip;		/* current fileinfo pointer */
	struct vnode	 *vp;		/* vnode being gathered */
	void	 *segsum;		/* segment summary info */
	u_int32_t ninodes;		/* number of inodes in this segment */
	int32_t seg_bytes_left;		/* bytes left in segment */
	int32_t sum_bytes_left;		/* bytes left in summary block */
	u_int32_t seg_number;		/* number of this segment */
	int32_t *start_lbp;		/* beginning lbn for this set */

#define	SEGM_CKP	0x01		/* doing a checkpoint */
#define	SEGM_CLEAN	0x02		/* cleaner call; don't sort */
#define	SEGM_SYNC	0x04		/* wait for segment */
#define	SEGM_PROT	0x08		/* don't inactivate at segunlock */
#define SEGM_PAGEDAEMON	0x10		/* pagedaemon called us */
#define SEGM_WRITERD	0x20		/* LFS writed called us */
#define SEGM_FORCE_CKP	0x40		/* Force checkpoint right away */
	u_int16_t seg_flags;		/* run-time flags for this segment */
	u_int32_t seg_iocount;		/* number of ios pending */
	int	  ndupino;		/* number of duplicate inodes */
};

#ifdef _KERNEL
struct lfs_cluster {
	size_t bufsize;	       /* Size of kept data */
	struct buf **bpp;      /* Array of kept buffers */
	int bufcount;	       /* Number of kept buffers */
#define LFS_CL_MALLOC	0x00000001
#define LFS_CL_SHIFT	0x00000002
#define LFS_CL_SYNC	0x00000004
	u_int32_t flags;       /* Flags */
	struct lfs *fs;	       /* LFS that this belongs to */
	struct segment *seg;   /* Segment structure, for LFS_CL_SYNC */
};

/*
 * Splay tree containing block numbers allocated through lfs_balloc.
 */
struct lbnentry {
	SPLAY_ENTRY(lbnentry) entry;
	daddr_t lbn;
};
#endif /* _KERNEL */

/*
 * LFS inode extensions.
 */
struct lfs_inode_ext {
	off_t	  lfs_osize;		/* size of file on disk */
	u_int32_t lfs_effnblocks;  /* number of blocks when i/o completes */
	size_t	  lfs_fragsize[NDADDR]; /* size of on-disk direct blocks */
	TAILQ_ENTRY(inode) lfs_dchain;  /* Dirop chain. */
	TAILQ_ENTRY(inode) lfs_pchain;  /* Paging chain. */
#define LFSI_NO_GOP_WRITE 0x01
#define LFSI_DELETED      0x02
#define LFSI_WRAPBLOCK    0x04
#define LFSI_WRAPWAIT     0x08
	u_int32_t lfs_iflags;           /* Inode flags */
	daddr_t   lfs_hiblk;		/* Highest lbn held by inode */
#ifdef _KERNEL
	SPLAY_HEAD(lfs_splay, lbnentry) lfs_lbtree; /* Tree of balloc'd lbns */
	int	  lfs_nbtree;		/* Size of tree */
	LIST_HEAD(, segdelta) lfs_segdhd;
#endif
	int16_t	  lfs_odnlink;		/* on-disk nlink count for cleaner */
};
#define i_lfs_osize		inode_ext.lfs->lfs_osize
#define i_lfs_effnblks		inode_ext.lfs->lfs_effnblocks
#define i_lfs_fragsize		inode_ext.lfs->lfs_fragsize
#define i_lfs_dchain		inode_ext.lfs->lfs_dchain
#define i_lfs_pchain		inode_ext.lfs->lfs_pchain
#define i_lfs_iflags		inode_ext.lfs->lfs_iflags
#define i_lfs_hiblk		inode_ext.lfs->lfs_hiblk
#define i_lfs_lbtree		inode_ext.lfs->lfs_lbtree
#define i_lfs_nbtree		inode_ext.lfs->lfs_nbtree
#define i_lfs_segdhd		inode_ext.lfs->lfs_segdhd
#define i_lfs_odnlink		inode_ext.lfs->lfs_odnlink

/*
 * Macros for determining free space on the disk, with the variable metadata
 * of segment summaries and inode blocks taken into account.
 */
/* Estimate number of clean blocks not available for writing */
#define LFS_EST_CMETA(F) (int32_t)((((F)->lfs_dmeta *			     \
				     (int64_t)(F)->lfs_nclean) /	     \
				      ((F)->lfs_nseg - (F)->lfs_nclean)))

/* Estimate total size of the disk not including metadata */
#define LFS_EST_NONMETA(F) ((F)->lfs_dsize - (F)->lfs_dmeta - LFS_EST_CMETA(F))

/* Estimate number of blocks actually available for writing */
#define LFS_EST_BFREE(F) ((F)->lfs_bfree > LFS_EST_CMETA(F) ?		     \
			  (F)->lfs_bfree - LFS_EST_CMETA(F) : 0)

/* Amount of non-meta space not available to mortal man */
#define LFS_EST_RSVD(F) (int32_t)((LFS_EST_NONMETA(F) *			     \
				   (u_int64_t)(F)->lfs_minfree) /	     \
				  100)

/* Can credential C write BB blocks */
#define ISSPACE(F, BB, C)						\
	((((C) == NOCRED || kauth_cred_geteuid(C) == 0) &&		\
	  LFS_EST_BFREE(F) >= (BB)) ||					\
	 (kauth_cred_geteuid(C) != 0 && IS_FREESPACE(F, BB)))

/* Can an ordinary user write BB blocks */
#define IS_FREESPACE(F, BB)						\
	  (LFS_EST_BFREE(F) >= (BB) + LFS_EST_RSVD(F))

/*
 * The minimum number of blocks to create a new inode.  This is:
 * directory direct block (1) + NIADDR indirect blocks + inode block (1) +
 * ifile direct block (1) + NIADDR indirect blocks = 3 + 2 * NIADDR blocks.
 */
#define LFS_NRESERVE(F) (btofsb((F), (2 * NIADDR + 3) << (F)->lfs_bshift))

/* Statistics Counters */
struct lfs_stats {	/* Must match sysctl list in lfs_vfsops.h ! */
	u_int	segsused;
	u_int	psegwrites;
	u_int	psyncwrites;
	u_int	pcleanwrites;
	u_int	blocktot;
	u_int	cleanblocks;
	u_int	ncheckpoints;
	u_int	nwrites;
	u_int	nsync_writes;
	u_int	wait_exceeded;
	u_int	write_exceeded;
	u_int	flush_invoked;
	u_int	vflush_invoked;
	u_int	clean_inlocked;
	u_int	clean_vnlocked;
	u_int   segs_reclaimed;
};
#ifdef _KERNEL
extern struct lfs_stats lfs_stats;
#endif

/* Fcntls to take the place of the lfs syscalls */
struct lfs_fcntl_markv {
	BLOCK_INFO *blkiov;	/* blocks to relocate */
	int blkcnt;		/* number of blocks */
};

#define LFCNSEGWAITALL	_FCNR_FSPRIV('L', 14, struct timeval)
#define LFCNSEGWAIT	_FCNR_FSPRIV('L', 15, struct timeval)
#define LFCNBMAPV	_FCNRW_FSPRIV('L', 2, struct lfs_fcntl_markv)
#define LFCNMARKV	_FCNRW_FSPRIV('L', 3, struct lfs_fcntl_markv)
#define LFCNRECLAIM	 _FCNO_FSPRIV('L', 4)

struct lfs_fhandle {
	char space[28];	/* FHANDLE_SIZE_COMPAT (but used from userland too) */
};
#define LFCNREWIND       _FCNR_FSPRIV('L', 6, int)
#define LFCNINVAL        _FCNR_FSPRIV('L', 7, int)
#define LFCNRESIZE       _FCNR_FSPRIV('L', 8, int)
#define LFCNWRAPSTOP	 _FCNR_FSPRIV('L', 9, int)
#define LFCNWRAPGO	 _FCNR_FSPRIV('L', 10, int)
#define LFCNIFILEFH	 _FCNW_FSPRIV('L', 11, struct lfs_fhandle)
#define LFCNWRAPPASS	 _FCNR_FSPRIV('L', 12, int)
# define LFS_WRAP_GOING   0x0
# define LFS_WRAP_WAITING 0x1
#define LFCNWRAPSTATUS	 _FCNW_FSPRIV('L', 13, int)

/*
 * Compat.  Defined for kernel only.  Userland always uses
 * "the one true version".
 */
#ifdef _KERNEL
#include <compat/sys/time_types.h>

#define LFCNSEGWAITALL_COMPAT	 _FCNW_FSPRIV('L', 0, struct timeval50)
#define LFCNSEGWAIT_COMPAT	 _FCNW_FSPRIV('L', 1, struct timeval50)
#define LFCNIFILEFH_COMPAT	 _FCNW_FSPRIV('L', 5, struct lfs_fhandle)
#define LFCNIFILEFH_COMPAT2	 _FCN_FSPRIV(F_FSOUT, 'L', 11, 32)
#define LFCNWRAPSTOP_COMPAT	 _FCNO_FSPRIV('L', 9)
#define LFCNWRAPGO_COMPAT	 _FCNO_FSPRIV('L', 10)
#define LFCNSEGWAITALL_COMPAT_50 _FCNR_FSPRIV('L', 0, struct timeval50)
#define LFCNSEGWAIT_COMPAT_50	 _FCNR_FSPRIV('L', 1, struct timeval50)
#endif

#ifdef _KERNEL
/* XXX MP */
#define	LFS_SEGLOCK_HELD(fs) \
	((fs)->lfs_seglock != 0 &&					\
	 (fs)->lfs_lockpid == curproc->p_pid &&				\
	 (fs)->lfs_locklwp == curlwp->l_lid)
#endif /* _KERNEL */

/* Debug segment lock */
#ifdef notyet
# define ASSERT_SEGLOCK(fs) KASSERT(LFS_SEGLOCK_HELD(fs))
# define ASSERT_NO_SEGLOCK(fs) KASSERT(!LFS_SEGLOCK_HELD(fs))
# define ASSERT_DUNNO_SEGLOCK(fs)
# define ASSERT_MAYBE_SEGLOCK(fs)
#else /* !notyet */
# define ASSERT_DUNNO_SEGLOCK(fs) \
	DLOG((DLOG_SEG, "lfs func %s seglock wrong (%d)\n", __func__, \
		LFS_SEGLOCK_HELD(fs)))
# define ASSERT_SEGLOCK(fs) do {					\
	if (!LFS_SEGLOCK_HELD(fs)) {					\
		DLOG((DLOG_SEG, "lfs func %s seglock wrong (0)\n", __func__)); \
	}								\
} while(0)
# define ASSERT_NO_SEGLOCK(fs) do {					\
	if (LFS_SEGLOCK_HELD(fs)) {					\
		DLOG((DLOG_SEG, "lfs func %s seglock wrong (1)\n", __func__)); \
	}								\
} while(0)
# define ASSERT_MAYBE_SEGLOCK(x)
#endif /* !notyet */

__BEGIN_DECLS
void lfs_itimes(struct inode *, const struct timespec *,
    const struct timespec *, const struct timespec *);
__END_DECLS

#endif /* !_UFS_LFS_LFS_H_ */
