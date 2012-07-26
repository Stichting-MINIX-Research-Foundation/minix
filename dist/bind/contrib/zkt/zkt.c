/*****************************************************************
**
**	@(#) zkt.c  -- A library for managing a list of dns zone files. 
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
# include <string.h>
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
# include "config_zkt.h"
# include "dki.h"
# include "misc.h"
# include "strlist.h"
# include "zconf.h"
# include "domaincmp.h"
# include "tcap.h"
#define extern
# include "zkt.h"
#undef extern

extern	char	*labellist;
extern	int	headerflag;
extern	int	timeflag;
extern	int	exptimeflag;
extern	int	lifetime;
extern	int	ageflag;
extern	int	lifetimeflag;
extern	int	kskflag;
extern	int	zskflag;
extern	int	pathflag;
extern	int	ljustflag;

static	void	printkeyinfo (const dki_t *dkp, const char *oldpath);

static	void	printkeyinfo (const dki_t *dkp, const char *oldpath)
{
	time_t	currtime;

	if ( dkp == NULL )	/* print headline */
	{
		if ( headerflag )
		{
			tc_attr (stdout, TC_BOLD, 1);
			printf ("%-33.33s %5s %3s %3.3s %-7s", "Keyname",
				"Tag", "Typ", "Status", "Algorit");
			if ( timeflag )
				printf (" %-20s", "Generation Time");
			if ( exptimeflag )
				printf (" %-20s", "Expiration Time");
			if ( ageflag  )
				printf (" %16s", "Age");
			if ( lifetimeflag  )
				printf (" %4s", "LfTm");
			tc_attr (stdout, TC_BOLD, 0);
			putchar ('\n');
		}
		return;
	}
	time (&currtime);

	/* TODO: use next line if dname is dynamically allocated */
	/* if ( pathflag && dkp->dname && strcmp (oldpath, dkp->dname) != 0 ) */
	if ( pathflag && strcmp (oldpath, dkp->dname) != 0 )
		printf ("%s/\n", dkp->dname);

	if ( (kskflag && dki_isksk (dkp)) || (zskflag && !dki_isksk (dkp)) )
	{
		int	color;

		if ( ljustflag )
			printf ("%-33.33s ", dkp->name);
		else
			printf ("%33.33s ", dkp->name);
		printf ("%05d ", dkp->tag);
		printf ("%3s ", dki_isksk (dkp) ? "KSK" : "ZSK");

		if ( dkp->status == DKI_ACT )
			color = TC_GREEN;
		else if ( dkp->status == DKI_PUB )
			color = TC_BLUE;
		else if ( dkp->status == DKI_DEP )
			color = TC_RED;
		else
			color = TC_BLACK;
		tc_attr (stdout, color, 1);
		printf ("%-3.3s ", dki_statusstr (dkp) );
		tc_attr (stdout, color, 0);

		printf ("%-7s", dki_algo2sstr(dkp->algo));

		if ( currtime < dkp->time + dkp->lifetime )
			color = TC_GREEN;
		else
			color = TC_BOLD|TC_RED;
		tc_attr (stdout, color, 1);

		if ( timeflag )
			printf (" %-20s", time2str (dkp->gentime ? dkp->gentime: dkp->time, 's')); 
		if ( exptimeflag )
			printf (" %-20s", time2str (dkp->exptime, 's')); 
		if ( ageflag )
			printf (" %16s", age2str (dki_age (dkp, currtime))); 
		if ( lifetimeflag && dkp->lifetime )
		{
			if ( dkp->status == 'a' )
				printf ("%c", (currtime < dkp->time + dkp->lifetime) ? '<' : '!'); 
			else
				putchar (' ');
			printf ("%hdd", dki_lifetimedays (dkp)); 
		}
		tc_attr (stdout, color, 0);
		putchar ('\n');
	}
}

#if defined(USE_TREE) && USE_TREE
static	void	list_key (const dki_t **nodep, const VISIT which, int depth)
{
	const	dki_t	*dkp;
	static	const	char	*oldpath = "";

	if ( nodep == NULL )
		return;
//fprintf (stderr, "listkey %d %d %s\n", which, depth, dkp->name);

	if ( which == INORDER || which == LEAF )
	{
		dkp = *nodep;
		while ( dkp )	/* loop through list */
		{
			if ( labellist == NULL || isinlist (dkp->name, labellist) )
				printkeyinfo (dkp, oldpath);		/* print entry */
			oldpath = dkp->dname;
			dkp = dkp->next;
		}
	}
}
#endif

void	zkt_list_keys (const dki_t *data)
{
#if ! defined(USE_TREE) || !USE_TREE
	const   dki_t   *dkp;
	const   char   *oldpath;
#endif

	if ( data )    /* print headline if list is not empty */
		printkeyinfo (NULL, "");

#if defined(USE_TREE) && USE_TREE
	twalk (data, list_key);
#else
	oldpath = "";
	for ( dkp = data; dkp; dkp = dkp->next )       /* loop through list */
	{
		if ( labellist == NULL || isinlist (dkp->name, labellist) )
			printkeyinfo (dkp, oldpath);            /* print entry */
		oldpath = dkp->dname;
	}
#endif
}

#if defined(USE_TREE) && USE_TREE
# if 0
static	void	list_trustedkey (const dki_t **nodep, const VISIT which, int depth)
{
	const	dki_t	*dkp;

	if ( nodep == NULL )
		return;

	dkp = *nodep;
	if ( which == INORDER || which == LEAF )
	{
// fprintf (stderr, "list_trustedkey order=%d(pre=0,in=1,post=2,leaf=3) depth=%d %s\n", which, depth, dkp->name);
		/* loop through list */
		while ( dkp )
		{
			if ( (dki_isksk (dkp) || zskflag) &&
			     (labellist == NULL || isinlist (dkp->name, labellist)) )
				dki_prt_trustedkey (dkp, stdout);
			dkp = dkp->next;
		}
	}
}
# else
const	dki_t	*parent;
static	void	list_trustedkey (const dki_t **nodep, const VISIT which, int depth)
{
	const	dki_t	*dkp;

	if ( nodep == NULL )
		return;

	dkp = *nodep;
	if ( which == INORDER || which == LEAF )
	{
// fprintf (stderr, "list_trustedkey order=%d(pre=0,in=1,post=2,leaf=3) depth=%d %s\n", which, depth, dkp->name);
		if ( labellist && !isinlist (dkp->name, labellist) )
			return;

		if ( parent == NULL || !issubdomain (dkp->name, parent->name) )
		{
			parent = dkp;	
			/* loop through list */
			while ( dkp )
			{
				if ( (dki_isksk (dkp) || zskflag) )
					dki_prt_trustedkey (dkp, stdout);
				dkp = dkp->next;
			}
		}
	}
}
# endif
#endif

void	zkt_list_trustedkeys (const dki_t *data)
{

	/* print headline if list is not empty */
	if ( data && headerflag )
		printf ("trusted-keys {\n");

#if defined(USE_TREE) && USE_TREE
	twalk (data, list_trustedkey);
#else
	for ( dkp = data; dkp; dkp = dkp->next )	/* loop through list */
		if ( (dki_isksk (dkp) || zskflag) &&
		     (labellist == NULL || isinlist (dkp->name, labellist)) )
			dki_prt_trustedkey (dkp, stdout);
#endif

	/* print end of trusted-key section */
	if ( data && headerflag )
		printf ("};\n");
}

#if defined(USE_TREE) && USE_TREE
static	void	list_dnskey (const dki_t **nodep, const VISIT which, int depth)
{
	const	dki_t	*dkp;
	int	ksk;

	if ( nodep == NULL )
		return;

	if ( which == INORDER || which == LEAF )
		for ( dkp = *nodep; dkp; dkp = dkp->next )
		{
			ksk = dki_isksk (dkp);
			if ( (ksk && !kskflag) || (!ksk && !zskflag) )
				continue;

			if ( labellist == NULL || isinlist (dkp->name, labellist) )
			{
				if ( headerflag )
					dki_prt_comment (dkp, stdout);
				dki_prt_dnskey (dkp, stdout);
			}
		}
}
#endif

void	zkt_list_dnskeys (const dki_t *data)
{
#if defined(USE_TREE) && USE_TREE
	twalk (data, list_dnskey);
#else
	const	dki_t	*dkp;
	int	ksk;

	for ( dkp = data; dkp; dkp = dkp->next )
	{
		ksk = dki_isksk (dkp);
		if ( (ksk && !kskflag) || (!ksk && !zskflag) )
			continue;

		if ( labellist == NULL || isinlist (dkp->name, labellist) )
		{
			if ( headerflag )
				dki_prt_comment (dkp, stdout);
			dki_prt_dnskey (dkp, stdout);
		}
	}
#endif
}

#if defined(USE_TREE) && USE_TREE
static	void	set_keylifetime (const dki_t **nodep, const VISIT which, int depth)
{
	const	dki_t	*dkp;
	int	ksk;

	if ( nodep == NULL )
		return;

	if ( which == INORDER || which == LEAF )
		for ( dkp = *nodep; dkp; dkp = dkp->next )
		{
			ksk = dki_isksk (dkp);
			if ( (ksk && !kskflag) || (!ksk && !zskflag) )
				continue;

			if ( labellist == NULL || isinlist (dkp->name, labellist) )
				dki_setlifetime ((dki_t *)dkp, lifetime);
		}
}
#endif

void	zkt_setkeylifetime (dki_t *data)
{
#if defined(USE_TREE) && USE_TREE
	twalk (data, set_keylifetime);
#else
	dki_t	*dkp;
	int	ksk;

	for ( dkp = data; dkp; dkp = dkp->next )
	{
		ksk = dki_isksk (dkp);
		if ( (ksk && !kskflag) || (!ksk && !zskflag) )
			continue;

		if ( labellist == NULL || isinlist (dkp->name, labellist) )
		{
			dki_setlifetime (dkp, lifetime);
		}
	}
#endif
}


#if defined(USE_TREE) && USE_TREE
static	const	dki_t	*searchresult;
static	int	searchitem;
static	void	tag_search (const dki_t **nodep, const VISIT which, int depth)
{
	const	dki_t	*dkp;

	if ( nodep == NULL )
		return;

	if ( which == PREORDER || which == LEAF )
		for ( dkp = *nodep; dkp; dkp = dkp->next )
		{
			if ( dkp->tag == searchitem )
			{
				if ( searchresult == NULL )
					searchresult = dkp;
				else
					searchitem = 0;
			}
		}
}
#endif
const	dki_t	*zkt_search (const dki_t *data, int searchtag, const char *keyname)
{
	const dki_t	*dkp = NULL;

#if defined(USE_TREE) && USE_TREE
	if ( keyname == NULL || *keyname == '\0' )
	{
		searchresult = NULL;
		searchitem = searchtag;
		twalk (data, tag_search);
		if ( searchresult != NULL && searchitem == 0 )
			dkp = (void *)01;
		else
			dkp = searchresult;
	}
	else
		dkp = (dki_t*)dki_tsearch (data, searchtag, keyname);
#else
	dkp = (dki_t*)dki_search (data, searchtag, keyname);
#endif
	return dkp;
}

