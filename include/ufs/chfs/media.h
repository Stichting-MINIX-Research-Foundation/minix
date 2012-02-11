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
#endif

/*****************************************************************************/
/*			File system specific structures			     */
/*****************************************************************************/

enum {
	CHFS_NODETYPE_VNODE = 1,
	CHFS_NODETYPE_DATA,
	CHFS_NODETYPE_DIRENT,
	CHFS_NODETYPE_PADDING,
};

//#define CHFS_NODE_HDR_SIZE 12 /* magic + type + length + hdr_crc */
#define CHFS_NODE_HDR_SIZE sizeof(struct chfs_flash_node_hdr)

/* Max size we have to read to get all info.
 * It is max size of chfs_flash_dirent_node with max name length.
 */
#define CHFS_MAX_NODE_SIZE 299

/* This will identify CHfs nodes */
#define CHFS_FS_MAGIC_BITMASK 0x4AF1

/**
 * struct chfs_flash_node_hdr - node header, its members are same for
 *				    all	nodes, used at scan
 * @magic: filesystem magic
 * @type: node type
 * @length: length of node
 * @hdr_crc: crc of the first 3 members
 */
struct chfs_flash_node_hdr
{
	le16 magic;
	le16 type;
	le32 length;
	le32 hdr_crc;
} __packed;

/**
 * struct chfs_flash_vnode - vnode informations stored on flash
 * @magic: filesystem magic
 * @type: node type (CHFS_NODETYPE_VNODE)
 * @length: length of node
 * @hdr_crc: crc of the first 3 members
 * @vno: vnode identifier id
 * @version: vnode's version number
 * @uid: owner of the file
 * @gid: group of file
 * @mode: permissions for vnode
 * @dn_size: size of written out data nodes
 * @atime: last access times
 * @mtime: last modification time
 * @ctime: change time
 * @dsize: size of the node's data
 * @node_crc: crc of full node
 */
struct chfs_flash_vnode
{
	le16 magic;		/*0 */
	le16 type;		/*2 */
	le32 length;		/*4 */
	le32 hdr_crc;		/*8 */
	le64 vno;		/*12*/
	le64 version;		/*20*/
	le32 uid;		/*28*/
	le32 gid;		/*32*/
	le32 mode;		/*36*/
	le32 dn_size;		/*40*/
	le32 atime;		/*44*/
	le32 mtime;		/*48*/
	le32 ctime;		/*52*/
	le32 dsize;		/*56*/
	le32 node_crc;		/*60*/
} __packed;

/**
 * struct chfs_flash_data_node - node informations of data stored on flash
 * @magic: filesystem magic
 * @type: node type (CHFS_NODETYPE_DATA)
 * @length: length of node with data
 * @hdr_crc: crc of the first 3 members
 * @vno: vnode identifier id
 * @version: vnode's version number
 * @offset: offset in the file where write begins
 * @data_length: length of data
 * @data_crc: crc of data
 * @node_crc: crc of full node
 * @data: array of data
 */
struct chfs_flash_data_node
{
	le16 magic;
	le16 type;
	le32 length;
	le32 hdr_crc;
	le64 vno;
	le64 version;
	le64 offset;
	le32 data_length;
	le32 data_crc;
	le32 node_crc;
	uint8_t  data[0];
} __packed;

/**
 * struct chfs_flash_dirent_node - vnode informations stored on flash
 * @magic: filesystem magic
 * @type: node type (CHFS_NODETYPE_DIRENT)
 * @length: length of node
 * @hdr_crc: crc of the first 3 members
 * @vno: vnode identifier id
 * @pvno: vnode identifier id of parent vnode
 * @version: vnode's version number
 * @mctime:
 * @nsize: length of name
 * @dtype: file type
 * @unused: just for padding
 * @name_crc: crc of name
 * @node_crc: crc of full node
 * @name: name of the directory entry
 */
struct chfs_flash_dirent_node
{
	le16 magic;
	le16 type;
	le32 length;
	le32 hdr_crc;
	le64 vno;
	le64 pvno;
	le64 version;
	le32 mctime;
	uint8_t nsize;
	uint8_t dtype;
	uint8_t unused[2];
	le32 name_crc;
	le32 node_crc;
	uint8_t  name[0];
} __packed;

/**
 * struct chfs_flash_padding_node - node informations of data stored on
 *					flash
 * @magic: filesystem magic
 * @type: node type (CHFS_NODETYPE_PADDING)
 * @length: length of node
 * @hdr_crc: crc of the first 3 members
 */
struct chfs_flash_padding_node
{
	le16 magic;
	le16 type;
	le32 length;
	le32 hdr_crc;
} __packed;

#endif /* __CHFS_MEDIA_H__ */
