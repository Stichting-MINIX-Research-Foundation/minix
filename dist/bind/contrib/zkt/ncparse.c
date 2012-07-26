/*****************************************************************
**	
**	@(#) ncparse.c -- A very simple named.conf parser
**
**	Copyright (c) Apr 2005 - Nov 2007, Holger Zuleger HZnet. All rights reserved.
**
**	This software is open source.
**
**	Redistribution and use in source and binary forms, with or without
**	modification, are permitted provided that the following conditions
**	are met:
**
**	Redistributions of source code must retain the above copyright notice,
**	this list of conditions and the following disclaimer.
**
**	Redistributions in binary form must reproduce the above copyright notice,
**	this list of conditions and the following disclaimer in the documentation
**	and/or other materials provided with the distribution.
**
**	Neither the name of Holger Zuleger HZnet nor the names of its contributors may
**	be used to endorse or promote products derived from this software without
**	specific prior written permission.
**
**	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
**	"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
**	TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
**	PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE
**	LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
**	CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
**	SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
**	INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
**	CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
**	ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
**	POSSIBILITY OF SUCH DAMAGE.
**
*****************************************************************/
# include <stdio.h>
# include <string.h>
# include <ctype.h>
# include <assert.h>
# include "debug.h"
# include "misc.h"
# include "log.h"
#define extern
# include "ncparse.h"
#undef extern

# define	TOK_STRING	257
# define	TOK_DIR		258
# define	TOK_INCLUDE	259

# define	TOK_ZONE	260
# define	TOK_TYPE	261
# define	TOK_MASTER	262
# define	TOK_SLAVE	263
# define	TOK_STUB	264
# define	TOK_HINT	265
# define	TOK_FORWARD	266
# define	TOK_DELEGATION	267
# define	TOK_VIEW	268

# define	TOK_FILE	270

# define	TOK_UNKNOWN	511

/* list of "named.conf" keywords we are interested in */
static struct KeyWords {
	char	*name;
	int	tok;
} kw[] = {
	{ "STRING",	TOK_STRING },
	{ "include",	TOK_INCLUDE },
	{ "directory",	TOK_DIR },
	{ "file",	TOK_FILE },
	{ "zone",	TOK_ZONE },
#if 0	/* we don't need the type keyword; master, slave etc. is sufficient */
	{ "type",	TOK_TYPE },
#endif
	{ "master",	TOK_MASTER },
	{ "slave",	TOK_SLAVE },
	{ "stub",	TOK_STUB },
	{ "hint",	TOK_HINT },
	{ "forward",	TOK_FORWARD },
	{ "delegation-only", TOK_DELEGATION },
	{ "view",	TOK_VIEW },
	{ NULL,		TOK_UNKNOWN },
};

#ifdef DBG
static	const char	*tok2str (int  tok)
{
	int	i;

	i = 0;
	while ( kw[i].name && kw[i].tok != tok )
		i++;

	return kw[i].name;
}
#endif

static	int	searchkw (const char *keyword)
{
	int	i;

	dbg_val ("ncparse: searchkw (%s)\n", keyword);
	i = 0;
	while ( kw[i].name && strcmp (kw[i].name, keyword) != 0 )
		i++;

	return kw[i].tok;
}

static	int	gettok (FILE *fp, char *val, size_t valsize)
{
	int	lastc;
	int	c;
	char	buf[255+1];
	char	*p;
	char	*bufend;

	*val = '\0';
	do {
		while ( (c = getc (fp)) != EOF && isspace (c) )
			;

		if ( c == '#' )		/* single line comment ? */
		{
			while ( (c = getc (fp)) != EOF && c != '\n' )
				;
			continue;
		}

		if ( c == EOF )
			return EOF;

		if ( c == '{' || c == '}' || c == ';' )
			continue;

		if ( c == '/' )		/* begin of C comment ? */
		{
			if ( (c = getc (fp)) == '*' )	/* yes! */
			{
				lastc = EOF;		/* read until end of c comment */
				while ( (c = getc (fp)) != EOF && !(lastc == '*' && c == '/') )
					lastc = c;
			}	
			else if ( c == '/' )	/* is it a C single line comment ? */
			{
				while ( (c = getc (fp)) != EOF && c != '\n' )
					;
			}
			else		/* no ! */
				ungetc (c, fp);
			continue;
		}

		if ( c == '\"' )
		{
			p = val;
			bufend = val + valsize - 1;
			while ( (c = getc (fp)) != EOF && p < bufend && c != '\"' )
				*p++ = c;
			*p = '\0';
			/* if string buffer is too small, eat up rest of string */
			while ( c != EOF && c != '\"' )
				c = getc (fp);
			
			return TOK_STRING;
		}

		p = buf;
		bufend = buf + sizeof (buf) - 1;
		do
			*p++ = tolower (c);
		while ( (c = getc (fp)) != EOF && p < bufend && (isalpha (c) || c == '-') );
		*p = '\0';
		ungetc (c, fp);

		if ( (c = searchkw (buf)) != TOK_UNKNOWN )
			return c;
	}  while ( c != EOF );

	return EOF;
}

/*****************************************************************
**
**	parse_namedconf (const char *filename, chroot_dir, dir, dirsize, int (*func) ())
**
**	Very dumb named.conf parser.
**	- In a zone declaration the _first_ keyword MUST be "type"
**	- For every master zone "func (directory, zone, filename)" will be called
**
*****************************************************************/
int	parse_namedconf (const char *filename, const char *chroot_dir, char *dir, size_t dirsize, int (*func) ())
{
	FILE	*fp;
	int	tok;
	char	path[511+1];
#if 1	/* this is potentialy too small for key data, but we don't need the keys... */
	char	strval[255+1];		
#else
	char	strval[4095+1];
#endif
	char	view[255+1];
	char	zone[255+1];
	char	zonefile[255+1];

	dbg_val ("parse_namedconf: parsing file \"%s\" \n", filename);

	assert (filename != NULL);
	assert (dir != NULL && dirsize != 0);
	assert (func != NULL);

	view[0] = '\0';
	if ( (fp = fopen (filename, "r")) == NULL )
		return 0;

	while ( (tok = gettok (fp, strval, sizeof strval)) != EOF )
	{
		if ( tok > 0 && tok < 256 )
		{
			error ("parse_namedconf: token found with value %-10d: %c\n", tok, tok);
			lg_mesg (LG_ERROR, "parse_namedconf: token found with value %-10d: %c", tok, tok);
		}
		else if ( tok == TOK_DIR )
		{
			if ( gettok (fp, strval, sizeof (strval)) == TOK_STRING )
			{
				dbg_val2 ("parse_namedconf: directory found \"%s\" (dir is %s)\n",
										 strval, dir);
				if ( *strval != '/' &&  *dir )
					snprintf (path, sizeof (path), "%s/%s", dir, strval);
				else
					snprintf (path, sizeof (path), "%s", strval);

				/* prepend chroot directory (do it only once) */
				if ( chroot_dir && *chroot_dir )
				{
					snprintf (dir, dirsize, "%s%s%s", chroot_dir, *path == '/' ? "": "/", path);
					chroot_dir = NULL;
				}
				else
					snprintf (dir, dirsize, "%s", path);
				dbg_val ("parse_namedconf: new dir \"%s\" \n", dir);
			}	
		}	
		else if ( tok == TOK_INCLUDE )
		{
			if ( gettok (fp, strval, sizeof (strval)) == TOK_STRING )
			{
				if ( *strval != '/' && *dir )
					snprintf (path, sizeof (path), "%s/%s", dir, strval);
				else
					snprintf (path, sizeof (path), "%s", strval);
				if ( !parse_namedconf (path, chroot_dir, dir, dirsize, func) )
					return 0;
			}
			else
			{
				error ("parse_namedconf: need a filename after \"include\"!\n");
				lg_mesg (LG_ERROR, "parse_namedconf: need a filename after \"include\"!");
			}
		}
		else if ( tok == TOK_VIEW )
		{
			if ( gettok (fp, strval, sizeof (strval)) != TOK_STRING )
				continue;
			snprintf (view, sizeof view, "%s", strval);	/* store the name of the view */
		}
		else if ( tok == TOK_ZONE )
		{
			if ( gettok (fp, strval, sizeof (strval)) != TOK_STRING )
				continue;
			snprintf (zone, sizeof zone, "%s", strval);	/* store the name of the zone */

			if ( gettok (fp, strval, sizeof (strval)) != TOK_MASTER )
				continue;
			if ( gettok (fp, strval, sizeof (strval)) != TOK_FILE )
				continue;
			if ( gettok (fp, strval, sizeof (strval)) != TOK_STRING )
				continue;
			snprintf (zonefile, sizeof zonefile, "%s", strval);	/* this is the filename */

			dbg_val4 ("dir %s view %s zone %s file %s\n", dir, view, zone, zonefile);
			(*func) (dir, view, zone, zonefile);
		}
		else 
			dbg_val3 ("%-10s(%d): %s\n", tok2str(tok), tok, strval);
	}
	fclose (fp);

	return 1;
}

#ifdef TEST_NCPARSE
int	printzone (const char *dir, const char *view, const char *zone, const char *file)
{
	printf ("printzone ");
	printf ("view \"%s\" " , view);
	printf ("zone \"%s\" " , zone);
	printf ("file ");
	if ( dir && *dir )
		printf ("%s/", dir, file);
	printf ("%s", file);
	putchar ('\n');
	return 1;
}

char	*progname;

main (int argc, char *argv[])
{
	char	directory[255+1];

	progname = argv[0];

	directory[0] = '\0';
	if ( --argc == 0 )
		parse_namedconf ("/var/named/named.conf", NULL, directory, sizeof (directory), printzone);
	else 
		parse_namedconf (argv[1], NULL, directory, sizeof (directory), printzone);
}
#endif
