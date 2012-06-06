/* net.c Copyright 1992-2000 by Michael Temari All Rights Reserved
 *
 * This file is part of ftp.
 *
 *
 * 01/25/96 Initial Release	Michael Temari, <Michael@TemWare.Com>
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <net/netlib.h>
#include <net/hton.h>
#include <net/gen/netdb.h>
#include <net/gen/in.h>
#include <net/gen/inet.h>
#include <net/gen/tcp.h>
#include <net/gen/tcp_io.h>

#include "ftp.h"
#include "xfer.h"
#include "net.h"

void donothing(int sig);

int ftpcomm_fd;
static ipaddr_t myip;
static ipaddr_t hostip;
static char host[256];
static int lpid;

int NETinit()
{
int s;
char *tcp_device;
int tcp_fd;
nwio_tcpconf_t nwio_tcpconf;

   /* All this just to get our ip address */

   if((tcp_device = getenv("TCP_DEVICE")) == (char *)NULL)
	tcp_device = TCP_DEVICE;

   tcp_fd = open(tcp_device, O_RDWR);
   if(tcp_fd < 0) {
	perror("ftp: Could not open tcp_device");
	return(-1);
   }
   s = ioctl(tcp_fd, NWIOGTCPCONF, &nwio_tcpconf);
   if(s < 0) {
	perror("ftp: Could not get tcp configuration");
	return(-1);
   }

   myip = nwio_tcpconf.nwtc_locaddr;

   close(tcp_fd);

   return(0);
}

int DOopen()
{
nwio_tcpconf_t tcpconf;
nwio_tcpcl_t tcpcopt;
char *tcp_device;
tcpport_t port;
int s;
struct hostent *hp;
struct servent *servent;

   if(linkopen) {
	printf("Use \"CLOSE\" to close the connection first.\n");
	return(0);
   }

   if(cmdargc < 2) {
	if(readline("Host: ", host, sizeof(host)) < 0)
		return(-1);
   } else
	strncpy(host, cmdargv[1], sizeof(host));

   if((servent = getservbyname("ftp", "tcp")) == (struct servent *)NULL) {
   	fprintf(stderr, "ftp: Could not find ftp tcp service\n");
   	port = htons(21);
   } else
	port = (tcpport_t)servent->s_port;

   hp = gethostbyname(host);
   if(hp == (struct hostent *)NULL) {
	hostip = (ipaddr_t)0;
	printf("Unresolved host %s\n", host);
	return(0);
   } else
	memcpy((char *) &hostip, (char *) hp->h_addr, hp->h_length);

   /* This HACK allows the server to establish data connections correctly */
   /* when using the loopback device to talk to ourselves */
#ifdef __NBSD_LIBC
   if((hostip & ntohl(0xFF000000)) == inet_addr("127.0.0.0"))
#else
   if((hostip & NTOHL(0xFF000000)) == inet_addr("127.0.0.0"))
#endif
	hostip = myip;

   if((tcp_device = getenv("TCP_DEVICE")) == NULL)
	tcp_device = "/dev/tcp";

   if((ftpcomm_fd = open(tcp_device, O_RDWR)) < 0) {
	perror("ftp: open error on tcp device");
	return(-1);
   }

   tcpconf.nwtc_flags = NWTC_LP_SEL | NWTC_SET_RA | NWTC_SET_RP;
   tcpconf.nwtc_remaddr = hostip;
   tcpconf.nwtc_remport = port;

   s = ioctl(ftpcomm_fd, NWIOSTCPCONF, &tcpconf);
   if(s < 0) {
	perror("ftp: ioctl error on NWIOSTCPCONF");
	close(ftpcomm_fd);
	return(-1);
   }

   tcpcopt.nwtcl_flags = 0;

   s = ioctl(ftpcomm_fd, NWIOTCPCONN, &tcpcopt);
   if(s < 0) {
	perror("ftp: ioctl error on NWIOTCPCONN");
	close(ftpcomm_fd);
	return(-1);
   }

   s = ioctl(ftpcomm_fd, NWIOGTCPCONF, &tcpconf);
   if(s < 0) {
	perror("ftp: ioctl error on NWIOGTCPCONF");
	close(ftpcomm_fd);
	return(-1);
   }

   s = DOgetreply();

   if(s < 0) {
	close(ftpcomm_fd);
	return(s);
   }

   if(s != 220) {
	close(ftpcomm_fd);
	return(0);
   }

   linkopen = 1;

   return(s);
}

int DOclose()
{
   if(!linkopen) {
	printf("You can't close a connection that isn't open.\n");
	return(0);
   }

   close(ftpcomm_fd);

   linkopen = 0;
   loggedin = 0;

   return(0);
}

int DOquit()
{
int s;

   if(linkopen) {
	s = DOcommand("QUIT", "");
	s = DOclose();
   }

   printf("FTP done.\n");

   exit(0);
}

void donothing(sig)
int sig;
{
}

int DOdata(datacom, file, direction, fd)
char *datacom;
char *file;
int direction;  /* RETR or STOR */
int fd;
{
nwio_tcpconf_t tcpconf;
nwio_tcpcl_t tcplopt, tcpcopt;
char *tcp_device;
static int ftpdata_fd = -1;
char *buff;
ipaddr_t ripaddr;
tcpport_t rport;
static tcpport_t lport;
int s;
int i;
int wpid;
int cs;
int pfd[2];
char dummy;
char port[32];
int wasopen;

   lport = htons(0xF000);

#ifdef DEBUG
   printf("DOdata %s %s %d %d\n", datacom, file, direction, fd);
#endif

   ripaddr = hostip;
#ifdef __NBSD_LIBC
   rport = htons(2);
#else
   rport = HTONS(20);
#endif

   /* here we set up a connection to listen on if not passive mode */
   /* otherwise we use this to connect for passive mode */

   if((tcp_device = getenv("TCP_DEVICE")) == NULL)
	tcp_device = "/dev/tcp";

   if(ftpdata_fd >= 0 && mode != MODE_B) {
   	close(ftpdata_fd);
   	ftpdata_fd = -1;
   }

   wasopen = (ftpdata_fd >= 0);

#ifdef DEBUG
   printf("wasopen = %d\n", wasopen);
#endif

   if(wasopen)
   	goto WASOPEN;

#ifdef DEBUG
   printf("b4 open = %d\n", ftpdata_fd);
#endif

   if(ftpdata_fd == -1)
	if((ftpdata_fd = open(tcp_device, O_RDWR)) < 0) {
		perror("ftp: open error on tcp device");
		return(0);
	}

#ifdef DEBUG
   printf("at open = %d\n", ftpdata_fd);
#endif

   if(passive) {
#ifdef DEBUG
	printf("b4 PASV command\n");
#endif
	s = DOcommand("PASV", "");
#ifdef DEBUG
	printf("PASV command returned %d\n", s);
#endif
	if(s != 227) {
		close(ftpdata_fd);
		ftpdata_fd = -1;
		return(s);
	}
	/* decode host and port */
	buff = reply;
	while(*buff && (*buff != '(')) buff++;
	buff++;
	ripaddr = (ipaddr_t)0;
	for(i = 0; i < 4; i++) {
		ripaddr = (ripaddr << 8) + (ipaddr_t)atoi(buff);
		if((buff = strchr(buff, ',')) == (char *)0) {
			printf("Could not parse PASV reply\n");
			return(0);
		}
		buff++;
	}
	rport = (tcpport_t)atoi(buff);
	if((buff = strchr(buff, ',')) == (char *)0) {
		printf("Could not parse PASV reply\n");
		return(0);
	}
	buff++;
	rport = (rport << 8) + (tcpport_t)atoi(buff);
	ripaddr = ntohl(ripaddr);
	rport = ntohs(rport);
#ifdef DEBUG
	printf("PASV %08x %04x\n", ripaddr, rport);
#endif
   }

   while(1) {
	tcpconf.nwtc_flags = NWTC_SET_RA | NWTC_SET_RP;
	if(passive || ntohs(lport) >= 0xF000) {
		tcpconf.nwtc_flags |= NWTC_LP_SEL;
	} else {
		/* For no good reason Sun hosts don't like it if they have to
		 * connect to the same port twice in a short time...
		 */
		lport = htons(ntohs(lport) + 1);
		tcpconf.nwtc_flags |= NWTC_LP_SET;
		tcpconf.nwtc_locport = lport;
	}

	tcpconf.nwtc_remaddr = ripaddr;
	tcpconf.nwtc_remport = rport;

#ifdef DEBUG
	printf("b4 STCPCONF locport = %d\n", lport);
#endif

	s = ioctl(ftpdata_fd, NWIOSTCPCONF, &tcpconf);
#ifdef DEBUG
	printf("at STCPCONF %d %d\n", s, errno);
#endif
	if(s < 0) {
		if(errno == EADDRINUSE) continue;
		perror("ftp: ioctl error on NWIOSTCPCONF");
		close(ftpdata_fd);
		ftpdata_fd = -1;
		return(0);
	}
	break;
   }

#ifdef DEBUG
	printf("b4 GTCPCONF\n");
#endif
   s = ioctl(ftpdata_fd, NWIOGTCPCONF, &tcpconf);
#ifdef DEBUG
   printf("at GTCPCONF %d %d\n", s, errno);
#endif
   if(s < 0) {
	perror("ftp: ioctl error on NWIOGTCPCONF");
	close(ftpdata_fd);
	ftpdata_fd = -1;
	return(0);
   }
   lport = tcpconf.nwtc_locport;

#ifdef DEBUG
   printf("lport = %04x\n", lport);
#endif

   if(passive) {
   	/* passive mode we connect to them */
	tcpcopt.nwtcl_flags = 0;
#ifdef DEBUG
	printf("Doing TCPCONN\n");
#endif
	s = ioctl(ftpdata_fd, NWIOTCPCONN, &tcpcopt);
#ifdef DEBUG
	printf("TCPCONN %d %d\n", s, errno);
#endif
	if(s < 0) {
		perror("ftp: error on ioctl NWIOTCPCONN");
		close(ftpdata_fd);
		ftpdata_fd = -1;
		return(0);
	}
#ifdef DEBUG
	printf("GTCPCONF\n");
#endif
	s = ioctl(ftpdata_fd, NWIOGTCPCONF, &tcpconf);
#ifdef DEBUG
	printf("GTCPCONF %d %d\n", s, errno);
#endif
	if(s < 0) {
		perror("ftp: error on ioctl NWIOGTCPCONF");
		close(ftpdata_fd);
		ftpdata_fd = -1;
		return(0);
	}
   } else {
   	/* we listen for them */
	tcplopt.nwtcl_flags = 0;
#ifdef DEBUG
	printf("Listen\n");
#endif

	if (pipe(pfd) < 0) {
		perror("ftp: could not create a pipe");
		close(ftpdata_fd);
		ftpdata_fd = -1;
		return(0);
	}
	lpid = fork();
	if(lpid < 0) {
		perror("ftp: could not fork listener");
		close(ftpdata_fd);
		ftpdata_fd = -1;
		close(pfd[0]);
		close(pfd[1]);
		return(0);
	} else if(lpid == 0) {
#ifdef DEBUG
		printf("Child here\n");
#endif
		close(pfd[0]);
		signal(SIGALRM, donothing);
		alarm(15);
		close(pfd[1]);
#ifdef DEBUG
		printf("child TCPLISTEN\n");
#endif
		s = ioctl(ftpdata_fd, NWIOTCPLISTEN, &tcplopt);
		alarm(0);
#ifdef DEBUG
		printf("listen %d %d\n", s, errno);
#endif
		if(s < 0)
			exit(errno);		/* error */
		else
			exit(0);		/* connection made */
	}
#ifdef DEBUG
	printf("Fork = %d\n", lpid);
#endif
	/* Wait for the pipe to close, then the listener is ready (almost). */
	close(pfd[1]);
	(void) read(pfd[0], &dummy, 1);
	close(pfd[0]);
	while(1) {
		wpid = waitpid(lpid, &cs, WNOHANG);
#ifdef DEBUG
		printf("waitpid %d %d\n", wpid, cs);
		printf("GTCPCONF loop\n");
#endif
		if(wpid != 0) break;
		signal(SIGALRM, donothing);
		alarm(1);
		s = ioctl(ftpdata_fd, NWIOGTCPCONF, &tcpconf);
		alarm(0);
#ifdef DEBUG
		printf("GTCPCONF loop %d %d\n", s, errno);
#endif
		if(s == -1) break;
		sleep(1);
	}
#ifdef DEBUG
	printf("GTCPCONF = %d\n", s);
#endif
   }

#define hiword(x)       ((u16_t)((x) >> 16))
#define loword(x)       ((u16_t)(x & 0xffff)) 
#define hibyte(x)       (((x) >> 8) & 0xff)
#define lobyte(x)       ((x) & 0xff)

   if(!passive) {
   	if(wpid != 0) {
   		close(ftpdata_fd);
   		ftpdata_fd = -1;
		cs = (cs >> 8) & 0x00ff;
		printf("Child listener error %s\n", strerror(cs));
   		return(0);
   	}
	sprintf(port, "%u,%u,%u,%u,%u,%u",
		hibyte(hiword(ntohl(myip))), lobyte(hiword(ntohl(myip))),
		hibyte(loword(ntohl(myip))), lobyte(loword(ntohl(myip))),
		hibyte(ntohs(lport)), lobyte(ntohs(lport)));
#ifdef DEBUG
	printf("sending port command %s\n", port);
#endif
	s = DOcommand("PORT", port);
#ifdef DEBUG
	printf("port command = %d\n", s);
#endif
	if(s != 200) {
		close(ftpdata_fd);
		ftpdata_fd = -1;
		kill(lpid, SIGKILL);
		(void) wait(&cs);
		return(s);
	}
   }

WASOPEN:

#ifdef DEBUG
   printf("doing data command %s %s\n", datacom, file);
#endif
   s = DOcommand(datacom, file);
#ifdef DEBUG
   printf("do command reply %d\n", s);
#endif
   if(s == 125 || s == 150) {
	if(!passive && !wasopen) {
		while(1) {
#ifdef DEBUG
			printf("Waiting for child %d\n", lpid);
#endif
			s = wait(&cs);
#ifdef DEBUG
			printf("Wait returned %d cs=%d errno=%d\n", s, cs, errno);
#endif
			if(s < 0 || s == lpid)
				break;
		}
		if(s < 0) {
			perror("wait error:");
			close(ftpdata_fd);
			ftpdata_fd = -1;
			kill(lpid, SIGKILL);
			(void) wait(&cs);
			return(s);
		}
		if((cs & 0x00ff)) {
			printf("Child listener failed %04x\n", cs);
			close(ftpdata_fd);
			ftpdata_fd = -1;
			return(-1);
		}
		cs = (cs >> 8) & 0x00ff;
		if(cs) {
			printf("Child listener error %s\n", strerror(cs));
			close(ftpdata_fd);
			ftpdata_fd = -1;
			return(DOgetreply());
		}
	}
#ifdef DEBUG
	printf("Before recvfile/sendfile call\n");
#endif
	switch(direction) {
		case RETR:
			s = recvfile(fd, ftpdata_fd);
			break;
		case STOR:
			s = sendfile(fd, ftpdata_fd);
			break;
	}
#ifdef DEBUG
	printf("send/recieve %d\n", s);
#endif
	if(mode != MODE_B) {
		close(ftpdata_fd);
		ftpdata_fd = -1;
	}

	s = DOgetreply();
#ifdef DEBUG
	printf("send/recieve reply %d\n", s);
#endif
	if(mode == MODE_B && s == 226) {
		close(ftpdata_fd);
		ftpdata_fd = -1;
	}
   } else {
	if(!passive) {
		kill(lpid, SIGKILL);
		(void) wait(&cs);
	}
	close(ftpdata_fd);
	ftpdata_fd = -1;
   }

   return(s);
}
