/*	$NetBSD: ext2fs_xattr.c,v 1.4 2016/08/23 06:40:54 christos Exp $	*/

/*-
 * Copyright (c) 2016 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jaromir Dolecek.
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
__KERNEL_RCSID(0, "$NetBSD: ext2fs_xattr.c,v 1.4 2016/08/23 06:40:54 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/file.h>
#include <sys/buf.h>
#include <sys/vnode.h>
#include <sys/kernel.h>
#include <sys/kmem.h>
#include <sys/trace.h>
#include <sys/resourcevar.h>
#include <sys/kauth.h>
#include <sys/extattr.h>

#include <ufs/ufs/inode.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/ufs_extern.h>

#include <ufs/ext2fs/ext2fs.h>
#include <ufs/ext2fs/ext2fs_extern.h>
#include <ufs/ext2fs/ext2fs_xattr.h>

static const char * const xattr_prefix_index[] = {
	"",
	"user.",
	"system.posix_acl_access",
	"system.posix_acl_default",
	"trusted.",
	"", 	/* unused */
	"security",
	"system.",
	"system.richacl",
	"c",
};

static int
ext2fs_find_xattr(struct ext2fs_xattr_entry *e, uint8_t *start, uint8_t *end,
    int attrnamespace, struct uio *uio, size_t *size, uint8_t name_index,
    const char *name)
{
	uint8_t *value;
	int error;
	size_t value_offs, value_len, len, old_len;

	/*
	 * Individual entries follow the header. Each is aligned on 4-byte
	 * boundary.
	 */
	for(; !EXT2FS_XATTR_IS_LAST_ENTRY(e, end); e = EXT2FS_XATTR_NEXT(e)) {
		/*
		 * Only EXT2FS_XATTR_PREFIX_USER is USER, anything else
		 * is considered SYSTEM.
		 */
		if ((attrnamespace == EXTATTR_NAMESPACE_USER
		    && e->e_name_index != EXT2FS_XATTR_PREFIX_USER) ||
		    (attrnamespace == EXTATTR_NAMESPACE_SYSTEM
		    && e->e_name_index == EXT2FS_XATTR_PREFIX_USER)) {
			continue;
		}

		if (e->e_name_index != name_index ||
		    e->e_name_len != strlen(name) ||
		    strncmp(e->e_name, name, e->e_name_len) != 0)
			continue;

		value_offs = fs2h32(e->e_value_offs);
		value_len = fs2h32(e->e_value_size);
		value = &start[value_offs];

		/* make sure the value offset are sane */
		if (&value[value_len] > end)
			return EINVAL;

		if (uio != NULL) {
			/*
			 * Figure out maximum to transfer -- use buffer size
			 * and local data limit.
			 */
			len = MIN(uio->uio_resid, value_len);
			old_len = uio->uio_resid;
			uio->uio_resid = len;

			uio->uio_resid = old_len - (len - uio->uio_resid);

			error = uiomove(value, value_len, uio);
			if (error)
				return error;
		}

		/* full data size */
		*size += value_len;

		goto found;
	}

	/* requested attribute not found */
	return ENODATA;

    found:
	return 0;
}

static int
ext2fs_get_inode_xattr(struct inode *ip, int attrnamespace, struct uio *uio,
    size_t *size, uint8_t name_index, const char *name)
{
	struct ext2fs_dinode *di = ip->i_din.e2fs_din;
	struct ext2fs_xattr_ibody_header *h;
	uint8_t *start, *end;

	start = &((uint8_t *)di)[EXT2_REV0_DINODE_SIZE + di->e2di_extra_isize];
	h = (struct ext2fs_xattr_ibody_header *)start;
	end = &((uint8_t *)di)[EXT2_DINODE_SIZE(ip->i_e2fs)];

	if (end <= start || fs2h32(h->h_magic) != EXT2FS_XATTR_MAGIC)
		return ENODATA;

	return ext2fs_find_xattr(EXT2FS_XATTR_IFIRST(h), start, end,
	    attrnamespace, uio, size, name_index, name);
}

static int
ext2fs_get_block_xattr(struct inode *ip, int attrnamespace, struct uio *uio,
    size_t *size, uint8_t name_index, const char *name)
{
	struct ext2fs_dinode *di = ip->i_din.e2fs_din;
	uint8_t *start, *end;
	struct ext2fs_xattr_header *h;
	int error = 0;
	struct buf *bp = NULL;
	daddr_t xblk;

	xblk = di->e2di_facl;
	if (EXT2F_HAS_INCOMPAT_FEATURE(ip->i_e2fs, EXT2F_INCOMPAT_64BIT))
		xblk |= (((daddr_t)di->e2di_facl_high) << 32);

	/* don't do anything if no attr block was allocated */
	if (xblk == 0)
		return 0;

	error = bread(ip->i_devvp, fsbtodb(ip->i_e2fs, xblk),
	    (int)ip->i_e2fs->e2fs_bsize, 0, &bp);
	if (error)
		goto out;

	start = (uint8_t *)bp->b_data;
	h     = (struct ext2fs_xattr_header *)start;
	end   = &((uint8_t *)bp->b_data)[bp->b_bcount];

	if (end <= start || fs2h32(h->h_magic) != EXT2FS_XATTR_MAGIC)
		goto out;

	error = ext2fs_find_xattr(EXT2FS_XATTR_BFIRST(h), start, end,
	    attrnamespace, uio, size, name_index, name);

out:
	if (bp)
		brelse(bp, 0);
	return error;
}
int
ext2fs_getextattr(void *v)
{
	struct vop_getextattr_args /* {
	        const struct vnodeop_desc *a_desc;
	        struct vnode *a_vp;
	        int a_attrnamespace;
	        const char *a_name;
	        struct uio *a_uio;
	        size_t *a_size;
	        kauth_cred_t a_cred;
	} */ *ap = v;
	struct inode *ip = VTOI(ap->a_vp);
	char namebuf[EXT2FS_XATTR_NAME_LEN_MAX + 1];
	int error;
	const char *prefix, *name;
	uint8_t name_index;
	size_t name_match, valuesize = 0;

	if (ap->a_attrnamespace == EXTATTR_NAMESPACE_USER)
		prefix = xattr_prefix_index[EXT2FS_XATTR_PREFIX_USER];
	else
		prefix = xattr_prefix_index[EXT2FS_XATTR_PREFIX_SYSTEM];
	snprintf(namebuf, sizeof(namebuf), "%s%s", prefix, ap->a_name);

        error = extattr_check_cred(ap->a_vp, namebuf, ap->a_cred, VREAD);
        if (error)
                return error;

        /*
         * Allow only offsets of zero to encourage the read/replace
         * extended attribute semantic.  Otherwise we can't guarantee
         * atomicity, as we don't provide locks for extended attributes.
         */
        if (ap->a_uio != NULL && ap->a_uio->uio_offset != 0)
                return ENXIO;

	/* figure out the name index */
	name = ap->a_name;
	name_index = 0;
	name_match = 0;
	for(size_t i = 0; i < __arraycount(xattr_prefix_index); i++) {
		prefix = xattr_prefix_index[i];
		size_t l = strlen(prefix);
		if (l > 0 && strncmp(ap->a_name, prefix, l) == 0 &&
		    name_match < l) {
			name = &ap->a_name[l];
			name_index = i;
			name_match = l;
			continue;
		}
	}

	/* fetch the xattr */
	error = ext2fs_get_inode_xattr(ip, ap->a_attrnamespace, ap->a_uio,
	    &valuesize, name_index, name);
	if (error == ENODATA) {
		/* not found in inode, try facl */
		error = ext2fs_get_block_xattr(ip, ap->a_attrnamespace,
		    ap->a_uio, &valuesize, name_index, name);
	}

	if (ap->a_size != NULL)
		*ap->a_size = valuesize;

	return error;
}

int
ext2fs_setextattr(void *v)
{
#if 0
	struct vop_setextattr_args /* {
	        const struct vnodeop_desc *a_desc;
	        struct vnode *a_vp;
	        int a_attrnamespace;
	        const char *a_name;
	        struct uio *a_uio;
	        kauth_cred_t a_cred;
	} */ *ap = v;

	/* XXX set EXT2F_COMPAT_EXTATTR in superblock after successful set */
#endif

	/* XXX Not implemented */
	return EOPNOTSUPP;
}

static int
ext2fs_list_xattr(struct ext2fs_xattr_entry *e, uint8_t *end,
    int attrnamespace, int flags, struct uio *uio, size_t *size)
{
	char name[EXT2FS_XATTR_NAME_LEN_MAX + 1];
	uint8_t len;
	int error;
	const char *prefix;

	/*
	 * Individual entries follow the header. Each is aligned on 4-byte
	 * boundary.
	 */
	for(; !EXT2FS_XATTR_IS_LAST_ENTRY(e, end); e = EXT2FS_XATTR_NEXT(e)) {
		/*
		 * Only EXT2FS_XATTR_PREFIX_USER is USER, anything else
		 * is considered SYSTEM.
		 */
		if ((attrnamespace == EXTATTR_NAMESPACE_USER
		    && e->e_name_index != EXT2FS_XATTR_PREFIX_USER) ||
		    (attrnamespace == EXTATTR_NAMESPACE_SYSTEM
		    && e->e_name_index == EXT2FS_XATTR_PREFIX_USER)) {
			continue;
		}

		if (e->e_name_index < __arraycount(xattr_prefix_index))
			prefix = xattr_prefix_index[e->e_name_index];
		else
			prefix = "";

		len = snprintf(name, sizeof(name), "%s%.*s",
			prefix,
			e->e_name_len, e->e_name);

		if (uio != NULL) {
			if (flags & EXTATTR_LIST_LENPREFIX) {
				/* write name length */
				error = uiomove(&len, sizeof(uint8_t), uio);
				if (error)
					return error;
			} else {
				/* include trailing NUL */
				len++;
			}

			error = uiomove(name, len, uio);
			if (error)
				return error;

			*size += len;
		}
	}

	return 0;
}

static int
ext2fs_list_inode_xattr(struct inode *ip, int attrnamespace, int flags,
    struct uio *uio, size_t *size)
{
	struct ext2fs_dinode *di = ip->i_din.e2fs_din;
	void *start, *end;
	struct ext2fs_xattr_ibody_header *h;

	start = &((uint8_t *)di)[EXT2_REV0_DINODE_SIZE + di->e2di_extra_isize];
	h     = start;
	end   = &((uint8_t *)di)[EXT2_DINODE_SIZE(ip->i_e2fs)];

	if (end <= start || fs2h32(h->h_magic) != EXT2FS_XATTR_MAGIC)
		return 0;

	return ext2fs_list_xattr(EXT2FS_XATTR_IFIRST(h), end, attrnamespace,
	    flags, uio, size);
}

static int
ext2fs_list_block_xattr(struct inode *ip, int attrnamespace, int flags,
    struct uio *uio, size_t *size)
{
	struct ext2fs_dinode *di = ip->i_din.e2fs_din;
	void *end;
	struct ext2fs_xattr_header *h;
	int error = 0;
	struct buf *bp = NULL;
	daddr_t xblk;

	xblk = di->e2di_facl;
	if (EXT2F_HAS_INCOMPAT_FEATURE(ip->i_e2fs, EXT2F_INCOMPAT_64BIT))
		xblk |= (((daddr_t)di->e2di_facl_high) << 32);

	/* don't do anything if no attr block was allocated */
	if (xblk == 0)
		return 0;

	error = bread(ip->i_devvp, fsbtodb(ip->i_e2fs, xblk),
	    (int)ip->i_e2fs->e2fs_bsize, 0, &bp);
	if (error)
		goto out;

	h = (struct ext2fs_xattr_header *)bp->b_data;
	end = &((uint8_t *)bp->b_data)[bp->b_bcount];

	if (end <= (void *)h || fs2h32(h->h_magic) != EXT2FS_XATTR_MAGIC)
		goto out;

	error = ext2fs_list_xattr(EXT2FS_XATTR_BFIRST(h), end, attrnamespace,
	    flags, uio, size);

out:
	if (bp)
		brelse(bp, 0);
	return error;
}

int
ext2fs_listextattr(void *v)
{
	struct vop_listextattr_args /* {
	        const struct vnodeop_desc *a_desc;
	        struct vnode *a_vp;
	        int a_attrnamespace;
	        struct uio *a_uio;
	        size_t *a_size;
	        int a_flag;
	        kauth_cred_t a_cred;
	} */ *ap = v;
	struct inode *ip = VTOI(ap->a_vp);
	int error;
	const char *prefix;
	size_t listsize = 0;

	if (!EXT2F_HAS_COMPAT_FEATURE(ip->i_e2fs, EXT2F_COMPAT_EXTATTR)) {
		/* no EA on the filesystem */
		goto out;
	}

        /*
         * XXX: We can move this inside the loop and iterate on individual
         *      attributes.
         */
	if (ap->a_attrnamespace == EXTATTR_NAMESPACE_USER)
		prefix = xattr_prefix_index[EXT2FS_XATTR_PREFIX_USER];
	else
		prefix = xattr_prefix_index[EXT2FS_XATTR_PREFIX_SYSTEM];
        error = extattr_check_cred(ap->a_vp, prefix, ap->a_cred, VREAD);
        if (error)
                return error;

        /*
         * Allow only offsets of zero to encourage the read/replace
         * extended attribute semantic.  Otherwise we can't guarantee
         * atomicity, as we don't provide locks for extended attributes.
	 * XXX revisit - vnode lock enough?
         */
        if (ap->a_uio != NULL && ap->a_uio->uio_offset != 0)
                return ENXIO;

	/* fetch inode xattrs */
	error = ext2fs_list_inode_xattr(ip, ap->a_attrnamespace, ap->a_flag,
	    ap->a_uio, &listsize);
	if (error)
		return error;

	error = ext2fs_list_block_xattr(ip, ap->a_attrnamespace, ap->a_flag,
	    ap->a_uio, &listsize);
	if (error)
		return error;

out:
	if (ap->a_size != NULL)
		*ap->a_size = listsize;

	return 0;
}

int
ext2fs_deleteextattr(void *v)
{
#if 0
	struct vop_deleteextattr_args /* {
	        const struct vnodeop_desc *a_desc;
	        struct vnode *a_vp;
	        int a_attrnamespace;
	        const char *a_name;
	        kauth_cred_t a_cred;
	} */ *ap = v;
#endif

	/* XXX Not implemented */
	return EOPNOTSUPP;
}
