/*****************************************************************
**
**	@(#) log.c -- The ZKT error logging module
**
**	Copyright (c) June 2008, Holger Zuleger HZnet. All rights reserved.
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
**
*****************************************************************/
# include <stdio.h>
# include <string.h>
# include <stdlib.h>
# include <ctype.h>
# include <sys/types.h>
# include <sys/stat.h>
# include <sys/time.h>
# include <time.h>
# include <assert.h>
# include <errno.h>
# include <syslog.h>
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
# include "config_zkt.h"
# include "misc.h"
# include "debug.h"
#define extern
# include "log.h"
#undef extern

/*****************************************************************
**	module internal vars & declarations
*****************************************************************/
static	FILE	*lg_fp;
static	FILE	*lg_fpsave;
static	int	lg_minfilelevel;
static	int	lg_syslogging;
static	int	lg_minsyslevel;
static	long	lg_errcnt;
static	const char	*lg_progname;

typedef	struct {
	lg_lvl_t	level;
	const	char	*str;
	int		syslog_level;
} lg_symtbl_t;

static	lg_symtbl_t	symtbl[] = {
	{ LG_NONE,	"none",		-1 },
	{ LG_DEBUG,	"debug",	LOG_DEBUG },
	{ LG_INFO,	"info",		LOG_INFO },
	{ LG_NOTICE,	"notice",	LOG_NOTICE },
	{ LG_WARNING,	"warning",	LOG_WARNING },
	{ LG_ERROR,	"error",	LOG_ERR },
	{ LG_FATAL,	"fatal",	LOG_CRIT },

	{ LG_NONE,	"user",		LOG_USER },
	{ LG_NONE,	"daemon",	LOG_DAEMON },
	{ LG_NONE,	"local0",	LOG_LOCAL0 },
	{ LG_NONE,	"local1",	LOG_LOCAL1 },
	{ LG_NONE,	"local2",	LOG_LOCAL2 },
	{ LG_NONE,	"local3",	LOG_LOCAL3 },
	{ LG_NONE,	"local4",	LOG_LOCAL4 },
	{ LG_NONE,	"local5",	LOG_LOCAL5 },
	{ LG_NONE,	"local6",	LOG_LOCAL6 },
	{ LG_NONE,	"local7",	LOG_LOCAL7 },
	{ LG_NONE,	NULL,		-1 }
};

# define	MAXFNAME	(1023)
/*****************************************************************
**	function definitions (for function declarations see log.h)
*****************************************************************/

/*****************************************************************
**	lg_fileopen (path, name) -- open the log file
**	Name is a (absolute or relative) file or directory name.
**	If path is given and name is a relative path name then path
**	is prepended to name.
**	returns the open file pointer or NULL on error
*****************************************************************/
static	FILE	*lg_fileopen (const char *path, const char *name)
{
	int	len;
	FILE	*fp;
	struct	tm	*t;
	time_t	sec;
	char	fname[MAXFNAME+1];

	if ( name == NULL || *name == '\0' )
		return NULL;
	else if ( *name == '/' || path == NULL )
		snprintf (fname, MAXFNAME, "%s", name);
	else
		snprintf (fname, MAXFNAME, "%s/%s", path, name);

# ifdef LOG_TEST
	fprintf (stderr, "\t ==> \"%s\"", fname);
# endif
	if ( is_directory (fname) )
	{
		len = strlen (fname);

		time (&sec);
		t = gmtime (&sec);
		snprintf (fname+len, MAXFNAME-len, LOG_FNAMETMPL,
			t->tm_year + 1900, t->tm_mon+1, t->tm_mday,
			t->tm_hour, t->tm_min, t->tm_sec);
# ifdef LOG_TEST
	fprintf (stderr, " isdir \"%s\"", fname);
# endif
	}

# ifdef LOG_TEST
	fprintf (stderr, "\n");
# endif

	if ( (fp = fopen (fname, "a")) == NULL )
		return NULL;

	return fp;
}

/*****************************************************************
**	lg_str2lvl (level_name)
*****************************************************************/
lg_lvl_t	lg_str2lvl (const char *name)
{
	lg_symtbl_t	*p;

	if ( !name )
		return LG_NONE;

	for ( p = symtbl; p->str; p++ )
		if ( strcasecmp (name, p->str) == 0 )
			return p->level;

	return LG_NONE;
}

/*****************************************************************
**	lg_lvl2syslog (level)
*****************************************************************/
lg_lvl_t	lg_lvl2syslog (lg_lvl_t level)
{
	lg_symtbl_t	*p;

	for ( p = symtbl; p->str; p++ )
		if ( level == p->level )
			return p->syslog_level;

	assert ( p->str != NULL );	/* we assume not to reach this! */

	return LOG_DEBUG;	/* if not found, return DEBUG as default */
}

/*****************************************************************
**	lg_str2syslog (facility_name)
*****************************************************************/
int	lg_str2syslog (const char *facility)
{
	lg_symtbl_t	*p;

	dbg_val1 ("lg_str2syslog (%s)\n", facility);
	if ( !facility )
		return LG_NONE;

	for ( p = symtbl; p->str; p++ )
		if ( strcasecmp (facility, p->str) == 0 )
			return p->syslog_level;

	return LG_NONE;
}

/*****************************************************************
**	lg_lvl2str (level)
*****************************************************************/
const	char	*lg_lvl2str (lg_lvl_t level)
{
	lg_symtbl_t	*p;

	if ( level < LG_DEBUG )
		return "none";

	for ( p = symtbl; p->str; p++ )
		if ( level == p->level )
			return p->str;
	return "fatal";
}

/*****************************************************************
**	lg_geterrcnt () -- returns the current value of the internal
**	error counter
*****************************************************************/
long	lg_geterrcnt ()
{
	return lg_errcnt;
}

/*****************************************************************
**	lg_seterrcnt () -- sets the internal error counter
**	returns the current value 
*****************************************************************/
long	lg_seterrcnt (long value)
{
	return lg_errcnt = value;
}

/*****************************************************************
**	lg_reseterrcnt () -- resets the internal error counter to 0
**	returns the current value 
*****************************************************************/
long	lg_reseterrcnt ()
{
	return lg_seterrcnt (0L);
}


/*****************************************************************
**	lg_open (prog, facility, syslevel, path, file, filelevel)
**		-- open the log channel
**	return values:
**		 0 on success
**		 -1 on file open error
*****************************************************************/
int	lg_open (const char *progname, const char *facility, const char *syslevel, const char *path, const char *file, const char *filelevel)
{
	int	sysfacility;

	dbg_val6 ("lg_open (%s, %s, %s, %s, %s, %s)\n", progname, facility, syslevel, path, file, filelevel);

	lg_minsyslevel = lg_str2lvl (syslevel);
	lg_minfilelevel = lg_str2lvl (filelevel);

	sysfacility = lg_str2syslog (facility);
	if ( sysfacility >= 0 )
	{
		lg_syslogging = 1;
		dbg_val2 ("lg_open: openlog (%s, LOG_NDELAY, %d)\n", progname, lg_str2syslog (facility));
		openlog (progname, LOG_NDELAY, lg_str2syslog (facility));
	}
	if ( file && * file )
	{
		if ( (lg_fp = lg_fileopen (path, file)) == NULL )
			return -1;
		lg_progname = progname;
	}
	
	return 0;
}

/*****************************************************************
**	lg_close () -- close the open filepointer for error logging
**	return 0 if no error log file is currently open,
**	otherwise the return code of fclose is returned.
*****************************************************************/
int	lg_close ()
{
	int	ret = 0;

	if ( lg_syslogging )
	{
		closelog ();
		lg_syslogging = 0;
	}
	if ( lg_fp )
	{
		ret = fclose (lg_fp);
		lg_fp = NULL;
	}

	return ret;
}

/*****************************************************************
**	lg_zone_start (domain)
**		-- reopen the log channel
**	return values:
**		 0 on success
**		 -1 on file open error
*****************************************************************/
int	lg_zone_start (const char *dir, const char *domain)
{
	char	fname[255+1];

	dbg_val2 ("lg_zone_start (%s, %s)\n", dir, domain);

	snprintf (fname, sizeof (fname), LOG_DOMAINTMPL, domain);
	if ( lg_fp )
		lg_fpsave = lg_fp;
	lg_fp = lg_fileopen (dir, fname);

	return lg_fp != NULL;
}

/*****************************************************************
**	lg_zone_end (domain)
**		-- close the (reopened) log channel
**	return values:
**		 0 on success
**		 -1 on file open error
*****************************************************************/
int	lg_zone_end ()
{
	if ( lg_fp && lg_fpsave )
	{
		lg_close ();
		lg_fp = lg_fpsave;
		lg_fpsave = NULL;
		return 1;
	}

	return 0;
}

/*****************************************************************
**
**	lg_args (level, argc, argv[])
**	log all command line arguments (up to a length of 511 chars)
**	with priority level 
**
*****************************************************************/
void	lg_args (lg_lvl_t level, int argc, char * const argv[])
{
	char	cmdline[511+1];
	int	len;
	int	i;

	len = 0;
	for ( i = 0; i < argc && len < sizeof (cmdline); i++ )
		len += snprintf (cmdline+len, sizeof (cmdline) - len, " %s", argv[i]);

#if 1
	lg_mesg (level, "------------------------------------------------------------");
#else
	lg_mesg (level, "");
#endif
	lg_mesg (level, "running%s ", cmdline);
}

/*****************************************************************
**
**	lg_mesg (level, fmt, ...)
**
**	Write a given message to the error log file and counts
**	all messages written with an level greater than LOG_ERR.
**
**	All messages will be on one line in the logfile, so it's
**	not necessary to add an '\n' to the message.
**
**	To call this function before an elog_open() is called is
**	useless!
**
*****************************************************************/
void	lg_mesg (int priority, char *fmt, ...)
{
	va_list ap;
	struct	timeval	tv;
	struct	tm	*t;
	char	format[256];

	assert (fmt != NULL);
	assert (priority >= LG_DEBUG && priority <= LG_FATAL);

	format[0] ='\0';

	dbg_val3 ("syslog = %d prio = %d >= sysmin = %d\n", lg_syslogging, priority, lg_minsyslevel);
	if ( lg_syslogging && priority >= lg_minsyslevel )
	{
#if defined (LOG_WITH_LEVEL) && LOG_WITH_LEVEL
		snprintf (format, sizeof (format), "%s: %s", lg_lvl2str(priority), fmt);
		fmt = format;
#endif
		va_start(ap, fmt);
		vsyslog (lg_lvl2syslog (priority), fmt, ap);
		va_end(ap);
	}

	dbg_val3 ("filelg = %d prio = %d >= filmin = %d\n", lg_fp!=NULL, priority, lg_minfilelevel);
	if ( lg_fp && priority >= lg_minfilelevel )
	{
#if defined (LOG_WITH_TIMESTAMP) && LOG_WITH_TIMESTAMP
		gettimeofday (&tv, NULL);
		t = localtime ((time_t *) &tv.tv_sec);
		fprintf (lg_fp, "%04d-%02d-%02d ",
			t->tm_year+1900, t->tm_mon+1, t->tm_mday);
		fprintf (lg_fp, "%02d:%02d:%02d.%03ld: ",
			t->tm_hour, t->tm_min, t->tm_sec, tv.tv_usec / 1000);
#endif
#if defined (LOG_WITH_PROGNAME) && LOG_WITH_PROGNAME
		if ( lg_progname )
			fprintf (lg_fp, "%s: ", lg_progname);
#endif
#if defined (LOG_WITH_LEVEL) && LOG_WITH_LEVEL
		if ( fmt != format )	/* level is not in fmt string */
			fprintf (lg_fp, "%s: ", lg_lvl2str(priority));
#endif
		va_start(ap, fmt);
		vfprintf (lg_fp, fmt, ap);
		va_end(ap);
		fprintf (lg_fp, "\n");
	}

	if ( priority >= LG_ERROR )
		lg_errcnt++;
}


#ifdef LOG_TEST
const char *progname;
int	main (int argc, char *argv[])
{
	const	char	*levelstr;
	const	char	*newlevelstr;
	int	level;
	int	err;

	progname = *argv;

	if ( --argc )
		levelstr = *++argv;
	else
		levelstr = "fatal";

	level = lg_str2lvl (levelstr);
	newlevelstr = lg_lvl2str (level+1);
	dbg_val4 ("base level = %s(%d) newlevel = %s(%d)\n", levelstr, level, newlevelstr, level+1);
	if ( (err = lg_open (progname,
#if 1
				"user",
#else
				"none",
#endif
				levelstr, ".",
#if 1
				"test.log",
#else
				NULL,
#endif
				newlevelstr)) )
		fprintf (stderr, "\topen error %d\n", err);
	else
	{
		lg_mesg (LG_DEBUG, "debug message");
		lg_mesg (LG_INFO, "INFO message");
		lg_mesg (LG_NOTICE, "Notice message");
		lg_mesg (LG_WARNING, "Warning message");
		lg_mesg (LG_ERROR, "Error message");
		lg_mesg (LG_FATAL, "Fatal message ");
	}

	if ( (err = lg_close ()) < 0 )
		fprintf (stderr, "\tclose error %d\n", err);

	return 0;
}
#endif
