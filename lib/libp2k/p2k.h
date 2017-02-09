/*	$NetBSD: p2k.h,v 1.8 2011/03/21 16:41:27 pooka Exp $	*/

/*
 * Copyright (c) 2007-2009 Antti Kantee.  All Rights Reserved.
 *
 * Development of this software was supported by Google Summer of Code.
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

#ifndef _RUMP_P2K_H_
#define _RUMP_P2K_H_

#include <sys/types.h>

#include <rump/ukfs.h>

struct p2k_mount;

__BEGIN_DECLS

int p2k_run_fs(const char *, const char *, const char *, int,
	       void *, size_t, uint32_t);
int p2k_run_diskfs(const char *, const char *, struct ukfs_part *,
		   const char *, int, void *, size_t, uint32_t);

struct p2k_mount *p2k_init(uint32_t);
void		p2k_cancel(struct p2k_mount *, int);

int		p2k_setup_fs(struct p2k_mount *, const char *,
			     const char *, const char *, int, void *, size_t);
int		p2k_setup_diskfs(struct p2k_mount *, const char *, const char *,
				 struct ukfs_part *, const char *, int,
				 void *, size_t);
int		p2k_mainloop(struct p2k_mount *);

__END_DECLS

#endif /* _RUMP_P2K_H_ */
