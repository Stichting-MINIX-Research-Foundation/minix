/*	$NetBSD: ebh.h,v 1.1 2011/11/24 15:51:32 ahoka Exp $	*/

/*-
 * Copyright (c) 2010 Department of Software Engineering,
 *		      University of Szeged, Hungary
 * Copyright (c) 2010 David Tengeri <dtengeri@inf.u-szeged.hu>
 * Copyright (c) 2010 Adam Hoka <ahoka@NetBSD.org>
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

/*
 * ebh.h
 *
 *  Created on: 2009.11.03.
 *      Author: dtengeri
 */

#ifndef EBH_H_
#define EBH_H_

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

#include <dev/flash/flash.h>
#include <ufs/chfs/ebh_media.h>
#include <ufs/chfs/debug.h>
#include <ufs/chfs/ebh_misc.h>

/* Maximum retries when getting new PEB before exit with failure */
#define CHFS_MAX_GET_PEB_RETRIES 2

/**
 * LEB status
 *
 */
enum {
	EBH_LEB_UNMAPPED = -1,
	EBH_LEB_MAPPED,
	EBH_LEB_DIRTY,
	EBH_LEB_INVALID,
	EBH_LEB_ERASE,
	EBH_LEB_ERASED,
	EBH_LEB_FREE,
};

/**
 * EB header status
 */
enum {
	EBHDR_LEB_OK = 0,
	EBHDR_LEB_DIRTY,
	EBHDR_LEB_INVALIDATED,
	EBHDR_LEB_BADMAGIC,
	EBHDR_LEB_BADCRC,
	EBHDR_LEB_FREE,
	EBHDR_LEB_NO_HDR,
};

struct chfs_ebh;

/**
 * struct chfs_ltree_entry - an netry in the lock tree
 * @rb: RB-node of the tree
 * @lnr: logical eraseblock number
 * @users: counts the tasks that are using or want to use the eraseblock
 * @mutex: read/write mutex to lock the eraseblock
 */
struct chfs_ltree_entry {
	RB_ENTRY(chfs_ltree_entry) rb;
	int lnr;
	int users;
	krwlock_t mutex;
};

/* Generate structure for Lock tree's red-black tree */
RB_HEAD(ltree_rbtree, chfs_ltree_entry);


/**
 * struct chfs_scan_leb - scanning infomration about a physical eraseblock
 * @erase_cnt: erase counter
 * @pebnr: physical eraseblock number
 * @info: the status of the PEB's eraseblock header when NOR serial when NAND
 * @u.list: link in one of the eraseblock list
 * @u.rb: link in the used RB-tree of chfs_scan_info
 */
struct chfs_scan_leb {
	int erase_cnt;
	int pebnr;
	int lnr;
	uint64_t info;
	union {
		TAILQ_ENTRY(chfs_scan_leb) queue;
		RB_ENTRY(chfs_scan_leb) rb;
	} u;
};

TAILQ_HEAD(scan_leb_queue, chfs_scan_leb);
RB_HEAD(scan_leb_used_rbtree, chfs_scan_leb);



/**
 * struct chfs_scan_info - chfs scanning information
 * @corrupted: queue of corrupted physical eraseblocks
 * @free: queue of free physical eraseblocks
 * @erase: queue of the physical eraseblocks signed to erase
 * @erased: queue of physical eraseblocks that contain no header
 * @used: RB-tree of used PEBs describing by chfs_scan_leb
 * @sum_of_ec: summary of erase counters
 * @num_of_eb: number of free and used eraseblocks
 * @bad_peb_cnt: counter of bad eraseblocks
 *
 * This structure contains information about the scanning for further
 * processing.
 */
struct chfs_scan_info {
	struct scan_leb_queue corrupted;
	struct scan_leb_queue free;
	struct scan_leb_queue erase;
	struct scan_leb_queue erased;
	struct scan_leb_used_rbtree used;
	uint64_t sum_of_ec;
	int num_of_eb;
	int bad_peb_cnt;
};

/**
 * struct chfs_peb - PEB information for erasing and wear leveling
 * @erase_cnt: erase counter of the physical eraseblock
 * @pebnr: physical eraseblock number
 * @u.queue: link to the queue of the PEBs waiting for erase
 * @u.rb: link to the RB-tree to the free PEBs
 */
struct chfs_peb {
	int erase_cnt;
	int pebnr;
	union {
		TAILQ_ENTRY(chfs_peb) queue;
		RB_ENTRY(chfs_peb) rb;
	} u;
};

/* Generate queue and rb-tree structures. */
TAILQ_HEAD(peb_queue, chfs_peb);
RB_HEAD(peb_free_rbtree, chfs_peb);
RB_HEAD(peb_in_use_rbtree, chfs_peb);

/**
 * struct  chfs_eb_hdr - in-memory representation of eraseblock headers
 * @ec_hdr: erase counter header ob eraseblock
 * @u.nor_hdr: eraseblock header on NOR flash
 * @u.nand_hdr: eraseblock header on NAND flash
 */
struct  chfs_eb_hdr {
	struct chfs_eb_ec_hdr ec_hdr;
	union {
		struct chfs_nor_eb_hdr  nor_hdr;
		struct chfs_nand_eb_hdr nand_hdr;
	} u;
};

/*
 * struct chfs_ebh_ops - collection of operations which
 * 								 depends on flash type
 * *************************************************************************** *
 * Direct flash operations:
 *
 * @read_eb_hdr: read eraseblock header from media
 * @write_eb_hdr: write eraseblock header to media
 * @check_eb_hdr: validates eraseblock header
 * @mark_eb_hdr_dirty_flash: marks eraseblock dirty on flash
 * @invalidate_eb_hdr: invalidates eraseblock header
 * @mark_eb_hdr_free: marks eraseblock header free (after erase)
 * *************************************************************************** *
 * Scanning operations:
 *
 * @process_eb: process an eraseblock information at scan
 * *************************************************************************** *
 * Misc operations:
 *
 * @create_eb_hdr: creates an eraseblock header based on flash type
 * @calc_data_offs: calculates where the data starts
 */
struct chfs_ebh_ops {
	int (*read_eb_hdr)(struct chfs_ebh *ebh, int pebnr,
	    struct chfs_eb_hdr *ebhdr);
	int (*write_eb_hdr)(struct chfs_ebh *ebh, int pebnr,
	    struct chfs_eb_hdr *ebhdr);
	int (*check_eb_hdr)(struct chfs_ebh *ebh, void *buf);
	int (*mark_eb_hdr_dirty_flash)(struct chfs_ebh *ebh, int pebnr, int lid);
	int (*invalidate_eb_hdr)(struct chfs_ebh *ebh, int pebnr);
	int (*mark_eb_hdr_free)(struct chfs_ebh *ebh, int pebnr, int ec);

	int (*process_eb)(struct chfs_ebh *ebh, struct chfs_scan_info *si,
	    int pebnr, struct chfs_eb_hdr *ebhdr);

	int (*create_eb_hdr)(struct chfs_eb_hdr *ebhdr, int lnr);
	int (*calc_data_offs)(struct chfs_ebh *ebh, int pebnr, int offset);
};

/**
 * struct erase_thread - background thread for erasing
 * @thread: pointer to thread structure
 * @wakeup: conditional variable for sleeping if there isn't any job to do
 * @running: flag to signal a thread shutdown
 */
struct erase_thread {
	lwp_t *eth_thread;
	kcondvar_t eth_wakeup;
	bool eth_running;
};


/**
 * struct chfs_ebh - eraseblock handler descriptor
 * @mtd: mtd device descriptor
 * @eb_size: eraseblock size
 * @peb_nr: number of PEBs
 * @lmap: LEB to PEB mapping
 * @layout_map: the LEBs layout (NOT USED YET)
 * @ltree: the lock tree
 * @ltree_lock: protects the tree
 * @alc_mutex: serializes "atomic LEB change" operation
 * @free: RB-tree of the free easeblocks
 * @in_use: RB-tree of PEBs are in use
 * @to_erase: list of the PEBs waiting for erase
 * @fully_erased: list of PEBs that have been erased but don't have header
 * @erase_lock: list and tree lock for fully_erased and to_erase lists and
 *		for the free RB-tree
 * @bg_erase: background thread for eraseing PEBs.
 * @ops: collection of operations which depends on flash type
 * @max_serial: max serial number of eraseblocks, only used on NAND
 */
struct chfs_ebh {
	struct peb_free_rbtree free;
	struct peb_in_use_rbtree in_use;
	struct peb_queue to_erase;
	struct peb_queue fully_erased;
	struct erase_thread bg_erase;
	device_t flash_dev;
	const struct flash_interface *flash_if;
	struct chfs_ebh_ops *ops;
	uint64_t *max_serial;
	int *lmap;
	//int *layout_map;
	struct ltree_rbtree ltree;
	//struct mutex alc_mutex;
	kmutex_t ltree_lock;
	kmutex_t alc_mutex;
	kmutex_t erase_lock;
	size_t eb_size;
	size_t peb_nr;
	flash_size_t flash_size;
};

/**
 * struct chfs_erase_info_priv - private information for erase
 * @ebh: eraseblock handler
 * @peb: physical eraseblock information
 */
struct chfs_erase_info_priv {
	struct chfs_ebh *ebh;
	struct chfs_peb *peb;
};

/* ebh.c */

int ebh_open(struct chfs_ebh *ebh, dev_t dev);
int ebh_close(struct chfs_ebh *ebh);
int ebh_read_leb(struct chfs_ebh *ebh, int lnr, char *buf,
    uint32_t offset, size_t len, size_t *retlen);
int ebh_write_leb(struct chfs_ebh *ebh, int lnr, char *buf,
    uint32_t offset, size_t len, size_t *retlen);
int ebh_erase_leb(struct chfs_ebh *ebh, int lnr);
int ebh_map_leb(struct chfs_ebh *ebh, int lnr);
int ebh_unmap_leb(struct chfs_ebh *ebh, int lnr);
int ebh_is_mapped(struct chfs_ebh *ebh, int lnr);
int ebh_change_leb(struct chfs_ebh *ebh, int lnr, char *buf,
    size_t len, size_t *retlen);


#endif /* EBH_H_ */
