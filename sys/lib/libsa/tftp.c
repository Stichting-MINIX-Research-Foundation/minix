/*	$NetBSD: tftp.c,v 1.34 2011/12/25 06:09:08 tsutsui Exp $	 */

/*
 * Copyright (c) 1996
 *	Matthias Drochner.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/*
 * Simple TFTP implementation for libsa.
 * Assumes:
 *  - socket descriptor (int) at open_file->f_devdata
 *  - server host IP in global servip
 * Restrictions:
 *  - read only
 *  - lseek only with SEEK_SET or SEEK_CUR
 *  - no big time differences between transfers (<tftp timeout)
 */

/*
 * XXX Does not currently implement:
 * XXX
 * XXX LIBSA_NO_FS_CLOSE
 * XXX LIBSA_NO_FS_SEEK
 * XXX LIBSA_NO_FS_WRITE
 * XXX LIBSA_NO_FS_SYMLINK (does this even make sense?)
 * XXX LIBSA_FS_SINGLECOMPONENT (does this even make sense?)
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netinet/udp.h>
#include <netinet/in_systm.h>
#include <lib/libkern/libkern.h>

#include "stand.h"
#include "net.h"

#include "tftp.h"

extern struct in_addr servip;

static int      tftpport = 2000;

#define RSPACE 520		/* max data packet, rounded up */

struct tftp_handle {
	struct iodesc  *iodesc;
	int             currblock;	/* contents of lastdata */
	int             islastblock;	/* flag */
	int             validsize;
	int             off;
	const char     *path;	/* saved for re-requests */
	struct {
		u_char header[UDP_TOTAL_HEADER_SIZE];
		struct tftphdr t;
		u_char space[RSPACE];
	} lastdata;
};

static const int tftperrors[8] = {
	0,			/* ??? */
	ENOENT,
	EPERM,
	ENOSPC,
	EINVAL,			/* ??? */
	EINVAL,			/* ??? */
	EEXIST,
	EINVAL,			/* ??? */
};

static ssize_t recvtftp(struct iodesc *, void *, size_t, saseconds_t);
static int tftp_makereq(struct tftp_handle *);
static int tftp_getnextblock(struct tftp_handle *);
#ifndef TFTP_NOTERMINATE
static void tftp_terminate(struct tftp_handle *);
#endif
static ssize_t tftp_size_of_file(struct tftp_handle *tftpfile);

static ssize_t
recvtftp(struct iodesc *d, void *pkt, size_t len, saseconds_t tleft)
{
	ssize_t n;
	struct tftphdr *t;

	errno = 0;

	n = readudp(d, pkt, len, tleft);

	if (n < 4)
		return -1;

	t = (struct tftphdr *)pkt;
	switch (ntohs(t->th_opcode)) {
	case DATA:
		if (htons(t->th_block) != d->xid) {
			/*
			 * Expected block?
			 */
			return -1;
		}
		if (d->xid == 1) {
			/*
			 * First data packet from new port.
			 */
			struct udphdr *uh;
			uh = (struct udphdr *)pkt - 1;
			d->destport = uh->uh_sport;
		} /* else check uh_sport has not changed??? */
		return (n - (t->th_data - (char *)t));
	case ERROR:
		if ((unsigned int)ntohs(t->th_code) >= 8) {
			printf("illegal tftp error %d\n", ntohs(t->th_code));
			errno = EIO;
		} else {
#ifdef DEBUG
			printf("tftp-error %d\n", ntohs(t->th_code));
#endif
			errno = tftperrors[ntohs(t->th_code)];
		}
		return -1;
	default:
#ifdef DEBUG
		printf("tftp type %d not handled\n", ntohs(t->th_opcode));
#endif
		return -1;
	}
}

/* send request, expect first block (or error) */
static int
tftp_makereq(struct tftp_handle *h)
{
	struct {
		u_char header[UDP_TOTAL_HEADER_SIZE];
		struct tftphdr t;
		u_char space[FNAME_SIZE + 6];
	} wbuf;
	char           *wtail;
	int             l;
	ssize_t         res;
	struct tftphdr *t;

	wbuf.t.th_opcode = htons((u_short)RRQ);
	wtail = wbuf.t.th_stuff;
	l = strlen(h->path);
	(void)memcpy(wtail, h->path, l + 1);
	wtail += l + 1;
	(void)memcpy(wtail, "octet", 6);
	wtail += 6;

	t = &h->lastdata.t;

	/* h->iodesc->myport = htons(--tftpport); */
	h->iodesc->myport = htons(tftpport + (getsecs() & 0x3ff));
	h->iodesc->destport = htons(IPPORT_TFTP);
	h->iodesc->xid = 1;	/* expected block */

	res = sendrecv(h->iodesc, sendudp, &wbuf.t, wtail - (char *)&wbuf.t,
		       recvtftp, t, sizeof(*t) + RSPACE);

	if (res == -1)
		return errno;

	h->currblock = 1;
	h->validsize = res;
	h->islastblock = 0;
	if (res < SEGSIZE)
		h->islastblock = 1;	/* very short file */
	return 0;
}

/* ack block, expect next */
static int
tftp_getnextblock(struct tftp_handle *h)
{
	struct {
		u_char header[UDP_TOTAL_HEADER_SIZE];
		struct tftphdr t;
	} wbuf;
	char           *wtail;
	int             res;
	struct tftphdr *t;

	wbuf.t.th_opcode = htons((u_short)ACK);
	wbuf.t.th_block = htons((u_short)h->currblock);
	wtail = (char *)&wbuf.t.th_data;

	t = &h->lastdata.t;

	h->iodesc->xid = h->currblock + 1;	/* expected block */

	res = sendrecv(h->iodesc, sendudp, &wbuf.t, wtail - (char *)&wbuf.t,
		       recvtftp, t, sizeof(*t) + RSPACE);

	if (res == -1)		/* 0 is OK! */
		return errno;

	h->currblock++;
	h->validsize = res;
	if (res < SEGSIZE)
		h->islastblock = 1;	/* EOF */
	return 0;
}

#ifndef TFTP_NOTERMINATE
static void
tftp_terminate(struct tftp_handle *h)
{
	struct {
		u_char header[UDP_TOTAL_HEADER_SIZE];
		struct tftphdr t;
	} wbuf;
	char           *wtail;

	wtail = (char *)&wbuf.t.th_data;
	if (h->islastblock) {
		wbuf.t.th_opcode = htons((u_short)ACK);
		wbuf.t.th_block = htons((u_short)h->currblock);
	} else {
		wbuf.t.th_opcode = htons((u_short)ERROR);
		wbuf.t.th_code = htons((u_short)ENOSPACE); /* ??? */
		*wtail++ = '\0'; /* empty error string */
	}

	(void)sendudp(h->iodesc, &wbuf.t, wtail - (char *)&wbuf.t);
}
#endif

__compactcall int
tftp_open(const char *path, struct open_file *f)
{
	struct tftp_handle *tftpfile;
	struct iodesc  *io;
	int             res;

	tftpfile = (struct tftp_handle *)alloc(sizeof(*tftpfile));
	if (!tftpfile)
		return ENOMEM;

	tftpfile->iodesc = io = socktodesc(*(int *)(f->f_devdata));
	io->destip = servip;
	tftpfile->off = 0;
	tftpfile->path = path;	/* XXXXXXX we hope it's static */

	res = tftp_makereq(tftpfile);

	if (res) {
		dealloc(tftpfile, sizeof(*tftpfile));
		return res;
	}
	f->f_fsdata = (void *)tftpfile;
	fsmod = "nfs";
	return 0;
}

__compactcall int
tftp_read(struct open_file *f, void *addr, size_t size, size_t *resid)
{
	struct tftp_handle *tftpfile;
#if !defined(LIBSA_NO_TWIDDLE)
	static int      tc = 0;
#endif
	tftpfile = (struct tftp_handle *)f->f_fsdata;

	while (size > 0) {
		int needblock;
		size_t count;

#if !defined(LIBSA_NO_TWIDDLE)
		if (!(tc++ % 16))
			twiddle();
#endif

		needblock = tftpfile->off / SEGSIZE + 1;

		if (tftpfile->currblock > needblock) {	/* seek backwards */
#ifndef TFTP_NOTERMINATE
			tftp_terminate(tftpfile);
#endif
			tftp_makereq(tftpfile);	/* no error check, it worked
			                         * for open */
		}

		while (tftpfile->currblock < needblock) {
			int res;

			res = tftp_getnextblock(tftpfile);
			if (res) {	/* no answer */
#ifdef DEBUG
				printf("tftp: read error (block %d->%d)\n",
				       tftpfile->currblock, needblock);
#endif
				return res;
			}
			if (tftpfile->islastblock)
				break;
		}

		if (tftpfile->currblock == needblock) {
			size_t offinblock, inbuffer;

			offinblock = tftpfile->off % SEGSIZE;

			if (offinblock > tftpfile->validsize) {
#ifdef DEBUG
				printf("tftp: invalid offset %d\n",
				    tftpfile->off);
#endif
				return EINVAL;
			}
			inbuffer = tftpfile->validsize - offinblock;
			count = (size < inbuffer ? size : inbuffer);
			(void)memcpy(addr,
			    tftpfile->lastdata.t.th_data + offinblock,
			    count);

			addr = (char *)addr + count;
			tftpfile->off += count;
			size -= count;

			if ((tftpfile->islastblock) && (count == inbuffer))
				break;	/* EOF */
		} else {
#ifdef DEBUG
			printf("tftp: block %d not found\n", needblock);
#endif
			return EINVAL;
		}

	}

	if (resid)
		*resid = size;
	return 0;
}

__compactcall int
tftp_close(struct open_file *f)
{
	struct tftp_handle *tftpfile;
	tftpfile = (struct tftp_handle *)f->f_fsdata;

#ifdef TFTP_NOTERMINATE
	/* let it time out ... */
#else
	tftp_terminate(tftpfile);
#endif

	dealloc(tftpfile, sizeof(*tftpfile));
	return 0;
}

__compactcall int
tftp_write(struct open_file *f, void *start, size_t size, size_t *resid)
{

	return EROFS;
}

static ssize_t 
tftp_size_of_file(struct tftp_handle *tftpfile)
{
	ssize_t filesize;

	if (tftpfile->currblock > 1) {	/* move to start of file */
#ifndef TFTP_NOTERMINATE
		tftp_terminate(tftpfile);
#endif
		tftp_makereq(tftpfile);	/* no error check, it worked
		      			 * for open */
	}

	/* start with the size of block 1 */
	filesize = tftpfile->validsize;

	/* and keep adding the sizes till we hit the last block */
	while (!tftpfile->islastblock) {
		int res;

		res = tftp_getnextblock(tftpfile);
		if (res) {	/* no answer */
#ifdef DEBUG
			printf("tftp: read error (block %d)\n",
					tftpfile->currblock);
#endif
			return -1;
		}
		filesize += tftpfile->validsize;
	}
#ifdef DEBUG
	printf("tftp_size_of_file: file is %zu bytes\n", filesize);
#endif
	return filesize;
}

__compactcall int
tftp_stat(struct open_file *f, struct stat *sb)
{
	struct tftp_handle *tftpfile;
	tftpfile = (struct tftp_handle *)f->f_fsdata;

	sb->st_mode = 0444;
	sb->st_nlink = 1;
	sb->st_uid = 0;
	sb->st_gid = 0;
	sb->st_size = tftp_size_of_file(tftpfile);
	return 0;
}

#if defined(LIBSA_ENABLE_LS_OP)
__compactcall void
tftp_ls(struct open_file *f, const char *pattern,
		void (*funcp)(char* arg), char* path)
{
	printf("Currently ls command is unsupported by tftp\n");
	return;
}
#endif

__compactcall off_t
tftp_seek(struct open_file *f, off_t offset, int where)
{
	struct tftp_handle *tftpfile;
	tftpfile = (struct tftp_handle *)f->f_fsdata;

	switch (where) {
	case SEEK_SET:
		tftpfile->off = offset;
		break;
	case SEEK_CUR:
		tftpfile->off += offset;
		break;
	default:
		errno = EOFFSET;
		return -1;
	}
	return tftpfile->off;
}
