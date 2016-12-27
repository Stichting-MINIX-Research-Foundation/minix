/*	$NetBSD: psshfs.h,v 1.40 2010/04/01 02:34:09 pooka Exp $	*/

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

#ifndef PSSHFS_H_
#define PSSHFS_H_

#include <sys/queue.h>

#include <puffs.h>

extern unsigned int max_reads;

/*
 * Later protocol versions would have some advantages (such as link count
 * supported directly as a part of stat), but since proto version 3 seems
 * to be the only widely available version, let's not try to jump through
 * too many hoops to be compatible with all versions.
 */
#define SFTP_PROTOVERSION 3

/* extensions, held in psshfs_ctx extensions */
#define SFTP_EXT_POSIX_RENAME	0x01
#define SFTP_EXT_STATVFS	0x02
#define SFTP_EXT_FSTATVFS	0x04

#define DEFAULTREFRESH 30
#define REFRESHTIMEOUT(pctx, t) \
  (!(pctx)->refreshival || ((pctx->refreshival!=-1) && ((t)>pctx->refreshival)))

PUFFSOP_PROTOS(psshfs);

#define NEXTREQ(pctx) ((pctx->nextreq)++)
#define PSSHFSAUTOVAR(pu)						\
	struct puffs_cc *pcc = puffs_cc_getcc(pu);			\
	struct psshfs_ctx *pctx = puffs_getspecific(pu);		\
	uint32_t reqid = NEXTREQ(pctx);					\
	struct puffs_framebuf *pb = psbuf_makeout();			\
	int rv = 0

#define PSSHFSRETURN(rv)						\
	puffs_framebuf_destroy(pb);					\
	return (rv)

#define GETRESPONSE(pb, fd)						\
do {									\
	if (puffs_framev_enqueue_cc(pcc, fd, pb, 0) == -1) 	{	\
		rv = errno;						\
		goto out;						\
	}								\
} while (/*CONSTCOND*/0)

#define JUSTSEND(pb,fd)							\
do {									\
	if (puffs_framev_enqueue_justsend(pu, fd, pb, 1, 0) == -1) {	\
		rv = errno;						\
		goto out;						\
	}								\
} while (/*CONSTCOND*/0)

#define SENDCB(pb, fd, f, a)						\
do {									\
	if (puffs_framev_enqueue_cb(pu, fd, pb, f, a, 0) == -1) {	\
		rv = errno;						\
		goto out;						\
	}								\
} while (/*CONSTCOND*/0)

struct psshfs_dir {
	int valid;
	struct puffs_node *entry;

	char *entryname;
	struct vattr va;
	time_t attrread;
};

struct psshfs_fid {
	time_t mounttime;
	struct puffs_node *node;
};

struct psshfs_wait {
	struct puffs_cc *pw_cc;
	int pw_type;
	TAILQ_ENTRY(psshfs_wait) pw_entries;
};
#define PWTYPE_READDIR	1
#define PWTYPE_READ1	2
#define PWTYPE_READ2	3
#define PWTYPE_WRITE	4

struct psshfs_node {
	struct puffs_node *parent;

	struct psshfs_dir *dir;	/* only valid if we're of type VDIR */

	size_t denttot;
	size_t dentnext;
	time_t dentread;
	int childcount;

	int stat;
	unsigned readcount;

	time_t attrread;
	char *symlink;
	time_t slread;

	char *fhand_r;
	char *fhand_w;
	uint32_t fhand_r_len;
	uint32_t fhand_w_len;
	struct puffs_framebuf *lazyopen_r;
	struct puffs_framebuf *lazyopen_w;
	int lazyopen_err_r, lazyopen_err_w;

	TAILQ_HEAD(, psshfs_wait) pw;
};
#define PSN_RECLAIMED	0x01
#define PSN_HASFH	0x02
#define PSN_READDIR	0x04
#define PSN_DOLAZY_R	0x08
#define PSN_DOLAZY_W	0x10
#define PSN_LAZYWAIT_R	0x20
#define PSN_LAZYWAIT_W	0x40
#define PSN_HANDLECLOSE	0x80

#define HANDLE_READ	0x1
#define HANDLE_WRITE	0x2

struct psshfs_ctx {
	int numconnections;
	int sshfd;
	int sshfd_data;
	pid_t sshpid;
	pid_t sshpid_data;

	const char *mountpath;
	char **sshargs;

	int protover;
	int extensions;

	uint32_t nextreq;

	struct puffs_framebuf *curpb;

	struct psshfs_node psn_root;
	ino_t nextino;

	int canexport;
	time_t mounttime;

	int refreshival;

	int domangleuid, domanglegid;
	uid_t mangleuid, myuid;
	gid_t manglegid, mygid;
};
#define PSSHFD_META 0
#define PSSHFD_DATA 1

int	psshfs_handshake(struct puffs_usermount *, int);

int	psbuf_read(struct puffs_usermount *, struct puffs_framebuf *,int,int*);
int	psbuf_write(struct puffs_usermount *, struct puffs_framebuf *,int,int*);
int	psbuf_cmp(struct puffs_usermount *,
		  struct puffs_framebuf *, struct puffs_framebuf *, int *);

struct puffs_framebuf	*psbuf_makeout(void);
void			psbuf_recycleout(struct puffs_framebuf *);

void	psbuf_put_1(struct puffs_framebuf *, uint8_t);
void	psbuf_put_2(struct puffs_framebuf *, uint16_t);
void	psbuf_put_4(struct puffs_framebuf *, uint32_t);
void	psbuf_put_8(struct puffs_framebuf *, uint64_t);
void	psbuf_put_str(struct puffs_framebuf *, const char *);
void	psbuf_put_data(struct puffs_framebuf *, const void *, uint32_t);
void	psbuf_put_vattr(struct puffs_framebuf *, const struct vattr *,
			const struct psshfs_ctx *);

uint8_t		psbuf_get_type(struct puffs_framebuf *);
uint32_t	psbuf_get_len(struct puffs_framebuf *);
uint32_t	psbuf_get_reqid(struct puffs_framebuf *);

int	psbuf_get_1(struct puffs_framebuf *, uint8_t *);
int	psbuf_get_2(struct puffs_framebuf *, uint16_t *);
int	psbuf_get_4(struct puffs_framebuf *, uint32_t *);
int	psbuf_get_8(struct puffs_framebuf *, uint64_t *);
int	psbuf_get_str(struct puffs_framebuf *, char **, uint32_t *);
int	psbuf_get_vattr(struct puffs_framebuf *, struct vattr *);

int	psbuf_expect_status(struct puffs_framebuf *);
int	psbuf_expect_handle(struct puffs_framebuf *, char **, uint32_t *);
int	psbuf_expect_name(struct puffs_framebuf *, uint32_t *);
int	psbuf_expect_attrs(struct puffs_framebuf *, struct vattr *);

int	psbuf_do_data(struct puffs_framebuf *, uint8_t *, uint32_t *);

void	psbuf_req_data(struct puffs_framebuf *, int, uint32_t,
		       const void *, uint32_t);
void	psbuf_req_str(struct puffs_framebuf *, int, uint32_t, const char *);

int	sftp_readdir(struct puffs_usermount *, struct psshfs_ctx *,
		     struct puffs_node *);

struct psshfs_dir *lookup(struct psshfs_dir *, size_t, const char *);
struct puffs_node *makenode(struct puffs_usermount *, struct puffs_node *,
			    const struct psshfs_dir *, const struct vattr *);
struct puffs_node *allocnode(struct puffs_usermount *, struct puffs_node *,
			    const char *, const struct vattr *);
struct psshfs_dir *direnter(struct puffs_node *, const char *);
void nukenode(struct puffs_node *, const char *, int);
void doreclaim(struct puffs_node *);
int getpathattr(struct puffs_usermount *, const char *, struct vattr *);
int getnodeattr(struct puffs_usermount *, struct puffs_node *, const char *);

void closehandles(struct puffs_usermount *, struct psshfs_node *, int);
void lazyopen_rresp(struct puffs_usermount *, struct puffs_framebuf *,
		    void *, int);
void lazyopen_wresp(struct puffs_usermount *, struct puffs_framebuf *,
		    void *, int);

#endif /* PSSHFS_H_ */
