/* $NetBSD: nilfs.h,v 1.5 2014/10/15 09:05:46 hannken Exp $ */

/*
 * Copyright (c) 2008, 2009 Reinoud Zandijk
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 */

#ifndef _FS_NILFS_NILFS_H_
#define _FS_NILFS_NILFS_H_

#include <sys/queue.h>
#include <sys/uio.h>
#include <sys/mutex.h>

#include <sys/bufq.h>
#include <sys/disk.h>
#include <sys/kthread.h>
#include <miscfs/genfs/genfs_node.h>
#include "nilfs_fs.h"


/* debug categories */
#define NILFS_DEBUG_VOLUMES		0x000001
#define NILFS_DEBUG_VFSCALL		0x000002
#define NILFS_DEBUG_CALL		0x000004
#define NILFS_DEBUG_LOCKING		0x000008
#define NILFS_DEBUG_NODE		0x000010
#define NILFS_DEBUG_LOOKUP		0x000020
#define NILFS_DEBUG_READDIR		0x000040
#define NILFS_DEBUG_TRANSLATE		0x000080
#define NILFS_DEBUG_STRATEGY		0x000100
#define NILFS_DEBUG_READ		0x000200
#define NILFS_DEBUG_WRITE		0x000400
#define NILFS_DEBUG_ATTR		0x001000
#define NILFS_DEBUG_EXTATTR		0x002000
#define NILFS_DEBUG_ALLOC		0x004000
#define NILFS_DEBUG_DIRHASH		0x010000
#define NILFS_DEBUG_NOTIMPL		0x020000
#define NILFS_DEBUG_SHEDULE		0x040000
#define NILFS_DEBUG_SYNC		0x100000
#define NILFS_DEBUG_PARANOIA		0x200000

extern int nilfs_verbose;

/* initial value of nilfs_verbose */
#define NILFS_DEBUGGING		0

#ifdef NILFS_DEBUG
#define DPRINTF(name, arg) { \
		if (nilfs_verbose & NILFS_DEBUG_##name) {\
			printf arg;\
		};\
	}
#define DPRINTFIF(name, cond, arg) { \
		if (nilfs_verbose & NILFS_DEBUG_##name) { \
			if (cond) printf arg;\
		};\
	}
#else
#define DPRINTF(name, arg) {}
#define DPRINTFIF(name, cond, arg) {}
#endif


/* Configuration values */
#define NILFS_INODE_HASHBITS 	10
#define NILFS_INODE_HASHSIZE	(1<<NILFS_INODE_HASHBITS)
#define NILFS_INODE_HASHMASK	(NILFS_INODE_HASHSIZE - 1)


/* readdir cookies */
#define NILFS_DIRCOOKIE_DOT 1


/* handies */
#define VFSTONILFS(mp)    ((struct nilfs_mount *)mp->mnt_data)


/* malloc pools */
MALLOC_DECLARE(M_NILFSMNT);
MALLOC_DECLARE(M_NILFSTEMP);

extern struct pool nilfs_node_pool;
struct nilfs_node;
struct nilfs_mount;


#define NILFS_MAXNAMLEN	255

/* structure and derivatives */
struct nilfs_mdt {
	uint32_t  entries_per_block;
	uint32_t  entries_per_group;
	uint32_t  blocks_per_group;
	uint32_t  groups_per_desc_block;	/* desc is super group */
	uint32_t  blocks_per_desc_block;	/* desc is super group */
};


/* all that is related to the nilfs itself */
struct nilfs_device {
	/* device info */
	struct vnode		*devvp;	
	struct mount		*vfs_mountp;
	int 			 refcnt;

	/* meta : super block etc. */
	uint64_t devsize;
	uint32_t blocksize;
	struct nilfs_super_block super, super2;
	struct nilfs_node	*dat_node;
	struct nilfs_node	*cp_node;
	struct nilfs_node	*su_node;

	/* segment usage */
	/* checkpoints   */

	/* dat structure and derivatives */
	struct nilfs_mdt	 dat_mdt;
	struct nilfs_mdt	 ifile_mdt;

	/* running values */
	int	 mount_state;	/* ? */
	uint64_t last_seg_seq;	/* current segment sequence number */
	uint64_t last_seg_num;	/* last segment                    */
	uint64_t next_seg_num;	/* next segment to fill            */
	uint64_t last_cno;	/* current checkpoint number       */
	struct nilfs_segment_summary last_segsum;
	struct nilfs_super_root      super_root;

	/* syncing and late allocation */
	int			 syncing;		/* are we syncing?   */
	/* XXX sync_cv on what mutex? */
	kcondvar_t 		 sync_cv;		/* sleeping on sync  */
	uint32_t		 uncomitted_bl;		/* for free space    */

	/* lists */
	STAILQ_HEAD(nilfs_mnts, nilfs_mount) mounts;
	SLIST_ENTRY(nilfs_device) next_device;
};

extern SLIST_HEAD(_nilfs_devices, nilfs_device) nilfs_devices;


/* a specific mountpoint; head or a checkpoint/snapshot */
struct nilfs_mount {
	struct mount		*vfs_mountp;
	struct nilfs_device	*nilfsdev;
	struct nilfs_args	 mount_args;		/* flags RO access */

	/* instance values */
	struct nilfs_node	*ifile_node;

	/* lists */
	STAILQ_ENTRY(nilfs_mount) next_mount;		/* in nilfs_device   */
};


/*
 * NILFS node describing a file/directory.
 *
 * BUGALERT claim node_mutex before reading/writing to prevent inconsistencies !
 */
struct nilfs_node {
	struct genfs_node	 i_gnode;		/* has to be first   */
	struct vnode		*vnode;			/* vnode associated  */
	struct nilfs_mount	*ump;
	struct nilfs_device	*nilfsdev;

	ino_t			 ino;
	struct nilfs_inode	 inode;			/* readin copy */
	struct dirhash		*dir_hash;		/* if VDIR */

	/* XXX do we need this lock? */
	kmutex_t		 node_mutex;
	kcondvar_t		 node_lock;		/* sleeping lock */
	char const		*lock_fname;
	int			 lock_lineno;

	/* misc */
	uint32_t		 i_flags;		/* associated flags  */
	struct lockf		*lockf;			/* lock list         */

	LIST_ENTRY(nilfs_node)	 hashchain;		/* inside hash line  */
};


/* misc. flags stored in i_flags (XXX needs cleaning up) */
#define	IN_ACCESS		0x0001	/* Inode access time update request  */
#define	IN_CHANGE		0x0002	/* Inode change time update request  */
#define	IN_UPDATE		0x0004	/* Inode was written to; update mtime*/
#define	IN_MODIFY		0x0008	/* Modification time update request  */
#define	IN_MODIFIED		0x0010	/* node has been modified */
#define	IN_ACCESSED		0x0020	/* node has been accessed */
#define	IN_RENAME		0x0040	/* node is being renamed. XXX ?? */
#define	IN_DELETED		0x0080	/* node is unlinked, no FID reference*/
#define	IN_LOCKED		0x0100	/* node is locked by condvar */
#define	IN_SYNCED		0x0200	/* node is being used by sync */
#define	IN_CALLBACK_ULK		0x0400	/* node will be unlocked by callback */
#define	IN_NODE_REBUILD		0x0800	/* node is rebuild */

#define IN_FLAGBITS \
	"\10\1IN_ACCESS\2IN_CHANGE\3IN_UPDATE\4IN_MODIFY\5IN_MODIFIED" \
	"\6IN_ACCESSED\7IN_RENAME\10IN_DELETED\11IN_LOCKED\12IN_SYNCED" \
	"\13IN_CALLBACK_ULK\14IN_NODE_REBUILD"

#endif /* !_FS_NILFS_NILFS_H_ */
