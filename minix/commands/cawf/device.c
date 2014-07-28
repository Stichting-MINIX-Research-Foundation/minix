/*
 *	device.c -- cawf(1) output device support functions
 */

/*
 *	Copyright (c) 1991 Purdue University Research Foundation,
 *	West Lafayette, Indiana 47907.  All rights reserved.
 *
 *	Written by Victor A. Abell <abe@mace.cc.purdue.edu>,  Purdue
 *	University Computing Center.  Not derived from licensed software;
 *	derived from awf(1) by Henry Spencer of the University of Toronto.
 *
 *	Permission is granted to anyone to use this software for any
 *	purpose on any computer system, and to alter it and redistribute
 *	it freely, subject to the following restrictions:
 *
 *	1. The author is not responsible for any consequences of use of
 *	   this software, even if they arise from flaws in it.
 *
 *	2. The origin of this software must not be misrepresented, either
 *	   by explicit claim or by omission.  Credits must appear in the
 *	   documentation.
 *
 *	3. Altered versions must be plainly marked as such, and must not
 *	   be misrepresented as being the original software.  Credits must
 *	   appear in the documentation.
 *
 *	4. This notice may not be removed or altered.
 */

#include "cawf.h"
#include <ctype.h>

static unsigned char *Convstr(char *s, int *len);
static int Convfont(char *nm, char *s, char **fn, unsigned char **fi);

#ifndef	UNIX
#define	strcasecmp	strcmpi
#endif



/*
 * Convstr(s, len) - convert a string
 */

static unsigned char *
Convstr(s, len)
	char *s;			/* input string */
	int *len;			/* length of result */
{
	int c;				/* character assembly */
	unsigned char *cp;		/* temporary character pointer */
	char *em;			/* error message */
	int i;				/* temporary index */
	int l;				/* length */
	unsigned char *r;		/* result string */
/*
 * Make space for the result.
 */
	if ((r = (unsigned char *)malloc(strlen((char *)s) + 1)) == NULL) {
		(void) fprintf(stderr, "%s: out of string space at %s\n",
			Pname, s);
		return(NULL);
	}
/*
 * Copy the input string to the result, processing '\\' escapes.
 */
	for (cp = r, l = 0; *s;) {
		switch (*s) {

		case '\\':
			s++;
			if (*s >= '0' && *s <= '7') {
		/*
		 * '\xxx' -- octal form
		 */
				for (c = i = 0; i < 3; i++, s++) {
					if (*s < '0' || *s > '7') {
						em = "non-octal char";
bad_string:
						(void) fprintf(stderr,
							"%s: %s : %s\n",
							Pname, em, (char *)r);
						return(NULL);
					}
					c = (c << 3) + *s - '0';
				}
				if (c > 0377) {
					em = "octal char > 0377";
					goto bad_string;
				}
				*cp++ = c;
				l++;
			} else if (*s == 'x') {
		/*
		 * '\xyy' -- hexadecimal form
		 */
				s++;
				for (c = i = 0; i < 2; i++, s++) {
#if	defined(__STDC__)
					if ( ! isalpha(*s) && ! isdigit(*s))
#else
					if ( ! isascii(*s) && ! isalpha(*s)
					&&   ! isdigit(*s))
#endif
					{
non_hex_char:
						em = "non-hex char";
						goto bad_string;
					}
					c = c << 4;
					if (*s >= '0' && *s <= '9')
						c += *s - '0';
					else if ((*s >= 'a' && *s <= 'f')
					     ||  (*s >= 'A' && *s <= 'F'))
						c += *s + 10 -
						     (isupper(*s) ? 'A' : 'a');
					else
						goto non_hex_char;
				}
				*cp++ = (unsigned char)c;
				l++;
			} else if (*s == 'E' || *s == 'e') {
		/*
		 * '\E' or '\e' -- ESCape
		 */
				*cp++ = ESC;
				l++;
				s++;
			} else if (*s == '\0') {
				em = "no char after \\";
				goto bad_string;
			} else {
		/*
		 * escaped character (for some reason)
		 */
				*cp++ = *s++;
				l++;
			}
			break;
	/*
	 * Copy a "normal" character.
	 */
		default:
			*cp++ = *s++;
			l++;
		}
	}
	*cp = '\0';
	*len = l;
	return(r);
}


/*
 * Convfont(nm, s, fn, fi) - convert a font for a device
 */

static int
Convfont(nm, s, fn, fi)
	char *nm;			/* output device name */
	char *s;			/* font definition string */
	char **fn;			/* font name address */
	unsigned char **fi;		/* initialization string address */
{
	char *cp;			/* temporary character pointer */
	int len;			/* length */
/*
 * Get the font name, allocate space for it and allocate space for
 * a font structure.
 */
	if ((cp = strchr(s, '=')) == NULL) {
		(void) fprintf(stderr, "%s: bad %s font line format: %s\n",
			Pname, nm, s);
		return(0);
	}
	if ((*fn = (char *)malloc(cp - s + 1)) == NULL) {
		(void) fprintf(stderr, "%s: no space for %s font name %s\n",
			Pname, nm, s);
		return(0);
	}
	(void) strncpy(*fn, s, cp - s);
	(*fn)[cp - s] = '\0';
/*
 * Assmble the font initialization string.
 */
	if ((*fi = Convstr(cp + 1, &len)) == NULL)
		return(0);
	return(len);
}


/*
 * Defdev() - define the output device
 */

int
Defdev()
{
	unsigned char *fi = NULL;	/* last font initialization string */
	char *fn = NULL;		/* font name */
	int fd = 0;			/* found-device flag */
	FILE *fs;			/* file stream */
	int err = 0;			/* errror count */
	int i;				/* temporary index */
	int len;			/* length */
	char line[MAXLINE];		/* line buffer */
	char *p;			/* output device configuration file */
	char *s;			/* temporary string pointer */
/*
 * Check for the built-in devices, ANSI, NONE or NORMAL (default).
 */
	Fstr.b = Fstr.i = Fstr.it = Fstr.r = NULL;
	Fstr.bl = Fstr.il = Fstr.itl = Fstr.rl = 0;
	if (Device == NULL || strcasecmp(Device, "normal") == 0) {
		Fontctl = 0;
check_font:
		if (Devfont) {
			(void) fprintf(stderr,
				"%s: font %s for device %s illegal\n",
				Pname, Devfont, Device ? Device : "NORMAL");
			return(1);
		}
		return(0);
	}
	Fontctl = 1;
	if (strcasecmp(Device, "ansi") == 0) {
		Fstr.b = Newstr((unsigned char *)"x[1m");
		Fstr.it = Newstr((unsigned char *)"x[4m");
		Fstr.r = Newstr((unsigned char *)"x[0m");
		Fstr.b[0] = Fstr.it[0] = Fstr.r[0] = ESC;
		Fstr.bl = Fstr.itl = Fstr.rl = 4;
		goto check_font;
	}
	if (strcasecmp(Device, "none") == 0)
		goto check_font;
/*
 * If a device configuration file path is supplied, use it.
 */
	if (Devconf)
		p = Devconf;
	else {

	/*
	 * Use the CAWFLIB environment if it is defined.
	 */
		if ((p = getenv("CAWFLIB")) == NULL)	
			p = CAWFLIB;
		len = strlen(p) + 1 + strlen(DEVCONFIG) + 1;
		if ((s = (char *)malloc(len)) == NULL) {
			(void) fprintf(stderr, "%s: no space for %s name\n",
				Pname, DEVCONFIG);
			return(1);
		}
		(void) sprintf(s, "%s/%s", p, DEVCONFIG);
		p = s;
	}
/*
 * Open the configuration file.
 */
#ifdef	UNIX
	if ((fs = fopen(p, "r")) == NULL)
#else
	if ((fs = fopen(p, "rt")) == NULL)
#endif
	{
		(void) fprintf(stderr, "%s: can't open config file: %s\n",
			Pname, p);
		return(1);
	}
	*line = ' ';
/*
 * Look for a device definition line -- a line that begins with a name.
 */
	while ( ! feof(fs)) {
		if (*line == '\t' || *line == '#' || *line == ' ') {
			(void) fgets(line, MAXLINE, fs);
			continue;
		}
		if ((s = strrchr(line, '\n')) != NULL)
			*s = '\0';
		else
			line[MAXLINE-1] = '\0';
	/*
	 * Match device name.
	 */
		if (strcmp(Device, line) != 0) {
			(void) fgets(line, MAXLINE, fs);
			continue;
		}
		fd = 1;
	/*
	 * Read the parameter lines for the device.
	 */
		while (fgets(line, MAXLINE, fs) != NULL) {
			if (*line == ' ') {
				for (i = 1; line[i] == ' '; i++)
					;
			} else if (*line == '\t')
				i = 1;
			else
				break;
#if	defined(__STDC__)
			if ( ! isalpha(line[i])
#else
			if ( ! isascii(line[i]) || ! isalpha(line[i])
#endif
			||   line[i+1] != '=')
				break;
			if ((s = strrchr(line, '\n')) != NULL)
				*s = '\0';
			else
				line[MAXLINE-1] = '\0';
			switch (line[i]) {
		/*
		 * \tb=<bolding_string>
		 */
			case 'b':
				if (Fstr.b != NULL) {
				    (void) fprintf(stderr,
					"%s: dup bold for %s in %s: %s\n",
					Pname, Device, p, line);
					(void) free(Fstr.b);
					Fstr.b = NULL;
				}
				if ((Fstr.b = Convstr(&line[i+2], &Fstr.bl))
				== NULL)
					err++;
				break;
		/*
		 * \ti=<italicization_string>
		 */
			case 'i':
				if (Fstr.it != NULL) {
				    (void) fprintf(stderr,
					"%s: dup italic for %s in %s: %s\n",
					Pname, Device, p, line);
					(void) free(Fstr.it);
					Fstr.it = NULL;
				}
				if ((Fstr.it = Convstr(&line[i+2], &Fstr.itl))
				== NULL)
					err++;
				break;
		/*
		 * \tr=<return_to_Roman_string>
		 */
			case 'r':
				if (Fstr.r != NULL) {
				    (void) fprintf(stderr,
					"%s: dup roman for %s in %s: %s\n",
					Pname, Device, p, line);
					(void) free(Fstr.r);
					Fstr.r = NULL;
				}
				if ((Fstr.r = Convstr(&line[i+2], &Fstr.rl))
				== NULL)
					err++;
				break;
		/*
		 * \tf=<font_name>=<font_initialization_string>
		 */
			case 'f':
				if ( ! Devfont || Fstr.i)
					break;
				if ((i = Convfont(Device, &line[i+2], &fn, &fi))
				< 0)
					err++;
				else if (fn && strcmp(Devfont, fn) == 0) {
					Fstr.i = fi;
					Fstr.il = i;
					fi = NULL;
				}
				if (fn) {
					(void) free(fn);
					fn = NULL;
				}
				if (fi) {
					(void) free((char *)fi);
					fi = NULL;
				}
				break;
		/*
		 * ????
		 */
			default:
				(void) fprintf(stderr,
					"%s: unknown device %s line: %s\n",
					Pname, Device, line);
				err++;
			}
		}
		break;
	}
	(void) fclose(fs);
	if (err)
		return(1);
/*
 * See if the device stanza was located and the font exists.
 */
	if ( ! fd) {
		(void) fprintf(stderr, "%s: can't find device %s in %s\n",
			Pname, Device, p);
		return(1);
	}
	if (Devfont && ! Fstr.i) {
		(void) fprintf(stderr,
			"%s: font %s for device %s not found in %s\n",
			Pname, Devfont, Device, p);
		return(1);
	}
	return(0);
}
