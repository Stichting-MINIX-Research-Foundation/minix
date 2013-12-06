/*	$NetBSD: svc_fdset.c,v 1.1 2013/03/05 19:55:23 christos Exp $	*/

#include <sys/cdefs.h>
__RCSID("$NetBSD: svc_fdset.c,v 1.1 2013/03/05 19:55:23 christos Exp $");

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>

#include "svc_fdset.h"

static pthread_key_t fdsetkey;
static pthread_key_t fdmaxkey;
static fd_set thefdset;
static int thefdmax;

void
init_fdsets(void)
{

	pthread_key_create(&fdsetkey, NULL);
	pthread_key_create(&fdmaxkey, NULL);
}

void
alloc_fdset(void)
{
	fd_set *fdsetti;
	int *fdmax;

	fdsetti = malloc(sizeof(*fdsetti));
	memset(fdsetti, 0, sizeof(*fdsetti));
	pthread_setspecific(fdsetkey, fdsetti);

	fdmax = malloc(sizeof(*fdmax));
	memset(fdmax, 0, sizeof(*fdmax));
	pthread_setspecific(fdmaxkey, fdmax);
}

fd_set *
get_fdset(void)
{
	fd_set *rv;

	rv = pthread_getspecific(fdsetkey);
	if (rv)
		return rv;
	else
		return &thefdset;
}

int *
get_fdsetmax(void)
{
	int *rv;

	rv = pthread_getspecific(fdmaxkey);
	if (rv)
		return rv;
	else
		return &thefdmax;
}
