/*****************************************************************
**
**	@(#) rollover.c -- The key rollover functions
**
**	Copyright (c) Jan 2005 - May 2008, Holger Zuleger HZnet. All rights reserved.
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
# include <ctype.h>
# include <time.h>
# include <assert.h>
# include <dirent.h>
# include <errno.h>	
# include <unistd.h>	
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
# include "config_zkt.h"
# include "zconf.h"
# include "debug.h"

# include "misc.h"
# include "zone.h"
# include "dki.h"
# include "log.h"
#define extern
# include "rollover.h"
#undef extern

/*****************************************************************
**	local function definition
*****************************************************************/

static	dki_t	*genkey (dki_t **listp, const char *dir, const char *domain, int ksk, const zconf_t *conf, int status)
{
	dki_t	*dkp;

	if ( listp == NULL || domain == NULL )
		return NULL;

	if ( ksk )
		dkp = dki_new (dir, domain, DKI_KSK, conf->k_algo, conf->k_bits, conf->k_random, conf->k_life / DAYSEC);
	else
		dkp = dki_new (dir, domain, DKI_ZSK, conf->k_algo, conf->z_bits, conf->z_random, conf->z_life / DAYSEC);
	dki_add (listp, dkp);
	dki_setstatus (dkp, status);

	return dkp;
}

static	dki_t	*genkey2 (dki_t **listp, const char *dir, const char *domain, int ksk, const zconf_t *conf, int status)
{
	dki_t	*dkp;

	if ( listp == NULL || domain == NULL )
		return NULL;

	if ( ksk )
		dkp = dki_new (dir, domain, DKI_KSK, conf->k2_algo, conf->k_bits, conf->k_random, conf->k_life / DAYSEC);
	else
		dkp = dki_new (dir, domain, DKI_ZSK, conf->k2_algo, conf->z_bits, conf->z_random, conf->z_life / DAYSEC);
	dki_add (listp, dkp);
	dki_setstatus (dkp, status);

	return dkp;
}

static	time_t	get_exptime (dki_t *key, const zconf_t *z)
{
	time_t	exptime;

	exptime = dki_exptime (key);
	if ( exptime == 0L )
	{
		if ( dki_lifetime (key) )
			exptime = dki_time (key) + dki_lifetime (key);
		else
			exptime = dki_time (key) + z->k_life;
	}

	return exptime;
}

/*****************************************************************
**	is_parentdirsigned (name)
**	Check if the parent directory of the zone specified by zp
**	is a directory with a signed zone
**	Returns 0 | 1
*****************************************************************/
static	int	is_parentdirsigned (const zone_t *zonelist, const zone_t *zp)
{
	char	path[MAX_PATHSIZE+1];
	const	char	*ext;
#if 0
	const	zconf_t	*conf;

	/* check if there is a local config file to get the name of the zone file */
	snprintf (path, sizeof (path), "%s/../%s", zp->dir, LOCALCONF_FILE);
	if ( fileexist (path) )		/* parent dir has local config file ? */
		conf = loadconfig (path, NULL);
	else
		conf = zp->conf;

	/* build the path of the .signed zone file */
	snprintf (path, sizeof (path), "%s/../%s.signed", conf->dir, conf->zonefile);
	if ( conf != zp->conf )	/* if we read in a local config file.. */
		free (conf);	/* ..free the memory used */

#else
	/* currently we use the signed zone file name of the
	 * current directory for checking if the file exist.
	 * TODO: Instead we have to use the name of the zone file
	 * used in the parent dir (see above)
	 */

	ext = strrchr (zp->sfile, '.');
	if ( ext && strcmp (zp->sfile, ".dsigned") == 0 )	/* is the current zone a dynamic one ? */
		/* hack: we are using the standard zone file name for a static zone here */
		snprintf (path, sizeof (path), "%s/../%s", zp->dir, "zone.db.signed");
	else
	{
# if 1
		const	zone_t	*parent;
		const	char	*parentname;

		/* find out name of parent */
		parentname = strchr (zp->zone, '.');	/* find first dot in zone name */
		if ( parentname == NULL )	/* no parent found! */
			return 0;
		parentname += 1;	/* skip '.' */

		/* try to find parent zone in zonelist */
		if ( (parent = zone_search (zonelist, parentname)) == NULL )
			return 0;
		snprintf (path, sizeof (path), "%s/%s", parent->dir, parent->sfile);
# else
		snprintf (path, sizeof (path), "%s/../%s", zp->dir, zp->sfile);
# endif
	}
#endif
lg_mesg (LG_DEBUG, "%s: is_parentdirsigned = %d fileexist (%s)\n", zp->zone, fileexist (path), path);
	return fileexist (path);	/* parent dir has zone.db.signed file ? */
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
		fprintf (fp, "; KSK rollover phase1 (new key generated but this is alread the old one)\n");
	else
		fprintf (fp, "; KSK rollover phase2 (this is the new key)\n");

	dki_prt_dnskeyttl (dkp, fp, ttl);
	fclose (fp);

	return phase;
}

/*****************************************************************
**	get_parent_phase ()
*****************************************************************/
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

/*****************************************************************
**	kskrollover ()
*****************************************************************/
static	int	kskrollover (dki_t *ksk, zone_t *zonelist, zone_t *zp)
{
	char	path[MAX_PATHSIZE+1];
	const	zconf_t	*z;
	time_t	lifetime;
	time_t	currtime;
	time_t	age;
	int	currphase;
	int	parfile_age;
	int	parent_propagation;
	int	parent_resign;
	int	parent_keyttl;


	assert ( ksk != NULL );
	assert ( zp != NULL );

	z = zp->conf;
	/* check ksk lifetime */
	if ( (lifetime = dki_lifetime (ksk)) == 0 )	/* if lifetime of key is not set.. */
		lifetime = z->k_life;			/* ..use global configured lifetime */

	currtime = time (NULL);
	age = dki_age (ksk, currtime);

	/* build path of parent-file */
	pathname (path, sizeof (path), zp->dir, "parent-", zp->zone);

	/* check if we have to change the ksk ? */
	if ( lifetime > 0 && age > lifetime && !fileexist (path) )	/* lifetime is over and no kskrollover in progress */
	{
		/* we are in hierachical mode and the parent directory contains a signed zone ? */
		if ( z->keysetdir && strcmp (z->keysetdir, "..") == 0 && is_parentdirsigned (zonelist, zp) )
		{
			verbmesg (2, z, "\t\tkskrollover: create new key signing key\n");
			/* create a new key: this is phase one of a double signing key rollover */
			ksk = genkey (&zp->keys, zp->dir, zp->zone, DKI_KSK, z, DKI_ACTIVE);
			if ( ksk == NULL )
			{
				lg_mesg (LG_ERROR, "\"%s\": unable to generate new ksk for double signing rollover", zp->zone);
				return 0;
			}
			lg_mesg (LG_INFO, "\"%s\": kskrollover phase1: New key %d generated", zp->zone, ksk->tag);

			/* find the oldest active ksk to create the parent file */
			if ( (ksk = (dki_t *)dki_findalgo (zp->keys, DKI_KSK, zp->conf->k_algo, 'a', 1)) == NULL )
				lg_mesg (LG_ERROR, "kskrollover phase1: Couldn't find the old active key\n");
			if ( !create_parent_file (path, 1, z->key_ttl, ksk) )
				lg_mesg (LG_ERROR, "Couldn't create parentfile %s\n", path);

		}
		else	/* print out a warning only */
		{
			logmesg ("\t\tWarning: Lifetime of Key Signing Key %d exceeded: %s\n",
							ksk->tag, str_delspace (age2str (age)));
			lg_mesg (LG_WARNING, "\"%s\": lifetime of key signing key %d exceeded since %s",
							zp->zone, ksk->tag, str_delspace (age2str (age - lifetime)));
		}
		return 1;
	}

	/* now check if there is an ongoing key rollover */

	/* check if parent-file already exist */
	if ( !fileexist (path) )	/* no parent-<zone> file found ? */
		return 0;	/* ok, that's it */

	/* check the ksk rollover phase we are in */
	currphase = get_parent_phase (path);	/* this is the actual state we are in */
	parfile_age = file_age (path);

	/* TODO: Set these values to the one found in the parent dnssec.conf file */
	parent_propagation = PARENT_PROPAGATION;
	parent_resign = z->resign;
	parent_keyttl = z->key_ttl;

	switch ( currphase )
	{
	case 1:	/* we are currently in state one (new ksk already generated) */
		if ( parfile_age > z->proptime + z->key_ttl )	/* can we go to phase 2 ? */
		{
			verbmesg (2, z, "\t\tkskrollover: save new ksk in parent file\n");
			ksk = ksk->next;    /* set ksk to new ksk */
			if ( !create_parent_file (path, currphase+1, z->key_ttl, ksk) )
				lg_mesg (LG_ERROR, "Couldn't create parentfile %s\n", path);
			lg_mesg (LG_INFO, "\"%s\": kskrollover phase2: send new key %d to the parent zone", zp->zone, ksk->tag);
			return 1;
		}
		else
			verbmesg (2, z, "\t\tkskrollover: we are in state 1 and waiting for propagation of the new key (parentfile %dsec < prop %dsec + keyttl %dsec\n", parfile_age, z->proptime, z->key_ttl);
		break;
	case 2:	/* we are currently in state two (propagation of new key to the parent) */
#if 0
		if ( parfile_age >= parent_propagation + parent_resign + parent_keyttl )	/* can we go to phase 3 ? */
#else
		if ( parfile_age >= parent_propagation + parent_keyttl )	/* can we go to phase 3 ? */
#endif
		{
			/* remove the parentfile */
			unlink (path);

			/* remove oldest key from list and mark file as removed */
			zp->keys = dki_remove (ksk);

			// verbmesg (2, z, "kskrollover: remove parentfile and rename old key to k<zone>+<algo>+<tag>.key\n");
			verbmesg (2, z, "\t\tkskrollover: remove parentfile and rename old key to k%s+%03d+%05d.key\n",
									ksk->name, ksk->algo, ksk->tag);
			lg_mesg (LG_INFO, "\"%s\": kskrollover phase3: Remove old key %d", zp->zone, ksk->tag);
			return 1;
		}
		else
#if 0
			verbmesg (2, z, "\t\tkskrollover: we are in state 2 and  waiting for parent propagation (parentfile %d < parentprop %d + parentresig %d + parentkeyttl %d\n", parfile_age, parent_propagation, parent_resign, parent_keyttl);
#else
			verbmesg (2, z, "\t\tkskrollover: we are in state 2 and waiting for parent propagation (parentfile %dsec < parentprop %dsec + parentkeyttl %dsec\n", parfile_age, parent_propagation, parent_keyttl);
#endif
		break;
	default:
		assert ( currphase == 1 || currphase == 2 );
		/* NOTREACHED */
	}
	
	return 0;
}

/*****************************************************************
**	global function definition
*****************************************************************/

/*****************************************************************
**	ksk5011status ()
**	Check if the list of zone keys containing a revoked or a
**	standby key.
**	Remove the revoked key if it is older than 30 days.
**	If the lifetime of the active key is reached, do a rfc5011
**	keyrollover.
**	Returns an int with the rightmost bit set if a resigning
**	is required. The second rightmost bit is set, if it is an
**	rfc5011 zone.
*****************************************************************/
int	ksk5011status (dki_t **listp, const char *dir, const char *domain, const zconf_t *z)
{
	dki_t	*standbykey;
	dki_t	*activekey;
	dki_t	*dkp;
	dki_t	*prev;
	time_t	currtime;
	time_t	exptime;
	int	ret;

	assert ( listp != NULL );
	assert ( z != NULL );

	if ( z->k_life == 0 )
		return 0;

	verbmesg (1, z, "\tCheck RFC5011 status\n");

	ret = 0;
	currtime = time (NULL);

	/* go through the list of key signing keys,	*/
	/* remove revoked keys and set a pointer to standby and active key */
	standbykey = activekey = NULL;
	prev = NULL;
	for ( dkp = *listp; dkp && dki_isksk (dkp); dkp = dkp->next )
	{
		exptime = get_exptime (dkp, z);
		if ( dki_isrevoked (dkp) )
			lg_mesg (LG_DEBUG, "zone \"%s\": found revoked key (id=%d exptime=%s); waiting for remove hold down time",
							domain, dkp->tag, time2str (exptime, 's'));

		/* revoked key is older than 30 days? */
		if ( dki_isrevoked (dkp) && currtime > exptime + REMOVE_HOLD_DOWN )
		{
			verbmesg (1, z, "\tRemove revoked key %d which is older than 30 days\n", dkp->tag);
			lg_mesg (LG_NOTICE, "zone \"%s\": removing revoked key %d", domain, dkp->tag);

			/* remove key from list and mark file as removed */
			if ( prev == NULL )		/* at the beginning of the list ? */
				*listp = dki_remove (dkp);
			else				/* anywhere in the middle of the list */
				prev->next = dki_remove (dkp);

			ret |= 01;		/* from now on a resigning is necessary */
		}

		/* remember oldest standby and active key */
		if ( dki_status (dkp) == DKI_PUBLISHED )
			standbykey = dkp;
		if ( dki_status (dkp) == DKI_ACTIVE )
			activekey = dkp;
	}
				/* no activekey or no standby key and also no revoked key found ? */
	if ( activekey == NULL || (standbykey == NULL && ret == 0) )
		return ret;				/* Seems that this is a non rfc5011 zone! */

	ret |= 02;		/* Zone looks like a rfc5011 zone */

	exptime = get_exptime (activekey, z);
#if 0
	lg_mesg (LG_DEBUG, "Act Exptime: %s", time2str (exptime, 's'));
	lg_mesg (LG_DEBUG, "Stb time: %s", time2str (dki_time (standbykey), 's'));
	lg_mesg (LG_DEBUG, "Stb time+wait: %s", time2str (dki_time (standbykey) + min (DAYSEC * 30, z->key_ttl), 's'));
#endif
	/* At the first time we introduce a standby key, the lifetime of the current KSK shouldn't be expired, */
	/* otherwise we run into an (nearly) immediate key rollover!	*/
	if ( currtime > exptime && currtime > dki_time (standbykey) + min (ADD_HOLD_DOWN, z->key_ttl) )
	{
		lg_mesg (LG_NOTICE, "\"%s\": starting rfc5011 rollover", domain);
		verbmesg (1, z, "\tLifetime of Key Signing Key %d exceeded (%s): Starting rfc5011 rollover!\n",
							activekey->tag, str_delspace (age2str (dki_age (activekey, currtime))));
		verbmesg (2, z, "\t\t=>Generating new standby key signing key\n");
		dkp = genkey (listp, dir, domain, DKI_KSK, z, DKI_PUBLISHED);	/* gentime == now; lifetime = z->k_life; exp = 0 */
		if ( !dkp )
		{
			error ("\tcould not generate new standby KSK\n");
			lg_mesg (LG_ERROR, "\%s\": can't generate new standby KSK", domain);
		}
		else
			lg_mesg (LG_NOTICE, "\"%s\": generated new standby KSK %d", domain, dkp->tag);

		/* standby key gets active  */
		verbmesg (2, z, "\t\t=>Activating old standby key %d \n", standbykey->tag);
		dki_setstatus (standbykey, DKI_ACT);

		/* active key should be revoked */ 
		verbmesg (2, z, "\t\t=>Revoking old active key %d \n", activekey->tag);
		dki_setstatus (activekey, DKI_REVOKED);	
		dki_setexptime (activekey, currtime);	/* now the key is expired */

		ret |= 01;		/* resigning necessary */
	}

	return ret;
}

/*****************************************************************
**	kskstatus ()
**	Check the ksk status of a zone if a ksk lifetime is set.
**	If there is no key signing key present create a new one.
**	Prints out a warning message if the lifetime of the current
**	key signing key is over.
**	Returns 1 if a resigning of the zone is necessary, otherwise
**	the function returns 0.
*****************************************************************/
int	kskstatus (zone_t *zonelist, zone_t *zp)
{
	dki_t	*akey;
	const	zconf_t	*z;

	assert ( zp != NULL );

	z = zp->conf;
	if ( z->k_life == 0 )
		return 0;

	verbmesg (1, z, "\tCheck KSK status\n");
	/* check if a key signing key exist ? */
	akey = (dki_t *)dki_findalgo (zp->keys, DKI_KSK, z->k_algo, 'a', 1);
	if ( akey == NULL )
	{
		verbmesg (1, z, "\tNo active KSK found: generate new one\n");
		akey = genkey (&zp->keys, zp->dir, zp->zone, DKI_KSK, z, DKI_ACTIVE);
		if ( !akey )
		{
			error ("\tcould not generate new KSK\n");
			lg_mesg (LG_ERROR, "\"%s\": can't generate new KSK: \"%s\"",
								zp->zone, dki_geterrstr());
		}
		else
			lg_mesg (LG_INFO, "\"%s\": generated new KSK %d", zp->zone, akey->tag);
		return akey != NULL;	/* return value of 1 forces a resigning of the zone */
	}
	else	/* try to start a full automated ksk rollover */
		kskrollover (akey, zonelist, zp);

	/* is a second algorithm requested ? (since 0.99) */
	if ( z->k2_algo && z->k2_algo != z->k_algo )
	{
		/* check for ksk supporting the additional algorithm */
		akey = (dki_t *)dki_findalgo (zp->keys, DKI_KSK, z->k2_algo, 'a', 1);
		if ( akey == NULL )
		{
			verbmesg (1, z, "\tNo active KSK for additional algorithm found: generate new one\n");
			akey = genkey2 (&zp->keys, zp->dir, zp->zone, DKI_KSK, z, DKI_ACTIVE);
			if ( !akey )
			{
				error ("\tcould not generate new KSK for additional algorithm\n");
				lg_mesg (LG_ERROR, "\"%s\": can't generate new KSK for 2nd algorithm: \"%s\"",
									zp->zone, dki_geterrstr());
			}
			else
				lg_mesg (LG_INFO, "\"%s\": generated new KSK %d for additional algorithm",
										zp->zone, akey->tag);
			return 1;	/* return value of 1 forces a resigning of the zone */
		}
	}

	return 0;
}

/*****************************************************************
**	zskstatus ()
**	Check the zsk status of a zone.
**	Returns 1 if a resigning of the zone is necessary, otherwise
**	the function returns 0.
*****************************************************************/
int	zskstatus (dki_t **listp, const char *dir, const char *domain, const zconf_t *z)
{
	dki_t	*akey;
	dki_t	*nextkey;
	dki_t	*dkp, *last;
	int	keychange;
	time_t	lifetime;
	time_t	age;
	time_t	currtime;

	assert ( listp != NULL );
	/* dir can be NULL */
	assert ( domain != NULL );
	assert ( z != NULL );

	currtime = time (NULL);

	verbmesg (1, z, "\tCheck ZSK status\n");
	dbg_val("zskstatus for %s \n", domain);
	keychange = 0;
	/* Is the depreciated key expired ? */
	/* As mentioned by olaf, this is the max_ttl of all the rr in the zone */
	lifetime = z->max_ttl + z->proptime;	/* draft kolkman/gieben */
	last = NULL;
	dkp = *listp;
	while ( dkp )
		if ( !dki_isksk (dkp) &&
		     dki_status (dkp) == DKI_DEPRECIATED && 
		     dki_age (dkp, currtime) > lifetime )
		{
			keychange = 1;
			verbmesg (1, z, "\tLifetime(%d sec) of depreciated key %d exceeded (%d sec)\n",
					 lifetime, dkp->tag, dki_age (dkp, currtime));
			lg_mesg (LG_INFO, "\"%s\": old ZSK %d removed", domain, dkp->tag);
			dkp = dki_destroy (dkp);	/* delete the keyfiles */
			dbg_msg("zskstatus: depreciated key removed ");
			if ( last )
				last->next = dkp;
			else
				*listp = dkp;
			verbmesg (1, z, "\t\t->remove it\n");
		}
		else
		{
			last = dkp;
			dkp = dkp->next;
		}

	/* check status of active key */
	dbg_msg("zskstatus check status of active key ");
	lifetime = z->z_life;			/* global configured lifetime for zsk */
	akey = (dki_t *)dki_findalgo (*listp, DKI_ZSK, z->k_algo, 'a', 1);
	if ( akey == NULL && lifetime > 0 )	/* no active key found */
	{
		verbmesg (1, z, "\tNo active ZSK found: generate new one\n");
		akey = genkey (listp, dir, domain, DKI_ZSK, z, DKI_ACTIVE);
		lg_mesg (LG_INFO, "\"%s\": generated new ZSK %d", domain, akey->tag);
	}
	else	/* active key exist */
	{
		if ( dki_lifetime (akey) )
			lifetime = dki_lifetime (akey);	/* set lifetime to lt of active key */

		/* lifetime of active key is expired and published key exist ? */
		age = dki_age (akey, currtime);
		if ( lifetime > 0 && age > lifetime - (OFFSET) )
		{
			verbmesg (1, z, "\tLifetime(%d +/-%d sec) of active key %d exceeded (%d sec)\n",
					lifetime, (OFFSET) , akey->tag, dki_age (akey, currtime) );

			/* depreciate the key only if there is another active or published key */
			if ( (nextkey = (dki_t *)dki_findalgo (*listp, DKI_ZSK, z->k_algo, 'a', 2)) == NULL ||
			      nextkey == akey )
				nextkey = (dki_t *)dki_findalgo (*listp, DKI_ZSK, z->k_algo, 'p', 1);

			/* Is the published key sufficient long in the zone ? */
			/* As mentioned by Olaf, this should be the ttl of the DNSKEY RR ! */
			if ( nextkey && dki_age (nextkey, currtime) > z->key_ttl + z->proptime )
			{
				keychange = 1;
				verbmesg (1, z, "\t\t->depreciate it\n");
				dki_setstatus (akey, 'd');	/* depreciate the active key */
				verbmesg (1, z, "\t\t->activate published key %d\n", nextkey->tag);
				dki_setstatus (nextkey, 'a');	/* activate published key */
				lg_mesg (LG_NOTICE, "\"%s\": lifetime of zone signing key %d exceeded: ZSK rollover done", domain, akey->tag);
				akey = nextkey;
				nextkey = NULL;
				lifetime = dki_lifetime (akey);	/* set lifetime to lt of the new active key (F. Behrens) */
			}
			else
			{
				verbmesg (1, z, "\t\t->waiting for published key\n");
				lg_mesg (LG_NOTICE, "\"%s\": lifetime of zone signing key %d exceeded since %s: ZSK rollover deferred: waiting for published key",
						domain, akey->tag, str_delspace (age2str (age - lifetime)));
			}
		}
	}
	/* Should we add a new publish key?  This is necessary if the active
	 * key will be expired at the next re-signing interval (The published
	 * time will be checked just before the active key will be removed.
	 * See above).
	 */
	nextkey = (dki_t *)dki_findalgo (*listp, DKI_ZSK, z->k_algo, 'p', 1);
	if ( nextkey == NULL && lifetime > 0 && (akey == NULL ||
	     dki_age (akey, currtime + z->resign) > lifetime - (OFFSET)) )
	{
		keychange = 1;
		verbmesg (1, z, "\tNew key for publishing needed\n");
		nextkey = genkey (listp, dir, domain, DKI_ZSK, z, DKI_PUB);

		if ( nextkey )
		{
			verbmesg (1, z, "\t\t->creating new key %d\n", nextkey->tag);
			lg_mesg (LG_INFO, "\"%s\": new key %d generated for publishing", domain, nextkey->tag);
		}
		else
		{
			error ("\tcould not generate new ZSK: \"%s\"\n", dki_geterrstr());
			lg_mesg (LG_ERROR, "\"%s\": can't generate new ZSK: \"%s\"",
								domain, dki_geterrstr());
		}
	}

	/* is a second algorithm requested ? (since 0.99) */
	if ( z->k2_algo && z->k2_algo != z->k_algo )
	{
		/* check for zsk supporting the additional algorithm */
		akey = (dki_t *)dki_findalgo (*listp, DKI_ZSK, z->k2_algo, 'a', 1);
		if ( akey == NULL )
		{
			verbmesg (1, z, "\tNo active ZSK for second algorithm found: generate new one\n");
			akey = genkey2 (listp, dir, domain, DKI_ZSK, z, DKI_ACTIVE);
			if ( !akey )
			{
				error ("\tcould not generate new ZSK for 2nd algorithm\n");
				lg_mesg (LG_ERROR, "\"%s\": can't generate new ZSK for 2nd algorithm: \"%s\"",
									domain, dki_geterrstr());
			}
			else
				lg_mesg (LG_INFO, "\"%s\": generated new ZSK %d for 2nd algorithm",
										domain, akey->tag);
			return 1;	/* return value of 1 forces a resigning of the zone */
		}
	}

	return keychange;
}

