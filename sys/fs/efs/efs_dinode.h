/*	$NetBSD: efs_dinode.h,v 1.2 2007/06/30 15:56:16 rumble Exp $	*/

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

/*
 * EFS on-disk inode format.
 *
 * See IRIX inode(4)
 */

#ifndef _FS_EFS_EFS_DINODE_H_
#define _FS_EFS_EFS_DINODE_H_

/*
 * Historical locations are always good.
 */
#define EFS_ROOTINO	((ino_t)2)

/*
 * EFS on-disk inode structure (128 bytes)
 *
 * [0] - NetBSD native uid_t is uint32_t.
 * [1] - NetBSD native gid_t is uint32_t.
 * [2] - NetBSD native off_t is int64_t.
 * [3] - See notes for di_u below for meanings of di_numextents.
 * [4] - Always 0 with EFS. Apparently it could take on other values when
 *	 used in conjunction with AFS.
 */

#define EFS_DIRECTEXTENTS 12
struct efs_dinode {
	uint16_t	di_mode;	/* 0:  file type and permissions */
	int16_t		di_nlink;	/* 2:  link count (minimum 2) */
	uint16_t	di_uid;		/* 4:  user ID [0] */
	uint16_t	di_gid;		/* 6:  group ID [1] */
	int32_t		di_size;	/* 8:  file size (in bytes) [2] */
	uint32_t	di_atime;	/* 12: file access time */
	uint32_t	di_mtime;	/* 16: file modification time */
	uint32_t	di_ctime;	/* 20: inode modification time */
	int32_t		di_gen;		/* 24: inode generation number */
	int16_t		di_numextents;	/* 28: number of extents in file [3] */
	uint8_t		di_version;	/* 30: inode version [4] */
	uint8_t		di_spare;	/* 31: unused */

	union {
		/*
		 * If di_numextents <= EFS_DIRECTEXTENTS, _di_extents contains
		 * direct extent descriptors. 
		 *
		 * else (di_numextents > EFS_DIRECTEXTENTS), _di_extents
		 * contains indirect extent descriptors.
		 *
		 * If indirect extents are being used, extents[0].ex_offset
		 * contains the number of indirect extents, i.e. the valid
		 * offsets in 'extents' are:
		 *     extents[0 ... (extents[0].ex_offset - 1)]
		 * It's not presently known if the ex_offset fields in
		 * extents[1 ... EFS_DIRECTEXTENTS] have any meaning.
		 */
		struct efs_dextent extents[EFS_DIRECTEXTENTS];

		/*
		 * If di_numextents == 0 and di_mode indicates a symlink, the
		 * symlink path is inlined into _di_symlink. Otherwise, the
		 * symlink exists in extents.
		 *
		 * Note that the symlink is stored without nul-termination,
		 * and di_size reflects this length.
		 */
		char symlink[sizeof(struct efs_dextent) * EFS_DIRECTEXTENTS];

		/*
		 * If di_numextents == 0 and di_mode indicates a character or
		 * block special file, the device tag is contained in _di_dev. 
		 *
		 * Note that IRIX moved from 16bit to 32bit dev_t's at some 
		 * point and a new field was added. It appears that when 32bit
		 * dev_t's are used, di_odev is set to 0xffff.
		 */
		struct {
			uint16_t dev_old; 
			uint32_t dev_new;
		} __packed dev;
	} di_u;
} __packed; 

#define di_extents	di_u.extents
#define di_symlink	di_u.symlink
#define di_odev		di_u.dev.dev_old
#define di_ndev		di_u.dev.dev_new

#define EFS_DINODE_SIZE		sizeof(struct efs_dinode)
#define EFS_DINODES_PER_BB	(EFS_BB_SIZE / EFS_DINODE_SIZE)

#define EFS_DINODE_ODEV_INVALID	(0xffff)
#define EFS_DINODE_ODEV_MAJ(_x)	(((_x) >>  8) & 0x7f)
#define EFS_DINODE_ODEV_MIN(_x)	(((_x) >>  0) & 0xff)
#define EFS_DINODE_NDEV_MAJ(_x)	(((_x) >> 18) & 0x1ff)
#define EFS_DINODE_NDEV_MIN(_x)	(((_x) >>  0) & 0x3ffff)

/* EFS file permissions. */
#define EFS_IEXEC	0000100		/* executable */
#define EFS_IWRITE	0000200		/* writable */
#define EFS_IREAD	0000400		/* readable */
#define EFS_ISVTX	0001000		/* sticky bit */
#define EFS_ISGID	0002000		/* setgid */
#define EFS_ISUID	0004000		/* setuid */

/* EFS file types. */
#define EFS_IFMT	0170000		/* file type mask */
#define EFS_IFIFO	0010000		/* named pipe */
#define EFS_IFCHR	0020000		/* character device */
#define EFS_IFDIR	0040000		/* directory */
#define EFS_IFBLK	0060000		/* block device */
#define EFS_IFREG	0100000		/* regular file */
#define EFS_IFLNK	0120000		/* symlink */
#define EFS_IFSOCK	0140000		/* UNIX domain socket */

#endif /* !_FS_EFS_EFS_DINODE_H_ */
