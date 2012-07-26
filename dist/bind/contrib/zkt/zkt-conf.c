/*****************************************************************
**
**	@(#) zkt-conf.c (c) Jan 2005 / Jan 2010  Holger Zuleger  hznet.de
**
**	A config file utility for the DNSSEC Zone Key Tool
**
**	Copyright (c) 2005 - 2008, Holger Zuleger HZnet. All rights reserved.
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
# include <stdlib.h>	/* abort(), exit(), ... */
# include <string.h>
# include <dirent.h>
# include <assert.h>
# include <unistd.h>
# include <ctype.h>
# include <time.h>

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
# include "config_zkt.h"
#if defined(HAVE_GETOPT_LONG) && HAVE_GETOPT_LONG
# include <getopt.h>
#endif

# include "debug.h"
# include "misc.h"
# include "zfparse.h"
# include "zconf.h"

extern  int	optopt;
extern  int	opterr;
extern  int	optind;
extern  char	*optarg;
const	char	*progname;

static	const	char	*view = "";
static	int	writeflag = 0;
static	int	allflag = 0;
static	int	testflag = 0;

# define	short_options	":aC:c:O:dlstvwV:rh"
#if defined(HAVE_GETOPT_LONG) && HAVE_GETOPT_LONG
static struct option long_options[] = {
	{"compability",		required_argument, NULL, 'C'},
	{"config",		required_argument, NULL, 'c'},
	{"option",		required_argument, NULL, 'O'},
	{"config-option",	required_argument, NULL, 'O'},
	{"default",		no_argument, NULL, 'd'},
	{"sidecfg",		no_argument, NULL, 's'},
	{"localcfg",		no_argument, NULL, 'l'},
	{"all-values",		no_argument, NULL, 'a'},
	{"test",		no_argument, NULL, 't'},
	{"overwrite",		no_argument, NULL, 'w'},
	{"version",		no_argument, NULL, 'v' },
	{"write",		no_argument, NULL, 'w'},
	{"view",		required_argument, NULL, 'V' },
	{"help",		no_argument, NULL, 'h'},
	{0, 0, 0, 0}
};
#endif

static	void    usage (char *mesg);


int	main (int argc, char *argv[])
{
	int	c;
	int	opt_index;
	int	action;
	int	major;
	int	minor;
	const	char	*file;
	const	char	*defconfname = NULL;
	const	char	*confname = NULL;
	char	*p;
	char	str[254+1];
	zconf_t	*refconfig = NULL;
	zconf_t	*config;

	progname = *argv;
	if ( (p = strrchr (progname, '/')) )
		progname = ++p;
	view = getnameappendix (progname, "zkt-conf");

	defconfname = getdefconfname (view);
	dbg_val0 ("Load built in config \"%s\"\n");
	config = loadconfig ("", (zconf_t *)NULL);	/* load built in config */

	if ( fileexist (defconfname) )			/* load default config file */
	{
		dbg_val ("Load site wide config file \"%s\"\n", defconfname);
		config = loadconfig (defconfname, config);
	}
	if ( config == NULL )
		fatal ("Out of memory\n");
	confname = defconfname;

        opterr = 0;
	opt_index = 0;
	action = 0;
	setconfigversion (100);
#if defined(HAVE_GETOPT_LONG) && HAVE_GETOPT_LONG
	while ( (c = getopt_long (argc, argv, short_options, long_options, &opt_index)) != -1 )
#else
	while ( (c = getopt (argc, argv, short_options)) != -1 )
#endif
	{
		switch ( c )
		{
		case 'V':		/* view name */
			view = optarg;
			defconfname = getdefconfname (view);
			if ( fileexist (defconfname) )		/* load default config file */
				config = loadconfig (defconfname, config);
			if ( config == NULL )
				fatal ("Out of memory\n");
			confname = defconfname;
			break;
		case 'O':		/* read option from commandline */
			config = loadconfig_fromstr (optarg, config);
			break;
		case 'C':
			switch ( sscanf (optarg, "%d.%d", &major, &minor) )
			{
			case 2:	major = major * 100 + minor;
			case 1: break;
			default:
				usage ("illegal release number");
			}
			setconfigversion (major);
			break;
		case 'c':
			if ( *optarg == '\0' )
				usage ("empty config file name");
			config = loadconfig (optarg, config);
			if ( *optarg == '-' || strcmp (optarg, "stdin") == 0 )
				confname = "stdout";
			else
				confname = optarg;
			break;
		case 'd':		/* built-in default config */
			config = loadconfig ("", config);	/* load built-in config */
			confname = defconfname;
			break;
		case 's':		/* side wide config */
			/* this is the default **/
			break;	
		case 'a':		/* set all flag */
			allflag = 1;
			break;
		case 'l':		/* local config file */
			refconfig = dupconfig (config);	/* duplicate current config */
			confname = LOCALCONF_FILE;
			if ( fileexist (LOCALCONF_FILE) )	/* try to load local config file */
			{
				dbg_val ("Load local config file \"%s\"\n", LOCALCONF_FILE);
				config = loadconfig (LOCALCONF_FILE, config);
			}
			else if ( !writeflag )
				usage ("error: no local config file found");
			break;
		case 't':		/* test config */
			testflag = 1;
			break;
		case 'v':		/* version */
			fprintf (stderr, "%s version %s compiled for BIND version %d\n",
							progname, ZKT_VERSION, BIND_VERSION);
			fprintf (stderr, "ZKT %s\n", ZKT_COPYRIGHT);
			return 0;
			break;
		case 'w':		/* write back conf file */
			writeflag = 1;
			break;
		case 'h':		/* print help */
			usage ("");
			break;
		case ':':
			snprintf (str, sizeof(str), "option \"-%c\" requires an argument.",
										optopt);
			usage (str);
			break;
		case '?':
			if ( isprint (optopt) )
				snprintf (str, sizeof(str), "Unknown option \"-%c\".",
										optopt);
			else
				snprintf (str, sizeof (str), "Unknown option char \\x%x.",
										optopt);
			usage (str);
			break;
		default:
			abort();
		}
	}

	c = optind;
	if ( c >= argc )	/* no arguments given on commandline */
	{
		if ( testflag )
		{
			if ( checkconfig (config) )
				fprintf (stderr, "All config file parameter seems to be ok\n");
		}
		else
		{
			if ( !writeflag )	/* print to stdout */
				confname = "stdout";

			if ( refconfig )	/* have we seen a local config file ? */
				if ( allflag )
					printconfig (confname, config);	
				else
					printconfigdiff (confname, refconfig, config);	
			else
				printconfig (confname, config);
		}
	}
	else	/* command line argument found: use it as name of zone file */
	{
		long	minttl;
		long	maxttl;
		int	keydbfound;
		char	*dnskeydb;

		file = argv[c++];

		dnskeydb = config->keyfile;

		minttl = 0x7FFFFFFF;
		maxttl = 0;
		keydbfound = parsezonefile (file, &minttl, &maxttl, dnskeydb);
		if ( keydbfound < 0 )
			error ("can't parse zone file %s\n", file);

		if ( dnskeydb && !keydbfound )
		{
			if ( writeflag )
			{
				addkeydb (file, dnskeydb);
				printf ("\"$INCLUDE %s\" directive added to \"%s\"\n", dnskeydb, file);
			}
			else
				printf ("\"$INCLUDE %s\" should be added to \"%s\" (run with option -w)\n",
							dnskeydb, file);
		}

		if ( minttl < (10 * MINSEC) )
			fprintf (stderr, "Min_TTL of %s (%ld seconds) is too low to use it in a signed zone (see RFC4641)\n", 
							timeint2str (minttl), minttl);
		else
			fprintf (stderr, "Min_TTL:\t%s\t# (%ld seconds)\n", timeint2str (minttl), minttl);
		fprintf (stdout, "Max_TTL:\t%s\t# (%ld seconds)\n", timeint2str (maxttl), maxttl);

		if ( writeflag )
		{
			refconfig = dupconfig (config);	/* duplicate current config */
			confname = LOCALCONF_FILE;
			if ( fileexist (LOCALCONF_FILE) )	/* try to load local config file */
			{
				dbg_val ("Load local config file \"%s\"\n", LOCALCONF_FILE);
				config = loadconfig (LOCALCONF_FILE, config);
			}
			setconfigpar (config, "Max_TTL", &maxttl);
			printconfigdiff (confname, refconfig, config);
		}
	}


	return 0;
}

# define	sopt_usage(mesg, value)	fprintf (stderr, mesg, value)
#if defined(HAVE_GETOPT_LONG) && HAVE_GETOPT_LONG
# define	lopt_usage(mesg, value)	fprintf (stderr, mesg, value)
# define	loptstr(lstr, sstr)	lstr
#else
# define	lopt_usage(mesg, value)
# define	loptstr(lstr, sstr)	sstr
#endif
static	void    usage (char *mesg)
{
	fprintf (stderr, "%s version %s\n", progname, ZKT_VERSION);
        if ( mesg && *mesg )
                fprintf (stderr, "%s\n", mesg);
        fprintf (stderr, "\n");
        fprintf (stderr, "usage: %s -h\n", progname);
        fprintf (stderr, "usage: %s [-V view] [-w|-t]      -d  [-O <optstr>]\n", progname);
        fprintf (stderr, "usage: %s [-V view] [-w|-t]     [-s] [-c config] [-O <optstr>]\n", progname);
        fprintf (stderr, "usage: %s [-V view] [-w|-t] [-a] -l  [-c config] [-O <optstr>]\n", progname);
        fprintf (stderr, "\n");
        fprintf (stderr, "usage: %s [-c config] [-w] <zonefile>\n", progname);
        fprintf (stderr, "\n");
	fprintf (stderr, " -V name%s", loptstr (", --view=name\n", ""));
	fprintf (stderr, "\t\t specify the view name \n");
        fprintf (stderr, " -d%s\tprint built-in default config parameter\n", loptstr (", --default", ""));
        fprintf (stderr, " -s%s\tprint site wide config file parameter (this is the default)\n", loptstr (", --sitecfg", ""));
        fprintf (stderr, " -l%s\tprint local config file parameter\n", loptstr (", --localcfg", ""));
        fprintf (stderr, " -a%s\tprint all parameter not only the different one\n", loptstr (", --all", ""));
        fprintf (stderr, " -c file%s", loptstr (", --config=file\n", ""));
	fprintf (stderr, " \t\tread config from <file> instead of %s\n", CONFIG_FILE);
        fprintf (stderr, " -O optstr%s", loptstr (", --config-option=\"optstr\"\n", ""));
	fprintf (stderr, " \t\tread config options from commandline\n");
        fprintf (stderr, " -t%s\ttest the config parameter if they are useful \n", loptstr (", --test", "\t"));
        fprintf (stderr, " -w%s\twrite or rewrite config file \n", loptstr (", --write", "\t"));
        fprintf (stderr, " -h%s\tprint this help \n", loptstr (", --help", "\t"));
        exit (1);
}

