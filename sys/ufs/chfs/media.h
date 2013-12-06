/*-
 * Copyright (c) 2010 Department of Software Engineering,
 *		      University of Szeged, Hungary
 * Copyright (C) 2009 Ferenc Havasi <havasi@inf.u-szeged.hu>
 * Copyright (C) 2009 Zoltan Sogor <weth@inf.u-szeged.hu>
 * Copyright (C) 2009 David Tengeri <dtengeri@inf.u-szeged.hu>
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

#ifndef __CHFS_MEDIA_H__
#define __CHFS_MEDIA_H__

#ifndef _LE_TYPES
#define _LE_TYPES
typedef uint16_t le16;
typedef uint32_t le32;
typedef uint64_t le64;
#endif	/* _LE_TYPES */

/* node types */
enum {
	CHFS_NODETYPE_VNODE = 1,	/* vnode information */
	CHFS_NODETYPE_DATA,			/* data node */
	CHFS_NODETYPE_DIRENT,		/* directory enrty */
	CHFS_NODETYPE_PADDING,		/* padding node */
};

#define CHFS_NODE_HDR_SIZE sizeof(struct chfs_flash_node_hdr)

/*
 * Max size we have to read to get all info.
 * It is max size of chfs_flash_dirent_node with max name length.
 */
#define CHFS_MAX_NODE_SIZE 299

/* This will identify CHfs nodes */
#define CHFS_FS_MAGIC_BITMASK 0x4AF1

/*
 * struct chfs_flash_node_hdr - 
 * node header, its members are same for all nodes, used at scan
 */
struct chfs_flash_node_hdr
{
	le16 magic;		/* filesystem magic */
	le16 type;		/* node type */
	le32 length;	/* length of node */
	le32 hdr_crc;	/* crc of the first 3 fields */
} __packed;

/* struct chfs_flash_vnode - vnode informations stored on flash */
struct chfs_flash_vnode
{
	le16 magic;		/* filesystem magic */
	le16 type;		/* node type (should be CHFS_NODETYPE_VNODE) */
	le32 length;	/* length of node */
	le32 hdr_crc;	/* crc of the first 3 fields  */
	le64 vno;		/* vnode number */
	le64 version;	/* version of node */
	le32 uid;		/* owner of file */
	le32 gid;		/* group of file */
	le32 mode;		/* permission of vnode */
	le32 dn_size;	/* size of written data */
	le32 atime;		/* last access time */
	le32 mtime;		/* last modification time */
	le32 ctime;		/* change time */
	le32 dsize;		/* NOT USED, backward compatibility */
	le32 node_crc;	/* crc of all the previous fields */
} __packed;

/* struct chfs_flash_data_node - data stored on flash */
struct chfs_flash_data_node
{
	le16 magic;			/* filesystem magic */
	le16 type;			/* node type (should be CHFS_NODETYPE_DATA) */
	le32 length;		/* length of vnode with data */
	le32 hdr_crc;		/* crc of the first 3 fields */
	le64 vno;			/* vnode number */
	le64 version;		/* version of node */
	le64 offset;		/* offset in the file */
	le32 data_length;	/* length of data */
	le32 data_crc;		/* crc of data*/
	le32 node_crc;		/* crc of full node */
	uint8_t  data[0];	/* data */
} __packed;

/*
 * struct chfs_flash_dirent_node -
 * directory entry information stored on flash
 */
struct chfs_flash_dirent_node
{
	le16 magic;			/* filesystem magic */
	le16 type;			/* node type (should be CHFS_NODETYPE_DIRENT) */
	le32 length;		/* length of node with name */
	le32 hdr_crc;		/* crc of the first 3 fields */
	le64 vno;			/* vnode number */
	le64 pvno;			/* parent's vnode number */
	le64 version;		/* version of node */
	le32 mctime;		/* */
	uint8_t nsize;		/* length of name */
	uint8_t dtype;		/* file type */
	uint8_t unused[2];	/* just for padding */
	le32 name_crc;		/* crc of name */
	le32 node_crc;		/* crc of full node */
	uint8_t  name[0];	/* name of directory entry */
} __packed;

/* struct chfs_flash_padding_node - spaceholder node on flash */
struct chfs_flash_padding_node
{
	le16 magic;		/* filesystem magic */
	le16 type;		/* node type (should be CHFS_NODETYPE_PADDING )*/
	le32 length;	/* length of node */
	le32 hdr_crc;	/* crc of the first 3 fields */
} __packed;

#endif /* __CHFS_MEDIA_H__ */
