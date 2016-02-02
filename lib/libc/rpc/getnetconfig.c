/*	$NetBSD: getnetconfig.c,v 1.22 2014/09/18 13:58:20 christos Exp $	*/

/*
 * Copyright (c) 2010, Oracle America, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials
 *       provided with the distribution.
 *     * Neither the name of the "Oracle America, Inc." nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *   FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *   COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 *   INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 *   GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *   INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 *   WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 *   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
#if 0
static        char sccsid[] = "@(#)getnetconfig.c	1.12 91/12/19 SMI";
#else
__RCSID("$NetBSD: getnetconfig.c,v 1.22 2014/09/18 13:58:20 christos Exp $");
#endif
#endif

/*
 * Copyright (c) 1989 by Sun Microsystems, Inc.
 */

#include "namespace.h"
#include "reentrant.h"
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <netconfig.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <rpc/rpc.h>
#include "rpc_internal.h"

#ifdef __weak_alias
__weak_alias(getnetconfig,_getnetconfig)
__weak_alias(setnetconfig,_setnetconfig)
__weak_alias(endnetconfig,_endnetconfig)
__weak_alias(getnetconfigent,_getnetconfigent)
__weak_alias(freenetconfigent,_freenetconfigent)
__weak_alias(nc_perror,_nc_perror)
__weak_alias(nc_sperror,_nc_sperror)
#endif

/*
 * The five library routines in this file provide application access to the
 * system network configuration database, /etc/netconfig.  In addition to the
 * netconfig database and the routines for accessing it, the environment
 * variable NETPATH and its corresponding routines in getnetpath.c may also be
 * used to specify the network transport to be used.
 */


/*
 * netconfig errors
 */

#define NC_NONETCONFIG	ENOENT
#define NC_NOMEM	ENOMEM
#define NC_NOTINIT	EINVAL	    /* setnetconfig was not called first */
#define NC_BADFILE	EBADF	    /* format for netconfig file is bad */

/*
 * semantics as strings (should be in netconfig.h)
 */
#define NC_TPI_CLTS_S	    "tpi_clts"
#define	NC_TPI_COTS_S	    "tpi_cots"
#define	NC_TPI_COTS_ORD_S   "tpi_cots_ord"
#define	NC_TPI_RAW_S        "tpi_raw"

/*
 * flags as characters (also should be in netconfig.h)
 */
#define	NC_NOFLAG_C	'-'
#define	NC_VISIBLE_C	'v'
#define	NC_BROADCAST_C	'b'

/*
 * Character used to indicate there is no name-to-address lookup library
 */
#define NC_NOLOOKUP	"-"

static const char * const _nc_errors[] = {
	"Netconfig database not found",
	"Not enough memory",
	"Not initialized",
	"Netconfig database has invalid format"
};

struct netconfig_info {
	int		eof;	/* all entries has been read */
	int		ref;	/* # of times setnetconfig() has been called */
	struct netconfig_list	*head;	/* head of the list */
	struct netconfig_list	*tail;	/* last of the list */
};

struct netconfig_list {
	char			*linep;	/* hold line read from netconfig */
	struct netconfig	*ncp;
	struct netconfig_list	*next;
};

struct netconfig_vars {
	    int   valid;	/* token that indicates valid netconfig_vars */
	    int   flag;		/* first time flag */
	    struct netconfig_list *nc_configs;
	   			 /* pointer to the current netconfig entry */
};

#define NC_VALID	0xfeed
#define NC_STORAGE	0xf00d
#define NC_INVALID	0


static int *__nc_error(void);
static int parse_ncp(char *, struct netconfig *);
static struct netconfig *dup_ncp(struct netconfig *);


static FILE *nc_file;		/* for netconfig db */
static struct netconfig_info	ni = { 0, 0, NULL, NULL};

#define MAXNETCONFIGLINE    1000

#ifdef _REENTRANT
static thread_key_t nc_key;
static once_t nc_once = ONCE_INITIALIZER;

static void 
__nc_error_setup(void)
{
	thr_keycreate(&nc_key, free);
}
#endif

static int *
__nc_error(void)
{
#ifdef _REENTRANT
	int *nc_addr = NULL;
#endif
	static int nc_error = 0;

#ifdef _REENTRANT
	if (__isthreaded == 0)
		return &nc_error;
	thr_once(&nc_once, __nc_error_setup);
	nc_addr = thr_getspecific(nc_key) ;
	if (nc_addr == NULL) {
		nc_addr = malloc(sizeof (int));
		if (nc_addr == NULL)
			return &nc_error;
		if (thr_setspecific(nc_key, (void *) nc_addr) != 0) {
			if (nc_addr)
				free(nc_addr);
			return &nc_error;
		}
		*nc_addr = 0;
	}
	return nc_addr;
#else
	return &nc_error;
#endif
}

#define nc_error        (*(__nc_error()))
/*
 * A call to setnetconfig() establishes a /etc/netconfig "session".  A session
 * "handle" is returned on a successful call.  At the start of a session (after
 * a call to setnetconfig()) searches through the /etc/netconfig database will
 * proceed from the start of the file.  The session handle must be passed to
 * getnetconfig() to parse the file.  Each call to getnetconfig() using the
 * current handle will process one subsequent entry in /etc/netconfig.
 * setnetconfig() must be called before the first call to getnetconfig().
 * (Handles are used to allow for nested calls to setnetpath()).
 *
 * A new session is established with each call to setnetconfig(), with a new
 * handle being returned on each call.  Previously established sessions remain
 * active until endnetconfig() is called with that session's handle as an
 * argument.
 *
 * setnetconfig() need *not* be called before a call to getnetconfigent().
 * setnetconfig() returns a NULL pointer on failure (for example, if
 * the netconfig database is not present).
 */
void *
setnetconfig(void)
{
	struct netconfig_vars *nc_vars;

	if ((nc_vars = malloc(sizeof(*nc_vars))) == NULL) {
		return(NULL);
	}

	/*
	 * For multiple calls, i.e. nc_file is not NULL, we just return the
	 * handle without reopening the netconfig db.
	 */
	ni.ref++;
	if ((nc_file != NULL) || (nc_file = fopen(NETCONFIG, "re")) != NULL) {
		nc_vars->valid = NC_VALID;
		nc_vars->flag = 0;
		nc_vars->nc_configs = ni.head;
		return ((void *)nc_vars);
	}
	ni.ref--;
	nc_error = NC_NONETCONFIG;
	free(nc_vars);
	return (NULL);
}


/*
 * When first called, getnetconfig() returns a pointer to the first entry in
 * the netconfig database, formatted as a struct netconfig.  On each subsequent
 * call, getnetconfig() returns a pointer to the next entry in the database.
 * getnetconfig() can thus be used to search the entire netconfig file.
 * getnetconfig() returns NULL at end of file.
 */

struct netconfig *
getnetconfig(void *handlep)
{
	struct netconfig_vars *ncp = (struct netconfig_vars *)handlep;
	char *stringp;		/* tmp string pointer */
	struct netconfig_list	*list;
	struct netconfig *np;

	/*
	 * Verify that handle is valid
	 */
	if (ncp == NULL || nc_file == NULL) {
		nc_error = NC_NOTINIT;
		return (NULL);
	}

	switch (ncp->valid) {
	case NC_VALID:
		/*
		 * If entry has already been read into the list,
		 * we return the entry in the linked list.
		 * If this is the first time call, check if there are any
		 * entries in linked list.  If no entries, we need to read the
		 * netconfig db.
		 * If we have been here and the next entry is there, we just
		 * return it.
		 */
		if (ncp->flag == 0) {	/* first time */
			ncp->flag = 1;
			ncp->nc_configs = ni.head;
			if (ncp->nc_configs != NULL) /* entry already exist */
				return(ncp->nc_configs->ncp);
		}
		else if (ncp->nc_configs != NULL &&
		    ncp->nc_configs->next != NULL) {
			ncp->nc_configs = ncp->nc_configs->next;
			return(ncp->nc_configs->ncp);
		}

		/*
		 * If we cannot find the entry in the list and is end of file,
		 * we give up.
		 */
		if (ni.eof == 1)
			return(NULL);
		break;
	default:
		nc_error = NC_NOTINIT;
		return (NULL);
	}

	stringp = malloc(MAXNETCONFIGLINE);
	if (stringp == NULL)
		return (NULL);

#ifdef MEM_CHK
	if (malloc_verify() == 0) {
		fprintf(stderr, "memory heap corrupted in getnetconfig\n");
		exit(1);
	}
#endif

	/*
	 * Read a line from netconfig file.
	 */
	do {
		if (fgets(stringp, MAXNETCONFIGLINE, nc_file) == NULL) {
			free(stringp);
			ni.eof = 1;
			return (NULL);
		}
	} while (*stringp == '#');

	list = malloc(sizeof(*list));
	if (list == NULL) {
		free(stringp);
		return(NULL);
	}
	np = malloc(sizeof(*np));
	if (np == NULL) {
		free(stringp);
		free(list);
		return(NULL);
	}
	list->ncp = np;
	list->next = NULL;
	list->ncp->nc_lookups = NULL;
	list->linep = stringp;
	if (parse_ncp(stringp, list->ncp) == -1) {
		free(stringp);
		free(np);
		free(list);
		return (NULL);
	} else {
		/*
		 * If this is the first entry that's been read, it is the
		 * head of the list.  If not, put the entry at the end of
		 * the list.  Reposition the current pointer of the handle to
		 * the last entry in the list.
		 */
		if (ni.head == NULL)		/* first entry */
			ni.head = ni.tail = list;
		else {
			ni.tail->next = list;
			ni.tail = ni.tail->next;
		}
		ncp->nc_configs = ni.tail;
		return(ni.tail->ncp);
	}
}

/*
 * endnetconfig() may be called to "unbind" or "close" the netconfig database
 * when processing is complete, releasing resources for reuse.  endnetconfig()
 * may not be called before setnetconfig().  endnetconfig() returns 0 on
 * success and -1 on failure (for example, if setnetconfig() was not called
 * previously).
 */
int
endnetconfig(void *handlep)
{
	struct netconfig_vars *nc_handlep = (struct netconfig_vars *)handlep;

	struct netconfig_list *q, *p;

	/*
	 * Verify that handle is valid
	 */
	if (nc_handlep == NULL || (nc_handlep->valid != NC_VALID &&
	    nc_handlep->valid != NC_STORAGE)) {
		nc_error = NC_NOTINIT;
		return (-1);
	}

	/*
	 * Return 0 if anyone still needs it.
	 */
	nc_handlep->valid = NC_INVALID;
	nc_handlep->flag = 0;
	nc_handlep->nc_configs = NULL;
	if (--ni.ref > 0) {
		free(nc_handlep);
		return(0);
	}

	/*
	 * Noone needs these entries anymore, then frees them.
	 * Make sure all info in netconfig_info structure has been
	 * reinitialized.
	 */
	q = p = ni.head;
	ni.eof = ni.ref = 0;
	ni.head = NULL;
	ni.tail = NULL;
	while (q) {
		p = q->next;
		if (q->ncp->nc_lookups != NULL) free(q->ncp->nc_lookups);
		free(q->ncp);
		free(q->linep);
		free(q);
		q = p;
	}
	free(nc_handlep);

	fclose(nc_file);
	nc_file = NULL;
	return (0);
}

/*
 * getnetconfigent(netid) returns a pointer to the struct netconfig structure
 * corresponding to netid.  It returns NULL if netid is invalid (that is, does
 * not name an entry in the netconfig database).  It returns NULL and sets
 * errno in case of failure (for example, if the netconfig database cannot be
 * opened).
 */

struct netconfig *
getnetconfigent(const char *netid)
{
	FILE *file;			/* NETCONFIG db's file pointer */
	char *linep;			/* holds current netconfig line */
	char *stringp;			/* temporary string pointer */
	struct netconfig *ncp = NULL;   /* returned value */
	struct netconfig_list *list;	/* pointer to cache list */

	if (netid == NULL || strlen(netid) == 0)
		return (NULL);

	/*
	 * Look up table if the entries have already been read and parsed in
	 * getnetconfig(), then copy this entry into a buffer and return it.
	 * If we cannot find the entry in the current list and there are more
	 * entries in the netconfig db that has not been read, we then read the
	 * db and try find the match netid.
	 * If all the netconfig db has been read and placed into the list and
	 * there is no match for the netid, return NULL.
	 */
	if (ni.head != NULL) {
		for (list = ni.head; list; list = list->next) {
			if (strcmp(list->ncp->nc_netid, netid) == 0)
			    return(dup_ncp(list->ncp));
		}
		if (ni.eof == 1)	/* that's all the entries */
			return(NULL);
	}

	if ((file = fopen(NETCONFIG, "re")) == NULL)
	    return (NULL);

	if ((linep = malloc(MAXNETCONFIGLINE)) == NULL) {
		fclose(file);
		return (NULL);
	}
	do {
		ptrdiff_t len;
		char *tmpp;	/* tmp string pointer */

		do {
			if ((stringp = fgets(linep, MAXNETCONFIGLINE, file))
			    == NULL)
				break;
		} while (*stringp == '#');
		if (stringp == NULL)	/* eof */
			break;
		if ((tmpp = strpbrk(stringp, "\t ")) == NULL) {
					/* can't parse file */
			nc_error = NC_BADFILE;
			break;
		}
		if (strlen(netid) == (size_t) (len = tmpp - stringp) &&	/* a match */
		    strncmp(stringp, netid, (size_t)len) == 0) {
			if ((ncp = malloc(sizeof(*ncp))) == NULL)
				break;
			ncp->nc_lookups = NULL;
			if (parse_ncp(linep, ncp) == -1) {
				free(ncp);
				ncp = NULL;
			}
			break;
		}
	} while (stringp != NULL);
	if (ncp == NULL)
		free(linep);
	fclose(file);
	return(ncp);
}

/*
 * freenetconfigent(netconfigp) frees the netconfig structure pointed to by
 * netconfigp (previously returned by getnetconfigent()).
 */

void
freenetconfigent(struct netconfig *netconfigp)
{
	if (netconfigp != NULL) {
				/* holds all netconfigp's strings */
		free(netconfigp->nc_netid);
		if (netconfigp->nc_lookups != NULL)
			free(netconfigp->nc_lookups);
		free(netconfigp);
	}
}

/*
 * Parse line and stuff it in a struct netconfig
 * Typical line might look like:
 *	udp tpi_cots vb inet udp /dev/udp /usr/lib/ip.so,/usr/local/ip.so
 *
 * We return -1 if any of the tokens don't parse, or malloc fails.
 *
 * Note that we modify stringp (putting NULLs after tokens) and
 * we set the ncp's string field pointers to point to these tokens within
 * stringp.
 */

static int
parse_ncp(
	char *stringp,		/* string to parse */
	struct netconfig *ncp)	/* where to put results */
{
	char    *tokenp;	/* for processing tokens */
	char    *lasts;

	_DIAGASSERT(stringp != NULL);
	_DIAGASSERT(ncp != NULL);

	nc_error = NC_BADFILE;
			/* nearly anything that breaks is for this reason */
	stringp[strlen(stringp)-1] = '\0';	/* get rid of newline */
	/* netid */
	if ((ncp->nc_netid = strtok_r(stringp, "\t ", &lasts)) == NULL)
		return (-1);

	/* semantics */
	if ((tokenp = strtok_r(NULL, "\t ", &lasts)) == NULL)
		return (-1);
	if (strcmp(tokenp, NC_TPI_COTS_ORD_S) == 0)
		ncp->nc_semantics = NC_TPI_COTS_ORD;
	else if (strcmp(tokenp, NC_TPI_COTS_S) == 0)
		ncp->nc_semantics = NC_TPI_COTS;
	else if (strcmp(tokenp, NC_TPI_CLTS_S) == 0)
		ncp->nc_semantics = NC_TPI_CLTS;
	else if (strcmp(tokenp, NC_TPI_RAW_S) == 0)
		ncp->nc_semantics = NC_TPI_RAW;
	else
		return (-1);

	/* flags */
	if ((tokenp = strtok_r(NULL, "\t ", &lasts)) == NULL)
		return (-1);
	for (ncp->nc_flag = NC_NOFLAG; *tokenp != '\0'; tokenp++) {
		switch (*tokenp) {
		case NC_NOFLAG_C:
			break;
		case NC_VISIBLE_C:
			ncp->nc_flag |= NC_VISIBLE;
			break;
		case NC_BROADCAST_C:
			ncp->nc_flag |= NC_BROADCAST;
			break;
		default:
			return (-1);
		}
	}
	/* protocol family */
	if ((ncp->nc_protofmly = strtok_r(NULL, "\t ", &lasts)) == NULL)
		return (-1);
	/* protocol name */
	if ((ncp->nc_proto = strtok_r(NULL, "\t ", &lasts)) == NULL)
		return (-1);
	/* network device */
	if ((ncp->nc_device = strtok_r(NULL, "\t ", &lasts)) == NULL)
		return (-1);
	if ((tokenp = strtok_r(NULL, "\t ", &lasts)) == NULL)
		return (-1);
	if (strcmp(tokenp, NC_NOLOOKUP) == 0) {
		ncp->nc_nlookups = 0;
		ncp->nc_lookups = NULL;
	} else {
		char *cp;	    /* tmp string */

		if (ncp->nc_lookups != NULL)	/* from last visit */
			free(ncp->nc_lookups);
		/* preallocate one string pointer */
		ncp->nc_lookups = malloc(sizeof(*ncp->nc_lookups));
		ncp->nc_nlookups = 0;
		while ((cp = tokenp) != NULL) {
			tokenp = _get_next_token(cp, ',');
			ncp->nc_lookups[(size_t)ncp->nc_nlookups++] = cp;
			ncp->nc_lookups = (char **)
			    realloc(ncp->nc_lookups,
			    (size_t)(ncp->nc_nlookups+1) *sizeof(char *));	
						/* for next loop */
		}
	}
	return (0);
}

/*
 * Returns a string describing the reason for failure.
 */
char *
nc_sperror(void)
{
	const char *message;

	switch(nc_error) {
	case NC_NONETCONFIG:
		message = _nc_errors[0];
		break;
	case NC_NOMEM:
		message = _nc_errors[1];
		break;
	case NC_NOTINIT:
		message = _nc_errors[2];
		break;
	case NC_BADFILE:
		message = _nc_errors[3];
		break;
	default:
		message = "Unknown network selection error";
	}
	return __UNCONST(message);
}

/*
 * Prints a message onto standard error describing the reason for failure.
 */
void
nc_perror(const char *s)
{

	_DIAGASSERT(s != NULL);

	fprintf(stderr, "%s: %s", s, nc_sperror());
}

/*
 * Duplicates the matched netconfig buffer.
 */
static struct netconfig *
dup_ncp(struct netconfig *ncp)
{
	struct netconfig	*p;
	char	*tmp;
	u_int	i;

	_DIAGASSERT(ncp != NULL);

	if ((tmp = malloc(MAXNETCONFIGLINE)) == NULL)
		return(NULL);
	if ((p = malloc(sizeof(*p))) == NULL) {
		free(tmp);
		return(NULL);
	}
	/*
	 * First we dup all the data from matched netconfig buffer.  Then we
	 * adjust some of the member pointer to a pre-allocated buffer where
	 * contains part of the data.
	 * To follow the convention used in parse_ncp(), we store all the
	 * necessary information in the pre-allocated buffer and let each
	 * of the netconfig char pointer member point to the right address
	 * in the buffer.
	 */
	*p = *ncp;
	p->nc_netid = (char *)strcpy(tmp,ncp->nc_netid);
	tmp = strchr(tmp, '\0') + 1;
	p->nc_protofmly = (char *)strcpy(tmp,ncp->nc_protofmly);
	tmp = strchr(tmp, '\0') + 1;
	p->nc_proto = (char *)strcpy(tmp,ncp->nc_proto);
	tmp = strchr(tmp, '\0') + 1;
	p->nc_device = (char *)strcpy(tmp,ncp->nc_device);
	p->nc_lookups = malloc((size_t)(p->nc_nlookups+1) * sizeof(char *));
	if (p->nc_lookups == NULL) {
		free(p->nc_netid);
		free(p);
		return(NULL);
	}
	for (i=0; i < p->nc_nlookups; i++) {
		tmp = strchr(tmp, '\0') + 1;
		p->nc_lookups[i] = strcpy(tmp,ncp->nc_lookups[i]);
	}
	return(p);
}
