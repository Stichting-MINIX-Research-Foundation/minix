/*****************************************************************
**
**	@(#) zconf.h  
**
**	Copyright (c) Jan 2005, Jeroen Masar, Holger Zuleger.
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
**	Neither the name of Jeroen Masar and Holger Zuleger nor the
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
*****************************************************************/
#ifndef ZCONF_H
# define ZCONF_H


# define	MINSEC	60L
# define	HOURSEC	(MINSEC * 60)
# define	DAYSEC	(HOURSEC * 24)
# define	WEEKSEC	(DAYSEC * 7)
# define	YEARSEC	(DAYSEC * 365)
# define	DAY	(1)
# define	WEEK	(DAY * 7)
# define	MONTH	(DAY * 30)
# define	YEAR	(DAY * 365)

# define	SIG_VALID_DAYS	(10)	/* or 3 Weeks ? */
# define	SIG_VALIDITY	(SIG_VALID_DAYS * DAYSEC)
# define	MAX_TTL		( 8 * HOURSEC)	/* default value of maximum ttl time */
# define	KEY_TTL		( 4 * HOURSEC)	/* default value of KEY TTL */
# define	PROPTIME	( 5 * MINSEC)	/* expected slave propagation time */
						/* should be small if notify is used  */
#if defined (DEF_TTL)
# define	DEF_TTL		(MAX_TTL/2)	/* currently not used */
#endif

# define	RESIGN_INT	((SIG_VALID_DAYS - (SIG_VALID_DAYS / 3)) * DAYSEC)
# define	KSK_LIFETIME	(1 * YEARSEC)
#if 0
# define	ZSK_LIFETIME	((SIG_VALID_DAYS * 3) * DAYSEC)	/* set to three times the sig validity */
#else
# if 0
#  define	ZSK_LIFETIME	((MONTH * 3) * DAYSEC)	/* set fixed to 3 month */
# else
#  define	ZSK_LIFETIME	(12 * WEEKSEC)	/* set fixed to 3 month */
# endif
#endif

/* # define	KSK_ALGO	(DK_ALGO_RSASHA1)	KSK_ALGO renamed to KEY_ALGO (v0.99) */
# define	KEY_ALGO	(DK_ALGO_RSASHA1)	/* general KEY_ALGO used for both ksk and zsk */
# define	ADDITIONAL_KEY_ALGO	0
# define	KSK_BITS	(1300)
# define	KSK_RANDOM	"/dev/urandom"	/* was NULL before v0.94 */
/* # define	ZSK_ALGO	(DK_ALGO_RSASHA1)	ZSK_ALGO has to be the same as KSK, so this is no longer used (v0.99) */
# define	ZSK_BITS	(512)
# define	ZSK_RANDOM	"/dev/urandom"
# define	NSEC3		0		/* by default nsec3 is off */
# define	SALTLEN		24		/* salt length in bits (resolution is 4 bits)*/

# define	ZONEDIR		"."
# define	RECURSIVE	0
# define	PRINTTIME	1
# define	PRINTAGE	0
# define	LJUST		0
# define	LSCOLORTERM	NULL	/* or "" */
# define	KEYSETDIR	NULL	/* keysets */
# define	LOGFILE		""
# define	LOGLEVEL	"error"
# define	LOGDOMAINDIR	""
# define	SYSLOGFACILITY	"none"
# define	SYSLOGLEVEL	"notice"
# define	VERBOSELOG	0
# define	ZONEFILE	"zone.db"
# define	DNSKEYFILE	"dnskey.db"
# define	LOOKASIDEDOMAIN	""	/* "dlv.trusted-keys.de" */
# define	SIG_RANDOM	NULL	/* "/dev/urandom" */
# define	SIG_PSEUDO	0
# define	SIG_GENDS	1
# define	SIG_DNSKEY_KSK	0	/* Sign DNSKEY RR with KSK only */
# define	SIG_PARAM	""
# define	DIST_CMD	NULL	/* default is to run "rndc reload" */
# define	NAMED_CHROOT	NULL	/* default is none */

#ifndef CONFIG_PATH
# define	CONFIG_PATH	"/var/named/"
#endif
# define	CONFIG_FILE	CONFIG_PATH "dnssec.conf"
# define	LOCALCONF_FILE	"dnssec.conf"

/* external command execution path (should be set via config.h) */
#ifndef BIND_UTIL_PATH
# define BIND_UTIL_PATH	"/usr/local/sbin/"	/* beware of trailing '/' */
#endif
# define	SIGNCMD		BIND_UTIL_PATH "dnssec-signzone"
# define	KEYGENCMD	BIND_UTIL_PATH "dnssec-keygen"
# define	RELOADCMD	BIND_UTIL_PATH "rndc"

typedef	enum {
	Unixtime = 1,
	Incremental
} serial_form_t;

typedef	enum {
	NSEC3_OFF = 0,
	NSEC3_ON,
	NSEC3_OPTOUT
} nsec3_t;

typedef	enum {
	none = 0,
	user,
	local0, local1, local2, local3, local4, local5, local6, local7
} syslog_facility_t;

typedef	struct zconf	{
	char	*zonedir;
	int	recursive;
	int	printtime;
	int	printage;
	int	ljust;
	char	*colorterm;
	long	sigvalidity;	/* should be less than expire time */
	long	max_ttl;	/* should be set to the maximum used ttl in the zone */
	long	key_ttl;
	long	proptime;	/* expected time offset for zone propagation */
#if defined (DEF_TTL)
	long	def_ttl;	/* default ttl set in soa record  */
#endif
	serial_form_t	serialform;	/* format of serial no */
	long	resign;		/* resign interval */

	int	k_algo;
	int	k2_algo;
	long	k_life;
	int	k_bits;
	char	*k_random;
	long	z_life;
	/* int	z_algo;		no longer used; renamed to k2_algo (v0.99) */
	int	z_bits;
	char	*z_random;
	nsec3_t	nsec3;		/* 0 == off; 1 == on; 2 == on with optout */
	int	saltbits;

	char	*view;
	int	noexec;
	// char	*errlog;
	char	*logfile;
	char	*loglevel;
	char	*logdomaindir;
	char	*syslogfacility;
	char	*sysloglevel;
	int	verboselog;
	int	verbosity;
	char	*keyfile;
	char	*zonefile;
	char	*keysetdir;
	char	*lookaside;
	char	*sig_random;
	int	sig_pseudo;
	int	sig_gends;
	int	sig_dnskeyksk;
	char	*sig_param;
	char	*dist_cmd;	/* cmd to run instead of "rndc reload" */
	char	*chroot_dir;	/* chroot directory of named */
} zconf_t;

extern	const char	*timeint2str (unsigned long val);
extern	zconf_t	*loadconfig (const char *filename, zconf_t *z);
extern	zconf_t	*loadconfig_fromstr (const char *str, zconf_t *z);
extern	zconf_t	*dupconfig (const zconf_t *conf);
extern	zconf_t	*freeconfig (zconf_t *conf);
extern	int	setconfigpar (zconf_t *conf, char *entry, const void *pval);
extern	int	printconfig (const char *fname, const zconf_t *cp);
extern	int	printconfigdiff (const char *fname, const zconf_t *ref, const zconf_t *z);
extern	int	checkconfig (const zconf_t *z);
extern	void	setconfigversion (int version);

#endif
