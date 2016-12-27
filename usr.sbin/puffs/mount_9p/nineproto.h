/*	$NetBSD: nineproto.h,v 1.1 2007/04/21 14:21:43 pooka Exp $	*/

/*
 * Copyright (c) 2007  Antti Kantee.  All Rights Reserved.
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

#ifndef PUFFS9P_9PROTO_H_
#define PUFFS9P_9PROTO_H_

#include <stdint.h>

#define P9PROTO_VERSION		"9P2000"

#define P9PROTO_T_VERSION	100
#define P9PROTO_R_VERSION	101
#define P9PROTO_T_AUTH		102
#define P9PROTO_R_AUTH		103
#define P9PROTO_T_ATTACH	104
#define P9PROTO_R_ATTACH	105
#define P9PROTO_T_ERROR		106
#define P9PROTO_R_ERROR		107
#define P9PROTO_T_FLUSH		108
#define P9PROTO_R_FLUSH		109
#define P9PROTO_T_WALK		110
#define P9PROTO_R_WALK		111
#define P9PROTO_T_OPEN		112
#define P9PROTO_R_OPEN		113
#define P9PROTO_T_CREATE	114
#define P9PROTO_R_CREATE	115
#define P9PROTO_T_READ		116
#define P9PROTO_R_READ		117
#define P9PROTO_T_WRITE		118
#define P9PROTO_R_WRITE		119
#define P9PROTO_T_CLUNK		120
#define P9PROTO_R_CLUNK		121
#define P9PROTO_T_REMOVE	122
#define P9PROTO_R_REMOVE	123
#define P9PROTO_T_STAT		124
#define P9PROTO_R_STAT		125
#define P9PROTO_T_WSTAT		126
#define P9PROTO_R_WSTAT		127
#define P9PROTO_MIN		9PROTO_T_VERSION
#define P9PROTO_MAX		9PROTO_R_MAX

#define P9PROTO_NOFID		(uint32_t)~0
#define P9PROTO_NOTAG		(uint16_t)~0

/* type field in a qid */
#define P9PROTO_QID_TYPE_DIR	0x80
#define P9PROTO_QID_TYPE_APPEND	0x40
#define P9PROTO_QID_TYPE_EXCL	0x20
#define P9PROTO_QID_TYPE_MOUNT	0x10
#define P9PROTO_QID_TYPE_AUTH	0x08

/* mode in open */
#define P9PROTO_OMODE_READ	0x00
#define P9PROTO_OMODE_WRITE	0x01
#define P9PROTO_OMODE_RDWR	0x02
#define P9PROTO_OMODE_EXEC	0x03
#define P9PROTO_OMODE_TRUNC	0x10
#define P9PROTO_OMODE_RMCLOSE	0x40

/* for creating directories */
#define P9PROTO_CPERM_DIR	0x80000000

/* stat non-values */
#define P9PROTO_STAT_NOVAL1	(uint8_t)~0
#define P9PROTO_STAT_NOVAL2	(uint16_t)~0
#define P9PROTO_STAT_NOVAL4	(uint32_t)~0
#define P9PROTO_STAT_NOVAL8	(uint64_t)~0

#endif /* PUFFS9P_PROTO_H_ */
