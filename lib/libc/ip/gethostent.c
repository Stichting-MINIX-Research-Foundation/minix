/*	gethostent() - Interface to /etc/hosts		Author: Kees J. Bot
 *								31 May 1999
 */

/* Prefix the functions defined here with underscores to distinguish them
 * from the newer replacements in the resolver library.
 */
#define sethostent	_sethostent
#define endhostent	_endhostent
#define gethostent	_gethostent
#define gethostbyname	_gethostbyname
#define gethostbyaddr	_gethostbyaddr

#define nil 0
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <net/gen/netdb.h>
#include <net/gen/in.h>
#include <net/gen/inet.h>
#include <net/gen/socket.h>

#define arraysize(a)	(sizeof(a) / sizeof((a)[0]))
#define arraylimit(a)	((a) + arraysize(a))
#define isspace(c)	((unsigned) (c) <= ' ')

static char HOSTS[]= _PATH_HOSTS;
static char *hosts= HOSTS;	/* Current hosts file. */
static FILE *hfp;		/* Open hosts file. */

void sethostent(int stayopen)
/* Start search.  (Same as ending it.) */
{
    endhostent();
}

void endhostent(void)
/* End search and reinitialize. */
{
    if (hfp != nil) {
	fclose(hfp);
	hfp= nil;
    }
    hosts= _PATH_HOSTS;
}

struct hostent *gethostent(void)
/* Return the next entry from the hosts files. */
{
    static char line[256];	/* One line in a hosts file. */
    static ipaddr_t addr;	/* IP address found first on the line. */
    static char *names[16];	/* Pointers to the words on the line. */
    static char *addrs[2]= {	/* List of IP addresses (just one.) */
	(char *) &addr,
	nil,
    };
    static struct hostent host = {
	nil,			/* h_name, will set to names[1]. */
	names + 2,		/* h_aliases, the rest of the names. */
	AF_INET,		/* h_addrtype */
	sizeof(ipaddr_t),	/* Size of an address in the address list. */
	addrs,			/* List of IP addresses. */
    };
    static char nexthosts[128];	/* Next hosts file to include. */
    char *lp, **np;
    int c;

    for (;;) {
	if (hfp == nil) {
	    /* No hosts file open, try to open the next one. */
	    if (hosts == 0) return nil;
	    if ((hfp= fopen(hosts, "r")) == nil) { hosts= nil; continue; }
	}

	/* Read a line. */
	lp= line;
	while ((c= getc(hfp)) != EOF && c != '\n') {
	    if (lp < arraylimit(line)) *lp++= c;
	}

	/* EOF?  Then close and prepare for reading the next file. */
	if (c == EOF) {
	    fclose(hfp);
	    hfp= nil;
	    hosts= nil;
	    continue;
	}

	if (lp == arraylimit(line)) continue;
	*lp= 0;

	/* Break the line up in words. */
	np= names;
	lp= line;
	for (;;) {
	    while (isspace(*lp) && *lp != 0) lp++;
	    if (*lp == 0 || *lp == '#') break;
	    if (np == arraylimit(names)) break;
	    *np++= lp;
	    while (!isspace(*lp) && *lp != 0) lp++;
	    if (*lp == 0) break;
	    *lp++= 0;
	}

	if (np == arraylimit(names)) continue;
	*np= nil;

	/* Special "include file" directive. */
	if (np == names + 2 && strcmp(names[0], "include") == 0) {
	    fclose(hfp);
	    hfp= nil;
	    hosts= nil;
	    if (strlen(names[1]) < sizeof(nexthosts)) {
		strcpy(nexthosts, names[1]);
		hosts= nexthosts;
	    }
	    continue;
	}

	/* At least two words, the first of which is an IP address. */
	if (np < names + 2) continue;
	if (!inet_aton((char *) names[0], &addr)) continue;
	host.h_name= (char *) names[1];

	return &host;
    }
}

/* Rest kept in reserve, we probably never need 'em. */
#if XXX
struct hostent *gethostbyname(const char *name)
{
    struct hostent *he;
    char **pa;
    char alias[256];
    char *domain;
    
    sethostent(0);
    while ((he= gethostent()) != nil) {
	if (strcasecmp(he->h_name, name) == 0) goto found;

	domain= strchr(he->h_name, '.');
	for (pa= he->h_aliases; *pa != nil; pa++) {
	    strcpy(alias, *pa);
	    if (domain != nil && strchr(alias, '.') == nil) {
		strcat(alias, domain);
	    }
	    if (strcasecmp(alias, name) == 0) goto found;
	}
    }
  found:
    endhostent();
    return he;
}

struct hostent *gethostbyaddr(const char *addr, int len, int type)
{
    struct hostent *he;

    sethostent(0);
    while ((he= gethostent()) != nil) {
	if (he->h_name[0] == '%') continue;
	if (type == AF_INET && memcmp(he->h_addr, addr, len) == 0) break;
    }
    endhostent();
    return he;
}
#endif
