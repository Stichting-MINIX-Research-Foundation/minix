/*
 *	termcap.c	V1.1	20/7/87		agc	Joypace Ltd
 *
 *	Copyright Joypace Ltd, London, UK, 1987. All rights reserved.
 *	This file may be freely distributed provided that this notice
 *	remains attached.
 *
 *	A public domain implementation of the termcap(3) routines.
 *
 *
 *
 *	 Klamer Schutte	      V1.2    Nov. 1988
 *
 *   - Can match multiple terminal names		 [tgetent]
 *   - Removal of **area assignments			 [tgetstr]
 *
 *	 Terrence W. Holm     V1.3    May, Sep, Oct.  1988
 *
 *   - Correct when TERM != name and TERMCAP is defined	 [tgetent]
 *   - Correct the comparison for the terminal name 	 [tgetent]
 *   - Correct the value of ^x escapes              	 [tgetstr]
 *   - Added %r to reverse row/column			 [tgoto]
 *   - Fixed end of definition test			 [tgetnum/flag/str]
 *
 *	 Terrence W. Holm     V1.4    Jan. 1989
 *
 *   - Incorporated Klamer's V1.2 fixes into V1.3
 *   - Added %d, (old %d is now %2)			 [tgoto]
 *   - Allow '#' comments in definition file		 [tgetent]
 */

#include <lib.h>
#include <termcap.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

char *capab = (char *)NULL;		/* the capability itself */

#if 0
/*  The following are not yet used.  */
extern short ospeed;		/* output speed */
extern char PC;			/* padding character */
extern char *BC;		/* back cursor movement */
extern char *UP;		/* up cursor movement */
#endif

/*
 *	tgetent - get the termcap entry for terminal name, and put it
 *	in bp (which must be an array of 1024 chars). Returns 1 if
 *	termcap entry found, 0 if not found, and -1 if file not found.
 */

int tgetent(bp, name)
char *bp;
char *name;
{
  FILE *fp;
  char *file;
  char *term;
  short len = strlen(name);

  capab = bp;

  /* If TERMCAP begins with a '/' then use TERMCAP as the path	 */
  /* Name of the termcap definitions file. If TERMCAP is a	 */
  /* Definition and TERM equals "name" then use TERMCAP as the	 */
  /* Definition. Otherwise use "/etc/termcap" as the path name.	 */

  if ((file = getenv("TERMCAP")) == (char *)NULL)
	file = "/etc/termcap";
  else if (*file != '/')
	if ((term = getenv("TERM")) != (char *)NULL && strcmp(term, name) == 0) {
		*bp = '\0';
		strncat(bp, file, 1023);
		return(1);
	} else
		file = "/etc/termcap";

  if ((fp = fopen(file, "r")) == (FILE *) NULL) {
	capab = (char *)NULL;		/* no valid termcap  */
	return(-1);
  }
  for (;;) {
	/* Read in each definition */
	int def_len = 0;
	char *cp = bp;

	do {
		if (fgets(&bp[def_len], (unsigned int)(1024 - def_len), fp) == (char *)NULL) {
			fclose(fp);
			capab = (char *)NULL;	/* no valid termcap */
			return(0);
		}
		def_len = strlen(bp) - 2;
	} while (bp[def_len] == '\\');

	while (isspace(*cp)) cp++;

	/* Comment lines start with a '#'  */
	if (*cp == '#') continue;

	/* See if any of the terminal names in this definition */
	/* Match "name".						 */

	do {
		if (strncmp(name, cp, len) == 0 &&
		    (cp[len] == '|' || cp[len] == ':')) {
			fclose(fp);
			return(1);
		}
		while ((*cp) && (*cp != '|') && (*cp != ':')) cp++;
	} while (*cp++ == '|');
  }
}


/*
 *	tgetnum - get the numeric terminal capability corresponding
 *	to id. Returns the value, -1 if invalid.
 */

int tgetnum(id)
char *id;
{
  register char *cp = capab;

  if (cp == (char *)NULL || id == (char *)NULL) return(-1);

  for (;;) {
	while (*cp++ != ':')
		if (cp[-1] == '\0') return(-1);

	while (isspace(*cp)) cp++;

	if (strncmp(cp, id, 2) == 0 && cp[2] == '#') return(atoi(cp + 3));
  }
}


/*
 *	tgetflag - get the boolean flag corresponding to id. Returns -1
 *	if invalid, 0 if the flag is not in termcap entry, or 1 if it is
 *	present.
 */

int tgetflag(id)
char *id;
{
  register char *cp = capab;

  if (cp == (char *)NULL || id == (char *)NULL) return(-1);

  for (;;) {
	while (*cp++ != ':')
		if (cp[-1] == '\0') return(0);

	while (isspace(*cp)) cp++;

	if (strncmp(cp, id, 2) == 0) return(1);
  }
}


/*
 *	tgetstr - get the string capability corresponding to id and place
 *	it in area (advancing area at same time). Expand escape sequences
 *	etc. Returns the string, or NULL if it can't do it.
 */

char *tgetstr(id, area)
char *id;
char **area;
{
  register char *cp = capab;
  register char *wsp = *area;	/* workspace pointer  */

  if (cp == (char *)NULL || id == (char *)NULL) return((char *)NULL);

  for (;;) {
	while (*cp++ != ':')
		if (cp[-1] == '\0') return((char *)NULL);

	while (isspace(*cp)) cp++;

	if (strncmp(cp, id, 2) == 0 && cp[2] == '=') {
		for (cp += 3; *cp && *cp != ':'; wsp++, cp++) switch (*cp) {
			    case '^':
				*wsp = *++cp - '@';
				break;

			    case '\\':
				switch (*++cp) {
				    case 'E':
					*wsp = '\033';
					break;
				    case 'n':
					*wsp = '\n';
					break;
				    case 'r':
					*wsp = '\r';
					break;
				    case 't':
					*wsp = '\t';
					break;
				    case 'b':
					*wsp = '\b';
					break;
				    case 'f':
					*wsp = '\f';
					break;
				    case '0':
				    case '1':
				    case '2':
				    case '3':
					{
						int i;
						int t = 0;
						for (i = 0; i < 3 &&
						     isdigit(*cp); ++i, ++cp)
							t = t * 8 + *cp - '0';
						*wsp = t;
						cp--;
						break;
					}
				    default:
					*wsp = *cp;
				}
				break;

			    default:	*wsp = *cp;
  			}

		*wsp++ = '\0';

		{
			char *ret = *area;
			*area = wsp;
			return(ret);
		}
	}
  }				/* end for(;;) */
}



/*
 *	tgoto - given the cursor motion string cm, make up the string
 *	for the cursor to go to (destcol, destline), and return the string.
 *	Returns "OOPS" if something's gone wrong, or the string otherwise.
 */

char *tgoto(cm, destcol, destline)
char *cm;
int destcol;
int destline;
{
  PRIVATE char ret[24];
  char *rp = ret;
  int incr = 0;
  int argno = 0;
  int numval;

  for (; *cm; cm++) {
	if (*cm == '%') {
		switch (*++cm) {
		    case 'i':	incr = 1;	  			break;

		    case 'r':	argno = 1;	  			break;

		    case '+':
			numval = (argno == 0 ? destline : destcol);
			*rp++ = numval + incr + *++cm;
			argno = 1 - argno;
			break;

		    case '2':
			numval = (argno == 0 ? destline : destcol);
			numval = (numval + incr) % 100;
			*rp++ = '0' + (numval / 10);
			*rp++ = '0' + (numval % 10);
			argno = 1 - argno;
			break;

		    case 'd':
			numval = (argno == 0 ? destline : destcol);
			numval = (numval + incr) % 1000;
			if (numval > 99) *rp++ = '0' + (numval / 100);
			if (numval > 9) *rp++ = '0' + (numval / 10) % 10;
			*rp++ = '0' + (numval % 10);
			argno = 1 - argno;
			break;

		    case '%':	*rp++ = '%';	  			break;

		    default:	return("OOPS");
  		}

	} else
		*rp++ = *cm;
  }

  *rp = '\0';
  return(ret);
}



/*
 *	tputs - put the string cp out onto the terminal, using the function
 *	outc. This should do padding for the terminal, but I can't find a
 *	terminal that needs padding at the moment...
 */

int tputs(cp, affcnt, outc)
register char *cp;
int affcnt;
_PROTOTYPE( void (*outc), (int ch));
{
  if (cp == (char *)NULL) return(1);
  /* Do any padding interpretation - left null for MINIX just now */
  while (*cp) (*outc) (*cp++);
  return(1);
}
