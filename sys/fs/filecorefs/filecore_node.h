/*	$NetBSD: filecore_node.h,v 1.6 2014/10/04 13:27:24 hannken Exp $	*/

/*-
 * Copyright (c) 1994 The Regents of the University of California.
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
 *	filecore_node.h		1.1	1998/6/26
 */

/*-
 * Copyright (c) 1998 Andrew McMurry
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *	filecore_node.h		1.1	1998/6/26
 */

#if !defined(_KERNEL)
#error not supposed to be exposed to userland.
#endif

#include <miscfs/genfs/genfs_node.h>

/*
 * In a future format, directories may be more than 2Gb in length,
 * however, in practice this seems unlikely. So, we define
 * the type doff_t as a long to keep down the cost of doing
 * lookup on a 32-bit machine. If you are porting to a 64-bit
 * architecture, you should make doff_t the same as off_t.
 */
#define doff_t	long

struct filecore_node {
	struct	genfs_node i_gnode;
	LIST_ENTRY(filecore_node) i_hash;
	struct	vnode *i_vnode;	/* vnode associated with this inode */
	struct	vnode *i_devvp;	/* vnode for block I/O */
	u_long	i_flag;		/* see below */
	dev_t	i_dev;		/* device where inode resides */
	ino_t	i_number;	/* the identity of the inode */
	daddr_t	i_block;	/* the disc address of the file */
	ino_t	i_parent;	/* the ino of the file's parent */

	struct	filecore_mnt *i_mnt;	/* filesystem associated with this inode */
	struct	lockf *i_lockf;	/* head of byte-level lock list */
        int	i_diroff;	/* offset in dir, where we found last entry */

	struct	filecore_direntry i_dirent; /* directory entry */
};

#define i_size		i_dirent.len

/* flags */
#define	IN_ACCESS	0x0020		/* inode access time to be updated */

#define VTOI(vp) ((struct filecore_node *)(vp)->v_data)
#define ITOV(ip) ((ip)->i_vnode)

#define filecore_staleinode(ip) ((ip)->i_dirent.name[0]==0)

/*
 * Prototypes for Filecore vnode operations
 */
int	filecore_lookup(void *);
#define	filecore_open		genfs_nullop
#define	filecore_close		genfs_nullop
int	filecore_access(void *);
int	filecore_getattr(void *);
int	filecore_read(void *);
#define	filecore_poll		genfs_poll
#define	filecore_mmap		genfs_mmap
#define	filecore_seek		genfs_seek
int	filecore_readdir(void *);
int	filecore_readlink(void *);
#define	filecore_abortop	genfs_abortop
int	filecore_inactive(void *);
int	filecore_reclaim(void *);
int	filecore_link(void *);
int	filecore_symlink(void *);
int	filecore_bmap(void *);
int	filecore_strategy(void *);
int	filecore_print(void *);
int	filecore_pathconf(void *);
int	filecore_blkatoff(void *);

mode_t	filecore_mode(struct filecore_node *);
struct timespec	filecore_time(struct filecore_node *);
ino_t	filecore_getparent(struct filecore_node *);
int	filecore_fn2unix(char *, char *, u_int16_t *);
int	filecore_fncmp(const char *, const char *, u_short);
int	filecore_dbread(struct filecore_node *, struct buf **);
