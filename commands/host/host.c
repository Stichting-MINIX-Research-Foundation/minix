/*
 * Copyright (c) 1986 Regents of the University of California
 * All Rights Reserved
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and that due credit is given
 * to the University of California at Berkeley. The name of the University
 * may not be used to endorse or promote products derived from this
 * software without specific prior written permission. This software
 * is provided ``as is'' without express or implied warranty.
 */

/*
 * Actually, this program is from Rutgers University, however it is 
 * based on nslookup and other pieces of named tools, so it needs
 * that copyright notice.
 */

#include <stdio.h>
#include <sys/types.h>
#include <arpa/nameser.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <resolv.h>
#include <sys/param.h>
#include <strings.h>
#include <ctype.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/ioc_net.h>
#include <net/netlib.h>
#include <net/gen/in.h>
#include <net/gen/tcp.h>
#include <net/gen/tcp_io.h>

extern int h_errno;

#define NUMMX 50

#define  SUCCESS		0
#define  TIME_OUT		-1
#define  NO_INFO 		-2
#define  ERROR 			-3
#define  NONAUTH 		-4

#define NAME_LEN 256

#ifndef T_TXT
#define T_TXT 16
#endif
#ifndef NO_DATA
#define NO_DATA NO_ADDRESS
#endif
#ifndef C_HS
#define C_HS 4
#endif
#ifndef NOCHANGE
#define NOCHANGE 0xf
#endif

FILE *filePtr;

static struct __res_state orig;
static u8_t *cname = NULL;
int getclass = C_IN;
int gettype, getdeftype = T_A;
int verbose = 0;
int list = 0;
int server_specified = 0;

union querybuf;

int main( int c, char *v[] );

static int parsetype( char *s );
static int parseclass( char *s );
static void hperror( int err_no );
static void printanswer( struct hostent *hp );
static int ListHosts( char *namePtr, int queryType );
static int gethostinfo( char *name );
static int getdomaininfo( char *name, char *domain );
static int getinfo( char *name, char *domain, int type );
static int printinfo( union querybuf *answer, u8_t *eom, int filter, int
	isls );
static char *DecodeError( int result );
static u8_t *pr_rr( u8_t *cp, u8_t *msg, FILE *file, int filter );
static u8_t * pr_cdname( u8_t *cp, u8_t *msg, u8_t *name, int namelen );
static char *pr_class( int class );
static char *pr_type( int type );
static int tcpip_writeall( int fd, char *buf, unsigned siz );

int
main(c, v)
	int c;
	char **v;
{
	char *domain;
	struct in_addr addr;
	register struct hostent *hp;
	register char *s, *p;
	register int inverse = 0;
	register int waitmode = 0;
	u8_t *oldcname;
	int ncnames;
	int isaddr;

	res_init();
	_res.retrans = 5;

	if (c < 2) {
		fprintf(stderr, "Usage: host [-w] [-v] [-r] [-d] [-V] [-t querytype] [-c class] [-a] host [server]\n  -w to wait forever until reply\n  -v for verbose output\n  -r to disable recursive processing\n  -d to turn on debugging output\n  -t querytype to look for a specific type of information\n  -c class to look for non-Internet data\n  -a is equivalent to '-v -t *'\n  -V to always use a virtual circuit\n");
		exit(1);
	}
	while (c > 2 && v[1][0] == '-') {
		if (strcmp (v[1], "-w") == 0) {
			_res.retry = 1;
			_res.retrans = 15;
			waitmode = 1;
			v++;
			c--;
		}
		else if (strcmp (v[1], "-r") == 0) {
			_res.options &= ~RES_RECURSE;
			v++;
			c--;
		}
		else if (strcmp (v[1], "-d") == 0) {
			_res.options |= RES_DEBUG;
			v++;
			c--;
		}
		else if (strcmp (v[1], "-v") == 0) {
			verbose = 1;
			v++;
			c--;
		}
		else if (strcmp (v[1], "-l") == 0) {
			list = 1;
			v++;
			c--;
		}
		else if (strncmp (v[1], "-t", 2) == 0) {
			v++;
			c--;
			gettype = parsetype(v[1]);
			v++;
			c--;
		}
		else if (strncmp (v[1], "-c", 2) == 0) {
			v++;
			c--;
			getclass = parseclass(v[1]);
			v++;
			c--;
		}
		else if (strcmp (v[1], "-a") == 0) {
			verbose = 1;
			gettype = T_ANY;
			v++;
			c--;
		}		
		else if (strcmp (v[1], "-V") == 0) {
			_res.options |= RES_USEVC;
			v++;
			c--;
		}
        }
	if (c > 2) {
		s = v[2];
		server_specified++;
		
		if ((p = strchr(s, ':')) != NULL) *p++ = 0;
		isaddr = inet_aton(s, &addr);
		if (p != NULL) p[-1] = ':';
		if (!isaddr) {
		  hp = gethostbyname(s);
		  if (hp == NULL) {
		    fprintf(stderr,"Error in looking up server name:\n");
		    hperror(h_errno);
		    exit(1);
		  }
		  memcpy(&_res.nsaddr.sin_addr, hp->h_addr, NS_INADDRSZ);
		  printf("Using domain server:\n");
		  printanswer(hp);
		}
		else {
		  _res.nsaddr.sin_family = AF_INET;
		  _res.nsaddr.sin_addr = addr;
		  _res.nsaddr.sin_port = htons(NAMESERVER_PORT);
		  printf("Using domain server %s:\n",
		         inet_ntoa(_res.nsaddr.sin_addr));

		}
	      }
	domain = v[1];
	if (strcmp (domain, ".") != 0 && inet_aton(domain, &addr)) {
	  static char ipname[sizeof("255.255.255.255.in-addr.arpa.")];
	  sprintf(ipname, "%d.%d.%d.%d.in-addr.arpa.",
	    ((unsigned char *) &addr)[3],
	    ((unsigned char *) &addr)[2],
	    ((unsigned char *) &addr)[1],
	    ((unsigned char *) &addr)[0]);
	  domain = ipname;
	  getdeftype = T_PTR;
	}

	hp = NULL;
	h_errno = TRY_AGAIN;
/*
 * we handle default domains ourselves, thank you
 */
	_res.options &= ~RES_DEFNAMES;

        if (list)
	  exit(ListHosts(domain, gettype ? gettype : getdeftype));
	oldcname = NULL;
	ncnames = 5;
	while (hp == NULL && h_errno == TRY_AGAIN) {
	  cname = NULL;
	  if (oldcname == NULL)
	    hp = (struct hostent *)gethostinfo(domain);
	  else
	    hp = (struct hostent *)gethostinfo((char *)oldcname);
	  if (cname) {
	    if (ncnames-- == 0) {
	      printf("Too many cnames.  Possible loop.\n");
	      exit(1);
	    }
	    oldcname = cname;
	    hp = NULL;
	    h_errno = TRY_AGAIN;
	    continue;
	  }
	  if (!waitmode)
	    break;
	}

	if (hp == NULL) {
	  hperror(h_errno);
	  exit(1);
	}

	exit(0);

}

static int
parsetype(s)
	char *s;
{
if (strcmp(s,"a") == 0)
  return(1);
if (strcmp(s,"ns") == 0)
  return(2);
if (strcmp(s,"md") == 0)
  return(3);
if (strcmp(s,"mf") == 0)
  return(4);
if (strcmp(s,"cname") == 0)
  return(5);
if (strcmp(s,"soa") == 0)
  return(6);
if (strcmp(s,"mb") == 0)
  return(7);
if (strcmp(s,"mg") == 0)
  return(8);
if (strcmp(s,"mr") == 0)
  return(9);
if (strcmp(s,"null") == 0)
  return(10);
if (strcmp(s,"wks") == 0)
  return(11);
if (strcmp(s,"ptr") == 0)
  return(12);
if (strcmp(s,"hinfo") == 0)
  return(13);
if (strcmp(s,"minfo") == 0)
  return(14);
if (strcmp(s,"mx") == 0)
  return(15);
if (strcmp(s,"txt") == 0)	/* Roy */
  return(T_TXT);		/* Roy */
if (strcmp(s,"uinfo") == 0)
  return(100);
if (strcmp(s,"uid") == 0)
  return(101);
if (strcmp(s,"gid") == 0)
  return(102);
if (strcmp(s,"unspec") == 0)
  return(103);
if (strcmp(s,"any") == 0)
  return(255);
if (strcmp(s,"*") == 0)
  return(255);
if (atoi(s))
  return(atoi(s));
fprintf(stderr, "Invalid query type: %s\n", s);
exit(2);
}

static int
parseclass(s)
	char *s;
{
if (strcmp(s,"in") == 0)
  return(C_IN);
if (strcmp(s,"chaos") == 0)
  return(C_CHAOS);
if (strcmp(s,"hs") == 0)
  return(C_HS);
if (strcmp(s,"any") == 0)
  return(C_ANY);
if (atoi(s))
  return(atoi(s));
fprintf(stderr, "Invalid query class: %s\n", s);
exit(2);
}

static void
printanswer(hp)
	register struct hostent *hp;
{
	register char **cp;
	struct in_addr **hptr;

	printf("Name: %s\n", hp->h_name);
	printf("Address:");
	for (hptr = (struct in_addr **)hp->h_addr_list; *hptr; hptr++)
	  printf(" %s", inet_ntoa(**hptr));
	printf("\nAliases:");
	for (cp = hp->h_aliases; cp && *cp && **cp; cp++)
		printf(" %s", *cp);
	printf("\n\n");
}

static void
hperror(err_no) 
int err_no;
{
switch(err_no) {
	case HOST_NOT_FOUND:
		fprintf(stderr,"Host not found.\n");
		break;
	case TRY_AGAIN:
		fprintf(stderr,"Host not found, try again.\n");
		break;
	case NO_RECOVERY:
		fprintf(stderr,"No recovery, Host not found.\n");
		break;
	case NO_ADDRESS:
		fprintf(stderr,"There is an entry for this host, but it doesn't have what you requested.\n");
		break;
	}
}

typedef union querybuf {
	HEADER qb1;
	u8_t qb2[PACKETSZ];
} querybuf_t;

static u8_t hostbuf[BUFSIZ+1];


static int
gethostinfo(name)
	char *name;
{
	register char *cp, **domain;
	int n;
	int hp;
	int nDomain;

	if (strcmp(name, ".") == 0)
		return(getdomaininfo(name, NULL));
	for (cp = name, n = 0; *cp; cp++)
		if (*cp == '.')
			n++;
	if (n && cp[-1] == '.') {
		if (cp[-1] == '.')
			cp[-1] = 0;
		hp = getdomaininfo(name, (char *)NULL);
		if (cp[-1] == 0)
			cp[-1] = '.';
		return (hp);
	}
	if (n == 0 && (cp = __hostalias(name))) {
	        if (verbose)
		    printf("Aliased to \"%s\"\n", cp);
		_res.options |= RES_DEFNAMES;	  
		return (getdomaininfo(cp, (char *)NULL));
	}
#ifdef MAXDS
	for (nDomain = 0;
	     _res.defdname_list[nDomain][0] != 0;
	     nDomain++) {
	    for (domain = _res.dnsrch_list[nDomain]; *domain; domain++) {
	        if (verbose)
		    printf("Trying domain \"%s\"\n", *domain);
		hp = getdomaininfo(name, *domain);
		if (hp)
			return (hp);
	    }
	}
#else
	for (domain = _res.dnsrch; *domain; domain++) {
	  if (verbose)
	    printf("Trying domain \"%s\"\n", *domain);
	  hp = getdomaininfo(name, *domain);
	  if (hp)
	    return (hp);
	}
#endif
	if (h_errno != HOST_NOT_FOUND ||
	   (_res.options & RES_DNSRCH) == 0)
		return (0);
	if (verbose)
	    printf("Trying null domain\n");
	return (getdomaininfo(name, (char *)NULL));
}

static int
getdomaininfo(name, domain)
	char *name, *domain;
{
  return getinfo(name, domain, gettype ? gettype : getdeftype);
}

static int
getinfo(name, domain, type)
	char *name, *domain;
{

	HEADER *hp;
	u8_t *eom, *bp, *cp;
	querybuf_t buf, answer;
	int n, n1, i, j, nmx, ancount, nscount, arcount, qdcount, buflen;
	char host[2*MAXDNAME+2];

	if (domain == NULL)
		(void)sprintf(host, "%.*s", MAXDNAME, name);
	else
		(void)sprintf(host, "%.*s.%.*s", MAXDNAME, name, MAXDNAME, domain);

	n = res_mkquery(QUERY, host, getclass, type, (char *)NULL, 0, NULL,
		(char *)&buf, sizeof(buf));
	if (n < 0) {
		if (_res.options & RES_DEBUG)
			printf("res_mkquery failed\n");
		h_errno = NO_RECOVERY;
		return(0);
	}
	n = res_send((char *)&buf, n, (char *)&answer, sizeof(answer));
	if (n < 0) {
		if (_res.options & RES_DEBUG)
			printf("res_send failed\n");
		h_errno = TRY_AGAIN;
		return (0);
	}
	eom = (u8_t *)&answer + n;
	return(printinfo(&answer, eom, T_ANY, 0));
}

static int
printinfo(answer, eom, filter, isls)
	querybuf_t *answer;
	u8_t *eom;
        int filter;
        int isls;
{
	HEADER *hp;
	u8_t *bp, *cp;
	int nmx, ancount, nscount, arcount, qdcount, buflen;

	/*
	 * find first satisfactory answer
	 */
	hp = (HEADER *) answer;
	ancount = ntohs(hp->ancount);
	qdcount = ntohs(hp->qdcount);
	nscount = ntohs(hp->nscount);
	arcount = ntohs(hp->arcount);
	if (_res.options & RES_DEBUG || (verbose && isls == 0))
		printf("rcode = %d (%s), ancount=%d\n", 
		       hp->rcode,
		       DecodeError(hp->rcode),
		       ancount);
	if (hp->rcode != NOERROR || (ancount+nscount+arcount) == 0) {
		switch (hp->rcode) {
			case NXDOMAIN:
				/* Check if it's an authoritive answer */
				if (hp->aa) {
					h_errno = HOST_NOT_FOUND;
					return(0);
				} else {
					h_errno = TRY_AGAIN;
					return(0);
				}
			case SERVFAIL:
				h_errno = TRY_AGAIN;
				return(0);
#ifdef OLDJEEVES
			/*
			 * Jeeves (TOPS-20 server) still does not
			 * support MX records.  For the time being,
			 * we must accept FORMERRs as the same as
			 * NOERROR.
			 */
			case FORMERR:
#endif /* OLDJEEVES */
			case NOERROR:
/* TpB - set a return error for this case. NO_DATA */
				h_errno = NO_DATA;
				return(0); /* was 1,but now indicates exception */
#ifndef OLDJEEVES
			case FORMERR:
#endif /* OLDJEEVES */
			case NOTIMP:
			case REFUSED:
				h_errno = NO_RECOVERY;
				return(0);
		}
		return (0);
	}
	bp = hostbuf;
	nmx = 0;
	buflen = sizeof(hostbuf);
	cp = (u8_t *)answer + sizeof(HEADER);
	if (qdcount) {
		cp += dn_skipname((u8_t *)cp,(u8_t *)eom) + QFIXEDSZ;
		while (--qdcount > 0)
			cp += dn_skipname((u8_t *)cp,(u8_t *)eom) + QFIXEDSZ;
	}
	if (ancount) {
	  if (!hp->aa)
	    if (verbose && isls == 0)
	      printf("The following answer is not authoritative:\n");
	  while (--ancount >= 0 && cp && cp < eom) {
	    cp = pr_rr(cp, (u8_t *)answer, stdout, filter);
/*
 * When we ask for address and there is a CNAME, it seems to return
 * both the CNAME and the address.  Since we trace down the CNAME
 * chain ourselves, we don't really want to print the address at
 * this point.
 */
	    if (cname && ! verbose)
	      return (1);
	  }
	}
	if (! verbose)
	  return (1);
	if (nscount) {
	  printf("For authoritative answers, see:\n");
	  while (--nscount >= 0 && cp && cp < eom) {
	    cp = pr_rr(cp, (u8_t *)answer, stdout, filter);
	  }
	}
	if (arcount) {
	  printf("Additional information:\n");
	  while (--arcount >= 0 && cp && cp < eom) {
	    cp = pr_rr(cp, (u8_t *)answer, stdout, filter);
	  }
	}
	return(1);
 }

static u8_t cnamebuf[MAXDNAME];

/*
 * Print resource record fields in human readable form.
 */
static u8_t *
pr_rr(cp, msg, file, filter)
	u8_t *cp, *msg;
	FILE *file;
        int filter;
{
	int type, class, dlen, n, c, proto, ttl;
	struct in_addr inaddr;
	u8_t *cp1;
	struct protoent *protop;
	struct servent *servp;
	char punc;
	int doprint;
	u8_t name[MAXDNAME];

	if ((cp = pr_cdname(cp, msg, name, sizeof(name))) == NULL)
		return (NULL);			/* compression error */

	type = _getshort(cp);
	cp += sizeof(u_short);

	class = _getshort(cp);
	cp += sizeof(u_short);

	ttl = _getlong(cp);
	cp += sizeof(u_long);

	if (filter == type || filter == T_ANY ||
	    (filter == T_A && (type == T_PTR || type == T_NS)))
	  doprint = 1;
	else
	  doprint = 0;

	if (doprint)
	  if (verbose)
	    fprintf(file,"%s\t%d%s\t%s",
		    name, ttl, pr_class(class), pr_type(type));
	  else {
	    fprintf(file,"%s%s %s",name, pr_class(class), pr_type(type));
	  }
	if (verbose)
	  punc = '\t';
	else
	  punc = ' ';

	dlen = _getshort(cp);
	cp += sizeof(u_short);
	cp1 = cp;
	/*
	 * Print type specific data, if appropriate
	 */
	switch (type) {
	case T_A:
		switch (class) {
		case C_IN:
			bcopy((char *)cp, (char *)&inaddr, sizeof(inaddr));
			if (dlen == 4) {
			        if (doprint)
				  fprintf(file,"%c%s", punc,
					inet_ntoa(inaddr));
				cp += dlen;
			} else if (dlen == 7) {
			        if (doprint) {
				  fprintf(file,"%c%s", punc,
					  inet_ntoa(inaddr));
				  fprintf(file,", protocol = %d", cp[4]);
				  fprintf(file,", port = %d",
					  (cp[5] << 8) + cp[6]);
				}
				cp += dlen;
			}
			break;
		}
		break;
	case T_CNAME:
		if (dn_expand(msg, msg + 512, cp, cnamebuf, 
			      sizeof(cnamebuf)-1) >= 0) {
			strcat((char *) cnamebuf, ".");
			if (gettype != T_CNAME && gettype != T_ANY)
				cname = cnamebuf;				
		}
	case T_MB:
#ifdef OLDRR
	case T_MD:
	case T_MF:
#endif /* OLDRR */
	case T_MG:
	case T_MR:
	case T_NS:
	case T_PTR:
		cp = pr_cdname(cp, msg, name, sizeof(name));
		if (doprint)
		  fprintf(file,"%c%s",punc, name);
		break;

	case T_HINFO:
		if ((n = *cp++)) {
			if (doprint)
			  fprintf(file,"%c%.*s", punc, n, cp);
			cp += n;
		}
		if ((n = *cp++)) {
			if (doprint)
			  fprintf(file,"%c%.*s", punc, n, cp);
			cp += n;
		}
		break;

	case T_SOA:
		cp = pr_cdname(cp, msg, name, sizeof(name));
		if (doprint)
		  fprintf(file,"\t%s", name);
		cp = pr_cdname(cp, msg, name, sizeof(name));
		if (doprint)
		  fprintf(file," %s", name);
		if (doprint)
		  fprintf(file,"(\n\t\t\t%d\t;serial (version)", _getlong(cp));
		cp += sizeof(u_long);
		if (doprint)
		  fprintf(file,"\n\t\t\t%d\t;refresh period", _getlong(cp));
		cp += sizeof(u_long);
		if (doprint)
		  fprintf(file,"\n\t\t\t%d\t;retry refresh this often", _getlong(cp));
		cp += sizeof(u_long);
		if (doprint)
		  fprintf(file,"\n\t\t\t%d\t;expiration period", _getlong(cp));
		cp += sizeof(u_long);
		if (doprint)
		  fprintf(file,"\n\t\t\t%d\t;minimum TTL\n\t\t\t)", _getlong(cp));
		cp += sizeof(u_long);
		break;

	case T_MX:
		if (doprint) {
		  if (verbose)
		    fprintf(file,"\t%d ",_getshort(cp));
		  else
		    fprintf(file," (pri=%d) by ",_getshort(cp));
		}
		cp += sizeof(u_short);
		cp = pr_cdname(cp, msg, name, sizeof(name));
		if (doprint)
		  fprintf(file, "%s", name);
		break;

	case T_MINFO:
		cp = pr_cdname(cp, msg, name, sizeof(name));
		if (doprint)
		  fprintf(file,"%c%s",punc, name);
		cp = pr_cdname(cp, msg, name, sizeof(name));
		if (doprint)
		  fprintf(file," %s", name);
		break;

		/* Roy start */
	case T_TXT:
		if ((n = *cp++)) {
			if (doprint)
			  fprintf(file,"%c%.*s", punc, n, cp);
			cp += n;
		}
		break;
		/* Roy end */
	case T_WKS:
		if (dlen < sizeof(u_long) + 1)
			break;
		bcopy((char *)cp, (char *)&inaddr, sizeof(inaddr));
		cp += sizeof(u_long);
		proto = *cp++;
		protop = getprotobynumber(proto);
		if (doprint)
		  if (protop)
		    fprintf(file,"%c%s %s", punc,
			    inet_ntoa(inaddr), protop->p_name);
		  else
		    fprintf(file,"%c%s %d", punc,
			    inet_ntoa(inaddr), proto);

		n = 0;
		while (cp < cp1 + dlen) {
			c = *cp++;
			do {
 				if (c & 0200) {
				  servp = NULL;
				  if (protop)
				    servp = getservbyport (htons(n),
							   protop->p_name);
				  if (doprint)
				    if (servp)
				      fprintf(file, " %s", servp->s_name);
				    else
				      fprintf(file, " %d", n);
				}
 				c <<= 1;
			} while (++n & 07);
		}
		break;

	default:
		if (doprint)
		  fprintf(file,"%c???", punc);
		cp += dlen;
	}
	if (cp != cp1 + dlen)
		fprintf(file,"packet size error (%#x != %#x)\n", cp, cp1+dlen);
	if (doprint)
	  fprintf(file,"\n");
	return (cp);
}

static	char nbuf[20];

/*
 * Return a string for the type
 */
static char *
pr_type(type)
	int type;
{
	switch (type) {
	case T_A:
		return(verbose? "A" : "has address");
	case T_NS:		/* authoritative server */
		return("NS");
#ifdef OLDRR
	case T_MD:		/* mail destination */
		return("MD");
	case T_MF:		/* mail forwarder */
		return("MF");
#endif /* OLDRR */
	case T_CNAME:		/* connonical name */
		return(verbose? "CNAME" : "is a nickname for");
	case T_SOA:		/* start of authority zone */
		return("SOA");
	case T_MB:		/* mailbox domain name */
		return("MB");
	case T_MG:		/* mail group member */
		return("MG");
	case T_MX:		/* mail routing info */
		return(verbose? "MX" : "mail is handled");
	/* Roy start */
	case T_TXT:		/* TXT - descriptive info */
		return(verbose? "TXT" : "descriptive text");
	/* Roy end */
	case T_MR:		/* mail rename name */
		return("MR");
	case T_NULL:		/* null resource record */
		return("NULL");
	case T_WKS:		/* well known service */
		return("WKS");
	case T_PTR:		/* domain name pointer */
		return("PTR");
	case T_HINFO:		/* host information */
		return("HINFO");
	case T_MINFO:		/* mailbox information */
		return("MINFO");
	case T_AXFR:		/* zone transfer */
		return("AXFR");
	case T_MAILB:		/* mail box */
		return("MAILB");
	case T_MAILA:		/* mail address */
		return("MAILA");
	case T_ANY:		/* matches any type */
		return("ANY");
	default:
		return (sprintf(nbuf, "%d", type) == EOF ? NULL : nbuf);
	}
}

/*
 * Return a mnemonic for class
 */
static char *
pr_class(class)
	int class;
{

	switch (class) {
	case C_IN:		/* internet class */
		return(verbose? " IN" : "");
	case C_CHAOS:		/* chaos class */
		return(verbose? " CHAOS" : "");
	case C_HS:		/* Hesiod class */
		return(verbose? " HS" : "");
	case C_ANY:		/* matches any class */
		return(" ANY");
	default:
		return (sprintf(nbuf," %d", class) == EOF ? NULL : nbuf);
	}
}

static u8_t *
pr_cdname(cp, msg, name, namelen)
	u8_t *cp, *msg;
        u8_t *name;
        int namelen;
{
	int n;

	if ((n = dn_expand(msg, msg + 512, cp, name, namelen - 2)) < 0)
		return (NULL);
	if (name[0] == '\0') {
		name[0] = '.';
		name[1] = '\0';
	}
	return (cp + n);
}

char *resultcodes[] = {
	"NOERROR",
	"FORMERR",
	"SERVFAIL",
	"NXDOMAIN",
	"NOTIMP",
	"REFUSED",
	"6",
	"7",
	"8",
	"9",
	"10",
	"11",
	"12",
	"13",
	"14",
	"NOCHANGE",
};



/*
 ******************************************************************************
 *
 *  ListHosts --
 *
 *	Requests the name server to do a zone transfer so we
 *	find out what hosts it knows about.
 *
 *  Results:
 *	SUCCESS		the listing was successful.
 *	ERROR		the server could not be contacted because 
 *			a socket could not be obtained or an error
 *			occured while receiving, or the output file
 *			could not be opened.
 *
 ******************************************************************************
 */

static int
ListHosts(namePtr, queryType)
    char *namePtr;
    int  queryType;  /* e.g. T_A */
{
	querybuf_t 		buf, answer;
	HEADER			*headerPtr;

	int 			msglen;
	int 			amtToRead;
	int 			numRead;
	int 			i;
	int 			numAnswers = 0;
	int 			result;
	int 			soacnt = 0;
	u_short 		len;
	int			dlen;
	int			type;
	int			nscount;
	u8_t 			*cp, *nmp;
	u8_t 			name[NAME_LEN];
	char 			dname[2][NAME_LEN];
	u8_t 			domain[NAME_LEN];
/* names and addresses of name servers to try */
#define NUMNS 8
	char			nsname[NUMNS][NAME_LEN];
	int			nshaveaddr[NUMNS];
#define IPADDRSIZE 4
#define NUMNSADDR 16
	char	 		nsipaddr[NUMNSADDR][IPADDRSIZE];
	int			numns;
	int			numnsaddr;
	int			thisns;
	struct hostent		*hp;
	enum {
	    NO_ERRORS, 
	    ERR_READING_LEN, 
	    ERR_READING_MSG,
	    ERR_PRINTING
	} error = NO_ERRORS;
	char *tcp_serv_name;
	int tcp_fd;
	nwio_tcpconf_t tcpconf;
	nwio_tcpcl_t clopt;
	int terrno;

/*
 * normalize to not have trailing dot.  We do string compares below
 * of info from name server, and it won't have trailing dots.
 */
	i = strlen(namePtr);
	if (namePtr[i-1] == '.')
	  namePtr[i-1] = 0;

	if (server_specified) {
	  bcopy((char *)&_res.nsaddr, nsipaddr[0], IPADDRSIZE);
	  numnsaddr = 1;
	}
	else {

/*
 * First we have to find out where to look.  This needs a NS query,
 * possibly followed by looking up addresses for some of the names.
 */

	msglen = res_mkquery(QUERY, namePtr, C_IN, T_NS,
				(char *)0, 0, (struct rrec *)0, 
				(char *)&buf, sizeof(buf));

	if (msglen < 0) {
		printf("res_mkquery failed\n");
		return (ERROR);
	}

	msglen = res_send((char *)&buf,msglen,(char *)&answer, sizeof(answer));
	
	if (msglen < 0) {
		printf("Unable to get to nameserver -- try again later\n");
		return (ERROR);
	}
	if (_res.options & RES_DEBUG || verbose)
		printf("rcode = %d (%s), ancount=%d\n", 
		       answer.qb1.rcode,
		       DecodeError(answer.qb1.rcode),
		       ntohs(answer.qb1.ancount));

/*
 * Analyze response to our NS lookup
 */

	nscount = ntohs(answer.qb1.ancount) +
		  ntohs(answer.qb1.nscount) +
		  ntohs(answer.qb1.arcount);


	if (answer.qb1.rcode != NOERROR || nscount == 0) {
	    switch (answer.qb1.rcode) {
			case NXDOMAIN:
				/* Check if it's an authoritive answer */
				if (answer.qb1.aa) {
					printf("No such domain\n");
				} else {
					printf("Unable to get information about domain -- try again later.\n");
				}
				break;
			case SERVFAIL:
				printf("Unable to get information about that domain -- try again later.\n");
				break;
			case NOERROR:
				printf("That domain exists, but seems to be a leaf node.\n");
				break;
			case FORMERR:
			case NOTIMP:
			case REFUSED:
				printf("Unrecoverable error looking up domain name.\n");
				break;
		}
		return (0);
	}

	cp = answer.qb2 + sizeof(HEADER);
	if (ntohs(answer.qb1.qdcount) > 0)
	  cp += dn_skipname(cp, answer.qb2 + msglen) + QFIXEDSZ;

	numns = 0;
	numnsaddr = 0;

/*
 * Look at response from NS lookup for NS and A records.
 */

	for (;nscount; nscount--) {
	  cp += dn_expand(answer.qb2, answer.qb2 + msglen, cp,
			  domain, sizeof(domain));
	  type = _getshort(cp);
	  cp += sizeof(u_short) + sizeof(u_short) + sizeof(u_long);
	  dlen = _getshort(cp);
	  cp += sizeof(u_short);
	  if (type == T_NS) {
	    if (dn_expand(answer.qb2, answer.qb2 + msglen, cp, 
			  name, sizeof(name)) >= 0) {
	      if (numns < NUMNS && strcasecmp((char *)domain, namePtr) == 0) {
		for (i = 0; i < numns; i++)
		  if (strcasecmp(nsname[i], (char *)name) == 0)
		    break;  /* duplicate */
		if (i >= numns) {
		  strncpy(nsname[numns], (char *)name, sizeof(name));
		  nshaveaddr[numns] = 0;
		  numns++;
		}
	      }
	    }
	  }
	  else if (type == T_A) {
	    if (numnsaddr < NUMNSADDR)
	      for (i = 0; i < numns; i++) {
		if (strcasecmp(nsname[i], (char *)domain) == 0) {
		  nshaveaddr[i]++;
		  bcopy((char *)cp, nsipaddr[numnsaddr],IPADDRSIZE);
		  numnsaddr++;
		  break;
		}
	      }
	  }
	  cp += dlen;
	}

/*
 * Usually we'll get addresses for all the servers in the additional
 * info section.  But in case we don't, look up their addresses.
 */

	for (i = 0; i < numns; i++) {
	  if (! nshaveaddr[i]) {
	    register long **hptr;
	    int numaddrs = 0;

	    hp = gethostbyname(nsname[i]);
	    if (hp) {
	      for (hptr = (long **)hp->h_addr_list; *hptr; hptr++)
		if (numnsaddr < NUMNSADDR) {
		  bcopy((char *)*hptr, nsipaddr[numnsaddr],IPADDRSIZE);
		  numnsaddr++;
		  numaddrs++;
		}
	    }
	    if (_res.options & RES_DEBUG || verbose)
	      printf("Found %d addresses for %s by extra query\n",
		     numaddrs, nsname[i]);
	  }
	  else
	    if (_res.options & RES_DEBUG || verbose)
	      printf("Found %d addresses for %s\n",
		     nshaveaddr[i], nsname[i]);
	}
        }
/*
 * Now nsipaddr has numnsaddr addresses for name servers that
 * serve the requested domain.  Now try to find one that will
 * accept a zone transfer.
 */

	thisns = 0;

again:

	numAnswers = 0;
	soacnt = 0;

	/*
	 *  Create a query packet for the requested domain name.
	 *
	 */
	msglen = res_mkquery(QUERY, namePtr, getclass, T_AXFR,
				(char *)0, 0, (struct rrec *)0, 
				(char *) &buf, sizeof(buf));
	if (msglen < 0) {
	    if (_res.options & RES_DEBUG) {
		fprintf(stderr, "ListHosts: Res_mkquery failed\n");
	    }
	    return (ERROR);
	}

	/*
	 *  Set up a virtual circuit to the server.
	 */

	tcp_serv_name= getenv("TCP_DEVICE");
	if (!tcp_serv_name)
		tcp_serv_name= TCP_DEVICE;
	for (;thisns < numnsaddr; thisns++) 
	{
		tcp_fd= open(tcp_serv_name, O_RDWR);
		if (tcp_fd == -1)
		{
			fprintf(stderr, "unable to open '%s': %s\n", tcp_serv_name,
				strerror(errno));
			return ERROR;
		}

		tcpconf.nwtc_flags= NWTC_EXCL | NWTC_LP_SEL | NWTC_SET_RA | 
								NWTC_SET_RP;
		tcpconf.nwtc_remaddr= *(ipaddr_t *)nsipaddr[thisns];
		tcpconf.nwtc_remport= _res.nsaddr.sin_port;
		result= ioctl(tcp_fd, NWIOSTCPCONF, &tcpconf);
		if (result == -1)
		{
			fprintf(stderr, "tcp_ioc_setconf failed: %s\n", 
				strerror(errno));
			close(tcp_fd);
			return ERROR;
		}
		if (_res.options & RES_DEBUG || verbose)
			printf("Trying %s\n", inet_ntoa(tcpconf.nwtc_remaddr));
		clopt.nwtcl_flags= 0;
		result= ioctl(tcp_fd, NWIOTCPCONN, &clopt);
		if (result == 0)
			break;
		terrno= errno;
		if (verbose)
			fprintf(stderr, 
				"Connection failed, trying next server: %s\n",
				strerror(errno));
		close(tcp_fd);
	}	
	if (thisns >= numnsaddr) {
	  printf("No server for that domain responded\n");
	  if (!verbose)
	    fprintf(stderr, "Error from the last server was: %s\n", 
						strerror(terrno));
	  return(ERROR);
	}

	/*
	 * Send length & message for zone transfer 
	 */

        len = htons(msglen);

	result= tcpip_writeall(tcp_fd, (char *)&len, sizeof(len));
	if (result != sizeof(len))
	{
		fprintf(stderr, "write failed: %s\n", strerror(errno));
		close(tcp_fd);
		return ERROR;
	}
	result= tcpip_writeall(tcp_fd, (char *)&buf, msglen);
	if (result != msglen)
	{
		fprintf(stderr, "write failed: %s\n",
			strerror(errno));
		close(tcp_fd);
		return ERROR;
	}
	filePtr = stdout;

	while (1) {

	    /*
	     * Read the length of the response.
	     */

	    cp = (u8_t *) &buf;
	    amtToRead = sizeof(u_short);
	    while(amtToRead > 0)
	    {
		result = read(tcp_fd, (char *)cp, amtToRead);
		if (result <= 0)
			break;
		cp 	  += result;
		amtToRead -= result;
	    }
	    if (amtToRead) {
		error = ERR_READING_LEN;
		break;
	    }	

	    if ((len = htons(*(u_short *)&buf)) == 0) {
		break;	/* nothing left to read */
	    }

	    /*
	     * Read the response.
	     */

	    amtToRead = len;
	    cp = (u8_t *) &buf;
	    while(amtToRead > 0)
	    {
		result = read(tcp_fd, (char *)cp, amtToRead);
		if (result<= 0)
			break;
		cp 	  += result;
		amtToRead -= result;
	    }
	    if (amtToRead) {
		error = ERR_READING_MSG;
		break;
	    }

	    i = buf.qb1.rcode;

	    if (i != NOERROR || ntohs(buf.qb1.ancount) == 0) {
	      if ((thisns+1) < numnsaddr &&
		  (i == SERVFAIL || i == NOTIMP || i == REFUSED)) {
		if (_res.options & RES_DEBUG || verbose)
		  printf("Server failed, trying next server: %s\n",
			 i != NOERROR ? 
			 DecodeError(i) : "Premature end of data");
		close(tcp_fd);
		thisns++;
		goto again;
	      }
	      printf("Server failed: %s\n",
		     i != NOERROR ? DecodeError(i) : "Premature end of data");
	      break;
	    }


	    result = printinfo(&buf, cp, queryType, 1);
	    if (! result) {
		error = ERR_PRINTING;
		break;
	    }
	    numAnswers++;
	    cp = buf.qb2 + sizeof(HEADER);
	    if (ntohs(buf.qb1.qdcount) > 0)
		cp += dn_skipname(cp, buf.qb2 + len) + QFIXEDSZ;

	    nmp = cp;
	    cp += dn_skipname(cp, (u_char *)&buf + len);
	    if ((_getshort(cp) == T_SOA)) {
		dn_expand(buf.qb2, buf.qb2 + len, nmp, (u8_t *)dname[soacnt],
			sizeof(dname[0]));
	        if (soacnt) {
		    if (strcmp(dname[0], dname[1]) == 0)
			break;
		} else
		    soacnt++;
	    }
        }

	close(tcp_fd);

	switch (error) {
	    case NO_ERRORS:
		return (SUCCESS);

	    case ERR_READING_LEN:
		return(ERROR);

	    case ERR_PRINTING:
		fprintf(stderr,"*** Error during listing of %s: %s\n", 
				namePtr, DecodeError(result));
		return(result);

	    case ERR_READING_MSG:
		headerPtr = (HEADER *) &buf;
		fprintf(stderr,"ListHosts: error receiving zone transfer:\n");
		fprintf(stderr,
	       "  result: %s, answers = %d, authority = %d, additional = %d\n", 
			resultcodes[headerPtr->rcode],
			ntohs(headerPtr->ancount),
			ntohs(headerPtr->nscount),
			ntohs(headerPtr->arcount));
		return(ERROR);
	    default:
		return(ERROR);
	}
}

static char *
DecodeError(result)
    int result;
{
	switch(result) {
	    case NOERROR: 	return("Success"); break;
	    case FORMERR:	return("Format error"); break;
	    case SERVFAIL:	return("Server failed"); break;
	    case NXDOMAIN:	return("Non-existent domain"); break;
	    case NOTIMP:	return("Not implemented"); break;
	    case REFUSED:	return("Query refused"); break;
	    case NOCHANGE:	return("No change"); break;
	    case NO_INFO: 	return("No information"); break;
	    case ERROR: 	return("Unspecified error"); break;
	    case TIME_OUT: 	return("Timed out"); break;
	    case NONAUTH: 	return("Non-authoritative answer"); break;
	    default: 		break;
	}
	return("BAD ERROR VALUE"); 
}

static int tcpip_writeall(fd, buf, siz)
int fd;
char *buf;
unsigned siz;
{
	unsigned siz_org;
	int nbytes;

	siz_org= siz;

	while (siz)
	{
		nbytes= write(fd, buf, siz);
		if (nbytes == -1)
			return nbytes;
		assert(siz >= nbytes);
		buf += nbytes;
		siz -= nbytes;
	}
	return siz_org;
}
