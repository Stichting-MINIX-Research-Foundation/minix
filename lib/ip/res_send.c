/*
 * Copyright (c) 1985, 1989 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)res_send.c	6.27 (Berkeley) 2/24/91";
#endif /* LIBC_SCCS and not lint */

/*
 * Send query to name server and wait for reply.
 */

#if !_MINIX
#include <sys/param.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <stdio.h>
#include <errno.h>
#include <resolv.h>
#include <unistd.h>
#include <string.h>

#else /* _MINIX */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <net/hton.h>

#include <net/netlib.h>
#include <net/gen/in.h>
#include <net/gen/inet.h>
#include <net/gen/netdb.h>
#include <net/gen/nameser.h>
#include <net/gen/resolv.h>
#include <net/gen/tcp.h>
#include <net/gen/tcp_io.h>
#include <net/gen/udp.h>
#include <net/gen/udp_hdr.h>
#include <net/gen/udp_io.h>

static int tcp_connect _ARGS(( ipaddr_t host, Tcpport_t port, int *terrno ));
static int tcpip_writeall _ARGS(( int fd, const char *buf, size_t siz ));
static int udp_connect _ARGS(( void ));
static int udp_sendto _ARGS(( int fd, const char *buf, unsigned buflen,
				ipaddr_t addr, Udpport_t port ));
static int udp_receive _ARGS(( int fd, char *buf, unsigned buflen,
				time_t timeout ));
static void alarm_handler _ARGS(( int sig ));

#endif /* !_MINIX */

static int s = -1;	/* socket used for communications */
#if !_MINIX
static struct sockaddr no_addr;

#ifndef FD_SET
#define	NFDBITS		32
#define	FD_SETSIZE	32
#define	FD_SET(n, p)	((p)->fds_bits[(n)/NFDBITS] |= (1 << ((n) % NFDBITS)))
#define	FD_CLR(n, p)	((p)->fds_bits[(n)/NFDBITS] &= ~(1 << ((n) % NFDBITS)))
#define	FD_ISSET(n, p)	((p)->fds_bits[(n)/NFDBITS] & (1 << ((n) % NFDBITS)))
#define FD_ZERO(p)	bzero((char *)(p), sizeof(*(p)))
#endif /* FD_SET */
#endif /* _MINIX */

res_send(buf, buflen, answer, anslen)
	const char *buf;
	int buflen;
	char *answer;
	int anslen;
{
	register int n;
	int try, v_circuit, resplen, ns;
	int gotsomewhere = 0, connected = 0;
	int connreset = 0;
#if !_MINIX
	u_short id, len;
#else /* _MINIX */
	u16_t id, len;
#endif /* !_MINIX */
	char *cp;
#if !_MINIX
	fd_set dsmask;
	struct timeval timeout;
	HEADER *hp = (HEADER *) buf;
	HEADER *anhp = (HEADER *) answer;
	struct iovec iov[2];
#else /* _MINIX */
	time_t timeout;
	dns_hdr_t *hp = (dns_hdr_t *) buf;
	dns_hdr_t *anhp = (dns_hdr_t *) answer;
#endif /* !_MINIX */
	int terrno = ETIMEDOUT;
	char junk[512];

#ifdef DEBUG
	if (_res.options & RES_DEBUG) {
		printf("res_send()\n");
		__p_query(buf);
	}
#endif /* DEBUG */
	if (!(_res.options & RES_INIT))
		if (res_init() == -1) {
			return(-1);
		}

	v_circuit = (_res.options & RES_USEVC) || buflen > PACKETSZ;
#if !_MINIX
	id = hp->id;
#else /* _MINIX */
	id = hp->dh_id;
#endif /* !_MINIX */
	/*
	 * Send request, RETRY times, or until successful
	 */
	for (try = 0; try < _res.retry; try++) {
	   for (ns = 0; ns < _res.nscount; ns++) {
#ifdef DEBUG
#if !_MINIX
		if (_res.options & RES_DEBUG)
			printf("Querying server (# %d) address = %s\n", ns+1,
			      inet_ntoa(_res.nsaddr_list[ns].sin_addr));
#else /* _MINIX */
		if (_res.options & RES_DEBUG)
			printf("Querying server (# %d) address = %s\n", ns+1,
			      inet_ntoa(_res.nsaddr_list[ns]));
#endif /* !_MINIX */
#endif /* DEBUG */
	usevc:
		if (v_circuit) {
#if !_MINIX
			int truncated = 0;

			/*
			 * Use virtual circuit;
			 * at most one attempt per server.
			 */
			try = _res.retry;
			if (s < 0) {
				s = socket(AF_INET, SOCK_STREAM, 0);
				if (s < 0) {
					terrno = errno;
#ifdef DEBUG
					if (_res.options & RES_DEBUG)
					    perror("socket (vc) failed");
#endif /* DEBUG */
					continue;
				}
				if (connect(s,
				    (struct sockaddr *)&(_res.nsaddr_list[ns]),
				    sizeof(struct sockaddr)) < 0) {
					terrno = errno;
#ifdef DEBUG
					if (_res.options & RES_DEBUG)
					    perror("connect failed");
#endif /* DEBUG */
					(void) close(s);
					s = -1;
					continue;
				}
			}
			/*
			 * Send length & message
			 */
			len = htons((u_short)buflen);
			iov[0].iov_base = (caddr_t)&len;
			iov[0].iov_len = sizeof(len);
			iov[1].iov_base = (char *)buf;
			iov[1].iov_len = buflen;
			if (writev(s, iov, 2) != sizeof(len) + buflen) {
				terrno = errno;
#ifdef DEBUG
				if (_res.options & RES_DEBUG)
					perror("write failed");
#endif /* DEBUG */
				(void) close(s);
				s = -1;
				continue;
			}
			/*
			 * Receive length & response
			 */
			cp = answer;
			len = sizeof(short);
			while (len != 0 &&
			    (n = read(s, (char *)cp, (int)len)) > 0) {
				cp += n;
				len -= n;
			}
			if (n <= 0) {
				terrno = errno;
#ifdef DEBUG
				if (_res.options & RES_DEBUG)
					perror("read failed");
#endif /* DEBUG */
				(void) close(s);
				s = -1;
				/*
				 * A long running process might get its TCP
				 * connection reset if the remote server was
				 * restarted.  Requery the server instead of
				 * trying a new one.  When there is only one
				 * server, this means that a query might work
				 * instead of failing.  We only allow one reset
				 * per query to prevent looping.
				 */
				if (terrno == ECONNRESET && !connreset) {
					connreset = 1;
					ns--;
				}
				continue;
			}
			cp = answer;
			if ((resplen = ntohs(*(u_short *)cp)) > anslen) {
#ifdef DEBUG
				if (_res.options & RES_DEBUG)
					fprintf(stderr, "response truncated\n");
#endif /* DEBUG */
				len = anslen;
				truncated = 1;
			} else
				len = resplen;
			while (len != 0 &&
			   (n = read(s, (char *)cp, (int)len)) > 0) {
				cp += n;
				len -= n;
			}
			if (n <= 0) {
				terrno = errno;
#ifdef DEBUG
				if (_res.options & RES_DEBUG)
					perror("read failed");
#endif /* DEBUG */
				(void) close(s);
				s = -1;
				continue;
			}
			if (truncated) {
				/*
				 * Flush rest of answer
				 * so connection stays in synch.
				 */
				anhp->tc = 1;
				len = resplen - anslen;
				while (len != 0) {
					n = (len > sizeof(junk) ?
					    sizeof(junk) : len);
					if ((n = read(s, junk, n)) > 0)
						len -= n;
					else
						break;
				}
			}
#else /* _MINIX */
			int truncated = 0;
			int nbytes;

			/*
			 * Use virtual circuit;
			 * at most one attempt per server.
			 */
			try = _res.retry;
			if (s < 0) 
			{
				s= tcp_connect(_res.nsaddr_list[ns],
					_res.nsport_list[ns], &terrno);
				if (s == -1)
					continue;
			}
			/*
			 * Send length & message
			 */
			len = htons((u_short)buflen);
			nbytes= tcpip_writeall(s, (char *)&len, 
				sizeof(len));
			if (nbytes != sizeof(len))
			{
				terrno= errno;
#ifdef DEBUG
				if (_res.options & RES_DEBUG)
					fprintf(stderr, "write failed: %s\n",
					strerror(terrno));
#endif /* DEBUG */
				close(s);
				s= -1;
				continue;
			}
			nbytes= tcpip_writeall(s, buf, buflen);
			if (nbytes != buflen)
			{
				terrno= errno;
#ifdef DEBUG
				if (_res.options & RES_DEBUG)
					fprintf(stderr, "write failed: %s\n",
					strerror(terrno));
#endif /* DEBUG */
				close(s);
				s= -1;
				continue;
			}
			/*
			 * Receive length & response
			 */
			cp = answer;
			len = sizeof(short);
			while (len != 0)
			{
				n = read(s, (char *)cp, (int)len);
				if (n <= 0)
					break;
				cp += n;
				assert(len >= n);
				len -= n;
			}
			if (len) {
				terrno = errno;
#ifdef DEBUG
				if (_res.options & RES_DEBUG)
					fprintf(stderr, "read failed: %s\n",
						strerror(terrno));
#endif /* DEBUG */
				close(s);
				s= -1;
				/*
				 * A long running process might get its TCP
				 * connection reset if the remote server was
				 * restarted.  Requery the server instead of
				 * trying a new one.  When there is only one
				 * server, this means that a query might work
				 * instead of failing.  We only allow one reset
				 * per query to prevent looping.
				 */
				if (terrno == ECONNRESET && !connreset) {
					connreset = 1;
					ns--;
				}
				continue;
			}
			cp = answer;
			if ((resplen = ntohs(*(u_short *)cp)) > anslen) {
#ifdef DEBUG
				if (_res.options & RES_DEBUG)
					fprintf(stderr, "response truncated\n");
#endif /* DEBUG */
				len = anslen;
				truncated = 1;
			} else
				len = resplen;
			while (len != 0)
			{
				n= read(s, (char *)cp, (int)len);
				if (n <= 0)
					break;
				cp += n;
				assert(len >= n);
				len -= n;
			}
			if (len) {
				terrno = errno;
#ifdef DEBUG
				if (_res.options & RES_DEBUG)
					fprintf(stderr, "read failed: %s\n",
						strerror(terrno));
#endif /* DEBUG */
				close(s);
				s= -1;
				continue;
			}
			if (truncated) {
				/*
				 * Flush rest of answer
				 * so connection stays in synch.
				 */
				anhp->dh_flag1 |= DHF_TC;
				len = resplen - anslen;
				while (len != 0) {
					n = (len > sizeof(junk) ?
					    sizeof(junk) : len);
					n = read(s, junk, n);
					if (n <= 0)
					{
						assert(len >= n);
						len -= n;
					}
					else
						break;
				}
			}
#endif /* _MINIX */
		} else {
#if !_MINIX
			/*
			 * Use datagrams.
			 */
			if (s < 0) {
				s = socket(AF_INET, SOCK_DGRAM, 0);
				if (s < 0) {
					terrno = errno;
#ifdef DEBUG
					if (_res.options & RES_DEBUG)
					    perror("socket (dg) failed");
#endif /* DEBUG */
					continue;
				}
			}
#if	BSD >= 43
			/*
			 * I'm tired of answering this question, so:
			 * On a 4.3BSD+ machine (client and server,
			 * actually), sending to a nameserver datagram
			 * port with no nameserver will cause an
			 * ICMP port unreachable message to be returned.
			 * If our datagram socket is "connected" to the
			 * server, we get an ECONNREFUSED error on the next
			 * socket operation, and select returns if the
			 * error message is received.  We can thus detect
			 * the absence of a nameserver without timing out.
			 * If we have sent queries to at least two servers,
			 * however, we don't want to remain connected,
			 * as we wish to receive answers from the first
			 * server to respond.
			 */
			if (_res.nscount == 1 || (try == 0 && ns == 0)) {
				/*
				 * Don't use connect if we might
				 * still receive a response
				 * from another server.
				 */
				if (connected == 0) {
			if (connect(s, (struct sockaddr *)&_res.nsaddr_list[ns],
					    sizeof(struct sockaddr)) < 0) {
#ifdef DEBUG
						if (_res.options & RES_DEBUG)
							perror("connect");
#endif /* DEBUG */
						continue;
					}
					connected = 1;
				}
				if (send(s, buf, buflen, 0) != buflen) {
#ifdef DEBUG
					if (_res.options & RES_DEBUG)
						perror("send");
#endif /* DEBUG */
					continue;
				}
			} else {
				/*
				 * Disconnect if we want to listen
				 * for responses from more than one server.
				 */
				if (connected) {
					(void) connect(s, &no_addr,
					    sizeof(no_addr));
					connected = 0;
				}
#endif /* BSD */
				if (sendto(s, buf, buflen, 0,
				    (struct sockaddr *)&_res.nsaddr_list[ns],
				    sizeof(struct sockaddr)) != buflen) {
#ifdef DEBUG
					if (_res.options & RES_DEBUG)
						perror("sendto");
#endif /* DEBUG */
					continue;
				}
#if	BSD >= 43
			}
#endif /* BSD */

			/*
			 * Wait for reply
			 */
			timeout.tv_sec = (_res.retrans << try);
			if (try > 0)
				timeout.tv_sec /= _res.nscount;
			if (timeout.tv_sec <= 0)
				timeout.tv_sec = 1;
			timeout.tv_usec = 0;
wait:
			FD_ZERO(&dsmask);
			FD_SET(s, &dsmask);
			n = select(s+1, &dsmask, (fd_set *)NULL,
				(fd_set *)NULL, &timeout);
			if (n < 0) {
#ifdef DEBUG
				if (_res.options & RES_DEBUG)
					perror("select");
#endif /* DEBUG */
				continue;
			}
			if (n == 0) {
				/*
				 * timeout
				 */
#ifdef DEBUG
				if (_res.options & RES_DEBUG)
					printf("timeout\n");
#endif /* DEBUG */
#if BSD >= 43
				gotsomewhere = 1;
#endif
				continue;
			}
			if ((resplen = recv(s, answer, anslen, 0)) <= 0) {
#ifdef DEBUG
				if (_res.options & RES_DEBUG)
					perror("recvfrom");
#endif /* DEBUG */
				continue;
			}
			gotsomewhere = 1;
			if (id != anhp->id) {
				/*
				 * response from old query, ignore it
				 */
#ifdef DEBUG
				if (_res.options & RES_DEBUG) {
					printf("old answer:\n");
					__p_query(answer);
				}
#endif /* DEBUG */
				goto wait;
			}
			if (!(_res.options & RES_IGNTC) && anhp->tc) {
				/*
				 * get rest of answer;
				 * use TCP with same server.
				 */
#ifdef DEBUG
				if (_res.options & RES_DEBUG)
					printf("truncated answer\n");
#endif /* DEBUG */
				(void) close(s);
				s = -1;
				v_circuit = 1;
				goto usevc;
			}
#else /* _MINIX */
			/*
			 * Use datagrams.
			 */
			if (s < 0) {
				s = udp_connect();
				if (s < 0) {
					terrno = errno;
#ifdef DEBUG
					if (_res.options & RES_DEBUG)
					    perror("udp_connect failed");
#endif /* DEBUG */
					continue;
				}
			}
			if (udp_sendto(s, buf, buflen, _res.nsaddr_list[ns],
				_res.nsport_list[ns]) != buflen) {
#ifdef DEBUG
				if (_res.options & RES_DEBUG)
					perror("sendto");
#endif /* DEBUG */
				continue;
			}

			/*
			 * Wait for reply
			 */
			timeout= (_res.retrans << try);
			if (try > 0)
				timeout /= _res.nscount;
			if (timeout <= 0)
				timeout= 1;
wait:
			if ((resplen= udp_receive(s, answer, anslen, timeout))
				== -1)
			{
				if (errno == EINTR)
				{
				/*
				 * timeout
				 */
#ifdef DEBUG
					if (_res.options & RES_DEBUG)
						printf("timeout\n");
#endif /* DEBUG */
					gotsomewhere = 1;
				}
				else
				{
#ifdef DEBUG
				if (_res.options & RES_DEBUG)
					perror("udp_receive");
#endif /* DEBUG */
				}
				continue;
			}
			gotsomewhere = 1;
			if (id != anhp->dh_id) {
				/*
				 * response from old query, ignore it
				 */
#ifdef DEBUG
				if (_res.options & RES_DEBUG) {
					printf("old answer:\n");
					__p_query(answer);
				}
#endif /* DEBUG */
				goto wait;
			}
			if (!(_res.options & RES_IGNTC) &&
				(anhp->dh_flag1 & DHF_TC)) {
				/*
				 * get rest of answer;
				 * use TCP with same server.
				 */
#ifdef DEBUG
				if (_res.options & RES_DEBUG)
					printf("truncated answer\n");
#endif /* DEBUG */
				(void) close(s);
				s = -1;
				v_circuit = 1;
				goto usevc;
			}
#endif /* !_MINIX */
		}
#ifdef DEBUG
		if (_res.options & RES_DEBUG) {
			printf("got answer:\n");
			__p_query(answer);
		}
#endif /* DEBUG */
		/*
		 * If using virtual circuits, we assume that the first server
		 * is preferred * over the rest (i.e. it is on the local
		 * machine) and only keep that one open.
		 * If we have temporarily opened a virtual circuit,
		 * or if we haven't been asked to keep a socket open,
		 * close the socket.
		 */
		if ((v_circuit &&
		    ((_res.options & RES_USEVC) == 0 || ns != 0)) ||
		    (_res.options & RES_STAYOPEN) == 0) {
			(void) close(s);
			s = -1;
		}
		return (resplen);
	   }
	}
	if (s >= 0) {
		(void) close(s);
		s = -1;
	}
	if (v_circuit == 0)
		if (gotsomewhere == 0)
			errno = ECONNREFUSED;	/* no nameservers found */
		else
			errno = ETIMEDOUT;	/* no answer obtained */
	else
		errno = terrno;
	return (-1);
}

/*
 * This routine is for closing the socket if a virtual circuit is used and
 * the program wants to close it.  This provides support for endhostent()
 * which expects to close the socket.
 *
 * This routine is not expected to be user visible.
 */
void
_res_close()
{
	if (s != -1) {
		(void) close(s);
		s = -1;
	}
}

#if _MINIX
static int tcp_connect(host, port, terrno)
ipaddr_t host;
tcpport_t port;
int *terrno;
{
	char *dev_name;
	int fd;
	int error;
	nwio_tcpconf_t tcpconf;
	nwio_tcpcl_t clopt;

	dev_name= getenv("TCP_DEVICE");
	if (!dev_name)
		dev_name= TCP_DEVICE;
	fd= open(dev_name, O_RDWR);
	if (fd == -1)
	{
		*terrno= errno;
		return -1;
	}
	tcpconf.nwtc_flags= NWTC_EXCL | NWTC_LP_SEL | NWTC_SET_RA | NWTC_SET_RP;
	tcpconf.nwtc_remaddr= host;
	tcpconf.nwtc_remport= port;
	error= ioctl(fd, NWIOSTCPCONF, &tcpconf);
	if (error == -1)
	{
		*terrno= errno;
		close(fd);
		return -1;
	}
	clopt.nwtcl_flags= 0;
	error= ioctl(fd, NWIOTCPCONN, &clopt);
	if (error == -1)
	{
		*terrno= errno;
		close(fd);
		return -1;
	}
	*terrno= 0;
	return fd;
}

static int tcpip_writeall(fd, buf, siz)
int fd;
const char *buf;
size_t siz;
{
	size_t siz_org;
	int nbytes;

	siz_org= siz;

	while (siz)
	{
		nbytes= write(fd, buf, siz);
		if (nbytes <= 0)
			return siz_org-siz;
		assert(siz >= nbytes);
		buf += nbytes;
		siz -= nbytes;
	}
	return siz_org;
}


static int udp_connect()
{
	nwio_udpopt_t udpopt;
	char *dev_name;
	int fd, r, terrno;

	dev_name= getenv("UDP_DEVICE");
	if (!dev_name)
		dev_name= UDP_DEVICE;
	fd= open(dev_name, O_RDWR);
	if (fd == -1)
		return -1;

	udpopt.nwuo_flags= NWUO_COPY | NWUO_LP_SEL | NWUO_EN_LOC |
		NWUO_EN_BROAD | NWUO_RP_ANY | NWUO_RA_ANY | NWUO_RWDATALL |
		NWUO_DI_IPOPT;
	r= ioctl(fd, NWIOSUDPOPT, &udpopt);
	if (r == -1)
	{
		terrno= errno;
		close(fd);
		errno= terrno;
		return -1;
	}
	return fd;
}

static int udp_sendto(fd, buf, buflen, addr, port)
int fd;
const char *buf;
unsigned buflen;
ipaddr_t addr;
udpport_t port;
{
	char *newbuf;
	udp_io_hdr_t *udp_io_hdr;
	int r, terrno;

	newbuf= malloc(sizeof(*udp_io_hdr) + buflen);
	if (newbuf == NULL)
	{
		errno= ENOMEM;
		return -1;
	}
	udp_io_hdr= (udp_io_hdr_t *)newbuf;
	udp_io_hdr->uih_dst_addr= addr;
	udp_io_hdr->uih_dst_port= port;
	udp_io_hdr->uih_ip_opt_len= 0;
	udp_io_hdr->uih_data_len= buflen;

	memcpy(newbuf + sizeof(*udp_io_hdr), buf, buflen);
	r= write(fd, newbuf, sizeof(*udp_io_hdr) + buflen);
	terrno= errno;
	free(newbuf);
	if (r >= sizeof(*udp_io_hdr))
		r -= sizeof(*udp_io_hdr);
	errno= terrno;
	return r;
}

static void alarm_handler(sig)
int sig;
{
	signal(SIGALRM, alarm_handler);
	alarm(1);
}

static int udp_receive(fd, buf, buflen, timeout)
int fd;
char *buf;
unsigned buflen;
time_t timeout;
{
	char *newbuf;
	udp_io_hdr_t *udp_io_hdr;
	int r, terrno;
	void (*u_handler) _ARGS(( int sig ));
	time_t u_timeout;

	newbuf= malloc(sizeof(*udp_io_hdr) + buflen);
	if (newbuf == NULL)
	{
		errno= ENOMEM;
		return -1;
	}

	u_handler= signal(SIGALRM, alarm_handler);
	u_timeout= alarm(timeout);

	r= read(fd, newbuf, sizeof(*udp_io_hdr) + buflen);
	terrno= errno;

	if (r < 0 || r <= sizeof(*udp_io_hdr))
	{
		if (r > 0)
			r= 0;
		free(newbuf);


		alarm(0);
		signal(SIGALRM, u_handler);
		alarm(u_timeout);

		errno= terrno;
		return r;
	}

	memcpy(buf, newbuf + sizeof(*udp_io_hdr), r - sizeof(*udp_io_hdr));
	free(newbuf);

	alarm(0);
	signal(SIGALRM, u_handler);
	alarm(u_timeout);

	return r-sizeof(*udp_io_hdr);
}

#endif
