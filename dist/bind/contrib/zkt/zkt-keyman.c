/*****************************************************************
**
**	@(#) zkt-keyman.c (c) Jan 2005 - Apr 2010  Holger Zuleger  hznet.de
**
**	ZKT key managing tool (formely knon as dnsses-zkt)
**	A wrapper command around the BIND dnssec-keygen utility
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

static	int	dirflag = 0;
static	int	recflag = RECURSIVE;
static	char	*kskdomain = "";
static	const	char	*view = "";

# define	short_options	":0:1:2:3:9A:C:D:P:S:R:h:ZV:F:c:O:krz"
#if defined(HAVE_GETOPT_LONG) && HAVE_GETOPT_LONG
static struct option long_options[] = {
	{"ksk-rollover",	no_argument, NULL, '9'},
	{"ksk-status",		required_argument, NULL, '0'},
	{"ksk-roll-status",	required_argument, NULL, '0'},
	{"ksk-newkey",		required_argument, NULL, '1'},
	{"ksk-publish",		required_argument, NULL, '2'},
	{"ksk-delkey",		required_argument, NULL, '3'},
	{"ksk-roll-phase1",	required_argument, NULL, '1'},
	{"ksk-roll-phase2",	required_argument, NULL, '2'},
	{"ksk-roll-phase3",	required_argument, NULL, '3'},
	{"ksk",			no_argument, NULL, 'k'},
	{"zsk",			no_argument, NULL, 'z'},
	{"recursive",		no_argument, NULL, 'r'},
	{"config",		required_argument, NULL, 'c'},
	{"option",		required_argument, NULL, 'O'},
	{"config-option",	required_argument, NULL, 'O'},
	{"published",		required_argument, NULL, 'P'},
	{"standby",		required_argument, NULL, 'S'},
	{"active",		required_argument, NULL, 'A'},
	{"depreciated",		required_argument, NULL, 'D'},
	{"create",		required_argument, NULL, 'C'},
	{"revoke",		required_argument, NULL, 'R'},
	{"remove",		required_argument, NULL, 19 },
	{"destroy",		required_argument, NULL, 20 },
	{"setlifetime",		required_argument, NULL, 'F' },
	{"view",		required_argument, NULL, 'V' },
	{"help",		no_argument, NULL, 'h'},
	{0, 0, 0, 0}
};
#endif

static	int	parsedirectory (const char *dir, dki_t **listp);
static	void	parsefile (const char *file, dki_t **listp);
static	void	createkey (const char *keyname, const dki_t *list, const zconf_t *conf);
static	void	ksk_roll (const char *keyname, int phase, const dki_t *list, const zconf_t *conf);
static	int	create_parent_file (const char *fname, int phase, int ttl, const dki_t *dkp);
static	void    usage (char *mesg, zconf_t *cp);
static	const char *parsetag (const char *str, int *tagp);

static	void	setglobalflags (zconf_t *config)
{
	recflag = config->recursive;
}

int	main (int argc, char *argv[])
{
	dki_t	*data = NULL;
	dki_t	*dkp;
	int	c;
	int	opt_index;
	int	action;
	const	char	*file;
	const	char	*defconfname = NULL;
	char	*p;
	char	str[254+1];
	const char	*keyname = NULL;
	int		searchtag;
	zconf_t	*config;

	progname = *argv;
	if ( (p = strrchr (progname, '/')) )
		progname = ++p;
	view = getnameappendix (progname, "dnssec-zkt");

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
		case '9':		/* ksk rollover help */
			ksk_roll ("help", c - '0', NULL, NULL);
			exit (1);
		case '1':		/* ksk rollover: create new key */
		case '2':		/* ksk rollover: publish DS */
		case '3':		/* ksk rollover: delete old key */
		case '0':		/* ksk rollover: show current status */
			action = c;
			if ( !optarg )
				usage ("ksk rollover requires an domain argument", config);
			kskdomain = domain_canonicdup (optarg);
			break;
		case 'h':
		case 'K':
		case 'Z':
			action = c;
			break;
		case 'C':
			pathflag = !pathflag;
			/* fall through */
		case 'P':
		case 'S':
		case 'A':
		case 'D':
		case 'R':
		case 's':
		case 19:
		case 20:
			if ( (keyname = parsetag (optarg, &searchtag)) != NULL )
				keyname = domain_canonicdup (keyname);
			action = c;
			break;
		case 'F':		/* set key lifetime */
			lifetime = atoi (optarg);
			action = c;
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
		case 'k':		/* ksk only */
			zskflag = 0;
			break;
		case 'r':		/* switch recursive flag */
			recflag = !recflag;
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

	c = optind;
	do {
		if ( c >= argc )		/* no args left */
			file = config->zonedir;	/* use default directory */
		else
			file = argv[c++];

		if ( is_directory (file) )
			parsedirectory (file, &data);
		else
			parsefile (file, &data);

	}  while ( c < argc );	/* for all arguments */

	switch ( action )
	{
	case 'h':
		usage ("", config);
	case 'C':
		createkey (keyname, data, config);
		break;
	case 'P':
	case 'S':
	case 'A':
	case 'D':
		if ( (dkp = (dki_t*)zkt_search (data, searchtag, keyname)) == NULL )
			fatal ("Key with tag %u not found\n", searchtag);
		else if ( dkp == (void *) 01 )
			fatal ("Key with tag %u found multiple times\n", searchtag);
		if ( (c = dki_setstatus_preservetime (dkp, action)) != 0 )
			fatal ("Couldn't change status of key %u: %d\n", searchtag, c);
		break;
	case 19:	/* remove (rename) key file */
		if ( (dkp = (dki_t *)zkt_search (data, searchtag, keyname)) == NULL )
			fatal ("Key with tag %u not found\n", searchtag);
		else if ( dkp == (void *) 01 )
			fatal ("Key with tag %u found multiple times\n", searchtag);
		dki_remove (dkp);
		break;
	case 20:	/* destroy the key (remove the files!) */
		if ( (dkp = (dki_t *)zkt_search (data, searchtag, keyname)) == NULL )
			fatal ("Key with tag %u not found\n", searchtag);
		else if ( dkp == (void *) 01 )
			fatal ("Key with tag %u found multiple times\n", searchtag);
		dki_destroy (dkp);
		break;
	case 'R':
		if ( (dkp = (dki_t *)zkt_search (data, searchtag, keyname)) == NULL )
			fatal ("Key with tag %u not found\n", searchtag);
		else if ( dkp == (void *) 01 )
			fatal ("Key with tag %u found multiple times\n", searchtag);
		if ( (c = dki_setstatus (dkp, action)) != 0 )
			fatal ("Couldn't change status of key %u: %d\n", searchtag, c);
		break;
	case '1':	/* ksk rollover new key */
	case '2':	/* ksk rollover publish DS */
	case '3':	/* ksk rollover delete old key */
	case '0':	/* ksk rollover status */
		ksk_roll (kskdomain, action - '0', data, config);
		break;
	case 'F':
		zkt_setkeylifetime (data);
		/* fall through */
	default:
		zkt_list_keys (data);
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
static	void    usage (char *mesg, zconf_t *cp)
{
        fprintf (stderr, "DNS Zone Key Management Tool %s\n", ZKT_VERSION);
        fprintf (stderr, "\n");
        fprintf (stderr, "Create a new key \n");
        sopt_usage ("\tusage: %s -C <name> [-k] [-dpr] [-c config] [dir ...]\n", progname);
        lopt_usage ("\tusage: %s --create=<name> [-k] [-dpr] [-c config] [dir ...]\n", progname);
        fprintf (stderr, "\t\tKSK (use -k):  %s %d bits\n", dki_algo2str (cp->k_algo), cp->k_bits);
        fprintf (stderr, "\t\tZSK (default): %s %d bits\n", dki_algo2str (cp->k_algo), cp->z_bits);
        fprintf (stderr, "\n");
        fprintf (stderr, "Change key status of specified key to published, active or depreciated\n");
        fprintf (stderr, "\t(<keyspec> := tag | tag:name) \n");
        sopt_usage ("\tusage: %s -P|-A|-D <keyspec> [-dr] [-c config] [dir ...]\n", progname);
        lopt_usage ("\tusage: %s --published=<keyspec> [-dr] [-c config] [dir ...]\n", progname);
        lopt_usage ("\tusage: %s --active=<keyspec> [-dr] [-c config] [dir ...]\n", progname);
        lopt_usage ("\tusage: %s --depreciated=<keyspec> [-dr] [-c config] [dir ...]\n", progname);
        fprintf (stderr, "\n");
        fprintf (stderr, "Revoke specified key (<keyspec> := tag | tag:name) \n");
        sopt_usage ("\tusage: %s -R <keyspec> [-dr] [-c config] [dir ...]\n", progname);
        lopt_usage ("\tusage: %s --revoke=<keyspec> [-dr] [-c config] [dir ...]\n", progname);
        fprintf (stderr, "\n");
        fprintf (stderr, "Remove (rename) or destroy (delete) specified key (<keyspec> := tag | tag:name) \n");
        lopt_usage ("\tusage: %s --remove=<keyspec> [-dr] [-c config] [dir ...]\n", progname);
        lopt_usage ("\tusage: %s --destroy=<keyspec> [-dr] [-c config] [dir ...]\n", progname);
        fprintf (stderr, "\n");
        fprintf (stderr, "Initiate a semi-automated KSK rollover");
        fprintf (stderr, "('%s -9%s' prints out a brief description)\n", progname, loptstr ("|--ksk-rollover", ""));
        sopt_usage ("\tusage: %s {-1} do.ma.in.\n", progname);
        lopt_usage ("\tusage: %s {--ksk-roll-phase1|--ksk-newkey} do.ma.in.\n", progname);
        sopt_usage ("\tusage: %s {-2} do.ma.in.\n", progname);
        lopt_usage ("\tusage: %s {--ksk-roll-phase2|--ksk-publish} do.ma.in.\n", progname);
        sopt_usage ("\tusage: %s {-3} do.ma.in.\n", progname);
        lopt_usage ("\tusage: %s {--ksk-roll-phase3|--ksk-delkey} do.ma.in.\n", progname);
        sopt_usage ("\tusage: %s {-0} do.ma.in.\n", progname);
        lopt_usage ("\tusage: %s {--ksk-roll-status|--ksk-status} do.ma.in.\n", progname);
        fprintf (stderr, "\n");

        fprintf (stderr, "\n");
        fprintf (stderr, "General options \n");
        fprintf (stderr, "\t-c file%s", loptstr (", --config=file\n", ""));
	fprintf (stderr, "\t\t read config from <file> instead of %s\n", CONFIG_FILE);
        fprintf (stderr, "\t-O optstr%s", loptstr (", --config-option=\"optstr\"\n", ""));
	fprintf (stderr, "\t\t read config options from commandline\n");
        fprintf (stderr, "\t-d%s\t skip directory arguments\n", loptstr (", --directory", "\t"));
        fprintf (stderr, "\t-r%s\t recursive mode on/off (default: %s)\n", loptstr(", --recursive", "\t"), recflag ? "on": "off");
        fprintf (stderr, "\t-F days%s=days\t set key lifetime\n", loptstr (", --setlifetime", "\t"));
        fprintf (stderr, "\t-k%s\t key signing keys only\n", loptstr (", --ksk", "\t"));
        fprintf (stderr, "\t-z%s\t zone signing keys only\n", loptstr (", --zsk", "\t"));
        if ( mesg && *mesg )
                fprintf (stderr, "%s\n", mesg);
        exit (1);
}

static	void	createkey (const char *keyname, const dki_t *list, const zconf_t *conf)
{
	const char *dir = "";
	dki_t	*dkp;

	if ( keyname == NULL || *keyname == '\0' )
		fatal ("Create key: no keyname!");

	dbg_val2 ("createkey: keyname %s, pathflag = %d\n", keyname, pathflag);
	/* search for already existent key to get the directory name */
	if ( pathflag && (dkp = (dki_t *)zkt_search (list, 0, keyname)) != NULL )
	{
		char    path[MAX_PATHSIZE+1];
		zconf_t localconf;

		dir = dkp->dname;
		pathname (path, sizeof (path), dir, LOCALCONF_FILE, NULL);
		if ( fileexist (path) )                 /* load local config file */
		{
			dbg_val ("Load local config file \"%s\"\n", path);
			memcpy (&localconf, conf, sizeof (zconf_t));
			conf = loadconfig (path, &localconf);
		}
	}
	
	if  ( zskflag )
		dkp = dki_new (dir, keyname, DKI_ZSK, conf->k_algo, conf->z_bits, conf->z_random, conf->z_life / DAYSEC);
	else
		dkp = dki_new (dir, keyname, DKI_KSK, conf->k_algo, conf->k_bits, conf->k_random, conf->k_life / DAYSEC);
	if ( dkp == NULL )
		fatal ("Can't create key %s: %s!\n", keyname, dki_geterrstr ());

	/* create a new key always in state published, which means "standby" for ksk */
	dki_setstatus (dkp, DKI_PUB);
}

static	int	get_parent_phase (const char *file)
{
	FILE	*fp;
	int	phase;

	if ( (fp = fopen (file, "r")) == NULL )
		return -1;

	phase = 0;
	if ( fscanf (fp, "; KSK rollover phase%d", &phase) != 1 )
		phase = 0;

	fclose (fp);
	return phase;
}

static	void	ksk_roll (const char *keyname, int phase, const dki_t *list, const zconf_t *conf)
{
	char    path[MAX_PATHSIZE+1];
	zconf_t localconf;
	const char *dir;
	dki_t	*keylist;
	dki_t	*dkp;
	dki_t	*standby;
	int	parent_exist;
	int	parent_age;
	int	parent_phase;
	int	parent_propagation;
	int	key_ttl;
	int	ksk;

	if ( phase == 9 )	/* usage */
	{
		fprintf (stderr, "A KSK rollover requires three consecutive steps:\n");
		fprintf (stderr, "\n");
		fprintf (stderr, "-1%s", loptstr ("|--ksk-roll-phase1 (--ksk-newkey)\n", ""));
		fprintf (stderr, "\t Create a new KSK.\n");
		fprintf (stderr, "\t This step also creates a parent-<domain> file which contains only\n");
		fprintf (stderr, "\t the _old_ key.  This file will be copied in hierarchical mode\n");
		fprintf (stderr, "\t by dnssec-signer to the parent directory as keyset-<domain> file.\n");
		fprintf (stderr, "\t Wait until the new keyset is propagated, before going to the next step.\n");
		fprintf (stderr, "\n");
		fprintf (stderr, "-2%s", loptstr ("|--ksk-roll-phase2 (--ksk-publish)\n", ""));
		fprintf (stderr, "\t This step creates a parent-<domain> file with the _new_ key only.\n");
		fprintf (stderr, "\t Please send this file immediately to the parent (In hierarchical\n");
		fprintf (stderr, "\t mode this will be done automatically by the dnssec-signer command).\n");
		fprintf (stderr, "\t Then wait until the new DS is generated by the parent and propagated\n");
		fprintf (stderr, "\t to all the parent name server, plus the old DS TTL before going to step three.\n");
		fprintf (stderr, "\n");
		fprintf (stderr, "-3%s", loptstr ("|--ksk-roll-phase3 (--ksk-delkey)\n", ""));
		fprintf (stderr, "\t Remove (rename) the old KSK and the parent-<domain> file.\n");
		fprintf (stderr, "\t You have to manually delete the old KSK (look at file names beginning\n");
		fprintf (stderr, "\t with an lower 'k').\n");
		fprintf (stderr, "\n");
		fprintf (stderr, "-0%s", loptstr ("|--ksk-roll-stat (--ksk-status)\n", ""));
		fprintf (stderr, "\t Show the current KSK rollover state of a domain.\n");

		fprintf (stderr, "\n");

		return;
	}

	if ( keyname == NULL || *keyname == '\0' )
		fatal ("ksk rollover: no domain!");

	dbg_val2 ("ksk_roll: keyname %s, phase = %d\n", keyname, phase);

	/* search for already existent key to get the directory name */
	if ( (keylist = (dki_t *)zkt_search (list, 0, keyname)) == NULL )
		fatal ("ksk rollover: domain %s not found!\n", keyname);
	dkp = keylist;

	/* try to read local config file */
	dir = dkp->dname;
	pathname (path, sizeof (path), dir, LOCALCONF_FILE, NULL);
	if ( fileexist (path) )                 /* load local config file */
	{
		dbg_val ("Load local config file \"%s\"\n", path);
		memcpy (&localconf, conf, sizeof (zconf_t));
		conf = loadconfig (path, &localconf);
	}
	key_ttl = conf->key_ttl;

	/* check if parent-file already exist */
	pathname (path, sizeof (path), dir, "parent-", keyname);
	parent_phase = parent_age = 0;
	if ( (parent_exist = fileexist (path)) != 0 )
	{
		parent_phase = get_parent_phase (path);
		parent_age = file_age (path);
	}
	// parent_propagation = 2 * DAYSEC;
	parent_propagation = 5 * MINSEC;

	ksk = 0;	/* count active(!) key signing keys */
	standby = NULL;	/* find standby key if available */
	for ( dkp = keylist; dkp; dkp = dkp->next )
		if ( dki_isksk (dkp) )
		 {
			if ( dki_status (dkp) == DKI_ACT )
				ksk++;
			else if ( dki_status (dkp) == DKI_PUB )
				standby = dkp;
		}

	switch ( phase )
	{
	case 0:	/* print status (debug) */
		fprintf (stdout, "ksk_rollover:\n");
		fprintf (stdout, "\t domain = %s\n", keyname);
		fprintf (stdout, "\t phase = %d\n", parent_phase);
		fprintf (stdout, "\t parent_file %s %s\n", path, parent_exist ? "exist": "not exist");
		if ( parent_exist )
			fprintf (stdout, "\t age of parent_file %d %s\n", parent_age, str_delspace (age2str (parent_age)));
		fprintf (stdout, "\t # of active key signing keys %d\n", ksk);
		fprintf (stdout, "\t parent_propagation %d %s\n", parent_propagation, str_delspace (age2str (parent_propagation)));
		fprintf (stdout, "\t keys ttl %d %s\n", key_ttl, age2str (key_ttl));

		for ( dkp = keylist; dkp; dkp = dkp->next )
		{
			/* TODO: Nur zum testen */
			dki_prt_dnskey (dkp, stdout);
		}
		break;
	case 1:
		if ( parent_exist || ksk > 1 )
			fatal ("Can\'t create new ksk because there is already an ksk rollover in progress\n");

		fprintf (stdout, "create new ksk \n");
		dkp = dki_new (dir, keyname, DKI_KSK, conf->k_algo, conf->k_bits, conf->k_random, conf->k_life / DAYSEC);
		if ( dkp == NULL )
			fatal ("Can't create key %s: %s!\n", keyname, dki_geterrstr ());
		if ( standby )
		{
			dki_setstatus (standby, DKI_ACT);	/* activate standby key */
			dki_setstatus (dkp, DKI_PUB);	/* new key will be the new standby */
		}

		// dkp = keylist;	/* use old key to create the parent file */
		if ( (dkp = (dki_t *)dki_findalgo (keylist, 1, conf->k_algo, 'a', 1)) == NULL )	/* find the oldest active ksk to create the parent file */
			fatal ("ksk_rollover phase1: Couldn't find the old active key\n");
		if ( !create_parent_file (path, phase, key_ttl, dkp) )
			fatal ("Couldn't create parentfile %s\n", path);
		break;

	case 2:
		if ( ksk < 2 )
			fatal ("Can\'t publish new key because no one exist\n");
		if ( !parent_exist )
			fatal ("More than one KSK but no parent file found!\n");
		if ( parent_phase != 1 )
			fatal ("Parent file exists but is in wrong state (phase = %d)\n", parent_phase);
		if ( parent_age < conf->proptime + key_ttl )
			fatal ("ksk_rollover (phase2): you have to wait for the propagation of the new KSK (at least %dsec or %s)\n",
				conf->proptime + key_ttl - parent_age,
				str_delspace (age2str (conf->proptime + key_ttl - parent_age)));

		fprintf (stdout, "save new ksk in parent file\n");
		dkp = keylist->next;	/* set dkp to new ksk */
		if ( !create_parent_file (path, phase, key_ttl, dkp) )
			fatal ("Couldn't create parentfile %s\n", path);
		break;
	case 3:
		if ( !parent_exist || ksk < 2 )
			fatal ("ksk-delkey only allowed after ksk-publish\n");
		if ( parent_phase != 2 )
			fatal ("Parent file exists but is in wrong state (phase = %d)\n", parent_phase);
		if ( parent_age < parent_propagation + key_ttl )
			fatal ("ksk_rollover (phase3): you have to wait for DS propagation (at least %dsec or %s)\n",
				parent_propagation + key_ttl - parent_age,
				str_delspace (age2str (parent_propagation + key_ttl - parent_age)));
		/* remove the parentfile */
		fprintf (stdout, "remove parentfile \n");
		unlink (path);
		/* remove or rename the old key */
		fprintf (stdout, "old ksk renamed \n");
		dkp = keylist;	/* set dkp to old ksk */
		dki_remove (dkp);
		break;
	default:	assert (phase == 1 || phase == 2 || phase == 3);
	}
}

/*****************************************************************
**	create_parent_file ()
*****************************************************************/
static	int	create_parent_file (const char *fname, int phase, int ttl, const dki_t *dkp)
{
	FILE	*fp;

	assert ( fname != NULL );

	if ( dkp == NULL || (phase != 1 && phase != 2) )
		return 0;

	if ( (fp = fopen (fname, "w")) == NULL )
		fatal ("can\'t create new parentfile \"%s\"\n", fname);

	if ( phase == 1 )
		fprintf (fp, "; KSK rollover phase1 (old key)\n");
	else
		fprintf (fp, "; KSK rollover phase2 (new key)\n");

	dki_prt_dnskeyttl (dkp, fp, ttl);
	fclose (fp);

	return phase;
}

static	int	parsedirectory (const char *dir, dki_t **listp)
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
			parsedirectory (path, listp);
		}
		else if ( is_keyfilename (dentp->d_name) )
			if ( (dkp = dki_read (dir, dentp->d_name)) )
			{
				// fprintf (stderr, "parsedir: tssearch (%d %s)\n", dkp, dkp->name);
#if defined (USE_TREE) && USE_TREE
				dki_tadd (listp, dkp, 1);
#else
				dki_add (listp, dkp);
#endif
			}
	}
	closedir (dirp);
	return 1;
}

static	void	parsefile (const char *file, dki_t **listp)
{
	char	path[MAX_PATHSIZE+1];
	dki_t	*dkp;

	/* file arg contains path ? ... */
	file = splitpath (path, sizeof (path), file);	/* ... then split of */

	if ( is_keyfilename (file) )	/* plain file name looks like DNS key file ? */
	{
		if ( (dkp = dki_read (path, file)) )	/* read DNS key file ... */
#if defined (USE_TREE) && USE_TREE
			dki_tadd (listp, dkp, 1);		/* ... and add to tree */
#else
			dki_add (listp, dkp);		/* ... and add to list */
#endif
		else
			error ("error parsing %s: (%s)\n", file, dki_geterrstr());
	}
}

static	const char *parsetag (const char *str, int *tagp)
{
	const	char	*p;

	*tagp = 0;
	while ( isspace (*str) )	/* skip leading ws */
		str++;

	p = str;
	if ( isdigit (*p) )		/* keytag starts with digit */
	{
		sscanf (p, "%u", tagp);	/* read keytag as number */
		do			/* eat up to the end of the number */
			p++;
		while ( isdigit (*p) );

		if ( *p == ':' )	/* label follows ? */
			return p+1;	/* return that */
		if ( *p == '\0' )
			return NULL;	/* no label */
	}
	return str;	/* return as label string if not a numeric keytag */
}
