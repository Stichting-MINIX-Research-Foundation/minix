/* This file was part of the etherlib package. */

#include <stdio.h>

#ifdef _MINIX
#include <sys/types.h>
#include <stdlib.h>

#include <net/gen/ether.h>
#include <net/gen/if_ether.h>

#define ETHERSTRLEN 18			/* max length of "xx:xx:xx:xx:xx:xx" */
#define ether_addr	ether_addr_t
#define bytes		ea_addr
char *ether_e2a _ARGS(( ether_addr_t *a, char *e ));
#else
#include "libether.h"
#endif

char *
ether_e2a (addr, estring)
ether_addr *addr;
char *estring;
{
#ifdef lint
    char *sprintf ();
#endif
    if (estring == NULL)
	estring = (char *) malloc (ETHERSTRLEN);

    if (estring != NULL)
	(void) sprintf (estring, "%x:%x:%x:%x:%x:%x",
			addr->bytes[0], addr->bytes[1], addr->bytes[2],
			addr->bytes[3], addr->bytes[4], addr->bytes[5]);
    return (estring);
}

#ifndef ETHERDB

char *
ether_ntoa (addr)
ether_addr *addr;
{
    static char estring[ETHERSTRLEN];

    return (ether_e2a (addr, estring));
}

#endif
