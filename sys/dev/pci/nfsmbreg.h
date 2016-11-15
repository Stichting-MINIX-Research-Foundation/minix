/*	$NetBSD: nfsmbreg.h,v 1.2 2007/12/04 15:58:11 xtraeme Exp $	*/
/*
 * Copyright (c) 2007 KIYOHARA Takashi
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
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */
#ifndef _NFSMB_H_
#define _NFSMB_H_

/* nForce 2/3/4 */
#define NFORCE_OLD_SMB1 	0x50
#define NFORCE_OLD_SMB2 	0x54
/* nForce MCPXX */
#define NFORCE_SMB1		0x20
#define NFORCE_SMB2		0x24

#define NFORCE_SMBBASE(x)	((x) & 0xfffc)
#define NFORCE_SMBSIZE		8

#define NFORCE_SMB_PROTOCOL	0x00
#define NFORCE_SMB_STATUS	0x01
#define NFORCE_SMB_ADDRESS	0x02
#define NFORCE_SMB_COMMAND	0x03
#define NFORCE_SMB_DATA		0x04		/* 32 data registers */
#define NFORCE_SMB_BCNT		0x24		/* number of data bytes */
#define NFORCE_SMB_ALRM_A	0x25		/* alarm address */
#define NFORCE_SMB_ALRM_D	0x26		/* 2 bytes alarm data */

#define NFORCE_SMB_PROTOCOL_WRITE		0x00
#define NFORCE_SMB_PROTOCOL_READ		0x01
#define NFORCE_SMB_PROTOCOL_QUICK		0x02
#define NFORCE_SMB_PROTOCOL_BYTE		0x04
#define NFORCE_SMB_PROTOCOL_BYTE_DATA		0x06
#define NFORCE_SMB_PROTOCOL_WORD_DATA		0x08
#define NFORCE_SMB_PROTOCOL_BLOCK_DATA		0x0a
#define NFORCE_SMB_PROTOCOL_PROC_CALL		0x0c
#define NFORCE_SMB_PROTOCOL_BLOCK_PROC_CALL	0x0d
#define NFORCE_SMB_PROTOCOL_I2C_BLOCK_DATA	0x4a
#define NFORCE_SMB_PROTOCOL_PEC			0x80

#define NFORCE_SMB_STATUS_DONE			0x80
#define NFORCE_SMB_STATUS_ALRM			0x40
#define NFORCE_SMB_STATUS_RES			0x20
#define NFORCE_SMB_STATUS_STATUS		0x1f

#endif	/* _NFSMB_H_ */
