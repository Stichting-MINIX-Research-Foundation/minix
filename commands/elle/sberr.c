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

#define PRINT		/* Include printout stuff */

#include "sb.h"
#include <stdio.h>

extern struct smblk *sbm_nfl;
extern struct smblk *sbm_list;
extern struct sdblk *sbx_nfl;

#ifdef PRINT
#define PRF(stmt) {if(p) stmt;}
#define PRFBUG(str,stmt) {if(p) stmt;else return(str);}
#define PRFBAD(str,stmt) {if(p) stmt; return(str);}
#else
#define PRF(stmt) ;
#define PRFBUG(str,stmt) return(str);
#define PRFBAD(str,stmt) return(str);
#endif

#ifndef NPTRS
#define NPTRS (1000)		/* Catch loops of period less than this. */
#endif

int sbe_dec = 0;		/* Set nonzero to use decimal printout */

struct ptab {
	int pt_pflag;		/* Printflag value */
	char *pt_err;		/* Error string return */
	int pt_xerr;		/* Error index return */
	int pt_hidx;		/* Highest freelist entry */
	int pt_nsto;		/* # entries stored in table */
	int pt_cnt;		/* # of entry store attempts */
	struct smblk *pt_tab[NPTRS];
};

_PROTOTYPE( char *sbe_sdtab, (struct ptab *pt, int p, int phys) );
_PROTOTYPE( char *sbe_schk, (struct sdblk *sd, struct ptab *pt) );
_PROTOTYPE( int sbe_tbent, (struct ptab *pt, struct smblk *sm) );

#define PTF_PRF	01	/* Do printout stuff */
#define PTF_OVFERR 02	/* Complain if table overflows */
#define PTF_SDPHYS 04	/* Follow SD phys links (else logical links) */

struct flgt {
	int flg_bit;
	int flg_chr;
};

_PROTOTYPE( char *sbe_fstr, (int flags, struct flgt *fp) );

char *sbe_mvfy(), *sbe_mfl(), *sbe_mlst();		/* SBM */
char *sbe_sbvfy(), *sbe_sbs();				/* SBBUF */
char *sbe_svfy(), *sbe_sdlist(), *sbe_sdtab(), *sbe_schk();	/* SD */
char *sbe_fstr();				/* Misc utility */


/* SBE_MEM() - Print out memory usage list
*/
sbe_mem()
{
	printf("\nMemory Usage:\n");
	printf("\tsbm_nfl : %6o\n",sbm_nfl);
	printf("\tsbm_list: %6o\n",sbm_list);
	printf("\tsmblk nodes are %o bytes long.\n",sizeof (struct smblk));

	sbe_mlst(1);		/* Scan mem list, printing stuff. */
}

/* SBE_MVFY() - Verify memory allocation structures
 *	Returns error message (0 if no errors found).
 */
char *
sbe_mvfy()
{	register char *res;

	if((res = sbe_mfl(0))
	  || (res = sbe_mlst(0)))
		return(res);
	return(0);
}

/* SBM Debugging Routines */

struct flgt smflgtab[] = {
	SM_USE,	'U',
	SM_NXM, 'N',
	SM_EXT, 'E',
	SM_MNODS,'M',
	SM_DNODS,'D',
	0,0
};

static char smfhelp[] = "U-Used, N-NXM, E-External, M-SMnodes, D-SDnodes";
static char smhdline[] = "\
      SM: back   smaddr   smlen  smuse  smflags";

/* SBE_MFL(printflag) - Verify/Print memory freelist
 *	Returns error message (0 if no errors found).
 */
char *
sbe_mfl(p)
int p;
{	register struct smblk *sm;
	register int i;
	struct ptab smtab;		/* For loop detection */

	PRF(printf("Tracing SM node freelist --\n"))
	PRF(printf("    Maximum loop detection size is %d.", NPTRS))
	if((sm = sbm_nfl) == 0)
	  {	PRF(printf("\n\tNo list.\n"))
		return(0);			/* Null freelist is ok */
	  }
	smtab.pt_pflag = p ? PTF_PRF : 0;
	smtab.pt_nsto = smtab.pt_cnt = 0;
	i = 0;				/* Print 8 addrs/line */
	for(; sm; sm = sm->smforw)
	  {
		PRF(printf("%s%7o->", (i==0 ? "\n    " : ""), sm))
		if(++i >= 8) i = 0;
		if(sbe_tbent(&smtab, sm) < 0)	/* If hit loop, stop */
			PRFBAD("SM freelist loop",
			  printf("\nLOOP - %o seen as node %d!!\n",
				sm, smtab.pt_xerr))
		if(sm->smflags)
		  {	PRF((i = 0, printf("\nFreelist node has flags:\n")))
			PRFBUG("Free SM flagged", sbe_smp(sm, 0))
		  }
	  }
	PRF(printf("\nEnd - %d nodes on SM freelist.\n", smtab.pt_cnt))
	return(0);
}

/* SBE_MLST(printflag) - Verify/Print allocated memory list.
 *	Returns error message (0 if no errors found).
 */
char *
sbe_mlst(p)
int p;
{	register struct smblk *sm, *smf, *smb;
	char *nextaddr;
	int i;
	struct ptab smtab;		/* For loop detection */

	PRF(printf("Tracing mem list -- \n"))
	if((sm = sbm_list) == 0)
	  {	PRF(printf("\tNo list?!\n"))
		if(sbm_nfl)		/* Ensure rest are 0 too */
			return("No mem list?!!");
		return(0);
	  }

	smtab.pt_pflag = p;
	smtab.pt_cnt = smtab.pt_nsto = 0;
	smb = 0;
	PRF(printf("   Flags: %s\n%s\n", smfhelp, smhdline))
	for(; sm; sm = smf)
	  {	PRF(printf("  %6o: ",sm))
		if(sbe_tbent(&smtab, sm) < 0)
			PRFBAD("Loop in mem list!!",
			  printf("LOOP - seen as node %d!!\n", smtab.pt_xerr))

		if(sm->smback == smb)
			PRF(printf("^ "))	/* Back ptr OK */

		else PRFBUG("Bad back ptr!",
			printf("%6o BAD Backptr!!\n\t    ",sm->smback))

		if((sm->smflags&0377)!= SM_NID)
			PRFBUG("SM: bad node ID",
				printf("BAD - no node ID!\n\t    "))
		PRF(printf((sm->smflags&SM_USE) ? "     " : "FREE "))
		if(sm->smlen == 0)
			PRFBUG("SM: len 0",
				printf("Zero-length area!"))
		if((sm->smflags&SM_USE)==0
		  && rndrem(sm->smaddr - sbm_lowaddr))
			PRFBUG("Bad free-mem block",
				printf("Bad free-mem block"))
		PRF(sbe_smp(sm, 1))		/* Print out rest of info */

		if(nextaddr != sm->smaddr
		  && smtab.pt_cnt != 1)		/* 1st time needs init */
		  {	PRFBUG("Alignment error!",
				printf("\t  BAD!! %6o expected; ",nextaddr))
#if !(MINIX)
			PRF((i = sm->smaddr - nextaddr) > 0
				? printf("%d skipped.\n",i)
				: printf("%d overlapped.\n",-i))
#endif
		  }
		nextaddr = sm->smaddr + sm->smlen;
		smf = sm->smforw;
		smb = sm;			/* Save ptr to back */
	  }
	PRF(printf("End = %6o\n",nextaddr))
	return(0);
}

#ifdef PRINT
sbe_smp(sm,type)
register struct smblk *sm;
int type;
{
	if(type==0)
		printf("  %6o:  %s  ", sm,
			((sm->smflags&SM_USE) ? "    " : "FREE"));
	printf("%6o: ", sm->smaddr);
	printf((sbe_dec ? "%5d. %5d." : "%6o %6o"), sm->smlen, sm->smuse);
	printf("  %7o = %s\n", sm->smflags, sbe_fstr(sm->smflags, smflgtab));
}
#endif /*PRINT*/

/* SD (SBSTR) debugging routines */

struct flgt sdflgtab[] = {
	SD_LOCK, 'L',
	SD_LCK2, 'T',
	SD_MOD,	 '*',
	0,0
};

static char sdfhelp[] = "\
<f> flags: *-MOD (disk outofdate), L-LOCK, T-LCK2 (temp)";
static char sdhdline[] = "\
<f>      SD: slforw slback sdflgs sdforw sdback  sdmem sdfile  sdaddr sdlen";


/* SBE_SFL(printflag) - Verify/Print SD freelist
 *	Returns error message (0 if no errors found).
 */
char *
sbe_sfl(p)
int p;
{	register struct sdblk *sd;
	register int i;
	struct ptab sdtab;		/* For loop detection */

	PRF(printf("Tracing SDBLK node freelist --\n"))
	PRF(printf("    Maximum loop detection size is %d.", NPTRS))
	if((sd = sbx_nfl) == 0)
	  {	PRF(printf("\n\tNo list.\n"))
		return(0);			/* Null freelist is ok */
	  }
	sdtab.pt_pflag = p ? PTF_PRF : 0;
	sdtab.pt_nsto = sdtab.pt_cnt = 0;
	i = 0;				/* Print 8 addrs/line */
	for(; sd; sd = sd->slforw)
	  {
		PRF(printf("%s%7o->", (i==0 ? "\n    " : ""), sd))
		if(++i >= 8) i = 0;
		if(sbe_tbent(&sdtab, sd) < 0)	/* If hit loop, stop */
			PRFBAD("SD freelist loop",
			  printf("\nLOOP - %o seen as node %d!!",
				sd, sdtab.pt_xerr))
		if(sd->sdflags)
		  {	PRF((i = 0, printf("\nFreelist node has flags:\n")))
			PRFBUG("Free SD flagged", sbe_psd(sd))
		  }
	  }
	PRF(printf("\nEnd - %d nodes on SD freelist.\n", sdtab.pt_cnt))
	return(0);
}



/* SBE_SDS() - Print out all sdblk data stuff
 */
sbe_sds()
{	int sbe_psd();

	printf("Printout of all in-use SDBLKs:\n");
	printf("  %s\n", sdfhelp);
	printf("%s\n", sdhdline);
	sbm_nfor(SM_DNODS,sizeof(struct sdblk),sbe_psd,0);
	printf("\n");
}

/* SBE_PSD - Auxiliary for invocation by SBE_SDS above. */
sbe_psd(sd)
register struct sdblk *sd;
{	register int flags;

	flags = sd->sdflags;
	printf("%c%c%c",
		((flags&SD_MOD)  ? '*' : ' '),
		((flags&SD_LOCK) ? 'L' : ' '),
		((flags&SD_LCK2) ? 'T' : ' '));

	printf(" %7o: %6o %6o %6o %6o %6o %6o %6o %7lo %5ld.\n", sd,
		sd->slforw, sd->slback, sd->sdflags,
		sd->sdforw, sd->sdback, sd->sdmem,
		sd->sdfile, sd->sdaddr, sd->sdlen);
	return(0);
}

/* SBE_SVFY() - Verify all SD blocks
 *	Returns error message (0 if no errors found).
 */
char *
sbe_svfy()
{	register char *res;
	return((res = sbe_sdlist(0,0)) ? res : sbe_sdlist(0,1));
}

/* SBE_SDLIST(printflag, physflag) - Verify/Print all SD blocks.
 *	Show logical lists if physflag 0
 *	Show physical lists otherwise
 *	Returns error message (0 if no errors found).
 */
char *
sbe_sdlist(p,phys)
int p, phys;
{	register char *res;
	struct ptab sdtab;	/* The SDLIST table to use */

	/* First put freelist in table, then scan for all
	 * SD nodes.  Each active node (not in table) gets
	 * its entire list traced forward/backward and added to table.
	 */
	if(res = sbe_sdtab(&sdtab, p, phys))	/* Set up freelist table */
		return(res);

	/* Freelist entered in table, now scan all SD's */
	res = (char *)sbm_nfor(SM_DNODS,sizeof(struct sdblk),
			sbe_schk, &sdtab);

	PRF(printf("\n"))
	return(res);
}

/* SBE_SDTAB(tableptr, printflag, physflag) - Auxiliary for SBE_SDLIST.
 *	Stuffs all freelist SDBLK addresses in table for dup detection.
 *	Returns error message (0 if no errors found).
 */
char *
sbe_sdtab(pt, p, phys)
register struct ptab *pt;
int p, phys;
{	register struct sdblk *sd;
	register int res;

	pt->pt_pflag = (p ? PTF_PRF : 0) | (phys ? PTF_SDPHYS : 0)
			| PTF_OVFERR;
	pt->pt_cnt = pt->pt_nsto = 0;	/* Initialize */

	/* Stick freelist in table */
	for(sd = sbx_nfl; sd; sd = sd->slforw)
	  {	if(sbe_tbent(pt, sd) < 0)
		  {	if(pt->pt_xerr < 0)
				PRFBAD("SD freelist too long",
					printf("SD freelist too long (%d)\n",
						NPTRS))
			PRFBAD("SD freelist loop",
			  printf("SD freelist loop at %o\n", pt->pt_xerr))
		  }

		if(sd->sdflags)
		  {
			PRF(printf("Bad free SD, non-zero flag:\n"))
			PRFBUG("Free SD flagged", sbe_psd(sd))
		  }
	  }
	pt->pt_hidx = pt->pt_nsto;	/* Set idx of 1st non-FL entry */
	return(0);
}

/* SBE_SCHK(SDptr, tableptr) - Auxiliary for SBE_SDLIST.
 *	If SD not already in table, verifies or prints
 *	the complete physical or logical list it's on, and enters all
 *	of its SDs into table (to prevent doing it again).
 *	Returns 0 if no errors, else error string.
** There is a problem when the table overflows.  The tbent routine
** wants to add it (wrapping around at bottom) in that case, because
** that still helps detect loops.  But this routine wants to reset
** the table back (after scanning to head of list) and once it starts
** scanning forward again it will fail, because some of the SDs are
** still in the table due to the wraparound!  Thus PTF_OVFERR is always
** set, in order to at least give the right error message.
*/
char *
sbe_schk(sd, pt)
register struct sdblk *sd;
struct ptab *pt;
{	register struct sdblk *sdx;
	register struct smblk *sm;
	struct sbfile *savfile;
	chroff lastaddr;
	int p, res, savidx, phys;

	phys = pt->pt_pflag&PTF_SDPHYS;	/* Set up physflag */
	if(phys && (sd->sdfile == 0))	/* Ignore non-phys stuff if phys */
		return(0);
	p = pt->pt_pflag&PTF_PRF;	/* Set up printflag */
	savidx = pt->pt_nsto;		/* Remember initial extent of table */

	if(sbe_tbent(pt, sd) < 0)
	  {	if(pt->pt_xerr >= 0)	/* OK if already in table */
			return(0);
		PRFBAD("Too many SDs",
			printf("Too many SDs for table (%d)\n",	NPTRS))
	  }

	/* Now search backward for start of list */
	while(sdx = (phys ? sd->sdback : sd->slback))
		if(sbe_tbent(pt,sdx) >= 0)
			sd = sdx;
		else break;
	if(sdx)
	  {	if(pt->pt_xerr < 0)	/* Table error? */
			PRFBAD("Too many SDs",
				printf("Too many SDs for table (%d)\n",NPTRS))
		PRF(printf("Backlist loop!! Dup'd node:%s\n",
				(pt->pt_xerr < pt->pt_hidx) ?
					"(on freelist!)" : "" ))
		PRFBUG((phys ? "Phys SD loop" : "SD loop"), sbe_psd(sdx))
	  }
	/* Reset table to flush nodes backed over */
	pt->pt_cnt = pt->pt_nsto = savidx;

	/* SD now points to start of list.  Begin stepping thru list... */
	PRF(printf("---- %sList started: ", (phys ? "Phys " : "")))
	if(phys)
	  {	savfile = sd->sdfile;
		PRF(printf(" SF: %o, fd= %d, ln= %ld\n",
			savfile,savfile->sffd,savfile->sflen))
		if(savfile->sfptr1 != sd)
			PRFBUG("SFPTR1 bad",
			  printf("  BAD!! Sfptr1 %o doesn't match SD %o!!\n",
				savfile->sfptr1, sd))
		lastaddr = 0;
	  }
	else PRF(printf("\n"))

	PRF(printf("%s\n", sdhdline))
	for(sdx = 0; sd; (sdx = sd, sd = (phys ? sd->sdforw : sd->slforw)))
	  {
		PRF(sbe_psd(sd))	/* Print it out */
		if(sdx != (phys ? sd->sdback : sd->slback))
		  {	if(phys)
			  PRFBUG("PSD bad sdback",printf("\tBad phys backptr\n"))
			else
			  PRFBUG("SD bad slback",printf("\tBad backptr\n"))
		  }

		if((sd->sdflags&0377) != SD_NID)
			PRFBUG("Bad SD node ID", printf("\tBad node ID!\n"))


		if(sd->sdfile && (sd->sdlen < 0 || sd->sdaddr < 0))
			PRFBUG("SD: neg len/addr",
				printf("\tNeg disk len/addr\n"))
		if(phys) goto dophys;

		/* Do special stuff for logical list */
		if(sm = sd->sdmem)
		  {	if((sm->smflags&0377) != SM_NID)
				PRFBUG("SD: bad SM",
					printf("\nBad SMBLK ptr\n"))
			if((sd->sdflags&SD_MOD)==0
			  && sd->sdlen != sm->smuse)
				PRFBUG("SD != SM",
					printf("\tBad SMBLK? Len conflict\n"))
			if(sm->smlen < sm->smuse)
				PRFBUG("SD: SM len < use",
					printf("\tBad SMBLK, len < use\n"))
		  }
		goto doboth;	/* Skip phys stuff */

		/* Do special stuff for phys list */
	dophys:	if(sd->sdfile != savfile)
			PRFBUG("SD: bad sdfile",
				printf("\tBad sdfile ptr! Shd be %o\n",
					savfile))
		if(sd->sdaddr < lastaddr)
			PRFBUG("SD addr out of order",
				printf("\tBad disk addr, not in order!\n"))
		lastaddr = sd->sdaddr;
		/* Done with special phys stuff */

	doboth:	if(sbe_tbent(pt, sd) < 0)
		  {	if(pt->pt_xerr < 0)
				PRFBAD("Too many SDs",
					printf("Too many SDs for table (%d)\n",NPTRS))

			PRFBUG("SD loop",
				printf("\tLOOP!! This SD already seen%s.\n",
					(pt->pt_xerr < pt->pt_hidx) ?
					" (on freelist!)" : "" ))
			break;
		  }
	  }
	PRF(printf("-----------\n"))
	return(0);
}

/* SBE_DSK(SFptr) - Print out disk usage list for specific file
 */

sbe_dsk(sfp)
SBFILE *sfp;
{
	printf("SBFILE printout not coded: %o\n",sfp);
}

/* SBBUF structure debugging routines */

struct flgt sbflgtab[] = {
	SB_OVW, 'O',
	SB_WRIT,'W',
	0,0
};
static char sbfhelp[] = "O-Overwrite, W-Write";

/* SBE_SBVFY(SBptr) - Verify a SB-string.
 *	Returns error message (0 if no errors found).
 */
char *
sbe_sbvfy(sbp)
SBBUF *sbp;
{	return(sbe_sbs(sbp,0));
}

/* SBE_SBS(SBptr, printflag) - Verify/Print SBSTR data stuff
 *	Returns error message (0 if no errors found).
 */
char *
sbe_sbs(sbp,p)
SBBUF *sbp;
int p;
{	register SBBUF *sb;
	register struct smblk *sm;
	register struct sdblk *sd;

	sb = sbp;
	PRF(printf("SBSTR %o: ",sb))
	if(sb == 0)
		PRFBUG(0,printf("Zero pointer???\n"))

	/* First print out cryptic summary in case pointers bomb
	 * out farther on. */
	PRF(printf(" (io,cur,r,w,f,.,+ = %o,%o,%d,%d,%o,%lo,%lo)\n",
		sb->sbiop, sb->sbcur, sb->sbrleft, sb->sbwleft,
		sb->sbflags, sb->sbdot, sb->sboff))

	PRF(printf("  sbflags %5o = %s (%s)\n",
			sb->sbflags, sbe_fstr(sb->sbflags,sbflgtab),
			sbfhelp))

	if(sd = sb->sbcur)	/* Okay, now try getting pointers */
		sm = sd->sdmem;
	else sm = 0;

	PRF(printf("  sbcur %6o",sd))
	if(sd)
	  {
		PRF(printf("\n   %s\n   ", sdhdline))
		PRF(sbe_psd(sd))

		if((sd->sdflags&0377) != SD_NID)
			PRFBUG("SBCUR not SD?",printf("   BAD SDBLK ID!! \n"))
		if(sm)
		  {
			PRF(printf("   %s\n   ", smhdline))
			PRF(sbe_smp(sm,0))
			if((sm->smflags&0377) != SM_NID)
				PRFBUG("SBCUR has bad SM",
					printf("   BAD SMBLK ID!!\n"))
		  }
	  }


	PRF(printf("  sbiop  %6o",sb->sbiop))
	if(sb->sbiop)
	  {	if(!sm || sb->sbiop < sm->smaddr
		  || sb->sbiop > (sm->smaddr + sm->smlen))
			PRFBUG("Bad SBIOP", printf("  BAD"))
	  }
	else if(sb->sbrleft > 0 || sb->sbwleft > 0)
		PRFBUG("Bad SBIOP/cnts", printf("  BAD"))
	PRF(printf("\n"))

	PRF(printf("  sbrleft %5o = %5d.",sb->sbrleft, sb->sbrleft))
	if(sb->sbrleft
	  && (	!sm
	    ||	sb->sbwleft
	    ||	(sb->sbflags&SB_WRIT)
	    ||	(sb->sbrleft != (sm->smuse - (sb->sbiop - sm->smaddr)))
	    ))
		PRFBUG("Bad sbrleft", printf("  BAD"))
	PRF(printf("\n"))

	PRF(printf("  sbwleft %5o = %5d.", sb->sbwleft, sb->sbwleft))
	if(sb->sbwleft
	  && (	!sm
	    ||	(sb->sbflags&SB_WRIT) == 0
	    ||	(sb->sbwleft > (sm->smlen - (sb->sbiop - sm->smaddr)))
	    ))
		PRFBUG("Bad sbwleft", printf("  BAD"))
	PRF(printf("\n"))

	PRF(printf("  sbdot %7lo = %7ld.", sb->sbdot, sb->sbdot))
	if(sb->sbdot < 0)
		PRFBUG("Bad sbdot", printf("  BAD"))

	PRF(printf("\n  sboff %7lo = %7ld.\n", sb->sboff, sb->sboff))
	PRF(printf("  I/O ptr loc: %ld.\n\n", sb_tell(sb)))

	return(0);
}

/* SBE_TBENT() - Auxiliary to add and check entries to a pointer table.
 *	Note we assume here that smblk ptrs are used, although sdblks
 *	can also be hacked.  This wins as long as the two kinds of ptrs
 *	are basically identical (saves horrible casting problems).
 *	Returns index # if successful (between 0 and NPTRS-1 inclusive).
 *	Otherwise an error (-1), with relevant info in pt_xerr:
 *		-1 if out of room and flag set making it an error
 *		0-n if entry already existed.
 */
sbe_tbent(pt, sm)
register struct ptab *pt;
struct smblk *sm;
{	register struct smblk **smt;
	register int i;
	int p;

	p = pt->pt_pflag&PTF_PRF;	/* Set up print flag */
	smt = &(pt->pt_tab[0]);
	if(i = pt->pt_nsto)
	  {	do {
			if(sm == *smt++)
			  {	pt->pt_xerr = pt->pt_nsto - i;
				return(-1);
			  }
		  } while(--i);
		--smt;
	  }

	i = pt->pt_cnt++;
	if(++(pt->pt_nsto) > NPTRS)
	  {	if(pt->pt_pflag&PTF_OVFERR)
		  {	pt->pt_err = "Ptrtab overflow";
			pt->pt_xerr = -1;
			return(-1);
		  }
		pt->pt_nsto = NPTRS;
		i %= NPTRS;
	  }
	pt->pt_tab[i] = sm;
	return(i);
}

/* SBE_FSTR(flags, flagtab) - Auxiliary to convert flag word to a string
 *	and return pointer to it.  Handy for printfs.
 */
char *
sbe_fstr(flags, fp)
register int flags;
register struct flgt *fp;
{	static char retstr[17];	/* Max of 16 flags */
	register char *cp;
	cp = retstr;
	for(; fp->flg_bit; ++fp)
		*cp++ = (fp->flg_bit&flags) ? fp->flg_chr : ' ';
	*cp = 0;
	return(retstr);
}
