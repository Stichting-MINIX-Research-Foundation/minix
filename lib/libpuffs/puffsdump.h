/*	$NetBSD: puffsdump.h,v 1.15 2010/07/11 12:29:08 pooka Exp $	*/

/*
 * Copyright (c) 2006  Antti Kantee.  All Rights Reserved.
 *
 * Development of this software was supported by the Ulla Tuominen Foundation.
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

#ifndef _PUFFSDUMP_H_
#define _PUFFSDUMP_H_

/*
 * Note for callers outside of libpuffs: these are to be used only
 * for debug builds.  Interfaces are not guaranteed to remain stable.
 */

#include <fs/puffs/puffs_msgif.h>

void puffsdump_req(struct puffs_req *);
void puffsdump_rv(struct puffs_req *);
void puffsdump_cookie(puffs_cookie_t, const char *);
void puffsdump_cn(struct puffs_kcn *);

void puffsdump_readwrite(struct puffs_req *);
void puffsdump_readwrite_rv(struct puffs_req *);
void puffsdump_readdir(struct puffs_req *);
void puffsdump_readdir_rv(struct puffs_req *);
void puffsdump_lookup(struct puffs_req *);
void puffsdump_lookup_rv(struct puffs_req *);
void puffsdump_create(struct puffs_req *);
void puffsdump_create_rv(struct puffs_req *);
void puffsdump_open(struct puffs_req *);

void puffsdump_attr(struct puffs_req *);
void puffsdump_targ(struct puffs_req *);

extern const char *puffsdump_vfsop_revmap[];
extern const char *puffsdump_vnop_revmap[];
extern const char *puffsdump_errnot_revmap[];
extern const char *puffsdump_flush_revmap[];

extern size_t puffsdump_vfsop_count;
extern size_t puffsdump_vnop_count;
extern size_t puffsdump_errnot_count;
extern size_t puffsdump_flush_count;

#endif /* _PUFFSDUMP_H_ */
