/*	$NetBSD: nfs_var.h,v 1.94 2015/07/15 03:28:55 manu Exp $	*/

/*-
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * XXX needs <nfs/rpcv2.h> and <nfs/nfs.h> because of typedefs
 */

#ifdef _KERNEL
#include <sys/mallocvar.h>
#include <sys/pool.h>

struct vnode;
struct uio;
struct proc;
struct buf;
struct nfs_diskless;
struct sockaddr_in;
struct nfs_dlmount;
struct vnode;
struct nfsd;
struct mbuf;
struct file;
struct nfssvc_sock;
struct nfsmount;
struct socket;
struct nfsreq;
struct vattr;
struct nameidata;
struct nfsnode;
struct sillyrename;
struct componentname;
struct nfsd_srvargs;
struct nfsrv_descript;
struct nfs_fattr;
struct nfsdircache;
union nethostaddr;

/* nfs_bio.c */
int nfs_bioread(struct vnode *, struct uio *, int, kauth_cred_t, int);
struct buf *nfs_getcacheblk(struct vnode *, daddr_t, int, struct lwp *);
int nfs_vinvalbuf(struct vnode *, int, kauth_cred_t, struct lwp *, int);
int nfs_flushstalebuf(struct vnode *, kauth_cred_t, struct lwp *, int);
#define	NFS_FLUSHSTALEBUF_MYWRITE	1	/* assume writes are ours */
int nfs_asyncio(struct buf *);
int nfs_doio(struct buf *);

/* nfs_boot.c */
/* see nfsdiskless.h */

/* nfs_kq.c */
void nfs_kqinit(void);
void nfs_kqfini(void);

/* nfs_node.c */
void nfs_node_init(void);
void nfs_node_done(void);

int nfs_nget1(struct mount *, nfsfh_t *, int, struct nfsnode **, int);
#define	nfs_nget(mp, fhp, fhsize, npp) \
	nfs_nget1((mp), (fhp), (fhsize), (npp), 0)

/* nfs_vnops.c */
int nfs_null(struct vnode *, kauth_cred_t, struct lwp *);
int nfs_setattrrpc(struct vnode *, struct vattr *, kauth_cred_t, struct lwp *);
int nfs_readlinkrpc(struct vnode *, struct uio *, kauth_cred_t);
int nfs_readrpc(struct vnode *, struct uio *);
int nfs_writerpc(struct vnode *, struct uio *, int *, bool, bool *);
int nfs_mknodrpc(struct vnode *, struct vnode **, struct componentname *,
	struct vattr *);
int nfs_removeit(struct sillyrename *);
int nfs_removerpc(struct vnode *, const char *, int, kauth_cred_t,
	struct lwp *);
int nfs_renameit(struct vnode *, struct componentname *, struct sillyrename *);
int nfs_renamerpc(struct vnode *, const char *, int, struct vnode *,
	const char *, int, kauth_cred_t, struct lwp *);
int nfs_readdirrpc(struct vnode *, struct uio *, kauth_cred_t);
int nfs_readdirplusrpc(struct vnode *, struct uio *, kauth_cred_t);
int nfs_sillyrename(struct vnode *, struct vnode *, struct componentname *,
	bool);
int nfs_lookitup(struct vnode *, const char *, int, kauth_cred_t,
	struct lwp *, struct nfsnode **);
int nfs_commit(struct vnode *, off_t, uint32_t, struct lwp *);
int nfs_flush(struct vnode *, kauth_cred_t, int, struct lwp *, int);

/* nfs_serv.c */
int nfsrv3_access(struct nfsrv_descript *, struct nfssvc_sock *,
	struct lwp *, struct mbuf **);
int nfsrv_getattr(struct nfsrv_descript *, struct nfssvc_sock *,
	struct lwp *, struct mbuf **);
int nfsrv_setattr(struct nfsrv_descript *, struct nfssvc_sock *,
	struct lwp *, struct mbuf **);
int nfsrv_lookup(struct nfsrv_descript *, struct nfssvc_sock *,
	struct lwp *, struct mbuf **);
int nfsrv_readlink(struct nfsrv_descript *, struct nfssvc_sock *,
	struct lwp *, struct mbuf **);
int nfsrv_read(struct nfsrv_descript *, struct nfssvc_sock *,
	struct lwp *, struct mbuf **);
int nfsrv_write(struct nfsrv_descript *, struct nfssvc_sock *,
	struct lwp *, struct mbuf **);
int nfsrv_writegather(struct nfsrv_descript **, struct nfssvc_sock *,
	struct lwp *, struct mbuf **);
void nfsrvw_coalesce(struct nfsrv_descript *, struct nfsrv_descript *);
int nfsrv_create(struct nfsrv_descript *, struct nfssvc_sock *,
	struct lwp *, struct mbuf **);
int nfsrv_mknod(struct nfsrv_descript *, struct nfssvc_sock *,
	struct lwp *, struct mbuf **);
int nfsrv_remove(struct nfsrv_descript *, struct nfssvc_sock *,
	struct lwp *, struct mbuf **);
int nfsrv_rename(struct nfsrv_descript *, struct nfssvc_sock *,
	struct lwp *, struct mbuf **);
int nfsrv_link(struct nfsrv_descript *, struct nfssvc_sock *,
	struct lwp *, struct mbuf **);
int nfsrv_symlink(struct nfsrv_descript *, struct nfssvc_sock *,
	struct lwp *, struct mbuf **);
int nfsrv_mkdir(struct nfsrv_descript *, struct nfssvc_sock *,
	struct lwp *, struct mbuf **);
int nfsrv_rmdir(struct nfsrv_descript *, struct nfssvc_sock *,
	struct lwp *, struct mbuf **);
int nfsrv_readdir(struct nfsrv_descript *, struct nfssvc_sock *,
	struct lwp *, struct mbuf **);
int nfsrv_readdirplus(struct nfsrv_descript *, struct nfssvc_sock *,
	struct lwp *, struct mbuf **);
int nfsrv_commit(struct nfsrv_descript *, struct nfssvc_sock *,
	struct lwp *, struct mbuf **);
int nfsrv_statfs(struct nfsrv_descript *, struct nfssvc_sock *,
	struct lwp *, struct mbuf **);
int nfsrv_fsinfo(struct nfsrv_descript *, struct nfssvc_sock *,
	struct lwp *, struct mbuf **);
int nfsrv_pathconf(struct nfsrv_descript *, struct nfssvc_sock *,
	struct lwp *, struct mbuf **);
int nfsrv_null(struct nfsrv_descript *, struct nfssvc_sock *,
	struct lwp *, struct mbuf **);
int nfsrv_noop(struct nfsrv_descript *, struct nfssvc_sock *,
	struct lwp *, struct mbuf **);
int nfsrv_access(struct vnode *, int, kauth_cred_t, int, struct lwp *, int);

/* nfs_socket.c */
int nfs_connect(struct nfsmount *, struct nfsreq *, struct lwp *);
int nfs_reconnect(struct nfsreq *);
void nfs_disconnect(struct nfsmount *);
void nfs_safedisconnect(struct nfsmount *);
int nfs_send(struct socket *, struct mbuf *, struct mbuf *, struct nfsreq *,
	struct lwp *);
int nfs_request(struct nfsnode *, struct mbuf *, int, struct lwp *,
	kauth_cred_t, struct mbuf **, struct mbuf **, char **, int *);
int nfs_rephead(int, struct nfsrv_descript *, struct nfssvc_sock *,
	int, int, u_quad_t *, struct mbuf **, struct mbuf **, char **);
void nfs_timer(void *);
void nfs_timer_init(void);
void nfs_timer_fini(void);
void nfs_timer_start(void);
void nfs_timer_srvinit(bool (*)(void));
void nfs_timer_srvfini(void);
int nfs_sigintr(struct nfsmount *, struct nfsreq *, struct lwp *);
int nfs_getreq(struct nfsrv_descript *, struct nfsd *, int);
int nfs_msg(struct lwp *, const char *, const char *);
void nfsrv_soupcall(struct socket *, void *, int, int);
void nfsrv_rcv(struct nfssvc_sock *);
int nfsrv_getstream(struct nfssvc_sock *, int);
int nfsrv_dorec(struct nfssvc_sock *, struct nfsd *, struct nfsrv_descript **,
    bool *);
void nfsrv_wakenfsd(struct nfssvc_sock *);
int nfsdsock_lock(struct nfssvc_sock *, bool);
void nfsdsock_unlock(struct nfssvc_sock *);
int nfsdsock_drain(struct nfssvc_sock *);
int nfsdsock_sendreply(struct nfssvc_sock *, struct nfsrv_descript *);
void nfsdreq_init(void);
void nfsdreq_fini(void);
struct nfsrv_descript *nfsdreq_alloc(void);
void nfsdreq_free(struct nfsrv_descript *);
bool nfsrv_timer(void);
int nfs_rcvlock(struct nfsmount *, struct nfsreq *);
void nfs_rcvunlock(struct nfsmount *);

void nfsdsock_setbits(struct nfssvc_sock *, int);
void nfsdsock_clearbits(struct nfssvc_sock *, int);
bool nfsdsock_testbits(struct nfssvc_sock *, int);

/*
 * Estimate rto for an nfs rpc sent via. an unreliable datagram.
 * Use the mean and mean deviation of rtt for the appropriate type of rpc
 * for the frequent rpcs and a default for the others.
 * The justification for doing "other" this way is that these rpcs
 * happen so infrequently that timer est. would probably be stale.
 * Also, since many of these rpcs are
 * non-idempotent, a conservative timeout is desired.
 * getattr, lookup - A+2D
 * read, write     - A+4D
 * other           - nm_timeo
 */
#define	NFS_RTO(n, t) \
	((t) == 0 ? (n)->nm_timeo : \
	 ((t) < 3 ? \
	  (((((n)->nm_srtt[t-1] + 3) >> 2) + (n)->nm_sdrtt[t-1] + 1) >> 1) : \
	  ((((n)->nm_srtt[t-1] + 7) >> 3) + (n)->nm_sdrtt[t-1] + 1)))
#define	NFS_SRTT(r)	(r)->r_nmp->nm_srtt[nfs_proct[(r)->r_procnum] - 1]
#define	NFS_SDRTT(r)	(r)->r_nmp->nm_sdrtt[nfs_proct[(r)->r_procnum] - 1]

extern int nfsrtton;
extern struct nfsrtt nfsrtt;
extern const int nfs_proct[];

extern const struct timeval nfs_err_interval;
extern struct timeval nfs_reply_last_err_time;
extern struct timeval nfs_timer_last_err_time;


/* nfs_srvcache.c */
void nfsrv_initcache(void);
void nfsrv_finicache(void);
int nfsrv_getcache(struct nfsrv_descript *, struct nfssvc_sock *,
	struct mbuf **);
void nfsrv_updatecache(struct nfsrv_descript *, int, struct mbuf *);
void nfsrv_cleancache(void);

/* nfs_subs.c */
struct mbuf *nfsm_reqh(struct nfsnode *, u_long, int, char **);
struct mbuf *nfsm_rpchead(kauth_cred_t, int, int, int, int, char *, int,
	char *, struct mbuf *, int, struct mbuf **, u_int32_t *);
int nfsm_mbuftouio(struct mbuf **, struct uio *, int, char **);
int nfsm_uiotombuf(struct uio *, struct mbuf **, int, char **);
int nfsm_disct(struct mbuf **, char **, int, int, char **);
int nfs_adv(struct mbuf **, char **, int, int);
int nfsm_strtmbuf(struct mbuf **, char **, const char *, long);
u_long nfs_dirhash(off_t);
void nfs_initdircache(struct vnode *);
void nfs_initdirxlatecookie(struct vnode *);
struct nfsdircache *nfs_searchdircache(struct vnode *, off_t, int, int *);
struct nfsdircache *nfs_enterdircache(struct vnode *, off_t, off_t, int,
	daddr_t);
void nfs_putdircache(struct nfsnode *, struct nfsdircache *);
void nfs_invaldircache(struct vnode *, int);
#define	NFS_INVALDIRCACHE_FORCE		1
#define	NFS_INVALDIRCACHE_KEEPEOF	2
void nfs_init(void);
void nfs_fini(void);
int nfsm_loadattrcache(struct vnode **, struct mbuf **, char **,
	struct vattr *, int flags);
int nfs_loadattrcache(struct vnode **, struct nfs_fattr *, struct vattr *,
	int flags);
int nfs_getattrcache(struct vnode *, struct vattr *);
void nfs_delayedtruncate(struct vnode *);
int nfs_check_wccdata(struct nfsnode *, const struct timespec *,
	struct timespec *, bool);
int nfs_namei(struct nameidata *, nfsrvfh_t *, uint32_t, struct nfssvc_sock *,
	struct mbuf *, struct mbuf **, char **, struct vnode **, struct lwp *,
	int, int);
void nfs_zeropad(struct mbuf *, int, int);
void nfsm_srvwcc(struct nfsrv_descript *, int, struct vattr *, int,
	struct vattr *, struct mbuf **, char **);
void nfsm_srvpostopattr(struct nfsrv_descript *, int, struct vattr *,
	struct mbuf **, char **);
void nfsm_srvfattr(struct nfsrv_descript *, struct vattr *, struct nfs_fattr *);
int nfsrv_fhtovp(nfsrvfh_t *, int, struct vnode **, kauth_cred_t,
	struct nfssvc_sock *, struct mbuf *, int *, int, int);
int nfs_ispublicfh(const nfsrvfh_t *);
int netaddr_match(int, union nethostaddr *, struct mbuf *);
time_t nfs_attrtimeo(struct nfsmount *, struct nfsnode *);

/* flags for nfs_loadattrcache and friends */
#define	NAC_NOTRUNC	1	/* don't truncate file size */

void nfs_clearcommit(struct mount *);
void nfs_merge_commit_ranges(struct vnode *);
int nfs_in_committed_range(struct vnode *, off_t, off_t);
int nfs_in_tobecommitted_range(struct vnode *, off_t, off_t);
void nfs_add_committed_range(struct vnode *, off_t, off_t);
void nfs_del_committed_range(struct vnode *, off_t, off_t);
void nfs_add_tobecommitted_range(struct vnode *, off_t, off_t);
void nfs_del_tobecommitted_range(struct vnode *, off_t, off_t);

int nfsrv_errmap(struct nfsrv_descript *, int);
void nfs_cookieheuristic(struct vnode *, int *, struct lwp *, kauth_cred_t);

u_int32_t nfs_getxid(void);
void nfs_renewxid(struct nfsreq *);

int nfsrv_composefh(struct vnode *, nfsrvfh_t *, bool);
int nfsrv_comparefh(const nfsrvfh_t *, const nfsrvfh_t *);
void nfsrv_copyfh(nfsrvfh_t *, const nfsrvfh_t *);

extern const enum vtype nv2tov_type[8];
extern const enum vtype nv3tov_type[8];

extern u_int32_t rpc_reply, rpc_msgdenied, rpc_mismatch, rpc_vers,
        rpc_auth_unix, rpc_msgaccepted, rpc_call, rpc_autherr,
	rpc_auth_kerb;
extern u_int32_t nfs_prog;
extern const int nfsv3_procid[NFS_NPROCS];
extern int nfs_ticks;

/* nfs_syscalls.c */
struct sys_getfh_args;
struct sys_nfssvc_args;
int sys_getfh(struct lwp *, const struct sys_getfh_args *, register_t *);
int sys_nfssvc(struct lwp *, const struct sys_nfssvc_args *, register_t *);
int nfssvc_addsock(struct file *, struct mbuf *);
void nfsrv_zapsock(struct nfssvc_sock *);
void nfsrv_slpderef(struct nfssvc_sock *);
void nfsrv_init(int);
void nfsrv_fini(void);
void nfs_iodinit(void);
void nfs_iodfini(void);
int nfs_iodbusy(struct nfsmount *);
int nfs_set_niothreads(int);
int nfs_getauth(struct nfsmount *, struct nfsreq *, kauth_cred_t, char **,
	int *, char *, int *, NFSKERBKEY_T);
int nfs_getnickauth(struct nfsmount *, kauth_cred_t, char **, int *, char *,
	int);
int nfs_savenickauth(struct nfsmount *, kauth_cred_t, int, NFSKERBKEY_T,
	struct mbuf **, char **, struct mbuf *);
/*
 * Backend copyin/out functions for nfssvc(2), so that netbsd32 can
 * easily access NFS.  Each operation either must perform a copyin or
 * copyout of the right data for the emulation.  exp_in() takes a count
 * of the number of export_args to copyin, and order arguments for
 * func(dst, src).
 */
struct nfssvc_copy_ops {
	int (*addsock_in)(struct nfsd_args *, const void *);
	int (*setexports_in)(struct mountd_exports_list *, const void *);
	int (*nsd_in)(struct nfsd_srvargs *, const void *);
	int (*nsd_out)(void *, const struct nfsd_srvargs *);
	int (*exp_in)(struct export_args *, const void *, size_t);
};
int do_nfssvc(struct nfssvc_copy_ops *, struct lwp *, int, void *, register_t *);

/* nfs_export.c */
extern struct nfs_public nfs_pub;
int mountd_set_exports_list(const struct mountd_exports_list *, struct lwp *,
    struct mount *);
int netexport_check(const fsid_t *, struct mbuf *, struct mount **, int *,
    kauth_cred_t *);
void netexport_rdlock(void);
void netexport_rdunlock(void);
void netexport_init(void);
void netexport_fini(void);
bool netexport_hasexports(void);
#endif /* _KERNEL */
