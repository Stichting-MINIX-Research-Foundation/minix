/*	$NetBSD: rump_vfs_private.h,v 1.19 2015/06/08 12:16:47 pooka Exp $	*/

/*
 * Copyright (c) 2008 Antti Kantee.  All Rights Reserved.
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

#ifndef _SYS_RUMP_VFS_PRIVATE_H_
#define _SYS_RUMP_VFS_PRIVATE_H_

#include <sys/types.h>
#include <sys/conf.h>

void		rumpfs_init(void);

int		rump_devnull_init(void);

#define RUMPBLK_DEVMAJOR 197 /* from conf/majors, XXX: not via config yet */
#define RUMPBLK_SIZENOTSET ((uint64_t)-1)
int	rumpblk_register(const char *, devminor_t *, uint64_t, uint64_t);
int	rumpblk_deregister(const char *);
int	rumpblk_init(void);
void	rumpblk_fini(void);

void	rump_biodone(void *, size_t, int);

void	rump_vfs_builddevs(struct devsw_conv *, size_t numelem);

extern int	(*rump_vfs_makeonedevnode)(dev_t, const char *,
					   devmajor_t, devminor_t);
extern int	(*rump_vfs_makedevnodes)(dev_t, const char *, char,
					 devmajor_t, devminor_t, int);
extern int	(*rump_vfs_makesymlink)(const char *, const char *);
extern void	(*rump_vfs_drainbufs)(int);
extern void	(*rump_vfs_fini)(void);

#include <sys/mount.h>
#include <sys/vnode.h>
#include <rump/rump.h>

#define	RUMPFS_MAXNAMLEN	255

#include "rumpvfs_if_priv.h"

#endif /* _SYS_RUMP_VFS_PRIVATE_H_ */
