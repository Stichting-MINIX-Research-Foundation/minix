/*	$NetBSD: ffs_appleufs.c,v 1.15 2015/02/15 11:04:43 maxv Exp $	*/

/*
 * Copyright (c) 2002 Darrin B. Jewell
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
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ffs_appleufs.c,v 1.15 2015/02/15 11:04:43 maxv Exp $");

#include <sys/param.h>
#include <sys/time.h>
#if defined(_KERNEL)
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/cprng.h>
#endif

#include <ufs/ufs/dinode.h>
#include <ufs/ufs/ufs_bswap.h>
#include <ufs/ffs/fs.h>
#include <ufs/ffs/ffs_extern.h>

#if !defined(_KERNEL) && !defined(STANDALONE)
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#define KASSERT(x) assert(x)
#endif

/*
 * This is the same calculation as in_cksum.
 */
u_int16_t
ffs_appleufs_cksum(const struct appleufslabel *appleufs)
{
	const u_int16_t *p = (const u_int16_t *)appleufs;
	int len = APPLEUFS_LABEL_SIZE; /* sizeof(struct appleufslabel) */
	long res = 0;
	while (len > 1)  {
		res += *p++;
		len -= 2;
	}
#if 0 /* APPLEUFS_LABEL_SIZE is guaranteed to be even */
	if (len == 1)
		res += htobe16(*(u_char *)p<<8);
#endif
	res = (res >> 16) + (res & 0xffff);
	res += (res >> 16);
	return (~res);
}

/*
 * Copies o to n, validating and byteswapping along the way. Returns 0 if ok,
 * EINVAL if not valid.
 */
int
ffs_appleufs_validate(const char *name, const struct appleufslabel *o,
    struct appleufslabel *n)
{
	struct appleufslabel tmp;

	if (!n)
		n = &tmp;
	if (o->ul_magic != be32toh(APPLEUFS_LABEL_MAGIC))
		return EINVAL;

	*n = *o;
	n->ul_checksum = 0;
	n->ul_checksum = ffs_appleufs_cksum(n);
	n->ul_magic = be32toh(o->ul_magic);
	n->ul_version = be32toh(o->ul_version);
	n->ul_time = be32toh(o->ul_time);
	n->ul_namelen = be16toh(o->ul_namelen);

	if (n->ul_checksum != o->ul_checksum)
		return EINVAL;
	if (n->ul_namelen == 0)
		return EINVAL;
	if (n->ul_namelen > APPLEUFS_MAX_LABEL_NAME)
		n->ul_namelen = APPLEUFS_MAX_LABEL_NAME;

	n->ul_name[n->ul_namelen - 1] = '\0';

#ifdef DEBUG
	printf("%s: found APPLE UFS label v%d: \"%s\"\n", name,
	    n->ul_version, n->ul_name);
#endif
	n->ul_uuid = be64toh(o->ul_uuid);

	return 0;
}

void
ffs_appleufs_set(struct appleufslabel *appleufs, const char *name, time_t t,
    uint64_t uuid)
{
	size_t namelen;

	if (!name)
		name = "untitled";
	if (t == ((time_t)-1)) {
#if defined(_KERNEL)
		t = time_second;
#elif defined(STANDALONE)
		t = 0;
#else
		(void)time(&t);
#endif
	}
	if (uuid == 0) {
#if defined(_KERNEL) && !defined(STANDALONE)
		uuid = cprng_fast64();
#endif
	}
	namelen = strlen(name);
	if (namelen > APPLEUFS_MAX_LABEL_NAME)
		namelen = APPLEUFS_MAX_LABEL_NAME;
	memset(appleufs, 0, APPLEUFS_LABEL_SIZE);
	appleufs->ul_magic   = htobe32(APPLEUFS_LABEL_MAGIC);
	appleufs->ul_version = htobe32(APPLEUFS_LABEL_VERSION);
	appleufs->ul_time    = htobe32((u_int32_t)t);
	appleufs->ul_namelen = htobe16(namelen);
	strncpy(appleufs->ul_name, name, namelen);
	appleufs->ul_uuid    = htobe64(uuid);
	appleufs->ul_checksum = ffs_appleufs_cksum(appleufs);
}
