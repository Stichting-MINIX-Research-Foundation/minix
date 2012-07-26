/*****************************************************************
**
**	@(#) dki.h -- Header file for DNSsec Key info/manipulation
**
**	Copyright (c) July 2004 - Jan 2005, Holger Zuleger HZnet. All rights reserved.
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
#ifndef DKI_H
# define DKI_H

# ifndef TYPES_H
#  include <sys/types.h>
#  include <stdio.h>
#  include <time.h>
# endif

# define	MAX_LABELSIZE	(255)	
# define	MAX_FNAMESIZE	(1+255+2+3+1+5+1+11)
				/* Kdomain.+ALG+KEYID.type  */
				/* domain == FQDN (max 255) */
				/* ALG == 3; KEYID == 5 chars */
				/* type == key||published|private|depreciated == 11 chars */
//# define	MAX_DNAMESIZE	(254)
# define	MAX_DNAMESIZE	(1023)
				/*   /path/name  /   filename  */
# define	MAX_PATHSIZE	(MAX_DNAMESIZE + 1 + MAX_FNAMESIZE)

/* algorithm types */
# define	DK_ALGO_RSA		1	/* RFC2537 */
# define	DK_ALGO_DH		2	/* RFC2539 */
# define	DK_ALGO_DSA		3	/* RFC2536 (mandatory) */
# define	DK_ALGO_EC		4	/* */
# define	DK_ALGO_RSASHA1		5	/* RFC3110 */
# define	DK_ALGO_NSEC3DSA	6	/* symlink to alg 3 RFC5155 */
# define	DK_ALGO_NSEC3RSASHA1	7	/* symlink to alg 5 RFC5155 */
# define	DK_ALGO_RSASHA256	8	/* RFCxxx */
# define	DK_ALGO_RSASHA512	10	/* RFCxxx */
# define	DK_ALGO_NSEC3RSASHA256	DK_ALGO_RSASHA256	/* same as non nsec algorithm RFCxxx */
# define	DK_ALGO_NSEC3RSASHA512	DK_ALGO_RSASHA512	/* same as non nsec algorithm RFCxxx */

/* protocol types */
# define	DK_PROTO_DNS	3

/* flag bits */
typedef enum {			/*             11 1111 */
				/* 0123 4567 8901 2345 */
	DK_FLAG_KSK=	01,	/* 0000 0000 0000 0001	Bit 15 RFC4034/RFC3757 */
	DK_FLAG_REVOKE=	0200,	/* 0000 0000 1000 0000	Bit 8  RFC5011 */
	DK_FLAG_ZONE=	0400,	/* 0000 0001 0000 0000	Bit 7  RFC4034 */
} dk_flag_t;

/* status types */
typedef enum {
	DKI_SEP=	'e',
	DKI_SECUREENTRYPOINT=	'e',
	DKI_PUB=	'p',
	DKI_PUBLISHED=	'p',
	DKI_ACT=	'a',
	DKI_ACTIVE=	'a',
	DKI_DEP=	'd',
	DKI_DEPRECIATED=	'd',
	DKI_REV=	'r',
	DKI_REVOKED=	'r',
} dk_status_t;

# define	DKI_KEY_FILEEXT	".key"
# define	DKI_PUB_FILEEXT	".published"
# define	DKI_ACT_FILEEXT	".private"
# define	DKI_DEP_FILEEXT	".depreciated"

# define	DKI_KSK	1
# define	DKI_ZSK	0

typedef	struct	dki {
	char	dname[MAX_DNAMESIZE+1];	/* directory */
	char	fname[MAX_FNAMESIZE+1];	/* file name without extension */
	char	name[MAX_LABELSIZE+1];	/* domain name or label */
	ushort	algo;			/* key algorithm */
	ushort	proto;			/* must be 3 (DNSSEC) */
	dk_flag_t	flags;		/* ZONE, optional SEP or REVOKE flag */
	time_t	time;			/* key file time */
	time_t	gentime;		/* key generation time (will be set on key generation and never changed) */
	time_t	exptime;		/* time the key was expired (0L if not) */
	ulong	lifetime;		/* proposed key life time at time of generation */
	uint	tag;			/* key id */
	dk_status_t	status;		/* key exist (".key") and name of private */
					/* key file is ".published", ".private" */
					/* or ".depreciated" */
	char	*pubkey;		/* base64 public key */
	struct	dki	*next;		/* ptr to next entry in list */
} dki_t;

#if defined(USE_TREE) && USE_TREE
/*
 *	Instead of including <search.h>, which contains horrible false function
 *	declarations, we declared it for our usage (Yes, these functions return
 *	the adress of a pointer variable)
 */
typedef enum
{
	/* we change the naming to the new, and more predictive one, used by Knuth */
	PREORDER,	/* preorder,	*/
	INORDER,	/* postorder,	*/
	POSTORDER,	/* endorder,	*/
	LEAF		/* leaf		*/
}
VISIT;

dki_t	**tsearch (const dki_t *dkp, dki_t **tree, int(*compar)(const dki_t *, const dki_t *));
dki_t	**tfind (const dki_t *dkp, const dki_t **tree, int(*compar)(const dki_t *, const dki_t *));
dki_t	**tdelete (const dki_t *dkp, dki_t **tree, int(*compar)(const dki_t *, const dki_t *));
void	twalk (const dki_t *root, void (*action)(const dki_t **nodep, VISIT which, int depth));

extern	void	dki_tfree (dki_t **tree);
extern	dki_t	*dki_tadd (dki_t **tree, dki_t *new, int sub_before);
extern	int	dki_tagcmp (const dki_t *a, const dki_t *b);
extern	int	dki_namecmp (const dki_t *a, const dki_t *b);
extern	int	dki_revnamecmp (const dki_t *a, const dki_t *b);
extern	int	dki_allcmp (const dki_t *a, const dki_t *b);
#endif

extern	dki_t	*dki_read (const char *dir, const char *fname);
extern	int	dki_readdir (const char *dir, dki_t **listp, int recursive);
extern	int	dki_prt_trustedkey (const dki_t *dkp, FILE *fp);
extern	int	dki_prt_dnskey (const dki_t *dkp, FILE *fp);
extern	int	dki_prt_dnskeyttl (const dki_t *dkp, FILE *fp, int ttl);
extern	int	dki_prt_dnskey_raw (const dki_t *dkp, FILE *fp);
extern	int	dki_prt_comment (const dki_t *dkp, FILE *fp);
extern	int	dki_cmp (const dki_t *a, const dki_t *b);
extern	int	dki_timecmp (const dki_t *a, const dki_t *b);
extern	int	dki_age (const dki_t *dkp, time_t curr);
extern	dk_flag_t	dki_getflag (const dki_t *dkp, time_t curr);
extern	dk_flag_t	dki_setflag (dki_t *dkp, dk_flag_t flag);
extern	dk_flag_t	dki_unsetflag (dki_t *dkp, dk_flag_t flag);
extern	dk_status_t	dki_status (const dki_t *dkp);
extern	const	char	*dki_statusstr (const dki_t *dkp);
extern	int	dki_isksk (const dki_t *dkp);
extern	int	dki_isdepreciated (const dki_t *dkp);
extern	int	dki_isrevoked (const dki_t *dkp);
extern	int	dki_isactive (const dki_t *dkp);
extern	int	dki_ispublished (const dki_t *dkp);
extern	time_t	dki_algo (const dki_t *dkp);
extern	time_t	dki_time (const dki_t *dkp);
extern	time_t	dki_exptime (const dki_t *dkp);
extern	time_t	dki_gentime (const dki_t *dkp);
extern	time_t	dki_lifetime (const dki_t *dkp);
extern	ushort	dki_lifetimedays (const dki_t *dkp);
extern	ushort	dki_setlifetime (dki_t *dkp, int days);
extern	time_t	dki_setexptime (dki_t *dkp, time_t sec);
extern	dki_t	*dki_new (const char *dir, const char *name, int ksk, int algo, int bitsize, const char *rfile, int lf_days);
extern	dki_t	*dki_remove (dki_t *dkp);
extern	dki_t	*dki_destroy (dki_t *dkp);
extern	int	dki_setstatus (dki_t *dkp, int status);
extern	int	dki_setstatus_preservetime (dki_t *dkp, int status);
extern	dki_t	*dki_add (dki_t **dkp, dki_t *new);
extern	const dki_t	*dki_tsearch (const dki_t *tree, int tag, const char *name);
extern	const dki_t	*dki_search (const dki_t *list, int tag, const char *name);
extern	const dki_t	*dki_find (const dki_t *list, int ksk, int status, int first);
extern	const dki_t	*dki_findalgo (const dki_t *list, int ksk, int alg, int status, int no);
extern	void	dki_free (dki_t *dkp);
extern	void	dki_freelist (dki_t **listp);
extern	char	*dki_algo2str (int algo);
extern	char	*dki_algo2sstr (int algo);
extern	const char	*dki_geterrstr (void);

#endif
