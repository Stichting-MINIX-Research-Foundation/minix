/*	tags.c - Obtain DHCP tags from the config file
 *							Author: Kees J. Bot
 *								16 Dec 2000
 */
#include <sys/types.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include <configfile.h>
#include <sys/ioctl.h>
#include <sys/asynchio.h>
#include <net/hton.h>
#include <net/gen/socket.h>
#include <netdb.h>
#include <net/gen/in.h>
#include <net/gen/inet.h>
#include <net/gen/ether.h>
#include <net/gen/if_ether.h>
#include <net/gen/eth_hdr.h>
#include <net/gen/ip_hdr.h>
#include <net/gen/udp.h>
#include <net/gen/udp_hdr.h>
#include <net/gen/dhcp.h>
#include "dhcpd.h"

#define doff(field)		offsetof(dhcp_t, field)

void settag(dhcp_t *dp, int tag, void *data, size_t len)
{
    if (!dhcp_settag(dp, tag, data, len)) {
	/* Oops, it didn't fit?  Is this really Minix??? */
	fprintf(stderr,
	    "%s: DHCP packet too big, please trim the configuration\n",
	    program);
	exit(1);
    }
}

static int name2ip(ipaddr_t *pip, const char *name, ipaddr_t ifip)
{
    /* Translate a name to an IP address, preferably from the hosts file,
     * but also from the DNS if being a server.  Prefer the address closest
     * to the interface with IP address 'ifip' if there are choices..
     */
    extern struct hostent *_gethostent(void);	/* File reading versions. */
    extern void _endhostent(void);
    struct hostent *he;
    size_t len= strlen(name);
    u32_t d, distance= -1;
    ipaddr_t ip;
    int i;
    char *hn;

    /* Already an IP address? */
    if (inet_aton(name, pip)) return 1;

    /* In the hosts file? */
    while ((he= _gethostent()) != nil) {
	hn= he->h_name;
	i= -1;
	do {
	    if (strncasecmp(name, hn, len) == 0
		&& (hn[len] == 0 || hn[len] == '.')
	    ) {
		memcpy(&ip, he->h_addr, sizeof(ip));
		d= ntohl(ip) ^ ntohl(ifip);
		if (d < distance) {
		    *pip= ip;
		    distance= d;
		}
		break;
	    }
	} while ((hn= he->h_aliases[++i]) != nil);
    }
    _endhostent();
    if (distance < -1) return 1;

    /* Nothing?  Try the real DNS if being a server. */
    if (serving) {
	if ((he= gethostbyname(name)) != nil && he->h_addrtype == AF_INET) {
	    /* Select the address closest to 'ifip'. */
	    for (i= 0; he->h_addr_list[i] != nil; i++) {
		memcpy(&ip, he->h_addr_list[i], sizeof(ip));
		d= ntohl(ip) ^ ntohl(ifip);
		if (d < distance) {
		    *pip= ip;
		    distance= d;
		}
	    }
	    return 1;
	}
    }
    return 0;
}

static char *ip2name(ipaddr_t ip)
{
    /* Translate an IP address to a name, etc, etc. */
    extern struct hostent *_gethostent(void);	/* File reading versions. */
    extern void _endhostent(void);
    struct hostent *he;

    /* In the hosts file? */
    while ((he= _gethostent()) != nil) {
	if (memcmp(he->h_addr, &ip, sizeof(ip)) == 0) break;
    }
    _endhostent();

    /* Nothing?  Try the real DNS if being a server. */
    if (he == nil && serving) {
	he= gethostbyaddr((char *) &ip, sizeof(ip), AF_INET);
    }
    return he != nil ? he->h_name : nil;
}

static int cidr_aton(const char *cidr, ipaddr_t *addr, ipaddr_t *mask)
{
    char *slash, *check;
    ipaddr_t a;
    int ok;
    unsigned long len;

    if ((slash= strchr(cidr, '/')) == nil) return 0;

    *slash++= 0;
    ok= inet_aton(cidr, &a);

    len= strtoul(slash, &check, 10);
    if (check == slash || *check != 0 || len > 32) ok= 0;

    *--slash= '/';
    if (!ok) return 0;
    *addr= a;
    *mask= htonl(len == 0 ? 0 : (0xFFFFFFFFUL << (32-len)) & 0xFFFFFFFFUL);
    return 1;
}

char *cidr_ntoa(ipaddr_t addr, ipaddr_t mask)
{
    ipaddr_t testmask= 0xFFFFFFFFUL;
    int n;
    static char result[sizeof("255.255.255.255/255.255.255.255")];

    for (n= 32; n >= 0; n--) {
	if (mask == htonl(testmask)) break;
	testmask= (testmask << 1) & 0xFFFFFFFFUL;
    }

    sprintf(result, "%s/%-2d", inet_ntoa(addr), n);
    if (n == -1) strcpy(strchr(result, '/')+1, inet_ntoa(mask));
    return result;
}

static size_t ascii2octet(u8_t *b, size_t size, const char *a)
{
    /* Convert a series of hex digit pairs to an octet (binary) array at
     * 'b' with length 'size'.  Return the number of octets in 'a' or
     * -1 on error.
     */
    size_t len;
    int n, c;

    len= 0;
    n= 0;
    while ((c= *a++) != 0) {
	if (between('0', c, '9')) c= (c - '0') + 0x0;
	else
	if (between('a', c, 'f')) c= (c - 'a') + 0xa;
	else
	if (between('A', c, 'F')) c= (c - 'A') + 0xA;
	else {
	    return -1;
	}

	if (n == 0) {
	    if (len < size) b[len] = c << 4;
	} else {
	    if (len < size) b[len] |= c;
	    len++;
	}
	n ^= 1;
    }
    return n == 0 ? len : -1;
}

void ether2clid(u8_t *clid, ether_addr_t *eth)
{
    /* Convert an Ethernet address to the default client ID form. */
    clid[0]= DHCP_HTYPE_ETH;
    memcpy(clid+1, eth, DHCP_HLEN_ETH);
}

static size_t ascii2clid(u8_t *clid, const char *a)
{
    /* Convert an ethernet address, or a series of hex digits to a client ID.
     * Return its length if ok, otherwise -1.
     */
    size_t len;
    ether_addr_t *eth;

    if ((eth= ether_aton(a)) != nil) {
	ether2clid(clid, eth);
	len= 1+DHCP_HLEN_ETH;
    } else {
	len= ascii2octet(clid, CLID_MAX, a);
    }
    return len;
}

static config_t *dhcpconf;		/* In-core DHCP configuration. */

/* DHCP tag types. */
typedef enum { TT_ASCII, TT_BOOLEAN, TT_IP, TT_NUMBER, TT_OCTET } tagtype_t;

/* DHCP/BOOTP tag definitions. */
typedef struct tagdef {
	u8_t		tag;		/* Tag number. */
	u8_t		type;		/* Type and flags. */
	u8_t		gran;		/* Granularity. */
	u8_t		max;		/* Maximum number of arguments. */
	const char	*name;		/* Defined name. */
} tagdef_t;

#define TF_TYPE		0x07		/* To mask out the type. */
#define TF_STATIC	0x08		/* "Static", i.e. a struct field. */
#define TF_RO		0x10		/* Read-only, user can't set. */

/* List of static DHCP fields.  The tag field is misused here as an offset
 * into the DHCP structure.
 */
static tagdef_t statictag[] = {
    { doff(op),     TT_NUMBER|TF_STATIC|TF_RO,	1,   1,	"op"		},
    { doff(htype),  TT_NUMBER|TF_STATIC|TF_RO,	1,   1,	"htype"		},
    { doff(hlen),   TT_NUMBER|TF_STATIC|TF_RO,	1,   1,	"hlen"		},
    { doff(hops),   TT_NUMBER|TF_STATIC|TF_RO,	1,   1,	"hops"		},
    { doff(xid),    TT_NUMBER|TF_STATIC|TF_RO,	4,   1,	"xid"		},
    { doff(secs),   TT_NUMBER|TF_STATIC|TF_RO,	2,   1,	"secs"		},
    { doff(flags),  TT_NUMBER|TF_STATIC|TF_RO,	2,   1,	"flags"		},
    { doff(ciaddr), TT_IP|TF_STATIC|TF_RO,	1,   1,	"ciaddr"	},
    { doff(yiaddr), TT_IP|TF_STATIC|TF_RO,	1,   1,	"yiaddr"	},
    { doff(siaddr), TT_IP|TF_STATIC,		1,   1,	"siaddr"	},
    { doff(giaddr), TT_IP|TF_STATIC|TF_RO,	1,   1,	"giaddr"	},
    { doff(chaddr), TT_OCTET|TF_STATIC|TF_RO,	1,  16,	"chaddr"	},
    { doff(sname),  TT_ASCII|TF_STATIC,		1,  64,	"sname"		},
    { doff(file),   TT_ASCII|TF_STATIC,		1, 128,	"file"		},
};
#define N_STATIC	arraysize(statictag)

static tagdef_t alltagdef[N_STATIC + 254];	/* List of tag definitions. */
#define tagdef	(alltagdef+N_STATIC-1)		/* Just the optional ones. */

#define tagdefined(tp)		((tp)->name != nil)

static void inittagdef(void)
{
    /* Initialize the tag definitions from the "tag" commands in the config
     * file.
     */
    int t;
    tagdef_t *tp;
    static tagdef_t predef[] = {
	{ DHCP_TAG_NETMASK,	TT_IP,	   1,	  1,	"netmask"	},
	{ DHCP_TAG_GATEWAY,	TT_IP,	   1,	255,	"gateway"	},
	{ DHCP_TAG_DNS,		TT_IP,	   1,	255,	"DNSserver"	},
    };
    static char *typenames[] = { "ascii", "boolean", "ip", "number", "octet" };
    config_t *cfg;
    static u8_t rotags[] = {
	DHCP_TAG_REQIP, DHCP_TAG_OVERLOAD, DHCP_TAG_TYPE, DHCP_TAG_SERVERID,
	DHCP_TAG_REQPAR, DHCP_TAG_MESSAGE, DHCP_TAG_MAXDHCP
    };

    for (t= 1; t <= 254; t++) {
	tp= &tagdef[t];
	tp->tag= t;
	tp->type= TT_OCTET;
	tp->name= nil;
    }

    /* Set the static and "all Minix needs" tags. */
    memcpy(alltagdef, statictag, sizeof(statictag));
    for (tp= predef; tp < arraylimit(predef); tp++) tagdef[tp->tag] = *tp;

    /* Search for tag definitions in the config file. */
    for (cfg= dhcpconf; cfg != nil; cfg= cfg->next) {
	config_t *cmd= cfg->list;

	if (strcasecmp(cmd->word, "tag") == 0) {
	    if (config_length(cmd) == 6
		&& (cmd->next->flags & CFG_DULONG)
		&& config_isatom(cmd->next->next)
		&& config_isatom(cmd->next->next->next)
		&& (cmd->next->next->next->next->flags & CFG_DULONG)
		&& (cmd->next->next->next->next->next->flags & CFG_DULONG)
	    ) {
		unsigned long tag, gran, max;
		const char *name, *typename;
		unsigned type;

		tag= strtoul(cmd->next->word, nil, 10);
		name= cmd->next->next->word;
		typename= cmd->next->next->next->word;
		gran= strtoul(cmd->next->next->next->next->word, nil, 10);
		max= strtoul(cmd->next->next->next->next->next->word, nil, 10);

		for (type= 0; type < arraysize(typenames); type++) {
		    if (strcasecmp(typename, typenames[type]) == 0) break;
		}

		if (!(1 <= tag && tag <= 254)
		    || !(type < arraysize(typenames))
		    || !((type == TT_NUMBER
			    && (gran == 1 || gran == 2 || gran == 4))
			|| (type != TT_NUMBER && 1 <= gran && gran <= 16))
		    || !(max <= 255)
		) {
		    fprintf(stderr,
			"\"%s\", line %u: Tag definition is incorrect\n",
			cmd->file, cmd->line);
		    exit(1);
		}

		tp= &tagdef[(int)tag];
		tp->type= type;
		tp->name= name;
		tp->gran= gran;
		tp->max= max;
	    } else {
		fprintf(stderr,
	    "\"%s\", line %u: Usage: tag number name type granularity max\n",
		    cmd->file, cmd->line);
		exit(1);
	    }
	}
    }

    /* Many DHCP tags are not for the user to play with. */
    for (t= 0; t < arraysize(rotags); t++) tagdef[rotags[t]].type |= TF_RO;
}

static tagdef_t *tagdefbyname(const char *name)
{
    /* Find a tag definition by the name of the tag.  Return null if not
     * defined.
     */
    tagdef_t *tp;

    for (tp= alltagdef; tp < arraylimit(alltagdef); tp++) {
	if (tagdefined(tp) && strcasecmp(tp->name, name) == 0) return tp;
    }
    return nil;
}

void initdhcpconf(void)
{
    /* Read/refresh configuration from the DHCP configuration file. */
    dhcpconf= config_read(configfile, 0, dhcpconf);
    if (config_renewed(dhcpconf)) inittagdef();
}

static void configtag(dhcp_t *dp, config_t *cmd, ipaddr_t ifip)
{
    /* Add a tag to a DHCP packet from the config file. */
    tagdef_t *tp;
    u8_t data[260], *d;
    size_t i;
    int delete= 0;

    if (strcasecmp(cmd->word, "no") == 0) {
	if (config_length(cmd) != 2 || !config_isatom(cmd->next)) {
	    fprintf(stderr, "\"%s\", line %u: Usage: no tag-name\n",
		cmd->file, cmd->line);
	    exit(1);
	}
	cmd= cmd->next;
	delete= 1;
    }

    if ((tp= tagdefbyname(cmd->word)) == nil) {
	fprintf(stderr, "\"%s\", line %u: Unknown tag '%s'\n",
	    cmd->file, cmd->line, cmd->word);
	exit(1);
    }

    if (tp->type & TF_RO) {
	fprintf(stderr, "\"%s\", line %u: Tag '%s' can't be configured\n",
	    cmd->file, cmd->line, cmd->word);
	exit(1);
    }

    i= 0;
    d= data;
    if (!delete) {
	config_t *arg= cmd->next;
	do {
	    switch (tp->type & TF_TYPE) {
	    case TT_ASCII: {
		if (arg == nil || !config_isatom(arg) || arg->next != nil) {
		    fprintf(stderr, "\"%s\", line %u: Usage: %s string\n",
			cmd->file, cmd->line, cmd->word);
		    exit(1);
		}
		strncpy((char *) data, arg->word, sizeof(data));
		d += i = strnlen((char *) data, sizeof(data));
		break;}
	    case TT_BOOLEAN: {
		if (arg == nil || !config_isatom(arg)
		    || !(strcasecmp(arg->word, "false") == 0
			    || strcasecmp(arg->word, "true") == 0)
		) {
		    fprintf(stderr,
			"\"%s\", line %u: Usage: %s false|true ...\n",
			cmd->file, cmd->line, cmd->word);
		    exit(1);
		}
		if (d < arraylimit(data)) {
		    *d++ = (arg->word[0] != 'f' && arg->word[0] != 'F');
		}
		i++;
		break;}
	    case TT_IP: {
		ipaddr_t ip;
		unsigned long len;
		char *end;

		if (arg == nil || !config_isatom(arg)) {
		    fprintf(stderr, "\"%s\", line %u: Usage: %s host ...\n",
			cmd->file, cmd->line, cmd->word);
		    exit(1);
		}
		if (arg->word[0] == '/'
			&& between(1, len= strtoul(arg->word+1, &end, 10), 31)
			&& *end == 0
		) {
		    ip= htonl((0xFFFFFFFFUL << (32-len)) & 0xFFFFFFFFUL);
		} else
		if (!name2ip(&ip, arg->word, ifip)) {
		    fprintf(stderr,
		    "\"%s\", line %u: Can't translate %s to an IP address\n",
			arg->file, arg->line, arg->word);
		    exit(1);
		}
		if (d <= arraylimit(data) - sizeof(ip)) {
		    memcpy(d, &ip, sizeof(ip));
		    d += sizeof(ip);
		}
		i++;
		break;}
	    case TT_NUMBER: {
		unsigned long n;
		int g;

		if (arg == nil || !(arg->flags & CFG_CLONG)) {
		    fprintf(stderr, "\"%s\", line %u: Usage: %s number ...\n",
			cmd->file, cmd->line, cmd->word);
		    exit(1);
		}
		n= strtoul(arg->word, nil, 0);
		g= tp->gran;
		do {
		    if (d <= arraylimit(data)) *d++ = (n >> (--g * 8)) & 0xFF;
		} while (g != 0);
		i++;
		break;}
	    case TT_OCTET: {
		if (arg == nil || !config_isatom(arg) || arg->next != nil) {
		    fprintf(stderr, "\"%s\", line %u: Usage: %s hexdigits\n",
			cmd->file, cmd->line, cmd->word);
		    exit(1);
		}
		i= ascii2octet(data, sizeof(data), arg->word);
		if (i == -1) {
		    fprintf(stderr,
			"\"%s\", line %u: %s: Bad hexdigit string\n",
			arg->file, arg->line, arg->word);
		    exit(1);
		}
		d= data + i;
		break;}
	    }
	} while ((arg= arg->next) != nil);

	if (d > data + 255) {
	    fprintf(stderr, "\"%s\", line %u: Tag value is way too big\n",
		cmd->file, cmd->line);
	    exit(1);
	}
	if ((tp->type & TF_TYPE) != TT_NUMBER && (i % tp->gran) != 0) {
	    fprintf(stderr,
		"\"%s\", line %u: Expected a multiple of %d initializers\n",
		cmd->file, cmd->line, tp->gran);
	    exit(1);
	}
	if (tp->max != 0 && i > tp->max) {
	    fprintf(stderr,
		"\"%s\", line %u: Got %d initializers, can have only %d\n",
		cmd->file, cmd->line, (int) i, tp->max);
	    exit(1);
	}
    }
    if (tp->type & TF_STATIC) {
	size_t len= tp->gran * tp->max;
	if ((tp->type & TF_TYPE) == TT_IP) len *= sizeof(ipaddr_t);
	memset(B(dp) + tp->tag, 0, len);
	memcpy(B(dp) + tp->tag, data, (d - data));
    } else {
	settag(dp, tp->tag, data, (d - data));
    }
}

int makedhcp(dhcp_t *dp, u8_t *class, size_t calen, u8_t *client, size_t cilen,
				ipaddr_t ip, ipaddr_t ifip, network_t *np)
{
    /* Fill in a DHCP packet at 'dp' for the host identified by the
     * (class, client, ip) combination.  Makedhcp is normally called twice,
     * once to find the IP address (so ip == 0) and once again to find all
     * data that goes with that IP address (ip != 0).  On the first call the
     * return value of this function should be ignored and only 'yiaddr'
     * checked and used as 'ip' on the next pass.  True is returned iff there
     * is information for the client on the network at interface address
     * 'ifip', by checking if the 'ip' and 'ifip' are on the same network.
     * If np is nonnull then we are working for one of our own interfaces, so
     * options can be set and adjourning interfaces can be programmed.
     */
    config_t *todo[16];
    size_t ntodo= 0;
    ipaddr_t hip, mask;
    u8_t *pmask;
    char *hostname;
    u32_t distance= -1;

    initdhcpconf();

    /* Start creating a packet. */
    dhcp_init(dp);
    dp->op= DHCP_BOOTREPLY;

    /* The initial TODO list is the whole DHCP config. */
    todo[ntodo++]= dhcpconf;

    while (ntodo > 0) {
	config_t *cmd, *follow;

	if (todo[ntodo-1] == nil) { ntodo--; continue; }
	cmd= todo[ntodo-1]->list;
	todo[ntodo-1]= todo[ntodo-1]->next;

	follow= nil;	/* Macro or list to follow next? */

	if (strcasecmp(cmd->word, "client") == 0) {
	    u8_t cfgid[CLID_MAX];
	    size_t cfglen;
	    char *name;
	    int ifno;
	    u32_t d;

	    if (between(3, config_length(cmd), 5)
		&& config_isatom(cmd->next)
		&& (cfglen= ascii2clid(cfgid, cmd->next->word)) != -1
		&& config_isatom(cmd->next->next)
		&& (((ifno= ifname2if(cmd->next->next->word)) == -1
			&& config_length(cmd) <= 4)
		    || ((ifno= ifname2if(cmd->next->next->word)) != -1
			&& config_length(cmd) >= 4
			&& config_isatom(cmd->next->next->next)))
	    ) {
		if (cilen == cfglen && memcmp(client, cfgid, cilen) == 0
		    && (ifno == -1 || np == nil || ifno == np->n)
		) {
		    config_t *atname= cmd->next->next;
		    if (ifno != -1) atname= atname->next;
		    name= atname->word;

		    if (name2ip(&hip, name, ifip) && (ip == 0 || ip == hip)) {
			d= ntohl(hip) ^ ntohl(ifip);
			if (d < distance) {
			    dp->yiaddr= hip;
			    follow= atname->next;
			    distance= d;
			}
		    }
		}
	    } else {
		fprintf(stderr,
	    "\"%s\", line %u: Usage: client ID [ip#] host [macro|{params}]\n",
		    cmd->file, cmd->line);
		exit(1);
	    }
	} else
	if (strcasecmp(cmd->word, "class") == 0) {
	    config_t *clist;
	    int match;

	    match= 0;
	    for (clist= cmd->next; clist != nil
				&& clist->next != nil
				&& config_isatom(clist); clist= clist->next) {
		if (calen > 0
		    && strncmp(clist->word, (char *) class, calen) == 0
		) {
		    match= 1;
		}
	    }
	    if (clist == cmd->next || clist->next != nil) {
		fprintf(stderr,
		"\"%s\", line %u: Usage: class class-name ... macro|{params}\n",
		    cmd->file, cmd->line);
	    }
	    if (match) follow= clist;
	} else
	if (strcasecmp(cmd->word, "host") == 0) {
	    if (config_length(cmd) == 3
		&& config_isatom(cmd->next)
	    ) {
		if (ip != 0) {
		    if (cidr_aton(cmd->next->word, &hip, &mask)) {
			if (((hip ^ ip) & mask) == 0) {
			    if (!gettag(dp, DHCP_TAG_NETMASK, nil, nil)) {
				settag(dp, DHCP_TAG_NETMASK,
						&mask, sizeof(mask));
			    }
			    dp->yiaddr= ip;
			    follow= cmd->next->next;
			}
		    } else
		    if (name2ip(&hip, cmd->next->word, ifip)) {
			if (hip == ip) {
			    dp->yiaddr= ip;
			    follow= cmd->next->next;
			}
		    }
		}
	    } else {
		fprintf(stderr,
		"\"%s\", line %u: Usage: host host-spec macro|{params}\n",
		    cmd->file, cmd->line);
		exit(1);
	    }
	} else
	if (strcasecmp(cmd->word, "interface") == 0) {
	    if (between(3, config_length(cmd), 4)
		&& config_isatom(cmd->next)
		&& config_isatom(cmd->next->next)
	    ) {
		network_t *ifnp;

		if (np != nil) {
		    if ((ifnp= if2net(ifname2if(cmd->next->word))) == nil) {
			fprintf(stderr,
			    "\"%s\", line %u: Can't find interface %s\n",
			    cmd->next->file, cmd->next->line, cmd->next->word);
			exit(1);
		    }
		    if (!name2ip(&hip, cmd->next->next->word, 0)) {
			fprintf(stderr,
			    "\"%s\", line %u: Can't find IP address of %s\n",
			    cmd->next->next->file, cmd->next->next->line,
			    cmd->next->next->word);
			exit(1);
		    }
		    ifnp->ip= hip;
		    if (ifnp == np) {
			dp->yiaddr= hip;
			follow= cmd->next->next->next;
		    }
		}
	    } else {
		fprintf(stderr,
		"\"%s\", line %u: Usage: interface ip# host%s\n",
		    cmd->file, cmd->line, ntodo==1 ? " [macro|{params}]" : "");
		exit(1);
	    }
	} else
	if (strcasecmp(cmd->word, "macro") == 0) {
	    if (config_length(cmd) == 2 && config_isatom(cmd->next)) {
		follow= cmd->next;
	    } else
	    if (ntodo > 1) {
		fprintf(stderr, "\"%s\", line %u: Usage: macro macro-name\n",
		    cmd->file, cmd->line);
		exit(1);
	    }
	} else
	if (strcasecmp(cmd->word, "tag") == 0) {
	    if (ntodo > 1) {
		fprintf(stderr,
		    "\"%s\", line %u: A %s can't be defined here\n",
		    cmd->file, cmd->line, cmd->word);
		exit(1);
	    }
	} else
	if (strcasecmp(cmd->word, "option") == 0) {
	    int ifno;
	    network_t *ifnp;
	    config_t *opt;

	    if ((opt= cmd->next) != nil
		&& config_isatom(opt)
		&& (ifno= ifname2if(opt->word)) != -1
	    ) {
		if ((ifnp= if2net(ifno)) == nil) {
		    fprintf(stderr,
			"\"%s\", line %u: Interface %s is not enabled\n",
			opt->file, opt->line, opt->word);
		    exit(1);
		}
		opt= opt->next;
	    } else {
		ifnp= np;
	    }

	    if (between(1, config_length(opt), 2)
		&& config_isatom(opt)
		&& strcasecmp(opt->word, "server") == 0
		&& (opt->next == nil
		    || strcasecmp(opt->next->word, "inform") == 0)
	    ) {
		if (np != nil) {
		    ifnp->flags |= NF_SERVING;
		    if (opt->next != nil) ifnp->flags |= NF_INFORM;
		}
	    } else
	    if (config_length(opt) == 2
		&& config_isatom(opt)
		&& strcasecmp(opt->word, "relay") == 0
		&& config_isatom(opt->next)
	    ) {
		if (np != nil) {
		    if (!name2ip(&hip, opt->next->word, ifip)) {
			fprintf(stderr,
			    "\"%s\", line %u: Can't find IP address of %s\n",
			    opt->next->file, opt->next->line,
			    opt->next->word);
			exit(1);
		    }
		    ifnp->flags |= NF_RELAYING;
		    ifnp->server= hip;
		}
	    } else
	    if (config_length(opt) == 1
		&& config_isatom(opt)
		&& strcasecmp(opt->word, "possessive") == 0
	    ) {
		if (np != nil) ifnp->flags |= NF_POSSESSIVE;
	    } else
	    if (config_length(opt) == 2
		&& config_isatom(opt)
		&& strcasecmp(opt->word, "hostname") == 0
		&& config_isatom(opt->next)
	    ) {
		if (np != nil) np->hostname= opt->next->word;
	    } else {
		fprintf(stderr, "\"%s\", line %u: Unknown option\n",
		    cmd->file, cmd->line);
		exit(1);
	    }
	} else {
	    /* Must be an actual data carrying tag. */
	    configtag(dp, cmd, ifip);
	}

	if (follow != nil) {
	    /* A client/class/host entry selects a macro or list that must
	     * be followed next.
	     */
	    config_t *macro;

	    if (config_isatom(follow)) {	/* Macro name */
		config_t *cfg;

		for (cfg= dhcpconf; cfg != nil; cfg= cfg->next) {
		    macro= cfg->list;

		    if (strcasecmp(macro->word, "macro") == 0) {
			if (config_length(macro) == 3
			    && config_isatom(macro->next)
			    && config_issub(macro->next->next)
			) {
			    if (strcasecmp(macro->next->word, follow->word) == 0
			    ) {
				break;
			    }
			} else {
			    fprintf(stderr,
			"\"%s\", line %u: Usage: macro macro-name {params}\n",
				macro->file, macro->line);
			}
		    }
		}
		follow= cfg == nil ? nil : macro->next->next->list;
	    } else {
		/* Simply a list of more tags and stuff. */
		follow= follow->list;
	    }

	    if (ntodo == arraysize(todo)) {
		fprintf(stderr, "\"%s\", line %u: Nesting is too deep\n",
		    follow->file, follow->line);
		exit(1);
	    }
	    todo[ntodo++]= follow;
	}
    }

    /* Check if the IP and netmask are OK for the interface. */
    if (!gettag(dp, DHCP_TAG_NETMASK, &pmask, nil)) return 0;
    memcpy(&mask, pmask, sizeof(mask));
    if (((ip ^ ifip) & mask) != 0) return 0;

    /* Fill in the hostname and/or domain. */
    if ((hostname= ip2name(ip)) != nil) {
	char *domain;

	if ((domain= strchr(hostname, '.')) != nil) *domain++ = 0;

	if (!gettag(dp, DHCP_TAG_HOSTNAME, nil, nil)) {
	    settag(dp, DHCP_TAG_HOSTNAME, hostname, strlen(hostname));
	}

	if (domain != nil && !gettag(dp, DHCP_TAG_DOMAIN, nil, nil)) {
	    settag(dp, DHCP_TAG_DOMAIN, domain, strlen(domain));
	}
    }

    return 1;
}

static char *dhcpopname(int op)
{
    static char *onames[] = { "??\?", "REQUEST", "REPLY" };
    return onames[op < arraysize(onames) ? op : 0];
}

char *dhcptypename(int type)
{
    static char *tnames[] = {
	"??\?", "DISCOVER", "OFFER", "REQUEST", "DECLINE",
	"ACK", "NAK", "RELEASE", "INFORM"
    };
    return tnames[type < arraysize(tnames) ? type : 0];
}

void printdhcp(dhcp_t *dp)
{
    /* Print the contents of a DHCP packet, usually for debug purposes. */
    tagdef_t *tp;
    u8_t *data, *ovld;
    size_t i, len;

    for (tp= alltagdef; tp < arraylimit(alltagdef); tp++) {
	if (tp->type & TF_STATIC) {
	    data= B(dp) + tp->tag;
	    len= tp->gran * tp->max;
	    if ((tp->type & TF_TYPE) == TT_IP) len *= sizeof(ipaddr_t);
	    if (tp->tag == doff(chaddr)) len= dp->hlen;

	    /* Don't show uninteresting stuff. */
	    if (tp->tag == doff(htype) && dp->htype == DHCP_HTYPE_ETH) continue;

	    if (tp->tag == doff(hlen) && dp->hlen == DHCP_HLEN_ETH) continue;

	    if ((tp->tag == doff(file) || tp->tag == doff(sname))
		&& gettag(dp, DHCP_TAG_OVERLOAD, &ovld, nil)
		&& (ovld[0] & (tp->tag == doff(file) ? 1 : 2))
	    ) {
		continue;
	    }
	    for (i= 0; i < len && data[i] == 0; i++) {}
	    if (i == len) continue;
	} else {
	    if (!gettag(dp, tp->tag, &data, &len)) continue;
	}

	if (tagdefined(tp)) {
	    printf("\t%s =", tp->name);
	} else {
	    printf("\tT%d =", tp->tag);
	}

	i= 0;
	while (i < len) {
	    switch (tp->type & TF_TYPE) {
	    case TT_ASCII: {
		printf(" \"%.*s\"", (int) len, data);
		i= len;
		break;}
	    case TT_BOOLEAN: {
		printf(data[i++] == 0 ? " false" : " true");
		break;}
	    case TT_IP: {
		ipaddr_t ip;
		memcpy(&ip, data+i, sizeof(ip));
		printf(" %s", inet_ntoa(ip));
		i += sizeof(ip);
		break;}
	    case TT_NUMBER: {
		u32_t n= 0;
		int g= tp->gran;

		do n= (n << 8) | data[i++]; while (--g != 0);
		printf(" %lu", (unsigned long) n);
		if ((tp->type & TF_STATIC) && tp->tag == doff(op)) {
		    printf(" (%s)", dhcpopname(n));
		}
		if (!(tp->type & TF_STATIC) && tp->tag == DHCP_TAG_TYPE) {
		    printf(" (%s)", dhcptypename(n));
		}
		break;}
	    case TT_OCTET: {
		if (i == 0) fputc(' ', stdout);
		printf("%02X", data[i++]);
		break;}
	    }
	}
	fputc('\n', stdout);
    }
}
