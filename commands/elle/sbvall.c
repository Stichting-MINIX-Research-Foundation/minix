/* VALLOC - Aligned memory allocator
 *	Emulation of the 4.2BSD library routine of the same name.
 *	Copyright 1985 by Ken Harrenstien, SRI International
 *	This software is quasi-public; it may be used freely with
 *	like software, but may NOT be sold or made part of licensed
 *	products without permission of the author.  In all cases
 *	the source code and any modifications thereto must remain
 *	available to any user.
 *
 *	This is part of the SB library package.
 *	Any software using the SB library must likewise be made
 *	quasi-public, with freely available sources.
 */

#include "sb.h"

char *
valloc(size)
unsigned size;
{	register int pagmsk;
	register SBMO i;
	register struct smblk *sm, *smr;
	struct smblk *sbm_mget(), *sbm_split();

	pagmsk = getpagesize() - 1;	/* Get page size in bytes, less 1 */
	if(!(sm = sbm_mget(size+pagmsk, size+pagmsk))) /* Get area big enuf */
		return(0);
	/* Now find # bytes prior to 1st page boundary.
	 * This expression gives 0 if already at boundary, else #-1.
	 */
	i = pagmsk - ((int)(sm->smaddr) & pagmsk);
	if(i)		/* If need to split off preceding stuff, */
	  {	smr = sbm_split(sm, i+1);	/* do so (note i adjusted) */
		sbm_mfree(sm);			/* Release preceding mem */
		if(!(sm = smr)) return(0);	/* If couldn't split, fail */
	  }
	if(i = (sm->smlen - size))	/* See if any trailing stuff */
	  {	smr = sbm_split(sm, size);	/* Yeah, split it off too */
		if(smr) sbm_mfree(smr);	/* If couldn't split, excess OK. */
	  }
	return((char *)(sm->smaddr));
}
