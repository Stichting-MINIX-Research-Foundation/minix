/*      $NetBSD: psbuf.c,v 1.19 2012/11/04 22:46:08 christos Exp $        */

/*
 * Copyright (c) 2006-2009  Antti Kantee.  All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#ifndef lint
__RCSID("$NetBSD: psbuf.c,v 1.19 2012/11/04 22:46:08 christos Exp $");
#endif /* !lint */

/*
 * buffering functions for network input/output.  slightly different
 * from the average joe buffer routines, as is usually the case ...
 * these use efuns for now.
 */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/vnode.h>
#include <sys/socket.h>

#include <err.h>
#include <errno.h>
#include <stdlib.h>
#include <util.h>
#include <unistd.h>

#include "psshfs.h"
#include "sftp_proto.h"

#define FAILRV(x) do { int rv; if ((rv=x)) return (rv); } while (/*CONSTCOND*/0)
#define READSTATE_LENGTH(off) (off < 4)

#define SFTP_LENOFF	0
#define SFTP_TYPEOFF	4
#define SFTP_REQIDOFF	5

#define CHECK(v) if (!(v)) abort()

uint8_t
psbuf_get_type(struct puffs_framebuf *pb)
{
	uint8_t type;

	puffs_framebuf_getdata_atoff(pb, SFTP_TYPEOFF, &type, 1);
	return type;
}

uint32_t
psbuf_get_len(struct puffs_framebuf *pb)
{
	uint32_t len;

	puffs_framebuf_getdata_atoff(pb, SFTP_LENOFF, &len, 4);
	return be32toh(len);
}

uint32_t
psbuf_get_reqid(struct puffs_framebuf *pb)
{
	uint32_t req;

	puffs_framebuf_getdata_atoff(pb, SFTP_REQIDOFF, &req, 4);
	return be32toh(req);
}

#define CUROFF(pb) (puffs_framebuf_telloff(pb))
int
psbuf_read(struct puffs_usermount *pu, struct puffs_framebuf *pb,
	int fd, int *done)
{
	void *win;
	ssize_t n;
	size_t howmuch, winlen;
	int lenstate;

 the_next_level:
	if ((lenstate = READSTATE_LENGTH(CUROFF(pb))))
		howmuch = 4 - CUROFF(pb);
	else
		howmuch = psbuf_get_len(pb) - (CUROFF(pb) - 4);

	if (puffs_framebuf_reserve_space(pb, howmuch) == -1)
		return errno;

	while (howmuch) {
		winlen = howmuch;
		if (puffs_framebuf_getwindow(pb, CUROFF(pb), &win, &winlen)==-1)
			return errno;
		n = recv(fd, win, winlen, MSG_NOSIGNAL);
		switch (n) {
		case 0:
			return ECONNRESET;
		case -1:
			if (errno == EAGAIN)
				return 0;
			return errno;
		default:
			howmuch -= n;
			puffs_framebuf_seekset(pb, CUROFF(pb) + n);
			break;
		}
	}

	if (!lenstate) {
		/* XXX: initial exchange shorter.. but don't worry, be happy */
		puffs_framebuf_seekset(pb, 9);
		*done = 1;
		return 0;
	} else
		goto the_next_level;
}

int
psbuf_write(struct puffs_usermount *pu, struct puffs_framebuf *pb,
	int fd, int *done)
{
	void *win;
	ssize_t n;
	size_t winlen, howmuch;
	
	/* finalize buffer.. could be elsewhere ... */
	if (CUROFF(pb) == 0) {
		uint32_t len;

		len = htobe32(puffs_framebuf_tellsize(pb) - 4);
		puffs_framebuf_putdata_atoff(pb, 0, &len, 4);
	}

	howmuch = puffs_framebuf_tellsize(pb) - CUROFF(pb);
	while (howmuch) {
		winlen = howmuch;
		if (puffs_framebuf_getwindow(pb, CUROFF(pb), &win, &winlen)==-1)
			return errno;
		n = send(fd, win, winlen, MSG_NOSIGNAL);
		switch (n) {
		case 0:
			return ECONNRESET;
		case -1:
			if (errno == EAGAIN)
				return 0;
			return errno;
		default:
			howmuch -= n;
			puffs_framebuf_seekset(pb, CUROFF(pb) + n);
			break;
		}
	}

	*done = 1;
	return 0;
}
#undef CUROFF

int
psbuf_cmp(struct puffs_usermount *pu,
	struct puffs_framebuf *cmp1, struct puffs_framebuf *cmp2, int *notresp)
{

	return psbuf_get_reqid(cmp1) != psbuf_get_reqid(cmp2);
}

struct puffs_framebuf *
psbuf_makeout()
{
	struct puffs_framebuf *pb;

	pb = puffs_framebuf_make();
	puffs_framebuf_seekset(pb, 4);
	return pb;
}

void
psbuf_recycleout(struct puffs_framebuf *pb)
{

	puffs_framebuf_recycle(pb);
	puffs_framebuf_seekset(pb, 4);
}

void
psbuf_put_1(struct puffs_framebuf *pb, uint8_t val)
{
	int rv;

	rv = puffs_framebuf_putdata(pb, &val, 1);
	CHECK(rv == 0);
}

void
psbuf_put_2(struct puffs_framebuf *pb, uint16_t val)
{
	int rv;

	HTOBE16(val);
	rv = puffs_framebuf_putdata(pb, &val, 2);
	CHECK(rv == 0);
}

void
psbuf_put_4(struct puffs_framebuf *pb, uint32_t val)
{
	int rv;

	HTOBE32(val);
	rv = puffs_framebuf_putdata(pb, &val, 4);
	CHECK(rv == 0);
}

void
psbuf_put_8(struct puffs_framebuf *pb, uint64_t val)
{
	int rv;

	HTOBE64(val);
	rv = puffs_framebuf_putdata(pb, &val, 8);
	CHECK(rv == 0);
}

void
psbuf_put_data(struct puffs_framebuf *pb, const void *data, uint32_t dlen)
{
	int rv;

	psbuf_put_4(pb, dlen);
	rv = puffs_framebuf_putdata(pb, data, dlen);
	CHECK(rv == 0);
}

void
psbuf_put_str(struct puffs_framebuf *pb, const char *str)
{

	psbuf_put_data(pb, str, strlen(str));
}

void
psbuf_put_vattr(struct puffs_framebuf *pb, const struct vattr *va,
	const struct psshfs_ctx *pctx)
{
	uint32_t flags;
	uint32_t theuid = -1, thegid = -1;
	flags = 0;

	if (va->va_size != (uint64_t)PUFFS_VNOVAL)
		flags |= SSH_FILEXFER_ATTR_SIZE;
	if (va->va_uid != (uid_t)PUFFS_VNOVAL) {
		theuid = va->va_uid;
		if (pctx->domangleuid && theuid == pctx->myuid)
			theuid = pctx->mangleuid;
		flags |= SSH_FILEXFER_ATTR_UIDGID;
	}
	if (va->va_gid != (gid_t)PUFFS_VNOVAL) {
		thegid = va->va_gid;
		if (pctx->domanglegid && thegid == pctx->mygid)
			thegid = pctx->manglegid;
		flags |= SSH_FILEXFER_ATTR_UIDGID;
	}
	if (va->va_mode != (mode_t)PUFFS_VNOVAL)
		flags |= SSH_FILEXFER_ATTR_PERMISSIONS;

	if (va->va_atime.tv_sec != PUFFS_VNOVAL)
		flags |= SSH_FILEXFER_ATTR_ACCESSTIME;

	psbuf_put_4(pb, flags);
	if (flags & SSH_FILEXFER_ATTR_SIZE)
		psbuf_put_8(pb, va->va_size);
	if (flags & SSH_FILEXFER_ATTR_UIDGID) {
		psbuf_put_4(pb, theuid);
		psbuf_put_4(pb, thegid);
	}
	if (flags & SSH_FILEXFER_ATTR_PERMISSIONS)
		psbuf_put_4(pb, va->va_mode);

	/* XXX: this is totally wrong for protocol v3, see OpenSSH */
	if (flags & SSH_FILEXFER_ATTR_ACCESSTIME) {
		psbuf_put_4(pb, va->va_atime.tv_sec);
		psbuf_put_4(pb, va->va_mtime.tv_sec);
	}
}

#define ERETURN(rv) return ((rv) == -1 ? errno : 0)

int
psbuf_get_1(struct puffs_framebuf *pb, uint8_t *val)
{

	ERETURN(puffs_framebuf_getdata(pb, val, 1));
}

int
psbuf_get_2(struct puffs_framebuf *pb, uint16_t *val)
{
	int rv;

	rv = puffs_framebuf_getdata(pb, val, 2);
	BE16TOH(*val);

	ERETURN(rv);
}

int
psbuf_get_4(struct puffs_framebuf *pb, uint32_t *val)
{
	int rv;

	rv = puffs_framebuf_getdata(pb, val, 4);
	BE32TOH(*val);

	ERETURN(rv);
}

int
psbuf_get_8(struct puffs_framebuf *pb, uint64_t *val)
{
	int rv;

	rv = puffs_framebuf_getdata(pb, val, 8);
	BE64TOH(*val);

	ERETURN(rv);
}

int
psbuf_get_str(struct puffs_framebuf *pb, char **strp, uint32_t *strlenp)
{
	char *str;
	uint32_t len;

	FAILRV(psbuf_get_4(pb, &len));

	if (puffs_framebuf_remaining(pb) < len)
		return EPROTO;

	str = emalloc(len+1);
	puffs_framebuf_getdata(pb, str, len);
	str[len] = '\0';
	*strp = str;

	if (strlenp)
		*strlenp = len;

	return 0;
}

int
psbuf_get_vattr(struct puffs_framebuf *pb, struct vattr *vap)
{
	uint32_t flags;
	uint32_t val;

	puffs_vattr_null(vap);

	FAILRV(psbuf_get_4(pb, &flags));

	if (flags & SSH_FILEXFER_ATTR_SIZE) {
		FAILRV(psbuf_get_8(pb, &vap->va_size));
		vap->va_bytes = vap->va_size;
	}
	if (flags & SSH_FILEXFER_ATTR_UIDGID) {
		FAILRV(psbuf_get_4(pb, &vap->va_uid));
		FAILRV(psbuf_get_4(pb, &vap->va_gid));
	}
	if (flags & SSH_FILEXFER_ATTR_PERMISSIONS) {
		FAILRV(psbuf_get_4(pb, &vap->va_mode));
		vap->va_type = puffs_mode2vt(vap->va_mode);
	}
	if (flags & SSH_FILEXFER_ATTR_ACCESSTIME) {
		/*
		 * XXX: this is utterly wrong if we want to speak
		 * protocol version 3, but it seems like the
		 * "internet standard" for doing this
		 */
		FAILRV(psbuf_get_4(pb, &val));
		vap->va_atime.tv_sec = val;
		FAILRV(psbuf_get_4(pb, &val));
		vap->va_mtime.tv_sec = val;
		/* make ctime the same as mtime */
		vap->va_ctime.tv_sec = val;

		vap->va_atime.tv_nsec = 0;
		vap->va_ctime.tv_nsec = 0;
		vap->va_mtime.tv_nsec = 0;
	}

	return 0;
}

/*
 * Buffer content helpers.  Caller frees all data.
 */

/*
 * error mapping.. most are not expected for a file system, but
 * should help with diagnosing a possible error
 */
static int emap[] = {
	0,			/* OK			*/
	0,			/* EOF			*/
	ENOENT,			/* NO_SUCH_FILE		*/
	EPERM,			/* PERMISSION_DENIED	*/
	EIO,			/* FAILURE		*/
	EBADMSG,		/* BAD_MESSAGE		*/
	ENOTCONN,		/* NO_CONNECTION	*/
	ECONNRESET,		/* CONNECTION_LOST	*/
	EOPNOTSUPP,		/* OP_UNSUPPORTED	*/
	EINVAL,			/* INVALID_HANDLE	*/
	ENXIO,			/* NO_SUCH_PATH		*/
	EEXIST,			/* FILE_ALREADY_EXISTS	*/
	ENODEV			/* WRITE_PROTECT	*/
};
#define NERRORS ((int)(sizeof(emap) / sizeof(emap[0])))

static int
sftperr_to_errno(int error)
{

	if (!error)
		return 0;

	if (error >= NERRORS || error < 0)
		return EPROTO;

	return emap[error];
}

#define INVALRESPONSE EPROTO

static int
expectcode(struct puffs_framebuf *pb, int value)
{
	uint32_t error;
	uint8_t type;

	type = psbuf_get_type(pb);
	if (type == value)
		return 0;

	if (type != SSH_FXP_STATUS)
		return INVALRESPONSE;

	FAILRV(psbuf_get_4(pb, &error));

	return sftperr_to_errno(error);
}

#define CHECKCODE(pb,val)						\
do {									\
	int rv;								\
	rv = expectcode(pb, val);					\
	if (rv)								\
		return rv;						\
} while (/*CONSTCOND*/0)

int
psbuf_expect_status(struct puffs_framebuf *pb)
{
	uint32_t error;

	if (psbuf_get_type(pb) != SSH_FXP_STATUS)
		return INVALRESPONSE;

	FAILRV(psbuf_get_4(pb, &error));
	
	return sftperr_to_errno(error);
}

int
psbuf_expect_handle(struct puffs_framebuf *pb, char **hand, uint32_t *handlen)
{

	CHECKCODE(pb, SSH_FXP_HANDLE);
	FAILRV(psbuf_get_str(pb, hand, handlen));

	return 0;
}

/* no memory allocation, direct copy */
int
psbuf_do_data(struct puffs_framebuf *pb, uint8_t *data, uint32_t *dlen)
{
	void *win;
	size_t bufoff, winlen;
	uint32_t len, dataoff;

	if (psbuf_get_type(pb) != SSH_FXP_DATA) {
		uint32_t val;

		if (psbuf_get_type(pb) != SSH_FXP_STATUS)
			return INVALRESPONSE;

		if (psbuf_get_4(pb, &val) != 0)
			return INVALRESPONSE;

		if (val != SSH_FX_EOF)
			return sftperr_to_errno(val);

		*dlen = 0;
		return 0;
	}
	if (psbuf_get_4(pb, &len) != 0)
		return INVALRESPONSE;

	if (*dlen < len)
		return EINVAL;

	*dlen = 0;

	dataoff = 0;
	while (dataoff < len) {
		winlen = len-dataoff;
		bufoff = puffs_framebuf_telloff(pb);
		if (puffs_framebuf_getwindow(pb, bufoff,
		    &win, &winlen) == -1)
			return EINVAL;
		if (winlen == 0)
			break;
			
		memcpy(data + dataoff, win, winlen);
		dataoff += winlen;
	}

	*dlen = dataoff;

	return 0;
}

int
psbuf_expect_name(struct puffs_framebuf *pb, uint32_t *count)
{

	CHECKCODE(pb, SSH_FXP_NAME);
	FAILRV(psbuf_get_4(pb, count));

	return 0;
}

int
psbuf_expect_attrs(struct puffs_framebuf *pb, struct vattr *vap)
{

	CHECKCODE(pb, SSH_FXP_ATTRS);
	FAILRV(psbuf_get_vattr(pb, vap));

	return 0;
}

/*
 * More helpers: larger-scale put functions
 */

void
psbuf_req_data(struct puffs_framebuf *pb, int type, uint32_t reqid,
	const void *data, uint32_t dlen)
{

	psbuf_put_1(pb, type);
	psbuf_put_4(pb, reqid);
	psbuf_put_data(pb, data, dlen);
}

void
psbuf_req_str(struct puffs_framebuf *pb, int type, uint32_t reqid,
	const char *str)
{

	psbuf_req_data(pb, type, reqid, str, strlen(str));
}
