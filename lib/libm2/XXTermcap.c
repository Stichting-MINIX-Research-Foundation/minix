/*
 *	termcap.c	1.1	20/7/87		agc	Joypace Ltd
 *
 *	Copyright Joypace Ltd, London, UK, 1987. All rights reserved.
 *	This file may be freely distributed provided that this notice
 *	remains attached.
 *
 *	A public domain implementation of the termcap(3) routines.
 *
 *	Made fully functional by Ceriel J.H. Jacobs.
 *
 * BUGS:
 *	- does not check termcap entry sizes
 *	- not fully tested
 */

#define CAPABLEN	2

#define ISSPACE(c)	((c) == ' ' || (c) == '\t' || (c) == '\r' || (c) == '\n')
#define ISDIGIT(x)	((x) >= '0' && (x) <= '9')

short	ospeed = 0;		/* output speed */
char	PC = 0;			/* padding character */
char	*BC = 0;		/* back cursor movement */
char	*UP = 0;		/* up cursor movement */

static char	*capab = 0;		/* the capability itself */
static int	check_for_tc();
static int	match_name();

#define NULL	0

/* Some things from C-library, needed here because the C-library is not
   loaded with Modula-2 programs
*/

static char *
strcat(s1, s2)
register char *s1, *s2;
{
  /* Append s2 to the end of s1. */

  char *original = s1;

  /* Find the end of s1. */
  while (*s1 != 0) s1++;

  /* Now copy s2 to the end of s1. */
  while (*s1++ = *s2++) /* nothing */ ;
  return(original);
}

static char *
strcpy(s1, s2)
register char *s1, *s2;
{
/* Copy s2 to s1. */
  char *original = s1;

  while (*s1++ = *s2++) /* nothing */;
  return(original);
}

static int
strlen(s)
char *s;
{
/* Return length of s. */

  char *original = s;

  while (*s != 0) s++;
  return(s - original);
}

static int
strcmp(s1, s2)
register char *s1, *s2;
{
/* Compare 2 strings. */

  for(;;) {
	if (*s1 != *s2) {
		if (!*s1) return -1;
		if (!*s2) return 1;
		return(*s1 - *s2);
	}
	if (*s1++ == 0) return(0);
	s2++;
  }
}

static int
strncmp(s1, s2, n)
	register char *s1, *s2;
	int n;
{
/* Compare two strings, but at most n characters. */

  while (n-- > 0) {
	if (*s1 != *s2) {
		if (!*s1) return -1;
		if (!*s2) return 1;
		return(*s1 - *s2);
	}
	if (*s1++ == 0) break;
	s2++;
  }
  return 0;
}

static char *
getenv(name)
register char *name;
{
  extern char ***_penviron;
  register char **v = *_penviron, *p, *q;

  if (v == 0 || name == 0) return 0;
  while ((p = *v++) != 0) {
	q = name;
	while (*q && *q++ == *p++) /* nothing */ ;
	if (*q || *p != '=') continue;
	return(p+1);
  }
  return(0);
}

static char *
fgets(buf, count, fd)
	char *buf;
{
	static char bf[1024];
	static int cnt = 0;
	static char *pbf = &bf[0];
	register char *c = buf;


	while (--count) {
		if (pbf >= &bf[cnt]) {
			if ((cnt = read(fd, bf, 1024)) <= 0) {
				if (c == buf) return (char *) NULL;
				*c = 0;
				return buf;
			}
			pbf = &bf[0];
		}
		*c = *pbf++;
		if (*c++ == '\n') {
			*c = 0;
			return buf;
		}
	}
	*c = 0;
	return buf;
}

/*
 *	tgetent - get the termcap entry for terminal name, and put it
 *	in bp (which must be an array of 1024 chars). Returns 1 if
 *	termcap entry found, 0 if not found, and -1 if file not found.
 */
int
tgetent(bp, name)
char	*bp;
char	*name;
{
	int	fp;
	char	*file;
	char	*cp;
	short	len = strlen(name);
	char	buf[1024];

	capab = bp;
	if ((file = getenv("TERMCAP")) != (char *) NULL) {
		if (*file != '/' &&
		    (cp = getenv("TERM")) != NULL && strcmp(name, cp) == 0) {
			(void) strcpy(bp, file);
			return(1);
		}
		else file = "/etc/termcap";
	} else
		file = "/etc/termcap";
	if ((fp = open(file, 0)) < 0) {
		capab = 0;
		return(-1); 
	}
	while (fgets(buf, 1024, fp) != NULL) {
		if (buf[0] == '#') continue;
		while (*(cp = &buf[strlen(buf) - 2]) == '\\')
			if (fgets(cp, 1024, fp) == NULL)
				return (0);
		if (match_name(buf, name)) {
			strcpy(bp, buf);
			close(fp);
			if(check_for_tc() == 0) {
				capab = 0;
				return 0;
			}
			return 1;
		}
	}
	capab = 0;
	close(fp);
	return(0);
}

/*
 *	Compare the terminal name with each termcap entry name; Return 1 if a
 *	match is found.
 */
static int
match_name(buf, name)
	char	*buf;
	char	*name;
{
	register char	*tp = buf;
	register char	*np;

	for (;;) {
		for (np = name; *np && *tp == *np; np++, tp++) { }
		if (*np == 0 && (*tp == '|' || *tp == ':' || *tp == 0))
			return(1);
		while (*tp != 0 && *tp != '|' && *tp != ':') tp++;
		if (*tp++ != '|') return (0);
	}
}

/*
 *	Handle tc= definitions recursively.
 */
static int
check_for_tc()
{
	static int	count = 0;
	char		*savcapab = capab;
	char		buf[1024];
	char		terminalname[128];
	register char	*p = capab + strlen(capab) - 2, *q;

	while (*p != ':')
		if (--p < capab)
			return(0);	/* no : in termcap entry */
	if (p[1] != 't' || p[2] != 'c')
		return(1);
	if (count > 16) {
		return(0);	/* recursion in tc= definitions */
	}
	count++;
	strcpy(terminalname, &p[4]);
	q = terminalname;
	while (*q && *q != ':') q++;
	*q = 0;
	if (tgetent(buf, terminalname) != 1) {
		--count;
		return(0);
	}
	--count;
	for (q = buf; *q && *q != ':'; q++) { }
	strcpy(p, q);
	capab = savcapab;
	return(1);
}

/*
 *	tgetnum - get the numeric terminal capability corresponding
 *	to id. Returns the value, -1 if invalid.
 */
int
tgetnum(id)
char	*id;
{
	char	*cp;
	int	ret;

	if ((cp = capab) == NULL || id == NULL || *cp == 0)
		return(-1);
	while (*++cp && *cp != ':')
		;
	while (*cp) {
		cp++;
		while (ISSPACE(*cp))
			cp++;
		if (strncmp(cp, id, CAPABLEN) == 0) {
			while (*cp && *cp != ':' && *cp != '#')
				cp++;
			if (*cp != '#')
				return(-1);
			for (ret = 0, cp++ ; *cp && ISDIGIT(*cp) ; cp++)
				ret = ret * 10 + *cp - '0';
			return(ret);
		}
		while (*cp && *cp != ':')
			cp++;
	}
	return(-1);
}

/*
 *	tgetflag - get the boolean flag corresponding to id. Returns -1
 *	if invalid, 0 if the flag is not in termcap entry, or 1 if it is
 *	present.
 */
int
tgetflag(id)
char	*id;
{
	char	*cp;

	if ((cp = capab) == NULL || id == NULL || *cp == 0)
		return(-1);
	while (*++cp && *cp != ':')
		;
	while (*cp) {
		cp++;
		while (ISSPACE(*cp))
			cp++;
		if (strncmp(cp, id, CAPABLEN) == 0)
			return(1);
		while (*cp && *cp != ':')
			cp++;
	}
	return(0);
}

/*
 *	tgetstr - get the string capability corresponding to id and place
 *	it in area (advancing area at same time). Expand escape sequences
 *	etc. Returns the string, or NULL if it can't do it.
 */
char *
tgetstr(id, area)
char	*id;
char	**area;
{
	char	*cp;
	char	*ret;
	int	i;

	if ((cp = capab) == NULL || id == NULL || *cp == 0)
		return(NULL);
	while (*++cp != ':')
		;
	while (*cp) {
		cp++;
		while (ISSPACE(*cp))
			cp++;
		if (strncmp(cp, id, CAPABLEN) == 0) {
			while (*cp && *cp != ':' && *cp != '=')
				cp++;
			if (*cp != '=')
				return(NULL);
			for (ret = *area, cp++; *cp && *cp != ':' ; (*area)++, cp++)
				switch(*cp) {
				case '^' :
					**area = *++cp - 'A' + 1;
					break;
				case '\\' :
					switch(*++cp) {
					case 'E' :
						**area = '\033';
						break;
					case 'n' :
						**area = '\n';
						break;
					case 'r' :
						**area = '\r';
						break;
					case 't' :
						**area = '\t';
						break;
					case 'b' :
						**area = '\b';
						break;
					case 'f' :
						**area = '\f';
						break;
					case '0' :
					case '1' :
					case '2' :
					case '3' :
						for (i=0 ; *cp && ISDIGIT(*cp) ; cp++)
							i = i * 8 + *cp - '0';
						**area = i;
						cp--;
						break;
					case '^' :
					case '\\' :
						**area = *cp;
						break;
					}
					break;
				default :
					**area = *cp;
				}
			*(*area)++ = '\0';
			return(ret);
		}
		while (*cp && *cp != ':')
			cp++;
	}
	return(NULL);
}

/*
 *	tgoto - given the cursor motion string cm, make up the string
 *	for the cursor to go to (destcol, destline), and return the string.
 *	Returns "OOPS" if something's gone wrong, or the string otherwise.
 */
char *
tgoto(cm, destcol, destline)
char	*cm;
int	destcol;
int	destline;
{
	register char	*rp;
	static char	ret[32];
	char		added[16];
	int		*dp = &destline;
	int 		numval;
	int		swapped = 0;

	added[0] = 0;
	for (rp = ret ; *cm ; cm++) {
		if (*cm == '%') {
			switch(*++cm) {
			case '>' :
				if (dp == NULL)
					return("OOPS");
				cm++;
				if (*dp > *cm++) {
					*dp += *cm;
				}
				break;
			case '+' :
			case '.' :
				if (dp == NULL)
					return("OOPS");
				if (*cm == '+') *dp = *dp + *++cm;
				for (;;) {
				    switch(*dp) {
				    case 0:
				    case 04:
				    case '\t':
				    case '\n':
					/* filter these out */
					if (dp == &destcol || swapped || UP) {
						strcat(added, dp == &destcol || swapped ?
							(BC ? BC : "\b") :
							UP);
						(*dp)++;
						continue;
					}
				    }
				    break;
				}
				*rp++ = *dp;
				dp = (dp == &destline) ? &destcol : NULL;
				break;

			case 'r' : {
				int tmp = destline;

				destline = destcol;
				destcol = tmp;
				swapped = 1 - swapped;
				break;
			}
			case 'n' :
				destcol ^= 0140;
				destline ^= 0140;
				break;

			case '%' :
				*rp++ = '%';
				break;

			case 'i' :
				destcol++;
				destline++;
				break;

			case 'B' :
				if (dp == NULL)
					return("OOPS");
				*dp = 16 * (*dp / 10) + *dp % 10;
				break;

			case 'D' :
				if (dp == NULL)
					return("OOPS");
				*dp = *dp - 2 * (*dp % 16);
				break;

			case 'd' :
			case '2' :
			case '3' :
				if (dp == NULL)
					return("OOPS");
				numval = *dp;
				dp = (dp == &destline) ? &destcol : NULL;
				if (numval >= 100) {
					*rp++ = '0' + numval / 100;
				}
				else if (*cm == '3') {
					*rp++ = ' ';
				}
				if (numval >= 10) {
					*rp++ = '0' + ((numval%100)/10);
				}
				else if (*cm == '3' || *cm == '2') {
					*rp++ = ' ';
				}
				*rp++ = '0' + (numval%10);
				break;
			default :
				return("OOPS");
			}
		}
		else *rp++ = *cm;
	}
	*rp = '\0';
	strcpy(rp, added);
	return(ret);
}

static int tens_of_ms_p_char[] = {	/* index as returned by gtty */
					/* assume 10 bits per char */
	0, 2000, 1333, 909, 743, 666, 500, 333, 166, 83, 55, 41, 20, 10, 5, 2
};
/*
 *	tputs - put the string cp out onto the terminal, using the function
 *	outc. Also handle padding.
 */
int
tputs(cp, affcnt, outc)
register char	*cp;
int		affcnt;
int		(*outc)();
{
	int delay = 0;
	if (cp == NULL)
		return(1);
	while (ISDIGIT(*cp)) {
		delay = delay * 10 + (*cp++ - '0');
	}
	delay *= 10;
	if (*cp == '.') {
		cp++;
		if (ISDIGIT(*cp)) {
			delay += *cp++ - '0';
		}
		while (ISDIGIT(*cp)) cp++;
	}
	if (*cp == '*') {
		delay *= affcnt;
		cp++;
	}
	while (*cp)
		(*outc)(*cp++);
	if (delay != 0 &&
	    ospeed > 0 &&
	    ospeed < (sizeof tens_of_ms_p_char / sizeof tens_of_ms_p_char[0])) {
		delay = (delay + tens_of_ms_p_char[ospeed] - 1) / 
				  tens_of_ms_p_char[ospeed];
		while (delay--) (*outc)(PC);
	}
	return(1);
}

/*
 *	That's all, folks...
 */
