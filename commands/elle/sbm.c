/* SB - Copyright 1982 by Ken Harrenstien, SRI International
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

#if 0
	This file contains the low-level memory allocation
subroutines which are used by the SBLK routines.  The code here
is quite machine-dependent, and the definitions in "sb.h" should be
carefully checked to verify that they are correct for the target
machine.

	The ultimate low-level routine is "sbrk()" which must be
provided by the system''s C library.  SBM expects that successive calls
to sbrk() will return contiguous areas of memory with progressively
higher addresses.  Also, the very first call to sbrk() is assumed to
return a word-aligned address.
#endif /*COMMENT*/

#include "sb.h"

#define FUDGE (sizeof(struct smblk))	/* Allow this much fudge in
				allocation, to prevent undue fragmentation */

char *(*sbm_debug)();           /* Debug switch - user-furnished routine */

struct smblk *sbm_nfl;          /* Pointer to node freelist */
struct smblk *sbm_nxtra;	/* Reserved extra free node */
struct smblk *sbm_list;         /* Pointer to smblk memory alloc list.
				 * ALL smblks are strung onto this list
				 * except for the freelist!
				 */
SBMA sbm_lowaddr;		/* Lowest word-aligned address we know about.*/

/* If compiling with debug switch set, use special routine in place of
 * sbrk so we can pretend we have a very limited area of free memory.
 */
#ifdef DBG_SIZE
#define SBM_SBRK sbm_brk
char *sbm_brk();
#else
#define SBM_SBRK sbrk
#endif /*DBG_SIZE*/

/* Forward routine declarations */
char *sbrk();
struct smblk *sbm_nmak(), *sbm_nget(), *sbm_mget(), *sbm_split();
struct smblk *sbm_lmak(), *sbm_err();

/* SBM_INIT - Initialize storage management.
 *	If args are zero, normal initialization is done.  Otherwise,
 *	args are understood to be pointers to an area of memory allocated
 *	on the stack (eg by an "int mem[2000]" declaration in MAIN) and
 *	initialization will include this area in addition to the
 *	unused space between "_end" and the start of the stack segment.
 *	This is mostly of use for PDP11s which would otherwise waste a lot
 *	of address space.
 *	Maybe should have a SBM_RESET() function?
 */

struct smblk *
sbm_init(xaddr,xlen)
SBMA xaddr;		/* Address of allocated stack area if any */
SBMO xlen;		/* Size of this area */
{       register struct smblk *sm, *sml;
	register char *cp;

	/* Get initial chunk of memory from standard system rtn */
	if((cp = SBM_SBRK(SMNODES*sizeof(struct smblk))) == 0
	  || (int) cp == -1)
		return(sbm_err(0,"Can't sbrk"));
	sm = (struct smblk *)cp;		/* Better be word-aligned! */
	sbm_lmak(sm,(SBMO)sizeof(struct smblk),SMNODES);      /* Make list */
	sbm_nfl = sm;				/* Point freelist at it */
	sbm_lowaddr = (SBMA)sm;			/* Remember lowest addr seen */

	/* Set up 1st node pointing to all memory from here on up.
	 * We don't know exactly how much will be available at this point,
	 * so we just pretend we have the maximum possible.
	 */
	sbm_list = sml = sbm_nget();
	sml->smforw = sml->smback = 0;
	sml->smflags = SM_USE|SM_NID;		/* Initial flags */
	sml->smaddr = (SBMA) sml;
	sml->smlen = MAXSBMO;			/* Pretend we have lots */
	sml->smuse = (SMNODES * sizeof(struct smblk));

	/* Now split off everything above initial allocation as NXM. */
	sm = sbm_split(sml, sm->smuse);
	sml->smflags |= SM_MNODS;	/* Mark 1st node as having SM nodes */
	sm->smflags  |= SM_NXM;		/* Mark 2nd node as NXM */

	/* Now possibly set up extra nodes, if stack mem is being allocated
	 * (From our viewpoint it looks as if a chunk in the middle of
	 * the initial NXM section has been declared usable)
	 */
	if(xlen)
	  {     /* Allow for "extra" static stack memory */
		/* Will lose if xaddr <= 1st NXM! */
		sml = sbm_split(sm, (SBMO)(xaddr - sm->smaddr));
		sbm_split(sml, xlen);		/* Split off following NXM */
		sml->smflags &= ~(SM_USE|SM_NXM); /* This node is free mem! */
	  }

	/* Now set up a small additional node which points to the NXM
	 * that we cannot get from SBRK.  At this stage, this is just
	 * a place-holder, to reserve the node so we don't have to
	 * worry about running out of nodes at the same time sbrk stops
	 * returning memory.
	 * SM points to the NXM that we expect SBRK to dig into.
	 */
	sbm_split(sm, sm->smlen - WDSIZE); /* Chop off teensy bit */
	sm->smflags &= ~SM_USE;		/* Now mark NXM "free"!! */

	/* Finally, reserve an "extra" SM node for use by sbm_nget
	 * when it is allocating more freelist node chunks.
	 */
	sbm_nxtra = sbm_nget();

	return(sbm_list);
}

/* SBM_NGET() - Get a free SM node.
 *	Note the hair to provide a spare SM node when
 *	we are allocating memory for more SM nodes.  This is necessary
 *	because sbm_mget and sbm_nget call each other recursively and
 *	sbm_mget cannot create any new memory without a SM node to point
 *	at the allocated chunk.
 */
struct smblk *
sbm_nget()
{       register struct smblk *sm, *sml;

	if(!(sm = sbm_nfl))		/* Get a node from freelist */
	  {	/* Freelist is empty, try to allocate more nodes. */

		/* Put our "spare" smblk on freelist temporarily so that
		 * sbm_mget has a chance of winning.
		 * Infinite recursion is avoided by a test
		 * in sbm_mget which checks sbm_nfl and sbm_nxtra.
		 */
		if(!(sm = sbm_nxtra))
			return(sbm_err(0,"Zero sbm_nxtra!"));
		sm->smforw = 0;
		sbm_nfl = sm;
		sbm_nxtra = 0;

		/* Try to allocate another chunk of SM nodes. */
		sml = sbm_nmak(sizeof(struct smblk),SM_MNODS);

		/* Put the new free nodes (if any) on freelist.
		 * Done this way because freelist may have had one or two
		 * nodes added to it by sbm_mget, so can't just stick
		 * a new pointer in sbm_nfl.
		 */
		while(sm = sml)
		  {	sml = sm->smforw;
			sbm_nfre(sm);
		  }

		/* Now reserve an extra node again.
		 * It is an error if there is nothing on freelist here,
		 * because even if sbm_mget failed the "extra node" should
		 * still be on freelist.  The check for a zero sbm_nxtra
		 * above will catch such an error.
		 */
		sbm_nxtra = sbm_nget();

		/* Now see if anything to return */
		if(!(sm = sbm_nfl))		/* If freelist empty again, */
			return(0);		/* give up. */
	  }
	sbm_nfl = sm->smforw;   /* If win, take it off freelist */
	return(sm);		/* Return ptr or 0 if none */
}

/* SBM_NFRE(sm) - Return a SM node to the SM freelist.
 */
sbm_nfre(smp)
struct smblk *smp;
{       register struct smblk *sm;
	(sm = smp)->smflags = 0;
	sm->smforw = sbm_nfl;
	sbm_nfl = sm;
}

/* SBM_NMAK(elsize, flag) - Make (allocate & build) a typeless node freelist.
 */
struct smblk *
sbm_nmak(elsize, flag)
SBMO elsize;
unsigned flag;
{       register struct smblk *sm, *smp;
	register int cnt;

	if((sm = sbm_mget(SMNODES*elsize,SMNODES*elsize)) == 0)
		return(0);

	sm->smflags |= flag;            /* Indicate type of nodes */
	cnt = sm->smlen/elsize;		/* Find # nodes that will fit */
	sm->smuse = cnt * elsize;	/* Actual size used */
	smp = (struct smblk *)(sm->smaddr);	/* Ptr to 1st loc of mem */
	sbm_lmak(smp, (SBMO)elsize, cnt);	/* Build freelist */
	return(smp);            /* Return 1st free node. Caller is */
				/* responsible for setting freelist ptr. */
}

/* SBM_LMAK - Build freelist of typeless nodes.
 *	Note this does not allocate memory, it just converts an already
 *	allocated memory area.
 */
struct smblk *
sbm_lmak(addr, elsize, num)
SBMA addr;
SBMO elsize;
int num;
{	register struct smblk *sm, *smp;
	register int cnt;

	smp = (struct smblk *) addr;
	if((cnt = num) <= 0)
		return(0);
	do {	sm = smp;       /* Save ptr */
		sm->smforw = (smp = (struct smblk *) ((SBMA)smp + elsize));
		sm->smflags = 0;
	  } while(--cnt);
	sm->smforw = 0;         /* Last node points to nothing */
	return(sm);		/* Return ptr to last node */
}

/* SBM_NMOV(sm1, sm2, begp, elsize) - Move a typeless node.
 *	Copy sm1 to sm2, adjust ptrs, leave sm1 free.
 */
sbm_nmov(smp1,smp2,begp,elsize)
struct smblk *smp1, *smp2, **begp;
int elsize;
{       register struct smblk *sm;

	bcopy((SBMA)smp1,(SBMA)(sm = smp2), elsize);     /* Copy the stuff */
	if(sm->smforw) sm->smforw->smback = sm; /* Fix up links */
	if(sm->smback) sm->smback->smforw = sm;
	else *begp = sm;
}

/* SBM_MGET(min,max) - Get a SMBLK with specified amount of memory.
 *      Returns 0 if none available.
 *      Memory is guaranteed to start on word boundary, but may not
 *              end on one.  Note that sbm_mfree is responsible for
 *              ensuring that free mem starts word-aligned.
 *	A subtle but major concern of this code is the number of freelist
 * nodes gobbled by a single call.  If the freelist happens to not have
 * enough nodes, then a recursive call to sbm_mget is made (via sbm_nget)
 * in order to allocate a new batch of freelist nodes!  sbm_nget will
 * always provide a single "spare" node during such an allocation, but
 * there is only one and it is essential that sbm_mget gobble only ONE
 * (if any) during such a call, which is indicated by sbm_nxtra==0.
 *	The maximum # of freelist nodes that sbm_mget can gobble is
 * 2, when (1) NXM memory is obtained, and a SM is needed to point at
 * the new free mem, plus (2) the resulting SM is too big, and has to
 * be split up, which requires another SM for the remainder.
 *	The "used-NXM" smblk is set up at init time precisely in order to
 * avoid the necessity of creating it here when sbrk stops winning, since
 * that would require yet another freelist node and make it possible for
 * sbm_mget to gobble 3 during one call -- too many.
 *	Further note: the sbm_nfl checks are necessary in order
 * to ensure that a SM node is available for use by sbm_split.  Otherwise
 * the calls to sbm_split might create a new SM freelist by gobbling the
 * very memory which we are hoping to return!
 */
SBMO sbm_chksiz = SMCHUNKSIZ;	/* Current chunk size to feed sbrk */

struct smblk *
sbm_mget(cmin,cmax)
SBMO cmin,cmax;
{       register struct smblk *sm, *sml;
	register SBMO csiz;
	register SBMA addr, xaddr;

	if((sm = sbm_list) == 0         /* If never done, */
	  && (sm = sbm_init((SBMA)0,(SBMO)0)) == 0)	/* initialize mem alloc stuff. */
		return(0);		/* Can't init??? */

	/* Round up sizes to word boundary */
	if(rndrem(cmin)) cmin = rndup(cmin);
	if(rndrem(cmax)) cmax = rndup(cmax);

	/* Search for a free block having enough memory.
	 * If run into a free-NXM block, always "win", since there may be
	 * a combination of preceding free-mem and new mem which will satisfy
	 * the request.  If it turns out this didn't work, we'll just fail
	 * a little farther on.
	 */
retry:	csiz = cmin;			/* Set size that will satisfy us */
	do {
		if(  ((sm->smflags&SM_USE) == 0)
		  && ((sm->smlen >= csiz) || (sm->smflags&SM_NXM)) )
			break;
	  } while(sm = sm->smforw);
	if(sm == 0)
		return(0);	/* Found none that minimum would fit */

	if(sm->smflags&SM_NXM)
	  {	/* Found free area, but it's marked NXM and the system
		 * must be persuaded (via sbrk) to let us use that portion
		 * of our address space.  Grab a good-sized chunk.
		 */
		if(sbm_nfl == 0)	/* Verify a spare SM node is avail */
			goto getnod;	/* Nope, must get one. */

		/* Decide amount of mem to ask system for, via sbrk.
		 * The fine point here is the check of sbm_nxtra to make sure
		 * that, when building more freelist nodes, we don't have
		 * to use more than one SM node in the process.  If we
		 * asked for too much mem, we'd have to use a SM node
		 * to hold the excess after splitting.
		 */
		csiz = cmax;
		if(sbm_nxtra		/* If normal then try for big chunk */
		  && csiz < sbm_chksiz) csiz = sbm_chksiz;	/* Max */
		if (csiz > sm->smlen)  csiz = sm->smlen;	/* Min */

		/* Get the NXM mem */
		if((addr = (SBMA)SBM_SBRK(csiz)) != sm->smaddr)
		  {     /* Unexpected value returned from SBRK! */

			if((int)addr != 0 && (int)addr != -1)
			  {	return(sbm_err(0,"SBRK %o != %o", addr,
						sm->smaddr));
#if 0
			/* If value indicates couldn't get the stuff, then
			 * we have probably hit our limit and the rest of
			 * NXM should be declared "used" to prevent further
			 * hopeless sbrk calls.  We split off the portion
			 * of NXM that is known for sure to be unavailable,
			 * and mark it "used".  If a "used NXM" area already
			 * exists following this one, the two are merged.
			 * The chunk size is then reduced by half, so
			 * only log2(SMCHUNKSIZ) attempts will be made, and
			 * we try again.
			 */
				/* If returned some mem which starts outside
				 * the NXM then something is screwed up. */
				if(addr < sm->smaddr
				  || (addr >= sm->smaddr+sm->smlen))
					return(sbm_err(0,"SBRK %o != %o",
						addr, sm->smaddr));
				/* Got some mem, falls within NXM.
				 * Presumably someone else has called sbrk
				 * since last time, so we need to fence off
				 * the intervening area. */
				sm = sbm_split((sml=sm),(addr - sm->smaddr));
				sml->smflags |= SM_USE|SM_EXT;
				return(sbm_mget(cmin,cmax));
#endif /*COMMENT*/
			  }

			/* Handle case of SBRK claiming no more memory.
			 * Gobble as much as we can, and then turn this NXM
			 * block into a free-mem block, and leave the
			 * remainder in the used-NXM block (which should
			 * immediately follow this free-NXM block!)
			 */
			if(!(sml = sm->smforw)	/* Ensure have used-NXM blk */
			  || (sml->smflags&(SM_USE|SM_NXM))
					!= (SM_USE|SM_NXM))
				return(sbm_err(0,"No uNXM node!"));
			xaddr = sm->smaddr;	/* Use this for checking */
			sm->smuse = 0;		/* Use this for sum */
			for(csiz = sm->smlen; csiz > 0;)
			  {	addr = SBM_SBRK(csiz);
				if((int)addr == 0 || (int)addr == -1)
				  {	csiz >>= 1;
					continue;
				  }
				if(addr != xaddr)
					return(sbm_err(0,"SBRK %o != %o", addr,
						xaddr));
				sm->smuse += csiz;
				xaddr += csiz;
			  }

			/* Have gobbled as much from SBRK as we could.
			 * Turn the free-NXM block into a free-mem block,
			 * unless we got nothing, in which case just merge
			 * it into the used-NXM block and continue
			 * searching from this point.
			 */
			if(!(csiz = sm->smuse))	/* Get total added */
			  {	sm->smflags = sml->smflags;	/* Ugh. */
				sbm_mmrg(sm);
				goto retry;		/* Keep looking */
			  }
			else
			  {	sml->smaddr = sm->smaddr + csiz;
				sml->smlen += sm->smlen - csiz;
				sm->smlen = csiz;
				sm->smflags &= ~SM_NXM;	/* No longer NXM */
			  }
		  }

		/* Here when we've acquired CSIZ more memory from sbrk.
		 * If preceding mem area is not in use, merge new mem
		 * into it.
		 */
		if((sml = sm->smback) && 
		  (sml->smflags&(SM_USE|SM_NXM))==0)    /* Previous free? */
		  {     sml->smlen += csiz;		/* Yes, simple! */
			sm->smaddr += csiz;		/* Fix up */
			if((sm->smlen -= csiz) == 0)	/* If no NXM left,*/
				sbm_mmrg(sml);	/* Merge NXM node w/prev */
			sm = sml;		/* Prev is now winning node */
		  }
		else
		  {	/* Prev node isn't a free area.  Split up the NXM
			 * node to account for acquired mem, unless we
			 * gobbled all the mem available.
			 */
			if(sm->smlen > csiz	/* Split unless all used */
			  && !sbm_split(sm,csiz)) /* Call shd always win */
				return(sbm_err(0,"getsplit err: %o",sm));
			sm->smflags &= ~SM_NXM;	/* Node is now real mem */
		  }

		/* Now make a final check that we have enough memory.
		 * This can fail because SBRK may not have been able
		 * to gobble enough memory, either because (1) not
		 * as much NXM was available as we thought,
		 * or (2) we noticed the free-NXM area and immediately
		 * gambled on trying it without checking any lengths.
		 * In any case, we try again starting from the current SM
		 * because there may be more free mem higher up (eg on
		 * stack).
		 */
		if(sm->smlen < cmin)
			goto retry;
	  }

	/* Check to see if node has too much mem.  This is especially true
	 * for memory just acquired via sbrk, which gobbles a huge chunk each
	 * time.  If there's too much, we split up the area.
	 */
	if(sm->smlen > cmax+FUDGE)	/* Got too much?  (Allow some fudge)*/
		/* Yes, split up so don't gobble too much. */
		if(sbm_nfl)                     /* If success guaranteed, */
			sbm_split(sm,cmax);     /* split it, all's well. */
		else goto getnod;

	sm->smuse = 0;
	sm->smflags |= SM_USE;  /* Finally seize it by marking "in-use". */
	return(sm);

	/* Come here when we will need to get another SM node but the
	 * SM freelist is empty.  We have to forget about using the area
	 * we just found, because sbm_nget may gobble it for the
	 * freelist.  So, we first force a refill of the freelist, and then
	 * invoke ourselves again on what's left.
	 */
getnod:
	if(sml = sbm_nget())		/* Try to build freelist */
	  {	sbm_nfre(sml);		/* Won, give node back, */
		sm = sbm_list;		/* and retry, starting over! */
		goto retry;	
	  }
	/* Failed.  Not enough memory for both this request
	 * and one more block of SM nodes.  Since such a SM_MNODS
	 * block isn't very big, we are so close to the limits that it
	 * isn't worth trying to do something fancy here to satisfy the
	 * original request.  So we just fail.
	 */
	return(0);
}

#ifdef DBG_SIZE
/* Code for debugging stuff by imposing an artificial limitation on size
 * of available memory.
 */
SBMO sbm_dlim = MAXSBMO;	/* Amount of mem to allow (default is max) */

char *
sbm_brk(size)
unsigned size;
{	register char *addr;

	if(size > sbm_dlim) return(0);
	addr = sbrk(size);
	if((int)addr == 0 || (int)addr == -1)
		return(0);
	sbm_dlim -= size;
	return(addr);
}
#endif /*DBG_SIZE*/

/* SBM_MFREE(sm) - Free up an allocated memory area.
 */
sbm_mfree(sm)
register struct smblk *sm;
{       register struct smblk *smx;
	register SBMO crem;

	sm->smflags &= ~SM_USE;			/* Say mem is free */
	if((smx = sm->smback)                   /* Check preceding mem */
	  && (smx->smflags&(SM_USE|SM_NXM))==0) /*   If it's free, */
		sbm_mmrg(sm = smx);		/*   then merge 'em. */
	if((smx = sm->smforw)			/* Check following mem */
	  && (smx->smflags&(SM_USE|SM_NXM))==0) /*   Again, if free,  */
		sbm_mmrg(sm);                   /*   merge them.   */

	if(sm->smlen == 0)              /* Just in case, chk for null blk */
	  {     if(smx = sm->smback)            /* If pred exists, */
			sbm_mmrg(smx);          /* merge quietly. */
		else {
			sbm_list = sm->smforw;  /* 1st node on list, so */
			sbm_nfre(sm);           /* simply flush it. */
		  }
		return;
	  }

	/* This code is slightly over-general for some machines.
	 * The pointer subtraction is done in order to get a valid integer
	 * offset value regardless of the internal representation of a pointer.
	 * We cannot reliably force alignment via casts; some C implementations
	 * treat that as a no-op.
	 */
	if(crem = rndrem(sm->smaddr - sbm_lowaddr))	/* On word bndry? */
	  {     /* No -- must adjust.  All free mem blks MUST, by fiat,
		 * start on word boundary.  Here we fix things by
		 * making the leftover bytes belong to the previous blk,
		 * no matter what it is used for.  Prev blk is guaranteed to
		 * (1) Exist (this cannot be 1st blk since 1st is known to
		 * start on wd boundary) and to be (2) Non-free (else it would
		 * have been merged).
		 */
		if((smx = sm->smback) == 0)     /* Get ptr to prev blk */
		  {	sbm_err(0,"Align err");	/* Catch screws */
			return;
		  }
		crem = WDSIZE - crem;	/* Find # bytes to flush */
		if(crem >= sm->smlen)	/* Make sure node has that many */
		  {	sbm_mmrg(smx);  /* Flush node to avoid zero length */
			return;
		  }
		smx->smlen += crem;	/* Make stray bytes part of prev */
		sm->smaddr += crem;	/* And flush from current. */
		sm->smlen -= crem;
	  }
}

/* SBM_EXP - Expand (or shrink) size of an allocated memory chunk.
 *	"nsize" is desired new size; may be larger or smaller than current
 *	size.
 */
struct smblk *
sbm_exp(sm,size)
register struct smblk *sm;
register SBMO size;
{       register struct smblk *smf;
	register SBMO mexp, pred, succ;

	if(sm->smlen >= size)		/* Do we want truncation? */
		goto realo2;		/* Yup, go split block */

	/* Block is expanding. */
	mexp = size - sm->smlen;		/* Get # bytes to expand by */
	pred = succ = 0;
	if((smf = sm->smforw)           	/* See if free mem follows */
	 && (smf->smflags&(SM_USE|SM_NXM)) == 0)
		if((succ = smf->smlen) >= mexp)
			goto realo1;		/* Quick stuff if succ OK */

	if((smf = sm->smback)			/* See if free mem precedes */
	 && (smf->smflags&(SM_USE|SM_NXM)) == 0)
		pred = smf->smlen;

	/* If not enough free space combined on both sides of this chunk,
	 * we have to look for a completely new block.
	 */
	if(pred+succ < mexp)
	  {	if((smf = sbm_mget(size,size)) == 0)
			return(0);              /* Couldn't find one */
		else pred = 0;			/* Won, indicate new block */
	  }

	/* OK, must copy either into new block or down into predecessor
	 * (overlap is OK as long as bcopy moves 1st byte first)
	 */
	bcopy(sm->smaddr, smf->smaddr, sm->smlen);
	smf->smflags = sm->smflags;     /* Copy extra attribs */
	smf->smuse = sm->smuse;
	if(!pred)			/* If invoked sbm_mget */
	  {	sbm_mfree(sm);		/* then must free up old area */
		return(smf);		/* and can return immediately. */
	  }
	sbm_mmrg(smf);			/* Merge current into pred blk */
	sm = smf;			/* Now pred is current blk. */

	if(succ)
realo1:		sbm_mmrg(sm);		/* Merge succ into current blk */
realo2: if(sm->smlen > size		/* If now have too much, */
	  && sbm_split(sm, size))       /* split up and possibly */
		sbm_mfree(sm->smforw);  /* free up unused space. */
	return(sm);

	/* Note that sbm_split can fail if it can't get a free node,
	 * which is only possible if we are reducing the size of an area.
	 * If it fails, we just return anyway without truncating the area.
	 */
}

/* SBM_MMRG(sm) - Merge a memory area with the area following it.
 *	The node (and memory area) following the SM pointed to are
 *	merged in and the successor node freed up.  The flags
 *	and smuse of the current SM (which is not moved or anything)
 *	remain the same.
 */
sbm_mmrg(smp)
struct smblk *smp;
{       register struct smblk *sm, *sm2;

	sm = smp;
	sm->smlen += (sm2 = sm->smforw)->smlen;	/* Add succ's len */
	if(sm->smforw = sm2->smforw)            /* and fix linkages */
		sm->smforw->smback = sm;
	sbm_nfre(sm2);                          /* now can flush succ node */
}

/* SBM_SPLIT - Split up an area (gets a new smblk to point to split-off
 *	portion.)
 * Note returned value is ptr to 2nd smblk, since this is a new one.
 * Ptr to 1st remains valid since original smblk stays where it is.
 * NOTE: Beware of splitting up free mem (SM_USE == 0) since sbm_nget may
 * steal it out from under unless precautions are taken!  See comments
 * at sbm_mget related to this.
 */
struct smblk *
sbm_split(smp,coff)
struct smblk *smp;
SBMO coff;
{       register struct smblk *sm, *smx;
	register SBMO csiz;

	if((sm = smp)->smlen <= (csiz = coff))
		return(0);
	if((smx = sbm_nget()) == 0)
		return(0);
	smx->smlen = sm->smlen - csiz;          /* Set 2nd size */
	smx->smaddr = sm->smaddr + csiz;        /* Set 2nd addr */
	sm->smlen = csiz;			/* Take from 1st size */
	smx->smflags = sm->smflags;             /* Copy flags */
	if(smx->smforw = sm->smforw)            /* Splice 2nd after 1 */
		smx->smforw->smback = smx;
	smx->smback = sm;
	sm->smforw = smx;                       /* Put 2nd into chain */
	return(smx);                            /* Return ptr to 2nd smblk */
}

#if 0	/* Replaced by "bcopy" for system-dep efficiency */
/* SBM_SCPY - Copy string of bytes.  Somewhat machine-dependent;
 *	Tries to be clever about using word moves instead of byte moves.
 */
sbm_scpy(from, to, count)       /* Copy count bytes from -> to */
char *from, *to;
unsigned count;
{       register char *s1, *s2;
	register unsigned cnt;
	int tmp;

	if((cnt = count) == 0)
		return;
	s1 = from;
	s2 = to;
	while(rndrem((int)s1))		/* Get 1st ptr aligned */
	  {     *s2++ = *s1++;
		if(--cnt == 0) return;
	  }
	if(rndrem((int)s2) == 0)	/* Do wd move if ptr 2 now aligned */
	  {
#ifdef DUMBPCC /* Code for dumber (Portable C type) compiler */
		register WORD *ap, *bp;
		tmp = cnt;
		ap = (WORD *) s1;
		bp = (WORD *) s2;
		if(cnt = rnddiv(cnt))
			do { *bp++ = *ap++; }
			while(--cnt);
		if ((cnt = rndrem(tmp)) ==0)
			return;
		s1 = (char *) ap;
		s2 = (char *) bp;
#else
	/* Tight loop for efficient copying on 11s */
		tmp = cnt;
		if(cnt = rnddiv(cnt))
			do { *((WORD *)s2)++ = *((WORD *)s1)++; }
			while(--cnt);
		if((cnt = rndrem(tmp)) == 0)
			return;
#endif /*-DUMBPCC*/
	  }                             
	do { *s2++ = *s1++; }	/* Finish up with byte loop */
	while(--cnt);
}
#endif /*COMMENT*/

struct smblk *		/* If it returns at all, this is most common type */
sbm_err(val,str,a0,a1,a2,a3)
char *str;
struct smblk *val;
{	int *sptr;

	sptr = (int *) &sptr;	/* Point to self on stack */
	sptr += 5;		/* Point to return addr */
	if((int)sbm_debug==1)
		abort();
	if(sbm_debug)
		(*sbm_debug)(0,*sptr,str,a0,a1,a2,a3);
	return(val);
}

/* These routines correspond to the V7 LIBC routines as described
 * in the V7 UPM (3).  They should provide satisfactory emulation
 * if the documentation is correct.  Replacement is necessary since
 * the SBM routines are jealous and cannot tolerate competition for
 * calls of SBRK; i.e. the memory being managed must be contiguous.
 */

/* Guaranteed to return word-aligned pointer to area of AT LEAST
 * requested size.  Area size is rounded up to word boundary.
 */

char *
malloc(size)
unsigned size;
{       register struct smblk *sm, **sma;
	register SBMO siz;

	siz = rndup(size + sizeof (struct smblk *));   /* Make room for ptr */
	if((sm = sbm_mget(siz,siz)) == 0)
		return(0);
	*(sma = (struct smblk **)sm->smaddr) = sm; /* Store ptr in addr-1 */
	return((char *)++sma);
}

char *
alloc(size)     /* For V6 programs - note different failure value! */
unsigned size;
{       register char *addr;
	return((addr = malloc(size)) ? addr : (char *) -1);
}

free(ptr)
char *ptr;
{       register struct smblk *sm, **smp;

	smp = &((struct smblk **)ptr)[-1];	/* Point to addr-1 */
	sm = *smp;				/* Pluck SM ptr therefrom */
	if(((sm->smflags&0377) != SM_NID) || sm->smaddr != (SBMA)smp)
		return((int)sbm_err(0,"free: bad arg %o", ptr));
	sbm_mfree(sm);
	return(1);
}

char *
realloc(ptr,size)
char *ptr;
unsigned size;
{       register struct smblk *sm, **smp;

	smp = &((struct smblk **)ptr)[-1];	/* Point to addr-1 */
	sm = *smp;				/* Pluck SM ptr therefrom */
	if(((sm->smflags&0377) != SM_NID) || (sm->smaddr != (SBMA)smp))
		return((char *)sbm_err(0,"realloc: bad arg %o",ptr));
	if((sm = sbm_exp(sm, rndup(size+(sizeof(struct smblk *))))) == 0)
		return(0);
	*(smp = (struct smblk **)sm->smaddr) = sm;      /* Save smblk ptr */
	return((char *)++smp);
}

char *
calloc(nelem,elsize)
unsigned nelem, elsize;
{       register SBMO cmin;
	register WORD *ip;                     /* Clear in units of words */
	register char *addr;

	if((cmin = nelem*elsize) == 0           /* Find # bytes to get */
	  || (addr = malloc(cmin)) == 0)        /* Get it */
		return(0);
	ip = (WORD *) addr;			/* Set up ptr to area */
	cmin = rnddiv(cmin+WDSIZE-1);		/* Find # words to clear */
	do { *ip++ = 0; } while (--cmin);       /* Zap the area */
	return(addr);
}

/* SBM_NGC() - Specific routine for GC'ing SMBLK nodes.
 *
 * SBM_XNGC(begp, elsize, type) - Compact nodes of specified type.
 *      Scans allocated mem from low to high to find chunks with nodes of
 *	the specified type.
 *      Flushes current freelist and rebuilds it as scan progresses,
 *      such that 1st thing on list is lowest-addr node.  When a node is
 *      seen that can be moved, new node is acquired from freelist if
 *      it exists, otherwise no move is made.  If a chunk has been scanned
 *      and no active nodes remain, it is flushed and freelist updated.
 *      NOTE: This has not yet been verified to work with nodes of any
 *		type other than SMBLK.
 */

sbm_ngc()
{	register struct smblk *sm;
	if(!(sm = sbm_nxtra))
		return((int)sbm_err(0,"Zero sbm_nxtra"));
	sm->smflags |= SM_USE;		/* Ensure this one isn't GC'd */
	sbm_xngc(&sbm_nfl, sizeof(struct smblk), SM_MNODS);
	sm->smflags = 0;		/* Flush temporary crock */
}
sbm_xngc(begp, elsize, flag)
struct smblk **begp;
unsigned elsize, flag;
{       register struct smblk *sm, *chk, *smf;
	struct smblk *ftail, *savtail;
	int cnt, inuse;

	*begp = ftail = 0;		/* Flush node freelist */
	for(chk = sbm_list; chk; chk = chk->smforw)
	  if(chk->smflags&flag)
	    {   sm = (struct smblk *) chk->smaddr;
		cnt = (chk->smuse)/elsize;
		savtail = ftail;
		inuse = 0;
		smf = *begp;
					 /* Set up ptr to 1st freelist node */
		while(--cnt >= 0)
		  {     /* Here decide if movable */
			if(sm->smflags && smf   /* Live and have copy place */
			  && (
				(sm->smflags&SM_USE) == 0       /* Free mem? */
			    ||  (sm->smflags&(SM_MNODS|SM_DNODS))
			     )
			  && sm->smback)        /* has backptr (see ncpy) */
			  {                             /* Move the node */
				*begp = smf->smforw;	/* Get free node */
				if(smf == ftail)
					ftail = 0;
				if(smf == savtail)
					savtail = 0;
				/* Move node.  Already checked for back ptr
				 * of 0 since no obvious way to tell where
				 * the ptr to list is kept.  Sigh.
				 */
				sbm_nmov(sm,smf,(struct smblk **)0,elsize);
				/* Get ptr to new freelist node.  Note
				 * check to ensure that it is not in this
				 * same chunk (if it is, no point in moving
				 * any nodes!)
				 */
				if((smf = *begp) >= chk)
					smf = 0;        /* Zero if same chk */
				sm->smflags = 0;        /* Make node free */
			  }
			/* At this point, not movable */
			if(sm->smflags == 0)            /* Free node? */
			  {     if(ftail)               /* Add to freelist */
					ftail->smforw = sm;
				ftail = sm;
				if(*begp == 0)
					*begp = sm;
				sm->smforw = 0;
			  }
			else inuse++;
			sm = (struct smblk *)((SBMA)sm + elsize);
		  }
		if(inuse == 0                           /* All free? */
		  && (sm = chk->smback))		/* & not 1st? */
		  {     if(savtail)                     /* Edit freelist */
				(ftail = savtail)->smforw = 0;
			else *begp = ftail = 0;
			sbm_mfree(chk);
			chk = sm;
		  }
	    }
}

/*
 *      Note that proc must return a zero value, or loop aborts and
 *      returns that selfsame value.
 */
sbm_nfor(flag,nodsiz,proc,arg)
int flag;
int (*proc)();
int nodsiz;
struct sbfile *arg;
{       register struct smblk *sm, *np;
	register int cnt;
	int res;

	for(sm = sbm_list; sm; sm = sm->smforw)
	  if(sm->smflags&flag)
	    {   np = (struct smblk *) sm->smaddr;
		cnt = sm->smuse/nodsiz;
		do {
			if(np->smflags)
				if(res = (*proc)(np,arg))
					return(res);
			np = (struct smblk *)((SBMA)np + nodsiz);
		  } while(--cnt);
	    }
	return(0);
}
