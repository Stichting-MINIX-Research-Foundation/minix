/*	$NetBSD: rump_syspuffs.h,v 1.1 2008/09/02 21:14:32 pooka Exp $	*/

/*
 * Copyright (c) 2008  Antti Kantee.  All Rights Reserved.
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

#ifndef _USR_SBIN_PUFFS_RUMP_SYSPUFFS_MOUNT_SYSPUFFS_H_
#define _USR_SBIN_PUFFS_RUMP_SYSPUFFS_MOUNT_SYSPUFFS_H_

#include <puffs.h>

struct syspuffs_args {
	struct puffs_kargs	us_kargs;
	int			us_pflags;
};

__BEGIN_DECLS

void	mount_syspuffs_parseargs(int, char **, struct syspuffs_args *,
				 int *, char *, char *);

__END_DECLS

#endif /* _USR_SBIN_PUFFS_RUMP_SYSPUFFS_MOUNT_SYSPUFFS_H_ */
