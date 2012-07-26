/*****************************************************************
**
**	@(#) misc.c -- helper functions for the dnssec zone key tools
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
# include <unistd.h>	/* for link(), unlink() */
# include <ctype.h>
# include <sys/types.h>
# include <sys/stat.h>
# include <time.h>
# include <utime.h>
# include <assert.h>
# include <errno.h>
# include <fcntl.h>
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
# include "config_zkt.h"
# include "zconf.h"
# include "log.h"
# include "debug.h"
#define extern
# include "misc.h"
#undef extern

# define	TAINTEDCHARS	"`$@;&<>|"

extern	const	char	*progname;

/*****************************************************************
**	getnameappendix (progname, basename)
**	return a pointer to the substring in progname subsequent
**	following "<basename>-".
*****************************************************************/
const	char	*getnameappendix (const char *progname, const char *basename)
{
	const	char	*p;
	int	baselen;

	assert (progname != NULL);
	assert (basename != NULL);

	if ( (p = strrchr (progname, '/')) != NULL )
		p++;
	else
		p = progname;

	baselen = strlen (basename);
	if ( strncmp (p, basename, baselen-1) == 0 && *(p+baselen) == '-' )
	{
		p += baselen + 1;
		if ( *p )
			return p;
	}

	return NULL;
}

/*****************************************************************
**	getdefconfname (view)
**	returns a pointer to a dynamic string containing the
**	default configuration file name
*****************************************************************/
const	char	*getdefconfname (const char *view)
{
	char	*p;
	char	*file;
	char	*buf;
	int	size;
	
	if ( (file = getenv ("ZKT_CONFFILE")) == NULL )
		file = CONFIG_FILE;
	dbg_val2 ("getdefconfname (%s) file = %s\n", view ? view : "NULL", file);

	if ( view == NULL || *view == '\0' || (p = strrchr (file, '.')) == NULL )
		return strdup (file);

	size = strlen (file) + strlen (view) + 1 + 1;
	if ( (buf = malloc (size)) == NULL )
		return strdup (file);

	dbg_val1 ("0123456789o123456789o123456789\tsize=%d\n", size);
	dbg_val4 ("%.*s-%s%s\n", p - file, file, view, p);

	snprintf (buf, size, "%.*s-%s%s", p - file, file, view, p);
	return buf;	
}

/*****************************************************************
**	domain_canonicdup (s)
**	returns NULL or a pointer to a dynamic string containing the
**	canonic (all lower case letters and ending with a '.')
**	domain name
*****************************************************************/
char	*domain_canonicdup (const char *s)
{
	char	*new;
	char	*p;
	int	len;
	int	add_dot;

	if ( s == NULL )
		return NULL;

	add_dot = 0;
	len = strlen (s);
	if ( len > 0 && s[len-1] != '.' )
		add_dot = len++;

	if ( (new = p = malloc (len + 1)) == NULL )
		return NULL;

	while ( *s )
		*p++ = tolower (*s++);
	if ( add_dot )
		*p++ = '.';
	*p = '\0';

	return new;
}
#if 0		/* replaced by domain_canonicdup */
/*****************************************************************
**	str_tolowerdup (s)
*****************************************************************/
char	*str_tolowerdup (const char *s)
{
	char	*new;
	char	*p;

	if ( s == NULL || (new = p = malloc (strlen (s) + 1)) == NULL )
		return NULL;

	while ( *s )
		*p++ = tolower (*s++);
	*p = '\0';

	return new;
}
#endif

/*****************************************************************
**	str_delspace (s)
**	Remove in string 's' all white space char 
*****************************************************************/
char	*str_delspace (char *s)
{
	char	*start;
	char	*p;

	if ( !s )	/* no string present ? */
		return NULL;

	start = s;
	for ( p = s; *p; p++ )
		if ( !isspace (*p) )
			*s++ = *p;	/* copy each nonspace */

	*s = '\0';	/* terminate string */

	return start;
}

/*****************************************************************
**	in_strarr (str, arr, cnt)
**	check if string array 'arr' contains the string 'str'
**	return 1 if true or 'arr' or 'str' is empty, otherwise 0
*****************************************************************/
int	in_strarr (const char *str, char *const arr[], int cnt)
{
	if ( arr == NULL || cnt <= 0 )
		return 1;

	if ( str == NULL || *str == '\0' )
		return 0;

	while ( --cnt >= 0 )
		if ( strcmp (str, arr[cnt]) == 0 )
			return 1;

	return 0;
}

/*****************************************************************
**	str_untaint (s)
**	Remove in string 's' all TAINTED chars
*****************************************************************/
char	*str_untaint (char *str)
{
	char	*p;

	assert (str != NULL);

	for ( p = str; *p; p++ )
		if ( strchr (TAINTEDCHARS, *p) )
			*p = ' ';
	return str;
}

/*****************************************************************
**	str_chop (str, c)
**	delete all occurrences of char 'c' at the end of string 's'
*****************************************************************/
char	*str_chop (char *str, char c)
{
	int	len;

	assert (str != NULL);

	len = strlen (str) - 1;
	while ( len >= 0 && str[len] == c )
		str[len--] = '\0';

	return str;
}

/*****************************************************************
**	parseurl (url, &proto, &host, &port, &para )
**	parses the given url (e.g. "proto://host.with.domain:port/para")
**	and set the pointer variables to the corresponding part of the string.
*****************************************************************/
void	parseurl (char *url, char **proto, char **host, char **port, char **para)
{
	char	*start;
	char	*p;

	assert ( url != NULL );

	/* parse protocol */
	if ( (p = strchr (url, ':')) == NULL )	/* no protocol string given ? */
		p = url;
	else					/* looks like a protocol string */
		if ( p[1] == '/' && p[2] == '/' )	/* protocol string ? */
		{
			*p = '\0';
			p += 3;
			if ( proto )
				*proto = url;
		}
		else				/* no protocol string found ! */
			p = url;

	/* parse host */
	if ( *p == '[' )	/* ipv6 address as hostname ? */
	{
		for ( start = ++p; *p && *p != ']'; p++ )
			;
		if ( *p )
			*p++ = '\0';
	}
	else
		for ( start = p; *p && *p != ':' && *p != '/'; p++ )
			;
	if ( host )
		*host = start;

	/* parse port */
	if ( *p == ':' )
	{
		*p++ = '\0';
		for ( start = p; *p && isdigit (*p); p++ )
			;
		if ( *p )
			*p++ = '\0';
		if ( port )
			*port = start;
	}

	if ( *p == '/' )
		*p++ = '\0';

	if ( *p && para )
		*para = p;
}

/*****************************************************************
**	splitpath (path, pathsize, filename)
**	if filename is build of "path/file" then copy filename to path
**	and split of the filename part.
**	return pointer to filename part in path or NULL if path is too
**	small to hold "path+filename"
*****************************************************************/
const	char	*splitpath (char *path, size_t psize, const char *filename)
{
	char 	*p;

	if ( !path )
		return NULL;

	*path = '\0';
	if ( !filename )
		return filename;

	if ( (p = strrchr (filename, '/')) )	/* file arg contains path ? */
	{
		if ( strlen (filename) + 1 > psize )
			return filename;

		strcpy (path, filename);	/* copy whole filename to path */
		path[p-filename] = '\0';	/* split of the file part */
		filename = ++p;
	}
	return filename;
}

/*****************************************************************
**	pathname (path, size, dir, file, ext)
**	Concatenate 'dir', 'file' and 'ext' (if not null) to build
**	a pathname, and store the result in the character array
**	with length 'size' pointed to by 'path'.
*****************************************************************/
char	*pathname (char *path, size_t size, const char *dir, const char *file, const char *ext)
{
	int	len;

	if ( path == NULL || file == NULL )
		return path;

	len = strlen (file) + 1;
	if ( dir )
		len += strlen (dir);
	if ( ext )
		len += strlen (ext);
	if ( len > size )
		return path;

	*path = '\0';
	if ( dir && *dir )
	{
		len = sprintf (path, "%s", dir);
		if ( path[len-1] != '/' )
		{
			path[len++] = '/';
			path[len] = '\0';
		}
	}
	strcat (path, file);
	if ( ext )
		strcat (path, ext);
	return path;
}

/*****************************************************************
**	is_directory (name)
**	Check if the given pathname 'name' exists and is a directory.
**	returns 0 | 1
*****************************************************************/
int	is_directory (const char *name)
{
	struct	stat	st;

	if ( !name || !*name )	
		return 0;
	
	return ( stat (name, &st) == 0 && S_ISDIR (st.st_mode) );
}

/*****************************************************************
**	fileexist (name)
**	Check if a file with the given pathname 'name' exists.
**	returns 0 | 1
*****************************************************************/
int	fileexist (const char *name)
{
	struct	stat	st;
	return ( stat (name, &st) == 0 && S_ISREG (st.st_mode) );
}

/*****************************************************************
**	filesize (name)
**	return the size of the file with the given pathname 'name'.
**	returns -1 if the file not exist 
*****************************************************************/
size_t	filesize (const char *name)
{
	struct	stat	st;
	if  ( stat (name, &st) == -1 )
		return -1L;
	return ( st.st_size );
}

/*****************************************************************
**	is_keyfilename (name)
**	Check if the given name looks like a dnssec (public)
**	keyfile name. Returns 0 | 1
*****************************************************************/
int	is_keyfilename (const char *name)
{
	int	len;

	if ( name == NULL || *name != 'K' )
		return 0;

	len = strlen (name);
	if ( len > 4 && strcmp (&name[len - 4], ".key") == 0 ) 
		return 1;

	return 0;
}

/*****************************************************************
**	is_dotfilename (name)
**	Check if the given pathname 'name' looks like "." or "..".
**	Returns 0 | 1
*****************************************************************/
int	is_dotfilename (const char *name)
{
	if ( name && (
	     (name[0] == '.' && name[1] == '\0') || 
	     (name[0] == '.' && name[1] == '.' && name[2] == '\0')) )
		return 1;

	return 0;
}

/*****************************************************************
**	touch (name, sec)
**	Set the modification time of the given pathname 'fname' to
**	'sec'.	Returns 0 on success.
*****************************************************************/
int	touch (const char *fname, time_t sec)
{
	struct	utimbuf	utb;

	utb.actime = utb.modtime = sec;
	return utime (fname, &utb);
}

/*****************************************************************
**	linkfile (fromfile, tofile)
*****************************************************************/
int	linkfile (const char *fromfile, const char *tofile)
{
	int	ret;

	/* fprintf (stderr, "linkfile (%s, %s)\n", fromfile, tofile); */
	if ( (ret = link (fromfile, tofile)) == -1 && errno == EEXIST )
		if ( unlink (tofile) == 0 )
			ret = link (fromfile, tofile);

	return ret;
}

/*****************************************************************
**	copyfile (fromfile, tofile, dnskeyfile)
**	copy fromfile into tofile.
**	Add (optional) the content of dnskeyfile to tofile.
*****************************************************************/
int	copyfile (const char *fromfile, const char *tofile, const char *dnskeyfile)
{
	FILE	*infp;
	FILE	*outfp;
	int	c;

	/* fprintf (stderr, "copyfile (%s, %s)\n", fromfile, tofile); */
	if ( (infp = fopen (fromfile, "r")) == NULL )
		return -1;
	if ( (outfp = fopen (tofile, "w")) == NULL )
	{
		fclose (infp);
		return -2;
	}
	while ( (c = getc (infp)) != EOF ) 
		putc (c, outfp);

	fclose (infp);
	if ( dnskeyfile && *dnskeyfile && (infp = fopen (dnskeyfile, "r")) != NULL )
	{
		while ( (c = getc (infp)) != EOF ) 
			putc (c, outfp);
		fclose (infp);
	}
	fclose (outfp);

	return 0;
}

/*****************************************************************
**	copyzonefile (fromfile, tofile, dnskeyfile)
**	copy a already signed zonefile and replace all zone DNSKEY
**	resource records by one "$INCLUDE dnskey.db" line
*****************************************************************/
int	copyzonefile (const char *fromfile, const char *tofile, const char *dnskeyfile)
{
	FILE	*infp;
	FILE	*outfp;
	int	len;
	int	dnskeys;
	int	multi_line_dnskey;
	int	bufoverflow;
	char	buf[1024];
	char	*p;

	if ( fromfile == NULL )
		infp = stdin;
	else
		if ( (infp = fopen (fromfile, "r")) == NULL )
			return -1;
	if ( tofile == NULL )
		outfp = stdout;
	else
		if ( (outfp = fopen (tofile, "w")) == NULL )
		{
			if ( fromfile )
				fclose (infp);
			return -2;
		}

	multi_line_dnskey = 0;
	dnskeys = 0;
	bufoverflow = 0;
	while ( fgets (buf, sizeof buf, infp) != NULL ) 
	{
		p = buf;
		if ( !bufoverflow && !multi_line_dnskey && (*p == '@' || isspace (*p)) )	/* check if DNSKEY RR */
		{
			do
				p++;
			while ( isspace (*p) ) ;

			/* skip TTL */
			while ( isdigit (*p) )
				p++;

			while ( isspace (*p) )
				p++;

			/* skip Class */
			if ( strncasecmp (p, "IN", 2) == 0 )
			{
				p += 2;
				while ( isspace (*p) )
					p++;
			}

			if ( strncasecmp (p, "DNSKEY", 6) == 0 )	/* bingo! */
			{
				dnskeys++;
				p += 6;
				while ( *p )
				{
					if ( *p == '(' )
						multi_line_dnskey = 1;
					if ( *p == ')' )
						multi_line_dnskey = 0;
					p++;
				}
				if ( dnskeys == 1 )
					fprintf (outfp, "$INCLUDE %s\n", dnskeyfile);	
			}
			else 
				fputs (buf, outfp);	
		}
		else
		{
			if ( bufoverflow )
				fprintf (stderr, "!! buffer overflow in copyzonefile() !!\n");
			if ( !multi_line_dnskey )
				fputs (buf, outfp);	
			else
			{
				while ( *p && *p != ')' )
					p++;
				if ( *p == ')' )
					multi_line_dnskey = 0;
			}
		}
		
		len = strlen (buf);
		bufoverflow = buf[len-1] != '\n';	/* line too long ? */
	}

	if ( fromfile )
		fclose (infp);
	if ( tofile )
		fclose (outfp);

	return 0;
}

/*****************************************************************
**	cmpfile (file1, file2)
**	returns -1 on error, 1 if the files differ and 0 if they
**	are identical.
*****************************************************************/
int	cmpfile (const char *file1, const char *file2)
{
	FILE	*fp1;
	FILE	*fp2;
	int	c1;
	int	c2;

	/* fprintf (stderr, "cmpfile (%s, %s)\n", file1, file2); */
	if ( (fp1 = fopen (file1, "r")) == NULL )
		return -1;
	if ( (fp2 = fopen (file2, "r")) == NULL )
	{
		fclose (fp1);
		return -1;
	}

	do {
		c1 = getc (fp1);
		c2 = getc (fp2);
	}  while ( c1 != EOF && c2 != EOF && c1 == c2 );

	fclose (fp1);
	fclose (fp2);

	if ( c1 == c2 )
		return 0;
	return 1;
}

/*****************************************************************
**	file_age (fname)
*****************************************************************/
int	file_age (const char *fname)
{
	time_t	curr = time (NULL);
	time_t	mtime = file_mtime (fname);

	return curr - mtime;
}

/*****************************************************************
**	file_mtime (fname)
*****************************************************************/
time_t	file_mtime (const char *fname)
{
	struct	stat	st;

	if ( stat (fname, &st) < 0 )
		return 0;
	return st.st_mtime;
}

/*****************************************************************
**	is_exec_ok (prog)
**	Check if we are running as root or if the file owner of
**	"prog" do not match the current user or the file permissions
**	allows file modification for others then the owner.
**	The same condition will be checked for the group ownership.
**	return 1 if the execution of the command "prog" will not
**	open a big security whole, 0 otherwise
*****************************************************************/
int	is_exec_ok (const char *prog)
{
	uid_t	curr_uid;
	struct	stat	st;

	if ( stat (prog, &st) < 0 )
		return 0;

	curr_uid = getuid ();
	if ( curr_uid == 0 )			/* don't run the cmd if we are root */
		return 0;

	/* if the file owner and the current user matches and */
	/* the file mode is not writable except for the owner, we are save */
	if ( curr_uid == st.st_uid && (st.st_mode & (S_IWGRP | S_IWOTH)) == 0 )
		return 1;

	/* if the file group and the current group matches and */
	/* the file mode is not writable except for the group, we are also save */
	if ( getgid() != st.st_gid && (st.st_mode & (S_IWUSR | S_IWOTH)) == 0 )
		return 1;

	return 0;
}

/*****************************************************************
**	fatal (fmt, ...)
*****************************************************************/
void fatal (char *fmt, ...)
{
        va_list ap;

        va_start(ap, fmt);
        if ( progname )
		fprintf (stderr, "%s: ", progname);
        vfprintf (stderr, fmt, ap);
        va_end(ap);
        exit (127);
}

/*****************************************************************
**	error (fmt, ...)
*****************************************************************/
void error (char *fmt, ...)
{
        va_list ap;

        va_start(ap, fmt);
        vfprintf (stderr, fmt, ap);
        va_end(ap);
}

/*****************************************************************
**	logmesg (fmt, ...)
*****************************************************************/
void logmesg (char *fmt, ...)
{
        va_list ap;

#if defined (LOG_WITH_PROGNAME) && LOG_WITH_PROGNAME
        fprintf (stdout, "%s: ", progname);
#endif
        va_start(ap, fmt);
        vfprintf (stdout, fmt, ap);
        va_end(ap);
}

/*****************************************************************
**	verbmesg (verblvl, conf, fmt, ...)
*****************************************************************/
void	verbmesg (int verblvl, const zconf_t *conf, char *fmt, ...)
{
	char	str[511+1];
        va_list ap;

	str[0] = '\0';
	va_start(ap, fmt);
	vsnprintf (str, sizeof (str), fmt, ap);
	va_end(ap);

	//fprintf (stderr, "verbmesg (%d stdout=%d filelog=%d str = :%s:\n", verblvl, conf->verbosity, conf->verboselog, str);
	if ( verblvl <= conf->verbosity )	/* check if we have to print this to stdout */
		logmesg (str);

	str_chop (str, '\n');
	if ( verblvl <= conf->verboselog )	/* check logging to syslog and/or file */
		lg_mesg (LG_DEBUG, str);
}


/*****************************************************************
**	logflush ()
*****************************************************************/
void logflush ()
{
        fflush (stdout);
}

/*****************************************************************
**	timestr2time (timestr)
**	timestr should look like "20071211223901" for 12 dec 2007 22:39:01
*****************************************************************/
time_t	timestr2time (const char *timestr)
{
	struct	tm	t;
	time_t	sec;

	// fprintf (stderr, "timestr = \"%s\"\n", timestr);
	if ( sscanf (timestr, "%4d%2d%2d%2d%2d%2d", 
			&t.tm_year, &t.tm_mon, &t.tm_mday, 
			&t.tm_hour, &t.tm_min, &t.tm_sec) != 6 )
		return 0L;
	t.tm_year -= 1900;
	t.tm_mon -= 1;
	t.tm_isdst = 0;

#if defined(HAVE_TIMEGM) && HAVE_TIMEGM
	sec = timegm (&t);
#else
	{
	char	tzstr[31+1];
	char	*tz;

	tz = getenv("TZ");
	snprintf (tzstr, sizeof (tzstr), "TZ=%s", "UTC");
	putenv (tzstr);
	tzset();
	sec = mktime(&t);
	if (tz)
		snprintf (tzstr, sizeof (tzstr), "TZ=%s", tz);
	else
		snprintf (tzstr, sizeof (tzstr), "TZ=%s", "");
	putenv (tzstr);
	tzset();
	}
#endif
	
	return sec < 0L ? 0L : sec;
}

/*****************************************************************
**	time2str (sec, precison)
**	sec is seconds since 1.1.1970
**	precison is currently either 's' (for seconds) or 'm' (minutes)
*****************************************************************/
char	*time2str (time_t sec, int precision)
{
	struct	tm	*t;
	static	char	timestr[31+1];	/* 27+1 should be enough */
#if defined(HAVE_STRFTIME) && HAVE_STRFTIME
	char	tformat[127+1];

	timestr[0] = '\0';
	if ( sec <= 0L )
		return timestr;
	t = localtime (&sec);
	if ( precision == 's' )
		strcpy (tformat, "%b %d %Y %T");
	else
		strcpy (tformat, "%b %d %Y %R");
# if PRINT_TIMEZONE
	strcat (tformat, " %z");
# endif
	strftime (timestr, sizeof (timestr), tformat, t);

#else	/* no strftime available */
	static	char	*mstr[] = {
			"Jan", "Feb", "Mar", "Apr", "May", "Jun",
			"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
	};

	timestr[0] = '\0';
	if ( sec <= 0L )
		return timestr;
	t = localtime (&sec);
# if PRINT_TIMEZONE
	{
	int	h,	s;

	s = abs (t->tm_gmtoff);
	h = t->tm_gmtoff / 3600;
	s = t->tm_gmtoff % 3600;
	if ( precision == 's' )
		snprintf (timestr, sizeof (timestr), "%s %2d %4d %02d:%02d:%02d %c%02d%02d",
			mstr[t->tm_mon], t->tm_mday, t->tm_year + 1900, 
			t->tm_hour, t->tm_min, t->tm_sec,
			t->tm_gmtoff < 0 ? '-': '+',
			h, s);
	else
		snprintf (timestr, sizeof (timestr), "%s %2d %4d %02d:%02d %c%02d%02d",
			mstr[t->tm_mon], t->tm_mday, t->tm_year + 1900, 
			t->tm_hour, t->tm_min, 
			t->tm_gmtoff < 0 ? '-': '+',
			h, s);
	}
# else
	if ( precision == 's' )
		snprintf (timestr, sizeof (timestr), "%s %2d %4d %02d:%02d:%02d",
			mstr[t->tm_mon], t->tm_mday, t->tm_year + 1900, 
			t->tm_hour, t->tm_min, t->tm_sec);
	else
		snprintf (timestr, sizeof (timestr), "%s %2d %4d %02d:%02d",
			mstr[t->tm_mon], t->tm_mday, t->tm_year + 1900, 
			t->tm_hour, t->tm_min);
# endif
#endif

	return timestr;
}

/*****************************************************************
**	time2isostr (sec, precison)
**	sec is seconds since 1.1.1970
**	precison is currently either 's' (for seconds) or 'm' (minutes)
*****************************************************************/
char	*time2isostr (time_t sec, int precision)
{
	struct	tm	*t;
	static	char	timestr[31+1];	/* 27+1 should be enough */

	timestr[0] = '\0';
	if ( sec <= 0L )
		return timestr;

	t = gmtime (&sec);
	if ( precision == 's' )
		snprintf (timestr, sizeof (timestr), "%4d%02d%02d%02d%02d%02d",
			t->tm_year + 1900, t->tm_mon+1, t->tm_mday,
			t->tm_hour, t->tm_min, t->tm_sec);
	else
		snprintf (timestr, sizeof (timestr), "%4d%02d%02d%02d%02d",
			t->tm_year + 1900, t->tm_mon+1, t->tm_mday,
			t->tm_hour, t->tm_min);

	return timestr;
}

/*****************************************************************
**	age2str (sec)
**	!!Attention: This function is not reentrant 
*****************************************************************/
char	*age2str (time_t sec)
{
	static	char	str[20+1];	/* "2y51w6d23h50m55s" == 16+1 chars */
	int	len;
	int	strsize = sizeof (str);

	len = 0;
# if PRINT_AGE_WITH_YEAR
	if ( sec / (YEARSEC) > 0 )
	{
		len += snprintf (str+len, strsize - len, "%1luy", sec / YEARSEC );
		sec %= (YEARSEC);
	}
	else
		len += snprintf (str+len, strsize - len, "  ");
# endif
	if ( sec / WEEKSEC > 0 )
	{
		len += snprintf (str+len, strsize - len, "%2luw", (ulong) sec / WEEKSEC );
		sec %= WEEKSEC;
	}
	else
		len += snprintf (str+len, strsize - len, "   ");
	if ( sec / DAYSEC > 0 )
	{
		len += snprintf (str+len, strsize - len, "%2lud", sec / (ulong)DAYSEC);
		sec %= DAYSEC;
	}
	else
		len += snprintf (str+len, strsize - len, "   ");
	if ( sec / HOURSEC > 0 )
	{
		len += snprintf (str+len, strsize - len, "%2luh", sec / (ulong)HOURSEC);
		sec %= HOURSEC;
	}
	else
		len += snprintf (str+len, strsize - len, "   ");
	if ( sec / MINSEC > 0 )
	{
		len += snprintf (str+len, strsize - len, "%2lum", sec / (ulong)MINSEC);
		sec %= MINSEC;
	}
	else
		len += snprintf (str+len, strsize - len, "   ");
	if ( sec > 0 )
		snprintf (str+len, strsize - len, "%2lus", (ulong) sec);
	else
		len += snprintf (str+len, strsize - len, "   ");

	return str;
}

/*****************************************************************
**	start_timer ()
*****************************************************************/
time_t	start_timer ()
{
	return (time(NULL));
}

/*****************************************************************
**	stop_timer ()
*****************************************************************/
time_t	stop_timer (time_t start)
{
	time_t	stop = time (NULL);

	return stop - start;
}


/****************************************************************
**
**	int	gensalt (saltstr, sizeofsaltstr, bits)
**
**	generate a random hexstring of 'bits' salt and store it
**	in saltstr. return 1 on success, otherwise 0.
**
*****************************************************************/
int	gensalt (char *salt, size_t saltsize, int saltbits, unsigned int seed)
{
	static	char	hexstr[] = "0123456789ABCDEF";
	int	saltlen = 0;	/* current length of salt in hex nibbles */
	int	i;
	int	hex;

	if ( seed == 0 )
		srandom (seed = (unsigned int)time (NULL));

	saltlen = saltbits / 4;
	if ( saltlen+1 > saltsize )
		return 0;

	for ( i = 0; i < saltlen; i++ )
	{
		hex = random () % 16;
		assert ( hex >= 0 && hex < 16 );
		salt[i] = hexstr[hex];
	}
	salt[i] = '\0';

	return 1;
}


#ifdef COPYZONE_TEST
const char *progname;
main (int argc, char *argv[])
{
	progname = *argv;

	if ( copyzonefile (argv[1], NULL) < 0 )
		error ("can't copy zone file %s\n", argv[1]);
}
#endif

#ifdef URL_TEST
const char *progname;
main (int argc, char *argv[])
{
	char	*proto;
	char	*host;
	char	*port;
	char	*para;
	char	url[1024];

	progname = *argv;

	proto = host = port = para = NULL;

	if ( --argc <= 0 )
	{
		fprintf (stderr, "usage: url_test <url>\n");
		fprintf (stderr, "e.g.: url_test http://www.hznet.de:80/zkt\n");
		exit (1);
	}
	
	strcpy (url, argv[1]);
	parseurl (url, &proto, &host, &port, &para);

	if ( proto )
		printf ("proto: \"%s\"\n", proto);
	if ( host )
		printf ("host: \"%s\"\n", host);
	if ( port )
		printf ("port: \"%s\"\n", port);
	if ( para )
		printf ("para: \"%s\"\n", para);

}
#endif

