/*****************************************************************
**
**	tcap.c	-- termcap color capabilities
**
**	(c) Jan 1991 - Feb 2010 by hoz
**
**	Feb 2002	max line size increased to 512 byte
**			default terminal "html" added
**	Feb 2010	color capabilities added
**
*****************************************************************/

#include <stdio.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

# include "config_zkt.h"

#if defined(COLOR_MODE) && COLOR_MODE && HAVE_LIBNCURSES
# ifdef HAVE_TERM_H
#  include <term.h>
# endif
# ifdef HAVE_CURSES_H
#  include <curses.h>
# endif
#endif

#define extern
# include "tcap.h"
#undef extern

/*****************************************************************
**	global vars
*****************************************************************/
/* termcap strings */
static	const	char	*is1 = "";
static	const	char	*is2 = "";
static	const	char	*r1 = "";
static	const	char	*r2 = "";
static	const	char	*bold_on = "";
static	const	char	*bold_off = "";
static	const	char	*italic_on = "";
static	const	char	*italic_off = "";
static	char	colortab[8][31+1];

/* termcap numbers */
static	int	maxcolor;

/* function declaration */
static	int	tc_printattr (FILE *fp, const char *attstr);
static	int	tc_color (FILE *fp, int color);

static	int	html = 0;



/*****************************************************************
**	global functions
*****************************************************************/
#if defined(COLOR_MODE) && COLOR_MODE && HAVE_LIBNCURSES
int	 tc_init (FILE *fp, const char *term)
{
	static	char	area[1024];
	char		buf[1024];
	char		*ap = area;
	char		*af = "";		/* AF */	/* ansi foreground */
	int		i;

	/* clear all color strings */
	for ( i = 0; i < 8; i++ )
		colortab[i][0] = '\0';

	if ( term == NULL || *term == '\0' ||
	     strcmp (term, "none") == 0 || strcmp (term, "dumb") == 0 )
		return 0;

	if ( strcmp (term, "html") == 0 || strcmp (term, "HTML") == 0 )
	{
		bold_on = "<B>";
		bold_off = "</B>";
		italic_on = "<I>";
		italic_off = "</I>";
		af = "";
		maxcolor = 8;
		snprintf (colortab[TC_BLACK], sizeof colortab[0], "<font color=black>");
		snprintf (colortab[TC_BLUE], sizeof colortab[0], "<font color=blue>");
		snprintf (colortab[TC_GREEN], sizeof colortab[0], "<font color=green>");
		snprintf (colortab[TC_CYAN], sizeof colortab[0], "<font color=cyan>");
		snprintf (colortab[TC_RED], sizeof colortab[0], "<font color=red>");
		snprintf (colortab[TC_MAGENTA], sizeof colortab[0], "<font color=magenta>");
		snprintf (colortab[TC_YELLOW], sizeof colortab[0], "<font color=yellow>");
		snprintf (colortab[TC_WHITE], sizeof colortab[0], "<font color=white>");
		html = 1;
		return 0;
	}
#if 0
	if ( !istty (fp) ) 
		return 0;
#endif
	switch ( tgetent (buf, term) )
	{
	case -1:	perror ("termcap file");
			return -1;
	case 0:		fprintf (stderr, "unknown terminal %s\n", term);
			return -1;
	}

	if ( !(is1 = tgetstr ("is1", &ap)) )
		is1 = "";
	if ( !(is2 = tgetstr ("is2", &ap)) )
		is2 = "";
	if ( !(r1 = tgetstr ("r1", &ap)) )
		r1 = "";
	if ( !(r2 = tgetstr ("r2", &ap)) )
		r2 = "";

		/* if bold is not present */
	if ( !(bold_on = tgetstr ("md", &ap)) )
			/* use standout mode */
		if ( !(bold_on = tgetstr ("so", &ap)) )
			bold_on = bold_off = "";
		else
			bold_off = tgetstr ("se", &ap);
	else
		bold_off = tgetstr ("me", &ap);

		/* if italic not present */
	if ( !(italic_on = tgetstr ("ZH", &ap)) )
			/* use underline mode */
		if ( !(italic_on = tgetstr ("us", &ap)) )
			italic_on = italic_off = "";
		else
			italic_off = tgetstr ("ue", &ap);
	else
		italic_off = tgetstr ("ZR", &ap);

	maxcolor = tgetnum ("Co");
	if ( maxcolor < 0 )	/* no colors ? */
		return 0;
	if ( maxcolor > 8 )
		maxcolor = 8;

	if ( (af = tgetstr ("AF", &ap)) )	/* set ansi color foreground */
	{
		for ( i = 0; i < maxcolor; i++ )
			snprintf (colortab[i], sizeof colortab[0], "%s", tparm (af, i));
	}
	else if ( (af = tgetstr ("Sf", &ap)) )	/* or set color foreground */
	{
		snprintf (colortab[TC_BLACK], sizeof colortab[0], "%s", tparm (af, 0));
		snprintf (colortab[TC_BLUE], sizeof colortab[0], "%s", tparm (af, 1));
		snprintf (colortab[TC_GREEN], sizeof colortab[0], "%s", tparm (af, 2));
		snprintf (colortab[TC_CYAN], sizeof colortab[0], "%s", tparm (af, 3));
		snprintf (colortab[TC_RED], sizeof colortab[0], "%s", tparm (af, 4));
		snprintf (colortab[TC_MAGENTA], sizeof colortab[0], "%s", tparm (af, 5));
		snprintf (colortab[TC_YELLOW], sizeof colortab[0], "%s", tparm (af, 6));
		snprintf (colortab[TC_WHITE], sizeof colortab[0], "%s", tparm (af, 7));
	}

#if 0
	if ( is1 && *is1 )
		tc_printattr (fp, is1);
	if ( is2 && *is2 )
		tc_printattr (fp, is2);
#endif

	return 0;
}
#else
int	 tc_init (FILE *fp, const char *term)
{
	int	i;

	is1 = "";
	is2 = "";
	r1 = "";
	r2 = "";
	bold_on = "";
	bold_off = "";
	italic_on = "";
	italic_off = "";
	for ( i = 0; i < 8; i++ )
		colortab[i][0] = '\0';
	maxcolor = 0;
	html = 0;

	return 0;
}
#endif

#if defined(COLOR_MODE) && COLOR_MODE && HAVE_LIBNCURSES
int	tc_end (FILE *fp, const char *term)
{
#if 0
	if ( term )
	{
//		if ( r1 && *r1 ) tc_printattr (fp, r1);
		if ( r2 && *r2 )
			tc_printattr (fp, r2);
	}
#endif
	return 0;
}
#else
int	tc_end (FILE *fp, const char *term)
{
	return 0;
}
#endif

#if defined(COLOR_MODE) && COLOR_MODE && HAVE_LIBNCURSES
int	tc_attr (FILE *fp, tc_att_t attr, int on)
{
	int	len;

	len = 0;
	if ( on )	/* turn attributes on ? */
	{
		if ( (attr & TC_BOLD) == TC_BOLD )
			len += tc_printattr (fp, bold_on);
		if ( (attr & TC_ITALIC) == TC_ITALIC )
			len += tc_printattr (fp, italic_on);

		if ( attr & 0xFF )
			len += tc_color (fp, attr & 0xFF);
	}
	else	/* turn attributes off */
	{
		if ( html )
			len += fprintf (fp, "</font>");
		else
			len += tc_color (fp, TC_BLACK);

		if ( (attr & TC_ITALIC) == TC_ITALIC )
			len += tc_printattr (fp, italic_off);
		if ( !html || (attr & TC_BOLD) == TC_BOLD )
			len += tc_printattr (fp, bold_off);
	}

	return len;
}
#else
int	tc_attr (FILE *fp, tc_att_t attr, int on)
{
	return 0;
}
#endif

/*****************************************************************
**	internal functions
*****************************************************************/
static	FILE	*tc_outfp;
static	int	put (int c)
{
	return putc (c, tc_outfp);
}

#if defined(COLOR_MODE) && COLOR_MODE && HAVE_LIBNCURSES
static	int	tc_printattr (FILE *fp, const char *attstr)
{
	tc_outfp = fp;
	return tputs (attstr, 0, put);
}
#else
static	int	tc_printattr (FILE *fp, const char *attstr)
{
	return 0;
}
#endif

#if defined(COLOR_MODE) && COLOR_MODE && HAVE_LIBNCURSES
static	int	tc_color (FILE *fp, int color)
{
	tc_outfp = fp;

	if ( color < 0 || color >= maxcolor )
		return 0;
	return tputs (colortab[color], 0, put);
}
#else
static	int	tc_color (FILE *fp, int color)
{
	return 0;
}
#endif


#ifdef TEST
static	const char	*progname;
/*****************************************************************
**	test main()
*****************************************************************/
main (int argc, const char *argv[])
{
	extern	char	*getenv ();
	char	*term = getenv ("TERM");
	int	i;
	const	char	*text;

	progname = *argv;

	tc_init (stdout, term);

	// printattr (is);	/* Initialisierungsstring ausgeben */

	text = "Test";
	if ( argc > 1 )
		text = *++argv;

	tc_attr (stdout, TC_BOLD, 1);
	printf ("Bold Headline\n");
	tc_attr (stdout, TC_BOLD, 0);
	for ( i = 0; i < 8; i++ )
	{
		tc_attr (stdout, i, 1);
		printf ("%s", text);
		tc_attr (stdout, i, 0);

#if 0
		tc_attr (stdout, (i | TC_BOLD), 1);
		printf ("\t%s", text);
		tc_attr (stdout, (i | TC_BOLD), 0);

		tc_attr (stdout, (i | TC_ITALIC), 1);
		printf ("\t%s", text);
		tc_attr (stdout, (i | TC_ITALIC), 0);

		tc_attr (stdout, (i | TC_BOLD | TC_ITALIC), 1);
		printf ("\t%s", text);
		tc_attr (stdout, (i | TC_BOLD | TC_ITALIC), 0);
#endif
		printf ("\n");
	}
	printf ("now back to black\n");

	// printattr (r2);	/* Zuruecksetzen */

	return (0);
}
#endif
