/*	$NetBSD: soaserial.c,v 1.1.1.1 2015/07/08 15:37:48 christos Exp $	*/

/*****************************************************************
**
**	@(#) soaserial.c -- helper function for the dnssec zone key tools
**
**	Copyright (c) Jan 2005, Holger Zuleger HZnet. All rights reserved.
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
# include <ctype.h>
# include <sys/types.h>
# include <sys/stat.h>
# include <time.h>
# include <utime.h>
# include <assert.h>
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
# include "config_zkt.h"
# include "zconf.h"
# include "log.h"
# include "debug.h"
#define extern
# include "soaserial.h"
#undef extern

static	int	inc_soa_serial (FILE *fp, int use_unixtime);
static	int	is_soa_rr (const char *line);
static	const	char	*strfindstr (const char *str, const char *search);


/****************************************************************
**
**	int	inc_serial (filename, use_unixtime)
**
**	This function depends on a special syntax formating the
**	SOA record in the zone file!!
**
**	To match the SOA record, the SOA RR must be formatted
**	like this:
**	@ [ttl]   IN  SOA <master.fq.dn.> <hostmaster.fq.dn.> (
**	<SPACEes or TABs>      1234567890; serial number 
**	<SPACEes or TABs>      86400	 ; other values
**				...
**	The space from the first digit of the serial number to
**	the first none white space char or to the end of the line
**	must be at least 10 characters!
**	So you have to left justify the serial number in a field
**	of at least 10 characters like this:
**	<SPACEes or TABs>      1         ; Serial 
**
**	Since ZKT 1.1.0 single line SOA records are also supported
**
****************************************************************/
int	inc_serial (const char *fname, int use_unixtime)
{
	FILE	*fp;
	char	buf[4095+1];
	int	error;
	int	serial_pos;

	/**
	   since BIND 9.4, there is a dnssec-signzone option available for
	   serial number increment.
	   If the user requests "unixtime"; then use this mechanism.
	**/
	if ( use_unixtime )
		return 0;

	if ( (fp = fopen (fname, "r+")) == NULL )
		return -1;

		/* read until the line matches the beginning of a soa record ... */
	while ( fgets (buf, sizeof buf, fp) )
	{
		dbg_val ("inc_serial() checking line for SOA RR \"%s\"\n", buf);
		serial_pos = is_soa_rr (buf);
		if ( serial_pos )	/* SOA record found ? */
			break;
	}

	if ( feof (fp) )
	{
		fclose (fp);
		return -2;
	}
	dbg_val ("serial_pos = %d\n", serial_pos);
	if (serial_pos > 1 )	/* if we found a single line SOA RR */
		fseek (fp, -(long)serial_pos, SEEK_CUR);	/* go back to the beginning of the line */

	error = inc_soa_serial (fp, use_unixtime);	/* .. inc soa serial no ... */
	dbg_val ("inc_soa_serial() returns %d\n", error);

	if ( fclose (fp) != 0 )		/* close the zone file in any case */
		return -5;
	return error;
}

#if 0
/*****************************************************************
**	check if line is the beginning of a SOA RR record, thus
**	containing the string "IN .* SOA" and ends with a '('
**	returns 1 if true
*****************************************************************/
static	int	is_soa_rr (const char *line)
{
	const	char	*p;

	assert ( line != NULL );

		/* line contains "IN" and "SOA" */
	if ( (p = strfindstr (line, "IN")) && strfindstr (p+2, "SOA") )
	{
		p = line + strlen (line) - 1;
		while ( p > line && isspace (*p) )
			p--;
		if ( *p == '(' )	/* last character must be a '(' to start a multi line record */
			return 1;
	}

	return 0;
}
#else
/*****************************************************************
**
**	check if line is the beginning of a SOA RR record, thus
**	containing the string "IN .* SOA" and ends with a '('
**	(multiline record) or is a single line record.
**
**	returns 1 if it is a multi line record (for compability to
**	the old function) or the position of the serial number
**	field counted from the end of the line 
**
*****************************************************************/
static	int	is_soa_rr (const char *line)
{
	const	char	*p;
	const	char	*soa_p;

	assert ( line != NULL );

			/* line contains "IN" and "SOA" ? */
	if ( (p = strfindstr (line, "IN")) && (soa_p = strfindstr (p+2, "SOA")) )
	{
		int	len = strlen (line);

		/* check for multiline record */
		p = line + len - 1;
		while ( p > line && isspace (*p) )
			p--;
		if ( *p == '(' )	/* last character must be a '(' to start a multi line record */
			return 1;

		/* line is single line record */
		p = soa_p + 3;			/* start just behind the SOA string */
		dbg_val1 ("p = \"%s\"\n", p);
		p += strspn (p, " \t");		/* skip white space */
		p += strcspn (p, " \t");	/* skip primary master */
		p += strspn (p, " \t");		/* skip white space */
		p += strcspn (p, " \t");	/* skip mail address */
		dbg_val1 ("p = \"%s\"\n", p);

		dbg_val1 ("is_soa_rr returns = %d\n", (line+len) - p);
		return (line+len) - p;	/* position of serial nr from the end of the line */
	}

	return 0;
}
#endif

/*****************************************************************
**	Find string 'search' in 'str' and ignore case in comparison.
**	returns the position of 'search' in 'str' or NULL if not found.
*****************************************************************/
static	const	char	*strfindstr (const char *str, const char *search)
{
	const	char	*p;
	int		c;

	assert ( str != NULL );
	assert ( search != NULL );

	c = tolower (*search);
	p = str;
	do {
		while ( *p && tolower (*p) != c )
			p++;
		if ( strncasecmp (p, search, strlen (search)) == 0 )
			return p;
		p++;
	} while ( *p );

	return NULL;
}

/*****************************************************************
**	return the serial number of the given time in the form
**	of YYYYmmdd00 as ulong value
*****************************************************************/
static	ulong	serialtime (time_t sec)
{
	struct	tm	*t;
	ulong	serialtime;

	t = gmtime (&sec);
	serialtime = (t->tm_year + 1900) * 10000;
	serialtime += (t->tm_mon+1) * 100;
	serialtime += t->tm_mday;
	serialtime *= 100;

	return serialtime;
}

/*****************************************************************
**	inc_soa_serial (fp, use_unixtime)
**	increment the soa serial number of the file 'fp'
**	'fp' must be opened "r+"
**	returns 0 on success or a negative value in case of an error
*****************************************************************/
static	int	inc_soa_serial (FILE *fp, int use_unixtime)
{
	int	c;
	long	pos,	eos;
	ulong	serial;
	int	digits;
	ulong	today;

	/* move forward until any non ws is reached */
	while ( (c = getc (fp)) != EOF && isspace (c) )
		;
	ungetc (c, fp);		/* push back the last char */

	pos = ftell (fp);	/* mark position */

	serial = 0L;	/* read in the current serial number */
	/* be aware of the trailing space in the format string !! */
	if ( fscanf (fp, "%lu ", &serial) != 1 )	/* try to get serial no */
		return -3;
	eos = ftell (fp);	/* mark first non digit/ws character pos */

	digits = eos - pos;
	if ( digits < 10 )	/* not enough space for serial no ? */
		return -4;

	today = time (NULL);
	if ( !use_unixtime )
	{
		today = serialtime (today);	/* YYYYmmdd00 */
		if ( serial > 1970010100L && serial < today )	
			serial = today;			/* set to current time */
		serial++;			/* increment anyway */
	}

	fseek (fp, pos, SEEK_SET);	/* go back to the beginning */
	fprintf (fp, "%-*lu", digits, serial);	/* write as many chars as before */

	return 0;	/* yep! */
}

/*****************************************************************
**	return the error text of the inc_serial return coode
*****************************************************************/
const	char	*inc_errstr (int err)
{
	switch ( err )
	{
	case -1:	return "couldn't open zone file for modifying";
	case -2:	return "unexpected end of file";
	case -3:	return "no serial number found in zone file";
	case -4:	return "not enough space left for serialno";
	case -5:	return "error on closing zone file";
	}
	return "";
}

#ifdef SOA_TEST
const char *progname;
main (int argc, char *argv[])
{
	ulong	now;
	int	err;
	char	cmd[255];

	progname = *argv;

	now = time (NULL);
	now = serialtime (now);
	printf ("now = %lu\n", now);

	if ( (err = inc_serial (argv[1], 0)) < 0 )
	{
		fprintf (stderr, "can't change serial no: errno=%d %s\n",
					err, inc_errstr (err));
		exit (1);
	}

	snprintf (cmd, sizeof(cmd), "head -15 %s", argv[1]);
	system (cmd);
}
#endif

