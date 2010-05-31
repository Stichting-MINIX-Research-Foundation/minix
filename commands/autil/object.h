/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */

#include <ansi.h>

#ifndef __OBJECT_INCLUDED__
#define __OBJECT_INCLUDED__

_PROTOTYPE(int wr_open, (char *f));
_PROTOTYPE(void wr_close, (void));
_PROTOTYPE(void wr_ohead, (struct outhead *h));
_PROTOTYPE(void wr_sect, (struct outsect *s, unsigned int c));
_PROTOTYPE(void wr_outsect, (int sectno));
_PROTOTYPE(void wr_emit, (char *b, long c));
_PROTOTYPE(void wr_putc, (int c));
_PROTOTYPE(void wr_relo, (struct outrelo *r, unsigned int c));
_PROTOTYPE(void wr_name, (struct outname *n, unsigned int c));
_PROTOTYPE(void wr_string, (char *s, long c));
_PROTOTYPE(void wr_arhdr, (int fd, struct ar_hdr *a));
_PROTOTYPE(void wr_ranlib, (int fd, struct ranlib *r, long cnt));
_PROTOTYPE(void wr_int2, (int fd, int i));
_PROTOTYPE(void wr_long, (int fd, long l));
_PROTOTYPE(void wr_bytes, (int fd, char *buf, long l));
_PROTOTYPE(int rd_open, (char *f));
_PROTOTYPE(int rd_fdopen, (int f));
_PROTOTYPE(void rd_close, (void));
_PROTOTYPE(void rd_ohead, (struct outhead *h));
_PROTOTYPE(void rd_sect, (struct outsect *s, unsigned int c));
_PROTOTYPE(void rd_outsect, (int sectno));
_PROTOTYPE(void rd_emit, (char *b, long c));
_PROTOTYPE(void rd_relo, (struct outrelo *r, unsigned int c));
_PROTOTYPE(void rd_rew_relo, (struct outhead *head));
_PROTOTYPE(void rd_name, (struct outname *n, unsigned int c));
_PROTOTYPE(void rd_string, (char *s, long c));
_PROTOTYPE(int rd_arhdr, (int fd, struct ar_hdr *a));
_PROTOTYPE(void rd_ranlib, (int fd, struct ranlib *r, long cnt));
_PROTOTYPE(int rd_int2, (int fd));
_PROTOTYPE(long rd_long, (int fd));
_PROTOTYPE(void rd_bytes, (int fd, char *buf, long l));
_PROTOTYPE(int rd_fd, (void));

#endif /* __OBJECT_INCLUDED__ */
