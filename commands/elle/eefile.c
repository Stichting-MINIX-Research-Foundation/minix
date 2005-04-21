/* ELLE - Copyright 1982, 1984, 1987 by Ken Harrenstien, SRI International
 *	This software is quasi-public; it may be used freely with
 *	like software, but may NOT be sold or made part of licensed
	 *	products without permission of the author.
 */
/*
 * EEFILE	File reading/writing functions
 */

#include "elle.h"
#include <stdio.h>	/* Use "standard" I/O package for writing */
#ifndef BUFSIZ
#define BUFSIZ BUFSIZE	/* Some places use wrong name in stdio.h */
#endif /*-BUFSIZ*/
#if V6
	struct stat {
		int st_dev;
		int st_ino;
		char *st_mode;
		char st_nlink;
		char st_uid;
		char st_gid;
		char st_size0;
		char st_size;
		int st_addr[8];
		long st_atime;
		long st_mtime;
	};
#define ENOENT (2)	/* Syscall error - no such file or dir */
#else
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#endif /*-V6*/

#if TOPS20
#include <sys/file.h>		/* Get open mode bits */
#endif

extern char *strerror();		/* Return error string for errno */
extern struct buffer *make_buf(), *find_buf();

char *fncons(), *last_fname();

int hoardfd = -1;	/* Retain a FD here to ensure we can always write */

/* Flags for iwritfile() */
#define WF_SMASK 07	/* Source Mask */
#define WF_SBUFF  0	/*   source: Buffer */
#define WF_SREG   1	/*   source: Region */
#define WF_SKILL 2	/*   source: Last Kill */
#define WF_ASK 010	/* Ask for filename to write to */
static int iwritfile();

/* EFUN: "Find File" */
/*	Ask user for a filename and do a find_file for it.
 *	If buffer exists for that filename, select that buffer.
 *	Else create a buffer for it, and read in the file if it exists.
 */
f_ffile()
{	int find_file();
#if IMAGEN
	hack_file("Visit file: ", find_file);
#else
	hack_file("Find file: ", find_file);
#endif /*-IMAGEN*/
}

/* EFUN: "Read File" */
/*	User read_file function, asks user for a filename and reads it
 */
f_rfile() { u_r_file("Read file: "); }

/* EFUN: "Visit File" */
/*	Same as Read File, with different prompt.
 */
f_vfile() { u_r_file("Visit file: "); }


u_r_file(prompt)
char *prompt;
{	register char *f_name;
	register struct buffer *b;

	if((f_name = ask (prompt))==0)	/* prompt user for filename */
		return;				/* user punted... */
	b = cur_buf;
	if(*f_name == '\0')
	  {	if (b -> b_fn == 0)
			ding("No default file name.");
		else read_file(b -> b_fn);
	  }
	else read_file(f_name);
	chkfree(f_name);
}

/* EFUN: "Insert File" */
/*	Asks for a filename and inserts the file at current location.
 *	Point is left at beginning, and the mark at the end.
 */
f_ifile()
{	int ins_file();
	hack_file("Insert file: ", ins_file);
}

/* EFUN: "Save File" */
/*	Save current buffer to its default file name
 */
f_sfile()
{	if(cur_buf->b_flags&B_MODIFIED)
		return(iwritfile(WF_SBUFF));	/* Write buffer, don't ask */
	else
	  {	saynow("(No changes need to be written)");
		return(1);
	  }
}

#if FX_SAVEFILES || FX_WFEXIT
/* EFUN: "Save All Files" */
/*  F_SAVEFILES - Offer to save all modified files.
 *	With argument, doesn't ask.
 *	Returns 0 if user aborts or if an error happened.
 */
f_savefiles()
{	register struct buffer *b, *savb;
	register int res = 1;
	char *ans;

	savb = cur_buf;
	for (b = buf_head; res && b; b = b->b_next)
		if ((b->b_flags & B_MODIFIED) && b->b_fn)
		  {	if(exp_p)		/* If arg, */
			  {	chg_buf(b);	/* just save, don't ask */
				res = f_sfile();
				continue;	/* Check next buffer */
			  }
			/* Ask user whether to save */
			ans = ask("Buffer %s contains changes - write out? ",
					b->b_name);
			if(ans == 0)
			  {	res = 0;	/* User aborted */
				break;
			  }
			if (upcase(*ans) == 'Y')
			  {	chg_buf(b);
				res = f_sfile();	/* Save File */
			  }
			chkfree(ans);
		  }
	chg_buf(savb);
	return(res);
}
#endif /*FX_SAVEFILES||FX_WFEXIT*/

/* EFUN: "Write File" */
/*	Write out the buffer to an output file.
 */
f_wfile()
{	return iwritfile(WF_ASK|WF_SBUFF);
}

/* EFUN: "Write Region" */
/*	Write region out to a file
 */
f_wreg()
{	return iwritfile(WF_ASK|WF_SREG);	/* Ask, write region */
}

#if FX_WLASTKILL
/* EFUN: "Write Last Kill" (not EMACS) */
/*	Write current kill buffer out to a file.
**	This is mainly for MINIX.
*/
extern int kill_ptr;		/* From EEF3 */
extern SBSTR *kill_ring[];

f_wlastkill()
{	return iwritfile(WF_ASK|WF_SKILL);
}
#endif


/* HACK_FILE - intermediate subroutine
 */
hack_file(prompt, rtn)
char *prompt;
int (*rtn)();
{	register char *f_name;

	if((f_name = ask(prompt)) == 0)
		return;
	if (*f_name != '\0')			/* Check for null answer */
		(*rtn)(f_name);
	chkfree(f_name);
}

/* FIND_FILE(f_name)
 *	If there is a buffer whose fn == f_name, select that buffer.
 *	Else create one with name of the last section of f_name and
 *	read the file into that buffer.
 */
find_file(f_name)
register char *f_name;
{	register struct buffer *b;
	register char *ans;
	char *lastn;
	int fd;

#if IMAGEN
	char real_name[128];		/* File name w/ expanded ~ and $ */
	expand_file(real_name, f_name);
	f_name = real_name;
#endif /*IMAGEN*/

	for (b = buf_head; b; b = b -> b_next)
		if(b->b_fn && (strcmp (b -> b_fn, f_name) == 0))
			break;
	if (b)				/* if we found one */
	  {	sel_buf(b);		/* go there */
		return;			/* and we are done */
	  }
	if((fd = open(f_name,0)) < 0)	/* See if file exists */
	  {	if(errno != ENOENT)	/* No, check reason */
		  {	ferr_ropn();	/* Strange error, complain */
			return;		/* and do nothing else. */
		  }
	  }
	else close(fd);			/* Found!  Close FD, since the */
					/* read_file rtn will re-open. */

	lastn = last_fname(f_name);	/* Find buffer name */
	b = find_buf(lastn);		/* Is there a buffer of that name? */
	if (b && (ex_blen(b) || b->b_fn))
	  {	ans = ask("Buffer %s contains %s, which buffer shall I use? ",
    			b -> b_name, b->b_fn ? b->b_fn : "something");
		if(ans == 0) return;		/* Aborted */
		if (*ans != '\0')		/* if null answer, use b */
			b = make_buf(ans);	/* else use ans */
		chkfree(ans);
	  }
	else
		b = make_buf(lastn);
	sel_buf(b);
	if(fd < 0)		/* If file doesn't exist, */
	  {	set_fn(f_name);	/* just say "new" and set filename */
		return;		/* and return right away. */
	  }
	if (read_file(f_name)==0)	/* File exists, read it in! */
	  {	if(b->b_fn)		/* Failed... if filename, */
		  {	chkfree(b->b_fn);	/* flush the filename. */
			b->b_fn = 0;
		  }
	  }
}

/* READ_FILE(f_name)
 *	Reads file into current buffer, flushing any
 *	previous contents (if buffer modified, will ask about saving)
 *	Returns 0 if failed.
 */
read_file(f_name)
char *f_name;
{
#if IMAGEN
	struct stat s;
	char real_name[128];		/* File name w/ expanded ~ and $ */
#endif /*IMAGEN*/

	if(!zap_buffer())	/* Flush the whole buffer */
		return;		/* Unless user aborts */
#if IMAGEN
	expand_file(real_name, f_name);
	f_name = real_name;		/* Hack, hack! */
#endif /*IMAGEN*/
	set_fn(f_name);
	if (ins_file(f_name)==0)
		return 0;
	f_bufnotmod();		/* Say not modified now */
#if IMAGEN
	stat(f_name, &s);		/* Get file stat */
	cur_buf->b_mtime = s.st_mtime;	/*  and pick out last-modified time */
#endif /*IMAGEN*/
	return 1;
}

/* INS_FILE(f_name)
 *	Inserts file named f_name into current buffer at current point
 *	Point is not moved; mark is set to end of inserted stuff.
 *	Returns 0 if failed, 1 if won.
 */
ins_file (f_name)
char *f_name;
{	register int ifd;
	register SBSTR *sd;
	chroff insdot;			/* To check for range of mods */

#if IMAGEN
	char real_name[128];		/* File name w/ expanded ~ and $ */
	expand_file(real_name, f_name);
	f_name = real_name;
#endif /*IMAGEN*/
#if !(TOPS20)
	if((ifd = open(f_name,0)) < 0)
#else
	if((ifd = open(f_name,O_RDONLY|O_UNCONVERTED)) < 0)
#endif /*TOPS20*/
	  {	ferr_ropn();		/* Can't open, complain */
		return 0;		/* no redisplay */
	  }
	errno = 0;
	if((sd = sb_fduse(ifd)) == 0)
	  {	if (ifd >= SB_NFILES)
			dingtoo(" Cannot read - too many internal files");
		else if (errno)
			ferr_ropn();
		else errbarf("SB rtn cannot read file?");
		close(ifd);
		return 0;
	  }
	sb_sins(cur_buf,sd);
	insdot = e_dot();
	f_setmark();			/* Set mark at current ptr */
	if(cur_dot != insdot)		/* If pointer was advanced, */
		buf_tmat(insdot);	/* then stuff was inserted */
	e_gocur();
	return 1;
}

ferr_ropn() { ferr(" Cannot read"); }
ferr_wopn() { ferr(" Cannot write"); }
ferr(str)
char *str;
{	saytoo(str);
	saytoo(" - ");
	dingtoo(strerror(errno));
}


/* IWRITFILE - auxiliary for writing files.
**	Returns 1 if write successful, 0 if not.
*/
static int
iwritfile(flags)
int flags;
{	register struct buffer *b;
	register char *o_name;		/* output file name */
	int styp = flags & WF_SMASK;	/* Source type, one of WF_Sxxx */
	char *prompt;
#ifdef STDWRITE
	register FILE *o_file;		/* output file pointer */
	char obuf[BUFSIZ];
	chroff dotcnt;
#endif /*STDWRITE*/
	int ofd;			/* output file FD */
	SBSTR *sd;
	char fname[FNAMSIZ];		/* To avoid chkfree hassle */
	char newname[FNAMSIZ];		/* for robustness */
	char oldname[FNAMSIZ];		/* ditto */
	int res;
	struct stat statb;
	int statres;
#if IMAGEN
	struct stat s;
	char real_name[128];		/* File name w/ expanded ~ and $ */
#endif /*IMAGEN*/
	res = 1;			/* Let's keep track of success */

	/* Check for existence of source, and set prompt string */
	switch(styp)
	  {
		case WF_SBUFF:
			prompt = "Write File: ";
			break;
		case WF_SREG:
			if(!mark_p)
			  {	dingtoo(" No Mark!");
				return(0);
			  }
			prompt = "Write Region: ";
			break;
#if FX_WLASTKILL
		case WF_SKILL:
			if(!kill_ring[kill_ptr])
			  {	dingtoo("No killed stuff");
				return(0);
			  }
			prompt = "Write Last Kill: ";
			break;
#endif
		default:			/* Internal error */
			errbarf("bad iwritfile arg");
			return 0;
	  }

	if (flags&WF_ASK)
	  {	if((o_name = ask(prompt))==0)
			return(0);		/* User punted. */
		strcpy(&fname[0], o_name);	/* Copy filename onto stack */
		chkfree(o_name);
	  }
	o_name = &fname[0];
	b = cur_buf;
	if (!(flags&WF_ASK) || (*o_name == '\0'))
	  {	if (b->b_fn == 0)
		  {	ding("No default file name.");
			return(0);
		  }
		strcpy(o_name, b->b_fn);
	  }

#if IMAGEN
	expand_file(real_name, o_name);
	o_name = real_name;		/* Hack, hack */
#endif /*IMAGEN*/

	statres = stat(o_name,&statb);	/* Get old file's info (if any) */

#if IMAGEN
	/* Now, make sure someone hasn't written the file behind our backs */
	if ((styp==WF_SBUFF) && !(flags&WF_ASK)
	  && b->b_fn && stat(b->b_fn, &s) >= 0)
		if (s.st_mtime != b->b_mtime)
		  {	char *ans;
			ans = ask("Since you last read \"%s\", someone has changed it.\nDo you want to write it anyway (NOT RECOMMENDED!)? ",
				   b->b_fn);
			if (ans == 0 || upcase(*ans) != 'Y')
			  {
				ding("I suggest you either read it again, or\nwrite it to a temporary file, and merge the two versions manually.");
				if (ans) chkfree(ans);
				return(0);
			  }
			if (ans) chkfree(ans);
		  }
#endif /*IMAGEN*/

  /* Try to get around major UNIX screw of smashing files.
   * This still isn't perfect (screws up with long filenames) but...
   * 1. Write out to <newname>
   * 2. Rename <name> to <oldname> (may have to delete existing <oldname>)
   * 3. Rename <newname> to <name>.
   */
	fncons(oldname,ev_fno1,o_name,ev_fno2);	/* Set up "old" filename */
	fncons(newname,ev_fnn1,o_name,ev_fnn2);	/* Set up "new" filename */
	unlink(newname);			/* Ensure we don't clobber */
	unhoard();				/* Now give up saved FD */
#if !(V6)	/* Standard V6 doesn't have access call */
	if(statres >= 0)			/* If file exists, */
	  {	if(access(o_name, 2) != 0)	/* check for write access */
		  {	ferr_wopn();
			res = 0;	/* Failure */
			goto wdone;
		  }
	  }
#endif /*-V6*/
#ifdef STDWRITE
	if(flags&WF_ASK)
	  {	if((o_file = fopen(newname, "w")) ==0)	/* Create new output file */
		  {	ferr_wopn();
			res = 0;		/* Failure */
			goto wdone;
		  }
		setbuf(o_file,obuf);	/* Ensure always have buffer */
	  }
	else	/* New stuff */
#endif /*STDWRITE*/
	  {
#if !(TOPS20)
		if((ofd = creat(newname,ev_filmod)) < 0)
#else
		if((ofd = open(newname,O_WRONLY|O_UNCONVERTED)) < 0)
#endif /*TOPS20*/
		  {	ferr_wopn();
			res = 0;		/* Failure */
			goto wdone;
		  }
	  }
	if (styp==WF_SBUFF)
		set_fn(o_name);		/* Won, so set default fn for buff */
#if IMAGEN
	saynow("Writing ");
	switch(styp)
	  {	case WF_SBUFF:	saytoo(b->b_fn); break;
		case WF_SREG:	saytoo("region"); break;
#if FX_WLASTKILL
		case WF_SKILL:	saytoo("last kill"); break;
#endif
	  }
	sayntoo("...");
#else
	saynow("Writing...");
#endif /*-IMAGEN*/

#if !(TOPS20)			/* T20 does all this already */
	if(statres >= 0)		/* Get old file's modes */
	  {				/* Try to duplicate them */
		/* Do chmod first since after changing owner we may not
		** have permission to change mode, at least on V6.
		*/
		chmod(newname,statb.st_mode & 07777);
#if V6
		chown(newname, (statb.st_gid<<8)|(statb.st_uid&0377));
#else
		chown(newname,statb.st_uid,statb.st_gid);
#endif /*-V6*/
	  }
#if V6
	/* If no old file existed, and we are a V6 system, try to set
	 * the modes explicitly.  On V7 we're OK because the user can
	 * diddle "umask" to get whatever is desired.
	 * On TOPS-20 of course everything is all peachy.
	 */
	else chmod(newname, ev_filmod);
#endif /*V6*/
#endif /*TOPS20*/


#ifdef STDWRITE
	if(flags&WF_ASK)
	  {	switch(styp)
		  {
			case WF_SBUFF:
				dotcnt = e_blen();
				e_gobob();
				break;
			case WF_SREG:
				if((dotcnt = mark_dot - cur_dot) < 0)
				  {	e_goff(dotcnt);
					dotcnt = -dotcnt;
				  }
				else e_gocur();
				break;
			/* WF_SKILL not implemented here */
		  }
		while(--dotcnt >= 0)
			putc(sb_getc(((SBBUF *)b)), o_file);
		e_gocur();
		fflush(o_file);			/* Force everything out */
		res = ferror(o_file);		/* Save result of stuff */
		fclose(o_file);			/* Now flush FD */
	  }
	else	/* New stuff */
#endif /*STDWRITE*/
	  {
		switch(styp)
		  {
			case WF_SBUFF:
				res = sb_fsave((SBBUF *)b, ofd);
				break;
			case WF_SREG:
				e_gocur();
				sd = e_copyn((chroff)(mark_dot - cur_dot));
				res = sbx_aout(sd, 2, ofd);
				sbs_del(sd);
				break;
#if FX_WLASTKILL
			case WF_SKILL:
				res = sbx_aout(kill_ring[kill_ptr], 2, ofd);
				break;
#endif
		  }
		close(ofd);
	  }
	if(errno = res)
	  {	ferr(" Output error");
		res = 0;		/* Failure */
		goto wdone;
	  }
	else
		res = 1;		/* Success so far */
	if(styp == WF_SBUFF)
		f_bufnotmod();		/* Reset "buffer modified" flag */

	/* Here we effect the screw-prevention steps explained earlier. */
	/* TOPS-20, with generation numbers, need not worry about this. */
#if TOPS20
	saynow("Written");

#else /*-TOPS20*/
#if IMAGEN	/* KLH -- This conditional bracketting is prone to lossage */
	/* Only create the .BAK file once per editing session!! */
	if ((styp==WF_SBUFF) || !(b->b_flags & B_BACKEDUP))
	  {	if (styp==WF_SBUFF)
			b->b_flags |= B_BACKEDUP;
#endif /*IMAGEN*/
	unlink(oldname);	/* remove any existing "old" file */
	if(link(o_name,oldname) == 0)	/* Rename current to "old" */
	 	unlink(o_name);
		/* Here is the critical point... if we stop here, there is no
		 * longer any file with the appropriate filename!!!
		 */
#if IMAGEN
	  }
	else
		unlink(o_name);
#endif /*IMAGEN*/
	if(link(newname,o_name) == 0)	/* Rename "new" to current */
	  {	unlink(newname);
#if IMAGEN
		sayntoo("OK");
#else
		saynow("Written");
#endif /*-IMAGEN*/
	  }
	else
	  {	dingtoo("rename error!");
		res = 0;
	  }
#endif /*-TOPS20*/

#if IMAGEN
	/* Update the last-modified time for the file in this buffer */
	if ((styp == WF_SBUFF) && b->b_fn)
	  {	stat(b->b_fn, &s);
		b->b_mtime = s.st_mtime;
	  }
#endif /*IMAGEN*/

wdone:
	hoard();			/* Get back a retained FD */
	return(res);
}

/* FNCONS(dest,pre,f_name,post)
 *	Specialized routine to cons up a filename string into "dest",
 *	given prefix and postfix strings to be added onto last component of
 *	filename.
 */
char *
fncons(dest, pre, f_name, post)
char *dest,*pre,*f_name,*post;
{	register char *cp, *cp2;
	char *last_fname();

	cp = dest;
	*cp = 0;			/* Make dest string null initially */
	cp2 = last_fname(f_name);	/* Get pointer to beg of last name */
	strncat(cp,f_name,cp2-f_name);	/* Copy first part of filename */
	if(pre)	strcat(cp, pre);	/* If prefix exists, add it on */
	cp = last_fname(cp);		/* Recheck in case levels added */
	strcat(cp, cp2);		/* Now add last name */
	if(cp2 = post)			/* If there's a postfix, must check */
	  {	cp[FNAMELEN-strlen(cp2)] = 0;	/* and cut dest so postfix */
		strcat(cp, cp2);		/* will fit on end. */
	  }
	return(dest);
}

/* LAST_FNAME(string)
 *	Get the last component of a file name.  Returns pointer to
 *	start of component; does NOT copy string!
 */
char *
last_fname(f_name)
char *f_name;
{	register char *cp, *p;
	register int c;

	p = f_name;		/* pointer to last slash */
	cp = p;
	while(c = *cp++)
		if(c == '/')
			p = cp;		/* point to after the slash */
	return(p);
}

/* SET_FN(string)
 *	Set the default filename for current buffer to "string".
 */
set_fn (string)
char *string;
{	register struct buffer *b;
	register char *str;
#if IMAGEN
	register char *cp;
	register int len;
#endif /*IMAGEN*/
	char *strdup();

	b = cur_buf;
	str = strdup(string);		/* Copy now in case copying self */
	if(b->b_fn)
		chkfree(b->b_fn);
	b -> b_fn = str;
#if IMAGEN
	/* Do mode determination based on file name (HACK HACK) */
	len = strlen(str);
	b->b_flags &= ~(B_CMODE|B_TEXTMODE);
	if (len > 4)
	  {	if (strcmp(&str[len - 5], "draft") == 0)
			b->b_flags |= B_TEXTMODE;
		else
		  {	cp = &str[len - 4];
			if (strcmp(cp, ".txt") == 0 ||
			    strcmp(cp, ".mss") == 0)
				b->b_flags |= B_TEXTMODE;
		  }
	  }
	if (len > 2)
	  {	cp = &str[len - 2];
		if (strcmp(cp, ".h") == 0 || strcmp(cp, ".c") == 0)
			b->b_flags |= B_CMODE;
	  }
#endif /*IMAGEN*/
	redp(RD_MODE);
}

/* SAVEWORLD - Attempt to save all changes user has made.
 *	Currently this amounts to writing out all modified buffers
 *	to the files $HOME/+buffername.  If a buffer is given as argument,
 *	only that buffer is saved.
 *	This is only called from the error handling routines with
 *	the TTY either gone or in normal (non-edit) mode.  The "grunt"
 *	flag says whether to output feedback during the saving process.
 */
saveworld(bp, grunt)
struct buffer *bp;
int grunt;
{	register struct buffer *b;
	register int wfd;
	char sfname[FNAMSIZ];
	struct buffer *sel_mbuf();

	unhoard();		/* Ensure a FD is free for writing */
	if(b = bp) goto once;
	while(!bp && (b = sel_mbuf(b)))
	  {
	once:	strcat(strcat(strcpy(sfname,homedir),"/+"),b->b_name);
		if(grunt) printf("Saving %s...",sfname);
#if !(TOPS20)
		if((wfd = creat(sfname, ev_filmod)) < 0)
#else
		if((wfd = open(sfname,O_WRONLY|O_UNCONVERTED)) < 0)
#endif /*TOPS20*/
		  {	if(grunt)
				printf(" error - %s\n", strerror(errno));
		  }
		else
		  {	sb_fsave((SBBUF *)b, wfd);
			close(wfd);
			if(grunt) printf("\n");
		  }
		b->b_flags &= ~B_MODIFIED;
	  }
	hoard();
}

/* HOARD, UNHOARD - Routines to save a FD for writing, to make sure
 *	that we can always write out a buffer no matter how many
 *	file descriptors we are currently using.
 */
hoard()			/* Stash away a FD */
{	if(hoardfd <= 0)
#if !(TOPS20)
		hoardfd = open("nul:", 1);
#else
		hoardfd = open("/dev/null", 1);
#endif
}
unhoard()		/* Give up our stashed FD so it can be re-used */
{	close(hoardfd);
	hoardfd = -1;
}

#if IMAGEN
#include <pwd.h>
#include <ctype.h>

/*
 * expand_file: expand any ~user-name/ or $env-var/ prefixes in sfn,
 * producing the full name in dfn
 */
expand_file(dfn, sfn)
register char *dfn, *sfn;
{
	register char *sp, *tp;
	register int c;
	register struct passwd *pw;
	char ts[128];

	/* HORRIBLE, GROSS, DISGUSTING HACK: if the destination and
	 * source strings are identical (same pointer), then do not
	 * do any expansion--this happens to work with the current
	 * structure very well, since multiple expansions may happen.
	 */
	if (dfn == sfn)
		return;

	ts[0] = 0;

	/* If have a leading $, then expand environment variable */
	if (*sfn == '$')
	  {	++sfn;
		tp = ts;
		while (*tp++ = *sfn)
			if (!isalnum(*sfn))
				break;
			else
				++sfn;
		*--tp = 0;		/* Just in case */
		strcpy(ts, getenv(ts));	/* MARGINAL!! */
	  }
	/* If have leading ~, then expand login name (null means $HOME) */
	else if (*sfn == '~')
	  {	++sfn;
		if (*sfn == '/' || *sfn == 0)
			strcpy(ts, getenv("HOME"));
		else
		  {	tp = ts;
			while (*sfn && *sfn != '/')
				*tp++ = *sfn++;
			*tp = 0;
			pw = (struct passwd *)getpwnam(ts);
			if (! pw)
				strcpy(ts, "???");
			else
				strcpy(ts, pw->pw_dir);
		  }
	  }

	/* Now, ts is either empty or contains the expansion;
	 * sfn has been updated correctly.
	 */
	strcpy(dfn, ts);
	strcat(dfn, sfn);
}
#endif /*IMAGEN*/
