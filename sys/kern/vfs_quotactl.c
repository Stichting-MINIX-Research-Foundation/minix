/*	$NetBSD: vfs_quotactl.c,v 1.40 2014/06/28 22:27:50 dholland Exp $	*/
/*-
 * Copyright (c) 2012 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by David A. Holland.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: vfs_quotactl.c,v 1.40 2014/06/28 22:27:50 dholland Exp $$");

#include <sys/mount.h>
#include <sys/quotactl.h>

int
vfs_quotactl_stat(struct mount *mp, struct quotastat *info)
{
	struct quotactl_args args;

	args.qc_op = QUOTACTL_STAT;
	args.u.stat.qc_info = info;
	return VFS_QUOTACTL(mp, &args);
}

int
vfs_quotactl_idtypestat(struct mount *mp, int idtype,
    struct quotaidtypestat *info)
{
	struct quotactl_args args;

	args.qc_op = QUOTACTL_IDTYPESTAT;
	args.u.idtypestat.qc_idtype = idtype;
	args.u.idtypestat.qc_info = info;
	return VFS_QUOTACTL(mp, &args);
}

int
vfs_quotactl_objtypestat(struct mount *mp, int objtype,
    struct quotaobjtypestat *info)
{
	struct quotactl_args args;

	args.qc_op = QUOTACTL_OBJTYPESTAT;
	args.u.objtypestat.qc_objtype = objtype;
	args.u.objtypestat.qc_info = info;
	return VFS_QUOTACTL(mp, &args);
}

int
vfs_quotactl_get(struct mount *mp, const struct quotakey *key,
    struct quotaval *val)
{
	struct quotactl_args args;

	args.qc_op = QUOTACTL_GET;
	args.u.get.qc_key = key;
	args.u.get.qc_val = val;
	return VFS_QUOTACTL(mp, &args);
}

int
vfs_quotactl_put(struct mount *mp, const struct quotakey *key,
    const struct quotaval *val)
{
	struct quotactl_args args;

	args.qc_op = QUOTACTL_PUT;
	args.u.put.qc_key = key;
	args.u.put.qc_val = val;
	return VFS_QUOTACTL(mp, &args);
}

int
vfs_quotactl_del(struct mount *mp, const struct quotakey *key)
{
	struct quotactl_args args;

	args.qc_op = QUOTACTL_DEL;
	args.u.del.qc_key = key;
	return VFS_QUOTACTL(mp, &args);
}

int
vfs_quotactl_cursoropen(struct mount *mp, struct quotakcursor *cursor)
{
	struct quotactl_args args;

	args.qc_op = QUOTACTL_CURSOROPEN;
	args.u.cursoropen.qc_cursor = cursor;
	return VFS_QUOTACTL(mp, &args);
}

int
vfs_quotactl_cursorclose(struct mount *mp, struct quotakcursor *cursor)
{
	struct quotactl_args args;

	args.qc_op = QUOTACTL_CURSORCLOSE;
	args.u.cursorclose.qc_cursor = cursor;
	return VFS_QUOTACTL(mp, &args);
}

int
vfs_quotactl_cursorskipidtype(struct mount *mp, struct quotakcursor *cursor,
    int idtype)
{
	struct quotactl_args args;

	args.qc_op = QUOTACTL_CURSORSKIPIDTYPE;
	args.u.cursorskipidtype.qc_cursor = cursor;
	args.u.cursorskipidtype.qc_idtype = idtype;
	return VFS_QUOTACTL(mp, &args);
}

int
vfs_quotactl_cursorget(struct mount *mp, struct quotakcursor *cursor,
    struct quotakey *keys, struct quotaval *vals, unsigned maxnum,
    unsigned *ret)
{
	struct quotactl_args args;

	args.qc_op = QUOTACTL_CURSORGET;
	args.u.cursorget.qc_cursor = cursor;
	args.u.cursorget.qc_keys = keys;
	args.u.cursorget.qc_vals = vals;
	args.u.cursorget.qc_maxnum = maxnum;
	args.u.cursorget.qc_ret = ret;
	return VFS_QUOTACTL(mp, &args);
}

int
vfs_quotactl_cursoratend(struct mount *mp, struct quotakcursor *cursor,
    int *ret)
{
	struct quotactl_args args;

	args.qc_op = QUOTACTL_CURSORATEND;
	args.u.cursoratend.qc_cursor = cursor;
	args.u.cursoratend.qc_ret = ret;
	return VFS_QUOTACTL(mp, &args);
}

int
vfs_quotactl_cursorrewind(struct mount *mp, struct quotakcursor *cursor)
{
	struct quotactl_args args;

	args.qc_op = QUOTACTL_CURSORREWIND;
	args.u.cursorrewind.qc_cursor = cursor;
	return VFS_QUOTACTL(mp, &args);
}

int
vfs_quotactl_quotaon(struct mount *mp, int idtype, const char *path)
{
	struct quotactl_args args;

	args.qc_op = QUOTACTL_QUOTAON;
	args.u.quotaon.qc_idtype = idtype;
	args.u.quotaon.qc_quotafile = path;
	return VFS_QUOTACTL(mp, &args);
}

int
vfs_quotactl_quotaoff(struct mount *mp, int idtype)
{
	struct quotactl_args args;

	args.qc_op = QUOTACTL_QUOTAOFF;
	args.u.quotaoff.qc_idtype = idtype;
	return VFS_QUOTACTL(mp, &args);
}
