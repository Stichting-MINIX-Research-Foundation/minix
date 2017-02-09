/*	$NetBSD: nineproto.c,v 1.9 2007/11/30 19:02:38 pooka Exp $	*/

/*
 * Copyright (c) 2007  Antti Kantee.  All Rights Reserved.
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
__RCSID("$NetBSD: nineproto.c,v 1.9 2007/11/30 19:02:38 pooka Exp $");
#endif /* !lint */

#include <sys/types.h>

#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <puffs.h>
#include <stdio.h>
#include <stdlib.h>

#include "ninepuffs.h"
#include "nineproto.h"

int
proto_getqid(struct puffs_framebuf *pb, struct qid9p *qid)
{

	if (puffs_framebuf_remaining(pb) < 1+4+8)
		return ENOBUFS;

	p9pbuf_get_1(pb, &qid->qidtype);
	p9pbuf_get_4(pb, &qid->qidvers);
	p9pbuf_get_8(pb, &qid->qidpath);

	return 0;
}

static uid_t
ustr2uid(char *uid)
{
	struct passwd *pw;

	pw = getpwnam(uid);
	if (pw == NULL)
		return 0; /* XXXXX */

	return pw->pw_uid;
}

static gid_t
gstr2gid(char *gid)
{
	struct group *grr;

	grr = getgrnam(gid);
	if (grr == NULL)
		return 0; /* more XXXX */

	return grr->gr_gid;
}

static const char *
uid2ustr(uid_t uid)
{
	struct passwd *pw;

	pw = getpwuid(uid);
	if (pw == NULL)
		return "root"; /* XXXXX */

	return pw->pw_name;
}

static const char *
gid2gstr(gid_t gid)
{
	struct group *grr;

	grr = getgrgid(gid);
	if (grr == NULL)
		return "wheel"; /* XXXXXX */

	return grr->gr_name;
}

#define GETFIELD(a,b,unitsize)						\
do {									\
	if (size < unitsize) return EPROTO;				\
	if ((rv = (a(pb, b)))) return rv;				\
	size -= unitsize;						\
} while (/*CONSTCOND*/0)
#define GETSTR(val,strsize)						\
do {									\
	if ((rv = p9pbuf_get_str(pb, val, strsize))) return rv;		\
	if (*strsize > size) return EPROTO;				\
	size -= *strsize;						\
} while (/*CONSTCOND*/0)
int
proto_getstat(struct puffs_framebuf *pb, struct vattr *vap,
	char **name, uint16_t *rs)
{
	char *uid, *gid;
	struct qid9p qid;
	uint64_t flen;
	uint32_t rdev, mode, atime, mtime;
	uint16_t size, v16;
	int rv;

	/* check size */
	if ((rv = p9pbuf_get_2(pb, &size)))
		return rv;
	if (puffs_framebuf_remaining(pb) < size)
		return ENOBUFS;

	if (rs)
		*rs = size+2; /* compensate for size field itself */

	GETFIELD(p9pbuf_get_2, &v16, 2);
	if (v16)
		printf("%d\n", v16);
	GETFIELD(p9pbuf_get_4, &rdev, 4);
	GETFIELD(proto_getqid, &qid, 13);
	GETFIELD(p9pbuf_get_4, &mode, 4);
	GETFIELD(p9pbuf_get_4, &atime, 4);
	GETFIELD(p9pbuf_get_4, &mtime, 4);
	GETFIELD(p9pbuf_get_8, &flen, 8);
	GETSTR(name, &v16);
	GETSTR(&uid, &v16);
	GETSTR(&gid, &v16);

	if (rdev)
		printf("%d\n", rdev);
	vap->va_rdev = rdev;
	vap->va_mode = mode & 0777; /* may contain other uninteresting bits */
	vap->va_atime.tv_sec = atime;
	vap->va_mtime.tv_sec = mtime;
	vap->va_ctime.tv_sec = mtime;
	vap->va_atime.tv_nsec=vap->va_mtime.tv_nsec=vap->va_ctime.tv_nsec = 0;
	vap->va_birthtime.tv_sec = vap->va_birthtime.tv_nsec = 0;
	vap->va_size = vap->va_bytes = flen;
	vap->va_uid = ustr2uid(uid);
	vap->va_gid = gstr2gid(gid);
	free(uid);
	free(gid);
	qid2vattr(vap, &qid);

	/* some defaults */
	if (vap->va_type == VDIR)
		vap->va_nlink = 1906;
	else
		vap->va_nlink = 1;
	vap->va_blocksize = 512;
	vap->va_flags = vap->va_vaflags = 0;
	vap->va_filerev = PUFFS_VNOVAL;

	/* muid, not used */
	GETSTR(NULL, &v16);

	return 0;
}

int
proto_cc_dupfid(struct puffs_usermount *pu, p9pfid_t oldfid, p9pfid_t newfid)
{
	struct puffs_cc *pcc = puffs_cc_getcc(pu);
	struct puffs9p *p9p = puffs_getspecific(pu);
	struct puffs_framebuf *pb;
	p9ptag_t tag = NEXTTAG(p9p);
	uint16_t qids;
	int rv = 0;

	pb = p9pbuf_makeout();
	p9pbuf_put_1(pb, P9PROTO_T_WALK);
	p9pbuf_put_2(pb, tag);
	p9pbuf_put_4(pb, oldfid);
	p9pbuf_put_4(pb, newfid);
	p9pbuf_put_2(pb, 0);
	GETRESPONSE(pb);

	rv = proto_expect_walk_nqids(pb, &qids);
	if (rv == 0 && qids != 0)
		rv = EPROTO;

 out:
	puffs_framebuf_destroy(pb);
	return rv;
}

int
proto_cc_clunkfid(struct puffs_usermount *pu, p9pfid_t fid, int waitforit)
{
	struct puffs_cc *pcc = puffs_cc_getcc(pu);
	struct puffs9p *p9p = puffs_getspecific(pu);
	struct puffs_framebuf *pb;
	p9ptag_t tag = NEXTTAG(p9p);
	int rv = 0;

	pb = p9pbuf_makeout();
	p9pbuf_put_1(pb, P9PROTO_T_CLUNK);
	p9pbuf_put_2(pb, tag);
	p9pbuf_put_4(pb, fid);

	if (waitforit) {
		if (puffs_framev_enqueue_cc(pcc, p9p->servsock, pb, 0) == 0) {
			if (p9pbuf_get_type(pb) != P9PROTO_R_CLUNK)
				rv = EPROTO;
		} else {
			rv = errno;
		}
		puffs_framebuf_destroy(pb);
	} else {
		JUSTSEND(pb);
	}

 out:
	return rv;
}

/*
 * walk a new fid, then open it
 */
int
proto_cc_open(struct puffs_usermount *pu, p9pfid_t fid,
	p9pfid_t newfid, int mode)
{
	struct puffs_cc *pcc = puffs_cc_getcc(pu);
	struct puffs9p *p9p = puffs_getspecific(pu);
	struct puffs_framebuf *pb;
	p9ptag_t tag = NEXTTAG(p9p);
	int rv;

	rv = proto_cc_dupfid(pu, fid, newfid);
	if (rv)
		return rv;

	pb = p9pbuf_makeout();
	p9pbuf_put_1(pb, P9PROTO_T_OPEN);
	p9pbuf_put_2(pb, tag);
	p9pbuf_put_4(pb, newfid);
	p9pbuf_put_1(pb, mode);
	GETRESPONSE(pb);
	if (p9pbuf_get_type(pb) != P9PROTO_R_OPEN)
		rv = EPROTO;

 out:
	puffs_framebuf_destroy(pb);
	return rv;
}

void
proto_make_stat(struct puffs_framebuf *pb, const struct vattr *vap,
	const char *filename, enum vtype vt)
{
	struct vattr fakeva;
	uint32_t mode, atime, mtime;
	uint64_t flen;
	const char *owner, *group;
	int startoff, curoff;

	if (vap == NULL) {
		puffs_vattr_null(&fakeva);
		vap = &fakeva;
	}

	startoff = puffs_framebuf_telloff(pb);
	puffs_framebuf_seekset(pb, startoff + 2+2); /* stat[n] incl. stat[2] */

	if (vap->va_mode != (mode_t)PUFFS_VNOVAL)
		mode = vap->va_mode | (vt == VDIR ? P9PROTO_CPERM_DIR : 0);
	else
		mode = P9PROTO_STAT_NOVAL4;
	if (vap->va_atime.tv_sec != (time_t)PUFFS_VNOVAL)
		atime = vap->va_atime.tv_sec;
	else
		atime = P9PROTO_STAT_NOVAL4;
	if (vap->va_mtime.tv_sec != (time_t)PUFFS_VNOVAL)
		mtime = vap->va_mtime.tv_sec;
	else
		mtime = P9PROTO_STAT_NOVAL4;
	if (vap->va_size != (u_quad_t)PUFFS_VNOVAL)
		flen = vap->va_size;
	else
		flen = P9PROTO_STAT_NOVAL8;
	if (vap->va_uid != (uid_t)PUFFS_VNOVAL)
		owner = uid2ustr(vap->va_uid);
	else
		owner = "";
	if (vap->va_gid != (gid_t)PUFFS_VNOVAL)
		group = gid2gstr(vap->va_gid);
	else
		group = "";

	p9pbuf_put_2(pb, P9PROTO_STAT_NOVAL2);	/* kernel type	*/
	p9pbuf_put_4(pb, P9PROTO_STAT_NOVAL4);	/* dev		*/
	p9pbuf_put_1(pb, P9PROTO_STAT_NOVAL1);	/* type		*/
	p9pbuf_put_4(pb, P9PROTO_STAT_NOVAL4);	/* version	*/
	p9pbuf_put_8(pb, P9PROTO_STAT_NOVAL8);	/* path		*/
	p9pbuf_put_4(pb, mode);
	p9pbuf_put_4(pb, atime);
	p9pbuf_put_4(pb, mtime);
	p9pbuf_put_8(pb, flen);
	p9pbuf_put_str(pb, filename ? filename : "");
	p9pbuf_put_str(pb, owner);
	p9pbuf_put_str(pb, group);
	p9pbuf_put_str(pb, "");			/* muid		*/

	curoff = puffs_framebuf_telloff(pb);
	puffs_framebuf_seekset(pb, startoff);
	p9pbuf_put_2(pb, curoff-(startoff+2));	/* stat[n] size	*/
	p9pbuf_put_2(pb, curoff-(startoff+4));	/* size[2] stat	*/

	puffs_framebuf_seekset(pb, curoff);
}

int
proto_expect_walk_nqids(struct puffs_framebuf *pb, uint16_t *nqids)
{

	if (p9pbuf_get_type(pb) != P9PROTO_R_WALK)
		return EPROTO;
	return p9pbuf_get_2(pb, nqids);
}

int
proto_expect_qid(struct puffs_framebuf *pb, uint8_t op, struct qid9p *qid)
{

	if (p9pbuf_get_type(pb) != op)
		return EPROTO;
	return proto_getqid(pb, qid);
}

int
proto_expect_stat(struct puffs_framebuf *pb, struct vattr *va)
{
	uint16_t dummy;
	int rv;

	if (p9pbuf_get_type(pb) != P9PROTO_R_STAT)
		return EPROTO;
	if ((rv = p9pbuf_get_2(pb, &dummy)))
		return rv;
	return proto_getstat(pb, va, NULL, NULL);
}
