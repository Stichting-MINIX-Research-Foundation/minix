/*	$NetBSD: smb_subr.c,v 1.37 2014/11/15 18:52:45 nakayama Exp $	*/

/*
 * Copyright (c) 2000-2001 Boris Popov
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
 * FreeBSD: src/sys/netsmb/smb_subr.c,v 1.6 2002/04/17 03:14:28 bp Exp
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: smb_subr.c,v 1.37 2014/11/15 18:52:45 nakayama Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/lock.h>
#include <sys/sysctl.h>
#include <sys/socket.h>
#include <sys/signal.h>
#include <sys/signalvar.h>
#include <sys/mbuf.h>
#include <sys/socketvar.h>		/* for M_SONAME */
#include <sys/kauth.h>

#include <netsmb/iconv.h>

#include <netsmb/smb.h>
#include <netsmb/smb_conn.h>
#include <netsmb/smb_rq.h>
#include <netsmb/smb_subr.h>

const smb_unichar smb_unieol = 0;

/* XXX M_SMBSTR could be static but that doesn't work with gcc 4.5 on alpha. */
MALLOC_DEFINE(M_SMBSTR, "smbstr", "SMB strings");
MALLOC_DEFINE(M_SMBTEMP, "smbtemp", "Temp netsmb data");

void
smb_makescred(struct smb_cred *scred, struct lwp *l, kauth_cred_t cred)
{
	if (l) {
		scred->scr_l = l;
		scred->scr_cred = cred ? cred : l->l_cred;
	} else {
		scred->scr_l = NULL;
		scred->scr_cred = cred ? cred : NULL;
	}
}

int
smb_proc_intr(struct lwp *l)
{
	struct proc *p;
	int error;

	if (l == NULL)
		return 0;
	p = l->l_proc;

	mutex_enter(p->p_lock);
	error = sigispending(l, 0);
	mutex_exit(p->p_lock);

	return (error != 0 ? EINTR : 0);
}

char *
smb_strdup(const char *s)
{
	char *p;
	size_t len;

	len = s ? strlen(s) + 1 : 1;
	p = malloc(len, M_SMBSTR, M_WAITOK);
	if (s)
		memcpy(p, s, len);
	else
		*p = 0;
	return p;
}

/*
 * duplicate string from a user space.
 */
char *
smb_strdupin(char *s, size_t maxlen)
{
	char *p, bt;
	size_t len = 0;

	for (p = s; ;p++) {
		if (copyin(p, &bt, 1))
			return NULL;
		len++;
		if (maxlen && len > maxlen)
			return NULL;
		if (bt == 0)
			break;
	}
	p = malloc(len, M_SMBSTR, M_WAITOK);
	copyin(s, p, len);
	return p;
}

/*
 * duplicate memory block from a user space.
 */
void *
smb_memdupin(void *umem, size_t len)
{
	char *p;

	if (len > 8 * 1024)
		return NULL;
	p = malloc(len, M_SMBSTR, M_WAITOK);
	if (copyin(umem, p, len) == 0)
		return p;
	free(p, M_SMBSTR);
	return NULL;
}

void
smb_strfree(char *s)
{
	free(s, M_SMBSTR);
}

void
smb_memfree(void *s)
{
	free(s, M_SMBSTR);
}

void *
smb_zmalloc(size_t size, struct malloc_type *type, int flags)
{

	return malloc(size, type, flags | M_ZERO);
}

void
smb_strtouni(u_int16_t *dst, const char *src)
{
	while (*src) {
		*dst++ = htole16(*src++);
	}
	*dst = 0;
}

#ifdef SMB_SOCKETDATA_DEBUG
void
m_dumpm(struct mbuf *m) {
	char *p;
	size_t len;
	printf("d=");
	while(m) {
		p = mtod(m,char *);
		len = m->m_len;
		printf("(%zu)", len);
		while(len--){
			printf("%02x ",((int)*(p++)) & 0xff);
		}
		m=m->m_next;
	};
	printf("\n");
}
#endif

int
smb_maperror(int eclass, int eno)
{
	if (eclass == 0 && eno == 0)
		return 0;
	switch (eclass) {
	    case ERRDOS:
		switch (eno) {
		    case ERRbadfunc:
		    case ERRbadmcb:
		    case ERRbadenv:
		    case ERRbadformat:
		    case ERRrmuns:
			return EINVAL;
		    case ERRnofiles:
		    case ERRbadfile:
		    case ERRbadpath:
		    case ERRremcd:
		    case ERRnoipc: /* nt returns it when share not available */
		    case ERRnosuchshare:	/* observed from nt4sp6 when sharename wrong */
			return ENOENT;
		    case ERRnofids:
			return EMFILE;
		    case ERRnoaccess:
		    case ERRbadshare:
			return EACCES;
		    case ERRbadfid:
			return EBADF;
		    case ERRnomem:
			return ENOMEM;	/* actually remote no mem... */
		    case ERRbadmem:
			return EFAULT;
		    case ERRbadaccess:
			return EACCES;
		    case ERRbaddata:
			return E2BIG;
		    case ERRbaddrive:
		    case ERRnotready:	/* nt */
			return ENXIO;
		    case ERRdiffdevice:
			return EXDEV;
		    case ERRlock:
			return EDEADLK;
		    case ERRfilexists:
			return EEXIST;
		    case ERRinvalidname:	/* dunno what is it, but samba maps as noent */
			return ENOENT;
		    case ERRdirnempty:	/* samba */
			return ENOTEMPTY;
		    case ERRrename:
			return EEXIST;
		    case ERRquota:
			return EDQUOT;
		    case ERRnotlocked:
			/* it's okay to try to unlock already unlocked file */
			return 0;
		    case NT_STATUS_NOTIFY_ENUM_DIR:
			return EMSGSIZE;
		}
		break;
	    case ERRSRV:
		switch (eno) {
		    case ERRerror:
			return EINVAL;
		    case ERRbadpw:
		    case ERRpasswordExpired:
		    case ERRbaduid:
			return EAUTH;
		    case ERRaccess:
			return EACCES;
		    case ERRinvnid:
			return ENETRESET;
		    case ERRinvnetname:
			SMBERROR(("NetBIOS name is invalid\n"));
			return EAUTH;
		    case ERRbadtype:	/* reserved and returned */
			return EIO;
		    case ERRaccountExpired:
		    case ERRbadClient:
		    case ERRbadLogonTime:
			return EPERM;
		    case ERRnosupport:
			return EBADRPC;
		}
		break;
	    case ERRHRD:
		switch (eno) {
		    case ERRnowrite:
			return EROFS;
		    case ERRbadunit:
			return ENODEV;
		    case ERRnotready:
		    case ERRbadcmd:
		    case ERRdata:
			return EIO;
		    case ERRbadreq:
			return EBADRPC;
		    case ERRbadshare:
			return ETXTBSY;
		    case ERRlock:
			return EDEADLK;
		    case ERRgeneral:
			/* returned e.g. for NT CANCEL SMB by Samba */
			return ECANCELED;
		}
		break;
	}
	SMBERROR(("Unmapped error %d:%d\n", eclass, eno));
	return EBADRPC;
}

static int
smb_copy_iconv(struct mbchain *mbp, const char *src, char *dst,
    size_t *srclen, size_t *dstlen)
{
	int error;
	size_t inlen = *srclen, outlen = *dstlen;

	error = iconv_conv((struct iconv_drv*)mbp->mb_udata, &src, &inlen,
	    &dst, &outlen);
	if (inlen != *srclen || outlen != *dstlen) {
		*srclen -= inlen;
		*dstlen -= outlen;
		return 0;
	} else
		return error;
}

int
smb_put_dmem(struct mbchain *mbp, struct smb_vc *vcp, const char *src,
	size_t size, int caseopt)
{
	struct iconv_drv *dp = vcp->vc_toserver;

	if (size == 0)
		return 0;
	if (dp == NULL) {
		return mb_put_mem(mbp, (const void *)src, size, MB_MSYSTEM);
	}
	mbp->mb_copy = smb_copy_iconv;
	mbp->mb_udata = dp;
	return mb_put_mem(mbp, (const void *)src, size, MB_MCUSTOM);
}

int
smb_put_dstring(struct mbchain *mbp, struct smb_vc *vcp, const char *src,
	int caseopt)
{
	int error;

	error = smb_put_dmem(mbp, vcp, src, strlen(src), caseopt);
	if (error)
		return error;
	return mb_put_uint8(mbp, 0);
}

#if 0
int
smb_put_asunistring(struct smb_rq *rqp, const char *src)
{
	struct mbchain *mbp = &rqp->sr_rq;
	struct iconv_drv *dp = rqp->sr_vc->vc_toserver;
	u_char c;
	int error;

	while (*src) {
		iconv_convmem(dp, &c, src++, 1);
		error = mb_put_uint16le(mbp, c);
		if (error)
			return error;
	}
	return mb_put_uint16le(mbp, 0);
}
#endif

struct sockaddr *
dup_sockaddr(struct sockaddr *sa, int canwait)
{
	struct sockaddr *sa2;

	sa2 = malloc(sa->sa_len, M_SONAME, canwait ? M_WAITOK : M_NOWAIT);
	if (sa2)
		memcpy(sa2, sa, sa->sa_len);
	return sa2;
}
