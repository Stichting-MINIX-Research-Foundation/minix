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
Todo stuff:
	New definitions:
		sbbuffer - old sbstr.  Abbrev & struct "sbbuff".  Macro SBBUFF
			(or SBBUF?)
		sbstring - list of sds.  Abbrev sbstr.  Macro SBSTR.
			Should *sbstr == *sdblk?  Yeah.
		sbfile - as before.  Macro SBFILE. (or SBFIL?)

	Try to get zero-length sdblks flushed on the fly,
		rather than waiting for moby GC.  Also, need to set
		up compaction of SD freelist, as well as SM freelist.
		Make SM freelist compact self-invoked by SBM_MGET?
	Any need for phys disk ptrs other than for tempfile?
		Can do sbm_forn through SDblks to find active sdfiles
		so list isn''t needed for that.
	Can sdback be flushed?  (not needed for keeping list sorted,
		or for searching it -- only used when linking
		blocks in or out of list.)  Perhaps use circular list?
		If list only used for tmpfile, then to link in/out could
		always start from sfptr1 of tmpfile? Sure, but slow?
		Last SD on phys list could belong to no logical list,
		and denote free space on tmpfile?

	--------------------------

	An "open" SBBUFFER will allow one to read, write, insert into,
and delete from a sbstring (a logical character string).  "Dot" refers
to the current logical character position, which is where all
operations must happen; sb_fseek must be used to change this location.
There are several states that the I/O can be in:
!SBCUR		----CLOSED----
		All other elements, including SBIOP, should also be 0.
		Dot is 0.
SBCUR && !SBIOP	----OPEN/IDLE----
		SBCUR points to a SD block (its SDMEM may or may not exist)
		SBIOP==0 (otherwise it would be open/ready)
		Dot is SBDOT + SBOFF.
		R/Wleft must be 0.
SBCUR && SBIOP	----OPEN/READY----
		SBCUR points to a SDBLK (SDMEM must exist!)
		SBIOP exists.
		Dot is SBDOT + offset into SMBLK.  SBOFF is ignored!
		SB_WRIT flag is set if "smuse" must be updated.
		The R/Wleft counts are set up:
		1. Rleft 0, Wleft 0 -- Since SBIOP is set, must assume
			counts are too.
			So this means at end of text, no room left.
			Otherwise would imply that setup needs doing.
		2. Rleft N, Wleft 0 -- At beg or middle of text
		3. Rleft 0, Wleft N -- At end of text
		4. Rleft N, Wleft N -- Shouldn''t ever happen

		Note that Rleft is always correct.  Wleft is sometimes
		set 0 in order to force a call to determine real state.

Note that SBIOP alone is a sufficient test for being OPEN/READY.

The important thing about updating the smblk is to ensure that the "smuse"
field is correct.  This can only be changed by writing or deleting.  We assume
that deletions always update immediately, thus to determine if an update
is necessary, see if SB_WRIT is set.  If so, update smuse before doing
anything but more writing!!!!

The SDBLK must be marked "modified" whenever a write operation is
done.  We try to do this only the first time, by keeping Wleft zero
until after the first write.  This is also when SB_WRIT gets set.
However, if in overwrite mode, Wleft must be kept zero in order to
force the proper actions; SB_WRIT is also not turned on since smuse
will not change.  Note that at EOF, overwrite becomes the same thing
as insert and is treated identically...

	If a SBLK has an in-core copy but no disk copy, it can be
freely modified.  Otherwise, modifications should preferably split
the block so as to retain "pure" blocks as long as possible.  "Pure" blocks
can always have their in-core versions flushed immediately (unless for
compaction purposes they''ll need to be written out in the same GC pass).
Alternatively, mods can simply mark the disk copy "free" and go
ahead as if no such copy existed.
	No additions or changes to a pure block are allowed, but
deletions from the end or beginning are always allowed.  All other
changes must split or insert new blocks to accomplish the changes.

Locking:
	SDBLKs are subject to unpredictable relocation, compaction,
and garbage collecting.  There are three ways in which a SDBLK can
remain fixed:

	1. The SDBLK has the SD_LOCK flag set.  This flag is used whenever
		a SBBUF''s SBCUR is pointing to this SDBLK.
	2. The SDBLK has the SD_LCK2 flag set.  This flag is used only
		during execution of various internal routines and should
		not be seen anywhere during execution of user code.
	3. The SDBLK has no back-pointer (is first block in a sbstring).
		Such SDBLKs cannot be relocated (since it is not known
		what may be pointing to them) but unlike the other 2 cases
		they are still subject to compaction with succeeding SDBLKs.

The SDBLK must be locked with SD_LOCK for as long as it is being
pointed to by SBCUR.  The sole exception is when a SBBUF in the
OPEN/IDLE state is pointing to the first SDBLK of a sbstring; this
sdblk is guaranteed not to be moved, since sdblks without a
back-pointer are never moved.  SD_LOCK is asserted as soon as the state
changes to OPEN/READY, of course.  The internal routines take pains to
always move SD_LOCK as appropriate.  Note that only one SD in a
sbstring can ever have SD_LOCK turned on.  SD_LCK2 is an auxiliary flag
which may appear in more than one SDBLK, for use by low-level routines
for various temporary reasons; either will prevent the SDBLK from being
modified in any way by the storage compactor.

SEEKs are a problem because it''s unclear at seek time what will happen
next, so the excision of the smblk can''t be optimized.  If the seek
happens to land in a sdblk with an existing smblk, there''s no problem;
but if it''s a sdblk alone, how to decide which part of it to read in???
If next action is:
	write - split up sdblk and create new one.  Read nothing in.
	read - read in 512 bytes starting at disk blk boundary if possible
		else read in 128 bytes starting with selected char
		(include beg of sdblk if less than 64 chars away)
	overwrite - as for read.
	backread - like read but position at end of sdblk.
	delete - split up sdblk, read nothing in.

We solve this through the OPEN/IDLE state, where SBIOP == 0 means SBOFF
points to logical offset from start of current sdblk, so that the seek
need not take any action.  Only when a specific operation is requested
will the transition to OPEN/READY take place, at which time we''ll know
what the optimal excision strategy is.  The routine SBX_READY performs
this function.

The physical links (SDFORW and SDBACK) are only valid when SDFILE is
set (likewise for SDLEN and SDADDR).  In other words, mungs to a sdblk
must check SDFILE to see whether or not the phys links should be
altered.  Normally they aren''t except during sdblk creation, deletion,
or swapout, no matter how much the sdblk gets shuffled around
logically.  The disk physical list is kept sorted in order of starting
addresses.  The text blocks indicated can overlap.  When a GC is
necessary, the code must figure out how much space is actually free.

-------------- Old woolgathering, ignore rest of this page ---------------

Question: should 512-byte buffers be maintained, one for each SBFILE?
Or should the in-core text be hacked up to serve for buffering?
Question is where to point the READ/WRITE system calls.  Currently,
they are pointed directly at the in-core text, and there are no
auxiliary buffers.

If use auxiliary buffers:
	How to handle flushing, when changing location etc?
	Could be clever about reading from large disk block, only
	get part of it into buffer instead of splitting up in order to
	read a "whole" block.
	Problem: sbstrings can include pieces of several different files.
		Hard to maintain just one buffer per FD without hacking
		done on one sbstring screwing that on another.
If don''t use buffers:
	Need to have a "chars-left" field in mem blocks, so know how
	much more can be added.  Will need heuristics for how much
	extra space to allocate.
#endif /*COMMENT*/

/* Includes, initial definitions */

#include <stdio.h>
#include "sb.h"

#ifndef V6
#define V6 0
#endif

#if V6
#include <stat.h>
#else
#include <sys/types.h>
#include <sys/stat.h>
#if MINIX
#include <fcntl.h>	/* For open() flags */
#else
#include <sys/file.h>	/* For open() flags */
#endif /* MINIX */
#endif /*-V6*/

extern int errno;
extern char *strerror();	/* From ANSI <string.h> */

/* Allocation decls */
SBFILE sbv_tf;		/* SBFILE for temp swapout file */
int (*sbv_debug)();	/* Error handler address */


/* SBX_READY argument flags (internal to SBSTR routines only)
 * The following values should all be unique; the exact value
 * doesn't matter as long as the right SKM flags are given.
 */
#define SK_READF 0			/* 0-skip fwd,  align BOB */
#define SK_READB (0|SKM_0BACK|SKM_EOB)	/* 0-skip bkwd, align EOB */
#define SK_WRITEF (0|SKM_EOB)		/* 0-skip fwd,  align EOB */
#define SK_DELF (4|SKM_0BACK)		/* 0-skip bkwd, align BOB */
#define SK_DELB (4|SKM_EOB)		/* 0-skip fwd,  align EOB */
#define SKM_0BACK 01	/* Zero-skip direction: 0 = fwd, set = backwd
			 * Don't ever change this value! See SBX_NORM. */
#define SKM_EOB	  02	/* Alignment: 0 = Beg-Of-Buf, set = End-Of-Buf */

/* Note on routine names:
 *	"SB_" 	User callable, deals with sbbufs (usually).
 *	"SBS_"	User callable, deals with sbstrings only.
 *	"SBX_"	Internal routine, not meant for external use.
 *	"SBM_"	Routine handling mem alloc, usually user callable.
 */

/* SBBUF Opening, Closing, Mode setting */

/* SB_OPEN(sb,sd) - Sets up SBBUF given pointer to first SD of a sbstring.
 * 	If SD == 0 then creates null sbstring.
 *	Any previous contents of SBBUF are totally ignored!!!  If you
 *		want to save the stuff, use SB_UNSET.
 *	Sets I/O ptr to start of sbstring.
 *	Returns 0 if error, else the given SB.
 */
SBBUF *
sb_open(sbp,sdp)
SBBUF *sbp;
SBSTR *sdp;
{	register struct sdblk *sd;
	register int cnt;
	register WORD *clrp;

	if(!sbp) return((SBBUF *)0);
	if((sd = sdp) == 0)
	  {	sd = sbx_ndget();	/* Get a fresh node */
		clrp = (WORD *) sd;	/* Clear it all */
		cnt = rnddiv(sizeof(struct sdblk));
		do { *clrp++ = 0; } while(--cnt);
		sd->sdflags = SD_NID;	/* Except flags of course */
	  }
	else if(sd->slback)		/* Must be first thing in sbstring */
		return((SBBUF *)0);		/* Perhaps could normalize tho */

	clrp = (WORD *) sbp;		/* Clear sbbuffer stuff */
	cnt = rnddiv(sizeof(SBBUF));
	do { *clrp++ = 0; } while(--cnt);

	sbp->sbcur = sd;
	/* Note that SD_LOCK need not be set, because first SDBLK has no
	 * backptr.  This is desirable to allow storage compactor maximum
	 * freedom in merging sdblks.
	 */
	/*	sd->sdflags |= SD_LOCK; */	/* Lock this one */
	return(sbp);
}


/* SB_CLOSE(sb)	- Close a SBBUF.
 *	Returns pointer to start of sbstring (first SD).
 *	Returns 0 if error.
 */
SBSTR *
sb_close(sbp)
SBBUF *sbp;
{	register SBBUF *sb;
	register struct sdblk *sd;

	if((sb = sbp) == 0)	/* Verify pointer */
		return((SBSTR *)0);
	sb_rewind(sb);		/* Do most of the work, including unlock */
	sd = sb->sbcur;		/* Save ptr to sbstring */
	sb->sbcur = 0;		/* Now reset the sbbuffer structure */
	sb->sbflags = 0;
	return(sd);
}


/* SB_SETOVW(sbp) - Set SBBUF Over-write mode for PUTC's.
 * SB_CLROVW(sbp) - Clear ditto.
 */
sb_setovw(sbp)
SBBUF *sbp;
{	register SBBUF *sb;
	if(sb=sbp)
	  {	sb->sbflags |= SB_OVW;
		sb->sbwleft = 0;
	  }
}

sb_clrovw(sbp)
SBBUF *sbp;
{	register SBBUF *sb;
	if(sb=sbp) sb->sbflags &= ~SB_OVW;
}

/* SBSTRING file system operations (see also sb_fsave) */

/* SB_FDUSE(fd) - Make a sbstring for given file.
 * 	FD is an open file descriptor.
 *	Returns pointer to a SBSTR containing the given file, or 0 if error.
 *	The FD must not be closed until a SB_FDCLS is done to
 *	purge memory of any blocks pointing at the file.
 * ** Maybe allocate sbfile structs with sbx_ndget, i.e. overlay on
 * ** top of sdblk node??  Wd this screw verify, GC, etc? Maybe not if
 * ** SD_LCK2 set...
 */

struct sbfile *sbv_ftab[SB_NFILES];

chroff
sbx_fdlen(fd)
int fd;
{
#if !V6
	struct stat statb;
#else
	struct statb statb;
	chroff len;
	struct {int hiwd ; int lowd;} foo;
#endif /*V6*/

	if(fstat(fd,&statb) < 0) return((chroff)-1);
#if V6
	len = statb.i_size1;
	len.hiwd = statb.i_size0 & 0377;
	return(len);
#else
	return((chroff)statb.st_size);
#endif /*-V6*/
}

SBSTR *
sb_fduse(ifd)
int ifd;
{	register struct sdblk *sd;
	register struct sbfile *sf;
	register int fd;
	chroff len;

	if((fd = ifd) < 0 || SB_NFILES <= fd	/* Check for absurd FD */
	  || sbv_ftab[fd])			/* and slot already in use */
		return((SBSTR *)0);
	if((len = sbx_fdlen(fd)) < 0) return((SBSTR *)0);
	sbv_ftab[fd]= sf = (struct sbfile *)sbx_malloc(sizeof(struct sbfile));
	sf->sffd = fd;
	sf->sfptr1 = sd = sbx_ndget();
	sf->sflen = len;
	sd->slforw = 0;
	sd->slback = 0;
	sd->sdforw = 0;
	sd->sdback = 0;
	sd->sdmem = 0;
	sd->sdfile = sf;
	sd->sdlen = len;
	sd->sdaddr = 0;
	return(sd);
}

/* SB_FDCLS(fd) - Close a file descriptor being used by sbstrings.
 *	If arg is -1, closes all FD's that are unused (a "sweep").
 *	For specific arg, returns 0 if couldn't close FD because still in use.
 *	Perhaps later version of routine could have option to copy
 *	still-used SD's to tempfile, and force the FD closed?
 */
sb_fdcls(ifd)
int ifd;
{	register int fd;

	if((fd = ifd) >= 0)
	  {	if(fd >= SB_NFILES) return(0);	/* Error of sorts */
		return(sbx_fcls(sbv_ftab[fd]));
	  }
	fd = SB_NFILES-1;
	do {
		sbx_fcls(sbv_ftab[fd]);
	  } while(--fd);	/* Doesn't try FD 0 ! */
	return(1);
}

sbx_fcls(sfp)
struct sbfile *sfp;
{	register struct sbfile *sf;
	register int fd;

	if((sf = sfp)==0		/* Ignore null args */
	  || sf == &sbv_tf)		/* and never close our tempfile! */
		return(0);
	fd = sf->sffd;			/* Find sys file descriptor */
	if(sbv_ftab[fd] != sf)		/* Ensure consistency */
		return(sbx_err(0,"SF table inconsistency"));
	if(sf->sfptr1)			/* Any phys list still exists? */
		return(0);		/* Yes, still in use, can't close */
	close(fd);			/* Maybe do this when list gone? */
	sbv_ftab[fd] = 0;		/* Remove from table */
	free(sf);			/* Remove sbfile struct from mem */
}

/* SB_FDINP(sb,fd) - Returns TRUE if specified fd is still in use
 *	by specified sbbuffer.
 */
sb_fdinp(sb, fd)
register SBBUF *sb;
int fd;
{	register struct sdblk *sd;
	register struct sbfile *sf;

	if((sf = sbv_ftab[fd]) == 0
	  || (sd = sb->sbcur) == 0)
		return(0);
	sd = sbx_beg(sd);		/* Move to beginning of sbstring */
	for(; sd; sd = sd->slforw)	/* Scan thru all blocks in string */
		if(sd->sdfile == sf)	/* If any of them match, */
			return(1);	/* Return tally-ho */
	return(0); 
}

/* SB_FSAVE(sb,fd) - Write entire SBBUF out to specified FD.
 *	Returns 0 if successful, else system call error number.
 */
sb_fsave(sb,fd)		/* Write all of given sbbuf to given fd */
register SBBUF *sb;
int fd;
{
	sbx_smdisc(sb);
	return(sbx_aout(sbx_beg(sb->sbcur), 2, fd));
}

/* SBBUF Character Operations */

/* SB_GETC(sb) - Get next char from sbstring.
 *	Returns char at current location and advances I/O ptr.
 *	Returns EOF on error or end-of-string.
 */
int
sb_sgetc(sb)
register SBBUF *sb;
{
	if(--(sb->sbrleft) >= 0)
		return sb_uchartoint(*sb->sbiop++);

	/* Must do hard stuff -- check ptrs, get next blk */
	sb->sbrleft = 0;			/* Reset cnt to zero */
	if(sb->sbcur == 0			/* Make sure sbbuffer there */
	  || (int)sbx_ready(sb,SK_READF,0,SB_BUFSIZ) <= 0)  /* Normalize & gobble */
		return(EOF);
	return(sb_sgetc(sb));			/* Try again */
}	/* Loop wd be faster, but PDL OV will catch infinite-loop bugs */


/* SB_PUTC(sb,ch) - Put char into sbstring.
 *	Inserts char at current location.
 *	Returns EOF on error, else the char value.
 */
int
sb_sputc(sb,ch)
register SBBUF *sb;
int ch;
{
	register struct sdblk *sd;

	if(--(sb->sbwleft) >= 0) return(*sb->sbiop++ = ch);

	sb->sbwleft = 0;		/* Reset cnt to avoid overflow */
	if((sd = sb->sbcur) == 0)	/* Verify string is there */
		return(EOF);		/* Could perhaps create it?? */
	if(sb->sbflags&SB_OVW)		/* If overwriting, handle std case */
	  {	if(sb->sbiop &&
		  --sb->sbrleft >= 0)		/* Use this for real count */
		  {	sd->sdflags |= SD_MOD;	/* Win, munging... */
			return(*sb->sbiop++ = ch);
		  }
		/* Overwriting and hit end of this block. */
		if((int)sbx_ready(sb,SK_READF,0,SB_BUFSIZ) > 0) /* Re-normalize */
			return(sb_sputc(sb,ch));

		/*  No blks left, fall through to insert stuff at end */
	  }

	/* Do canonical setup with heavy artillery */
	if((int)sbx_ready(sb,SK_WRITEF,SB_SLOP,SB_BUFSIZ) <= 0)	/* Get room */
		return(EOF);		/* Should never happen, but... */
	sb->sbflags |= SB_WRIT;
	sb->sbcur->sdflags |= SD_MOD;
	return(sb_sputc(sb,ch));	/* Try again */
}	/* Loop wd be faster, but PDL OV will catch infinite-loop bugs */


/* SB_PEEKC(sb) - Peek at next char from sbstring.
 *	Returns char that sb_getc would next return, but without
 *	changing I/O ptr.
 *	Returns EOF on error or end-of-string.
 */
int
sb_speekc(sb)
register SBBUF *sb;
{
	if (sb->sbrleft <= 0)			/* See if OK to read */
	  {	if (sb_sgetc(sb) == EOF)	/* No, try hard to get next */
			return EOF;		/* Failed, return EOF */
		sb_backc(sb);			/* Won, back up */
	  }
	return sb_uchartoint(*sb->sbiop);
}

/* SB_RGETC(sb) - Get previous char from sbstring.
 *	Returns char prior to current location and backs up I/O ptr.
 *	Returns EOF on error or beginning-of-string.
 */
int
sb_rgetc(sb)
register SBBUF *sb;
{
	register struct smblk *sm;
	register struct sdblk *sd;

	if((sd=sb->sbcur) && (sm = sd->sdmem)
	  && sb->sbiop > sm->smaddr)
	  {	if(sb->sbflags&SB_WRIT)
		  {	sm->smuse = sb->sbiop - sm->smaddr;
			sb->sbwleft = 0;
			sb->sbflags &= ~SB_WRIT;
		  }
		sb->sbrleft++;
		return sb_uchartoint(*--sb->sbiop);	/* Return char */ 
	  }
	if((int)sbx_ready(sb,SK_READB,SB_BUFSIZ,0) <= 0)
		return(EOF);
	return(sb_rgetc(sb));
}

/* SB_RDELC(sb) - Delete backwards one char.
 *	Returns nothing.
 */
sb_rdelc(sbp)
SBBUF *sbp;
{	register SBBUF *sb;
	register struct sdblk *sd;

	if(((sb=sbp)->sbflags&SB_WRIT)	/* Handle simple case fast */
	  && sb->sbiop > (sd = sb->sbcur)->sdmem->smaddr)
		  {	sb->sbwleft++;
			sb->sbiop--;
			sd->sdflags |= SD_MOD;
			return;
		  }
	else sb_deln(sb,(chroff) -1);	/* Else punt... */
}

/* SB_DELC(sb) - Delete one char forward? */
/* SB_INSC(sb,ch) - Insert char?  (instead of or in addition to PUTC) */


/* SBBUF string or N-char operations */

/* SB_DELN(sb,chroff) - delete N chars.  Negative N means backwards.
 *	Differs from sb_killn in that it flushes the text forever,
 *	and doesn't return anything.
 */

sb_deln(sbp, num)
SBBUF *sbp;
chroff num;
{
	register struct sdblk *sd;

	if(sd = sb_killn(sbp,num))
		sbs_del(sd);	/* Punt */
}

/* SB_KILLN(sb,chroff) - delete N chars, saving.  Negative N means backwards.
 *	Returns SD pointer to beginning of saved sbstring.
 */
struct sdblk *
sb_killn(sbp, num)
SBBUF *sbp;
chroff num;
{	register SBBUF *sb;
	register struct sdblk *sd, *sd2;
	struct sdblk *sdr, *sdx;
	chroff savdot;

	if((sd = sbx_xcis((sb=sbp),num,&sdr,&savdot)) == 0)
		return((struct sdblk *)0);

	sb->sbcur->sdflags &= ~SD_LOCK;	/* Now can flush sbcur lock */

	/* SD and SD2 now delimit bounds of stuff to excise.
	 * First do direction dependent fixups
	 */
	if(num >= 0)			/* If deleting forward, */
		sb->sbdot = savdot;	/* must reset dot to initial loc */

	/* SD and SD2 now in first/last order.  Complete SBCUR fixup. */
	sd2 = sdr;			/* sdr has ptr to end of stuff */
	if(sd2 = sd2->slforw)		/* More stuff after killed list? */
	  {	sb->sbcur = sd2;	/* Yes, point at it */
		sb->sboff = 0;		/* Dot already set right */
	  }
	else if(sdx = sd->slback)	/* See if any prior to killed list */
	  {	sb->sbcur = sdx;		/* Yes, point at it */
		sb->sboff = (sdx->sdmem ?	/* Get len of prev blk */
			sdx->sdmem->smuse : sdx->sdlen);
		sb->sbdot -= sb->sboff;
	  }
	else sb_open(sb,(SBSTR *)0);	/* No stuff left!  Create null sbstring */

	/* Fix up logical links.  Note SD2 points to succ of killed stuff */
	if(sd->slback)			/* If previous exists */
	  {	if(sd->slback->slforw = sd2)	/* Point it to succ, and */
			sd2->slback = sd->slback; /* thence to self */
		sd->slback = 0;			/* Now init killed list */
	  }
	else if(sd2) sd2->slback = 0;		/* No prev, clean rest */
	(sd2 = sdr)->slforw = 0;		/* Finish killed list */

	sb->sbcur->sdflags |= SD_LOCK;	/* Ensure current SD now locked */
	sd->sdflags &= ~SD_LCK2;	/* And unlock killed list */
	sd2->sdflags &= ~SD_LCK2;
	return(sd);
}

/* SB_CPYN(sbp,num) - Copy num characters, returns SD to sbstring.
 *	Like SB_KILLN but doesn't take chars out of original sbstring.
 */
SBSTR *
sb_cpyn(sbp,num)
SBBUF *sbp;
chroff num;
{	register SBBUF *sb;
	register struct sdblk *sd, *sd2;
	struct sdblk *sdr;
	chroff savloc;

	sb = sbp;
	if((sd = sbx_xcis(sb,num,&sdr,&savloc)) == 0)
		return((SBSTR *)0);
	sd2 = sbx_scpy(sd,sdr);
	sb_seek(sb,-num,1);		/* Return to original loc */
	return(sd2);		/* Return val is ptr to head of copy.
				 * It needn't be locked, because GC will
				 * never move list heads!
				 */
}

/* SB_SINS(sb,sd) - Insert sbstring at current location
 *
 */
sb_sins(sbp,sdp)
SBBUF *sbp;
struct sdblk *sdp;
{	register SBBUF *sb;
	register struct sdblk *sd, *sdx;
	chroff inslen;

	if((sb = sbp)==0
	  || (sd = sdp) == 0)
		return(0);
	if(sd->slback)		/* Perhaps normalize to beg? */
		return(0);
	if((sdx = (struct sdblk *)sbx_ready(sb,SK_DELB)) == 0)	/* Get cur pos ready */
		return(0);
	inslen = sbs_len(sd);		/* Save length of inserted stuff */

	sd->slback = sdx;		/* Fix up links */
	if(sdx->slforw)
	  {	while(sd->slforw)	/* Hunt for end of inserted sbstring */
			sd = sd->slforw;
		sd->slforw = sdx->slforw;
		sd->slforw->slback = sd;
	  }
	sdx->slforw = sdp;
	sb->sboff += inslen;		/* Set IO ptr to end of new stuff */
	return(1);
}

/* SBSTRING routines - operate on "bare" sbstrings. */

/* SBS_CPY(sd) - Copies given sbstring, returns ptr to new sbstring.
 */
SBSTR *
sbs_cpy(sdp)
SBSTR *sdp;
{	return(sbx_scpy(sdp,(struct sdblk *)0));
}

/* SBS_DEL(sd) - Flush a sbstring.
 */
sbs_del(sdp)
SBSTR *sdp;
{	register struct sdblk *sd;

	if(sd = sdp)
		while(sd = sbx_ndel(sd));
}


/* SBS_APP(sd1,sd2) - Appends sbstring sd2 at end of sbstring sd1.
 *	Returns sd1 (pointer to new sbstring).
 */

SBSTR *
sbs_app(sdp,sdp2)
struct sdblk *sdp,*sdp2;
{	register struct sdblk *sd, *sdx;

	if(sd = sdp)
	  {	while(sdx = sd->slforw)
			sd = sdx;
		if(sd->slforw = sdx = sdp2)
			sdx->slback = sd;
	  }
	return(sdp);
}

/* SBS_LEN(sd) - Find length of sbstring.
 */
chroff
sbs_len(sdp)
SBSTR *sdp;
{	register struct sdblk *sd;
	register struct smblk *sm;
	chroff len;

	if((sd = sdp)==0) return((chroff)0);
	len = 0;
	for(; sd ; sd = sd->slforw)
	  {	if(sm = sd->sdmem)
			len += (chroff)sm->smuse;
		else len += sd->sdlen;
	  }
	return(len);
}

/* SBBUF I/O pointer ("dot") routines */

/* SB_SEEK(sb,chroff,flag) - Like FSEEK.  Changes I/O ptr value as
 *	indicated by "flag":
 * 		0 - offset from beg
 *		1 - offset from current pos
 *		2 - offset from EOF
 *	Returns -1 on errors.
 *	Seeking beyond beginning or end of sbbuf will leave pointer
 *	at the beginning or end respectively.
 *	Returns 0 unless error (then returns -1).
 */
sb_seek(sbp, coff, flg)
SBBUF *sbp;
chroff coff;
int flg;
{	register SBBUF *sb;
	register struct smblk *sm;
	register struct sdblk *sd;
	SBMO moff;

	sb = sbp;
	if((sd = sb->sbcur) == 0) return(-1);
	if(sb->sbiop == 0)
	  {	switch(flg)
		  {	case 0:	if(coff == 0)	/* Optimize common case */
					return(sb_rewind(sb));
				sb->sboff = coff - sb->sbdot;	/* Abs */
				break;
			case 1:	sb->sboff += coff;		/* Rel */
				break;
			case 2:	sb->sboff += sb_ztell(sb) + coff;
				break;
			default: return(-1);
		  }
		sbx_norm(sb,0);
		return(0);
	  }
	if((sm = sd->sdmem) == 0)
		return(sbx_err(-1,"SDMEM 0"));
	moff = sb->sbiop - sm->smaddr;	/* Get cur smblk offset */
	if(sb->sbflags&SB_WRIT)		/* Update since moving out */
	  {	sm->smuse = moff;
		sb->sbflags &= ~SB_WRIT;
	  }
	sb->sbwleft = 0;		/* Always gets zapped */
	switch(flg)
	  {	case 0:		/* Offset from beginning */
			coff -= sb->sbdot + (chroff)moff; /* Make rel */

		case 1:		/* Offset from current loc */
			break;

		case 2:		/* Offset from end */
			coff += sb_ztell(sb);
			break;
		default: return(-1);
	  }

	/* COFF now has relative offset from current location */
	if (-(chroff)moff <= coff && coff <= sb->sbrleft)
	  {				/* Win! Handle repos-within-smblk */
		sb->sbiop += coff;
		sb->sbrleft -= coff;	/* Set r; wleft already 0 */
		return(0);
	  }

	/* Come here when moving to a different sdblk. */
	sb->sbrleft = 0;
	sb->sbiop = 0;
	sb->sboff = coff + (chroff)moff;
	sbx_norm(sb,0);
	return(0);
}

/* SB_REWIND(sb) - Go to beginning of sbbuffer.
 *	Much faster than using sb_seek.  Note that this leaves the sbbuffer
 *	in an open/idle state which is maximally easy to compact.
 */
sb_rewind(sbp)
SBBUF *sbp;
{	register SBBUF *sb;
	register struct sdblk *sd;

	if((sb = sbp)==0) return;
	sbx_smdisc(sb);			/* Ensure I/O disconnected */
	(sd = sb->sbcur)->sdflags &= ~SD_LOCK;	/* Unlock current blk */
	sd = sbx_beg(sd);		/* Move to beg of sbstring */
	/* Need not lock - see sb_open comments, also sb_close */
	/*	sd->sdflags |= SD_LOCK; */	/* Lock onto this one */
	sb->sbcur = sd;
	sb->sbdot = 0;
	sb->sboff = 0;
}

/* SB_TELL(sb) - Get I/O ptr value for SBBUF.
 *	Returns -1 on errors.
 */

chroff
sb_tell(sbp)
SBBUF *sbp;
{	register SBBUF *sb;
	register struct smblk *sm;
	register struct sdblk *sd;

	if((sd = (sb=sbp)->sbcur) == 0)
		return((chroff)-1);
	if(sb->sbiop == 0)
		return(sb->sbdot + sb->sboff);
	if((sm = sd->sdmem) == 0)
		return(sbx_err(0,"SDMEM 0"));
	return(sb->sbdot + (unsigned)(sb->sbiop - sm->smaddr));
}

/* SB_ZTELL(sb) - Get I/O ptr relative to "Z" (EOF).
 *	Returns # chars from current location to EOF; 0 if any errors.
 */
chroff
sb_ztell(sbp)
SBBUF *sbp;
{	register SBBUF *sb;
	register struct smblk *sm;
	register struct sdblk *sd;

	if((sd = (sb=sbp)->sbcur) == 0)
		return((chroff)0);
	if(sb->sbiop && (sm = sd->sdmem))
	  {	if(sb->sbflags&SB_WRIT)		/* If actively writing, */
			return(sbs_len(sd->slforw));	/* ignore this blk. */
			/* Note that previous code makes it unnecessary
			 * to invoke sbx_smdisc.  (otherwise wrong
			 * smuse would confuse sbs_len).
			 */
		return(sbs_len(sd) - (sb->sbiop - sm->smaddr));
	  }
	else
		return(sbs_len(sd) - sb->sboff);
}

/* Code past this point should insofar as possible be INTERNAL. */

/* SBX_READY(sb,type,cmin,cmax) - Set up SBBUF for reading or writing.
 *
 * If no current smblk:
 *	reading - set up for reading
 *	writing - set up for splitting?
 * If current smblk:
 *	reading - if can read, OK.  Else position at beg of next sdblk
 *	writing - if can write, OK.  Else position at end of prev sdblk,
 *		or set up for splitting?
 * Types:
 *	0 - Read forward (BOB)
 *	1 - Read backward (EOB)
 *	3 - Write (insert forward) (EOB)
 *	4 - Delete forward (return SD, force BOB-aligned)
 *	5 - Delete backward (return SD, force EOB-aligned)
 * Connected SD is always locked.
 * Returns 0 if error, -1 if EOF-type error, 1 for success.
 *
 * For types 0,1:
 *	CMIN,CMAX represent max # chars to read in to left and right of
 *		I/O ptr (prev and post).  Actual amount read in may be
 *		much less, but will never be zero.
 *	Successful return guarantees that SBIOP etc. are ready.
 * For type 3:
 *	If new block is allocated, CMIN and CMAX represent min, max sizes
 *		of the block.
 *	Successful return guarantees that SBIOP etc. are ready, but
 *	NOTE that SB_WRIT and SD_MOD are not set!  If not going to use
 *	for writing, be sure to clear sbwleft on return!
 * For types 4,5:
 *	CMIN, CMAX are ignored.
 *	SBIOP is always cleared.  SBOFF is guaranteed to be 0 for
 *	type 4, SMUSE for type 5.
 *	Return value is a SD ptr; 0 indicates error.  -1 isn't used.
 */

struct sdblk *
sbx_ready(sbp,type,cmin,cmax)
SBBUF *sbp;
int type;
SBMO cmin,cmax;
{	register SBBUF *sb;
	register struct sdblk *sd;
	register struct smblk *sm;
	int cnt, slop, rem;
	SBMO moff;

	if((sd = (sb=sbp)->sbcur) == 0)
		return(0);
	if(sb->sbiop)		/* Canonicalize for given operation */
	  {	if((sm = sd->sdmem)==0)
			return(0);
		moff = sb->sbiop - sm->smaddr;	/* Current block offset */
	  switch(type)
	  {
	case SK_READF:		/* Read Forward */
		if(sb->sbrleft > 0)	/* Already set up? */
			return(1);	/* Yup, fast return */
		sbx_smdisc(sb);		/* None left, disc to get next */
		if((sd = sbx_next(sb)) == 0)	/* Try to get next blk */
			return(-1);	/* At EOF */
		break;

	case SK_READB:		/* Read Backward */
		if(moff)		/* Stuff there to read? */
		  {	if(sb->sbflags&SB_WRIT)	/* Yup, turn writes off */
			  {	sm->smuse = moff;
				sb->sbflags &= ~SB_WRIT;
			  }
			sb->sbwleft = 0;
			return(1);
		  }
		sbx_smdisc(sb);
		break;

	case SK_WRITEF:		/* Writing */
		if(sb->sbrleft <= 0)
			sb->sbwleft = sm->smlen - moff;
		if(sb->sbwleft > 0)
			return(1);	/* OK to write now */
					/* NOTE: flags not set!!! */
		sbx_smdisc(sb);
		break;

	case SK_DELF:		/* Delete forward - force BOB */
		if(sb->sbrleft <= 0)		/* At end of blk? */
		  {	sbx_smdisc(sb);		/* Win, unhook */
			return(sbx_next(sb));   /* Return next or 0 if EOF */
		  }
		sbx_smdisc(sb);			/* Not at end, but see if */
		if(moff == 0)			/* at beg of blk? */
			return(sd);	/* Fast win! */
		break;

	case SK_DELB:		/* Delete backward - force EOB */
		if(sb->sbrleft <= 0)		/* Win if already EOB */
		  {	sbx_smdisc(sb);
			return(sd);
		  }
		sbx_smdisc(sb);
		break;

	default:
		return(0);
	  }
	  }

	/* Schnarf in the text, or whatever.
	 * SD points to current sdblk (must be SD_LOCKed)
	 * SBDOT must have correct value for this SD
	 * SBOFF has offset from there to put I/O ptr at.
	 *
	 * After normalization, SBOFF is guaranteed to point within
	 * the SD.  Other guarantees apply to boundary cases, depending
	 * on the mode (type) bits.
	 */
	sd = sbx_norm(sb,type);	/* Normalize I/O pos appropriately */
	sm = sd->sdmem;
	switch(type)
	  {
	case SK_READB:		/* Read Backward */
		if(sb->sboff == 0)	/* Due to normalize, if 0 seen */
			return(-1);	/* then we know it's BOF */
		if(sm) goto sekr2;
		else goto sekr1;

	case SK_READF:		/* Read Forward */
		if(sm) goto sekr2;
		if(sb->sboff == sd->sdlen)	/* Normalize means if EOB */
			return(-1);		/* then at EOF. */
	sekr1:	slop = SB_SLOP;
	sekr3:	if(sb->sboff > cmin+slop)	/* Too much leading text? */
		  {				/* Split off leading txt */
			sbx_split(sd,(chroff)(sb->sboff - cmin));
			sd = sbx_next(sb);	/* Point to next sdblk */
			sb->sboff = cmin;	/* Set correct offset */
						/* (sbx_next assumes 0) */
		  }
		if(sd->sdlen > sb->sboff+cmax+slop) /* Too much trailing txt? */
			sbx_split(sd,(chroff)(sb->sboff+cmax));

		/* ----- Try to get mem blk to read stuff into ----- */
		/* Note alignment hack for extra efficiency.  This ensures
		 * that all reads from disk to memory are made with the same
		 * source and destination word alignment, so the system kernel
		 * only needs byte-moves for the first or last bytes; all
		 * others can be word-moves.
		 * This works because sbx_mget always returns word-aligned
		 * storage, and we use sbx_msplit to trim off the right number
		 * of bytes from the start.
		 */
		cnt = sd->sdlen;		/* Get # bytes we'd like */
		if(rem = rndrem(sd->sdaddr))	/* If disk not word-aligned */
			cnt += rem;		/* allow extra for aligning.*/
		if(sm == 0)			/* Always true 1st time */
		  {	sm = sbx_mget(SB_SLOP,cnt); /* Get room (may GC!)*/
			if(sm->smlen < cnt)	/* Got what we wanted? */
			  {	slop = 0;	/* NO!!	 Impose stricter */
				cmin = 0;	/* limits.  Allow for new */
				cmax = sm->smlen - (WDSIZE-1); /* rem. */
				if(type == SK_READB)
				  {	cmin = cmax; cmax = 0; }
				goto sekr3;	/* Go try again, sigh. */
			  }
		  }
		else if(sm->smlen < cnt)	/* 2nd time shd always win */
		  {	sbx_err(0,"Readin blksiz err");	/* Internal error, */
			if((cmax /= 2) > 0) goto sekr3;	/* w/crude recovery */
			return(0);
		  }
		if(rem)		/* If disk not word-aligned, hack stuff */
		  {	sm = sbx_msplit(sm, (SBMO)rem);	/* Trim off from beg*/
			sbm_mfree(sm->smback);		/* Free the excess */
		  }
		sd->sdmem = sm;
		sm->smuse = sd->sdlen;

		if(sd->sdfile == 0)
			return(sbx_err(0,"No file"));	/* Gasp? */
		if(!sbx_rdf(sd->sdfile->sffd, sm->smaddr, sm->smuse,
				1, sd->sdaddr))
			return(sbx_err(0,"Readin SD: %o", sd));
		/* ------- */

	sekr2:	sbx_sbrdy(sb);		/* Make it current, pt to beg */
		sb->sbwleft = 0;	/* Ensure not set (esp if READB) */
		break;

	case SK_WRITEF:		/* Write-type seek */
		if(sm == 0)
		  {	/* Block is on disk, so always split (avoid readin) */
			if(sd->sdlen)			/* May be empty */
			  {	sbx_split(sd, sb->sboff); /* Split at IO ptr */
				sd = sbx_next(sb);	/* Move to 2nd part */
				if(sd->sdlen)		/* If stuff there, */
							/* split it again. */
					sbx_split(sd, (chroff) 0);
			  }
			goto sekwget;
		  }

		/* Block in memory */
		moff = sm->smuse;
		if(sb->sboff == moff)		/* At end of the block? */
		  {	if(sm->smlen > moff)	/* Yes, have room? */
				goto sekw;	/* Win, go setup and ret */
			if(sm->smforw			/* If next mem blk */
			  && (sm->smforw->smflags	/* Can have bytes */
				& (SM_USE|SM_NXM))==0	/* stolen from it */
			  && (sd->sdflags&SD_MOD)	/* and we ain't pure*/
			  && sm->smlen < cmax)		/* and not too big */
			  {	/* Then steal some core!!  Note that without
				 * the size test, a stream of putc's could
				 * create a monster block gobbling all mem.
				 */
				cmin = cmax - sm->smlen;
				if(cmin&01) cmin++;	/* Ensure wd-align */
				if(sm->smforw->smlen <= cmin)
				  {	sbm_mmrg(sm);
					goto sekw;
				  }
				sm->smforw->smlen -= cmin;
				sm->smforw->smaddr += cmin;
				sm->smlen += cmin;
				goto sekw;
			  }
			/* Last try... check next logical blk for room */
			if(sd->slforw && (sm = sd->slforw->sdmem)
			  && sm->smuse == 0
			  && sm->smlen)
			  {	sd = sbx_next(sb);	/* Yup, go there */
				goto sekw;
			  }
		  }

		/* Middle of block, split up to insert */
		sbx_split(sd, sb->sboff);	/* Split at IO ptr */
		if(sd->sdmem)			/* Unless blk now empty, */
		  {	sd = sbx_next(sb);	/* move to next. */
			if(sd->sdmem)		/* If not empty either */
			  sbx_split(sd, (chroff) 0);	/* Split again */
		  }

		/* Have empty SD block, get some mem for it */
  sekwget:	sd->sdmem = sm = sbx_mget(cmin,cmax);
		sm->smuse = 0;
   sekw:	sbx_sbrdy(sb);		/* Sets up sbwleft... */
		return(1);

	case SK_DELF:		/* Delete forward */
		if(sb->sboff == 0)	/* At block beg already? */
			return(sd);	/* Win, return it */
		sbx_split(sd, sb->sboff);	/* No, split up and */
		return(sbx_next(sb));	/* return ptr to 2nd part */

	case SK_DELB:		/* Delete backward (force EOB align) */
		if(sb->sboff !=		/* If not at EOB already, */
		  (sm ? (chroff)(sm->smuse) : sd->sdlen))
			sbx_split(sd, sb->sboff);	/* Then split */
		return(sd);	/* And return ptr to 1st part */
		break;

	default:
		return(0);
	  }	/* End of switch */
	return(1);
}

struct sdblk *
sbx_next (sbp)
SBBUF *sbp;
{	register SBBUF *sb;
	register struct sdblk *sd, *sdf;
	if((sdf = (sd = (sb=sbp)->sbcur)->slforw) == 0)
		return((struct sdblk *)0);
	sb->sbdot += (sd->sdmem ? (chroff)sd->sdmem->smuse : sd->sdlen);
	sb->sboff = 0;
	sd->sdflags &= ~SD_LOCK;	/* Unlock current */
	sdf->sdflags |= SD_LOCK;	/* Lock next */
	sb->sbcur = sdf;
	return(sdf);
}

/* SBX_NORM(sb,mode) - Normalizes I/O position as desired.
 *	The SBBUF must have I/O disconnected (SBIOP==0).
 *	Adjusts SBCUR, SBDOT, and SBOFF so that SBOFF is guaranteed
 *	to point to a location in the current SD block.
 *	The mode flags determine action when there is more than
 *	one possible SD that could be pointed to, as is the case
 *	when the I/O pos falls on a block boundary (possibly with
 *	adjacent zero-length blocks as well).
 *	SKM_0BACK - Zero-skip direction.
 *		  0 = Skip forward over zero-length blocks.
 *		set = Skip backward over zero-length blocks.
 *	SKM_EOB	  - Block-end selection (applies after skipping done).
 *		  0 = Point to BOB (Beginning Of Block).
 *		set = Point to EOB (End Of Block).
 * Returns the new current SD as a convenience.
 * Notes:
 *	The SKM_0BACK flag value is a special hack to search in
 *		the right direction when SBOFF is initially 0.
 *	None of the mode flags have any effect if the I/O pos falls
 *		within a block.
 *	Perhaps this routine should flush the zero-length blks it
 *		finds, if they're not locked??
 */
struct sdblk *
sbx_norm(sbp,mode)
SBBUF *sbp;
int mode;
{	register struct sdblk *sd;
	register struct smblk *sm;
	register SBBUF *sb;
	chroff len;

	if((sd = (sb=sbp)->sbcur) == 0)
	  {	sb->sbdot = 0;
		sb->sboff = 0;
		return(sd);
	  }
	sd->sdflags &= ~SD_LOCK;	/* Unlock current blk */

	if(sb->sboff >= (mode&01))	/* Hack hack to get right skip */
	  for(;;)			/* Scan forwards */
	  {	if(sm = sd->sdmem)		/* Get length of this blk */
			len = sm->smuse;
		else len = sd->sdlen;
		if(sb->sboff <= len)
		  if(sb->sboff < len	/* If == and fwd 0-skip, continue */
		  || (mode&SKM_0BACK))
		    {	if((mode&SKM_EOB)	/* Done, adjust to EOB? */
			  && sb->sboff == 0	/* Yes, are we at BOB? */
			  && sd->slback)	/* and can do it? */
			  {	sd = sd->slback;	/* Move to EOB */
				sb->sboff = (sm = sd->sdmem) 
					? (chroff)(sm->smuse) : sd->sdlen;
				sb->sbdot -= sb->sboff;
			  }
			break;
		    }
		if(sd->slforw == 0)	/* At EOF? */
		  {	sb->sboff = len;
			break;
		  }
		sd = sd->slforw;
		sb->sboff -= len;
		sb->sbdot += len;
	  }
	else				/* Scan backwards */
	 for(;;)
	  {	if(sd->slback == 0)	/* At BOF? */
		  {	sb->sboff = 0;
			sb->sbdot = 0;	/* Should already be 0, but... */
			break;
		  }
		sd = sd->slback;
		if(sm = sd->sdmem)		/* Get length of this blk */
			len = sm->smuse;
		else len = sd->sdlen;
		sb->sbdot -= len;
		if((sb->sboff += len) >= 0)
		  if(sb->sboff > 0	/* If == 0 and bkwd 0-skip, continue */
		    || !(mode&SKM_0BACK))
		    {	if((mode&SKM_EOB) == 0	/* Done, adjust to BOB? */
			  && sb->sboff == len	/* Yes, are we at EOB? */
			  && sd->slforw)	/* and can do it? */
			  {	sd = sd->slforw;	/* Move to BOB */
				sb->sboff = 0;
				sb->sbdot += len;
			  }
			break;
		    }
	  }
	sb->sbcur = sd;
	sd->sdflags |= SD_LOCK;
	return(sd);
}


struct sdblk *
sbx_beg(sdp)
struct sdblk *sdp;
{	register struct sdblk *sd, *sdx;
	if(sd = sdp)
		while(sdx = sd->slback)
			sd = sdx;
	return(sd);
}


sbx_smdisc(sbp)
SBBUF *sbp;
{	register SBBUF *sb;
	register struct smblk *sm;
	register struct sdblk *sd;

	sb = sbp;
	if((sd = sb->sbcur) == 0
	  || (sm = sd->sdmem) == 0)
		return;
	if(sb->sbflags&SB_WRIT)
	  {	sm->smuse = sb->sbiop - sm->smaddr;
		sb->sbflags &= ~SB_WRIT;
	  }
	sb->sboff = sb->sbiop - sm->smaddr;
	sb->sbiop = 0;
	sb->sbrleft = sb->sbwleft = 0;
}

sbx_sbrdy(sbp)		/* Sets up SBIOP, SBRLEFT, SBWLEFT */
SBBUF *sbp;
{	register SBBUF *sb;
	register struct sdblk *sd;
	register struct smblk *sm;

	if((sd = (sb=sbp)->sbcur) == 0
	  || (sm = sd->sdmem) == 0)
		return;
	sd->sdflags |= SD_LOCK;
	sb->sbiop = sm->smaddr + sb->sboff;
	if(sb->sbrleft = sm->smuse - sb->sboff)
		sb->sbwleft = 0;
	else sb->sbwleft = sm->smlen - sm->smuse;
}


/* SBX_SCPY(sd,sdl) - Copies given sbstring, returns ptr to new sbstring.
 *	Only goes as far as sdl (last copied blk); 0 for entire sbstring.
 */
struct sdblk *
sbx_scpy(sdp,sdlast)
struct sdblk *sdp, *sdlast;
{	register struct sdblk *sd, *sd2, *sdn;
	struct sdblk *sdr;

	if((sd = sdp) == 0) return((struct sdblk *)0);
	sdn = 0;
	do {
		sd->sdflags |= SD_LCK2;
		sd2 = sbx_sdcpy(sd);
		if(sd2->slback = sdn)
		  {	sdn->slforw = sd2;
			sdn->sdflags &= ~SD_LOCKS;
		  }
		else sdr = sd2;		/* Save 1st */
		sdn = sd2;
		sd->sdflags &= ~SD_LCK2;
	  } while(sd != sdlast && (sd = sd->slforw));
	sd2->slforw = 0;
	sd2->sdflags &= ~SD_LOCKS;
	return(sdr);
}


/* SBX_SDCPY(sd) - Copies given sdblk, returns ptr to new blk.
 *	Does not set locks, assumes caller does this (which it MUST,
 *	to avoid compaction lossage!)
 */

struct sdblk *
sbx_sdcpy(sdp)
struct sdblk *sdp;
{	register struct sdblk *sd, *sd2;
	register struct smblk *sm, *sm2;

	if((sd = sdp) == 0) return((struct sdblk *)0);
	sd2 = sbx_ndget();		/* Get a free sdblk */
	bcopy((SBMA)sd, (SBMA)sd2, sizeof(struct sdblk));	/* Copy sdblk data */
	sd2->slforw = 0;		/* Don't let it think it's on a list */
	sd2->slback = 0;
	if(sd2->sdfile)			/* If has disk copy, */
	  {	sd->sdforw = sd2;	/* Fix phys list ptrs */
		sd2->sdback = sd;
		if(sd2->sdforw)
			sd2->sdforw->sdback = sd2;
	  }
	if(sm = sd2->sdmem)		/* If has in-core copy, try to */
	  {	if(sm2 = sbm_mget(sm->smuse,sm->smuse))	/* get mem for it */
		  {	bcopy(sm->smaddr,sm2->smaddr,sm->smuse);
			sm2->smuse = sm->smuse;
			sd2->sdmem = sm2;	/* Point new sd to copy */
		  }
		else				/* Can't get mem... */
		  {	if(sd2->sdflags&SD_MOD)
				sbx_aout(sd2,1);	/* Swap out the blk */
			sd2->sdmem = 0;		/* Don't have incore copy */
		  }
	  }
	return(sd2);
}

/* SBX_XCIS(sbp,coff,&sdp2,adot) - Internal routine to excise a sbstring,
 *	defined as everything between current location and given offset.
 *	SD to first sdblk is returned (0 if error)
 *	SD2 (address passed as 3rd arg) is set to last sdblk.
 *	Both are locked with LCK2 to ensure that pointers are valid.
 *	The current location at time of call is also returned via adot.
 */
struct sdblk *
sbx_xcis(sbp,num,asd2,adot)
SBBUF *sbp;
chroff num, *adot;
struct sdblk **asd2;
{	register SBBUF *sb;
	register struct sdblk *sd, *sd2;
	int dirb;

	if((sb = sbp) == 0) return((struct sdblk *)0);
	dirb = 0;		/* Delete forward */
	if(num == 0) return((struct sdblk *)0);	/* Delete nothing */
	if(num < 0) dirb++;	/* Delete backward */

	if((sd = (struct sdblk *)
			sbx_ready(sb, (dirb ? SK_DELB : SK_DELF))) == 0)
		return((struct sdblk *)0);		/* Maybe nothing there */
	sd->sdflags |= SD_LCK2;		/* Lock up returned SD */
	*adot = sb->sbdot;		/* Save current location */
	sb->sboff += num;		/* Move to other end of range */

	if((sd2 = (struct sdblk *)
			sbx_ready(sb,(dirb ? SK_DELF : SK_DELB))) == 0)
	  {	sd->sdflags &= ~SD_LCK2;	/* This shd never happen if */
		return(				/* we got this far, but...  */
		  (struct sdblk *)sbx_err(0,"KILLN SD2 failed"));
	  }
	sd2->sdflags |= SD_LCK2;	/* Lock up other end of stuff */

	/* SD and SD2 now delimit bounds of stuff to excise.
	 * Now do direction dependent fixups
	 */
	if(dirb)
	  {	/* Backward, current sbdot is ok but must get SD/SD2
		 * into first/last order.  Also, due to nature of block
		 * splitups, a backward delete within single block will leave
		 * SD actually pointing at predecessor block.
		 */
		if(sd->slforw == sd2)	/* If SD became pred, fix things. */
		  {	sd->sdflags &= ~SD_LCK2;	/* Oops, unlock! */
			sd = sd2;
		  }
		else	/* Just need to swap SD, SD2 ptrs. */
		  {	/* Goddamit why doesn't C have an */
			/* exchange operator??? */
			*asd2 = sd;
			return(sd2);
		  }
	  }
	*asd2 = sd2;
	return(sd);
}

/* SBX_SPLIT(sd,chroff) - Splits block SD at point CHROFF (offset from
 *	start of block).  SD remains valid; it is left locked.
 *	The smblk is split too, if one exists, and SMUSE adjusted.
 *	If offset 0, or equal to block length, the 1st or 2nd SD respectively
 *	will not have a smblk and its sdlen will be 0.
 *	(Note that if a smblk exists, a zero sdlen doesn't indicate much)
 */
struct sdblk *
sbx_split(sdp, coff)
struct sdblk *sdp;
chroff coff;
{	register struct sdblk *sd, *sdf, *sdx;

	if((sd=sdp) == 0)
		return((struct sdblk *)0);
	sd->sdflags |= SD_LOCK;
	if(sd->sdflags&SD_MOD)		/* If block has been munged, */
		sbx_npdel(sd);		/* Flush from phys list now. */
	sdf = sbx_ndget();		/* Get a sdblk node */
	bcopy((SBMA)sd, (SBMA)sdf, (sizeof (struct sdblk)));	/* Copy node */
	/* Note that the flags are copied, so both sdblks are locked and
	 * safe from possible GC compaction during call to sbx_msplit...
	 */
	if(coff == 0)			/* If offset was 0, */
	  {				/* then 1st SD becomes null */
		if(sdf->sdfile)		/* Fix up phys links here */
		  {	if(sdx = sdf->sdback)
				sdx->sdforw = sdf;
			else sdf->sdfile->sfptr1 = sdf;
			if(sdx = sdf->sdforw)
				sdx->sdback = sdf;
		  }
		sdx = sd;
		goto nulsdx;
	  }
	else if(sd->sdmem)
		if(coff >= sd->sdmem->smuse)
			goto nulsdf;
		else sdf->sdmem = sbx_msplit(sd->sdmem, (SBMO)coff);
	else if(coff >= sd->sdlen)
nulsdf:	  {	sdx = sdf;
nulsdx:		sdx->sdforw = 0;
		sdx->sdback = 0;
		sdx->sdmem = 0;
		sdx->sdfile = 0;
		sdx->sdlen = 0;
		sdx->sdaddr = 0;
		goto nulskp;
	  }
	if(sd->sdfile)
	  {	sdf->sdlen -= coff;		/* Set size of remainder */
		sdf->sdaddr += coff;		/* and address */
		sd->sdlen = coff;		/* Set size of 1st part */

	/* Link 2nd block into proper place in physical sequence.
	 * 1st block is already in right place.	 Search forward until
	 * find a block with same or higher disk address, and insert
	 * in front of it.  If sdlen is zero, just flush the links,
	 * which is OK since the 1st block is what's pointed to anyway.
	 */
		if(sdf->sdlen > 0)
		  {	while((sdx = sd->sdforw) /* Find place to insert */
			  && sdf->sdaddr > sdx->sdaddr)
				sd = sdx;
			sdf->sdback = sd;	/* Link following sd. */
			if(sdf->sdforw = sd->sdforw)
				sdf->sdforw->sdback = sdf;
			sd->sdforw = sdf;
			sd = sdp;		/* Restore pointer */
		  }
		else
		  {	sdf->sdforw = 0;
			sdf->sdback = 0;
			sdf->sdfile = 0;	/* Say no disk */
		  }
	  }

nulskp:	sdf->slback = sd;		/* Link in logical sequence */
	if(sd->slforw)
		sd->slforw->slback = sdf;
	sd->slforw = sdf;

	sdf->sdflags &= ~SD_LOCKS;	/* Unlock 2nd but not 1st */
	return(sd);			/* Note sd, not sdf */
}

/* SBX_MSPLIT - Like sbm_split but never fails, and sets
 *	SMUSE values appropriately
 */
struct smblk *
sbx_msplit(smp, size)
struct smblk *smp;
SBMO size;
{	register struct smblk *sm, *smx;
	register int lev;

	lev = 0;
	while((smx = sbm_split((sm = smp), size)) == 0)
		sbx_comp(SB_BUFSIZ,lev++); /* Need to get some smblk nodes */
	if(sm->smlen >= sm->smuse)	/* Split across used portion? */
		smx->smuse = 0;		/* Nope, new blk is all free */
	else
	  {	smx->smuse = sm->smuse - sm->smlen;
		sm->smuse = sm->smlen;
	  }
	return(smx);
}

/* SBX_NDEL - flush a SD and associated SM.  Fixes up logical
 * and physical links properly.  Returns ptr to next logical SD.
 * NOTE: if sd->slback does not exist, the returned SD is your
 * only hold on the list, since the SD gets flushed anyway!
 */
struct sdblk *
sbx_ndel(sdp)
struct sdblk *sdp;
{	register struct sdblk *sd, *sdx;
	register struct smblk *sm;

	sd = sdp;
	if(sm = sd->sdmem)		/* If smblk exists, */
	  {	sbm_mfree(sm);		/* flush it. */
		sd->sdmem = 0;
	  }
	if(sdx = sd->slback)
		sdx->slforw = sd->slforw;
	if(sd->slforw)
		sd->slforw->slback = sdx;	/* May be zero */

	/* Logical links done, now hack phys links */
	if(sd->sdfile)			/* Have phys links? */
		sbx_npdel(sd);		/* Yes, flush from phys list */

	sdx = sd->slforw;
	sbx_ndfre(sd);
	return(sdx);
}

sbx_npdel(sdp)
struct sdblk *sdp;
{	register struct sdblk *sd, *sdx;
	register struct sbfile *sf;

	if((sf = (sd=sdp)->sdfile) == 0)
		return;
	if(sdx = sd->sdback)	/* Start of disk file? */
		sdx->sdforw = sd->sdforw;
	else
		sf->sfptr1 = sd->sdforw;
	if(sdx = sd->sdforw)
		sdx->sdback = sd->sdback;
	sd->sdfile = 0;
	sd->sdlen = 0;
}


struct sdblk *sbx_nfl;	/* Pointer to sdblk node freelist */

struct sdblk *
sbx_ndget()		/* Like sbm_nget but never fails! */
{	register struct sdblk *sd;
	register int lev;

	lev = 0;
	while((sd = sbx_nfl) == 0		/* Get a node */
						/* If fail, make more */
		&& (sd = sbm_nmak((sizeof (struct sdblk)),SM_DNODS)) == 0)
						/* If still fail, try GC */
			sbx_comp(sizeof(struct sdblk)*SM_DNODS,lev++);

	sbx_nfl = sd->slforw;		/* Take it off freelist */
	sd->sdflags = SD_NID;
	return(sd);			/* Return ptr to it */
}

sbx_ndfre(sdp)
struct sdblk *sdp;
{	register struct sdblk *sd;
	(sd = sdp)->sdflags = 0;
	sd->slforw = sbx_nfl;
	sbx_nfl = sd;
}

SBMA
sbx_malloc(size)
unsigned size;
{
	register int lev;
	register SBMA res;

	lev = 0;
	while((res = (SBMA)malloc(size)) == 0)
		sbx_comp(size,lev++);
	return(res);
}

struct smblk *
sbx_mget(cmin,cmax)     /* like sbm_mget but never fails! */
SBMO cmin, cmax;
{	register struct smblk *sm;
	register int lev, csiz;

	lev = 0;
	csiz = cmax;
	for(;;)
	  {	if(sm = sbm_mget(csiz,cmax))
			return(sm);		/* Won right off... */
		sbx_comp(csiz,lev++);		/* Barf, invoke GC */
		if(sm = sbm_mget(csiz,cmax))	/* Then try again */
			return(sm);
		if((csiz >>= 1) < cmin)		/* If still short, reduce */
			csiz = cmin;		/* request down to min */
	  }
}

chroff sbv_taddr;	/* Disk addr of place to write into (set by TSET) */
struct sdblk *sbv_tsd;	/* SD that disk addr comes after (set by TSET) */

#define sbx_qlk(sd)  (sd->sdflags&SD_LOCKS)

#if 0
	This is the compaction routine, which is the key to the
entire scheme.	Paging text to and from disk is trivial, but the
ability to merge blocks is the important thing since it allows
flushing the pointer information as well as the actual text!  This
eliminates fragmentation as a fatal problem.
	There are a variety of ways that storage can be reclaimed:

- "pure" in-core blocks can be flushed instantly.
- "impure" incore blocks can be written to tempfile storage and flushed.
- The SM node freelist can be compacted so as to flush memory which is
	used for nothing but holding free nodes.
- The SD node freelist can be compacted, ditto.
- SBBUFs can be compacted, by:
	- Merging logically & physically adjacent on-disk pieces
	- merging logically & physically adjacent in-core pieces
	- merging logically adjacent in-core pieces
	- merging logically adjacent disk pieces, by reading in
		and then writing out to tempfile storage.
		Worst case would reduce whole sbstr to single tempfile block.

Problems:
	What is "optimal" algorithm for typical usage?
	Must go over all code to make sure right things get locked
		and unlocked to avoid having rug pulled out from under.
	Could have optional "registration table" for sbstruc; if exist
		in table, can check during GC.	If find one, can first
		do sbx_smdisc and then repoint sbcur to 1st block,
		with sbdot of 0 and sboff of sb_tell().	 This allows
		reducing whole thing to one block even tho "locked".
		Never touch stuff locked with SD_LCK2, though.
		Also may need way to protect the sbstr SD actually being
		pointed to by current sbx routine processing.
	Could have count of # nodes free for SM and SD; don''t GC 
		unless # is some number greater than size of a node block!
	Have different levels of compaction; pass level # down thru calls
		so as to invoke progressively sterner compaction measures.
		Can invoke sbx_comp with any particular level!
	Must have list somewhere of SBBUFs?  or maybe OK to scan core
		for SM_DNODS, then scan sdblks?
	Screw: could happen that stuff gets flushed (cuz pure) or even
		written out to tempfile, and then we have to read it back
		in so as to compact more stuff into tempfile... how to avoid?
		If pure stuff small and next to impure stuff, merge?
	Some calls just want to get another free node and don''t need
		new core.  How to indicate this?  How to decide between
		freeing up used nodes, and creating new node freelist?
#endif /*COMMENT*/
/* Compact stuff.
 * General algorithm for getting storage is:
 *	1) allocate from freelist if enough there
 *	2) find unlocked pure smblk to free up
 *	3) find unlocked impure smblks, write out.
 *	4) Compact stuff by reducing # of sdblks.  This is key to scheme!
 *		Otherwise fragmentation will kill program.
 * Maybe put age cnt in each sbstr?  Bump global and set cntr each time
 * sbstr gets major hacking (not just getc/putc).
 */
extern struct smblk *sbm_list;
sbx_comp(cmin,lev)
int cmin, lev;
{	int sbx_sdgc();

	if(lev > 100)		/* If program has no way to handle this, */
		abort();	/* then simply blow up. */
	if(lev > 10)		/* Too many iterations? Try to warn. */
		return(sbx_err(0,"GC loop, cannot free block of size %d",
				cmin));

	/* Step thru core hunting for SD node blocks */
	sbm_nfor(SM_DNODS,sizeof(struct sdblk),sbx_sdgc,lev);
}

/* Do GC stuff on a sdblk.  Guaranteed to exist, but may be locked */
sbx_sdgc(sdp,lev)
struct sdblk *sdp;
int lev;
{	register struct sdblk *sd, *sdf;
	register struct smblk *sm;
	struct smblk *smf, *sbm_exp ();
	SBMO more;

	sd = sdp;
	if(sbx_qlk(sd)) return(0);
	sm = sd->sdmem;
	sdf = sd->slforw;
	if (lev < 4) goto lev3;

	/* Level 4 - write out everything possible */
	/* Back up to start of sbstr */
	while((sdf = sd->slback) && !sbx_qlk(sdf))
		sd = sdf;
	if((sdf = sd->slforw) == 0	/* If only 1 blk, ensure on disk */
	  || sbx_qlk(sdf))
	  {	if(sm = sd->sdmem)
		  {	if(sd->sdflags&SD_MOD)		/* If impure, */
				sbx_aout(sd, 1);	/* swap out the SD */
			sbm_mfree(sm);
			sd->sdmem = 0;
		  }
		return(0);
	  }
	/* At least two blocks in string.  Set up for flushout. */
	sbx_aout(sd, 0);	/* Swapout as much of sbstring as possible */
	return(0);

lev3:			/* Level 3 - write out more */
lev2:			/* Level 2 - write out all impure & small pure */
lev1:	if(lev >= 1)	/* Level 1 - merge small impure & small pure */
	  {	if(!sm || !sdf) return(0);
		while(((smf = sdf->sdmem) && !(sdf->sdflags&SD_LOCKS)
		  && (more = smf->smuse + sm->smuse) < SB_BUFSIZ) )
		  {	if(sm->smforw != smf
			  && more > sm->smlen)		/* If need more rm */
			  {	sm = sbm_exp(sm,more);	/* Get it */
				if(!sm) return(0);	/* If none, stop */
				sd->sdmem = sm;
			  }
			bcopy(smf->smaddr,
			     sm->smaddr + sm->smuse, smf->smuse);
			sm->smuse = more;
			if(sm->smforw == smf)
			  {	sdf->sdmem = 0;
				sbm_mmrg(sm);	/* Merge */
				if(sm->smlen > more+SB_SLOP)
					sbm_mfree(sbm_split(sm, more));
					/* Guaranteed to win since mmrg
					 * just freed a mem node */
			  }
			sd->sdflags |= SD_MOD;
			if(sdf = sbx_ndel(sdf))
				continue;
			return(0);
		  }
	  }

	if(lev <= 0)	/* Level 0 - free up large pure blocks */
			/* Also merge blocks which are adjacent on disk */
	  {	if(sm)
		  {	if(sm->smuse == 0)
				sd->sdlen = 0;
			else if((sd->sdflags&SD_MOD) == 0
			    && sm->smuse > 64)
			  {	sbm_mfree(sm);
				sd->sdmem = 0;
				goto lev0adj;
			  }
			else goto lev0adj;
		  }

		if(sd->sdlen == 0	/* Free zero blocks */
		  && sd->slback)	/* Make sure don't lose list */
		  {	sbx_ndel(sd);
			if((sd = sdf) == 0)
				return(0);
			sdf = sd->slforw;
		  }
	lev0adj:	/* Merge blocks if adjacent on disk */
			/* This is common after reading thru large chunks
			* of a file but not modifying it much.
			*/
		if((sd->sdflags&SD_MOD) == 0	/* Pure */
		  && sdf && (sdf->sdflags&(SD_LOCKS|SD_MOD)) == 0
		  && sd->sdfile && (sd->sdfile == sdf->sdfile)
		  && (sd->sdaddr + sd->sdlen) == sdf->sdaddr )
		  {	sd->sdlen += sdf->sdlen;
			sbx_ndel(sdf);		/* Flush 2nd */
			if(sm = sd->sdmem)
			  {	sbm_mfree(sm);
				sd->sdmem = 0;
			  }
		  }
		return(0);
	  }
	return(0);
}

/* SBX_AOUT - output ALL of a hackable sbstring starting at given sdblk.
 *	Note that code is careful to do things so that an abort at any
 *	time (e.g. write error) will still leave sbstring in valid state.
 * Flag value:
 *	0 - Writes out as many unlocked sdblks as possible, and merges
 *		so that resulting sdblk (same one pointed to by arg)
 *		incorporates all stuff written out.
 *	1 - Writes out single sdblk indicated, whether unlocked or not.
 *		Doesn't free mem or merge anything; does update physlist
 *		and flags.
 *	2 - Writes out all sdblks to specified FD/offset, no mods at all,
 *		not even to physlist or flags.	Good for saving files
 *		when something seems wrong.  (How to pass fd/off args?)
 *		(offset arg not implemented, no need yet; 0 assumed)
 * Returns 0 if successful, else UNIX system call error number.
 */

sbx_aout(sdp,flag,fd)
struct sdblk *sdp;
int flag, fd;
{	register struct sdblk *sd;
	register struct smblk *sm;
	register int cnt;
	int ifd, ofd, skflg, rem;
	chroff inlen;
	extern SBMA sbm_lowaddr;	/* Need this from SBM for rndrem */
	char buf[SB_BUFSIZ+16];	/* Get buffer space from stack! */
				/* Allow extra for word-align reads. */
				/* This should be +WDSIZE, but some */
				/* C compilers (eg XENIX) can't handle */
				/* "sizeof" arith in allocation stmts! */

	/* This flag and the two ptrs below are needed because UNIX
	 * maintains only one I/O ptr per open file, and we can sometimes
	 * be reading from/writing to the swapout file at same time.
	 * Using DUP() to get a new FD (to avoid seeking back and forth)
	 * won't help since both FD's will use the same I/O ptr!!!
	 * Lastly, can't depend on returned value of LSEEK to push/pop
	 * ptr, since V6 systems don't implement tell() or lseek() directly.
	 * So we have to do it by hand...
	 */
	int botchflg;
	chroff outptr, inptr;

	if((sd = sdp)==0) return;
	ofd = sbv_tf.sffd;		/* Default output FD */
	if(flag==0)
	  {	sbx_tset(sbx_qlen(sd),0);/* Find place for whole string */
		outptr = sbv_taddr;	/* We'll have to update wrt ptr */
	  }
	else if (flag==1)	/* Single SD block, so it's reasonable to 
				 * try aligning the output with the input. */
	  {	if(sm = sd->sdmem)
		  {	cnt = rndrem(sm->smaddr - sbm_lowaddr);
			sbx_tset((chroff)(sm->smuse),cnt);
		  }
		else
		  {	cnt = rndrem(sd->sdaddr);
			sbx_tset(sd->sdlen, cnt);
		  }
		outptr = sbv_taddr;	/* We'll have to update wrt ptr */
	  }
	else		/* Outputting a whole sbstring to a file */
	  {	ofd = fd;
		outptr = 0;
	  }

	for(; sd;)
	  {	if(flag==0 && sbx_qlk(sd))
			break;		/* Stop when hit locked sdblk */
		if(sm = sd->sdmem)
		  {	if(cnt = sm->smuse)
				if(write(ofd, sm->smaddr, cnt) != cnt)
					return(sbx_err(errno,"Swapout wrt err"));
			outptr += cnt;
			if(flag==0)
			  {	sd->sdmem = 0;	/* Flush the mem used */
				sbm_mfree(sm);
			  }
			inlen = cnt;
		  }
		else if(inlen = sd->sdlen)
		  {	if(sd->sdfile == 0)
				return(sbx_err(errno,"Sdfile 0, SD %o",sd));
			/* Foo on UNIX */
			botchflg = ((ifd = sd->sdfile->sffd) == ofd) ? 1 : 0;
			skflg = 1;		/* Always seek first time */
			inptr = sd->sdaddr;
			/* Efficiency hack - set up for first read so that
			 * transfer is word-aligned and terminates at end
			 * of a disk block.
			 */
			rem = rndrem(inptr);		/* Get alignment */
			cnt = SB_BUFSIZ - (int)(inptr%SB_BUFSIZ);
			while(inlen > 0)
			  {
				if(inlen < cnt) cnt = inlen;
				if(!sbx_rdf(ifd, buf+rem, cnt, skflg, inptr))
					return(sbx_err(errno,"Swapout err, SD %o",sd));
				/* Further seeks depend on botch setting */
				if(skflg = botchflg)
				  {	if(lseek(ofd,outptr,0) < 0)
						return(sbx_err(errno,
							"Swapout sk err"));
					inptr += cnt;
				  }
				if(write(ofd, buf+rem, cnt) != cnt)
					return(sbx_err(errno,
						"Swapout wrt err"));
				outptr += cnt;
				inlen -= cnt;
				cnt = SB_BUFSIZ; /* Now can use full blocks */
				rem = 0;	/* Aligned nicely, too! */
			  }
			inlen = sd->sdlen;
		  }

		/* Text written out, now merge block in */
		if(flag == 2)			/* No merge if saving file */
			goto donxt;
		if(sd != sdp)			/* First block? */
		  {	sdp->sdlen += inlen;	/* No, simple merge */
			sd = sbx_ndel(sd);	/* Flush, get next */
			continue;
		  }

		/* Handle 1st block specially */
		if(sd->sdfile		/* Unlink from phys list */
		  && sd != sbv_tsd)	/* Don't unlink if self */
			sbx_npdel(sd);
		sd->sdlen = inlen;
		sd->sdfile = &sbv_tf;
		sd->sdaddr = sbv_taddr;	/* Set from sbx_tset val */
		sd->sdflags &= ~SD_MOD;	/* On disk, no longer modified */

		/* Now insert into phys list at specified place */
		if(sd == sbv_tsd)	/* If already same place */
			goto next;	/* Skip linkin. */
		if(sd->sdback = sbv_tsd)
		  {	sd->sdforw = sbv_tsd->sdforw;
			sd->sdback->sdforw = sd;
		  }
		else
		  {	sd->sdforw = sbv_tf.sfptr1;
			sbv_tf.sfptr1 = sd;
		  }
		if(sd->sdforw)
			sd->sdforw->sdback = sd;

	next:	if(flag==1)		/* If only doing 1 sdblk, */
			break;		/* stop here. */
	donxt:	sd = sd->slforw;	/* Done with 1st, get next */
	  }
	return(0);			/* Win return, no errors */
}

/* Returns hackable length of a sbstring (ends at EOF or locked block) */
chroff
sbx_qlen(sdp)
struct sdblk *sdp;
{	register struct sdblk *sd;
	register struct smblk *sm;
	chroff len;

	len = 0;
	for(sd = sdp; sd && !sbx_qlk(sd); sd = sd->slforw)
		if(sm = sd->sdmem)
			len += (chroff)sm->smuse;
		else len += sd->sdlen;
	return(len);
}


/* SBX_TSET - finds a place on temp swapout file big enough to hold
 *	given # of chars.  Sets SBV_TADDR to that location, as well
 *	as seeking to it so the next write call will output there.
 *	This location is guaranteed to have the requested
 *	byte alignment (0 = word-aligned).
 */
sbx_tset(loff, align)
chroff loff;
int align;
{	register int fd;

	if(sbv_tf.sffd <= 0)
	  {		/* Must open the temp file! */
/* Temporary file mechanism is system-dependent.  Eventually this
** will probably require inclusion of a true c-env header file; for the
** time being, we can cheat a little by checking O_T20_WILD, which will
** be defined by <sys/file.h> on TOPS-20.  Otherwise, we assume we are
** on a real Unix.
*/
#ifdef O_T20_WILD
		extern char *tmpnam();	/* Use ANSI function */
		fd = open(tmpnam((char *)NULL),
				O_RDWR | O_CREAT | O_TRUNC | O_BINARY);
		if(fd < 0)
			return(sbx_err(0,"Swapout creat err"));		
#else /* Real Unix */
		static char fcp[] = "/tmp/sbd.XXXXXX";
		if((fd = creat(mktemp(fcp),0600)) < 0)
			return(sbx_err(0,"Swapout creat err"));
		/* Must re-open so that we can both read and write to it */
		close(fd);
		if((fd = open(fcp,2)) < 0)
			return(sbx_err(0,"Swapout open err"));
		unlink(fcp);	/* Set so it vanishes when we do */
#endif

		sbv_tf.sffd = fd;	/* Initialize the sbfile struct */
		sbv_tf.sfptr1 = 0;
		sbv_ftab[fd] = &sbv_tf;	/* Record in table of all sbfiles */
		sbv_taddr = 0;		/* "Return" this value */
		return;		/* Ignore alignment for now */
	  }
	sbv_tsd = sbx_ffnd(&sbv_tf, loff+align, &sbv_taddr);
	sbv_taddr += align;
	if(lseek(sbv_tf.sffd, sbv_taddr, 0) < 0)
		return(sbx_err(0,"Swapout seek err: (%d,%ld,0) %d %s",
			sbv_tf.sffd, sbv_taddr, errno, strerror(errno)));

}

/* SBX_FFND - searches disk list of given file for free space of
 *	at least size chars.  Note that list must be sorted by ascending
 *	disk addrs in order for this to work!  If sdaddrs are only
 *	changed in SBX_SPLIT this will be true.
 *	Sets "aloc" to disk address for writing (this is guaranteed to
 *	be word-aligned, for efficiency), and returns SD ptr to
 *	block which this addr should follow in the physical list.  If ptr
 *	is 0, it means addr should be 1st thing in list.
 */
struct sdblk *
sbx_ffnd(sfp, size, aloc)
SBFILE *sfp;
chroff size, *aloc;
{	register struct sdblk *sd, *sds, *sdl;
	chroff cur;

	cur = 0;
	sds = 0;
	sd = sfp->sfptr1;
redo:	for(; sd ; sd = (sds=sd)->sdforw)
	  {	if(cur < sd->sdaddr)		/* Gap seen? */
		  {	if(size <= (sd->sdaddr - cur))	/* Yes, big enuf? */
				break;			/* Yup! */
		  }					/* No, bump. */
		else if(cur >=(sd->sdaddr + sd->sdlen))	/* No gap but chk */
			continue;			/* No overlap, ok */
		/* Bump to next possible gap. */
		cur = sd->sdaddr + sd->sdlen;
		cur = (long)rndup(cur);	/* Round up to word boundary! */
	  }
	*aloc = cur;		/* Return winning addr */

	/* Perform verification check -- make sure this really is OK
	 * and complain if not.	 If this never blows up, eventually can
	 * take the check out.
	 */
	sdl = sd;
	for(sd = sfp->sfptr1; sd; sd = sd->sdforw)
	  {	if(cur < sd->sdaddr)
		  {	if(size <= (sd->sdaddr - cur))
				continue;
		  }
		else if(cur >= (sd->sdaddr + sd->sdlen))
			continue;

		sbx_err(0,"FFND blew it, but recovered. SD %o siz %ld",
			sd, size);
		sd = (sds = sdl)->sdforw;
		goto redo;
	  }


	return(sds);		/* Return ptr to block this addr follows */
}

sbx_rdf(fd,addr,cnt,skflg,loc)
register int fd;
char *addr;
int skflg;
chroff loc;
{	register int rres, eres;
	long lres;
	char *errtyp, *ftyp;
	chroff curlen;

	errno = 0;
	if(skflg && (lres = lseek(fd, (long)loc, 0)) == -1)
	  {	errtyp = "Sk err";
		goto errhan;
	  }
	if((rres = read(fd, addr, cnt)) != cnt)
	  {	lres = rres;
		errtyp = "Rd err";
		goto errhan;
	  }
	return(rres);
errhan:				/* Handle read or seek error */
	eres = errno;
	if(fd == sbv_tf.sffd)	/* See if dealing with swapout file */
	  {	ftyp = "(swap)";
		curlen = 0;
	  }
	else {			/* No, normal buffer file. */
		ftyp = "";
		curlen = sbx_fdlen(fd);
		if(sbv_ftab[fd] &&
		  (curlen != sbv_ftab[fd]->sflen))	/* File changed? */
			if(sbx_rugpull(fd))	/* Yes, handle special case */
				return(cnt);	/* Allow "win" return */
	  }
	sbx_err(0,"%s %d:%s, %ld:(%d%s,%o,%d)=%ld (fl %ld)",
			errtyp,	eres, strerror(eres),
			loc, fd, ftyp, addr, cnt, lres,
			curlen);
	return(0);
}

/* SBX_RUGPULL(fd) - Special routine called when package detects that
 *	the file indicated by the fd has changed since its original
 *	opening.  This can happen when a file is over-written by some
 *	other program (ED, for example).
 *	This means that all sdblks which reference this fd
 *	are probably bad.  Pass special error back up to the calling
 *	program to give it a chance at doing something.
 *	Extra credit: scan all sdblks and unpurify all which point to this
 *	file, so as to protect everything we still remember about it.
 *	Otherwise a GC could flush pure in-core portions.
 */
sbx_rugpull(fd)		/* FD already known to have entry in sbv_ftab */
register int fd;
{	int sbx_unpur();

	/* First scan all sdblks to save what we still have. */
	/* This does NOT remove the sdfile pointer, so we can still
	 * find blocks that are affected. */
	sbm_nfor(SM_DNODS, sizeof(struct sdblk), sbx_unpur, sbv_ftab[fd]);

	if((int)sbv_debug == 1 || !sbv_debug)
		return(0);			/* Nothing else we can do */
	return((*sbv_debug)(2,(int *)0,"",fd));	/* Let caller handle it */
}
sbx_unpur(sd, sf)		/* Auxiliary routine for SBX_RUGPULL */
register struct sdblk *sd;
register struct sbfile *sf;
{	if(sd->sdfile == sf	/* If sdblk belongs to affected file */
	  && sd->sdmem)		/* and has in-core version of text, */
		sd->sdflags |= SD_MOD;	/* then ensure core version is used */
}

sbx_err(val,str,a0,a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12)
char *str;
{	int *sptr;

	sptr = (int *) &sptr;	/* Point to self on stack */
	sptr += 5;		/* Point to return addr */
	if((int)sbv_debug == 1)
	  {	abort();
	  }
	if(sbv_debug)
		(*sbv_debug)(1,*sptr,str,a0,a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12);
	return(val);
}
