/*****************************************************************
**
**	@(#) zkt-ls.c (c) Jan 2010  Holger Zuleger  hznet.de
**
**	Secure DNS zone key tool
**	A command to list dnssec keys
**
**	Copyright (c) 2005 - 2010, Holger Zuleger HZnet. All rights reserved.
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
# include "config_zkt.h"
#if defined(HAVE_GETOPT_LONG) && HAVE_GETOPT_LONG
# include <getopt.h>
#endif

# include "debug.h"
# include "misc.h"
# include "strlist.h"
# include "zconf.h"
# include "dki.h"
# include "tcap.h"
# include "zkt.h"

extern  int	optopt;
extern  int	opterr;
extern  int	optind;
extern  char	*optarg;
const	char	*progname;

char	*labellist = NULL;

int	headerflag = 1;
int	ageflag = 0;
int	lifetime = 0;
int	lifetimeflag = 0;
int	timeflag = 1;
int	exptimeflag = 0;
int	pathflag = 0;
int	kskflag = 1;
int	zskflag = 1;
int	ljustflag = 0;
int	subdomain_before_parent = 1;

static	int	dirflag = 0;
static	int	recflag = RECURSIVE;
static	int	trustedkeyflag = 0;
static	const	char	*view = "";
static	const	char	*term = NULL;

#if defined(COLOR_MODE) && COLOR_MODE
# define	short_options	":HKTV:afC::c:O:dhkLl:prstez"
#else
# define	short_options	":HKTV:af:c:O:dhkLl:prstez"
#endif
#if defined(HAVE_GETOPT_LONG) && HAVE_GETOPT_LONG
static struct option long_options[] = {
	{"list-dnskeys",	no_argument, NULL, 'K'},
	{"list-trustedkeys",	no_argument, NULL, 'T'},
	{"ksk",			no_argument, NULL, 'k'},
	{"zsk",			no_argument, NULL, 'z'},
	{"age",			no_argument, NULL, 'a'},
	{"lifetime",		no_argument, NULL, 'f'},
	{"time",		no_argument, NULL, 't'},
	{"expire",		no_argument, NULL, 'e'},
	{"recursive",		no_argument, NULL, 'r'},
	{"leftjust",		no_argument, NULL, 'L'},
	{"label-list",		no_argument, NULL, 'l'},
	{"path",		no_argument, NULL, 'p'},
	{"sort",		no_argument, NULL, 's'},
	{"subdomain",		no_argument, NULL, 's'},
	{"nohead",		no_argument, NULL, 'h'},
	{"directory",		no_argument, NULL, 'd'},
#if defined(COLOR_MODE) && COLOR_MODE
	{"color",		optional_argument, NULL, 'C'},
#endif
	{"config",		required_argument, NULL, 'c'},
	{"option",		required_argument, NULL, 'O'},
	{"config-option",	required_argument, NULL, 'O'},
	{"view",		required_argument, NULL, 'V' },
	{"help",		no_argument, NULL, 'H'},
	{0, 0, 0, 0}
};
#endif

static	int	parsedirectory (const char *dir, dki_t **listp, int sub_before);
static	void	parsefile (const char *file, dki_t **listp, int sub_before);
static	void    usage (char *mesg, zconf_t *cp);

static	void	setglobalflags (zconf_t *config)
{
	recflag = config->recursive;
	ageflag = config->printage;
	timeflag = config->printtime;
	ljustflag = config->ljust;
	term = config->colorterm;
	if ( term && *term == '\0' )
		term = getenv ("TERM");
}

int	main (int argc, char *argv[])
{
	dki_t	*data = NULL;
	int	c;
	int	opt_index;
	int	action;
	const	char	*file;
	const	char	*defconfname = NULL;
	char	*p;
	char	str[254+1];
	zconf_t	*config;

	progname = *argv;
	if ( (p = strrchr (progname, '/')) )
		progname = ++p;
	view = getnameappendix (progname, "zkt-ls");

	defconfname = getdefconfname (view);
	config = loadconfig ("", (zconf_t *)NULL);	/* load built in config */
	if ( fileexist (defconfname) )			/* load default config file */
		config = loadconfig (defconfname, config);
	if ( config == NULL )
		fatal ("Out of memory\n");
	setglobalflags (config);

        opterr = 0;
	opt_index = 0;
	action = 0;
#if defined(HAVE_GETOPT_LONG) && HAVE_GETOPT_LONG
	while ( (c = getopt_long (argc, argv, short_options, long_options, &opt_index)) != -1 )
#else
	while ( (c = getopt (argc, argv, short_options)) != -1 )
#endif
	{
		switch ( c )
		{
#if defined(COLOR_MODE) && COLOR_MODE
		case 'C':	/* color mode on; optional with terminal name */
			if ( optarg )
				term = optarg;
			else
				term = getenv ("TERM");
			break;
#endif
		case 'T':
			trustedkeyflag = 1;
			subdomain_before_parent = 0;
			zskflag = pathflag = 0;
			/* fall through */
		case 'H':
		case 'K':
		case 'Z':
			action = c;
			break;
		case 'a':		/* age */
			ageflag = !ageflag;
			break;
		case 'f':		/* key lifetime */
			lifetimeflag = !lifetimeflag;
			break;
		case 'V':		/* view name */
			view = optarg;
			defconfname = getdefconfname (view);
			if ( fileexist (defconfname) )		/* load default config file */
				config = loadconfig (defconfname, config);
			if ( config == NULL )
				fatal ("Out of memory\n");
			setglobalflags (config);
			break;
		case 'c':
			config = loadconfig (optarg, config);
			setglobalflags (config);
			checkconfig (config);
			break;
		case 'O':		/* read option from commandline */
			config = loadconfig_fromstr (optarg, config);
			setglobalflags (config);
			checkconfig (config);
			break;
		case 'd':		/* ignore directory arg */
			dirflag = 1;
			break;
		case 'h':		/* print no headline */
			headerflag = 0;
			break;
		case 'k':		/* ksk only */
			zskflag = 0;
			break;
		case 'L':		/* ljust */
			ljustflag = !ljustflag;
			break;
		case 'l':		/* label list */
			labellist = prepstrlist (optarg, LISTDELIM);
			if ( labellist == NULL )
				fatal ("Out of memory\n");
			break;
		case 'p':		/* print path */
			pathflag = 1;
			break;
		case 'r':		/* switch recursive flag */
			recflag = !recflag;
			break;
		case 's':		/* switch subdomain sorting flag */
			subdomain_before_parent = !subdomain_before_parent;
			break;
		case 't':		/* time */
			timeflag = !timeflag;
			break;
		case 'e':		/* expire time */
			exptimeflag = !exptimeflag;
			break;
		case 'z':		/* zsk only */
			kskflag = 0;
			break;
		case ':':
			snprintf (str, sizeof(str), "option \"-%c\" requires an argument.\n",
										optopt);
			usage (str, config);
			break;
		case '?':
			if ( isprint (optopt) )
				snprintf (str, sizeof(str), "Unknown option \"-%c\".\n",
										optopt);
			else
				snprintf (str, sizeof (str), "Unknown option char \\x%x.\n",
										optopt);
			usage (str, config);
			break;
		default:
			abort();
		}
	}

	if ( kskflag == 0 && zskflag == 0 )
		kskflag = zskflag = 1;

	tc_init (stdout, term);

	c = optind;
	do {
		if ( c >= argc )		/* no args left */
			file = config->zonedir;	/* use default directory */
		else
			file = argv[c++];

		if ( is_directory (file) )
			parsedirectory (file, &data, subdomain_before_parent);
		else
			parsefile (file, &data, subdomain_before_parent);

	}  while ( c < argc );	/* for all arguments */

	switch ( action )
	{
	case 'H':
		usage ("", config);
	case 'K':
		zkt_list_dnskeys (data);
		break;
	case 'T':
		zkt_list_trustedkeys (data);
		break;
	default:
		zkt_list_keys (data);
	}

	tc_end (stdout, term);

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
static	void    usage (char *mesg, zconf_t *cp)
{
        fprintf (stderr, "Secure DNS Zone Key Tool %s\n", ZKT_VERSION);
        fprintf (stderr, "\n");

        fprintf (stderr, "List keys in current or given directory (-r for recursive mode)\n");
        sopt_usage ("\tusage: %s [-adefhkLprtzC] [-c config] [file|dir ...]\n", progname);
        fprintf (stderr, "\n");
        fprintf (stderr, "List public part of keys in DNSKEY RR format\n");
        sopt_usage ("\tusage: %s -K [-dhkrz] [-c config] [file|dir ...]\n", progname);
        lopt_usage ("\tusage: %s --list-dnskeys [-dhkzr] [-c config] [file|dir ...]\n", progname);
        fprintf (stderr, "\n");
        fprintf (stderr, "List keys (output is suitable for trusted-keys section)\n");
        sopt_usage ("\tusage: %s -T [-dhrz] [-c config] [file|dir ...]\n", progname);
        lopt_usage ("\tusage: %s --list-trustedkeys [-dhzr] [-c config] [file|dir ...]\n", progname);
        fprintf (stderr, "\n");

        fprintf (stderr, "General options \n");
        fprintf (stderr, "\t-c file%s", loptstr (", --config=file\n", ""));
	fprintf (stderr, "\t\t read config from <file> instead of %s\n", CONFIG_FILE);
        fprintf (stderr, "\t-O optstr%s", loptstr (", --config-option=\"optstr\"\n", ""));
	fprintf (stderr, "\t\t read config options from commandline\n");
        fprintf (stderr, "\t-h%s\t no headline or trusted-key section header/trailer in -T mode\n", loptstr (", --nohead", "\t"));
        fprintf (stderr, "\t-d%s\t skip directory arguments\n", loptstr (", --directory", "\t"));
        fprintf (stderr, "\t-L%s\t print the domain name left justified (default: %s)\n", loptstr (", --leftjust", "\t"), ljustflag ? "on": "off");
        fprintf (stderr, "\t-l list%s", loptstr (", --label=\"list\"\n\t", ""));
        fprintf (stderr, "\t\t print out only zone keys from the given domain list\n");
        fprintf (stderr, "\t-C[term]%s", loptstr (", --color[=\"term\"]\n\t", ""));
        fprintf (stderr, "\t\t turn color mode on \n");
        fprintf (stderr, "\t-p%s\t show path of keyfile / create key in current directory\n", loptstr (", --path", "\t"));
        fprintf (stderr, "\t-r%s\t recursive mode on/off (default: %s)\n", loptstr(", --recursive", "\t"), recflag ? "on": "off");
        fprintf (stderr, "\t-s%s\t change sorting of subdomains\n", loptstr(", --subdomain", "\t"));
        fprintf (stderr, "\t-a%s\t print age of key (default: %s)\n", loptstr (", --age", "\t"), ageflag ? "on": "off");
        fprintf (stderr, "\t-t%s\t print key generation time (default: %s)\n", loptstr (", --time", "\t"),
								timeflag ? "on": "off");
        fprintf (stderr, "\t-e%s\t print key expiration time\n", loptstr (", --expire", "\t"));
        fprintf (stderr, "\t-f%s\t print key lifetime\n", loptstr (", --lifetime", "\t"));
        fprintf (stderr, "\t-k%s\t key signing keys only\n", loptstr (", --ksk", "\t"));
        fprintf (stderr, "\t-z%s\t zone signing keys only\n", loptstr (", --zsk", "\t"));
        if ( mesg && *mesg )
                fprintf (stderr, "%s\n", mesg);
        exit (1);
}

static	int	parsedirectory (const char *dir, dki_t **listp, int sub_before)
{
	dki_t	*dkp;
	DIR	*dirp;
	struct  dirent  *dentp;
	char	path[MAX_PATHSIZE+1];

	if ( dirflag )
		return 0;

	dbg_val ("directory: opendir(%s)\n", dir);
	if ( (dirp = opendir (dir)) == NULL )
		return 0;

	while ( (dentp = readdir (dirp)) != NULL )
	{
		if ( is_dotfilename (dentp->d_name) )
			continue;

		dbg_val ("directory: check %s\n", dentp->d_name);
		pathname (path, sizeof (path), dir, dentp->d_name, NULL);
		if ( is_directory (path) && recflag )
		{
			dbg_val ("directory: recursive %s\n", path);
			parsedirectory (path, listp, sub_before);
		}
		else if ( is_keyfilename (dentp->d_name) )
			if ( (dkp = dki_read (dir, dentp->d_name)) )
			{
				// fprintf (stderr, "parsedir: tssearch (%d %s)\n", dkp, dkp->name);
#if defined (USE_TREE) && USE_TREE
				dki_tadd (listp, dkp, sub_before);
#else
				dki_add (listp, dkp);
#endif
			}
	}
	closedir (dirp);
	return 1;
}

static	void	parsefile (const char *file, dki_t **listp, int sub_before)
{
	char	path[MAX_PATHSIZE+1];
	dki_t	*dkp;

	/* file arg contains path ? ... */
	file = splitpath (path, sizeof (path), file);	/* ... then split of */

	if ( is_keyfilename (file) )	/* plain file name looks like DNS key file ? */
	{
		if ( (dkp = dki_read (path, file)) )	/* read DNS key file ... */
#if defined (USE_TREE) && USE_TREE
			dki_tadd (listp, dkp, sub_before);		/* ... and add to tree */
#else
			dki_add (listp, dkp);		/* ... and add to list */
#endif
		else
			error ("error parsing %s: (%s)\n", file, dki_geterrstr());
	}
}
