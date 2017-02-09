/*	$NetBSD: filecore_bmap.c,v 1.11 2015/03/28 19:24:05 maxv Exp $	*/

/*-
 * Copyright (c) 1994 The Regents of the University of California.
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
 *	filecore_bmap.c		1.1	1998/6/26
 */

/*-
 * Copyright (c) 1998 Andrew McMurry
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
 *	filecore_bmap.c		1.1	1998/6/26
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: filecore_bmap.c,v 1.11 2015/03/28 19:24:05 maxv Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/buf.h>
#include <sys/file.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/kauth.h>

#include <fs/filecorefs/filecore.h>
#include <fs/filecorefs/filecore_extern.h>
#include <fs/filecorefs/filecore_node.h>

/*
 * Bmap converts a the logical block number of a file to its physical block
 * number on the disk. The conversion is done by using the logical block
 * number to index into the data block (extent) for the file.
 */
int
filecore_bmap(void *v)
{
	struct vop_bmap_args /* {
		struct vnode *a_vp;
		daddr_t a_bn;
		struct vnode **a_vpp;
		daddr_t *a_bnp;
		int *a_runp;
	} */ *ap = v;
	struct filecore_node *ip = VTOI(ap->a_vp);
	struct filecore_mnt *fcmp = ip->i_mnt;
	daddr_t lbn = ap->a_bn;

	/*
	 * Check for underlying vnode requests and ensure that logical
	 * to physical mapping is requested.
	 */
	if (ap->a_vpp != NULL)
		*ap->a_vpp = ip->i_devvp;
	if (ap->a_bnp == NULL)
		return (0);

	/*
	 * Determine maximum number of readahead blocks following the
	 * requested block.
	 */
	if (ap->a_runp) {
		int nblk;
		int bshift=fcmp->log2bsize;

		nblk = (ip->i_size >> bshift) - (lbn + 1);
		if (nblk <= 0)
			*ap->a_runp = 0;
		else if (nblk >= (MAXBSIZE >> bshift))
			*ap->a_runp = (MAXBSIZE >> bshift) - 1;
		else
			*ap->a_runp = nblk;
	}
	/*
	 * Compute the requested block number
	 */
	return filecore_map(fcmp, ip->i_dirent.addr, lbn, ap->a_bnp);
}

int
filecore_map(struct filecore_mnt *fcmp, u_int32_t addr, daddr_t lbn, daddr_t *bnp)
{
	struct buf *bp;
	u_long frag, sect, zone, izone, a, b, m, n;
	u_int64_t zaddr;
	u_long *ptr;
	long c;
	int error = 0;

#ifdef FILECORE_DEBUG
	printf("filecore_map(addr=%x, lbn=%llx)\n", addr, (long long)lbn);
#endif
	frag = addr >> 8;
	sect = (addr & 0xff) +
		((lbn << fcmp->log2bsize) >> fcmp->drec.log2secsize);
	if (frag != 2)
		zone = frag / fcmp->idspz;
	else
		zone = fcmp->drec.nzones / 2;
	izone = zone;
	if (zone != 0)
		zaddr=((8<<fcmp->drec.log2secsize)-fcmp->drec.zone_spare)*zone
		  - 8*FILECORE_DISCREC_SIZE;
	else
		zaddr = 0;
	if (sect > 0)
		sect--;
	sect <<= fcmp->drec.share_size;
	do {
		error=bread(fcmp->fc_devvp, fcmp->map + zone,
			    1 << fcmp->drec.log2secsize, 0, &bp);
#ifdef FILECORE_DEBUG_BR
		printf("bread(%p, %lx, %d, CRED, %p)=%d\n", fcmp->fc_devvp,
		       fcmp->map+zone, 1 << fcmp->drec.log2secsize, bp, error);
		printf("block is at %p\n", bp->b_data);
#endif
		if (error != 0) {
			return error;
		}
		ptr = (u_long *)(bp->b_data) + 1; /* skip map zone header */
		if (zone == 0)
			ptr += FILECORE_DISCREC_SIZE >> 2;
		b = 0;
		while (b < (8 << (fcmp->drec.log2secsize))
		   - fcmp->drec.zone_spare) {
			a = ptr[b >> 5] >> (b & 31);
			c = 32 - (b & 31) - fcmp->drec.idlen;
			if (c <= 0) {
				m = ptr[(b >> 5) + 1];
				a |= m << (fcmp->drec.idlen+c);
				m >>= -c;
				c += 32;
			} else
				m = a >> fcmp->drec.idlen;
			n = fcmp->drec.idlen + 1;
			while ((m & 1) == 0) {
				m >>= 1;
				n++;
				if (--c == 0) {
					c=32;
					m = ptr[(b + n - 1) >> 5];
				}
			}
			a &= fcmp->mask;
			if (a == frag) {
				if (sect << fcmp->drec.log2secsize < n
				    << fcmp->drec.log2bpmb) {
					*bnp = (((zaddr+b)
					    << fcmp->drec.log2bpmb)
					    >> fcmp->drec.log2secsize) + sect;

#ifdef FILECORE_DEBUG_BR
					printf("brelse(%p) bm2\n", bp);
#endif
					brelse(bp, 0);
					return 0;
				} else
					sect -= (n<<fcmp->drec.log2bpmb)
					    >> fcmp->drec.log2secsize;
			}
			b += n;
		}
#ifdef FILECORE_DEBUG_BR
		printf("brelse(%p) bm3\n", bp);
#endif
		brelse(bp, 0);
		if (++zone == fcmp->drec.nzones) {
			zone = 0;
			zaddr=0;
		} else
			zaddr += ((8 << fcmp->drec.log2secsize)
			    - fcmp->drec.zone_spare);
	} while (zone != izone);
	return (E2BIG);
}

int
filecore_bread(struct filecore_mnt *fcmp, u_int32_t addr, int size, kauth_cred_t cred, struct buf **bp)
{
	int error = 0;
	daddr_t bn;

	error = filecore_map(fcmp, addr, 0, &bn);
	if (error) {

#ifdef FILECORE_DEBUG
		printf("filecore_bread(error=%d)\n", error);
#endif
		return error;
	}
	error = bread(fcmp->fc_devvp, bn, size, 0, bp);
#ifdef FILECORE_DEBUG_BR
	printf("bread(%p, %llx, %d, CRED, %p)=%d\n", fcmp->fc_devvp,
	    (long long)bn, size, *bp, error);
#endif
	return error;
}

int
filecore_dbread(struct filecore_node *ip, struct buf **bp)
{
	int error = 0;

	if (ip->i_block == -1)
		error = filecore_map(ip->i_mnt, ip->i_dirent.addr,
			0, &(ip->i_block));
	if (error)
		return error;
	error = bread(ip->i_mnt->fc_devvp, ip->i_block, FILECORE_DIR_SIZE,
		      0, bp);
#ifdef FILECORE_DEBUG_BR
	printf("bread(%p, %llx, %d, CRED, %p)=%d\n", ip->i_mnt->fc_devvp,
	       (long long)ip->i_block, FILECORE_DIR_SIZE, *bp, error);
#endif
	return error;
}
