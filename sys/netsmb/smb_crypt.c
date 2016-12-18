/*	$NetBSD: smb_crypt.c,v 1.11 2009/03/18 16:00:24 cegger Exp $	*/

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
 * FreeBSD: src/sys/netsmb/smb_crypt.c,v 1.3 2001/08/21 08:07:18 bp Exp
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: smb_crypt.c,v 1.11 2009/03/18 16:00:24 cegger Exp $");

#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/proc.h>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>

#include <sys/md4.h>

#include <netsmb/smb.h>
#include <netsmb/smb_conn.h>
#include <netsmb/smb_subr.h>
#include <netsmb/smb_dev.h>

/* always enable */
#define NETSMBCRYPTO

#ifdef NETSMBCRYPTO

#include <crypto/des/des.h>

static const u_char N8[] = {0x4b, 0x47, 0x53, 0x21, 0x40, 0x23, 0x24, 0x25};


static void
smb_E(const u_char *key, const u_char *data, u_char *dest)
{
	des_key_schedule *ksp;
	u_char kk[8];

	kk[0] = key[0] & 0xfe;
	kk[1] = key[0] << 7 | (key[1] >> 1 & 0xfe);
	kk[2] = key[1] << 6 | (key[2] >> 2 & 0xfe);
	kk[3] = key[2] << 5 | (key[3] >> 3 & 0xfe);
	kk[4] = key[3] << 4 | (key[4] >> 4 & 0xfe);
	kk[5] = key[4] << 3 | (key[5] >> 5 & 0xfe);
	kk[6] = key[5] << 2 | (key[6] >> 6 & 0xfe);
	kk[7] = key[6] << 1;
	ksp = malloc(sizeof(des_key_schedule), M_SMBTEMP, M_WAITOK);
	des_set_key((des_cblock *)kk, *ksp);
	/* XXXUNCONST */
	des_ecb_encrypt(__UNCONST(data), (des_cblock *)dest, *ksp, 1);
	free(ksp, M_SMBTEMP);
}
#endif


int
smb_encrypt(const u_char *apwd, u_char *C8, u_char *RN)
{
#ifdef NETSMBCRYPTO
	u_char *p, *P14, *S21;

	p = malloc(14 + 21, M_SMBTEMP, M_WAITOK|M_ZERO);
	P14 = p;
	S21 = p + 14;
	bcopy(apwd, P14, min(14, strlen(apwd)));
	/*
	 * S21 = concat(Ex(P14, N8), zeros(5));
	 */
	smb_E(P14, N8, S21);
	smb_E(P14 + 7, N8, S21 + 8);

	smb_E(S21, C8, RN);
	smb_E(S21 + 7, C8, RN + 8);
	smb_E(S21 + 14, C8, RN + 16);
	free(p, M_SMBTEMP);
	return 0;
#else
	SMBERROR(("password encryption is not available\n"));
	memset(RN, 0, 24);
	return EAUTH;
#endif
}

int
smb_ntencrypt(const u_char *apwd, u_char *C8, u_char *RN)
{
#ifdef NETSMBCRYPTO
	u_char S21[21];
	u_int16_t *unipwd;
	MD4_CTX *ctxp;
	int len;

	len = strlen(apwd);
	unipwd = malloc((len + 1) * sizeof(u_int16_t), M_SMBTEMP, M_WAITOK);
	/*
	 * S21 = concat(MD4(U(apwd)), zeros(5));
	 */
	smb_strtouni(unipwd, apwd);
	ctxp = malloc(sizeof(MD4_CTX), M_SMBTEMP, M_WAITOK);
	MD4Init(ctxp);
	MD4Update(ctxp, (u_char*)unipwd, len * sizeof(u_int16_t));
	free(unipwd, M_SMBTEMP);
	memset(S21, 0, 21);
	MD4Final(S21, ctxp);
	free(ctxp, M_SMBTEMP);

	smb_E(S21, C8, RN);
	smb_E(S21 + 7, C8, RN + 8);
	smb_E(S21 + 14, C8, RN + 16);
	return 0;
#else
	SMBERROR(("password encryption is not available\n"));
	memset(RN, 0, 24);
	return EAUTH;
#endif
}

