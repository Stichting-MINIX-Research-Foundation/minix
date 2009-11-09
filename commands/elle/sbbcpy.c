
#include "sb.h"

/* BCOPY(from,to,cnt) - Copy string of bytes.
 *	Normally this routine is an assembly-language library routine,
 *	but not all systems have it.  Hence this C-language version
 *	which tries to be fairly machine-independent.
 *	Attempts to be clever about using word moves instead of byte moves.
 *	Does not hack overlapping backward moves.
 */
bcopy(from, to, cnt)	/* Copy count bytes from -> to */
register SBMA from;
register SBMA to;
register unsigned cnt;
{
	if(!cnt)
		return;
	while(rndrem((int)from))	/* Get source aligned */
	  {     *to++ = *from++;
		if(--cnt == 0) return;
	  }
	if(rndrem((int)to) == 0)	/* Do word move if dest now aligned */
	  {	register unsigned tmp;
		tmp = cnt;
		if((cnt = rnddiv(cnt)) > 4)
		  {	sbm_wcpy((int *)from, (int *)to, cnt);
			if((cnt = rndrem(tmp)) == 0)
				return;	/* No leftover bytes, all done */
			tmp -= cnt;	/* Ugh, must update pointers */
			from += tmp;
			to += tmp;
		  }
		else cnt = tmp;		/* Not worth call overhead */
	  }                             
	do { *to++ = *from++; }		/* Finish up with byte loop */
	while(--cnt);
}

/* SBM_WCPY - word-move auxiliary routine.
 *	This is a separate routine so that machines with only a few
 *	registers have a chance to use them for the word copy loop.
 *	This cannot be made part of BCOPY without doing some
 *	unnecessary pointer conversions and using extra variables
 *	(since most compilers will not accept type casts on lvalues,
 *	which are needed to treat (char *) as (int *)).
 */
sbm_wcpy(from, to, cnt)
register int *from, *to;
register unsigned cnt;
{
	if(cnt)
		do { *to++ = *from++; }
		while(--cnt);
}
