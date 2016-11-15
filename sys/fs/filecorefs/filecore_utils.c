/*	$NetBSD: filecore_utils.c,v 1.11 2012/12/20 08:03:42 hannken Exp $	*/

/*-
 * Copyright (c) 1994 The Regents of the University of California.
 * All rights reserved.
 *
 * This code includes code derived from software contributed to the
 * NetBSD project by Mark Brinicombe.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	filecore_utils.c	1.1	1998/6/26
 */

/*-
 * Copyright (c) 1998 Andrew McMurry
 *
 * This code includes code derived from software contributed to the
 * NetBSD project by Mark Brinicombe.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	filecore_utils.c	1.1	1998/6/26
 */

/*
 * Copyright (c) 1998 Christopher G. Demetriou.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Christopher G. Demetriou
 *	for the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
__KERNEL_RCSID(0, "$NetBSD: filecore_utils.c,v 1.11 2012/12/20 08:03:42 hannken Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/stat.h>
#include <sys/buf.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/dirent.h>
#include <sys/kauth.h>

#include <fs/filecorefs/filecore.h>
#include <fs/filecorefs/filecore_extern.h>
#include <fs/filecorefs/filecore_node.h>
#include <fs/filecorefs/filecore_mount.h>

/*
 * int filecore_bbchecksum(u_char *bootblock)
 *
 * Calculates the filecore boot block checksum. This is used to validate
 * a filecore boot block on the disk.  If a boot block is validated then
 * it is used to locate the partition table. If the boot block is not
 * validated, it is assumed that the whole disk is NetBSD.
 *
 * The basic algorithm is:
 *
 *	for (each byte in block, excluding checksum) {
 *		sum += byte;
 *		if (sum > 255)
 *			sum -= 255;
 *	}
 *
 * That's equivalent to summing all of the bytes in the block
 * (excluding the checksum byte, of course), then calculating the
 * checksum as "cksum = sum - ((sum - 1) / 255) * 255)".  That
 * expression may or may not yield a faster checksum function,
 * but it's easier to reason about.
 *
 * Note that if you have a block filled with bytes of a single
 * value "X" (regardless of that value!) and calculate the cksum
 * of the block (excluding the checksum byte), you will _always_
 * end up with a checksum of X.  (Do the math; that can be derived
 * from the checksum calculation function!)  That means that
 * blocks which contain bytes which all have the same value will
 * always checksum properly.  That's a _very_ unlikely occurence
 * (probably impossible, actually) for a valid filecore boot block,
 * so we treat such blocks as invalid.
 */
int
filecore_bbchecksum(void *bb)
{
	u_char *bootblock = bb;
	u_char byte0, accum_diff;
	u_int sum;
	int i;

	sum = 0;
	accum_diff = 0;
	byte0 = bootblock[0];

	/*
	 * Sum the contents of the block, keeping track of whether
	 * or not all bytes are the same.  If 'accum_diff' ends up
	 * being zero, all of the bytes are, in fact, the same.
	 */
	for (i = 0; i < 511; ++i) {
		sum += bootblock[i];
		accum_diff |= bootblock[i] ^ byte0;
	}

	/*
	 * Check to see if the checksum byte is the same as the
	 * rest of the bytes, too.  (Note that if all of the bytes
	 * are the same except the checksum, a checksum compare
	 * won't succeed, but that's not our problem.)
	 */
	accum_diff |= bootblock[i] ^ byte0;
	sum = (sum - ((sum - 1) / 255) * 255);

	/*
	 * If all bytes in block are the same
	 * or the checksum does not match ; call it invalid.
	 */
	if (accum_diff == 0 || sum != bootblock[511])
		return -1;
	return 0;
}

mode_t
filecore_mode(struct filecore_node *ip)
{
	mode_t m = 0;
	int rf = 0;

	if ((ip->i_dirent.attr & FILECORE_ATTR_READ) ||
	    (ip->i_mnt->fc_mntflags & FILECOREMNT_OWNREAD) ||
	    (ip->i_dirent.attr & FILECORE_ATTR_DIR))
		rf = 1;
	if (ip->i_mnt->fc_mntflags & FILECOREMNT_ALLACCESS) {
		m |= S_IRUSR | S_IXUSR;
		if (rf || (ip->i_dirent.attr & FILECORE_ATTR_OREAD))
			m |= S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
	} else if (ip->i_mnt->fc_mntflags & FILECOREMNT_OWNACCESS) {
		if (rf) m |= S_IRUSR | S_IXUSR;
		if (ip->i_dirent.attr & FILECORE_ATTR_OREAD)
			m |= S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
	} else {
		m |= S_IRUSR | S_IXUSR;
		if (rf) m |= S_IRGRP | S_IXGRP;
		if (ip->i_dirent.attr & FILECORE_ATTR_OREAD)
			m |= S_IROTH | S_IXOTH;
	}
	if (ip->i_dirent.attr & FILECORE_ATTR_DIR) {
		m |= S_IFDIR | S_IXUSR | S_IXGRP | S_IXOTH;
	} else
		m |= S_IFREG;
	return m;
}

struct timespec
filecore_time(struct filecore_node *ip)
{
	struct timespec ts;
	u_int64_t cs;

	cs = (((u_int64_t)(ip->i_dirent.load & 0xFF)) << 32)
	    + ip->i_dirent.exec - ((u_int64_t)1725772500 << 7);
	ts.tv_sec = cs / 100;
	ts.tv_nsec = (cs % 100) * 10000000;
	return ts;
}

ino_t
filecore_getparent(struct filecore_node *ip)
{
	struct buf *pbp;
	u_int32_t addr;
	u_int32_t paddr;
	int error = 0;
	int i = 0;

#ifdef FILECORE_DEBUG
	printf("filecore_getparent(ino=%llx)\n", (long long)ip->i_number);
#endif
	if (ip->i_parent != -2) {
		return ip->i_parent;
	}
	if (ip->i_number == FILECORE_ROOTINO) {
		ip->i_parent = ip->i_number;
		return ip->i_number;
	}
	addr = ip->i_number & FILECORE_INO_MASK;
	/* Read directory data for parent dir to find its parent */
#ifdef FILECORE_DEBUG
	printf("filecore_getparent() read parent dir contents\n");
#endif
	error = filecore_bread(ip->i_mnt, addr, FILECORE_DIR_SIZE,
	    NOCRED, &pbp);
	if (error) {
		return error;
	}
	paddr = fcdirtail(pbp->b_data)->parent1
	    | fcdirtail(pbp->b_data)->parent2 << 16;
#ifdef FILECORE_DEBUG_BR
	printf("brelse(%p) ut1\n", pbp);
#endif
	brelse(pbp, 0);

	/* If parent's parent is the parent then parent is root dir */
	if (paddr == addr) {
		ip->i_parent = FILECORE_ROOTINO;
		return FILECORE_ROOTINO;
	}

#ifdef FILECORE_DEBUG
	printf("filecore_getparent() read grand-parent dir contents\n");
#endif
	error = filecore_bread(ip->i_mnt, paddr, FILECORE_DIR_SIZE,
	    NOCRED, &pbp);
	if (error) {
		return error;
	}
	while (fcdirentry(pbp->b_data,i)->addr != addr) {
		if (fcdirentry(pbp->b_data, i++)->name[0] == 0) {
#ifdef FILECORE_DEBUG_BR
			printf("brelse(%p) ut2\n", pbp);
#endif
			brelse(pbp, 0);
			return FILECORE_ROOTINO;
		}
	}
#ifdef FILECORE_DEBUG_BR
	printf("brelse(%p) ut3\n", pbp);
#endif
	brelse(pbp, 0);
	ip->i_parent = paddr + (i << FILECORE_INO_INDEX);
	return (paddr + (i << FILECORE_INO_INDEX));
}

int
filecore_fn2unix(char *fcfn, char *ufn, u_int16_t *len)
{
	int i = 0;

	if (*fcfn == 0)
		return (-1);
	while (i++ < 10 && *fcfn >= ' ') {
		if (*fcfn == '/')
			*ufn++ = '.';
		else
			*ufn++ = *fcfn;
		fcfn++;
	}
#ifdef notdef
	if (ip->i_mnt->fc_mntflags & FILECOREMNT_FILETYPE) {
		*ufn++ = ',';
		*ufn++ = hexdigits[(ip->i_dirent.load >> 10) & 15];
		*ufn++ = hexdigits[(ip->i_dirent.load >> 9) & 15];
		*ufn++ = hexdigits[(ip->i_dirent.load >> 8) & 15];
	}
#endif
	*ufn = 0;
	*len = i - 1;
	return 0;
}

int
filecore_fncmp(const char *fcfn, const char *ufn, u_short len)
{
	char f, u;
	int i = 0;

	if (*fcfn == 0 || len > 10)
		return -1;
	while (i++ < len) {
		if (*fcfn < ' ')
			return 1;
		f = *fcfn++;
		u = *ufn++;
		if (u == '.')
			u = '/';
		if (u >= 'a' && u <= 'z') u -= 'a' - 'A';
		if (f >= 'a' && f <= 'z') f -= 'a' - 'A';
		if (f < u)
			return 1;
		else if (f > u)
			return -1;
	}
	if (len == 10 || *fcfn < ' ')
		return 0;
	return -1;
}
