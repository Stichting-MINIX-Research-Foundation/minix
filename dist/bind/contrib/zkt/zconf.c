/****************************************************************
**
**	@(#) zconf.c -- configuration file parser for dnssec.conf
**
**	Most of the code is from the SixXS Heartbeat Client
**	written by Jeroen Massar <jeroen@sixxs.net>
**
**	New config types and many code changes by Holger Zuleger
**
**	Copyright (c) Aug 2005, Jeroen Massar.
**	Copyright (c) Aug 2005 - Apr 2010, Holger Zuleger.
**	All rights reserved.
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
**	Neither the name of Jeroen Masar or Holger Zuleger nor the
**	names of its contributors may be used to endorse or promote products
**	derived from this software without specific prior written permission.
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
****************************************************************/
# include <sys/types.h>
# include <stdio.h>
# include <errno.h>
# include <unistd.h>
# include <stdlib.h>
# include <stdarg.h>
# include <string.h>
# include <strings.h>
# include <assert.h>
# include <ctype.h>

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
# include "config_zkt.h"
# include "debug.h"
# include "misc.h"
#define extern
# include "zconf.h"
#undef extern
# include "dki.h"

# define	ISTRUE(val)	(strcasecmp (val, "yes") == 0 || \
				strcasecmp (val, "true") == 0    )
# define	ISCOMMENT(cp)	(*(cp) == '#' || *(cp) == ';' || \
				(*(cp) == '/' && *((cp)+1) == '/') )
# define	ISDELIM(c)	(isspace (c) || (c) == ':' || (c) == '=')


# define	cmdln	(0)
# define	first	(1)
# define	last	(0x7FFF)

# define	iscmdline(x)	((x)->used_since == cmdln)
# define	iscompatible(x)	((x)->used_since != cmdln && compversion >= (x)->used_since && \
				((x)->used_till == 1 || (compversion <= (x)->used_till)))

typedef enum {
	CONF_END = 0,
	CONF_STRING,
	CONF_INT,
	CONF_TIMEINT,
	CONF_BOOL,
	CONF_ALGO,
	CONF_SERIAL,
	CONF_FACILITY,
	CONF_LEVEL,
	CONF_NSEC3,
	CONF_COMMENT,
	CONF_VERSION,
} ctype_t;

/*****************************************************************
**	private (static) variables
*****************************************************************/
static	int	compversion;

static	zconf_t	def = {
	ZONEDIR, RECURSIVE, 
	PRINTTIME, PRINTAGE, LJUST, LSCOLORTERM,
	SIG_VALIDITY, MAX_TTL, KEY_TTL, PROPTIME, Unixtime,
	RESIGN_INT,
	KEY_ALGO, ADDITIONAL_KEY_ALGO,
	KSK_LIFETIME, KSK_BITS, KSK_RANDOM,
	ZSK_LIFETIME, ZSK_BITS, ZSK_RANDOM,
	NSEC3_OFF, SALTLEN,
	NULL, /* viewname cmdline parameter */
	0, /* noexec cmdline parameter */
	LOGFILE, LOGLEVEL, LOGDOMAINDIR, SYSLOGFACILITY, SYSLOGLEVEL, VERBOSELOG, 0,
	DNSKEYFILE, ZONEFILE, KEYSETDIR,
	LOOKASIDEDOMAIN,
	SIG_RANDOM, SIG_PSEUDO, SIG_GENDS, SIG_DNSKEY_KSK, SIG_PARAM,
	DIST_CMD,	/* defaults to NULL which means to run "rndc reload" */
	NAMED_CHROOT
};

typedef	struct {
	char	*label;		/* the name of the paramter */
	short	used_since;	/* compability (from version; 0 == command line) */
	short	used_till;	/* compability (to version) */
	ctype_t	type;		/* the parameter type */
	void	*var;		/* pointer to the parameter variable */
	const void	*var2;	/* pointer to a second parameter variable */
				/* this is a ugly hack needed by cmpconfig () */
} zconf_para_t;

static	zconf_para_t	confpara[] = {
	{ "",			first,	last,	CONF_COMMENT,	""},
	{ "",			first,	last,	CONF_COMMENT,	"\t@(#) dnssec.conf "},
	{ "",			first,	last,	CONF_VERSION,	"" },
	{ "",			first,	last,	CONF_COMMENT,	""},
	{ "",			first,	last,	CONF_COMMENT,	NULL },

	{ "",			first,	99,	CONF_COMMENT,	"dnssec-zkt options" },
	{ "",			100,	last,	CONF_COMMENT,	"zkt-ls options" },
	{ "ZoneDir",		first,	last,	CONF_STRING,	&def.zonedir },
	{ "Recursive",		first,	last,	CONF_BOOL,	&def.recursive },
	{ "PrintTime",		first,	last,	CONF_BOOL,	&def.printtime },
	{ "PrintAge",		first,	last,	CONF_BOOL,	&def.printage },
	{ "LeftJustify",	first,	last,	CONF_BOOL,	&def.ljust },
	{ "lsColor",		100,	last,	CONF_STRING,	&def.colorterm },

	{ "",			first,	last,	CONF_COMMENT,	NULL },
	{ "",			first,	last,	CONF_COMMENT,	"zone specific values" },
	{ "ResignInterval",	first,	last,	CONF_TIMEINT,	&def.resign },
	{ "SigValidity",	first,	last,	CONF_TIMEINT,	&def.sigvalidity },
	{ "Max_TTL",		first,	100,	CONF_TIMEINT,	&def.max_ttl },
	{ "MaximumTTL",		101,	last,	CONF_TIMEINT,	&def.max_ttl },
	{ "Propagation",	first,	last,	CONF_TIMEINT,	&def.proptime },
	{ "Key_TTL",		90,	100,	CONF_TIMEINT,	&def.key_ttl },
	{ "DnsKeyTTL",		101,	last,	CONF_TIMEINT,	&def.key_ttl },
#if defined (DEF_TTL)
	{ "def_ttl",		first,	last,	CONF_TIMEINT,	&def.def_ttl },
#endif
	{ "SerialFormat",	92,	last,	CONF_SERIAL,	&def.serialform },

	{ "",			first,	last,	CONF_COMMENT,	NULL },
	{ "",			first,	last,	CONF_COMMENT,	"signing key parameters"},
	{ "Key_Algo",		99,	100,	CONF_ALGO,	&def.k_algo },	/* now used as general KEY algoritjm (KSK & ZSK) */
	{ "KeyAlgo",		101,	last,	CONF_ALGO,	&def.k_algo },	/* now used as general KEY algoritjm (KSK & ZSK) */
	{ "AddKey_Algo",	99,	100,	CONF_ALGO,	&def.k2_algo },		/* second key algorithm added (v0.99) */
	{ "AddKeyAlgo",		101,	last,	CONF_ALGO,	&def.k2_algo },		/* second key algorithm added (v0.99) */
	{ "KSK_lifetime",	first,	100,	CONF_TIMEINT,	&def.k_life },
	{ "KSKlifetime",	101,	last,	CONF_TIMEINT,	&def.k_life },
	{ "KSK_algo",		first,	98,	CONF_ALGO,	&def.k_algo },	/* old KSK value changed to key algorithm */
	{ "KSK_bits",		first,	100,	CONF_INT,	&def.k_bits },
	{ "KSKbits",		101,	last,	CONF_INT,	&def.k_bits },
	{ "KSK_randfile",	first,	100,	CONF_STRING,	&def.k_random },
	{ "KSKrandfile",	101,	last,	CONF_STRING,	&def.k_random },
	{ "ZSK_lifetime",	first,	100,	CONF_TIMEINT,	&def.z_life },
	{ "ZSKlifetime",	101,	last,	CONF_TIMEINT,	&def.z_life },
	/* { "ZSK_algo",			1,	CONF_ALGO,	&def.z_algo },		ZSK algo removed (set to same as ksk) */
	{ "ZSK_algo",		first,	98,	CONF_ALGO,	&def.k2_algo },		/* if someone using it already, map the algo to the additional key algorithm */
	{ "ZSK_bits",		first,	100,	CONF_INT,	&def.z_bits },
	{ "ZSKbits",		101,	last,	CONF_INT,	&def.z_bits },
	{ "ZSK_randfile",	first,	100,	CONF_STRING,	&def.z_random },
	{ "ZSKrandfile",	101,	last,	CONF_STRING,	&def.z_random },
	{ "NSEC3",		100,	last,	CONF_NSEC3,	&def.nsec3 },
	{ "SaltBits",		98,	last,	CONF_INT,	&def.saltbits },

	{ "",			first,	last,	CONF_COMMENT,	NULL },
	{ "",			first,	99,	CONF_COMMENT,	"dnssec-signer options"},
	{ "",			100,	last,	CONF_COMMENT,	"zkt-signer options"},
	{ "--view",		cmdln,	last,	CONF_STRING,	&def.view },
	{ "--noexec",		cmdln,	last,	CONF_BOOL,	&def.noexec },
	{ "LogFile",		96,	last,	CONF_STRING,	&def.logfile },
	{ "LogLevel",		96,	last,	CONF_LEVEL,	&def.loglevel },
	{ "LogDomainDir",	96,	last,	CONF_STRING,	&def.logdomaindir },
	{ "SyslogFacility",	96,	last,	CONF_FACILITY,	&def.syslogfacility },
	{ "SyslogLevel",	96,	last,	CONF_LEVEL,	&def.sysloglevel },
	{ "VerboseLog",		96,	last,	CONF_INT,	&def.verboselog },
	{ "-v",			cmdln,	last,	CONF_INT,	&def.verbosity },
	{ "KeyFile",		first,	last,	CONF_STRING,	&def.keyfile },
	{ "ZoneFile",		first,	last,	CONF_STRING,	&def.zonefile },
	{ "KeySetDir",		first,	last,	CONF_STRING,	&def.keysetdir },
	{ "DLV_Domain",		first,	100,	CONF_STRING,	&def.lookaside },
	{ "DLVdomain",		101,	last,	CONF_STRING,	&def.lookaside },
	{ "Sig_Randfile",	first,	100,	CONF_STRING,	&def.sig_random },
	{ "SigRandfile",	101,	last,	CONF_STRING,	&def.sig_random },
	{ "Sig_Pseudorand",	first,	100,	CONF_BOOL,	&def.sig_pseudo },
	{ "SigPseudorand",	101,	last,	CONF_BOOL,	&def.sig_pseudo },
	{ "Sig_GenerateDS",	first,	100,	CONF_BOOL,	&def.sig_gends },
	{ "SigGenerateDS",	101,	last,	CONF_BOOL,	&def.sig_gends },
	{ "Sig_DnsKeyKSK",	99,	100,	CONF_BOOL,	&def.sig_dnskeyksk },
	{ "SigDnsKeyKSK",	101,	last,	CONF_BOOL,	&def.sig_dnskeyksk },
	{ "Sig_Parameter",	first,	100,	CONF_STRING,	&def.sig_param },
	{ "SigParameter",	101,	last,	CONF_STRING,	&def.sig_param },
	{ "Distribute_Cmd",	97,	100,	CONF_STRING,	&def.dist_cmd },
	{ "DistributeCmd",	101,	last,	CONF_STRING,	&def.dist_cmd },
	{ "NamedChrootDir",	99,	last,	CONF_STRING,	&def.chroot_dir },

	{ NULL,			0,	0,	CONF_END,	NULL},
};

/*****************************************************************
**	private (static) function deklaration and definition
*****************************************************************/
static	const char	*bool2str (int val)
{
	return val ? "True" : "False";
}

static	int set_varptr (char *entry, void *ptr, const void *ptr2)
{
	zconf_para_t	*c;

	for ( c = confpara; c->label; c++ )
		if ( strcasecmp (entry, c->label) == 0 )
		{
			c->var = ptr;
			c->var2 = ptr2;
			return 1;
		}
	return 0;
}

static	void set_all_varptr (zconf_t *cp, const zconf_t *cp2)
{
	set_varptr ("zonedir", &cp->zonedir, cp2 ? &cp2->zonedir: NULL);
	set_varptr ("recursive", &cp->recursive, cp2 ? &cp2->recursive: NULL);
	set_varptr ("printage", &cp->printage, cp2 ? &cp2->printage: NULL);
	set_varptr ("printtime", &cp->printtime, cp2 ? &cp2->printtime: NULL);
	set_varptr ("leftjustify", &cp->ljust, cp2 ? &cp2->ljust: NULL);
	set_varptr ("lscolor", &cp->colorterm, cp2 ? &cp2->colorterm: NULL);

	set_varptr ("resigninterval", &cp->resign, cp2 ? &cp2->resign: NULL);
	set_varptr ("sigvalidity", &cp->sigvalidity, cp2 ? &cp2->sigvalidity: NULL);
	set_varptr ("max_ttl", &cp->max_ttl, cp2 ? &cp2->max_ttl: NULL);
	set_varptr ("key_ttl", &cp->key_ttl, cp2 ? &cp2->key_ttl: NULL);
	set_varptr ("propagation", &cp->proptime, cp2 ? &cp2->proptime: NULL);
#if defined (DEF_TTL)
	set_varptr ("def_ttl", &cp->def_ttl, cp2 ? &cp2->def_ttl: NULLl);
#endif
	set_varptr ("serialformat", &cp->serialform, cp2 ? &cp2->serialform: NULL);

	set_varptr ("key_algo", &cp->k_algo, cp2 ? &cp2->k_algo: NULL);
	set_varptr ("addkey_algo", &cp->k2_algo, cp2 ? &cp2->k2_algo: NULL);
	set_varptr ("ksk_lifetime", &cp->k_life, cp2 ? &cp2->k_life: NULL);
	set_varptr ("ksk_algo", &cp->k_algo, cp2 ? &cp2->k_algo: NULL);		/* used only in compability mode */
	set_varptr ("ksk_bits", &cp->k_bits, cp2 ? &cp2->k_bits: NULL);
	set_varptr ("ksk_randfile", &cp->k_random, cp2 ? &cp2->k_random: NULL);

	set_varptr ("zsk_lifetime", &cp->z_life, cp2 ? &cp2->z_life: NULL);
	// set_varptr ("zsk_algo", &cp->z_algo, cp2 ? &cp2->z_algo: NULL);
	set_varptr ("zsk_algo", &cp->k2_algo, cp2 ? &cp2->k2_algo: NULL);
	set_varptr ("zsk_bits", &cp->z_bits, cp2 ? &cp2->z_bits: NULL);
	set_varptr ("zsk_randfile", &cp->z_random, cp2 ? &cp2->z_random: NULL);
	set_varptr ("nsec3", &cp->nsec3, cp2 ? &cp2->nsec3: NULL);
	set_varptr ("saltbits", &cp->saltbits, cp2 ? &cp2->saltbits: NULL);

	set_varptr ("--view", &cp->view, cp2 ? &cp2->view: NULL);
	set_varptr ("--noexec", &cp->noexec, cp2 ? &cp2->noexec: NULL);
	set_varptr ("logfile", &cp->logfile, cp2 ? &cp2->logfile: NULL);
	set_varptr ("loglevel", &cp->loglevel, cp2 ? &cp2->loglevel: NULL);
	set_varptr ("logdomaindir", &cp->logdomaindir, cp2 ? &cp2->logdomaindir: NULL);
	set_varptr ("syslogfacility", &cp->syslogfacility, cp2 ? &cp2->syslogfacility: NULL);
	set_varptr ("sysloglevel", &cp->sysloglevel, cp2 ? &cp2->sysloglevel: NULL);
	set_varptr ("verboselog", &cp->verboselog, cp2 ? &cp2->verboselog: NULL);
	set_varptr ("-v", &cp->verbosity, cp2 ? &cp2->verbosity: NULL);
	set_varptr ("keyfile", &cp->keyfile, cp2 ? &cp2->keyfile: NULL);
	set_varptr ("zonefile", &cp->zonefile, cp2 ? &cp2->zonefile: NULL);
	set_varptr ("keysetdir", &cp->keysetdir, cp2 ? &cp2->keysetdir: NULL);
	set_varptr ("dlv_domain", &cp->lookaside, cp2 ? &cp2->lookaside: NULL);
	set_varptr ("sig_randfile", &cp->sig_random, cp2 ? &cp2->sig_random: NULL);
	set_varptr ("sig_pseudorand", &cp->sig_pseudo, cp2 ? &cp2->sig_pseudo: NULL);
	set_varptr ("sig_generateds", &cp->sig_gends, cp2 ? &cp2->sig_gends: NULL);
	set_varptr ("sig_dnskeyksk", &cp->sig_dnskeyksk, cp2 ? &cp2->sig_dnskeyksk: NULL);
	set_varptr ("sig_parameter", &cp->sig_param, cp2 ? &cp2->sig_param: NULL);
	set_varptr ("distribute_cmd", &cp->dist_cmd, cp2 ? &cp2->dist_cmd: NULL);
	set_varptr ("namedchrootdir", &cp->chroot_dir, cp2 ? &cp2->chroot_dir: NULL);
}

static	void	parseconfigline (char *buf, unsigned int line, zconf_t *z)
{
	char		*end, *val, *p;
	char		*tag;
	unsigned int	len, found;
	zconf_para_t	*c;

	assert (buf[0] != '\0');

	p = &buf[strlen(buf)-1];        /* Chop off white space at eol */
	while ( p >= buf && isspace (*p) )
		*p-- = '\0';

	for (p = buf; isspace (*p); p++ )	/* Ignore leading white space */
		;
	
	/* Ignore comments and emtpy lines */
	if ( *p == '\0' || ISCOMMENT (p) )
		return;

	tag = p;
	/* Get the end of the first argument */
	end = &buf[strlen(buf)-1];
	while ( p < end && !ISDELIM (*p) )      /* Skip until delim */
		p++;
	*p++ = '\0';    /* Terminate this argument */
	dbg_val1 ("Parsing \"%s\"\n", tag);

	while ( p < end && ISDELIM (*p) )	/* Skip delim chars */
		p++;

	val = p;	/* Start of the value */
	dbg_val1 ("\tgot value \"%s\"\n", val);

	/* If starting with quote, skip until next quote */
	if ( *p == '"' || *p == '\'' )
	{
		p++;    /* Find next quote */
		while ( p <= end && *p && *p != *val )
			p++;
		*p = '\0';
		val++;          /* Skip the first quote */
	}
	else    /* Otherwise check if there is any comment char at the end */
	{
		while ( p < end && *p && !ISCOMMENT(p) )
			p++;
		if ( ISCOMMENT (p) )
		{
			do      /* Chop off white space before comment */
				*p-- = '\0';
			while ( p >= val && isspace (*p) );
		}
	}

	/* Otherwise it is already terminated above */
	found = 0;
	c = confpara;
	while ( !found && c->type != CONF_END )
	{
		len = strlen (c->label);
		if ( strcasecmp (tag, c->label) == 0 )
		{
			char	**str;
			char	quantity;
			long	lval;

			found = 1;
			switch ( c->type )
			{
			case CONF_VERSION:
				break;
			case CONF_LEVEL:
			case CONF_FACILITY:
			case CONF_STRING:
				str = (char **)c->var;
				*str = strdup (val);
				str_untaint (*str);	/* remove "bad" characters */
				break;
			case CONF_INT:
				sscanf (val, "%d", (int *)c->var);
				break;
			case CONF_TIMEINT:
				quantity = 'd';
				if ( *val == 'u' || *val == 'U' )
					lval = 0L;
				else
					sscanf (val, "%ld%c", &lval, &quantity);
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
				(*(long *)c->var) = lval;
				break;
			case CONF_ALGO:
				if ( strcmp (val, "1") == 0 || strcasecmp (val, "rsa") == 0 ||
								strcasecmp (val, "rsamd5") == 0 )
					*((int *)c->var) = DK_ALGO_RSA;
				else if ( strcmp (val, "3") == 0 ||
					  strcasecmp (val, "dsa") == 0 )
					*((int *)c->var) = DK_ALGO_DSA;
				else if ( strcmp (val, "5") == 0 ||
					  strcasecmp (val, "rsasha1") == 0 )
					*((int *)c->var) = DK_ALGO_RSASHA1;
				else if ( strcmp (val, "6") == 0 ||
					  strcasecmp (val, "nsec3dsa") == 0 ||
				          strcasecmp (val, "n3dsa") == 0 )
					*((int *)c->var) = DK_ALGO_NSEC3DSA;
				else if ( strcmp (val, "7") == 0 ||
					  strcasecmp (val, "nsec3rsasha1") == 0 ||
					  strcasecmp (val, "n3rsasha1") == 0 )
					*((int *)c->var) = DK_ALGO_NSEC3RSASHA1;
#if defined(BIND_VERSION) && BIND_VERSION >= 970
				else if ( strcmp (val, "8") == 0 ||
					  strcasecmp (val, "rsasha2") == 0 ||
				          strcasecmp (val, "rsasha256") == 0 ||
					  strcasecmp (val, "nsec3rsasha2") == 0 ||
					  strcasecmp (val, "n3rsasha2") == 0 ||
					  strcasecmp (val, "nsec3rsasha256") == 0 ||
					  strcasecmp (val, "n3rsasha256") == 0 )
					*((int *)c->var) = DK_ALGO_RSASHA256;
				else if ( strcmp (val, "10") == 0 ||
					  strcasecmp (val, "rsasha5") == 0 ||
				          strcasecmp (val, "rsasha212") == 0 ||
					  strcasecmp (val, "nsec3rsasha5") == 0 ||
					  strcasecmp (val, "n3rsasha5") == 0 ||
					  strcasecmp (val, "nsec3rsasha512") == 0 ||
					  strcasecmp (val, "n3rsasha512") == 0 )
					*((int *)c->var) = DK_ALGO_RSASHA512;
#endif
				else
					error ("Illegal algorithm \"%s\" "
						"in line %d.\n" , val, line);
				break;
			case CONF_SERIAL:
				if ( strcasecmp (val, "unixtime") == 0 )
					*((serial_form_t *)c->var) = Unixtime;
				else if ( strcasecmp (val, "incremental") == 0 || strcasecmp (val, "inc") == 0 )
					*((serial_form_t *)c->var) = Incremental;
				else
					error ("Illegal serial no format \"%s\" "
						"in line %d.\n" , val, line);
				break;
			case CONF_NSEC3:
				if ( strcasecmp (val, "off") == 0 )
					*((nsec3_t *)c->var) = NSEC3_OFF;
				else if ( strcasecmp (val, "on") == 0 )
					*((nsec3_t *)c->var) = NSEC3_ON;
				else if ( strcasecmp (val, "optout") == 0 )
					*((nsec3_t *)c->var) = NSEC3_OPTOUT;
				else
					error ("Illegal NSEC3 format \"%s\" "
						"in line %d.\n" , val, line);
				break;
			case CONF_BOOL:
				*((int *)c->var) = ISTRUE (val);
				break;
			default:
				fatal ("Illegal configuration type in line %d.\n", line);
			}
		}
		c++;
	}
	if ( !found )
		error ("Unknown configuration statement: %s \"%s\"\n", tag, val);
	return;
}

static	void	printconfigline (FILE *fp, zconf_para_t *cp)
{
	int	i;
	long	lval;

	assert (fp != NULL);
	assert (cp != NULL);

	switch ( cp->type )
	{
	case CONF_VERSION:
			fprintf (fp, "#\tZKT config file for version %d.%02d\n", 
						compversion / 100, compversion % 100);
		break;
	case CONF_COMMENT:
		if ( cp->var )
			fprintf (fp, "#   %s\n", (char *)cp->var);
		else
			fprintf (fp, "\n");
		break;
	case CONF_LEVEL:
	case CONF_FACILITY:
		if ( *(char **)cp->var != NULL )
		{
			if ( **(char **)cp->var != '\0' )
			{
				char	*p;

				fprintf (fp, "%s:\t", cp->label);
				for ( p = *(char **)cp->var; *p; p++ )
					putc (toupper (*p), fp);
				fprintf (fp, "\n");
			}
			else
				fprintf (fp, "%s:\tNONE", cp->label);
		}
		break;
	case CONF_STRING:
		if ( *(char **)cp->var )
			fprintf (fp, "%s:\t\"%s\"\n", cp->label, *(char **)cp->var);
		break;
	case CONF_BOOL:
		fprintf (fp, "%s:\t%s\n", cp->label, bool2str ( *(int*)cp->var ));
		break;
	case CONF_TIMEINT:
		lval = *(ulong*)cp->var;	/* in that case it should be of type ulong */
		fprintf (fp, "%s:\t%s", cp->label, timeint2str (lval));
		if ( lval )
			fprintf (fp, "\t\t# (%ld seconds)", lval);
		putc ('\n', fp);
		break;
	case CONF_ALGO:
		i = *(int*)cp->var;
		if ( i )
		{
			fprintf (fp, "%s:\t%s ", cp->label, dki_algo2str (i));
			fprintf (fp, "\t# (Algorithm ID %d)\n", i);
		}
		break;
	case CONF_SERIAL:
		fprintf (fp, "%s:\t", cp->label);
		if ( *(serial_form_t*)cp->var == Unixtime )
			fprintf (fp, "UnixTime");
		else
			fprintf (fp, "Incremental");
		fprintf (fp, "\t# (UnixTime|Incremental)\n");
		break;
	case CONF_NSEC3:
		fprintf (fp, "%s:\t\t", cp->label);
		if ( *(nsec3_t*)cp->var == NSEC3_OFF )
			fprintf (fp, "Off");
		else if ( *(nsec3_t*)cp->var == NSEC3_ON )
			fprintf (fp, "On");
		else if ( *(nsec3_t*)cp->var == NSEC3_OPTOUT )
			fprintf (fp, "OptOut");
		fprintf (fp, "\t\t# (On|Off|OptOut)\n");
		break;
	case CONF_INT:
		fprintf (fp, "%s:\t%d\n", cp->label, *(int *)cp->var);
		break;
	case CONF_END:
		/* NOTREACHED */
		break;
	}
}

/*****************************************************************
**	public function definition
*****************************************************************/

void	setconfigversion (int version)
{
	compversion = version;
}

const char	*timeint2str (unsigned long val)
{
	static	char	str[20+1];

	if ( val == 0 )
		snprintf (str, sizeof (str), "Unset");
	else if ( val % YEARSEC == 0 )
		snprintf (str, sizeof (str), "%luy", val / YEARSEC);
	else if ( val % WEEKSEC == 0 )
		snprintf (str, sizeof (str), "%luw", val / WEEKSEC);
	else if ( val % DAYSEC == 0 )
		snprintf (str, sizeof (str), "%lud", val / DAYSEC);
	else if ( val % HOURSEC == 0 )
		snprintf (str, sizeof (str), "%luh", val / HOURSEC);
	else if ( val % MINSEC == 0 )
		snprintf (str, sizeof (str), "%lum", val / MINSEC);
	else
		snprintf (str, sizeof (str), "%lus", val);

	return str;
}


/*****************************************************************
**	loadconfig (file, conf)
**	Loads a config file into the "conf" structure pointed to by "z".
**	If "z" is NULL then a new conf struct will be dynamically
**	allocated.
**	If no filename is given the conf struct will be initialized
**	with the builtin default config
*****************************************************************/
zconf_t	*loadconfig (const char *filename, zconf_t *z)
{
	FILE		*fp;
	char		buf[1023+1];
	unsigned int	line;

	if ( z == NULL )	/* allocate new memory for zconf_t */
	{
		if ( (z = calloc (1, sizeof (zconf_t))) == NULL )
			return NULL;

		if ( filename && *filename )
			memcpy (z, &def, sizeof (zconf_t));	/* init new struct with defaults */
	}

	if ( filename == NULL || *filename == '\0' )	/* no file name given... */
	{
		dbg_val0("loadconfig (NULL)\n");
		memcpy (z, &def, sizeof (zconf_t));		/* ..then init with defaults */
		return z;
	}

	dbg_val1 ("loadconfig (%s)\n", filename);
	set_all_varptr (z, NULL);

	if ( (fp = fopen(filename, "r")) == NULL )
		fatal ("Could not open config file \"%s\"\n", filename);

	line = 0;
	while (fgets(buf, sizeof(buf), fp))
		parseconfigline (buf, ++line, z);

	fclose(fp);
	return z;
}

# define	STRCONFIG_DELIMITER	";\r\n"
zconf_t	*loadconfig_fromstr (const char *str, zconf_t *z)
{
	char		*buf;
	char		*tok,	*toksave;
	unsigned int	line;

	if ( z == NULL )
	{
		if ( (z = calloc (1, sizeof (zconf_t))) == NULL )
			return NULL;
		memcpy (z, &def, sizeof (zconf_t));		/* init with defaults */
	}

	if ( str == NULL || *str == '\0' )
	{
		dbg_val0("loadconfig_fromstr (NULL)\n");
		memcpy (z, &def, sizeof (zconf_t));		/* init with defaults */
		return z;
	}

	dbg_val1 ("loadconfig_fromstr (\"%s\")\n", str);
	set_all_varptr (z, NULL);

	/* str is const, so we have to copy it into a new buffer */
	if ( (buf = strdup (str)) == NULL )
		fatal ("loadconfig_fromstr: Out of memory");

	line = 0;
	tok = strtok_r (buf, STRCONFIG_DELIMITER, &toksave);
	while ( tok )
	{
		line++;
		parseconfigline (tok, line, z);
		tok = strtok_r (NULL, STRCONFIG_DELIMITER, &toksave);
	}
	free (buf);
	return z;
}

/*****************************************************************
**	dupconfig (config)
**	duplicate config struct and return a ptr to the new struct
*****************************************************************/
zconf_t	*dupconfig (const zconf_t *conf)
{
	zconf_t	*z;

	assert (conf != NULL);

	if ( (z = calloc (1, sizeof (zconf_t))) == NULL )
		return NULL;

	memcpy (z, conf, sizeof (zconf_t));

	return z;
}

/*****************************************************************
**	freeconfig (config)
**	free memory for config struct and return a NULL ptr
*****************************************************************/
zconf_t	*freeconfig (zconf_t *conf)
{
	if (conf != NULL);
		free (conf);

	return (zconf_t *)NULL;
}

/*****************************************************************
**	setconfigpar (entry, pval)
*****************************************************************/
int	setconfigpar (zconf_t *config, char *entry, const void *pval)
{
	char	*str;
	zconf_para_t	*c;

	set_all_varptr (config, NULL);

	for ( c = confpara; c->type != CONF_END; c++ )
		if ( strcasecmp (entry, c->label) == 0 )
		{
			switch ( c->type )
			{
			case CONF_VERSION:
				break;
			case CONF_LEVEL:
			case CONF_FACILITY:
			case CONF_STRING:
				if ( pval )
				{
					str = strdup ((char *)pval);
					str_untaint (str);	/* remove "bad" characters */
				}
				else
					str = NULL;
				*((char **)c->var) = str;
				break;
			case CONF_BOOL:
				/* fall through */
			case CONF_ALGO:
				/* fall through */
			case CONF_INT:
				*((int *)c->var) = *((int *)pval);
				break;
			case CONF_TIMEINT:
				*((long *)c->var) = *((long *)pval);
				break;
			case CONF_NSEC3:
				*((nsec3_t *)c->var) = *((nsec3_t *)pval);
				break;
			case CONF_SERIAL:
				*((serial_form_t *)c->var) = *((serial_form_t *)pval);
				break;
			case CONF_COMMENT:
			case CONF_END:
				/* NOTREACHED */
				break;
			}
			return 1;
		}
	return 0;
}

/*****************************************************************
**	printconfig (fname, config)
*****************************************************************/
int	printconfig (const char *fname, const zconf_t *z)
{
	zconf_para_t	*cp;
	FILE	*fp;

	if ( z == NULL )
		return 0;

	fp = stdout;
	if ( fname && *fname )
	{
		if ( strcmp (fname, "stdout") == 0 )
			fp = stdout;
		else if ( strcmp (fname, "stderr") == 0 )
			fp = stderr;
		else if ( (fp = fopen(fname, "w")) == NULL )
		{
			error ("Could not open config file \"%s\" for writing\n", fname);
			return -1;
		}
	}
		
	set_all_varptr ((zconf_t *)z, NULL);

	for ( cp = confpara; cp->type != CONF_END; cp++ )	/* loop through all parameter */
		if ( iscompatible (cp) )	/* is parameter compatible to current version? */
			printconfigline (fp, cp);	/* print it out */

	if ( fp && fp != stdout && fp != stderr )
		fclose (fp);

	return 1;
}

/*****************************************************************
**	printconfigdiff (fname, conf_a, conf_b)
*****************************************************************/
int	printconfigdiff (const char *fname, const zconf_t *ref, const zconf_t *z)
{
	zconf_para_t	*cp;
	int	eq;
	char	*p1,	*p2;
	FILE	*fp;

	if ( ref == NULL || z == NULL )
		return 0;

	fp = NULL;
	if ( fname && *fname )
	{
		if ( strcmp (fname, "stdout") == 0 )
			fp = stdout;
		else if ( strcmp (fname, "stderr") == 0 )
			fp = stderr;
		else if ( (fp = fopen(fname, "w")) == NULL )
		{
			error ("Could not open config file \"%s\" for writing\n", fname);
			return -1;
		}
	}
		
	set_all_varptr ((zconf_t *)z, ref);

	for ( cp = confpara; cp->type != CONF_END; cp++ )	/* loop through all parameter */
	{
		eq = 0;
		if ( iscmdline (cp) )	/* skip command line parameter */
			continue;

		switch ( cp->type )
		{
		case CONF_VERSION:
		case CONF_END:
		case CONF_COMMENT:
			continue;
		case CONF_NSEC3:
			eq = ( *(nsec3_t *)cp->var == *(nsec3_t *)cp->var2 );
			break;
		case CONF_SERIAL:
			eq = ( *(serial_form_t *)cp->var == *(serial_form_t *)cp->var2 );
			break;
		case CONF_BOOL:
		case CONF_ALGO:
		case CONF_INT:
			eq = ( *(int *)cp->var == *(int *)cp->var2 );
			break;
		case CONF_TIMEINT:
			eq = ( *(long *)cp->var == *(long *)cp->var2 );
			break;
		case CONF_LEVEL:
		case CONF_FACILITY:
		case CONF_STRING:
			p1 = *(char **)cp->var;
			p2 = *(char **)cp->var2;
			if ( p1 && p2 )
				eq = strcmp (p1, p2) == 0;
			else if ( p1 == NULL || p2 == NULL )
				eq = 0;
			else
				eq = 1;
		}
		if ( !eq )
			printconfigline (fp, cp);	/* print it out */
	}

	if ( fp && fp != stdout && fp != stderr )
		fclose (fp);

	return 1;
}

/*****************************************************************
**	checkconfig (config)
*****************************************************************/
int	checkconfig (const zconf_t *z)
{
	int	ret;
	long	max_ttl;

	if ( z == NULL )
		return 1;

	max_ttl = z->max_ttl;
	if ( max_ttl <= 0 )
		max_ttl = z->sigvalidity;

	ret = 0;
	if ( strcmp (z->k_random, "/dev/urandom") == 0 )
		ret = fprintf (stderr, "random device without enough entropie used for KSK generation \n");
	if ( strcmp (z->z_random, "/dev/urandom") == 0 )
		ret = fprintf (stderr, "random device without enough entropie used for ZSK generation\n");

	if ( z->saltbits < 4 )
		ret = fprintf (stderr, "Saltlength must be at least 4 bits\n");
	if ( z->saltbits > 128 )
	{
		fprintf (stderr, "While the maximum is 520 bits of salt, it's not recommended to use more than 128 bits.\n");
		ret = fprintf (stderr, "The current value is %d bits\n", z->saltbits);
	}

	if ( z->sigvalidity < (1 * DAYSEC) || z->sigvalidity > (12 * WEEKSEC) )
	{
		fprintf (stderr, "Signature should be valid for at least 1 day and no longer than 3 month (12 weeks)\n");
		ret = fprintf (stderr, "The current value is %s\n", timeint2str (z->sigvalidity));
	}

	if ( z->max_ttl <= 0 )
	{
		ret = fprintf (stderr, "The max TTL is unknown which results in suboptimal key rollover.\n");
		fprintf (stderr, "Please set max_ttl to the maximum ttl used in the zone (run zkt-conf -w zone.db)\n");
	}
	else
		if ( max_ttl > z->sigvalidity/2 )
			ret = fprintf (stderr, "Max TTL (%ld) should be less or equal signature validity (%ld)\n",
								max_ttl, z->sigvalidity);

	// if ( z->resign > (z->sigvalidity*5/6) - (max_ttl + z->proptime) )
	if ( z->resign > (z->sigvalidity*5/6) )
	{
		fprintf (stderr, "Re-signing interval (%s) should be less than ", timeint2str (z->resign));
		ret = fprintf (stderr, "5/6 of sigvalidity (%s)\n", timeint2str (z->sigvalidity));
	}

	if ( z->max_ttl > 0 && z->resign > (z->sigvalidity - max_ttl) )
	{
		fprintf (stderr, "Re-signing interval (%s) should be ", timeint2str (z->resign));
		fprintf (stderr, "end at least one max_ttl (%ld) before the end of ", max_ttl);
		ret = fprintf (stderr, "signature lifetime (%ld) (%s)\n", z->sigvalidity, timeint2str(z->sigvalidity - max_ttl));
	}

	if ( z->z_life > (12 * WEEKSEC) * (z->z_bits / 512.) )
	{
		fprintf (stderr, "Lifetime of zone signing key (%s) ", timeint2str (z->z_life));
		fprintf (stderr, "seems a little bit high ");
		ret = fprintf (stderr, "(In respect of key size (%d))\n", z->z_bits);
	}

	if ( z->k_life > 0 && z->k_life <= z->z_life )
	{
		fprintf (stderr, "Lifetime of key signing key (%s) ", timeint2str (z->k_life));
		ret = fprintf (stderr, "should be greater than lifetime of zsk\n");
	}
	if ( z->k_life > 0 && z->k_life > (26 * WEEKSEC) * (z->k_bits / 512.) )
	{
		fprintf (stderr, "Lifetime of key signing key (%s) ", timeint2str (z->k_life));
		fprintf (stderr, "seems a little bit high ");
		ret = fprintf (stderr, "(In respect of key size (%d))\n", z->k_bits);
	}

	return !ret;
}

#ifdef CONF_TEST
const char *progname;
static	zconf_t	*config;

main (int argc, char *argv[])
{
	char	*optstr;
	int	val;

	progname = *argv;

	config = loadconfig ("", (zconf_t *) NULL);	/* load built in defaults */

	while ( --argc >= 1 )
	{
		optstr = *++argv;
		config = loadconfig_fromstr (optstr, config);
	}

	val = 1;
	setconfigpar (config, "-v", &val);
	val = 2;
	setconfigpar (config, "verboselog", &val);
	val = 1;
	setconfigpar (config, "recursive", &val);
	val = 1200;
	setconfigpar (config, "propagation", &val);
	
	printconfig ("stdout", config);
}
#endif
