/*	$NetBSD: smb_smb.c,v 1.33 2012/11/24 19:48:25 nakayama Exp $	*/

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
 * FreeBSD: src/sys/netsmb/smb_smb.c,v 1.10 2003/02/19 05:47:38 imp Exp
 */
/*
 * various SMB requests. Most of the routines merely packs data into mbufs.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: smb_smb.c,v 1.33 2012/11/24 19:48:25 nakayama Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/lock.h>
#include <sys/sysctl.h>
#include <sys/socket.h>
#include <sys/uio.h>

#include <netsmb/iconv.h>

#include <netsmb/smb.h>
#include <netsmb/smb_subr.h>
#include <netsmb/smb_rq.h>
#include <netsmb/smb_conn.h>
#include <netsmb/smb_tran.h>

struct smb_dialect {
	int		 d_id;
	const char	*d_name;
};

static const struct smb_dialect smb_dialects[] = {
	{SMB_DIALECT_CORE,	"PC NETWORK PROGRAM 1.0"},
	{SMB_DIALECT_COREPLUS,	"MICROSOFT NETWORKS 1.03"},
	{SMB_DIALECT_LANMAN1_0,	"MICROSOFT NETWORKS 3.0"},
	{SMB_DIALECT_LANMAN1_0,	"LANMAN1.0"},
	{SMB_DIALECT_LANMAN2_0,	"LM1.2X002"},
	{SMB_DIALECT_LANMAN2_0,	"Samba"},
	{SMB_DIALECT_NTLM0_12,	"NT LANMAN 1.0"},
	{SMB_DIALECT_NTLM0_12,	"NT LM 0.12"},
	{-1,			NULL}
};

static u_int32_t
smb_vc_maxread(struct smb_vc *vcp)
{
	/*
	 * Specs say up to 64k data bytes, but Windows traffic
	 * uses 60k... no doubt for some good reason.
	 */
	if (SMB_CAPS(vcp) & SMB_CAP_LARGE_READX)
		return (60*1024);
	else
		return (vcp->vc_sopt.sv_maxtx);
}

static u_int32_t
smb_vc_maxwrite(struct smb_vc *vcp)
{
	/*
	 * Specs say up to 64k data bytes, but Windows traffic
	 * uses 60k... probably for some good reason.
	 */
	if (SMB_CAPS(vcp) & SMB_CAP_LARGE_WRITEX)
		return (60*1024);
	else
		return (vcp->vc_sopt.sv_maxtx);
}

int
smb_smb_negotiate(struct smb_vc *vcp, struct smb_cred *scred)
{
	const struct smb_dialect *dp;
	struct smb_sopt *sp = NULL;
	struct smb_rq *rqp;
	struct mbchain *mbp;
	struct mdchain *mdp;
	u_int8_t wc, stime[8], sblen;
	u_int16_t dindex, tw, swlen, bc;
	int error, maxqsz;

	KASSERT(scred->scr_l == vcp->vc_iod->iod_l);

	vcp->vc_hflags = 0;
	vcp->vc_hflags2 = 0;
	vcp->obj.co_flags &= ~(SMBV_ENCRYPT);
	sp = &vcp->vc_sopt;
	memset(sp, 0, sizeof(struct smb_sopt));
	error = smb_rq_alloc(VCTOCP(vcp), SMB_COM_NEGOTIATE, scred, &rqp);
	if (error)
		return error;
	smb_rq_getrequest(rqp, &mbp);
	smb_rq_wstart(rqp);
	smb_rq_wend(rqp);
	smb_rq_bstart(rqp);
	for(dp = smb_dialects; dp->d_id != -1; dp++) {
		mb_put_uint8(mbp, SMB_DT_DIALECT);
		smb_put_dstring(mbp, vcp, dp->d_name, SMB_CS_NONE);
	}
	smb_rq_bend(rqp);
	error = smb_rq_simple(rqp);
	SMBSDEBUG(("%d\n", error));
	if (error)
		goto bad;
	smb_rq_getreply(rqp, &mdp);
	do {
		error = md_get_uint8(mdp, &wc);
		if (error)
			break;
		error = md_get_uint16le(mdp, &dindex);
		if (error)
			break;
		if (dindex > 7) {
			SMBERROR(("Don't know how to talk with server %s (%d)\n", "xxx", dindex));
			error = EBADRPC;
			break;
		}
		dp = smb_dialects + dindex;
		sp->sv_proto = dp->d_id;
		SMBSDEBUG(("Dialect %s (%d, %d)\n", dp->d_name, dindex, wc));
		error = EBADRPC;
		if (dp->d_id >= SMB_DIALECT_NTLM0_12) {
			u_int8_t tb;

			if (wc != 17)
				break;
			md_get_uint8(mdp, &tb);
			sp->sv_sm = tb;
			md_get_uint16le(mdp, &sp->sv_maxmux);
			md_get_uint16le(mdp, &sp->sv_maxvcs);
			md_get_uint32le(mdp, &sp->sv_maxtx);
			md_get_uint32le(mdp, &sp->sv_maxraw);
			md_get_uint32le(mdp, &sp->sv_skey);
			md_get_uint32le(mdp, &sp->sv_caps);
			md_get_mem(mdp, stime, 8, MB_MSYSTEM);
			md_get_uint16le(mdp, &tw);
			sp->sv_tz = tw;
			md_get_uint8(mdp, &sblen);
			if (sblen && (sp->sv_sm & SMB_SM_ENCRYPT)) {
				if (sblen != SMB_MAXCHALLENGELEN) {
					SMBERROR(("Unexpected length of security blob (%d)\n", sblen));
					break;
				}
				error = md_get_uint16le(mdp, &bc);
				if (error)
					break;
				if (sp->sv_caps & SMB_CAP_EXT_SECURITY)
					md_get_mem(mdp, NULL, 16, MB_MSYSTEM);
				error = md_get_mem(mdp, vcp->vc_ch, sblen, MB_MSYSTEM);
				if (error)
					break;
				vcp->vc_chlen = sblen;
				vcp->obj.co_flags |= SMBV_ENCRYPT;
			}
			vcp->vc_hflags2 |= SMB_FLAGS2_KNOWS_LONG_NAMES;
			if (dp->d_id == SMB_DIALECT_NTLM0_12 &&
			    sp->sv_maxtx < 4096 &&
			    (sp->sv_caps & SMB_CAP_NT_SMBS) == 0) {
				vcp->obj.co_flags |= SMBV_WIN95;
				SMBSDEBUG(("Win95 detected\n"));
			}
		} else if (dp->d_id > SMB_DIALECT_CORE) {
			md_get_uint16le(mdp, &sp->sv_sm);
			md_get_uint16le(mdp, &tw);
			sp->sv_maxtx = tw;
			md_get_uint16le(mdp, &sp->sv_maxmux);
			md_get_uint16le(mdp, &sp->sv_maxvcs);
			md_get_uint16(mdp, NULL);	/* rawmode */
			md_get_uint32le(mdp, &sp->sv_skey);
			if (wc == 13) {		/* >= LANMAN1 */
				md_get_uint16(mdp, NULL);		/* time */
				md_get_uint16(mdp, NULL);		/* date */
				md_get_uint16le(mdp, &tw);
				sp->sv_tz = tw;
				md_get_uint16le(mdp, &swlen);
				if (swlen > SMB_MAXCHALLENGELEN)
					break;
				md_get_uint16(mdp, NULL);	/* mbz */
				if (md_get_uint16le(mdp, &bc) != 0)
					break;
				if (bc < swlen)
					break;
				if (swlen && (sp->sv_sm & SMB_SM_ENCRYPT)) {
					error = md_get_mem(mdp, vcp->vc_ch, swlen, MB_MSYSTEM);
					if (error)
						break;
					vcp->vc_chlen = swlen;
					vcp->obj.co_flags |= SMBV_ENCRYPT;
				}
			}
			vcp->vc_hflags2 |= SMB_FLAGS2_KNOWS_LONG_NAMES;
		} else {	/* an old CORE protocol */
			sp->sv_maxmux = 1;
		}
		error = 0;
	} while (0);
	if (error == 0) {
		vcp->vc_maxvcs = sp->sv_maxvcs;
		if (vcp->vc_maxvcs <= 1) {
			if (vcp->vc_maxvcs == 0)
				vcp->vc_maxvcs = 1;
		}
		if (sp->sv_maxtx <= 0 || sp->sv_maxtx > 0xffff)
			sp->sv_maxtx = 1024;
		else
			sp->sv_maxtx = min(sp->sv_maxtx,
					   63*1024 + SMB_HDRLEN + 16);
		SMB_TRAN_GETPARAM(vcp, SMBTP_RCVSZ, &maxqsz);
		vcp->vc_rxmax = min(smb_vc_maxread(vcp), maxqsz - 1024);
		SMB_TRAN_GETPARAM(vcp, SMBTP_SNDSZ, &maxqsz);
		vcp->vc_wxmax = min(smb_vc_maxwrite(vcp), maxqsz - 1024);
		vcp->vc_txmax = min(sp->sv_maxtx, maxqsz);
		SMBSDEBUG(("TZ = %d\n", sp->sv_tz));
		SMBSDEBUG(("CAPS = %x\n", sp->sv_caps));
		SMBSDEBUG(("MAXMUX = %d\n", sp->sv_maxmux));
		SMBSDEBUG(("MAXVCS = %d\n", sp->sv_maxvcs));
		SMBSDEBUG(("MAXRAW = %d\n", sp->sv_maxraw));
		SMBSDEBUG(("MAXTX = %d\n", sp->sv_maxtx));
	}
bad:
	smb_rq_done(rqp);
	return error;
}

int
smb_smb_ssnsetup(struct smb_vc *vcp, struct smb_cred *scred)
{
	struct smb_rq *rqp;
	struct mbchain *mbp;
	const smb_unichar *unipp;
	smb_uniptr ntencpass = NULL;
	char *up, *pbuf, *encpass;
	const char *pp;
	int error, plen, uniplen, ulen, upper;

	KASSERT(scred->scr_l == vcp->vc_iod->iod_l);

	upper = 0;

again:

	vcp->vc_smbuid = SMB_UID_UNKNOWN;

	error = smb_rq_alloc(VCTOCP(vcp), SMB_COM_SESSION_SETUP_ANDX, scred, &rqp);
	if (error)
		return error;
	pbuf = malloc(SMB_MAXPASSWORDLEN + 1, M_SMBTEMP, M_WAITOK);
	encpass = malloc(24, M_SMBTEMP, M_WAITOK);
	if (vcp->vc_sopt.sv_sm & SMB_SM_USER) {
		/*
		 * We try w/o uppercasing first so Samba mixed case
		 * passwords work.  If that fails we come back and try
		 * uppercasing to satisfy OS/2 and Windows for Workgroups.
		 */
		if (upper) {
			iconv_convstr(vcp->vc_toupper, pbuf,
			    smb_vc_getpass(vcp), SMB_MAXPASSWORDLEN + 1);
		} else {
			strlcpy(pbuf, smb_vc_getpass(vcp),
			    SMB_MAXPASSWORDLEN + 1);
		}
		if (!SMB_UNICODE_STRINGS(vcp))
			iconv_convstr(vcp->vc_toserver, pbuf, pbuf,
			    SMB_MAXPASSWORDLEN + 1);

		if (vcp->vc_sopt.sv_sm & SMB_SM_ENCRYPT) {
			uniplen = plen = 24;
			smb_encrypt(pbuf, vcp->vc_ch, encpass);
			ntencpass = malloc(uniplen, M_SMBTEMP, M_WAITOK);
			if (SMB_UNICODE_STRINGS(vcp)) {
				strlcpy(pbuf, smb_vc_getpass(vcp),
				    SMB_MAXPASSWORDLEN + 1);
			} else
				iconv_convstr(vcp->vc_toserver, pbuf,
				    smb_vc_getpass(vcp),
				    SMB_MAXPASSWORDLEN + 1);
			smb_ntencrypt(pbuf, vcp->vc_ch, (u_char*)ntencpass);
			pp = encpass;
			unipp = ntencpass;
		} else {
			plen = strlen(pbuf) + 1;
			pp = pbuf;
			uniplen = plen * 2;
			ntencpass = malloc(uniplen, M_SMBTEMP, M_WAITOK);
			smb_strtouni(ntencpass, smb_vc_getpass(vcp));
			plen--;

			/*
			 * The uniplen is zeroed because Samba cannot deal
			 * with this 2nd cleartext password.  This Samba
			 * "bug" is actually a workaround for problems in
			 * Microsoft clients.
			 */
			uniplen = 0/*-= 2*/;
			unipp = ntencpass;
		}
	} else {
		/*
		 * In the share security mode password will be used
		 * only in the tree authentication
		 */
		 pp = "";
		 plen = 1;
		 unipp = &smb_unieol;
		 uniplen = 0;
	}
	smb_rq_wstart(rqp);
	mbp = &rqp->sr_rq;
	up = vcp->vc_username;
	ulen = strlen(up) + 1;
	/*
	 * If userid is null we are attempting anonymous browse login
	 * so passwords must be zero length.
	 */
	if (ulen == 1)
		plen = uniplen = 0;
	mb_put_uint8(mbp, 0xff);
	mb_put_uint8(mbp, 0);
	mb_put_uint16le(mbp, 0);
	mb_put_uint16le(mbp, vcp->vc_sopt.sv_maxtx);
	mb_put_uint16le(mbp, vcp->vc_sopt.sv_maxmux);
	mb_put_uint16le(mbp, vcp->vc_number);
	mb_put_uint32le(mbp, vcp->vc_sopt.sv_skey);
	mb_put_uint16le(mbp, plen);
	if (SMB_DIALECT(vcp) < SMB_DIALECT_NTLM0_12) {
		mb_put_uint32le(mbp, 0);
		smb_rq_wend(rqp);
		smb_rq_bstart(rqp);
		mb_put_mem(mbp, pp, plen, MB_MSYSTEM);
		smb_put_dstring(mbp, vcp, up, SMB_CS_NONE);
	} else {
		mb_put_uint16le(mbp, uniplen);
		mb_put_uint32le(mbp, 0);		/* reserved */
		mb_put_uint32le(mbp, vcp->obj.co_flags & SMBV_UNICODE ?
				     SMB_CAP_UNICODE : 0);
		smb_rq_wend(rqp);
		smb_rq_bstart(rqp);
		mb_put_mem(mbp, pp, plen, MB_MSYSTEM);
		mb_put_mem(mbp, (const void *)unipp, uniplen, MB_MSYSTEM);
		smb_put_dstring(mbp, vcp, up, SMB_CS_NONE);		/* AccountName */
		smb_put_dstring(mbp, vcp, vcp->vc_domain, SMB_CS_NONE);	/* PrimaryDomain */
		smb_put_dstring(mbp, vcp, "NetBSD", SMB_CS_NONE);	/* Client's OS */
		smb_put_dstring(mbp, vcp, "NETSMB", SMB_CS_NONE);		/* Client name */
	}
	smb_rq_bend(rqp);
	if (ntencpass)
		free(ntencpass, M_SMBTEMP);
	error = smb_rq_simple(rqp);
	SMBSDEBUG(("%d\n", error));
	if (error) {
		if (error == EACCES)
			error = EAUTH;
		goto bad;
	}
	vcp->vc_smbuid = rqp->sr_rpuid;
bad:
	free(encpass, M_SMBTEMP);
	free(pbuf, M_SMBTEMP);
	smb_rq_done(rqp);
	if (error && !upper && vcp->vc_sopt.sv_sm & SMB_SM_USER) {
		upper = 1;
		goto again;
	}
	return error;
}

int
smb_smb_ssnclose(struct smb_vc *vcp, struct smb_cred *scred)
{
	struct smb_rq *rqp;
	struct mbchain *mbp;
	int error;

	KASSERT(scred->scr_l == vcp->vc_iod->iod_l);

	if (vcp->vc_smbuid == SMB_UID_UNKNOWN)
		return 0;

	error = smb_rq_alloc(VCTOCP(vcp), SMB_COM_LOGOFF_ANDX, scred, &rqp);
	if (error)
		return error;
	mbp = &rqp->sr_rq;
	smb_rq_wstart(rqp);
	mb_put_uint8(mbp, 0xff);
	mb_put_uint8(mbp, 0);
	mb_put_uint16le(mbp, 0);
	smb_rq_wend(rqp);
	smb_rq_bstart(rqp);
	smb_rq_bend(rqp);
	error = smb_rq_simple(rqp);
	SMBSDEBUG(("%d\n", error));
	smb_rq_done(rqp);
	return error;
}

static const char *
smb_share_typename(int stype)
{
	static const char smb_any_share[] = "?????";
	const char *pp;

	switch (stype) {
	case SMB_ST_DISK:
		pp = "A:";
		break;
	case SMB_ST_PRINTER:
		pp = smb_any_share;		/* can't use LPT: here... */
		break;
	case SMB_ST_PIPE:
		pp = "IPC";
		break;
	case SMB_ST_COMM:
		pp = "COMM";
		break;
	case SMB_ST_ANY:
	default:
		pp = smb_any_share;
		break;
	}
	return pp;
}

int
smb_smb_treeconnect(struct smb_share *ssp, struct smb_cred *scred)
{
	struct smb_vc *vcp;
	struct smb_rq rq, *rqp = &rq;
	struct mbchain *mbp;
	const char *pp;
	char *pbuf, *encpass;
	int error, plen, caseopt, upper;

	upper = 0;

again:

#if 0
	/* Disable Unicode for SMB_COM_TREE_CONNECT_ANDX requests */
	if (SSTOVC(ssp)->vc_hflags2 & SMB_FLAGS2_UNICODE) {
		vcp = SSTOVC(ssp);
		if (vcp->vc_toserver) {
			iconv_close(vcp->vc_toserver);
			/* Use NULL until UTF-8 -> ASCII works */
			vcp->vc_toserver = NULL;
		}
		if (vcp->vc_tolocal) {
			iconv_close(vcp->vc_tolocal);
			/* Use NULL until ASCII -> UTF-8 works*/
			vcp->vc_tolocal = NULL;
		}
		vcp->vc_hflags2 &= ~SMB_FLAGS2_UNICODE;
	}
#endif

	ssp->ss_tid = SMB_TID_UNKNOWN;
	error = smb_rq_alloc(SSTOCP(ssp), SMB_COM_TREE_CONNECT_ANDX, scred, &rqp);
	if (error)
		return error;
	vcp = rqp->sr_vc;
	caseopt = SMB_CS_NONE;
	if (vcp->vc_sopt.sv_sm & SMB_SM_USER) {
		plen = 1;
		pp = "";
		pbuf = NULL;
		encpass = NULL;
	} else {
		pbuf = malloc(SMB_MAXPASSWORDLEN + 1, M_SMBTEMP, M_WAITOK);
		encpass = malloc(24, M_SMBTEMP, M_WAITOK);
		/*
		 * We try w/o uppercasing first so Samba mixed case
		 * passwords work.  If that fails we come back and try
		 * uppercasing to satisfy OS/2 and Windows for Workgroups.
		 */
		if (upper) {
			iconv_convstr(vcp->vc_toupper, pbuf,
			    smb_share_getpass(ssp), SMB_MAXPASSWORDLEN + 1);
		} else {
			strlcpy(pbuf, smb_share_getpass(ssp),
			    SMB_MAXPASSWORDLEN + 1);
		}
		if (vcp->vc_sopt.sv_sm & SMB_SM_ENCRYPT) {
			plen = 24;
			smb_encrypt(pbuf, vcp->vc_ch, encpass);
			pp = encpass;
		} else {
			plen = strlen(pbuf) + 1;
			pp = pbuf;
		}
	}
	mbp = &rqp->sr_rq;
	smb_rq_wstart(rqp);
	mb_put_uint8(mbp, 0xff);
	mb_put_uint8(mbp, 0);
	mb_put_uint16le(mbp, 0);
	mb_put_uint16le(mbp, 0);		/* Flags */
	mb_put_uint16le(mbp, plen);
	smb_rq_wend(rqp);
	smb_rq_bstart(rqp);
	mb_put_mem(mbp, pp, plen, MB_MSYSTEM);
	smb_put_dmem(mbp, vcp, "\\\\", 2, caseopt);
	pp = vcp->vc_srvname;
	smb_put_dmem(mbp, vcp, pp, strlen(pp), caseopt);
	smb_put_dmem(mbp, vcp, "\\", 1, caseopt);
	pp = ssp->ss_name;
	smb_put_dstring(mbp, vcp, pp, caseopt);
	pp = smb_share_typename(ssp->ss_type);
	smb_put_dstring(mbp, vcp, pp, caseopt);
	smb_rq_bend(rqp);
	error = smb_rq_simple(rqp);
	SMBSDEBUG(("%d\n", error));
	if (error)
		goto bad;
	ssp->ss_tid = rqp->sr_rptid;
	ssp->ss_vcgenid = vcp->vc_genid;
	ssp->ss_flags |= SMBS_CONNECTED;
bad:
	if (encpass)
		free(encpass, M_SMBTEMP);
	if (pbuf)
		free(pbuf, M_SMBTEMP);
	smb_rq_done(rqp);
	if (error && !upper) {
		upper = 1;
		goto again;
	}
	return error;
}

int
smb_smb_treedisconnect(struct smb_share *ssp, struct smb_cred *scred)
{
	struct smb_rq *rqp;
	int error;

	if (ssp->ss_tid == SMB_TID_UNKNOWN)
		return 0;
	error = smb_rq_alloc(SSTOCP(ssp), SMB_COM_TREE_DISCONNECT, scred, &rqp);
	if (error)
		return error;
	smb_rq_wstart(rqp);
	smb_rq_wend(rqp);
	smb_rq_bstart(rqp);
	smb_rq_bend(rqp);
	error = smb_rq_simple(rqp);
	SMBSDEBUG(("%d\n", error));
	smb_rq_done(rqp);
	ssp->ss_tid = SMB_TID_UNKNOWN;
	return error;
}

static inline int
smb_smb_readx(struct smb_share *ssp, u_int16_t fid, size_t *len, size_t *rresid,
	      struct uio *uio, struct smb_cred *scred)
{
	struct smb_rq *rqp;
	struct mbchain *mbp;
	struct mdchain *mdp;
	u_int8_t wc;
	int error;
	u_int16_t residhi, residlo, off, doff;
	u_int32_t resid;

	if (!(SMB_CAPS(SSTOVC(ssp)) & SMB_CAP_LARGE_FILES) &&
	    uio->uio_offset >= (1LL << 32)) {
		/* Cannot read at/beyond 4G */
		return (EFBIG);
	}

	if (!(SMB_CAPS(SSTOVC(ssp)) & SMB_CAP_LARGE_READX)) {
		size_t blksz;

		blksz = SSTOVC(ssp)->vc_txmax - SMB_HDRLEN - 64;
		if (blksz > 0xffff)
			blksz = 0xffff;

		*len = min(blksz, *len);
	}

	error = smb_rq_alloc(SSTOCP(ssp), SMB_COM_READ_ANDX, scred, &rqp);
	if (error)
		return error;
	smb_rq_getrequest(rqp, &mbp);
	smb_rq_wstart(rqp);
	mb_put_uint8(mbp, 0xff);	/* no secondary command */
	mb_put_uint8(mbp, 0);		/* MBZ */
	mb_put_uint16le(mbp, 0);	/* offset to secondary */
	mb_put_mem(mbp, (void *)&fid, sizeof(fid), MB_MSYSTEM);
	mb_put_uint32le(mbp, uio->uio_offset);
	*len = min(SSTOVC(ssp)->vc_rxmax, *len);
	mb_put_uint16le(mbp, *len);	/* MaxCount */
	mb_put_uint16le(mbp, *len);	/* MinCount (only indicates blocking) */
	mb_put_uint32le(mbp, *len >> 16);	/* MaxCountHigh */
	mb_put_uint16le(mbp, *len);	/* Remaining ("obsolete") */
	mb_put_uint32le(mbp, uio->uio_offset >> 32);	/* OffsetHigh */
	smb_rq_wend(rqp);
	smb_rq_bstart(rqp);
	smb_rq_bend(rqp);
	do {
		error = smb_rq_simple(rqp);
		if (error)
			break;
		smb_rq_getreply(rqp, &mdp);
		off = SMB_HDRLEN;
		md_get_uint8(mdp, &wc);
		off++;
		if (wc != 12) {
			error = EBADRPC;
			break;
		}
		md_get_uint8(mdp, NULL);
		off++;
		md_get_uint8(mdp, NULL);
		off++;
		md_get_uint16(mdp, NULL);
		off += 2;
		md_get_uint16(mdp, NULL);
		off += 2;
		md_get_uint16(mdp, NULL);	/* data compaction mode */
		off += 2;
		md_get_uint16(mdp, NULL);
		off += 2;
		md_get_uint16le(mdp, &residlo);
		off += 2;
		md_get_uint16le(mdp, &doff);	/* data offset */
		off += 2;
		md_get_uint16le(mdp, &residhi);
		off += 2;
		resid = (residhi << 16) | residlo;
		md_get_mem(mdp, NULL, 4 * 2, MB_MSYSTEM);
		off += 4*2;
		md_get_uint16(mdp, NULL);	/* ByteCount */
		off += 2;
		if (doff > off)	/* pad byte(s)? */
			md_get_mem(mdp, NULL, doff - off, MB_MSYSTEM);
		if (resid == 0) {
			*rresid = resid;
			break;
		}
		error = md_get_uio(mdp, uio, resid);
		if (error)
			break;
		*rresid = resid;
	} while(0);
	smb_rq_done(rqp);
	return (error);
}

static inline int
smb_smb_writex(struct smb_share *ssp, u_int16_t fid, size_t *len, size_t *rresid,
	struct uio *uio, struct smb_cred *scred)
{
	struct smb_rq *rqp;
	struct mbchain *mbp;
	struct mdchain *mdp;
	int error;
	u_int8_t wc;
	u_int16_t resid;

	if (!(SMB_CAPS(SSTOVC(ssp)) & SMB_CAP_LARGE_FILES) &&
	    uio->uio_offset >= (1LL << 32)) {
		/* Cannot write at/beyond 4G */
		return (EFBIG);
	}

	if (SMB_CAPS(SSTOVC(ssp)) & SMB_CAP_LARGE_WRITEX) {
		*len = min(SSTOVC(ssp)->vc_wxmax, *len);
	} else {
		size_t blksz;

		blksz = SSTOVC(ssp)->vc_txmax - SMB_HDRLEN - 64;
		if (blksz > 0xffff)
			blksz = 0xffff;

		*len = min(blksz, *len);
	}

	error = smb_rq_alloc(SSTOCP(ssp), SMB_COM_WRITE_ANDX, scred, &rqp);
	if (error != 0)
		return (error);
	smb_rq_getrequest(rqp, &mbp);
	smb_rq_wstart(rqp);
	mb_put_uint8(mbp, 0xff);	/* no secondary command */
	mb_put_uint8(mbp, 0);		/* MBZ */
	mb_put_uint16le(mbp, 0);	/* offset to secondary */
	mb_put_mem(mbp, (void *)&fid, sizeof(fid), MB_MSYSTEM);
	mb_put_uint32le(mbp, uio->uio_offset);
	mb_put_uint32le(mbp, 0);	/* MBZ (timeout) */
	mb_put_uint16le(mbp, 0);	/* !write-thru */
	mb_put_uint16le(mbp, 0);
	mb_put_uint16le(mbp, *len >> 16);
	mb_put_uint16le(mbp, *len);
	mb_put_uint16le(mbp, 64);	/* data offset from header start */
	mb_put_uint32le(mbp, uio->uio_offset >> 32);	/* OffsetHigh */
	smb_rq_wend(rqp);
	smb_rq_bstart(rqp);
	do {
		mb_put_uint8(mbp, 0xee);	/* mimic xp pad byte! */
		error = mb_put_uio(mbp, uio, *len);
		if (error)
			break;
		smb_rq_bend(rqp);
		error = smb_rq_simple(rqp);
		if (error)
			break;
		smb_rq_getreply(rqp, &mdp);
		md_get_uint8(mdp, &wc);
		if (wc != 6) {
			error = EBADRPC;
			break;
		}
		md_get_uint8(mdp, NULL);
		md_get_uint8(mdp, NULL);
		md_get_uint16(mdp, NULL);
		md_get_uint16le(mdp, &resid);
		*rresid = resid;
	} while(0);

	smb_rq_done(rqp);
	return (error);
}

static inline int
smb_smb_read(struct smb_share *ssp, u_int16_t fid,
	size_t *len, size_t *rresid, struct uio *uio, struct smb_cred *scred)
{
	struct smb_rq *rqp;
	struct mbchain *mbp;
	struct mdchain *mdp;
	u_int16_t resid, bc;
	u_int8_t wc;
	int error, rlen, blksz;

	/* Cannot read at/beyond 4G */
	if (uio->uio_offset >= (1LL << 32))
		return (EFBIG);

	error = smb_rq_alloc(SSTOCP(ssp), SMB_COM_READ, scred, &rqp);
	if (error)
		return error;

	blksz = SSTOVC(ssp)->vc_txmax - SMB_HDRLEN - 16;
	rlen = *len = min(blksz, *len);

	smb_rq_getrequest(rqp, &mbp);
	smb_rq_wstart(rqp);
	mb_put_mem(mbp, (void *)&fid, sizeof(fid), MB_MSYSTEM);
	mb_put_uint16le(mbp, rlen);
	mb_put_uint32le(mbp, uio->uio_offset);
	mb_put_uint16le(mbp, min(uio->uio_resid, 0xffff));
	smb_rq_wend(rqp);
	smb_rq_bstart(rqp);
	smb_rq_bend(rqp);
	do {
		error = smb_rq_simple(rqp);
		if (error)
			break;
		smb_rq_getreply(rqp, &mdp);
		md_get_uint8(mdp, &wc);
		if (wc != 5) {
			error = EBADRPC;
			break;
		}
		md_get_uint16le(mdp, &resid);
		md_get_mem(mdp, NULL, 4 * 2, MB_MSYSTEM);
		md_get_uint16le(mdp, &bc);
		md_get_uint8(mdp, NULL);		/* ignore buffer type */
		md_get_uint16le(mdp, &resid);
		if (resid == 0) {
			*rresid = resid;
			break;
		}
		error = md_get_uio(mdp, uio, resid);
		if (error)
			break;
		*rresid = resid;
	} while(0);
	smb_rq_done(rqp);
	return error;
}

int
smb_read(struct smb_share *ssp, u_int16_t fid, struct uio *uio,
	struct smb_cred *scred)
{
	size_t tsize, len, resid;
	int error = 0;
	bool rx = (SMB_CAPS(SSTOVC(ssp)) &
		   (SMB_CAP_LARGE_FILES|SMB_CAP_LARGE_READX)) != 0;

	resid = 0;	/* XXX gcc */

	tsize = uio->uio_resid;
	while (tsize > 0) {
		len = tsize;
		if (rx)
		    error = smb_smb_readx(ssp, fid, &len, &resid, uio, scred);
		else
		    error = smb_smb_read(ssp, fid, &len, &resid, uio, scred);
		if (error)
			break;
		tsize -= resid;
		if (resid < len)
			break;
	}
	return error;
}

static inline int
smb_smb_write(struct smb_share *ssp, u_int16_t fid, size_t *len, size_t *rresid,
	struct uio *uio, struct smb_cred *scred)
{
	struct smb_rq *rqp;
	struct mbchain *mbp;
	struct mdchain *mdp;
	u_int16_t resid;
	u_int8_t wc;
	int error, blksz;

	/* Cannot write at/beyond 4G */
	if (uio->uio_offset >= (1LL << 32))
		return (EFBIG);

	blksz = SSTOVC(ssp)->vc_txmax - SMB_HDRLEN - 16;
	if (blksz > 0xffff)
		blksz = 0xffff;

	resid = *len = min(blksz, *len);

	error = smb_rq_alloc(SSTOCP(ssp), SMB_COM_WRITE, scred, &rqp);
	if (error)
		return error;
	smb_rq_getrequest(rqp, &mbp);
	smb_rq_wstart(rqp);
	mb_put_mem(mbp, (void *)&fid, sizeof(fid), MB_MSYSTEM);
	mb_put_uint16le(mbp, resid);
	mb_put_uint32le(mbp, uio->uio_offset);
	mb_put_uint16le(mbp, min(uio->uio_resid, 0xffff));
	smb_rq_wend(rqp);
	smb_rq_bstart(rqp);
	mb_put_uint8(mbp, SMB_DT_DATA);
	mb_put_uint16le(mbp, resid);
	do {
		error = mb_put_uio(mbp, uio, resid);
		if (error)
			break;
		smb_rq_bend(rqp);
		error = smb_rq_simple(rqp);
		if (error)
			break;
		smb_rq_getreply(rqp, &mdp);
		md_get_uint8(mdp, &wc);
		if (wc != 1) {
			error = EBADRPC;
			break;
		}
		md_get_uint16le(mdp, &resid);
		*rresid = resid;
	} while(0);
	smb_rq_done(rqp);
	return error;
}

int
smb_write(struct smb_share *ssp, u_int16_t fid, struct uio *uio,
	struct smb_cred *scred)
{
	int error = 0;
	size_t len, tsize, resid;
	bool wx = (SMB_CAPS(SSTOVC(ssp)) &
		   (SMB_CAP_LARGE_FILES|SMB_CAP_LARGE_WRITEX)) != 0;

	resid = 0;	/* XXX gcc */

	tsize = uio->uio_resid;
	while (tsize > 0) {
		len = tsize;
		if (wx)
		    error = smb_smb_writex(ssp, fid, &len, &resid, uio, scred);
		else
		    error = smb_smb_write(ssp, fid, &len, &resid, uio, scred);
		if (error != 0)
			break;
		if (resid < len) {
			error = EIO;
			break;
		}
		tsize -= resid;
	}
	return error;
}

#if 0
int
smb_smb_echo(struct smb_vc *vcp, struct smb_cred *scred)
{
	struct smb_rq *rqp;
	struct mbchain *mbp;
	int error;

	error = smb_rq_alloc(VCTOCP(vcp), SMB_COM_ECHO, scred, &rqp);
	if (error)
		return error;
	mbp = &rqp->sr_rq;
	smb_rq_wstart(rqp);
	mb_put_uint16le(mbp, 1);
	smb_rq_wend(rqp);
	smb_rq_bstart(rqp);
	mb_put_uint32le(mbp, 0);
	smb_rq_bend(rqp);
	error = smb_rq_simple(rqp);
	SMBSDEBUG(("%d\n", error));
	smb_rq_done(rqp);
	return error;
}
#endif
