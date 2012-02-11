/*	$NetBSD: chfs.h,v 1.4 2011/11/28 12:50:07 ahoka Exp $	*/

/*-
 * Copyright (c) 2010 Department of Software Engineering,
 *		      University of Szeged, Hungary
 * Copyright (C) 2009 Ferenc Havasi <havasi@inf.u-szeged.hu>
 * Copyright (C) 2009 Zoltan Sogor <weth@inf.u-szeged.hu>
 * Copyright (C) 2009 David Tengeri <dtengeri@inf.u-szeged.hu>
 * Copyright (C) 2009 Tamas Toth <ttoth@inf.u-szeged.hu>
 * Copyright (C) 2010 Adam Hoka <ahoka@NetBSD.org>
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by the Department of Software Engineering, University of Szeged, Hungary
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef __CHFS_H__
#define __CHFS_H__

#if 0
#define DBG_MSG
#define DBG_MSG_GC
#endif

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/cdefs.h>
#include <sys/stdint.h>
#include <sys/types.h>
#include <sys/tree.h>
#include <sys/queue.h>
#include <sys/kmem.h>
#include <sys/endian.h>
#include <sys/rwlock.h>
#include <sys/condvar.h>
#include <sys/mutex.h>
#include <sys/kthread.h>
#include <sys/rbtree.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/hash.h>
#include <sys/module.h>
#include <sys/dirent.h>

#include <ufs/ufs/quota.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/dir.h>

/* XXX shouldnt be defined here, but needed by chfs_inode.h */
TAILQ_HEAD(chfs_dirent_list, chfs_dirent);

#include "chfs_pool.h"
#include "ebh.h"
#include "media.h"
#include "chfs_inode.h"

#ifndef MOUNT_CHFS
#define MOUNT_CHFS "chfs"
#endif

#define CHFS_ROOTINO ROOTINO    /* ROOTINO == 2 */

enum {
	VNO_STATE_UNCHECKED,	/* CRC checks not yet done */
	VNO_STATE_CHECKING,	/* CRC checks in progress */
	VNO_STATE_PRESENT,	/* In core */
	VNO_STATE_CHECKEDABSENT,/* Checked, cleared again */
	VNO_STATE_GC,		/* GCing a 'pristine' node */
	VNO_STATE_READING,	/* In read_inode() */
	VNO_STATE_CLEARING	/* In clear_inode() */
};

#define VNODECACHE_SIZE 128

#define MAX_READ_FREE(chmp) (((chmp)->chm_ebh)->eb_size / 8)
/* an eraseblock will be clean if its dirty size is smaller than this */
#define MAX_DIRTY_TO_CLEAN 255
#define VERY_DIRTY(chmp, size) ((size) >= (((chmp)->chm_ebh)->eb_size / 2))

#define CHFS_PAD(x) (((x)+3)&~3)

enum {
	CHFS_NODE_OK = 0,
	CHFS_NODE_BADMAGIC,
	CHFS_NODE_BADCRC,
	CHFS_NODE_BADNAMECRC
};

enum {
	CHFS_BLK_STATE_FREE = 100,
	CHFS_BLK_STATE_CLEAN,
	CHFS_BLK_STATE_PARTDIRTY,
	CHFS_BLK_STATE_ALLDIRTY
};

extern struct pool chfs_inode_pool;
extern const struct genfs_ops chfs_genfsops;

/**
 * struct chfs_node_ref - a reference to a node
 * @lnr: logical identifier of the eraseblock where the node is
 * @offset: offset int hte eraseblock where the node starts
 * @next: used at data and dirent nodes, it points to the next data node which
 * 		  belongs to the same vnode
 */
struct chfs_node_ref
{
	struct chfs_node_ref *nref_next;
	uint32_t nref_lnr;
	uint32_t nref_offset;
};

/* Constants  for allocating node refs */
#define REFS_BLOCK_LEN (255/sizeof(struct chfs_node_ref))
#define REF_EMPTY_NODE (UINT_MAX)
#define REF_LINK_TO_NEXT (UINT_MAX - 1)

enum {
	CHFS_NORMAL_NODE_MASK,
	CHFS_UNCHECKED_NODE_MASK,
	CHFS_OBSOLETE_NODE_MASK,
	CHFS_PRISTINE_NODE_MASK
};

#define CHFS_REF_FLAGS(ref)		((ref)->nref_offset & 3)
#define CHFS_REF_OBSOLETE(ref)	(((ref)->nref_offset & 3) == CHFS_OBSOLETE_NODE_MASK)
#define CHFS_MARK_REF_NORMAL(ref)					      \
	do {								      \
		(ref)->nref_offset = CHFS_GET_OFS((ref)->nref_offset) | CHFS_NORMAL_NODE_MASK; \
	} while(0)

#define CHFS_GET_OFS(ofs) (ofs & ~ 3)

static inline struct chfs_node_ref *
node_next(struct chfs_node_ref *nref)
{
	//dbg("node next: %u : %u\n", nref->nref_lnr, nref->nref_offset);
	nref++;
	//dbg("nref++: %u : %u\n", nref->nref_lnr, nref->nref_offset);

	if (nref->nref_lnr == REF_LINK_TO_NEXT) {
		//dbg("link to next\n");
		nref = nref->nref_next;
		if (!nref)
			return nref;
	}

	if (nref->nref_lnr == REF_EMPTY_NODE) {
		//dbg("empty\n");
		return NULL;
	}

	return nref;
}

/**
 * struct chfs_dirent - full representation of a directory entry
 */
struct chfs_dirent
{
	struct chfs_node_ref *nref;
//	struct chfs_dirent *next;
	TAILQ_ENTRY(chfs_dirent) fds;
	uint64_t version;
	ino_t vno;
	uint32_t nhash;
	enum vtype type;
	uint8_t  nsize;
	uint8_t  name[0];

	/* used by chfs_alloc_dirent and free counterpart */
//	size_t alloc_size;
};

struct chfs_tmp_dnode {
	struct chfs_full_dnode *node;
	uint64_t version;
	uint32_t data_crc;
	//uint32_t partial_crc;
	//uint16_t csize;
	uint16_t overlapped;
	struct chfs_tmp_dnode *next;
};

struct chfs_tmp_dnode_info {
	struct rb_node rb_node;
	struct chfs_tmp_dnode *tmpnode;
};

struct chfs_readinode_info {
	struct rb_tree tdi_root;
	struct chfs_tmp_dnode_info *mdata_tn;
	uint64_t highest_version;
	struct chfs_node_ref *latest_ref;
};

struct chfs_full_dnode {
	struct chfs_node_ref *nref;
	uint64_t ofs;
	uint32_t size;
	uint32_t frags;
};

struct chfs_node_frag {
	struct rb_node rb_node;
	struct chfs_full_dnode *node;
	uint32_t size;
	uint64_t ofs;
};

static inline struct chfs_node_frag *
frag_first(struct rb_tree *tree)
{
	struct chfs_node_frag *frag;

	frag = (struct chfs_node_frag *)RB_TREE_MIN(tree);

	return frag;
}

static inline struct chfs_node_frag *
frag_last(struct rb_tree *tree)
{
	struct chfs_node_frag *frag;

	frag = (struct chfs_node_frag *)RB_TREE_MAX(tree);

	return frag;
}

#define frag_next(tree, frag) (struct chfs_node_frag *)rb_tree_iterate(tree, frag, RB_DIR_RIGHT)
#define frag_prev(tree, frag) (struct chfs_node_frag *)rb_tree_iterate(tree, frag, RB_DIR_LEFT)


/* XXX hack
   #ifndef CHFS_FRAG_TREE
   #define CHFS_FRAG_TREE
   RB_HEAD(chfs_frag_tree, chfs_node_frag);
   #endif
*/

/* for prototypes, properly defined in chfs_inode.h */
//struct chfs_inode_ext;

/**
 * struct chfs_vnode_cache - in memory representation of a vnode
 * @v: pointer to the vnode info node
 * @dnode: pointer to the list of data nodes
 * @dirents: pointer to the list of directory entries
 * @vno_version: used only during scan, holds the current version number of
 * 				 chfs_flash_vnode
 * @scan_dirents: used only during scan, holds the full representation of
 * 				  directory entries of this vnode
 * @pvno: parent vnode number
 * @nlink: number of links to this vnode
 */
struct chfs_vnode_cache {
//	struct chfs_dirent *scan_dirents;
	void *p;
	struct chfs_dirent_list scan_dirents;

	struct chfs_node_ref *v;
	struct chfs_node_ref *dnode;
	struct chfs_node_ref *dirents;

	uint64_t *vno_version;
	uint64_t highest_version;

	uint8_t flags;
	uint16_t state;
	ino_t vno;
	ino_t pvno;
	struct chfs_vnode_cache* next;
	uint32_t nlink;
};

struct chfs_eraseblock
{
	uint32_t lnr;

	TAILQ_ENTRY(chfs_eraseblock) queue;
	//uint32_t bad_count;
	uint32_t unchecked_size;
	uint32_t used_size;
	uint32_t dirty_size;
	uint32_t free_size;
	uint32_t wasted_size;

	struct chfs_node_ref *first_node;
	struct chfs_node_ref *last_node;

	/* Next block to be garbage collected */
	struct chfs_node_ref *gc_node;
};

TAILQ_HEAD(chfs_eraseblock_queue, chfs_eraseblock);

#define ALLOC_NORMAL    0
#define ALLOC_DELETION	1
#define ALLOC_GC        2

struct garbage_collector_thread {
	lwp_t *gcth_thread;
	kcondvar_t gcth_wakeup;
	bool gcth_running;
};

#define CHFS_MP_FLAG_SCANNING 2
#define CHFS_MP_FLAG_BUILDING 4

/**
 * struct chfs_mount - CHFS main descriptor structure
 * @ebh: eraseblock handler
 * @fl_index: index of flash device in the flash layer
 * @fs_version: filesystem version descriptor
 * @gbl_version: global version number
 * @max_vno: max vnode id
 * @chm_lock_mountfields:
 * @vnocache_hash: hash table of vnode caches
 * @vnocache_lock:
 * @blocks: array of eraseblocks on flash
 * @chm_root: used to protect all fields
 * @free_size: free size on the flash
 * @dirty_size: dirtied size on flash
 * @unchecked_size: size of unchecked data on flash
 * @free_queue: queue of free eraseblocks
 * @clean_queue: queue of clean eraseblocks
 * @dirty_queue: queue of dirty eraseblocks
 * @very_dirty_queue: queue of very dirty eraseblocks
 * @erase_pending_queue: queue of eraseblocks waiting for erasing
 * @erasable_pending_wbuf_queue: queue of eraseblocks waiting for erasing and
 * 			      	 have data to write to them
 * @nextblock: next eraseblock to write to
 * @nr_free_blocks: number of free blocks on the free_queue
 * @nr_erasable_blocks: number of blocks that can be erased and are on the
 *			erasable_queue
 */
struct chfs_mount {
	struct mount *chm_fsmp;
	struct chfs_ebh *chm_ebh;
	int chm_fs_version;
	uint64_t chm_gbl_version;
	ino_t chm_max_vno;
	ino_t chm_checked_vno;
	unsigned int chm_flags;

	/* chm_lock_mountfields:
	 * Used to protect all the following fields. */
	kmutex_t chm_lock_mountfields;

	struct chfs_vnode_cache **chm_vnocache_hash;
	/* chm_lock_vnocache:
	 * Used to protect the vnode cache.
	 * If you have to lock chm_lock_mountfields and also chm_lock_vnocache,
	 * you must lock chm_lock_mountfields first. */
	kmutex_t chm_lock_vnocache;

	struct chfs_eraseblock *chm_blocks;

	struct chfs_node *chm_root;

	uint32_t chm_free_size;
	uint32_t chm_dirty_size;
	uint32_t chm_unchecked_size;
	uint32_t chm_used_size;
	uint32_t chm_wasted_size;
	/* chm_lock_sizes:
	 * Used to protect the (free, used, etc.) sizes of the FS
	 * (and also the sizes of each eraseblock).
	 * If you have to lock chm_lock_mountfields and also chm_lock_sizes,
	 * you must lock chm_lock_mountfields first. */
	kmutex_t chm_lock_sizes;

	struct chfs_eraseblock_queue chm_free_queue;
	struct chfs_eraseblock_queue chm_clean_queue;
	struct chfs_eraseblock_queue chm_dirty_queue;
	struct chfs_eraseblock_queue chm_very_dirty_queue;
	struct chfs_eraseblock_queue chm_erasable_pending_wbuf_queue;
	struct chfs_eraseblock_queue chm_erase_pending_queue;

	uint8_t chm_resv_blocks_deletion;
	uint8_t chm_resv_blocks_write;
	uint8_t chm_resv_blocks_gctrigger;
	uint8_t chm_resv_blocks_gcmerge;
	uint8_t chm_nospc_dirty;

	uint8_t chm_vdirty_blocks_gctrigger;

	struct chfs_eraseblock *chm_nextblock;

	struct garbage_collector_thread chm_gc_thread;
	struct chfs_eraseblock *chm_gcblock;

	int chm_nr_free_blocks;
	int chm_nr_erasable_blocks;

	int32_t chm_fs_bmask;
	int32_t chm_fs_bsize;
	int32_t chm_fs_qbmask;
	int32_t chm_fs_bshift;
	int32_t chm_fs_fmask;
	int64_t chm_fs_qfmask;

	/* TODO will we use these? */
	unsigned int		chm_pages_max;
	unsigned int		chm_pages_used;
	unsigned int		chm_nodes_max;
	unsigned int		chm_nodes_cnt;
	struct chfs_pool	chm_dirent_pool;
	struct chfs_pool	chm_node_pool;
	struct chfs_str_pool	chm_str_pool;
	/**/

	size_t chm_wbuf_pagesize;
	unsigned char* chm_wbuf;
	size_t chm_wbuf_ofs;
	size_t chm_wbuf_len;
	/* chm_lock_wbuf:
	 * Used to protect the write buffer.
	 * If you have to lock chm_lock_mountfields and also chm_lock_wbuf,
	 * you must lock chm_lock_mountfields first. */
	krwlock_t chm_lock_wbuf;
};

/*
 * TODO we should move here all of these from the bottom of the file
 * Macros/functions to convert from generic data structures to chfs
 * specific ones.
 */

#define	CHFS_OFFSET_DOT		0
#define	CHFS_OFFSET_DOTDOT	1
#define CHFS_OFFSET_EOF		2
#define CHFS_OFFSET_FIRST	3


/*---------------------------------------------------------------------------*/

/* chfs_build.c */
void chfs_calc_trigger_levels(struct chfs_mount *);
int chfs_build_filesystem(struct chfs_mount *);
void chfs_build_set_vnodecache_nlink(struct chfs_mount *,
    struct chfs_vnode_cache *);
void chfs_build_remove_unlinked_vnode(struct chfs_mount *,
    struct chfs_vnode_cache *, struct chfs_dirent_list *);

/* chfs_scan.c */
int chfs_scan_eraseblock(struct chfs_mount *, struct chfs_eraseblock *);
struct chfs_vnode_cache *chfs_scan_make_vnode_cache(struct chfs_mount *,
    ino_t);
int chfs_scan_check_node_hdr(struct chfs_flash_node_hdr *);
int chfs_scan_check_vnode(struct chfs_mount *,
    struct chfs_eraseblock *, void *, off_t);
int chfs_scan_mark_dirent_obsolete(struct chfs_mount *,
    struct chfs_vnode_cache *, struct chfs_dirent *);
void chfs_add_fd_to_list(struct chfs_mount *,
    struct chfs_dirent *, struct chfs_vnode_cache *);
int chfs_scan_check_dirent_node(struct chfs_mount *,
    struct chfs_eraseblock *, void *, off_t);
int chfs_scan_check_data_node(struct chfs_mount *,
    struct chfs_eraseblock *, void *, off_t);
int chfs_scan_classify_cheb(struct chfs_mount *,
    struct chfs_eraseblock *);

/* chfs_nodeops.c */
int chfs_update_eb_dirty(struct chfs_mount *,
    struct chfs_eraseblock *, uint32_t);
void chfs_add_node_to_list(struct chfs_mount *, struct chfs_vnode_cache *,
    struct chfs_node_ref *, struct chfs_node_ref **);
void chfs_add_fd_to_inode(struct chfs_mount *,
    struct chfs_inode *, struct chfs_dirent *);
void chfs_add_vnode_ref_to_vc(struct chfs_mount *, struct chfs_vnode_cache *,
    struct chfs_node_ref *);
struct chfs_node_ref* chfs_nref_next(struct chfs_node_ref *);
int chfs_nref_len(struct chfs_mount *,
    struct chfs_eraseblock *, struct chfs_node_ref *);
int chfs_close_eraseblock(struct chfs_mount *,
    struct chfs_eraseblock *);
int chfs_reserve_space_normal(struct chfs_mount *, uint32_t, int);
int chfs_reserve_space_gc(struct chfs_mount *, uint32_t);
int chfs_reserve_space(struct chfs_mount *, uint32_t);
void chfs_mark_node_obsolete(struct chfs_mount *, struct chfs_node_ref *);

static inline struct chfs_vnode_cache *
chfs_nref_to_vc(struct chfs_node_ref *nref)
{
	while (nref->nref_next) {
		nref = nref->nref_next;
		//dbg("lnr: %u, ofs: %u\n", nref->nref_lnr, nref->nref_offset);
		//dbg("vno: %llu\n", ((struct chfs_vnode_cache *)(nref))->vno);
		//dbg("scan_dirents: %p\n", ((struct chfs_vnode_cache *)(nref))->scan_dirents);
		if (nref->nref_lnr == REF_LINK_TO_NEXT) {
			dbg("Link to next!\n");
		} else if (nref->nref_lnr == REF_EMPTY_NODE) {
			dbg("Empty!\n");
		}
	}
	//dbg("vno: %llu\n", ((struct chfs_vnode_cache *)(nref))->vno);

	//dbg("NREF_TO_VC: GET IT\n");
	//dbg("nref_next: %p, lnr: %u, ofs: %u\n", nref->nref_next, nref->nref_lnr, nref->nref_offset);
	struct chfs_vnode_cache *vc = (struct chfs_vnode_cache *) nref;
	dbg("vno: %ju, pvno: %ju, hv: %ju, nlink: %u\n", (intmax_t )vc->vno,
	    (intmax_t )vc->pvno, (intmax_t )vc->highest_version, vc->nlink);
	//return ((struct chfs_vnode_cache *)nref);
	return vc;
}


/* chfs_malloc.c */
int chfs_alloc_pool_caches(void);
void chfs_destroy_pool_caches(void);
struct chfs_vnode_cache* chfs_vnode_cache_alloc(ino_t);
void chfs_vnode_cache_free(struct chfs_vnode_cache *);
struct chfs_node_ref* chfs_alloc_node_ref(
	struct chfs_eraseblock *);
void chfs_free_node_refs(struct chfs_eraseblock *);
struct chfs_dirent* chfs_alloc_dirent(int);
void chfs_free_dirent(struct chfs_dirent *);
struct chfs_flash_vnode* chfs_alloc_flash_vnode(void);
void chfs_free_flash_vnode(struct chfs_flash_vnode *);
struct chfs_flash_dirent_node* chfs_alloc_flash_dirent(void);
void chfs_free_flash_dirent(struct chfs_flash_dirent_node *);
struct chfs_flash_data_node* chfs_alloc_flash_dnode(void);
void chfs_free_flash_dnode(struct chfs_flash_data_node *);
struct chfs_node_frag* chfs_alloc_node_frag(void);
void chfs_free_node_frag(struct chfs_node_frag *);
struct chfs_node_ref* chfs_alloc_refblock(void);
void chfs_free_refblock(struct chfs_node_ref *);
struct chfs_full_dnode* chfs_alloc_full_dnode(void);
void chfs_free_full_dnode(struct chfs_full_dnode *);
struct chfs_tmp_dnode * chfs_alloc_tmp_dnode(void);
void chfs_free_tmp_dnode(struct chfs_tmp_dnode *);
struct chfs_tmp_dnode_info * chfs_alloc_tmp_dnode_info(void);
void chfs_free_tmp_dnode_info(struct chfs_tmp_dnode_info *);

/* chfs_readinode.c */
int chfs_read_inode(struct chfs_mount *, struct chfs_inode *);
int chfs_read_inode_internal(struct chfs_mount *, struct chfs_inode *);
void chfs_kill_fragtree(struct rb_tree *);
uint32_t chfs_truncate_fragtree(struct chfs_mount *,
	struct rb_tree *, uint32_t);
int chfs_add_full_dnode_to_inode(struct chfs_mount *,
    struct chfs_inode *,
    struct chfs_full_dnode *);
int chfs_read_data(struct chfs_mount*, struct vnode *,
    struct buf *);

/* chfs_erase.c */
int chfs_remap_leb(struct chfs_mount *);

/* chfs_ihash.c */
void chfs_ihashinit(void);
void chfs_ihashreinit(void);
void chfs_ihashdone(void);
struct vnode *chfs_ihashlookup(dev_t, ino_t);
struct vnode *chfs_ihashget(dev_t, ino_t, int);
void chfs_ihashins(struct chfs_inode *);
void chfs_ihashrem(struct chfs_inode *);

extern kmutex_t	chfs_ihash_lock;
extern kmutex_t	chfs_hashlock;

/* chfs_gc.c */
void chfs_gc_trigger(struct chfs_mount *);
int chfs_gc_thread_should_wake(struct chfs_mount *);
void chfs_gc_thread(void *);
void chfs_gc_thread_start(struct chfs_mount *);
void chfs_gc_thread_stop(struct chfs_mount *);
int chfs_gcollect_pass(struct chfs_mount *);

/* chfs_vfsops.c*/
int chfs_gop_alloc(struct vnode *, off_t, off_t,  int, kauth_cred_t);
int chfs_mountfs(struct vnode *, struct mount *);

/* chfs_vnops.c */
extern int (**chfs_vnodeop_p)(void *);
extern int (**chfs_specop_p)(void *);
extern int (**chfs_fifoop_p)(void *);
int chfs_lookup(void *);
int chfs_create(void *);
int chfs_mknod(void *);
int chfs_open(void *);
int chfs_close(void *);
int chfs_access(void *);
int chfs_getattr(void *);
int chfs_setattr(void *);
int chfs_chown(struct vnode *, uid_t, gid_t, kauth_cred_t);
int chfs_chmod(struct vnode *, int, kauth_cred_t);
int chfs_read(void *);
int chfs_write(void *);
int chfs_fsync(void *);
int chfs_remove(void *);
int chfs_link(void *);
int chfs_rename(void *);
int chfs_mkdir(void *);
int chfs_rmdir(void *);
int chfs_symlink(void *);
int chfs_readdir(void *);
int chfs_readlink(void *);
int chfs_inactive(void *);
int chfs_reclaim(void *);
int chfs_advlock(void *);
int chfs_strategy(void *);
int chfs_bmap(void *);

/* chfs_vnode.c */
struct vnode *chfs_vnode_lookup(struct chfs_mount *, ino_t);
int chfs_readvnode(struct mount *, ino_t, struct vnode **);
int chfs_readdirent(struct mount *, struct chfs_node_ref *,
    struct chfs_inode *);
int chfs_makeinode(int, struct vnode *, struct vnode **,
    struct componentname *, int );
void chfs_set_vnode_size(struct vnode *, size_t);
void chfs_change_size_free(struct chfs_mount *,
	struct chfs_eraseblock *, int);
void chfs_change_size_dirty(struct chfs_mount *,
	struct chfs_eraseblock *, int);
void chfs_change_size_unchecked(struct chfs_mount *,
	struct chfs_eraseblock *, int);
void chfs_change_size_used(struct chfs_mount *,
	struct chfs_eraseblock *, int);
void chfs_change_size_wasted(struct chfs_mount *,
	struct chfs_eraseblock *, int);

/* chfs_vnode_cache.c */
struct chfs_vnode_cache **chfs_vnocache_hash_init(void);
void chfs_vnocache_hash_destroy(struct chfs_vnode_cache **);
void chfs_vnode_cache_set_state(struct chfs_mount *,
    struct chfs_vnode_cache *, int);
struct chfs_vnode_cache* chfs_vnode_cache_get(struct chfs_mount *, ino_t);
void chfs_vnode_cache_add(struct chfs_mount *, struct chfs_vnode_cache *);
void chfs_vnode_cache_remove(struct chfs_mount *, struct chfs_vnode_cache *);

/* chfs_wbuf.c */
int chfs_write_wbuf(struct chfs_mount*,
    const struct iovec *, long, off_t, size_t *);
int chfs_flush_pending_wbuf(struct chfs_mount *);

/* chfs_write.c */
int chfs_write_flash_vnode(struct chfs_mount *, struct chfs_inode *, int);
int chfs_write_flash_dirent(struct chfs_mount *, struct chfs_inode *,
    struct chfs_inode *, struct chfs_dirent *, ino_t, int);
int chfs_write_flash_dnode(struct chfs_mount *, struct vnode *,
    struct buf *, struct chfs_full_dnode *);
int chfs_do_link(struct chfs_inode *,
    struct chfs_inode *, const char *, int, enum vtype);
int chfs_do_unlink(struct chfs_inode *,
    struct chfs_inode *, const char *, int);

/* chfs_subr.c */
size_t chfs_mem_info(bool);
struct chfs_dirent * chfs_dir_lookup(struct chfs_inode *,
    struct componentname *);
int chfs_filldir (struct uio *, ino_t, const char *, int, enum vtype);
int chfs_chsize(struct vnode *, u_quad_t, kauth_cred_t);
int chfs_chflags(struct vnode *, int, kauth_cred_t);
void chfs_itimes(struct chfs_inode *, const struct timespec *,
    const struct timespec *, const struct timespec *);
int	chfs_update(struct vnode *, const struct timespec *,
    const struct timespec *, int);
//int	chfs_truncate(struct vnode *, off_t);

/*---------------------------------------------------------------------------*/

/* Some inline functions temporarily placed here */
static inline int
chfs_map_leb(struct chfs_mount *chmp, int lnr)
{
	int err;

	err = ebh_map_leb(chmp->chm_ebh, lnr);
	if (err)
		chfs_err("unmap leb %d failed, error: %d\n",lnr, err);

	return err;

}

static inline int
chfs_unmap_leb(struct chfs_mount *chmp, int lnr)
{
	int err;

	err = ebh_unmap_leb(chmp->chm_ebh, lnr);
	if (err)
		chfs_err("unmap leb %d failed, error: %d\n",lnr, err);

	return err;
}

static inline int
chfs_read_leb(struct chfs_mount *chmp, int lnr, char *buf,
    int offset, int len, size_t *retlen)
{
	int err;

	err = ebh_read_leb(chmp->chm_ebh, lnr, buf, offset, len, retlen);
	if (err)
		chfs_err("read leb %d:%d failed, error: %d\n",
		    lnr, offset, err);

	return err;
}

static inline int chfs_write_leb(struct chfs_mount *chmp, int lnr, char *buf,
    int offset, int len, size_t *retlen)
{
	int err;
	err = ebh_write_leb(chmp->chm_ebh, lnr, buf, offset, len, retlen);
	if (err)
		chfs_err("write leb %d:%d failed, error: %d\n",
		    lnr, offset, err);

	return err;
}

/******************************************************************************/
/* Code from dummyfs.h														  */
/******************************************************************************/
/* --------------------------------------------------------------------- */

#define CHFS_PAGES_RESERVED (4 * 1024 * 1024 / PAGE_SIZE)

static __inline size_t
CHFS_PAGES_MAX(struct chfs_mount *chmp)
{
	size_t freepages;

	freepages = chfs_mem_info(false);
	if (freepages < CHFS_PAGES_RESERVED)
		freepages = 0;
	else
		freepages -= CHFS_PAGES_RESERVED;

	return MIN(chmp->chm_pages_max, freepages + chmp->chm_pages_used);
}

#define	CHFS_ITIMES(ip, acc, mod, cre)				      \
	while ((ip)->iflag & (IN_ACCESS | IN_CHANGE | IN_UPDATE | IN_MODIFY)) \
		chfs_itimes(ip, acc, mod, cre)

/* used for KASSERTs */
#define IMPLIES(a, b) (!(a) || (b))
#define IFF(a, b) (IMPLIES(a, b) && IMPLIES(b, a))

#endif /* __CHFS_H__ */
