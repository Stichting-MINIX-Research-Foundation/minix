/* Interface definitions for ethernet access library */

typedef union etheraddr
{
    unsigned char bytes[6];		/* byteorder safe initialization */
    unsigned short shorts[3];		/* force 2-byte alignment */
}
	  ether_addr;

typedef struct etherpacket
{
    ether_addr dest;
    ether_addr src;
    unsigned char type[2];		/* in network byte order! */
    unsigned short pktlen;		/* length of pktbuf ONLY */
    char *pktbuf;
}
	    ether_packet;

typedef struct ethervec
{
    ether_addr dest;
    ether_addr src;
    unsigned char type[2];		/* in network byte order! */
    unsigned short iovcnt;		/* number of iovec to use */
    struct iovec *iov;			/* ptr to array of iovec */
}
	 ether_vec;

#ifndef __ETHER_BCAST_ADDR__
extern ether_addr ether_bcast_addr;
#endif

#ifdef __STDC__

int ether_open (char *name, unsigned type, ether_addr * address);

ether_addr *ether_address (int fd, ether_addr * address);

ether_addr *ether_intfaddr (char *intf, ether_addr * address);

char **ether_interfaces (void);

int ether_write (int fd, ether_packet * packet);

int ether_writev (int fd, ether_vec * packet);

int ether_read (int fd, ether_packet * packet);

int ether_readv (int fd, ether_vec * packet);

int ether_blocking (int fd, int state);

int ether_send_self (int fd);

int ether_mcast_self (int fd);

int ether_bcast_self (int fd);

char *ether_ntoa (ether_addr *);

ether_addr *ether_aton (char *);

#ifdef __GNUC__

/*
 * Avoid stupid warnings if structs aren't defined
 */

typedef struct in_addr *_ether_NoNsEnSe;
typedef struct hostent *_ether_nOnSeNsE;

#endif

char *ether_e2a (ether_addr *, char *);

ether_addr *ether_a2e (char *, ether_addr *);

struct in_addr *ether_e2ip (ether_addr *, struct in_addr *);

ether_addr *ether_ip2e (struct in_addr *, ether_addr *);

char *ether_e2host (ether_addr *, char *);

ether_addr *ether_host2e (char *, ether_addr *);

ether_addr *ether_hostent2e (struct hostent *, ether_addr *);

#else

int ether_open ();
ether_addr *ether_address ();
ether_addr *ether_intfaddr ();
char **ether_interfaces ();
int ether_write ();
int ether_writev ();
int ether_read ();
int ether_readv ();
int ether_blocking ();
int ether_send_self ();
int ether_mcast_self ();
int ether_bcast_self ();

char *ether_ntoa ();
ether_addr *ether_aton ();
char *ether_e2a ();
ether_addr *ether_a2e ();
struct in_addr *ether_e2ip ();
ether_addr *ether_ip2e ();
char *ether_e2host ();
ether_addr *ether_host2e ();
ether_addr *ether_hostent2e ();

#endif

#undef ether_cmp			/* lose def from netinet/if_ether.h */

#define ether_cmp(addr1,addr2) \
 ((addr1)->shorts[0] != (addr2)->shorts[0] \
  || (addr1)->shorts[1] != (addr2)->shorts[1] \
  || (addr1)->shorts[2] != (addr2)->shorts[2])

#define ETHERSTRLEN 18			/* max length of "xx:xx:xx:xx:xx:xx" */

#ifdef NOFILE				/* i.e. we have included sys/param.h */
#ifndef MAXHOSTNAMELEN			/* but MAXHOSTNAMELEN still isnt set */
#define MAXHOSTNAMELEN 64
#endif
#endif

/* should be defined in terms of ether_packet struct; need offsetof() macro */

#define ETHER_DST	0
#define ETHER_SRC	6
#define ETHER_TYPE	12
#define ETHER_PKT	14
#define ETHER_MIN	46
#define ETHER_MAX	1500

#define ETHER_MINTYPE	0x5DD		/* lowest protocol not valid IEEE802 */
#define ETHER_MAXTYPE	0xFFFF		/* largest possible protocol */

#define ETHER_MCAST(addr) (((unsigned char *) (addr))[0] & 0x01)

#ifdef NT_ALLTYPES
#define ETHER_ALLTYPES NT_ALLTYPES
#else
#define ETHER_ALLTYPES ((unsigned) -1)
#endif
