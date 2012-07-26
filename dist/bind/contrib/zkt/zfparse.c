/*****************************************************************
**
**	@(#) zfparse.c -- A zone file parser
**
**	Copyright (c) Jan 2010 - Jan 2010, Holger Zuleger HZnet. All rights reserved.
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
# include <stdlib.h>
# include <unistd.h>	/* for link(), unlink() */
# include <ctype.h>
# include <assert.h>
#if 0
# include <sys/types.h>
# include <sys/stat.h>
# include <time.h>
# include <utime.h>
# include <errno.h>
# include <fcntl.h>
#endif
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
# include "config_zkt.h"
# include "zconf.h"
# include "log.h"
# include "debug.h"
#define extern
# include "zfparse.h"
#undef extern


extern	const	char	*progname;

/*****************************************************************
**	is_multiline_rr (const char *s)
*****************************************************************/
static	const	char	*is_multiline_rr (int *multi_line_rr, const char *p)
{
	while ( *p && *p != ';' )
	{
		if ( *p == '\"' )
			do
				p++;
			while ( *p && *p != '\"' );

		if ( *p == '(' )
			*multi_line_rr = 1;
		if ( *p == ')' )
			*multi_line_rr = 0;
		p++;
	}
	return p;
}

/*****************************************************************
**	skipws (const char *s)
*****************************************************************/
static	const	char	*skipws (const char *s)
{
	while ( *s && (*s == ' ' || *s == '\t' || *s == '\n') )
		s++;
	return s;
}

/*****************************************************************
**	skiplabel (const char *s)
*****************************************************************/
static	const	char	*skiplabel (const char *s)
{
	while ( *s && *s != ';' && *s != ' ' && *s != '\t' && *s != '\n' )
		s++;
	return s;
}

/*****************************************************************
**	setminmax ()
*****************************************************************/
static	void	setminmax (long *pmin, long val, long *pmax)
{
	if ( val < *pmin )
		*pmin = val;
	if ( val > *pmax )
		*pmax = val;
}

/*****************************************************************
**	get_ttl ()
*****************************************************************/
static	long	get_ttl (const char *s)
{
	char	quantity;
	long	lval;

	quantity = 'd';
	sscanf (s, "%ld%c", &lval, &quantity);
	quantity = tolower (quantity);
	if  ( quantity == 'm' )
		lval *= MINSEC;
	else if  ( quantity == 'h' )
		lval *= HOURSEC;
	else if  ( quantity == 'd' )
		lval *= DAYSEC;
	else if  ( quantity == 'w' )
		lval *= WEEKSEC;
	else if  ( quantity == 'y' )
		lval *= YEARSEC;

	return lval;
}

/*****************************************************************
**	addkeydb ()
*****************************************************************/
int	addkeydb (const char *file, const char *keydbfile)
{
	FILE	*fp;

	if ( (fp = fopen (file, "a")) == NULL )
		return -1;

	fprintf (fp, "\n");	
	fprintf (fp, "$INCLUDE %s\t; this is the database of public DNSKEY RR\n", keydbfile);	

	fclose (fp);

	return 0;
}

/*****************************************************************
**	parsezonefile ()
**	parse the BIND zone file 'file' and store the minimum and
**	maximum ttl value in the corresponding parameter.
**	if keydbfile is set, check if this file is already include.
**	return 0 if keydbfile is not included
**	return 1 if keydbfile is included
**	return -1 on error
*****************************************************************/
int	parsezonefile (const char *file, long *pminttl, long *pmaxttl, const char *keydbfile)
{
	FILE	*infp;
	int	len;
	int	lnr;
	long	ttl;
	int	multi_line_rr;
	int	keydbfilefound;
	char	buf[1024];
	const	char	*p;

	assert (file != NULL);
	assert (pminttl != NULL);
	assert (pmaxttl != NULL);

	dbg_val4 ("parsezonefile (\"%s\", %ld, %ld, \"%s\")\n", file, *pminttl, *pmaxttl, keydbfile);

	if ( (infp = fopen (file, "r")) == NULL )
		return -1;

	lnr = 0;
	keydbfilefound = 0;
	multi_line_rr = 0;
	while ( fgets (buf, sizeof buf, infp) != NULL ) 
	{
		len = strlen (buf);
		if ( buf[len-1] != '\n' )	/* line too long ? */
			fprintf (stderr, "line too long\n");
		lnr++;

		p = buf;
		if ( multi_line_rr )	/* skip line if it's part of a multiline rr */
		{
			is_multiline_rr (&multi_line_rr, p);
			continue;
		}

		if ( *p == '$' )	/* special directive ? */
		{
			if ( strncmp (p+1, "TTL", 3) == 0 )	/* $TTL ? */
			{
				ttl = get_ttl (p+4);
				dbg_val3 ("%s:%d:ttl %ld\n", file, lnr, ttl);
				setminmax (pminttl, ttl, pmaxttl);
			}
			else if ( strncmp (p+1, "INCLUDE", 7) == 0 )	/* $INCLUDE ? */
			{
				char	fname[30+1];

				sscanf (p+9, "%30s", fname);
				dbg_val ("$INCLUDE directive for file \"%s\" found\n", fname);
				if ( keydbfile && strcmp (fname, keydbfile) == 0 )
					keydbfilefound = 1;
				else
					keydbfilefound = parsezonefile (fname, pminttl, pmaxttl, keydbfile);
			}
		}
		else if ( !isspace (*p) )	/* label ? */
			p = skiplabel (p);

		p = skipws (p);
		if ( *p == ';' )	/* skip line if it's  a comment line */
			continue;

			/* skip class (hesiod is not supported now) */
		if ( (toupper (*p) == 'I' && toupper (p[1]) == 'N') ||
		     (toupper (*p) == 'C' && toupper (p[1]) == 'H') )
			p += 2;
		p = skipws (p);

		if ( isdigit (*p) )	/* ttl ? */
		{
			ttl = get_ttl (p);
			dbg_val3 ("%s:%d:ttl %ld\n", file, lnr, ttl);
			setminmax (pminttl, ttl, pmaxttl);
		}

		/* check the rest of the line if it's the beginning of a multi_line_rr */
		is_multiline_rr (&multi_line_rr, p);
	}

	if ( file )
		fclose (infp);

	dbg_val5 ("parsezonefile (\"%s\", %ld, %ld, \"%s\") ==> %d\n",
			file, *pminttl, *pmaxttl, keydbfile, keydbfilefound);
	return keydbfilefound;
}


#ifdef TEST
const char *progname;
int	main (int argc, char *argv[])
{
	long	minttl;
	long	maxttl;
	int	keydbfound;
	char	*dnskeydb;

	progname = *argv;
	dnskeydb = NULL;
	dnskeydb = "dnskey.db";

	minttl = 0x7FFFFFFF;
	maxttl = 0;
	keydbfound = parsezonefile (argv[1], &minttl, &maxttl, dnskeydb);
	if ( keydbfound < 0 )
		error ("can't parse zone file %s\n", argv[1]);

	if ( dnskeydb && !keydbfound )
	{
		printf ("$INCLUDE %s directive added \n", dnskeydb);
		addkeydb (argv[1], dnskeydb);
	}

	printf ("minttl = %ld\n", minttl);
	printf ("maxttl = %ld\n", maxttl);

	return 0;
}
#endif
