/*	$NetBSD: sftp_proto.h,v 1.2 2007/06/06 01:55:03 pooka Exp $	*/

/*
 * Copyright (c) 2006  Antti Kantee.  All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * copypaste from draft-ietf-secsh-filexfer-03.txt
 *
 * XXX: we only implement protocol version 03.  We should *definitely*
 * support a later version, since it maps to the vnode interface much
 * better.  the problem is that most sftp servers only implement v3
 * and I don't currently have the energy to deal with compat-fuddling
 * in the code.
 */

#ifndef PSSHFS_SFTPPROTO_H_
#define PSSHFS_SFTPPROTO_H_

/* 3 General Packet Format */

#define SSH_FXP_INIT		1
#define SSH_FXP_VERSION		2
#define SSH_FXP_OPEN		3
#define SSH_FXP_CLOSE		4
#define SSH_FXP_READ		5
#define SSH_FXP_WRITE		6
#define SSH_FXP_LSTAT		7
#define SSH_FXP_FSTAT		8
#define SSH_FXP_SETSTAT		9
#define SSH_FXP_FSETSTAT	10
#define SSH_FXP_OPENDIR		11
#define SSH_FXP_READDIR		12
#define SSH_FXP_REMOVE		13
#define SSH_FXP_MKDIR		14
#define SSH_FXP_RMDIR		15
#define SSH_FXP_REALPATH	16
#define SSH_FXP_STAT		17
#define SSH_FXP_RENAME		18
#define SSH_FXP_READLINK	19
#define SSH_FXP_SYMLINK		20

#define SSH_FXP_STATUS		101
#define SSH_FXP_HANDLE		102
#define SSH_FXP_DATA		103
#define SSH_FXP_NAME		104
#define SSH_FXP_ATTRS		105

#define SSH_FXP_EXTENDED	200
#define SSH_FXP_EXTENDED_REPLY	201

/* 5.1 Flags */

/* XXX: UIDGID is obsoleted *AND NOT VALID* for version 3 of the protocol */
#define SSH_FILEXFER_ATTR_SIZE		0x00000001
#define SSH_FILEXFER_ATTR_UIDGID	0x00000002
#define SSH_FILEXFER_ATTR_PERMISSIONS	0x00000004
#define SSH_FILEXFER_ATTR_ACCESSTIME	0x00000008
#define SSH_FILEXFER_ATTR_CREATETIME	0x00000010
#define SSH_FILEXFER_ATTR_MODIFYTIME	0x00000020
#define SSH_FILEXFER_ATTR_ACL		0x00000040
#define SSH_FILEXFER_ATTR_OWNERGROUP	0x00000080
#define SSH_FILEXFER_ATTR_EXTENDED	0x80000000 

/* 5.2 Type */

#define SSH_FILEXFER_TYPE_REGULAR	1
#define SSH_FILEXFER_TYPE_DIRECTORY	2
#define SSH_FILEXFER_TYPE_SYMLINK	3
#define SSH_FILEXFER_TYPE_SPECIAL	4
#define SSH_FILEXFER_TYPE_UNKNOWN	5


/* 6.3 Opening, Creating, and Closing Files */

#define SSH_FXF_READ	0x00000001
#define SSH_FXF_WRITE	0x00000002
#define SSH_FXF_APPEND	0x00000004
#define SSH_FXF_CREAT	0x00000008
#define SSH_FXF_TRUNC	0x00000010
#define SSH_FXF_EXCL	0x00000020
#define SSH_FXF_TEXT	0x00000040


/* 7. Responses from the Server to the Client */

#define SSH_FX_OK			0 
#define SSH_FX_EOF			1
#define SSH_FX_NO_SUCH_FILE		2
#define SSH_FX_PERMISSION_DENIED	3
#define SSH_FX_FAILURE			4
#define SSH_FX_BAD_MESSAGE		5
#define SSH_FX_NO_CONNECTION		6
#define SSH_FX_CONNECTION_LOST		7 
#define SSH_FX_OP_UNSUPPORTED		8  
#define SSH_FX_INVALID_HANDLE		9
#define SSH_FX_NO_SUCH_PATH		10
#define SSH_FX_FILE_ALREADY_EXISTS	11
#define SSH_FX_WRITE_PROTECT		12

#endif /* PSSHFS_SFTPPROTO_H_ */
