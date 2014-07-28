/*
 * TNET		A server program for MINIX which implements the TCP/IP
 *		suite of networking protocols.  It is based on the
 *		TCP/IP code written by Phil Karn et al, as found in
 *		his NET package for Packet Radio communications.
 *
 *		Definitions for the TELNET server.
 *
 * Author:	Fred N. van Kempen, <waltje@uwalt.nl.mugnet.org>
 */

extern int opt_d;			/* debugging flag		*/

int get_pty(int *, char **);
void term_init(void);
void term_inout(int pty_fd);
void tel_init(void);
void telopt(int fdout, int what, int option);
void tel_in(int fdout, int telout, char *buffer, int len);
void tel_out(int fdout, char *buf, int size);
