#if ever
static char sccsid[] = "@(#)printf.c	(U of Maryland) FLB 6-Jan-1987";
static char RCSid[] = "@(#)$Header$";
#endif

/*
 * Printf - Duplicate the C library routine of the same name, but from
 *	    the shell command level.
 *
 * Fred Blonder <fred@Mimsy.umd.edu>
 *
 * To Compile:
 %	cc -s -O printf.c -o printf
 *
 * $Log$
 * Revision 1.1  2005/04/21 14:55:31  beng
 * Initial revision
 *
 * Revision 1.1.1.1  2005/04/20 13:33:30  beng
 * Initial import of minix 2.0.4
 *
 * Revision 1.4  87/01/29  20:52:30  fred
 * Re-installed backslash-notation conversion for string & char arguments.
 * 
 * Revision 1.3  87/01/29  20:44:23  fred
 * Converted to portable algorithm.
 * Added Roman format for integers.
 * 	29-Jan-87  FLB
 * 
 * Revision 1.2  87/01/09  19:10:57  fred
 * Fixed bug in argument-count error-checking.
 * Changed backslash escapes within strings to correspond to ANSII C
 * 	draft standard.  (9-Jan-87 FLB)
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define EX_OK		0
#define EX_USAGE	1

int ctrl(char *s);

#define atoi(a)		strtoul((a), NULL, 0)

/****************************************************************************/

int main(int argc, char *argv[])
{
register char *cp, *conv_spec, **argp, **ep;
char *ctor(int x);

if (argc < 2) {
	fprintf(stderr,
		"printf: Usage: printf <format-string> [ arg1 . . . ]\n");
	exit(EX_USAGE);
	}

argp = &argv[2];	/* Point at first arg (if any) beyond format string. */
ep = &argv[argc];	/* Point beyond last arg. */

ctrl(argv[1]);	/* Change backslash notation to control chars in fmt string. */

/* Scan format string for conversion specifications, and do appropriate
   conversion on the corresponding argument. */
for (cp = argv[1]; *cp; cp++) {
register int dynamic_count;

	/* Look for next conversion spec. */
	while (*cp && *cp != '%') {
		putchar(*cp++);
		}

	if (!*cp)	/* End of format string */
		break;
		
	dynamic_count = 0;	/* Begin counting dynamic field width specs. */
	conv_spec = cp++;	/* Remember where this conversion begins. */

	for (;*cp; cp++) {	/* Scan until conversion character. */
		char conv_buf[BUFSIZ];	/* Save conversion string here. */
		register int conv_len;	/* Length of ``conv_buf''. */

		switch (*cp) {	/* Field-width spec.: Keep scanning. */
			case '.': case '0': case '1': case '2': case '3':
			case '4': case '5': case '6': case '7': case '8':
			case '9':
				continue;

			case '*':	/* Dynamic field-width spec */
				dynamic_count++;
				continue;

			case 's':	/* String */
				if (&argp[dynamic_count] >= ep) {
					fprintf(stderr,
					"printf: Not enough args for format.\n"
						);
					exit(EX_USAGE);
					}

				(void) strncpy(conv_buf, conv_spec,
					conv_len = cp - conv_spec + 1);
				conv_buf[conv_len] = '\0';

				switch (dynamic_count) {
					case 0:
						ctrl(*argp);
						printf(conv_buf, *argp++);
						break;

					case 1:
						{
						register int a1;

						a1 = atoi(*argp++);
						ctrl(*argp);
						printf(conv_buf, a1, *argp++);
						}
						break;

					case 2:
						{
						register int a1, a2;

						a1 = atoi(*argp++);
						a2 = atoi(*argp++);
						ctrl(*argp);
						printf(conv_buf, a1, a2, *argp++);
						}
						break;

					}
				goto out;

			case 'c':	/* Char */
				if (&argp[dynamic_count] >= ep) {
					fprintf(stderr,
					"printf: Not enough args for format.\n"
						);
					exit(EX_USAGE);
					}

				(void) strncpy(conv_buf, conv_spec,
					conv_len = cp - conv_spec + 1);
				conv_buf[conv_len] = '\0';

				switch (dynamic_count) {
					case 0:
						ctrl(*argp);
						printf(conv_buf, **argp++);
						break;

					case 1:
						{
						register int a1;

						a1 = atoi(*argp++);
						ctrl(*argp);
						printf(conv_buf, a1, **argp++);
						}
						break;

					case 2:
						{
						register int a1, a2;

						a1 = atoi(*argp++);
						a2 = atoi(*argp++);
						ctrl(*argp);
						printf(conv_buf, a1, a2, **argp++);
						}
						break;
					}
				goto out;

			case 'd':	/* Integer */
			case 'o':
			case 'x':
			case 'X':
			case 'u':
				if (&argp[dynamic_count] >= ep) {
					fprintf(stderr,
					"printf: Not enough args for format.\n"
						);
					exit(EX_USAGE);
					}

				(void) strncpy(conv_buf, conv_spec,
					conv_len = cp - conv_spec + 1);
				conv_buf[conv_len] = '\0';

				switch (dynamic_count) {
					case 0:
						printf(conv_buf, atoi(*argp++));
						break;

					case 1:
						{
						register int a1;

						a1 = atoi(*argp++);
						printf(conv_buf, a1, atoi(*argp++));
						}
						break;

					case 2:
						{
						register int a1, a2;

						a1 = atoi(*argp++);
						a2 = atoi(*argp++);
						printf(conv_buf, a1, a2, atoi(*argp++));
						}
						break;

					}
				goto out;

			case 'f':	/* Real */
			case 'e':
			case 'g':
				if (&argp[dynamic_count] >= ep) {
					fprintf(stderr,
					"printf: Not enough args for format.\n"
						);
					exit(EX_USAGE);
					}

				(void) strncpy(conv_buf, conv_spec,
					conv_len = cp - conv_spec + 1);
				conv_buf[conv_len] = '\0';

				switch (dynamic_count) {
					case 0:
						printf(conv_buf, atof(*argp++));
						break;

					case 1:
						{
						register int a1;

						a1 = atoi(*argp++);
						printf(conv_buf, a1, atof(*argp++));
						}
						break;

					case 2:
						{
						register int a1, a2;

						a1 = atoi(*argp++);
						a2 = atoi(*argp++);
						printf(conv_buf, a1, a2, atof(*argp++));
						}
						break;

					}
				goto out;

			case 'r':	/* Roman (Well, why not?) */
				if (&argp[dynamic_count] >= ep) {
					fprintf(stderr,
					"printf: Not enough args for format.\n"
						);
					exit(EX_USAGE);
					}

				(void) strncpy(conv_buf, conv_spec,
					conv_len = cp - conv_spec + 1);
				conv_buf[conv_len] = '\0';
				conv_buf[conv_len - 1] = 's';

				switch (dynamic_count) {
					case 0:
						printf(conv_buf,
							ctor(atoi(*argp++)));
						break;

					case 1:
						{
						register int a1;

						a1 = atoi(*argp++);
						printf(conv_buf, a1,
							ctor(atoi(*argp++)));
						}
						break;

					case 2:
						{
						register int a1, a2;

						a1 = atoi(*argp++);
						a2 = atoi(*argp++);
						printf(conv_buf, a1, a2,
							ctor(atoi(*argp++)));
						}
						break;

					}
				goto out;

			case '%':	/* Boring */
				putchar('%');
				break;

			default:	/* Probably an error, but let user
					   have his way. */
				continue;
			}
		}
	out: ;
	}

exit(EX_OK);
}

/****************************************************************************/

/* Convert backslash notation to control characters, in place. */

int ctrl(char *s)
{
register char *op;
static int val;

for (op = s; *s; s++)
	if (*s == '\\')
		switch (*++s) {
			case '\0':	/* End-of-string: user goofed */
				goto out;

			case '\\':	/* Backslash */
				*op++ = '\\';
				break;

			case 'n':	/* newline */
				*op++ = '\n';
				break;

			case 't':	/* horizontal tab */
				*op++ = '\t';
				break;

			case 'r':	/* carriage-return */
				*op++ = '\r';
				break;

			case 'f':	/* form-feed */
				*op++ = '\f';
				break;

			case 'b':	/* backspace */
				*op++ = '\b';
				break;

			case 'v':	/* vertical tab */
				*op++ = '\13';
				break;

			case 'a':	/* WARNING! DANGER! DANGER! DANGER! */
				*op++ = '\7';
				break;

			case '0': case '1': case '2': case '3':
			case '4': case '5': case '6': case '7':
				{	/* octal constant */
				register int digits;

				val = 0;
				(void) sscanf(s, "%3o", &val);
				*op++ = val;
				for (digits = 3; s[1] &&
					strchr("01234567", s[1])
					&& --digits > 0;
						s++);
				}
				break;

			case 'x':	/* hex constant */
			case 'X':
				s++;
				{
				register int digits;

				val = 0;
				(void) sscanf(s, "%3x", &val);
				*op++ = val;
				for (digits = 3; *s && s[1] &&
					strchr("0123456789abcdefABCDEF",
									s[1])
					&& --digits > 0;
						s++);
				}
				break;

			}
	else
		*op++ = *s;

out:

*op = '\0';
}

/****************************************************************************/

/* Convert integer to Roman Numerals. (Have have you survived without it?) */

struct roman {
	unsigned r_mag;
	char r_units, r_fives;
	} roman[] = {
		{ 1000, 'M', '\0', },
		{  100, 'C', 'D',  },
		{   10, 'X', 'L',  },
		{    1, 'I', 'V',  },
		};

char *ctor(int x)
{
register struct roman *mp;
static char buf[BUFSIZ];
register char *cp = buf;

/* I've never actually seen a roman numeral with a minus-sign.
   Probably ought to print out some appropriate latin phrase instead. */
if (x < 0) {
	*cp++ = '-';
	x = -x;
	}

for (mp = roman; x; mp++) {
	register unsigned units;

	units = x / mp->r_mag;
	x = x % mp->r_mag;

	if (cp > &buf[BUFSIZ-2])
		return "???";

	if (units == 9 && mp > roman) {	/* Do inverse notation: Eg: ``IX''. */
		*cp++ = mp->r_units;
		*cp++ = mp[-1].r_units;
		}
	else if (units == 4 && mp->r_fives) {
		/* Inverse notation for half-decades: Eg: ``IV'' */
		*cp++ = mp->r_units;
		*cp++ = mp->r_fives;
		}
	else {	/* Additive notation */
		if (units >= 5 && mp->r_fives) {
			*cp++ = mp->r_fives;
			units -= 5;
			}
		while (units--) {
			*cp++ = mp->r_units;
			if (cp > &buf[BUFSIZ-5])
				return "???";
			}
		}
	}

*cp = '\0';

return buf;
}

/****************************************************************************/
