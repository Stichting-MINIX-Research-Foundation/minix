/* proxy.c Copyright 2000 by Michael Temari All Rights Reserved */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <net/netlib.h>
#include <net/hton.h>
#include <net/gen/in.h>
#include <net/gen/inet.h>
#include <net/gen/tcp.h>
#include <net/gen/tcp_io.h>
#include <net/gen/socket.h>
#include <net/gen/netdb.h>

#include "config.h"
#include "http.h"
#include "utility.h"
#include "net.h"

_PROTOTYPE(static int connect, (char *host));
#if 0
_PROTOTYPE(static int readline, (char *p, int len));
#endif
_PROTOTYPE(static int sendout, (int fd, char *data));

static int connect(host)
char *host;
{
nwio_tcpconf_t tcpconf;
nwio_tcpcl_t tcpcopt;
char *tcp_device;
int netfd;
ipaddr_t nethost;
tcpport_t netport = 0;
struct hostent *hp;
struct servent *sp;
char *p;
int s;
int tries;

   p = host;
   while(*p && *p != ':') p++;
   if(*p == ':') {
   	*p++ = '\0';
   	netport = htons(atoi(p));
   }

   if((hp = gethostbyname(host)) == (struct hostent *)NULL) {
	fprintf(stderr, "Unknown host %s!\n", host);  
	return(-1);
   } else
	memcpy((char *) &nethost, (char *) hp->h_addr, hp->h_length);

   /* Now, to which port must we connect? */
   if(netport == 0)
	if((sp = getservbyname("http", "tcp")) == (struct servent *)NULL) {
		fprintf(stderr, "HTTP port is unknown????\n");
		return(-1);
	} else
		netport = sp->s_port;

   /* Connect to the host */
   if((tcp_device = getenv("TCP_DEVICE")) == NULL)
	tcp_device = TCP_DEVICE;

   if((netfd = open(tcp_device, O_RDWR)) < 0) {
	perror("httpget: opening tcp");
	return(-1);
   }

   tcpconf.nwtc_flags = NWTC_LP_SEL | NWTC_SET_RA | NWTC_SET_RP;
   tcpconf.nwtc_remaddr = nethost;
   tcpconf.nwtc_remport = netport;

   s = ioctl(netfd, NWIOSTCPCONF, &tcpconf);
   if(s < 0) {
	perror("httpget: NWIOSTCPCONF");
	close(netfd);
	return(-1);
   }

   s = ioctl(netfd, NWIOGTCPCONF, &tcpconf);
   if(s < 0) {
	perror("httpget: NWIOGTCPCONF");
	close(netfd);
	return(-1);
   }

   tcpcopt.nwtcl_flags = 0;

   tries = 0;
   do {
	s = ioctl(netfd, NWIOTCPCONN, &tcpcopt);
	if(s == -1 && errno == EAGAIN) {
		if(tries++ >= 10)
			break;
		sleep(10);
	} else
		break;
   } while(1);

   if(s < 0) {
	perror("httpget: NWIOTCPCONN");
	close(netfd);
	return(-1);
   }

   return(netfd);
}

char buffer[8192];

#if 0
static int readline(p, len)
char *p;
int len;
{
int c;
int cr = 0;
int n = 0;

   len--;
   if(len < 0) return(-1);
   while(len > 0 && (c = getchar()) != EOF) {
   	if(c == '\n' && cr) {
   		*p = '\0';
   		return(n);
   	}
   	if(c == '\r') {
   		cr = 1;
   		continue;
   	}
   	n++;
   	*p++ = c;
   }
   *p = '\0';
   return(n);
}
#endif

static int sendout(fd, data)
int fd;
char *data;
{
   if(strlen(data) > 0)
	write(fd, data, strlen(data));
	write(fd, "\r\n", 2);
	if(dbglog != (FILE *)NULL) {
		fprintf(dbglog, "REPLY: %s\n", data);
	fflush(dbglog);
   }

   return(0);
}

void proxy(rq, rp)
struct http_request *rq;
struct http_reply *rp;
{
int s;
char *p;
char *ps;
char *b;
char *host;
static char user[256];
static char pass[256];
char *url;
char *at;
int fd;
int bad;

   while(1) {
   	bad = 0;
   	p = rq->uri;
   	if(tolower(*p++) != 'h') bad++;
   	if(tolower(*p++) != 't') bad++;
   	if(tolower(*p++) != 't') bad++;
   	if(tolower(*p++) != 'p') bad++;
   	if(tolower(*p++) != ':') bad++;
   	if(tolower(*p++) != '/') bad++;
   	if(tolower(*p++) != '/') bad++;
   	if(bad) {
		sprintf(buffer, "HTTP/%d.%d 400 Bad Request",
			rq->vmajor, rq->vminor);
		sendout(1, buffer);
		sendout(1, "");
		sendout(1, "Proxy Request was not http:");
		return;
   	}
   	host = p;
	while(*p && *p != '/') p++;
	url = p;
	*url = '\0';
	at = strchr(host, '@');
	if(at != (char *)NULL) {
		*at = '\0';
		p = host;
		while(*p && *p != ':') p++;
		if(*p)
			*p++ = '\0';
		strcpy(user, host);
		strcpy(pass, p);
		host = at + 1;
	} else {
		user[0] = '\0';
		pass[0] = '\0';
	}

   	fd = connect(host);
   	if(fd < 0) {
		sprintf(buffer, "HTTP/%d.%d 400 Bad Request",
			rq->vmajor, rq->vminor);
		sendout(1, buffer);
		sendout(1, "");
		sendout(1, "Could not connect to host");
		return;
   	}
   	if(rq->method == HTTP_METHOD_GET)
   		write(fd, "GET ", 4); else
   	if(rq->method == HTTP_METHOD_POST)
   		write(fd, "POST ", 5);
   	*url = '/';
   	if(strlen(url) > 0)
   		write(fd, url, strlen(url));
   	write(fd, " ", 1);
	sprintf(buffer, "HTTP/%d.%d", rq->vmajor, rq->vminor);
	sendout(fd, buffer);
   	if(rq->ifmodsince != -1) {
   		write(fd, "If-Mod-Since: ", 14);
   		sendout(fd, httpdate(&rq->ifmodsince));
   	}
	if(rq->size != 0) {
		sendout(fd, "Content-Type: application/x-www-form-urlencoded");
		sprintf(buffer, "Content-Length: %lu", rq->size);
		sendout(fd, buffer);
	}
	if(*rq->cookie) {
		sprintf(buffer, "Cookie: %s", rq->cookie);
		sendout(fd, buffer);
	}
	if(*rq->useragent) {
		sprintf(buffer, "User-Agent: %s", rq->useragent);
		sendout(fd, buffer);
	}
	if(*rq->host) {
		sprintf(buffer, "Host: %s", rq->host);
		sendout(fd, buffer);
	}
	if(*rq->wwwauth) {
		sprintf(buffer, "Authorization: %s", rq->wwwauth);
		sendout(fd, buffer);
	}
	sprintf(buffer, "X-Forwarded-From: %s", rmthostaddr);
	sendout(fd, buffer);
   	sendout(fd, "");
	if(rq->size != 0) {
		if(stdlog != (FILE *)NULL) {
			fprintf(stdlog, "%s %s %d %d ",
				logdate((time_t *)NULL), rmthostname,
				rq->method, rp->status);
			fprintf(stdlog, "proxy %s?", rq->uri);
		}
	   	while((s = read(0, buffer, rq->size >
   			sizeof(buffer) ? sizeof(buffer) : rq->size)) > 0) {
   			write(fd, buffer, s);
   			rq->size -= s;
   			b = buffer;
   			if(stdlog != (FILE *)NULL)
   				while(s--) fputc(*b++, stdlog);
			if(rq->size == 0) break;
   		}
   		if(stdlog != (FILE *)NULL) {
   			fprintf(stdlog, "\n");
			fflush(stdlog);
		}
   	}
   	while((s = read(fd, buffer, sizeof(buffer))) > 0) {
   		write(1, buffer, s);
   	}
   	close(fd);
   	return;
   }
}
