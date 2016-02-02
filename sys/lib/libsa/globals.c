/*	$NetBSD: globals.c,v 1.11 2014/03/29 14:30:16 jakllsch Exp $	*/

/*
 *	globals.c:
 *
 *	global variables should be separate, so nothing else
 *	must be included extraneously.
 */

#include <sys/param.h>
#include <net/if_ether.h>		/* for ETHER_ADDR_LEN */
#include <netinet/in.h>
#include <netinet/in_systm.h>

#include "stand.h"
#include "net.h"

u_char	bcea[ETHER_ADDR_LEN] = BA;	/* broadcast ethernet address */

char	rootpath[FNAME_SIZE];		/* root mount path */
char	bootfile[FNAME_SIZE];		/* bootp says to boot this */
char	hostname[FNAME_SIZE];		/* our hostname */
const char	*fsmod = NULL;		/* file system module name to load */
struct	in_addr myip;			/* my ip address */
struct	in_addr rootip;			/* root ip address */
struct	in_addr gateip;			/* swap ip address */
n_long	netmask = 0xffffff00;		/* subnet or net mask */
