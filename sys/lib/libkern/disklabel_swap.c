/*	$NetBSD: disklabel_swap.c,v 1.1 2021/05/17 08:50:36 mrg Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1988 Regents of the University of California.
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
 *	@(#)ufs_disksubr.c	7.16 (Berkeley) 5/4/91
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: disklabel_swap.c,v 1.1 2021/05/17 08:50:36 mrg Exp $");

#ifdef _KERNEL_OPT
#include "opt_disklabel.h"
#endif /* _KERNEL_OPT */

#if defined(DISKLABEL_EI) || defined(LIBSA_DISKLABEL_EI)

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/disklabel.h>
#include <sys/conf.h>
#include <lib/libkern/libkern.h>

/*
 * from sh3/disksubr.c and kern/subr_disk_mbr.c with modifications:
 *	- update d_checksum properly
 *	- replace memcpy(9) by memmove(9) as a precaution
 *	- avoid memmove(9) for libkern version, check if the labels
 *	  are the same and skip copying in-place.
 */
void
disklabel_swap(struct disklabel *nlp, struct disklabel *olp)
{
	int i;
	uint16_t npartitions;

#define	SWAP16(x)	nlp->x = bswap16(olp->x)
#define	SWAP32(x)	nlp->x = bswap32(olp->x)

	SWAP32(d_magic);
	SWAP16(d_type);
	SWAP16(d_subtype);
	if (nlp != olp) {
		/* Do not need to swap char strings. */
		memcpy(nlp->d_typename, olp->d_typename,
			sizeof(nlp->d_typename));

		/*
		 * XXX What should we do for d_un (an union of char and
		 * pointers)?
		 */
		memcpy(nlp->d_packname, olp->d_packname,
			sizeof(nlp->d_packname));
	}

	SWAP32(d_secsize);
	SWAP32(d_nsectors);
	SWAP32(d_ntracks);
	SWAP32(d_ncylinders);
	SWAP32(d_secpercyl);
	SWAP32(d_secperunit);

	SWAP16(d_sparespertrack);
	SWAP16(d_sparespercyl);

	SWAP32(d_acylinders);

	SWAP16(d_rpm);
	SWAP16(d_interleave);
	SWAP16(d_trackskew);
	SWAP16(d_cylskew);
	SWAP32(d_headswitch);
	SWAP32(d_trkseek);
	SWAP32(d_flags);
	for (i = 0; i < NDDATA; i++)
		SWAP32(d_drivedata[i]);
	for (i = 0; i < NSPARE; i++)
		SWAP32(d_spare[i]);
	SWAP32(d_magic2);
	/* d_checksum is updated later. */

	SWAP16(d_npartitions);
	SWAP32(d_bbsize);
	SWAP32(d_sbsize);
	for (i = 0; i < MAXPARTITIONS; i++) {
		SWAP32(d_partitions[i].p_size);
		SWAP32(d_partitions[i].p_offset);
		SWAP32(d_partitions[i].p_fsize);
		/* p_fstype and p_frag is uint8_t, so no need to swap. */
		nlp->d_partitions[i].p_fstype = olp->d_partitions[i].p_fstype;
		nlp->d_partitions[i].p_frag = olp->d_partitions[i].p_frag;
		SWAP16(d_partitions[i].p_cpg);
	}

#undef SWAP16
#undef SWAP32

	/* Update checksum in the target endian. */
	nlp->d_checksum = 0;
	npartitions = nlp->d_magic == DISKMAGIC ?
	    nlp->d_npartitions : olp->d_npartitions;
	/*
	 * npartitions can be larger than MAXPARTITIONS when the label was not
	 * validated by setdisklabel. If so, the label is intentionally(?)
	 * corrupted and checksum should be meaningless.
	 */
	if (npartitions <= MAXPARTITIONS)
		nlp->d_checksum = dkcksum_sized(nlp, npartitions);
}
#endif /* DISKLABEL_EI || LIBSA_DISKLABEL_EI */
