/*****************************************************************
**
**	@(#) zkt-signer.c  (c) Jan 2005 - Jan 2010  Holger Zuleger hznet.de
**
**	A wrapper around the BIND dnssec-signzone command which is able
**	to resign a zone if necessary and doing a zone or key signing key rollover.
**
**	Copyright (c) 2005 - 2010, Holger Zuleger HZnet. All rights reserved.
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
# include <assert.h>
# include <dirent.h>
# include <errno.h>	
# include <unistd.h>	
# include <ctype.h>	

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
# include "config_zkt.h"
#if defined(HAVE_GETOPT_LONG) && HAVE_GETOPT_LONG
# include <getopt.h>
#endif
# include "zconf.h"
# include "debug.h"
# include "misc.h"
# include "ncparse.h"
# include "nscomm.h"
# include "soaserial.h"
# include "zone.h"
# include "dki.h"
# include "rollover.h"
# include "log.h"

#if defined(BIND_VERSION) && BIND_VERSION >= 940
# define	short_options	"c:L:V:D:N:o:O:dfHhnrv"
#else
# define	short_options	"c:L:V:D:N:o:O:fHhnrv"
#endif
#if defined(HAVE_GETOPT_LONG) && HAVE_GETOPT_LONG
static struct option long_options[] = {
	{"reload",		no_argument, NULL, 'r'},
	{"force",		no_argument, NULL, 'f'},
	{"noexec",		no_argument, NULL, 'n'},
	{"verbose",		no_argument, NULL, 'v'},
	{"directory",		no_argument, NULL, 'd'},
	{"config",		required_argument, NULL, 'c'},
	{"option",		required_argument, NULL, 'O'},
	{"config-option",	required_argument, NULL, 'O'},
	{"logfile",		required_argument, NULL, 'L' },
	{"view",		required_argument, NULL, 'V' },
	{"directory",		required_argument, NULL, 'D'},
	{"named-conf",		required_argument, NULL, 'N'},
	{"origin",		required_argument, NULL, 'o'},
#if defined(BIND_VERSION) && BIND_VERSION >= 940
	{"dynamic",		no_argument, NULL, 'd' },
#endif
	{"help",		no_argument, NULL, 'h'},
	{0, 0, 0, 0}
};
#endif


/**	function declaration	**/
static	void	usage (char *mesg, zconf_t *conf);
static	int	add2zonelist (const char *dir, const char *view, const char *zone, const char *file);
static	int	parsedir (const char *dir, zone_t **zp, const zconf_t *conf);
static	int	dosigning (zone_t *zonelist, zone_t *zp);
static	int	check_keydb_timestamp (dki_t *keylist, time_t reftime);
static	int	new_keysetfiles (const char *dir, time_t zone_signing_time);
static	int	writekeyfile (const char *fname, const dki_t *list, int key_ttl);
static	int	sign_zone (const zone_t *zp);
static	void	register_key (dki_t *listp, const zconf_t *z);
static	void	copy_keyset (const char *dir, const char *domain, const zconf_t *conf);

/**	global command line options	**/
extern  int	optopt;
extern  int	opterr;
extern  int	optind;
extern  char	*optarg;
const	char	*progname;
static	const	char	*viewname = NULL;
static	const	char	*logfile = NULL;
static	const	char	*origin = NULL;
static	const	char	*namedconf = NULL;
static	const	char	*dirname = NULL;
static	int	verbose = 0;
static	int	force = 0;
static	int	reloadflag = 0;
static	int	noexec = 0;
static	int	dynamic_zone = 0;	/* dynamic zone ? */
static	zone_t	*zonelist = NULL;	/* must be static global because add2zonelist use it */
static	zconf_t	*config;

/**	macros **/
#define	set_bind94_dynzone(dz)	((dz) = 1)
#define	set_bind96_dynzone(dz)	((dz) = 6)
#define	bind94_dynzone(dz)	( (dz) > 0 && (dz) < 6 )
#define	bind96_dynzone(dz)	( (dz) >= 6 )
#define	is_defined(str)		( (str) && *(str) )

int	main (int argc, char *const argv[])
{
	int	c;
	int	errcnt;
#if defined(HAVE_GETOPT_LONG) && HAVE_GETOPT_LONG
	int	opt_index;
#endif
	char	errstr[255+1];
	char	*p;
	const	char	*defconfname;
	zone_t	*zp;

	progname = *argv;
	if ( (p = strrchr (progname, '/')) )
		progname = ++p;

	if ( strncmp (progname, "dnssec-signer", 13) == 0 )
	{
		fprintf (stderr, "The use of dnssec-signer is deprecated, please run zkt-signer instead\n");
		viewname = getnameappendix (progname, "dnssec-signer");
	}
	else
		viewname = getnameappendix (progname, "zkt-signer");
	defconfname = getdefconfname (viewname);
	config = loadconfig ("", (zconf_t *)NULL);	/* load build-in config */
	if ( fileexist (defconfname) )			/* load default config file */
		config = loadconfig (defconfname, config);
	if ( config == NULL )
		fatal ("Couldn't load config: Out of memory\n");

	zonelist = NULL;
        opterr = 0;
#if defined(HAVE_GETOPT_LONG) && HAVE_GETOPT_LONG
	while ( (c = getopt_long (argc, argv, short_options, long_options, &opt_index)) != -1 )
#else
	while ( (c = getopt (argc, argv, short_options)) != -1 )
#endif
	{
		switch ( c )
		{
		case 'V':		/* view name */
			viewname = optarg;
			defconfname = getdefconfname (viewname);
			if ( fileexist (defconfname) )		/* load default config file */
				config = loadconfig (defconfname, config);
			if ( config == NULL )
				fatal ("Out of memory\n");
			break;
		case 'c':		/* load config from file */
			config = loadconfig (optarg, config);
			if ( config == NULL )
				fatal ("Out of memory\n");
			break;
		case 'O':		/* load config option from commandline */
			config = loadconfig_fromstr (optarg, config);
			if ( config == NULL )
				fatal ("Out of memory\n");
			break;
		case 'o':
			origin = optarg;
			break;
		case 'N':
			namedconf = optarg;
			break;
		case 'D':
			dirname = optarg;
			break;
		case 'L':		/* error log file|directory */
			logfile = optarg;
			break;
		case 'f':
			force++;
			break;
		case 'H':
		case 'h':
			usage (NULL, config);
			break;
#if defined(BIND_VERSION) && BIND_VERSION >= 940
		case 'd':
# if BIND_VERSION >= 960
			set_bind96_dynzone (dynamic_zone);
# else
			set_bind94_dynzone(dynamic_zone);
# endif
			/* dynamic zone requires a name server reload... */
			reloadflag = 0;		/* ...but "rndc thaw" reloads the zone anyway */
			break;
#endif
		case 'n':
			noexec = 1;
			break;
		case 'r':
			if ( !dynamic_zone )	/* dynamic zones don't need a rndc reload (see "-d" */
				reloadflag = 1;
			break;
		case 'v':
			verbose++;
			break;
		case '?':
			if ( isprint (optopt) )
				snprintf (errstr, sizeof(errstr),
					"Unknown option \"-%c\".\n", optopt);
			else
				snprintf (errstr, sizeof (errstr),
					"Unknown option char \\x%x.\n", optopt);
			usage (errstr, config);
			break;
		default:
			abort();
		}
	}
	dbg_line();

	/* store some of the commandline parameter in the config structure */
	setconfigpar (config, "--view", viewname);
	setconfigpar (config, "-v", &verbose);
	setconfigpar (config, "--noexec", &noexec);
	if ( logfile == NULL )
		logfile = config->logfile;

	if ( lg_open (progname, config->syslogfacility, config->sysloglevel, config->zonedir, logfile, config->loglevel) < -1 )
		fatal ("Couldn't open logfile %s in dir %s\n", logfile, config->zonedir);

#if defined(DBG) && DBG
	for ( zp = zonelist; zp; zp = zp->next )
		zone_print ("in main: ", zp);
#endif
	lg_args (LG_NOTICE, argc, argv);

	/* 1.0rc1: If the ttl for dynamic zones is not known or if it is 0, use sig valid time for this */
	if ( config->max_ttl <= 0 || dynamic_zone )
	{
		// config = dupconfig (config);
		config->max_ttl = config->sigvalidity;
	}


	if ( origin )		/* option -o ? */
	{
		int	ret;

		if ( (argc - optind) <= 0 )	/* no arguments left ? */
			ret = zone_readdir (".", origin, NULL, &zonelist, config, dynamic_zone);
		else
			ret = zone_readdir (".", origin, argv[optind], &zonelist, config, dynamic_zone);

		/* anyway, "delete" all (remaining) arguments */
		optind = argc;

		/* complain if nothing could read in */
		if ( ret != 1 || zonelist == NULL )
		{
			lg_mesg (LG_FATAL, "\"%s\": couldn't read", origin);
			fatal ("Couldn't read zone \"%s\"\n", origin);
		}
	}
	if ( namedconf )	/* option -N ? */
	{
		char	dir[255+1];

		memset (dir, '\0', sizeof (dir));
		if ( config->zonedir )
			strncpy (dir, config->zonedir, sizeof(dir));
		if ( !parse_namedconf (namedconf, config->chroot_dir, dir, sizeof (dir), add2zonelist) )
			fatal ("Can't read file %s as namedconf file\n", namedconf);
		if ( zonelist == NULL )
			fatal ("No signed zone found in file %s\n", namedconf);
	}
	if ( dirname )		/* option -D ? */
	{
		char	*dir = strdup (dirname);

		p = dir + strlen (dir);
		if ( p > dir )
			p--;
		if ( *p == '/' )
			*p = '\0';	/* remove trailing path seperator */

		if ( !parsedir (dir, &zonelist, config) )
			fatal ("Can't read directory tree %s\n", dir);
		if ( zonelist == NULL )
			fatal ("No signed zone found in directory tree %s\n", dir);
		free (dir);
	}

	/* none of the above: read current directory tree */
	if ( zonelist == NULL )
		parsedir (config->zonedir, &zonelist, config);

	for ( zp = zonelist; zp; zp = zp->next )
		if ( in_strarr (zp->zone, &argv[optind], argc - optind) )
		{
			dosigning (zonelist, zp);
			verbmesg (1, zp->conf, "\n");
		}

	zone_freelist (&zonelist);

	errcnt = lg_geterrcnt ();
	lg_mesg (LG_NOTICE, "end of run: %d error%s occured", errcnt, errcnt == 1 ? "" : "s");
	lg_close ();

	return errcnt < 64 ? errcnt : 64;
}

# define	sopt_usage(mesg, value) fprintf (stderr, mesg, value)
#if defined(HAVE_GETOPT_LONG) && HAVE_GETOPT_LONG
# define	lopt_usage(mesg, value) fprintf (stderr, mesg, value)
# define	loptstr(lstr, sstr)     lstr
#else
# define	lopt_usage(mesg, value)
# define	loptstr(lstr, sstr)     sstr
#endif
static	void	usage (char *mesg, zconf_t *conf)
{
	fprintf (stderr, "%s version %s compiled for BIND %d\n", progname, ZKT_VERSION, BIND_VERSION);
	fprintf (stderr, "ZKT %s\n", ZKT_COPYRIGHT);
	fprintf (stderr, "\n");

	fprintf (stderr, "usage: %s [-L] [-V view] [-c file] [-O optstr] ", progname);
	fprintf (stderr, "[-D directorytree] ");
	fprintf (stderr, "[-fhnr] [-v [-v]] [zone ...]\n");

	fprintf (stderr, "usage: %s [-L] [-V view] [-c file] [-O optstr] ", progname);
	fprintf (stderr, "-N named.conf ");
	fprintf (stderr, "[-fhnr] [-v [-v]] [zone ...]\n");

	fprintf (stderr, "usage: %s [-L] [-V view] [-c file] [-O optstr] ", progname);
	fprintf (stderr, "-o origin ");
	fprintf (stderr, "[-fhnr] [-v [-v]] [zonefile.signed]\n");

	fprintf (stderr, "\t-c file%s", loptstr (", --config=file\n", ""));
	fprintf (stderr, "\t\t read config from <file> instead of %s\n", CONFIG_FILE);
	fprintf (stderr, "\t-O optstr%s", loptstr (", --config-option=\"optstr\"\n", ""));
	fprintf (stderr, "\t\t set config options on the commandline\n");
	fprintf (stderr, "\t-L file|dir%s", loptstr (", --logfile=file|dir\n", ""));
	fprintf (stderr, "\t\t specify file or directory for the log output\n");
	fprintf (stderr, "\t-V name%s", loptstr (", --view=name\n", ""));
	fprintf (stderr, "\t\t specify the view name \n");
	fprintf (stderr, "\t-D dir%s", loptstr (", --directory=dir\n", ""));
	fprintf (stderr, "\t\t parse the given directory tree for a list of secure zones \n");
	fprintf (stderr, "\t-N file%s", loptstr (", --named-conf=file\n", ""));
	fprintf (stderr, "\t\t get the list of secure zones out of the named like config file \n");
	fprintf (stderr, "\t-o zone%s", loptstr (", --origin=zone", ""));
	fprintf (stderr, "\tspecify the name of the zone \n");
	fprintf (stderr, "\t\t The file to sign should be given as an argument (default is \"%s.signed\")\n", conf->zonefile);
	fprintf (stderr, "\t-h%s\t print this help\n", loptstr (", --help", "\t"));
	fprintf (stderr, "\t-f%s\t force re-signing\n", loptstr (", --force", "\t"));
	fprintf (stderr, "\t-n%s\t no execution of external signing command\n", loptstr (", --noexec", "\t"));
	// fprintf (stderr, "\t-r%s\t reload zone via <rndc reload zone> (or via the external distribution command)\n", loptstr (", --reload", "\t"));
	fprintf (stderr, "\t-r%s\t reload zone via %s\n", loptstr (", --reload", "\t"), conf->dist_cmd ? conf->dist_cmd: "rndc");
        fprintf (stderr, "\t-v%s\t be verbose (use twice to be very verbose)\n", loptstr (", --verbose", "\t"));

        fprintf (stderr, "\t[zone]\t sign only those zones given as argument\n");

        fprintf (stderr, "\n");
        fprintf (stderr, "\tif neither -D nor -N nor -o is given, the directory tree specified\n");
	fprintf (stderr, "\tin the dnssec config file (\"%s\") will be parsed\n", conf->zonedir);

	if ( mesg && *mesg )
		fprintf (stderr, "%s\n", mesg);
	exit (127);
}

/**	fill zonelist with infos coming out of named.conf	**/
static	int	add2zonelist (const char *dir, const char *view, const char *zone, const char *file)
{
#ifdef DBG
	fprintf (stderr, "printzone ");
	fprintf (stderr, "view \"%s\" " , view);
	fprintf (stderr, "zone \"%s\" " , zone);
	fprintf (stderr, "file ");
	if ( dir && *dir )
		fprintf (stderr, "%s/", dir);
	fprintf (stderr, "%s", file);
	fprintf (stderr, "\n");
#endif
	dbg_line ();
	if ( view[0] != '\0' )	/* view found in named.conf */
	{
		if ( viewname == NULL || viewname[0] == '\0' )	/* viewname wasn't set on startup ? */
		{
			dbg_line ();
			error ("zone \"%s\" in view \"%s\" found in name server config, but no matching view was set on startup\n", zone, view);
			lg_mesg (LG_ERROR, "\"%s\" in view \"%s\" found in name server config, but no matching view was set on startup", zone, view);
			return 0;
		}
		dbg_line ();
		if ( strcmp (viewname, view) != 0 )	/* zone is _not_ in current view */
			return 0;
	}
	return zone_readdir (dir, zone, file, &zonelist, config, dynamic_zone);
}

static	int	parsedir (const char *dir, zone_t **zp, const zconf_t *conf)
{
	DIR	*dirp;
	struct  dirent  *dentp;
	char	path[MAX_PATHSIZE+1];

	dbg_val ("parsedir: (%s)\n", dir);
	if ( !is_directory (dir) )
		return 0;

	dbg_line ();
	zone_readdir (dir, NULL, NULL, zp, conf, dynamic_zone);

	dbg_val ("parsedir: opendir(%s)\n", dir);
	if ( (dirp = opendir (dir)) == NULL )
		return 0;

	while ( (dentp = readdir (dirp)) != NULL )
	{
		if ( is_dotfilename (dentp->d_name) )
			continue;

		pathname (path, sizeof (path), dir, dentp->d_name, NULL);
		if ( !is_directory (path) )
			continue;

		dbg_val ("parsedir: recursive %s\n", path);
		parsedir (path, zp, conf);
	}
	closedir (dirp);
	return 1;
}

static	int	dosigning (zone_t *zonelist, zone_t *zp)
{
	char	path[MAX_PATHSIZE+1];
	int	err;
	int	newkey;
	int	newkeysetfile;
	int	use_unixtime;
	time_t	currtime;
	time_t	zfile_time;
	time_t	zfilesig_time;
	char	mesg[255+1];

	verbmesg (1, zp->conf, "parsing zone \"%s\" in dir \"%s\"\n", zp->zone, zp->dir);

	pathname (path, sizeof (path), zp->dir, zp->sfile, NULL);
	dbg_val("parsezonedir fileexist (%s)\n", path);
	if ( !fileexist (path) )
	{
		error ("Not a secure zone directory (%s)!\n", zp->dir);
		lg_mesg (LG_ERROR, "\"%s\": not a secure zone directory (%s)!", zp->zone, zp->dir);
		return 1;
	}
	zfilesig_time = file_mtime (path);

	pathname (path, sizeof (path), zp->dir, zp->file, NULL);
	dbg_val("parsezonedir fileexist (%s)\n", path);
	if ( !fileexist (path) )
	{
		error ("No zone file found (%s)!\n", path);
		lg_mesg (LG_ERROR, "\"%s\": no zone file found (%s)!", zp->zone, path);
		return 2;
	}
	
	zfile_time = file_mtime (path);
	currtime = time (NULL);

	/* check for domain based logging */
	if ( is_defined (zp->conf->logdomaindir) )	/* parameter is not null or empty ? */
	{
		if ( strcmp (zp->conf->logdomaindir, ".") == 0 )	/* current (".") means zone directory */
			lg_zone_start (zp->dir, zp->zone);
		else
			lg_zone_start (zp->conf->logdomaindir, zp->zone);
	}

	/* check rfc5011 key signing keys, create new one if necessary */
	dbg_msg("parsezonedir check rfc 5011 ksk ");
	newkey = ksk5011status (&zp->keys, zp->dir, zp->zone, zp->conf);
	if ( (newkey & 02) != 02 )	/* not a rfc 5011 zone ? */
	{
		verbmesg (2, zp->conf, "\t\t->not a rfc5011 zone, looking for a regular ksk rollover\n");
		/* check key signing keys, create new one if necessary */
		dbg_msg("parsezonedir check ksk ");
		newkey |= kskstatus (zonelist, zp);
	}
	else
		newkey &= ~02;		/* reset bit 2 */

	/* check age of zone keys, probably retire (depreciate) or remove old keys */
	dbg_msg("parsezonedir check zsk ");
	newkey += zskstatus (&zp->keys, zp->dir, zp->zone, zp->conf);

	/* check age of "dnskey.db" file against age of keyfiles */
	pathname (path, sizeof (path), zp->dir, zp->conf->keyfile, NULL);
	dbg_val("parsezonedir check_keydb_timestamp (%s)\n", path);
	if ( !newkey )
		newkey = check_keydb_timestamp (zp->keys, file_mtime (path));

	newkeysetfile = 0;
#if defined(ALWAYS_CHECK_KEYSETFILES) && ALWAYS_CHECK_KEYSETFILES	/* patch from Shane Wegner 15. June 2009 */
	/* check if there is a new keyset- file */
	if ( !newkey )
		newkeysetfile = new_keysetfiles (zp->dir, zfilesig_time);
#else
	/* if we work in subdir mode, check if there is a new keyset- file */
	if ( !newkey && zp->conf->keysetdir && strcmp (zp->conf->keysetdir, "..") == 0 )
		newkeysetfile = new_keysetfiles (zp->dir, zfilesig_time);
#endif

	/**
	** Check if it is time to do a re-sign. This is the case if
	**	a) the command line flag -f is set, or
	**	b) new keys are generated, or
	**	c) we found a new KSK of a delegated domain, or
	**	d) the "dnskey.db" file is newer than "zone.db" 
	**	e) the "zone.db" is newer than "zone.db.signed" or
	**	f) "zone.db.signed" is older than the re-sign interval
	**/
	mesg[0] = '\0';
	if ( force )
		snprintf (mesg, sizeof(mesg), "Option -f"); 
	else if ( newkey )
		snprintf (mesg, sizeof(mesg), "Modfied zone key set"); 
	else if ( newkeysetfile )
		snprintf (mesg, sizeof(mesg), "Modified KSK in delegated domain"); 
	else if ( file_mtime (path) > zfilesig_time )
		snprintf (mesg, sizeof(mesg), "Modified keys");
	else if ( zfile_time > zfilesig_time )
		snprintf (mesg, sizeof(mesg), "Zone file edited");
	else if ( (currtime - zfilesig_time) > zp->conf->resign - (OFFSET) )
		snprintf (mesg, sizeof(mesg), "re-signing interval (%s) reached",
						str_delspace (age2str (zp->conf->resign)));
	else if ( bind94_dynzone (dynamic_zone) )
		snprintf (mesg, sizeof(mesg), "dynamic zone");

	if ( *mesg )
		verbmesg (1, zp->conf, "\tRe-signing necessary: %s\n", mesg);
	else
		verbmesg (1, zp->conf, "\tRe-signing not necessary!\n");

	if ( *mesg )
		lg_mesg (LG_NOTICE, "\"%s\": re-signing triggered: %s", zp->zone,  mesg);

	dbg_line ();
	if ( !(force || newkey || newkeysetfile || zfile_time > zfilesig_time ||	
	     file_mtime (path) > zfilesig_time ||
	     (currtime - zfilesig_time) > zp->conf->resign - (OFFSET) ||
	      bind94_dynzone (dynamic_zone)) )
	{
		verbmesg (2, zp->conf, "\tCheck if there is a parent file to copy\n");
		if ( zp->conf->keysetdir && strcmp (zp->conf->keysetdir, "..") == 0 )
			copy_keyset (zp->dir, zp->zone, zp->conf);	/* copy the parent- file if it exist */
		if ( is_defined (zp->conf->logdomaindir) )
			lg_zone_end ();
		return 0;	/* nothing to do */
	}

	/* let's start signing the zone */
	dbg_line ();

	/* create new "dnskey.db" file  */
	pathname (path, sizeof (path), zp->dir, zp->conf->keyfile, NULL);
	verbmesg (1, zp->conf, "\tWriting key file \"%s\"\n", path);
	if ( !writekeyfile (path, zp->keys, zp->conf->key_ttl) )
	{
		error ("Can't create keyfile %s \n", path);
		lg_mesg (LG_ERROR, "\"%s\": can't create keyfile %s", zp->zone , path);
	}

	err = 1;
	use_unixtime = ( zp->conf->serialform == Unixtime );
	dbg_val1 ("Use unixtime = %d\n", use_unixtime);
#if defined(BIND_VERSION) && BIND_VERSION >= 940
	if ( !dynamic_zone && !use_unixtime ) /* increment serial number in static zone files */
#else
	if ( !dynamic_zone ) /* increment serial no in static zone files */
#endif
	{
		pathname (path, sizeof (path), zp->dir, zp->file, NULL);
		err = 0;
		if ( noexec == 0 )
		{
			if ( (err = inc_serial (path, use_unixtime)) < 0 )
			{
				error ("could not increment serialno of domain %s in file %s: %s!\n",
								zp->zone, path, inc_errstr (err));
				lg_mesg (LG_ERROR,
					"zone \"%s\": couldn't increment serialno in file %s: %s",
							zp->zone, path, inc_errstr (err));
			}
			else 
			verbmesg (1, zp->conf, "\tIncrementing serial number in file \"%s\"\n", path);
		}
		else 
			verbmesg (1, zp->conf, "\tIncrementing serial number in file \"%s\"\n", path);
	}

	/* at last, sign the zone file */
	if ( err > 0 )
	{
		time_t	timer;

		verbmesg (1, zp->conf, "\tSigning zone \"%s\"\n", zp->zone);
		logflush ();

		/* dynamic zones uses incremental signing, so we have to */
		/* prepare the old (signed) file as new input file */
		if ( dynamic_zone )
		{
			char	zfile[MAX_PATHSIZE+1];

			dyn_update_freeze (zp->zone, zp->conf, 1);	/* freeze dynamic zone ! */

			pathname (zfile, sizeof (zfile), zp->dir, zp->file, NULL);
			pathname (path, sizeof (path), zp->dir, zp->sfile, NULL);
			if ( filesize (path) == 0L )    /* initial signing request ? */
			{
				verbmesg (1, zp->conf, "\tDynamic Zone signing: Initial signing request: Add DNSKEYs to zonefile\n");
				copyfile (zfile, path, zp->conf->keyfile);
			}
#if 1
			else if ( zfile_time > zfilesig_time )  /* zone.db is newer than signed file */
			{
				verbmesg (1, zp->conf, "\tDynamic Zone signing: zone file manually edited: Use it as new input file\n");
				copyfile (zfile, path, NULL);
			}
#endif
			verbmesg (1, zp->conf, "\tDynamic Zone signing: copy old signed zone file %s to new input file %s\n",
										path, zfile); 

			if ( newkey )	/* if we have new keys, they should be added to the zone file */
			{
				copyzonefile (path, zfile, zp->conf->keyfile);
#if 0
				if ( zp->conf->dist_cmd )
					dist_and_reload (zp, 2);	/* ... and send to the name server */
#endif
			}
			else		/* else we can do a simple file copy */
				copyfile (path, zfile, NULL);
		}

		timer = start_timer ();
		if ( (err = sign_zone (zp)) < 0 )
		{
			error ("\tSigning of zone %s failed (%d)!\n", zp->zone, err);
			lg_mesg (LG_ERROR, "\"%s\": signing failed!", zp->zone);
		}
		timer = stop_timer (timer);

		if ( dynamic_zone )
			dyn_update_freeze (zp->zone, zp->conf, 0);	/* thaw dynamic zone file */

		if ( err >= 0 )
		{
		const	char	*tstr = str_delspace (age2str (timer));

		if ( !tstr || *tstr == '\0' )
			tstr = "0s";
		verbmesg (1, zp->conf, "\tSigning completed after %s.\n", tstr);
		}
	}

	copy_keyset (zp->dir, zp->zone, zp->conf);

	if ( err >= 0 && reloadflag )
	{
		if ( zp->conf->dist_cmd )
			dist_and_reload (zp, 1);
		else
			reload_zone (zp->zone, zp->conf);

		register_key (zp->keys, zp->conf);
	}

	if ( is_defined (zp->conf->logdomaindir) )
		lg_zone_end ();

	return err;
}

static	void	register_key (dki_t *list, const zconf_t *z)
{
	dki_t	*dkp;
	time_t	currtime;
	time_t	age;

	assert ( list != NULL );
	assert ( z != NULL );

	currtime = time (NULL);
	for ( dkp = list; dkp && dki_isksk (dkp); dkp = dkp->next )
	{
		age = dki_age (dkp, currtime);
#if 0
		/* announce "new" and active key signing keys */
		if ( REG_URL && *REG_URL && dki_status (dkp) == DKI_ACT && age <= z->resign * 4 )
		{
			if ( verbose )
				logmesg ("\tRegister new KSK with tag %d for domain %s\n",
								dkp->tag, dkp->name);
		}
#endif
	}
}

/*
 *	This function is not working with symbolic links to keyset- files,
 *	because file_mtime() returns the mtime of the underlying file, and *not*
 *	that of the symlink file.
 *	This is bad, because the keyset-file will be newly generated by dnssec-signzone
 *	on every re-signing call.
 *	Instead, in the case of a hierarchical directory structure, we copy the file
 *	(and so we change the timestamp) only if it was modified after the last
 *	generation (checked with cmpfile(), see func sign_zone()).
 */
# define	KEYSET_FILE_PFX	"keyset-"
static	int	new_keysetfiles (const char *dir, time_t zone_signing_time)
{
	DIR	*dirp;
	struct  dirent  *dentp;
	char	path[MAX_PATHSIZE+1];
	int	newkeysetfile;

	if ( (dirp = opendir (dir)) == NULL )
		return 0;

	newkeysetfile = 0;
	dbg_val2 ("new_keysetfile (%s, %s)\n", dir, time2str (zone_signing_time, 's')); 
	while ( !newkeysetfile && (dentp = readdir (dirp)) != NULL )
	{
		if ( strncmp (dentp->d_name, KEYSET_FILE_PFX, strlen (KEYSET_FILE_PFX)) != 0 )
			continue;

		pathname (path, sizeof (path), dir, dentp->d_name, NULL);
		dbg_val2 ("newkeysetfile timestamp of %s = %s\n", path, time2str (file_mtime(path), 's')); 
		if ( file_mtime (path) > zone_signing_time )
			newkeysetfile = 1;
	}
	closedir (dirp);

	return newkeysetfile;
}

static	int	check_keydb_timestamp (dki_t *keylist, time_t reftime)
{
	dki_t	*key;

	assert ( keylist != NULL );
	if ( reftime == 0 )
		return 1;

	for ( key = keylist; key; key = key->next )
		if ( dki_time (key) > reftime )
			return 1;

	return 0;
}

static	int	writekeyfile (const char *fname, const dki_t *list, int key_ttl)
{
	FILE	*fp;
	const	dki_t	*dkp;
	time_t	curr = time (NULL);
	int	ksk;

	if ( (fp = fopen (fname, "w")) == NULL )
		return 0;
	fprintf (fp, ";\n");
	fprintf (fp, ";\t!!! Don\'t edit this file by hand.\n");
	fprintf (fp, ";\t!!! It will be generated by %s.\n", progname);
	fprintf (fp, ";\n");
	fprintf (fp, ";\t Last generation time %s\n", time2str (curr, 's'));
	fprintf (fp, ";\n");

	fprintf (fp, "\n");
	fprintf (fp, ";  ***  List of Key Signing Keys  ***\n");
	ksk = 1;
	for ( dkp = list; dkp; dkp = dkp->next )
	{
		if ( ksk && !dki_isksk (dkp) )
		{
			fprintf (fp, "; ***  List of Zone Signing Keys  ***\n");
			ksk = 0;
		}
		dki_prt_comment (dkp, fp);
		dki_prt_dnskeyttl (dkp, fp, key_ttl);
		putc ('\n', fp);
	}
	
	fclose (fp);
	return 1;
}

static	int	sign_zone (const zone_t *zp)
{
	char	cmd[2047+1];
	char	str[1023+1];
	char	rparam[254+1];
	char	nsec3param[637+1];
	char	keysetdir[254+1];
	const	char	*gends;
	const	char	*dnskeyksk;
	const	char	*pseudo;
	const	char	*param;
	int	len;
	FILE	*fp;

	const	char	*dir;
	const	char	*domain;
	const	char	*file;
	const	zconf_t	*conf;

	assert (zp != NULL);
	dir = zp->dir;
	domain = zp->zone;
	file = zp->file;
	conf = zp->conf;

	len = 0;
	str[0] = '\0';
	if ( conf->lookaside && conf->lookaside[0] )
		len = snprintf (str, sizeof (str), "-l %.250s", conf->lookaside);

	dbg_line();
#if defined(BIND_VERSION) && BIND_VERSION >= 940
	if ( !dynamic_zone && conf->serialform == Unixtime )
		snprintf (str+len, sizeof (str) - len, " -N unixtime");
#endif

	gends = "";
	if ( conf->sig_gends )
#if defined(BIND_VERSION) && BIND_VERSION >= 970
		gends = "-C -g ";
#else
		gends = "-g ";
#endif

	dnskeyksk = "";
#if defined(BIND_VERSION) && BIND_VERSION >= 970
	if ( conf->sig_dnskeyksk )
		dnskeyksk = "-x ";
#endif

	pseudo = "";
	if ( conf->sig_pseudo )
		pseudo = "-p ";

	param = "";
	if ( conf->sig_param && conf->sig_param[0] )
		param = conf->sig_param;

	nsec3param[0] = '\0';
#if defined(BIND_VERSION) && BIND_VERSION >= 960
	if ( conf->k_algo == DK_ALGO_NSEC3DSA || conf->k_algo == DK_ALGO_NSEC3RSASHA1 ||
	     conf->nsec3 != NSEC3_OFF )
	{
		char	salt[510+1];	/* salt has a maximum of 255 bytes == 510 hex nibbles */
		const	char	*update;
		const	char	*optout;
		unsigned int	seed;

# if defined(BIND_VERSION) && BIND_VERSION >= 970
		update = "-u ";		/* trailing blank is necessary */
# else
		update = "";
# endif
		if ( conf->nsec3 == NSEC3_OPTOUT )
			optout = "-A ";
		else
			optout = "";

			/* static zones can use always a new salt (full zone signing) */
		seed = 0L;	/* no seed: use mechanism build in gensalt() */
		if ( dynamic_zone )
		{		/* dynamic zones have to reuse the salt on signing */
			const	dki_t	*kp;

			/* use gentime timestamp of ZSK for seeding rand generator */
			kp = dki_find (zp->keys, DKI_ZSK, DKI_ACTIVE, 1);
			assert ( kp != NULL );
			if ( kp->gentime )
				seed = kp->gentime;
			else
				seed = kp->time;
		}

		if ( gensalt (salt, sizeof (salt), conf->saltbits, seed) )
			snprintf (nsec3param, sizeof (nsec3param), "%s%s-3 %s ", update, optout, salt);
	}
#endif

	dbg_line();
	rparam[0] = '\0';
	if ( conf->sig_random && conf->sig_random[0] )
		snprintf (rparam, sizeof (rparam), "-r %.250s ", conf->sig_random);

	dbg_line();
	keysetdir[0] = '\0';
	if ( conf->keysetdir && conf->keysetdir[0] && strcmp (conf->keysetdir, "..") != 0 )
		snprintf (keysetdir, sizeof (keysetdir), "-d %.250s ", conf->keysetdir);

	if ( dir == NULL || *dir == '\0' )
		dir = ".";

	dbg_line();
#if defined(BIND_VERSION) && BIND_VERSION >= 940
	if ( dynamic_zone )
		snprintf (cmd, sizeof (cmd), "cd %s; %s %s %s%s%s%s%s%s-o %s -e +%ld %s -N increment -f %s.dsigned %s K*.private 2>&1",
			dir, SIGNCMD, param, nsec3param, dnskeyksk, gends, pseudo, rparam, keysetdir, domain, conf->sigvalidity, str, file, file);
	else
#endif
		snprintf (cmd, sizeof (cmd), "cd %s; %s %s %s%s%s%s%s%s-o %s -e +%ld %s %s K*.private 2>&1",
			dir, SIGNCMD, param, nsec3param, dnskeyksk, gends, pseudo, rparam, keysetdir, domain, conf->sigvalidity, str, file);
	verbmesg (2, conf, "\t  Run cmd \"%s\"\n", cmd);
	*str = '\0';
	if ( noexec == 0 )
	{
#if 0
		if ( (fp = popen (cmd, "r")) == NULL || fgets (str, sizeof str, fp) == NULL )
			return -1;
#else
		if ( (fp = popen (cmd, "r")) == NULL )
			return -1;
		str[0] = '\0';
		while ( fgets (str, sizeof str, fp) != NULL )	/* eat up all output until the last line */
			;
#endif
		pclose (fp);
	}

	dbg_line();
	verbmesg (2, conf, "\t  Cmd dnssec-signzone return: \"%s\"\n", str_chop (str, '\n'));
	len = strlen (str) - 6;
	if ( len < 0 || strcmp (str+len, "signed") != 0 )
		return -1;

	return 0;
}

static	void	copy_keyset (const char *dir, const char *domain, const zconf_t *conf)
{
	char	fromfile[1024];
	char	tofile[1024];
	int	ret;

	/* propagate "keyset"-file to parent dir */
	if ( conf->keysetdir && strcmp (conf->keysetdir, "..") == 0 )
	{
		/* check if special parent-file exist (ksk rollover) */
		snprintf (fromfile, sizeof (fromfile), "%s/parent-%s", dir, domain);
		if ( !fileexist (fromfile) )	/* use "normal" keyset-file */
			snprintf (fromfile, sizeof (fromfile), "%s/keyset-%s", dir, domain);

		/* verbmesg (2, conf, "\t  check \"%s\" against parent dir\n", fromfile); */
		snprintf (tofile, sizeof (tofile), "%s/../keyset-%s", dir, domain);
		if ( cmpfile (fromfile, tofile) != 0 )
		{
			verbmesg (2, conf, "\t  copy \"%s\" to parent dir\n", fromfile);
			if ( (ret = copyfile (fromfile, tofile, NULL)) != 0 )
			{
				error ("Couldn't copy \"%s\" to parent dir (%d:%s)\n",
					fromfile, ret, strerror(errno));
				lg_mesg (LG_ERROR, "\%s\": can't copy \"%s\" to parent dir (%d:%s)",
					domain, fromfile, ret, strerror(errno));
			}
		}
	}
}
