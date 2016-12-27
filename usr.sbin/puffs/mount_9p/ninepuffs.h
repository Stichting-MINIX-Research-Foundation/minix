/*	$NetBSD: ninepuffs.h,v 1.12 2007/11/30 19:02:38 pooka Exp $	*/

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

#ifndef PUFFS9P_H_
#define PUFFS9P_H_

#include <sys/queue.h>

#include <puffs.h>

PUFFSOP_PROTOS(puffs9p);

/* Qid structure.  optimized for in-mem.  different order on-wire */
struct qid9p {
	uint64_t	qidpath;
	uint32_t	qidvers;
	uint8_t		qidtype;
};

typedef uint16_t p9ptag_t;
typedef uint32_t p9pfid_t;

/*
 * refuse (no, not *that* refuse) to play if the server doesn't
 * support requests of at least the following size.  It would only
 * make life difficult
 */
#define P9P_MINREQLEN	512

#define P9P_DEFREQLEN	(16*1024)
#define P9P_INVALFID	0
#define P9P_ROOTFID	1

#define NEXTTAG(p9p)	\
    ((++(p9p->nexttag)) == P9PROTO_NOTAG ? p9p->nexttag = 0 : p9p->nexttag)

#define NEXTFID(p9p)	\
    ((++(p9p->nextfid)) == P9P_INVALFID ? p9p->nextfid = 2 : p9p->nextfid)

#define AUTOVAR(pu)							\
	struct puffs_cc *pcc = puffs_cc_getcc(pu);			\
	struct puffs9p *p9p = puffs_getspecific(pu);			\
	uint16_t tag = NEXTTAG(p9p);					\
	struct puffs_framebuf *pb = p9pbuf_makeout();			\
	int rv = 0

#define RETURN(rv)							\
	puffs_framebuf_destroy(pb);					\
	return (rv)

#define GETRESPONSE(pb)							\
do {									\
	if (puffs_framev_enqueue_cc(pcc, p9p->servsock, pb, 0) == -1) {	\
		rv = errno;						\
		goto out;						\
	}								\
} while (/*CONSTCOND*/0)

#define JUSTSEND(pb)							\
do {									\
	if (puffs_framev_enqueue_justsend(pu,p9p->servsock,pb,1,0)==-1){\
		rv = errno;						\
		goto out;						\
	}								\
} while (/*CONSTCOND*/0)

#define SENDCB(pb, f, a)						\
do {									\
	if (puffs_framev_enqueue_cb(pu, p9p->servsock,pb,f,a,0) == -1) {\
		rv = errno;						\
		goto out;						\
	}								\
} while (/*CONSTCOND*/0)

struct puffs9p {
	int servsock;

	p9ptag_t nexttag;
	p9pfid_t nextfid;

	size_t maxreq;		/* negotiated with server */
};

struct dirfid {
	p9pfid_t	fid;
	off_t		seekoff;
	LIST_ENTRY(dirfid) entries;
};

struct p9pnode {
	p9pfid_t	fid_base;
	p9pfid_t	fid_read;
	p9pfid_t	fid_write;

	LIST_HEAD(,dirfid) dir_openlist;
};

struct puffs_framebuf	*p9pbuf_makeout(void);
void			p9pbuf_recycleout(struct puffs_framebuf *);

int	p9pbuf_read(struct puffs_usermount *, struct puffs_framebuf *,int,int*);
int	p9pbuf_write(struct puffs_usermount *, struct puffs_framebuf*,int,int*);
int	p9pbuf_cmp(struct puffs_usermount *,
		   struct puffs_framebuf *, struct puffs_framebuf *, int *);

void	p9pbuf_put_1(struct puffs_framebuf *, uint8_t);
void	p9pbuf_put_2(struct puffs_framebuf *, uint16_t);
void	p9pbuf_put_4(struct puffs_framebuf *, uint32_t);
void	p9pbuf_put_8(struct puffs_framebuf *, uint64_t);
void	p9pbuf_put_str(struct puffs_framebuf *, const char *);
void	p9pbuf_put_data(struct puffs_framebuf *, const void *, uint16_t);
void	p9pbuf_write_data(struct puffs_framebuf *, uint8_t *, uint32_t);

int	p9pbuf_get_1(struct puffs_framebuf *, uint8_t *);
int	p9pbuf_get_2(struct puffs_framebuf *, uint16_t *);
int	p9pbuf_get_4(struct puffs_framebuf *, uint32_t *);
int	p9pbuf_get_8(struct puffs_framebuf *, uint64_t *);
int	p9pbuf_get_str(struct puffs_framebuf *, char **, uint16_t *);
int	p9pbuf_get_data(struct puffs_framebuf *, uint8_t **, uint16_t *);
int	p9pbuf_read_data(struct puffs_framebuf *, uint8_t *, uint32_t);

uint8_t		p9pbuf_get_type(struct puffs_framebuf *);
uint16_t	p9pbuf_get_tag(struct puffs_framebuf *);

int	proto_getqid(struct puffs_framebuf *, struct qid9p *);
int	proto_getstat(struct puffs_framebuf *, struct vattr *,
		      char **, uint16_t *);
int	proto_expect_walk_nqids(struct puffs_framebuf *, uint16_t *);
int	proto_expect_stat(struct puffs_framebuf *, struct vattr *);
int	proto_expect_qid(struct puffs_framebuf *, uint8_t, struct qid9p *);

int	proto_cc_dupfid(struct puffs_usermount *, p9pfid_t, p9pfid_t);
int	proto_cc_clunkfid(struct puffs_usermount *, p9pfid_t, int);
int	proto_cc_open(struct puffs_usermount *, p9pfid_t, p9pfid_t, int);

void	proto_make_stat(struct puffs_framebuf *, const struct vattr *,
			const char *, enum vtype);

struct puffs_node	*p9p_handshake(struct puffs_usermount *,
				       const char *, const char *);

void			qid2vattr(struct vattr *, const struct qid9p *);
struct puffs_node	*newp9pnode_va(struct puffs_usermount *,
				       const struct vattr *, p9pfid_t);
struct puffs_node	*newp9pnode_qid(struct puffs_usermount *,
					const struct qid9p *, p9pfid_t);

int	getdfwithoffset(struct puffs_usermount *, struct p9pnode *, off_t,
			 struct dirfid **);
void	storedf(struct p9pnode *, struct dirfid *);
void	releasedf(struct puffs_usermount *, struct dirfid *);
void	nukealldf(struct puffs_usermount *, struct p9pnode *);

#endif /* PUFFS9P_H_ */
