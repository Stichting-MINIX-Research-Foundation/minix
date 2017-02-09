/*	$NetBSD: smb_subr.h,v 1.21 2012/03/13 18:41:01 elad Exp $	*/

/*
 * Copyright (c) 2000-2001, Boris Popov
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
 *    This product includes software developed by Boris Popov.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * FreeBSD: src/sys/netsmb/smb_subr.h,v 1.4 2001/12/10 08:09:48 obrien Exp
 */
#ifndef _NETSMB_SMB_SUBR_H_
#define _NETSMB_SMB_SUBR_H_

#ifndef _KERNEL
#error not supposed to be exposed to userland.
#endif /* !_KERNEL */

MALLOC_DECLARE(M_SMBTEMP);

#define SMBERROR(x)	aprint_error x
#define SMBPANIC(x)	aprint_error x

#ifdef SMB_SOCKET_DEBUG
#define SMBSDEBUG(x)	aprint_debug x
#else
#define SMBSDEBUG(x)	/* nothing */
#endif

#ifdef SMB_IOD_DEBUG
#define SMBIODEBUG(x)	aprint_debug x
#else
#define SMBIODEBUG(x)	/* nothing */
#endif

#ifdef SMB_SOCKETDATA_DEBUG
struct mbuf;
void m_dumpm(struct mbuf *m);
#else
#define m_dumpm(m)
#endif

#define SIGISMEMBER(s,n) sigismember(&(s),n)

#define	SMB_SIGMASK(set) 						\
	(SIGISMEMBER(set, SIGINT) || SIGISMEMBER(set, SIGTERM) ||	\
	 SIGISMEMBER(set, SIGHUP) || SIGISMEMBER(set, SIGKILL) ||	\
	 SIGISMEMBER(set, SIGQUIT))

/* smb_suser() is not used in NetBSD. */
#define	smb_suser(cred)	kauth_authorize_generic(cred, KAUTH_GENERIC_ISSUSER, NULL)

/*
 * Compatibility wrappers for simple locks
 */

#define	smb_slock			kmutex
#define	smb_sl_init(mtx, desc)		mutex_init((mtx), MUTEX_DEFAULT, IPL_NONE)
#define	smb_sl_destroy(mtx)		mutex_destroy(mtx)
#define	smb_sl_lock(mtx)		mutex_enter(mtx)
#define	smb_sl_unlock(mtx)		mutex_exit(mtx)

#define SMB_STRFREE(p)	do { if (p) smb_strfree(p); } while(0)

typedef u_int16_t	smb_unichar;
typedef	smb_unichar	*smb_uniptr;

/*
 * Crediantials of user/process being processing in the connection procedures
 */
struct smb_cred {
	/* struct thread *	scr_td; */
	struct lwp *	scr_l;
	kauth_cred_t	scr_cred;
};

extern const smb_unichar smb_unieol;

struct mbchain;
struct smb_vc;
struct smb_rq;

void smb_makescred(struct smb_cred *scred, struct lwp *l,
    kauth_cred_t cred);
int  smb_proc_intr(struct lwp *);
char *smb_strdup(const char *s);
char *smb_strdupin(char *s, size_t maxlen);
void *smb_memdupin(void *umem, size_t len);
void smb_strtouni(u_int16_t *dst, const char *src);
void smb_strfree(char *s);
void smb_memfree(void *s);
void *smb_zmalloc(size_t size, struct malloc_type *type, int flags);

int  smb_encrypt(const u_char *apwd, u_char *C8, u_char *RN);
int  smb_ntencrypt(const u_char *apwd, u_char *C8, u_char *RN);
int  smb_maperror(int eclass, int eno);
int  smb_put_dmem(struct mbchain *mbp, struct smb_vc *vcp,
	const char *src, size_t len, int caseopt);
int  smb_put_dstring(struct mbchain *mbp, struct smb_vc *vcp,
	const char *src, int caseopt);
int  smb_put_string(struct smb_rq *rqp, const char *src);
#if 0
int  smb_put_asunistring(struct smb_rq *rqp, const char *src);
#endif

struct sockaddr *dup_sockaddr(struct sockaddr *, int);

#endif /* !_NETSMB_SMB_SUBR_H_ */
