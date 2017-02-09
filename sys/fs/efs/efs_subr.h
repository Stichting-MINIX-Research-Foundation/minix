/*	$NetBSD: efs_subr.h,v 1.2 2007/07/04 19:24:09 rumble Exp $	*/

/*
 * Copyright (c) 2006 Stephen M. Rumble <rumble@ephemeral.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _FS_EFS_EFS_SUBR_H_
#define _FS_EFS_EFS_SUBR_H_

extern struct pool efs_inode_pool;

struct efs_extent_iterator {
	struct efs_inode       *exi_eip;
	off_t			exi_next;		/* next logical extent*/
	off_t			exi_dnext;		/* next direct extent */
	off_t			exi_innext;		/* next indirect ext. */
};

int32_t	efs_sb_checksum(struct efs_sb *, int);
int	efs_sb_validate(struct efs_sb *, const char **);
void	efs_locate_inode(ino_t, struct efs_sb *, uint32_t *, int *);
int	efs_read_inode(struct efs_mount *, ino_t, struct lwp *,
	    struct efs_dinode *);
void	efs_dextent_to_extent(struct efs_dextent *, struct efs_extent *);
void	efs_extent_to_dextent(struct efs_extent *, struct efs_dextent *);
int	efs_inode_lookup(struct efs_mount *, struct efs_inode *,
	    struct componentname *, ino_t *);
int	efs_bread(struct efs_mount *, uint32_t, struct lwp *, struct buf **);
void	efs_sync_inode_to_dinode(struct efs_inode *);
void	efs_sync_dinode_to_inode(struct efs_inode *);
void	efs_extent_iterator_init(struct efs_extent_iterator *,
	    struct efs_inode *, off_t);
int	efs_extent_iterator_next(struct efs_extent_iterator *,
	    struct efs_extent *);

#endif /* !_FS_EFS_EFS_SUBR_H_ */
