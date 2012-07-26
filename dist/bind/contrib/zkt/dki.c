/*****************************************************************
**
**	@(#) dki.c  (c) Jan 2005  Holger Zuleger  hznet.de
**
**	A library for managing BIND dnssec key files.
**
**	Copyright (c) Jan 2005, Holger Zuleger HZnet. All rights reserved.
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
# include <ctype.h>	/* tolower(), ... */
# include <unistd.h>	/* link(), unlink(), ... */
# include <stdlib.h>
# include <sys/types.h>
# include <sys/time.h>
# include <sys/stat.h>
# include <dirent.h>
# include <assert.h>
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
# include "config_zkt.h"
# include "debug.h"
# include "domaincmp.h"
# include "misc.h"
# include "zconf.h"
#define	extern
# include "dki.h"
#undef	extern

/*****************************************************************
**	private (static) function declaration and definition
*****************************************************************/
static	char	dki_estr[255+1];

static	dki_t	*dki_alloc ()
{
	dki_estr[0] = '\0';
	dki_t	*dkp = malloc (sizeof (dki_t));

	if ( (dkp = malloc (sizeof (dki_t))) )
	{
		memset (dkp, 0, sizeof (dki_t));
		return dkp;
	}

	snprintf (dki_estr, sizeof (dki_estr),
			"dki_alloc: Out of memory");
	return NULL;
}

static	int	dki_readfile (FILE *fp, dki_t *dkp)
{
	int	algo,	flags,	type;
	int	c;
	char	*p;
	char	buf[4095+1];
	char	tag[25+1];
	char	val[14+1];	/* e.g. "YYYYMMDDhhmmss" | "60d" */

	assert (dkp != NULL);
	assert (fp != NULL);

	while ( (c = getc (fp)) == ';' )	/* line start with comment ? */
	{	
		tag[0] = val[0] = '\0';
		if ( (c = getc (fp)) == '%' )	/* special comment? */
		{
			while ( (c = getc (fp)) == ' ' || c == '\t' )
				;
			ungetc (c, fp);
			/* then try to read in the creation, expire and lifetime */
			if ( fscanf (fp, "%25[a-zA-Z]=%14s", tag, val) == 2 )
			{
				dbg_val2 ("dki_readfile: tag=%s val=%s \n", tag, val);
				switch ( tolower (tag[0]) )
				{
				case 'g': dkp->gentime = timestr2time (val);	break;
				case 'e': dkp->exptime = timestr2time (val);	break;
				case 'l': dkp->lifetime = atoi (val) * DAYSEC;	break;
				}
			}
		}
		else
			ungetc (c, fp);
		while ( (c = getc (fp)) != EOF && c != '\n' )	/* eat up rest of the line */
			;
	}
	ungetc (c, fp);	/* push back last char */

	if ( fscanf (fp, "%4095s", buf) != 1 )	/* read label */
		return -1;

	if ( strcmp (buf, dkp->name) != 0 )
		return -2;

#if defined(TTL_IN_KEYFILE_ALLOWED) && TTL_IN_KEYFILE_ALLOWED
	/* skip optional TTL value */
	while ( (c = getc (fp)) != EOF && isspace (c) )	/* skip spaces */
		;
	if ( isdigit (c) )				/* skip ttl */
		fscanf (fp, "%*d");
	else
		ungetc (c, fp);				/* oops, no ttl */
#endif

	if ( (c = fscanf (fp, " IN DNSKEY %d %d %d", &flags, &type, &algo)) != 3 &&
	     (c = fscanf (fp, "KEY %d %d %d", &flags, &type, &algo)) != 3 )
		return -3;
	if ( type != 3 || algo != dkp->algo )
		return -4;		/* no DNSKEY or algorithm mismatch */
	if ( ((flags >> 8) & 0xFF) != 01 )
		return -5;		/* no ZONE key */
	dkp->flags = flags;

	if ( fgets (buf, sizeof buf, fp) == NULL || buf[0] == '\0' )
		return -6;
	p = buf + strlen (buf);
	*--p = '\0';		/* delete trailing \n */
	/* delete leading ws */
	for ( p = buf; *p  && isspace (*p); p++ )
		;

	dkp->pubkey = strdup (p);

	return 0;
}

static	int	dki_writeinfo (const dki_t *dkp, const char *path)
{
	FILE	*fp;

	assert (dkp != NULL);
	assert (path != NULL && path[0] != '\0');

	if ( (fp = fopen (path, "w")) == NULL )
		return 0;
	dbg_val1 ("dki_writeinfo %s\n", path);
	if ( dki_prt_dnskey_raw (dkp, fp) == 0 )
		return 0;
	fclose (fp);
	touch (path, dkp->time);	/* restore time of key file */

	return 1;
}

static	int	dki_setstat (dki_t *dkp, int status, int preserve_time);

/*****************************************************************
**	public function definition
*****************************************************************/

/*****************************************************************
**	dki_free ()
*****************************************************************/
void	dki_free (dki_t *dkp)
{
	assert (dkp != NULL);

	if ( dkp->pubkey )
		free (dkp->pubkey);
	free (dkp);
}

/*****************************************************************
**	dki_freelist ()
*****************************************************************/
void	dki_freelist (dki_t **listp)
{
	dki_t	*curr;
	dki_t	*next;

	assert (listp != NULL);

	curr = *listp;
	while ( curr )
	{
		next = curr->next;
		dki_free (curr);
		curr = next;
	}
	if ( *listp )
		*listp = NULL;
}

#if defined(USE_TREE) && USE_TREE
/*****************************************************************
**	dki_tfree ()
*****************************************************************/
void	dki_tfree (dki_t **tree)
{
	assert (tree != NULL);
	// TODO: tdestroy is a GNU extension
	// tdestroy (*tree, dki_free);
}
#endif

#if defined(BIND_VERSION) && BIND_VERSION >= 970
# define	KEYGEN_COMPMODE	"-C -q "	/* this is the compability mode needed by BIND 9.7 */
#else
# define	KEYGEN_COMPMODE	""
#endif
/*****************************************************************
**	dki_new ()
**	create new keyfile
**	allocate memory for new dki key and init with keyfile
*****************************************************************/
dki_t	*dki_new (const char *dir, const char *name, int ksk, int algo, int bitsize, const char *rfile, int lf_days)
{
	char	cmdline[511+1];
	char	fname[254+1];
	char	randfile[254+1];
	FILE	*fp;
	int	len;
	char	*flag = "";
	char	*expflag = "";
	dki_t	*new;

	if ( ksk )
		flag = "-f KSK";

	randfile[0] = '\0';
	if ( rfile && *rfile )
		snprintf (randfile, sizeof (randfile), "-r %.250s ", rfile);
		
	if ( algo == DK_ALGO_RSA || algo == DK_ALGO_RSASHA1 || algo == DK_ALGO_RSASHA256 || algo == DK_ALGO_RSASHA512 )
		expflag = "-e ";

	if ( dir && *dir )
		snprintf (cmdline, sizeof (cmdline), "cd %s ; %s %s%s%s-n ZONE -a %s -b %d %s %s",
			dir, KEYGENCMD, KEYGEN_COMPMODE, randfile, expflag, dki_algo2str(algo), bitsize, flag, name);
	else
		snprintf (cmdline, sizeof (cmdline), "%s %s%s%s-n ZONE -a %s -b %d %s %s",
			KEYGENCMD, KEYGEN_COMPMODE, randfile, expflag, dki_algo2str(algo), bitsize, flag, name);

	dbg_msg (cmdline);

	if ( (fp = popen (cmdline, "r")) == NULL || fgets (fname, sizeof fname, fp) == NULL )
		return NULL;
	pclose (fp);

	len = strlen (fname) - 1;
	if ( len >= 0 && fname[len] == '\n' )
		fname[len] = '\0';

	new = dki_read (dir, fname);
	if ( new )
		dki_setlifetime (new, lf_days);	/* sets gentime + proposed lifetime */
	
	return new;
}

/*****************************************************************
**	dki_read ()
**	read key from file 'filename' (independed of the extension)
*****************************************************************/
dki_t	*dki_read (const char *dirname, const char *filename)
{
	dki_t	*dkp;
	FILE	*fp;
	struct	stat	st;
	int	len;
	int	err;
	char	fname[MAX_FNAMESIZE+1];
	char	path[MAX_PATHSIZE+1];

	dki_estr[0] = '\0';
	if ( (dkp = dki_alloc ()) == NULL )
		return (NULL);

	len = sizeof (fname) - 1;
	fname[len] = '\0';
	strncpy (fname, filename, len);

	len = strlen (fname);			/* delete extension */
	if ( len > 4 && strcmp (&fname[len - 4], DKI_KEY_FILEEXT) == 0 )
		fname[len - 4] = '\0';
	else if ( len > 10 && strcmp (&fname[len - 10], DKI_PUB_FILEEXT) == 0 )
		fname[len - 10] = '\0';
	else if ( len > 8 && strcmp (&fname[len - 8], DKI_ACT_FILEEXT) == 0 )
		fname[len - 8] = '\0';
	else if ( len > 12 && strcmp (&fname[len - 12], DKI_DEP_FILEEXT) == 0 )
		fname[len - 12] = '\0';
	dbg_line ();

	assert (strlen (dirname)+1 < sizeof (dkp->dname));
	strcpy (dkp->dname, dirname);

	assert (strlen (fname)+1 < sizeof (dkp->fname));
	strcpy (dkp->fname, fname);
	dbg_line ();
	if ( sscanf (fname, "K%254[^+]+%hd+%d", dkp->name, &dkp->algo, &dkp->tag) != 3 )
	{
		snprintf (dki_estr, sizeof (dki_estr),
			"dki_read: Filename don't match expected format (%s)", fname);
		return (NULL);
	}

	pathname (path, sizeof (path), dkp->dname, dkp->fname, DKI_KEY_FILEEXT);
	dbg_val ("dki_read: path \"%s\"\n", path);
	if ( (fp = fopen (path, "r")) == NULL )
	{
		snprintf (dki_estr, sizeof (dki_estr),
			"dki_read: Can\'t open file \"%s\" for reading", path);
		return (NULL);
	}
	
	dbg_line ();
	if ( (err = dki_readfile (fp, dkp)) != 0 )
	{
		dbg_line ();
		snprintf (dki_estr, sizeof (dki_estr),
			"dki_read: Can\'t read key from file %s (errno %d)", path, err);
		fclose (fp);
		return (NULL);
	}

	dbg_line ();
	if ( fstat (fileno(fp), &st) )
	{
		snprintf (dki_estr, sizeof (dki_estr),
			"dki_read: Can\'t stat file %s", fname);
		return (NULL);
	}
	dkp->time = st.st_mtime;

	dbg_line ();
	pathname (path, sizeof (path), dkp->dname, dkp->fname, DKI_ACT_FILEEXT);
	if ( fileexist (path) )
	{
		if ( dki_isrevoked (dkp) )
			dkp->status = DKI_REV;
		else
			dkp->status = DKI_ACT;
	}
	else
	{
		pathname (path, sizeof (path), dkp->dname, dkp->fname, DKI_PUB_FILEEXT);
		if ( fileexist (path) )
			dkp->status = DKI_PUB;
		else
		{
			pathname (path, sizeof (path), dkp->dname, dkp->fname, DKI_DEP_FILEEXT);
			if ( fileexist (path) )
				dkp->status = DKI_DEP;
			else
				dkp->status = DKI_SEP;
		}
	}

	dbg_line ();
	fclose (fp);

	dbg_line ();
	return dkp;
}

/*****************************************************************
**	dki_readdir ()
**	read key files from directory 'dir' and, if recursive is
**	true, from all directorys below that.
*****************************************************************/
int	dki_readdir (const char *dir, dki_t **listp, int recursive)
{
	dki_t	*dkp;
	DIR	*dirp;
	struct  dirent  *dentp;
	char	path[MAX_PATHSIZE+1];

	dbg_val ("directory: opendir(%s)\n", dir);
	if ( (dirp = opendir (dir)) == NULL )
		return 0;

	while ( (dentp = readdir (dirp)) != NULL )
	{
		if ( is_dotfilename (dentp->d_name) )
			continue;

		dbg_val ("directory: check %s\n", dentp->d_name);
		pathname (path, sizeof (path), dir, dentp->d_name, NULL);
		if ( is_directory (path) && recursive )
		{
			dbg_val ("directory: recursive %s\n", path);
			dki_readdir (path, listp, recursive);
		}
		else if ( is_keyfilename (dentp->d_name) )
			if ( (dkp = dki_read (dir, dentp->d_name)) )
				dki_add (listp, dkp);
	}
	closedir (dirp);
	return 1;
}

/*****************************************************************
**	dki_setstatus_preservetime ()
**	set status of key and change extension to
**	".published", ".private" or ".depreciated"
*****************************************************************/
int	dki_setstatus_preservetime (dki_t *dkp, int status)
{
	return dki_setstat (dkp, status, 1);
}

/*****************************************************************
**	dki_setstatus ()
**	set status of key and change extension to
**	".published", ".private" or ".depreciated"
*****************************************************************/
int	dki_setstatus (dki_t *dkp, int status)
{
	return dki_setstat (dkp, status, 0);
}

/*****************************************************************
**	dki_setstat ()
**	low level function of dki_setstatus and dki_setstatus_preservetime
*****************************************************************/
static	int	dki_setstat (dki_t *dkp, int status, int preserve_time)
{
	char	frompath[MAX_PATHSIZE+1];
	char	topath[MAX_PATHSIZE+1];
	time_t	totime;
	time_t	currtime;

	if ( dkp == NULL )
		return 0;

	currtime = time (NULL);
	status = tolower (status);
	switch ( dkp->status )	/* look at old status */
	{
	case 'r':
		if ( status == 'r' )
			return 1;
		break;
	case 'a':
		if ( status == 'a' )
			return 1;
		pathname (frompath, sizeof (frompath), dkp->dname, dkp->fname, DKI_ACT_FILEEXT);
		break;
	case 'd':
		if ( status == 'd' )
			return 1;
		pathname (frompath, sizeof (frompath), dkp->dname, dkp->fname, DKI_DEP_FILEEXT);
		break;
	case 'p':	/* or 's' */
		if ( status == 'p' || status == 's' )
			return 1;
		pathname (frompath, sizeof (frompath), dkp->dname, dkp->fname, DKI_PUB_FILEEXT);
		break;
	default:
		/* TODO: set error code */
		return 0;
	}

	dbg_val ("dki_setstat: \"%s\"\n", frompath);
	dbg_val ("dki_setstat: to status \"%c\"\n", status);

	/* a state change could result in different things: */
	/* 1) write a new keyfile when the REVOKE bit is set or unset */
	if ( status == 'r' || (status == 'a' && dki_isrevoked (dkp)) )
	{
		pathname (topath, sizeof (topath), dkp->dname, dkp->fname, DKI_KEY_FILEEXT);

		if ( status == 'r' )
			dki_setflag (dkp, DK_FLAG_REVOKE);	/* set REVOKE bit */
		else
			dki_unsetflag (dkp, DK_FLAG_REVOKE);	/* clear REVOKE bit */
			

		dki_writeinfo (dkp, topath);	/* ..and write it to the key file */
		
		if ( !preserve_time )
			touch (topath, time (NULL));
			
		return 0;
	}


	/* 2) change the filename of the private key in all other cases */
	totime = 0L;
	if ( preserve_time )
		totime = file_mtime (frompath);    /* get original timestamp */
	topath[0] = '\0';
	switch ( status )
	{
	case 'a':
		pathname (topath, sizeof (topath), dkp->dname, dkp->fname, DKI_ACT_FILEEXT);
		break;
	case 'd':
		pathname (topath, sizeof (topath), dkp->dname, dkp->fname, DKI_DEP_FILEEXT);
		break;
	case 's':		/* standby means a "published KSK" */
		if ( !dki_isksk (dkp) )
			return 2;
		status = 'p';
		/* fall through */
	case 'p':
		pathname (topath, sizeof (topath), dkp->dname, dkp->fname, DKI_PUB_FILEEXT);
		break;
	}

	if ( topath[0] )
	{
		dbg_val ("dki_setstat: to  \"%s\"\n", topath);
		if ( link (frompath, topath) == 0 )
			unlink (frompath);
		dkp->status = status;
		if ( !totime )
			totime = time (NULL);	/* set .key file to current time */
		pathname (topath, sizeof (topath), dkp->dname, dkp->fname, DKI_KEY_FILEEXT);
		touch (topath, totime);	/* store/restore time of status change */
	}

	return 0;
}

/*****************************************************************
**	dki_remove ()
**	rename files associated with key, so that the keys are not
**	recognized by the zkt tools e.g.
**	Kdo.ma.in.+001+12345.key ==> kdo.ma.in.+001+12345.key 
**	(second one starts with a lower case 'k')
*****************************************************************/
dki_t	*dki_remove (dki_t *dkp)
{
	char	path[MAX_PATHSIZE+1];
	char	newpath[MAX_PATHSIZE+1];
	char	newfile[MAX_FNAMESIZE+1];
	dki_t	*next;
	const	char	**pext;
	static	const	char	*ext[] = {
		DKI_KEY_FILEEXT, DKI_PUB_FILEEXT, 
		DKI_ACT_FILEEXT, DKI_DEP_FILEEXT, 
		NULL
	};

	if ( dkp == NULL )
		return NULL;

	strncpy (newfile, dkp->fname, sizeof (newfile));
	*newfile = tolower (*newfile);
	for ( pext = ext; *pext; pext++ )
	{
		pathname (path, sizeof (path), dkp->dname, dkp->fname, *pext);
		if ( fileexist (path) )
		{
			pathname (newpath, sizeof (newpath), dkp->dname, newfile, *pext);
			
			dbg_val2 ("dki_remove: %s ==> %s \n", path, newpath);
			rename (path, newpath);
		}
	}
	next = dkp->next;
	dki_free (dkp);

	return next;
}

/*****************************************************************
**	dki_destroy ()
**	delete files associated with key and free allocated memory
*****************************************************************/
dki_t	*dki_destroy (dki_t *dkp)
{
	char	path[MAX_PATHSIZE+1];
	dki_t	*next;
	const	char	**pext;
	static	const	char	*ext[] = {
		DKI_KEY_FILEEXT, DKI_PUB_FILEEXT, 
		DKI_ACT_FILEEXT, DKI_DEP_FILEEXT, 
		NULL
	};

	if ( dkp == NULL )
		return NULL;

	for ( pext = ext; *pext; pext++ )
	{
		pathname (path, sizeof (path), dkp->dname, dkp->fname, *pext);
		if ( fileexist (path) )
		{
			dbg_val ("dki_remove: %s \n", path);
			unlink (path);
		}
	}
	next = dkp->next;
	dki_free (dkp);

	return next;
}

/*****************************************************************
**	dki_algo2str ()
**	return a string describing the key algorithm
*****************************************************************/
char	*dki_algo2str (int algo)
{
	switch ( algo )
	{
	case DK_ALGO_RSA:		return ("RSAMD5");
	case DK_ALGO_DH:		return ("DH");
	case DK_ALGO_DSA:		return ("DSA");
	case DK_ALGO_EC:		return ("EC");
	case DK_ALGO_RSASHA1:		return ("RSASHA1");
	case DK_ALGO_NSEC3DSA:		return ("NSEC3DSA");
	case DK_ALGO_NSEC3RSASHA1:	return ("NSEC3RSASHA1");
	case DK_ALGO_RSASHA256:		return ("RSASHA256");
	case DK_ALGO_RSASHA512:		return ("RSASHA512");
	}
	return ("unknown");
}

/*****************************************************************
**	dki_algo2sstr ()
**	return a short string describing the key algorithm
*****************************************************************/
char	*dki_algo2sstr (int algo)
{
	switch ( algo )
	{
	case DK_ALGO_RSA:		return ("RSAMD5");
	case DK_ALGO_DH:		return ("DH");
	case DK_ALGO_DSA:		return ("DSA");
	case DK_ALGO_EC:		return ("EC");
	case DK_ALGO_RSASHA1:		return ("RSASHA1");
	case DK_ALGO_NSEC3DSA:		return ("N3DSA");
	case DK_ALGO_NSEC3RSASHA1:	return ("N3RSA1");
	case DK_ALGO_RSASHA256:		return ("RSASHA2");
	case DK_ALGO_RSASHA512:		return ("RSASHA5");
	}
	return ("unknown");
}

/*****************************************************************
**	dki_geterrstr ()
**	return error string 
*****************************************************************/
const	char	*dki_geterrstr ()
{
	return dki_estr;
}

/*****************************************************************
**	dki_prt_dnskey ()
*****************************************************************/
int	dki_prt_dnskey (const dki_t *dkp, FILE *fp)
{
	return dki_prt_dnskeyttl (dkp, fp, 0);
}

/*****************************************************************
**	dki_prt_dnskeyttl ()
*****************************************************************/
int	dki_prt_dnskeyttl (const dki_t *dkp, FILE *fp, int ttl)
{
	char	*p;

	if ( dkp == NULL )
		return 0;

	fprintf (fp, "%s ", dkp->name);
	if ( ttl > 0 )
		fprintf (fp, "%d ", ttl);
	fprintf (fp, "IN DNSKEY  ");
	fprintf (fp, "%d 3 %d (", dkp->flags, dkp->algo);
	fprintf (fp, "\n\t\t\t"); 
	for ( p = dkp->pubkey; *p ; p++ )
		if ( *p == ' ' )
			fprintf (fp, "\n\t\t\t"); 
		else
			putc (*p, fp);
	fprintf (fp, "\n\t\t");
	if ( dki_isrevoked (dkp) )
		fprintf (fp, ") ; key id = %u (original key id = %u)", (dkp->tag + 128) % 65535, dkp->tag); 
	else
		fprintf (fp, ") ; key id = %u", dkp->tag); 
	fprintf (fp, "\n"); 

	return 1;
}

/*****************************************************************
**	dki_prt_dnskey_raw ()
*****************************************************************/
int	dki_prt_dnskey_raw (const dki_t *dkp, FILE *fp)
{
	int	days;

	if ( dkp == NULL )
		return 0;

	if ( dkp->gentime  )
		fprintf (fp, ";%%\tgenerationtime=%s\n", time2isostr (dkp->gentime, 's'));
	if ( (days = dki_lifetimedays (dkp)) )
		fprintf (fp, ";%%\tlifetime=%dd\n", days);
	if ( dkp->exptime  )
		fprintf (fp, ";%%\texpirationtime=%s\n", time2isostr (dkp->exptime, 's'));

	fprintf (fp, "%s ", dkp->name);
#if 0
	if ( ttl > 0 )
		fprintf (fp, "%d ", ttl);
#endif
	fprintf (fp, "IN DNSKEY  ");
	fprintf (fp, "%d 3 %d ", dkp->flags, dkp->algo);
	fprintf (fp, "%s\n", dkp->pubkey); 

	return 1;
}

/*****************************************************************
**	dki_prt_comment ()
*****************************************************************/
int	dki_prt_comment (const dki_t *dkp, FILE *fp)
{
	int	len = 0;

	if ( dkp == NULL )
		return len;
	len += fprintf (fp, "; %s  ", dkp->name);
	len += fprintf (fp, "tag=%u  ", dkp->tag);
	len += fprintf (fp, "algo=%s  ", dki_algo2str(dkp->algo));
	len += fprintf (fp, "generated %s\n", time2str (dkp->time, 's')); 

	return len;
}

/*****************************************************************
**	dki_prt_trustedkey ()
*****************************************************************/
int	dki_prt_trustedkey (const dki_t *dkp, FILE *fp)
{
	char	*p;
	int	spaces;
	int	len = 0;

	if ( dkp == NULL )
		return len;
	len += fprintf (fp, "\"%s\"  ", dkp->name);
	spaces = 22 - (strlen (dkp->name) + 3);
	len += fprintf (fp, "%*s", spaces > 0 ? spaces : 0 , " ");
	len += fprintf (fp, "%d 3 %d ", dkp->flags, dkp->algo);
	if ( spaces < 0 )
		len += fprintf (fp, "\n\t\t\t%7s", " "); 
	len += fprintf (fp, "\"");
	for ( p = dkp->pubkey; *p ; p++ )
		if ( *p == ' ' )
			len += fprintf (fp, "\n\t\t\t\t"); 
		else
			putc (*p, fp), len += 1;

	if ( dki_isrevoked (dkp) )
		len += fprintf (fp, "\" ; # key id = %u (original key id = %u)\n\n", (dkp->tag + 128) % 65535, dkp->tag); 
	else
		len += fprintf (fp, "\" ; # key id = %u\n\n", dkp->tag); 
	return len;
}


/*****************************************************************
**	dki_cmp () 	return <0 | 0 | >0
*****************************************************************/
int	dki_cmp (const dki_t *a, const dki_t *b)
{
	int	res;

	if ( a == NULL ) return -1;
	if ( b == NULL ) return 1;

	/* sort by domain name, */
	if ( (res = domaincmp (a->name, b->name)) != 0 )
		return res; 

	/* then by key type, */
	if ( (res = dki_isksk (b) - dki_isksk (a)) != 0 )
		return res;

	/* and last by creation time,  */
	return (ulong)a->time - (ulong)b->time;
}

#if defined(USE_TREE) && USE_TREE
/*****************************************************************
**	dki_allcmp () 	return <0 | 0 | >0
*****************************************************************/
int	dki_allcmp (const dki_t *a, const dki_t *b)
{
	int	res;

	if ( a == NULL ) return -1;
	if ( b == NULL ) return 1;

// fprintf (stderr, "dki_allcmp %s, %s)\n", a->name, b->name);
	/* sort by domain name, */
	if ( (res = domaincmp (a->name, b->name)) != 0 )
		return res; 

	/* then by key type, */
	if ( (res = dki_isksk (b) - dki_isksk (a)) != 0 )
		return res;

	/* creation time,  */
	if ( (res = (ulong)a->time - (ulong)b->time) != 0 )
		return res;

	/* and last by tag */
	return a->tag - b->tag;
}

/*****************************************************************
**	dki_namecmp () 	return <0 | 0 | >0
*****************************************************************/
int	dki_namecmp (const dki_t *a, const dki_t *b)
{
	if ( a == NULL ) return -1;
	if ( b == NULL ) return 1;

	return domaincmp (a->name, b->name);
}

/*****************************************************************
**	dki_revnamecmp () 	return <0 | 0 | >0
*****************************************************************/
int	dki_revnamecmp (const dki_t *a, const dki_t *b)
{
	if ( a == NULL ) return -1;
	if ( b == NULL ) return 1;

	return domaincmp_dir (a->name, b->name, 0);
}

/*****************************************************************
**	dki_tagcmp () 	return <0 | 0 | >0
*****************************************************************/
int	dki_tagcmp (const dki_t *a, const dki_t *b)
{
	if ( a == NULL ) return -1;
	if ( b == NULL ) return 1;

	return a->tag - b->tag;
}
#endif

/*****************************************************************
**	dki_timecmp ()
*****************************************************************/
int	dki_timecmp (const dki_t *a, const dki_t *b)
{
	if ( a == NULL ) return -1;
	if ( b == NULL ) return 1;

	return ((ulong)a->time - (ulong)b->time);
}

/*****************************************************************
**	dki_algo ()	return the algorithm of the key
*****************************************************************/
time_t	dki_algo (const dki_t *dkp)
{
	assert (dkp != NULL);
	return (dkp->algo);
}

/*****************************************************************
**	dki_time ()	return the timestamp of the key
*****************************************************************/
time_t	dki_time (const dki_t *dkp)
{
	assert (dkp != NULL);
	return (dkp->time);
}

/*****************************************************************
**	dki_exptime ()	return the expiration timestamp of the key
*****************************************************************/
time_t	dki_exptime (const dki_t *dkp)
{
	assert (dkp != NULL);
	return (dkp->exptime);
}

/*****************************************************************
**	dki_lifetime (dkp)	return the lifetime of the key in sec!
*****************************************************************/
time_t	dki_lifetime (const dki_t *dkp)
{
	assert (dkp != NULL);
	return (dkp->lifetime);
}

/*****************************************************************
**	dki_lifetimedays (dkp)	return the lifetime of the key in days!
*****************************************************************/
ushort	dki_lifetimedays (const dki_t *dkp)
{
	assert (dkp != NULL);
	return (dkp->lifetime / DAYSEC);
}

/*****************************************************************
**	dki_gentime (dkp)	return the generation timestamp of the key
*****************************************************************/
time_t	dki_gentime (const dki_t *dkp)
{
	assert (dkp != NULL);
	return (dkp->gentime > 0L ? dkp->gentime: dkp->time);
}

/*****************************************************************
**	dki_setlifetime (dkp, int days)
**	set the lifetime in days (and also the gentime if not set)
**	return the old lifetime of the key in days!
*****************************************************************/
ushort	dki_setlifetime (dki_t *dkp, int days)
{
	ulong	lifetsec;
	char	path[MAX_PATHSIZE+1];

	assert (dkp != NULL);

	lifetsec = dkp->lifetime;		/* old lifetime */
	dkp->lifetime = days * DAYSEC;		/* set new lifetime */

	dbg_val1 ("dki_setlifetime (%d)\n", days);
	if ( lifetsec == 0 )	/* initial setup (old lifetime was zero)? */
		dkp->gentime = dkp->time;

	pathname (path, sizeof (path), dkp->dname, dkp->fname, DKI_KEY_FILEEXT);
	dki_writeinfo (dkp, path);

	return (lifetsec / DAYSEC);
}

/*****************************************************************
**	dki_setexptime (dkp, time_t sec)
**	set the expiration time of the key in seconds since the epoch
**	return the old exptime 
*****************************************************************/
time_t	dki_setexptime (dki_t *dkp, time_t sec)
{
	char	path[MAX_PATHSIZE+1];
	time_t	oldexptime;

	assert (dkp != NULL);

	dbg_val1 ("dki_setexptime (%ld)\n", sec);
	oldexptime = dkp->exptime;
	dkp->exptime = sec;

	pathname (path, sizeof (path), dkp->dname, dkp->fname, DKI_KEY_FILEEXT);
	dki_writeinfo (dkp, path);

#if 0	/* not necessary ? */
	touch (path, time (NULL));
#endif
	return (oldexptime);
}

/*****************************************************************
**	dki_age ()	return age of key in seconds since 'curr'
*****************************************************************/
int	dki_age (const dki_t *dkp, time_t curr)
{
	assert (dkp != NULL);
	return ((ulong)curr - (ulong)dkp->time);
}

/*****************************************************************
**	dki_getflag ()	return the flags field of a key 
*****************************************************************/
dk_flag_t	dki_getflag (const dki_t *dkp, time_t curr)
{
	return dkp->flags;
}

/*****************************************************************
**	dki_setflag ()	set a flag of a key 
*****************************************************************/
dk_flag_t	dki_setflag (dki_t *dkp, dk_flag_t flag)
{
	return dkp->flags |= (ushort)flag;
}

/*****************************************************************
**	dki_unsetflag ()	unset a flag of a key 
*****************************************************************/
dk_flag_t	dki_unsetflag (dki_t *dkp, dk_flag_t flag)
{
	return dkp->flags &= ~((ushort)flag);
}

/*****************************************************************
**	dki_isksk ()
*****************************************************************/
int	dki_isksk (const dki_t *dkp)
{
	assert (dkp != NULL);
	return (dkp->flags & DK_FLAG_KSK) == DK_FLAG_KSK;
}

/*****************************************************************
**	dki_isrevoked ()
*****************************************************************/
int	dki_isrevoked (const dki_t *dkp)
{
	assert (dkp != NULL);
	return (dkp->flags & DK_FLAG_REVOKE) == DK_FLAG_REVOKE;
}

/*****************************************************************
**	dki_isdepreciated ()
*****************************************************************/
int	dki_isdepreciated (const dki_t *dkp)
{
	return dki_status (dkp) == DKI_DEPRECIATED;
}

/*****************************************************************
**	dki_isactive ()
*****************************************************************/
int	dki_isactive (const dki_t *dkp)
{
	return dki_status (dkp) == DKI_ACTIVE;
}

/*****************************************************************
**	dki_ispublished ()
*****************************************************************/
int	dki_ispublished (const dki_t *dkp)
{
	return dki_status (dkp) == DKI_PUBLISHED;
}


/*****************************************************************
**	dki_status ()	return key status
*****************************************************************/
dk_status_t	dki_status (const dki_t *dkp)
{
	assert (dkp != NULL);
	return (dkp->status);
}

/*****************************************************************
**	dki_statusstr ()	return key status as string
*****************************************************************/
const	char	*dki_statusstr (const dki_t *dkp)
{
	assert (dkp != NULL);
	switch ( dkp->status )
	{
	case DKI_ACT:	return "active";
	case DKI_PUB:   if ( dki_isksk (dkp) )
				return "standby";
			else
				return "published";
	case DKI_DEP:   return "depreciated";
	case DKI_REV:   return "revoked";
	case DKI_SEP:   return "sep";
	}
	return "unknown";
}

/*****************************************************************
**	dki_add ()	add a key to the given list
*****************************************************************/
dki_t	*dki_add (dki_t **list, dki_t *new)
{
	dki_t	*curr;
	dki_t	*last;

	if ( list == NULL )
		return NULL;
	if ( new == NULL )
		return *list;

	last = curr = *list;
	while ( curr && dki_cmp (curr, new) < 0 )
	{
		last = curr;
		curr = curr->next;
	}

	if ( curr == *list )	/* add node at start of list */
		*list = new;
	else			/* add node at end or between two nodes */
		last->next = new;
	new->next = curr;
	
	return *list;
}

/*****************************************************************
**	dki_search ()	search a key with the given tag, or the first
**			occurence of a key with the given name
*****************************************************************/
const dki_t	*dki_search (const dki_t *list, int tag, const char *name)
{
	const dki_t	*curr;

	curr = list;
	if ( tag )
		while ( curr && (tag != curr->tag ||
				(name && *name && strcmp (name, curr->name) != 0)) )
			curr = curr->next;
	else if ( name && *name )
		while ( curr && strcmp (name, curr->name) != 0 )
			curr = curr->next;
	else
		curr = NULL;

	return curr;
}

#if defined(USE_TREE) && USE_TREE
/*****************************************************************
**	dki_tadd ()	add a key to the given tree
*****************************************************************/
dki_t	*dki_tadd (dki_t **tree, dki_t *new, int sub_before)
{
	dki_t	**p;

	if ( sub_before )
		p = tsearch (new, tree, dki_namecmp);
	else
		p = tsearch (new, tree, dki_revnamecmp);
	if ( *p == new )
		dbg_val ("dki_tadd: New entry %s added\n", new->name);
	else
	{
		dbg_val ("dki_tadd: New key added to %s\n", new->name);
		dki_add (p, new);
	}

	return *p;
}

/*****************************************************************
**	dki_tsearch ()	search a key with the given tag, or the first
**			occurence of a key with the given name
*****************************************************************/
const dki_t	*dki_tsearch (const dki_t *tree, int tag, const char *name)
{
	dki_t	search;
	dki_t	**p;

	search.tag = tag;
	snprintf (search.name, sizeof (search.name), "%s", name);
	p = tfind (&search, &tree, dki_namecmp);
	if ( p == NULL )
		return NULL;

	return dki_search (*p, tag, name);
}
#endif

/*****************************************************************
**	dki_find ()	find the n'th ksk or zsk key with given status
*****************************************************************/
const dki_t	*dki_find (const dki_t *list, int ksk, int status, int no)
{
	const	dki_t	*dkp;
	const	dki_t	*last;

	last = NULL;
	for ( dkp = list; no > 0 && dkp; dkp = dkp->next )
		if ( dki_isksk (dkp) == ksk && dki_status (dkp) == status )
		{
			no--;
			last = dkp;
		}

	return last;
}

/*****************************************************************
**	dki_findalgo ()	find the n'th ksk or zsk key with given
**			algorithm and status
*****************************************************************/
const dki_t	*dki_findalgo (const dki_t *list, int ksk, int alg, int status, int no)
{
	const	dki_t	*dkp;
	const	dki_t	*last;

	last = NULL;
	for ( dkp = list; no > 0 && dkp; dkp = dkp->next )
		if ( dki_isksk (dkp) == ksk && dki_algo (dkp) == alg &&
						dki_status (dkp) == status )
		{
			no--;
			last = dkp;
		}

	return last;
}
