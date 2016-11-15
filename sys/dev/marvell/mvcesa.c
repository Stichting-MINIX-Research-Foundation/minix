/*	$NetBSD: mvcesa.c,v 1.1 2012/07/27 03:00:01 kiyohara Exp $	*/
/*
 * Copyright (c) 2008 KIYOHARA Takashi
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: mvcesa.c,v 1.1 2012/07/27 03:00:01 kiyohara Exp $");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/cprng.h>
#include <sys/device.h>
#include <sys/endian.h>
#include <sys/errno.h>
#include <sys/mbuf.h>
#include <sys/md5.h>
#include <sys/uio.h>
#include <sys/sha1.h>

#include <opencrypto/cryptodev.h>
#include <opencrypto/xform.h>

#include <dev/marvell/marvellreg.h>
#include <dev/marvell/marvellvar.h>
#include <dev/marvell/mvcesareg.h>

#include "locators.h"

#define MVCESA_SESSION(sid)		((sid) & 0x0fffffff)
#define MVCESA_SID(crd, sesn)		(((crd) << 28) | ((sesn) & 0x0fffffff))


struct mvcesa_session {
	int ses_used;

	int ses_klen;
	uint32_t ses_iv[4];
	uint32_t ses_key[8];

	uint32_t ses_hminner[5];	/* HMAC inner state */
	uint32_t ses_hmouter[5];	/* HMAC outer state */
};

struct mvcesa_softc {
	device_t sc_dev;

	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_ioh;
	bus_dma_tag_t sc_dmat;

	int sc_cid;
	int sc_nsessions;
	struct mvcesa_session *sc_sessions;
};

static int mvcesa_match(device_t, cfdata_t, void *);
static void mvcesa_attach(device_t, device_t, void *);

static int mvcesa_intr(void *);

static int mvcesa_newsession(void *, u_int32_t *, struct cryptoini *);
static int mvcesa_freesession(void *, u_int64_t);
static int mvcesa_process(void *, struct cryptop *, int);

static int mvcesa_authentication(struct mvcesa_softc *, struct mvcesa_session *,
				 uint32_t, uint32_t *, uint32_t *, uint64_t,
				 int, int, char *, struct mbuf *, struct uio *);
static int mvcesa_des_encdec(struct mvcesa_softc *, struct mvcesa_session *,
			     uint32_t, uint32_t, uint32_t, uint32_t *, int, int,
			     char *, struct mbuf *, struct uio *);


CFATTACH_DECL_NEW(mvcesa_gt, sizeof(struct mvcesa_softc),
    mvcesa_match, mvcesa_attach, NULL, NULL);
CFATTACH_DECL_NEW(mvcesa_mbus, sizeof(struct mvcesa_softc),
    mvcesa_match, mvcesa_attach, NULL, NULL);


/* ARGSUSED */
static int
mvcesa_match(device_t parent, cfdata_t match, void *aux)
{
	struct marvell_attach_args *mva = aux;

	if (strcmp(mva->mva_name, match->cf_name) != 0)
		return 0;
	if (mva->mva_offset == MVA_OFFSET_DEFAULT ||
	    mva->mva_irq == MVA_IRQ_DEFAULT)
		return 0;

	mva->mva_size = MVCESA_SIZE;
	return 1;
}

/* ARGSUSED */
static void
mvcesa_attach(device_t parent, device_t self, void *aux)
{
	struct mvcesa_softc *sc = device_private(self);
	struct marvell_attach_args *mva = aux;

	aprint_normal(
	    ": Marvell Cryptographic Engines and Security Accelerator\n");
	aprint_naive("\n");

	sc->sc_dev = self;
	sc->sc_iot = mva->mva_iot;
        /* Map I/O registers */
	if (bus_space_subregion(mva->mva_iot, mva->mva_ioh, mva->mva_offset,
	    mva->mva_size, &sc->sc_ioh)) {
		aprint_error_dev(self, "can't map registers\n");
		return;
	}
	sc->sc_dmat = mva->mva_dmat;

	sc->sc_nsessions = 0;

	/* Setup Opencrypto stuff */
	sc->sc_cid = crypto_get_driverid(0);
	if (sc->sc_cid < 0) {
		aprint_error_dev(self, "couldn't get crypto driver id\n");
		return;
	}
	crypto_register(sc->sc_cid, CRYPTO_DES_CBC, 0, 0,
	    mvcesa_newsession, mvcesa_freesession, mvcesa_process, sc);
	crypto_register(sc->sc_cid, CRYPTO_3DES_CBC, 0, 0,
	    mvcesa_newsession, mvcesa_freesession, mvcesa_process, sc);
#if __DMA_notyet__
/*
 * Don't know how to process to AES CBC in PIO-mode.
 * I havn't found IV registers.
 */
	crypto_register(sc->sc_cid, CRYPTO_AES_CBC, 0, 0,
	    mvcesa_newsession, mvcesa_freesession, mvcesa_process, sc);
#endif
	crypto_register(sc->sc_cid, CRYPTO_SHA1, 0, 0,
	    mvcesa_newsession, mvcesa_freesession, mvcesa_process, sc);
	crypto_register(sc->sc_cid, CRYPTO_SHA1_HMAC, 0, 0,
	    mvcesa_newsession, mvcesa_freesession, mvcesa_process, sc);
	crypto_register(sc->sc_cid, CRYPTO_MD5, 0, 0,
	    mvcesa_newsession, mvcesa_freesession, mvcesa_process, sc);
	crypto_register(sc->sc_cid, CRYPTO_MD5_HMAC, 0, 0,
	    mvcesa_newsession, mvcesa_freesession, mvcesa_process, sc);

	/* Clear and establish interrupt */
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVCESA_IC, 0);
	marvell_intr_establish(mva->mva_irq, IPL_NET, mvcesa_intr, sc);

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVCESA_IM, 0);
}


static int
mvcesa_intr(void *arg)
{
#if 0
	struct mvcesa_softc *sc = (struct mvcesa_softc *)arg;
#endif
	int handled = 0;

	return handled;
}


/*
 * Opencrypto functions
 */
/*
 * Allocate a new 'session' and return an encoded session id.  'sidp'
 * contains our registration id, and should contain an encoded session
 * id on successful allocation.
 */
static int
mvcesa_newsession(void *arg, u_int32_t *sidp, struct cryptoini *cri)
{
	struct mvcesa_softc *sc = (struct mvcesa_softc *)arg;
	struct cryptoini *c;
	struct mvcesa_session *ses = NULL;
	int sesn, count, enc, mac, i;

	KASSERT(sc != NULL /*, ("mvcesa_newsession: null softc")*/);
	if (sidp == NULL || cri == NULL || sc == NULL)
		return EINVAL;

	for (sesn = 0; sesn < sc->sc_nsessions; sesn++)
		if (sc->sc_sessions[sesn].ses_used == 0) {
			ses = sc->sc_sessions + sesn;
			break;
		}

	if (ses == NULL) {
		sesn = sc->sc_nsessions;
		ses = malloc((sesn + 1) * sizeof(*ses), M_DEVBUF, M_NOWAIT);
		if (ses == NULL)
			return ENOMEM;
		if (sesn != 0) {
			memcpy(ses, sc->sc_sessions, sesn * sizeof(*ses));
			memset(sc->sc_sessions, 0, sesn * sizeof(*ses));
			free(sc->sc_sessions, M_DEVBUF);
		}
		sc->sc_sessions = ses;
		ses = sc->sc_sessions + sesn;
		sc->sc_nsessions++;
	}
	memset(ses, 0, sizeof(*ses));

	count = 0;
	enc = mac = 0;
	for (c = cri; c != NULL; c = c->cri_next) {
		switch (c->cri_alg) {
		case CRYPTO_DES_CBC:
		case CRYPTO_3DES_CBC:
			if (enc)
				return EINVAL;
			enc = 1;

			cprng_fast(ses->ses_iv,
			    c->cri_alg == CRYPTO_AES_CBC ? 16 : 8);

			/* Go ahead and compute key in CESA's byte order */
			ses->ses_klen = c->cri_klen;
			memcpy(ses->ses_key, c->cri_key, c->cri_klen / 8);
			switch (c->cri_alg) {
			case CRYPTO_3DES_CBC:
				ses->ses_key[5] = htobe32(ses->ses_key[5]);
				ses->ses_key[4] = htobe32(ses->ses_key[4]);
				ses->ses_key[3] = htobe32(ses->ses_key[3]);
				ses->ses_key[2] = htobe32(ses->ses_key[2]);

				/* FALLTHROUGH */
			case CRYPTO_DES_CBC:
				ses->ses_key[1] = htobe32(ses->ses_key[1]);
				ses->ses_key[0] = htobe32(ses->ses_key[0]);
			}
			break;

		case CRYPTO_SHA1_HMAC:
		case CRYPTO_MD5_HMAC:
		{
			MD5_CTX md5ctx;
			SHA1_CTX sha1ctx;
			int klen_bytes = c->cri_klen / 8;

			KASSERT(c->cri_klen == 512);

			for (i = 0; i < klen_bytes; i++)
				c->cri_key[i] ^= HMAC_IPAD_VAL;
			if (c->cri_alg == CRYPTO_MD5_HMAC_96) {
				MD5Init(&md5ctx);
				MD5Update(&md5ctx, c->cri_key, klen_bytes);
				MD5Update(&md5ctx, hmac_ipad_buffer,
				    HMAC_BLOCK_LEN - klen_bytes);
				memcpy(ses->ses_hminner, md5ctx.state,
				    sizeof(md5ctx.state));
			} else {
				SHA1Init(&sha1ctx);
				SHA1Update(&sha1ctx, c->cri_key, klen_bytes);
				SHA1Update(&sha1ctx, hmac_ipad_buffer,
				    HMAC_BLOCK_LEN - klen_bytes);
				memcpy(ses->ses_hminner, sha1ctx.state,
				    sizeof(sha1ctx.state));
			}

			for (i = 0; i < klen_bytes; i++)
				c->cri_key[i] ^=
				    (HMAC_IPAD_VAL ^ HMAC_OPAD_VAL);
			if (c->cri_alg == CRYPTO_MD5_HMAC_96) {
				MD5Init(&md5ctx);
				MD5Update(&md5ctx, c->cri_key, klen_bytes);
				MD5Update(&md5ctx, hmac_opad_buffer,
				    HMAC_BLOCK_LEN - klen_bytes);
				memcpy(ses->ses_hmouter, md5ctx.state,
				    sizeof(md5ctx.state));
			} else {
				SHA1Init(&sha1ctx);
				SHA1Update(&sha1ctx, c->cri_key, klen_bytes);
				SHA1Update(&sha1ctx, hmac_opad_buffer,
				    HMAC_BLOCK_LEN - klen_bytes);
				memcpy(ses->ses_hmouter, sha1ctx.state,
				    sizeof(sha1ctx.state));
			}

			for (i = 0; i < klen_bytes; i++)
				c->cri_key[i] ^= HMAC_OPAD_VAL;
		}
			/* FALLTHROUGH */

		case CRYPTO_SHA1:
		case CRYPTO_MD5:
			if (mac)
				return EINVAL;
			mac = 1;
		}
		count++;
	}
	if (count > 2) {
		mvcesa_freesession(sc, sesn);
		return EINVAL;
	}

	*sidp = MVCESA_SID(device_unit(sc->sc_dev), sesn);
	ses->ses_used = 1;

	return 0;
}

/*
 * Deallocate a session.
 */
static int
mvcesa_freesession(void *arg, u_int64_t tid)
{
	struct mvcesa_softc *sc = (struct mvcesa_softc *)arg;
	int session;
	uint32_t sid = ((uint32_t)tid) & 0xffffffff;

	KASSERT(sc != NULL /*, ("mvcesa_freesession: null softc")*/);

	session = MVCESA_SESSION(sid);
	if (session >= sc->sc_nsessions)
		return EINVAL;

	memset(&sc->sc_sessions[session], 0, sizeof(sc->sc_sessions[session]));
	return (0);
}

static int
mvcesa_process(void *arg, struct cryptop *crp, int hint)
{
	struct mvcesa_softc *sc = (struct mvcesa_softc *)arg;
	struct mvcesa_session *ses;
	struct cryptodesc *crd;
	struct mbuf *m = NULL;
	struct uio *uio = NULL;
	int session;
	char *buf = NULL;

	KASSERT(sc != NULL /*, ("mvcesa_process: null softc")*/);

	if (crp == NULL)
		return EINVAL;
	if (crp->crp_callback == NULL || sc == NULL) {
		crp->crp_etype = EINVAL;
		goto done;
	}

	session = MVCESA_SESSION(crp->crp_sid);
	if (session >= sc->sc_nsessions) {
		crp->crp_etype = ENOENT;
		goto done;
	}
	ses = &sc->sc_sessions[session];

	if (crp->crp_flags & CRYPTO_F_IMBUF)
		m = (struct mbuf *)crp->crp_buf;
	else if (crp->crp_flags & CRYPTO_F_IOV)
		uio = (struct uio *)crp->crp_buf;
	else
		buf = (char *)crp->crp_buf;

	if (0 /* DMA support */) {
		/* not yet... */

		goto done;
	}

	/* PIO operation */

	for (crd = crp->crp_desc; crd; crd = crd->crd_next) {
		switch (crd->crd_alg) {
		case CRYPTO_DES_CBC:
		case CRYPTO_3DES_CBC:
		{
			uint32_t alg, mode, dir, *iv, ivbuf[2];

			mode = MVCESA_DESE_C_DESMODE_CBC;
			if (crd->crd_alg == CRYPTO_DES_CBC)
				alg = MVCESA_DESE_C_ALGORITHM_DES;
			else {	/* CRYPTO_3DES_CBC */
				alg = MVCESA_DESE_C_ALGORITHM_3DES;
				mode |= MVCESA_DESE_C_3DESMODE_EDE;
			}
			if (crd->crd_flags & CRD_F_ENCRYPT) {
				dir = MVCESA_DESE_C_DIRECTION_ENC;
				if (crd->crd_flags & CRD_F_IV_EXPLICIT)
					iv = (uint32_t *)crd->crd_iv;
				else
					iv = ses->ses_iv;
				if (!(crd->crd_flags & CRD_F_IV_PRESENT)) {
					if (m != NULL)
						m_copyback(m, crd->crd_inject,
						    8, iv);
					else if (uio != NULL)
						cuio_copyback(uio,
						    crd->crd_inject, 8, iv);
				}
			} else {
				dir = MVCESA_DESE_C_DIRECTION_DEC;
				if (crd->crd_flags & CRD_F_IV_EXPLICIT)
					iv = (uint32_t *)crd->crd_iv;
				else {
					if (m != NULL)
						m_copydata(m, crd->crd_inject,
						    8, ivbuf);
					else if (uio != NULL)
						cuio_copydata(uio,
						    crd->crd_inject, 8, ivbuf);
					iv = ivbuf;
				}
			}

			crp->crp_etype = mvcesa_des_encdec(sc, ses,
			    alg, mode, dir, iv, crd->crd_skip, crd->crd_len,
			    buf, m, uio);
			break;
		}

		case CRYPTO_SHA1:
		case CRYPTO_SHA1_HMAC:
		case CRYPTO_MD5:
		case CRYPTO_MD5_HMAC:
		{
			uint64_t bits;
			uint32_t alg, *iv = NULL, digest[512 / 8 / 4], dlen;

			if (crd->crd_alg == CRYPTO_SHA1 ||
			    crd->crd_alg == CRYPTO_SHA1_HMAC) {
				alg = MVCESA_SHA1MD5I_AC_ALGORITHM_SHA1;
				dlen = 160;
			} else {	/* CRYPTO_MD5 || CRYPTO_MD5_HMAC */
				alg = MVCESA_SHA1MD5I_AC_ALGORITHM_MD5;
				dlen = 128;
			}
			bits = crd->crd_len << 3;
			if (crd->crd_alg == CRYPTO_SHA1_HMAC ||
			    crd->crd_alg == CRYPTO_MD5_HMAC) {
				iv = ses->ses_hminner;
				bits += 512;
			}

			crp->crp_etype = mvcesa_authentication(sc, ses,
			    alg, iv, digest, bits, crd->crd_skip, crd->crd_len,
			    buf, m, uio);
			if (crp->crp_etype != 0)
				break;

			if (crd->crd_alg == CRYPTO_SHA1_HMAC ||
			    crd->crd_alg == CRYPTO_MD5_HMAC)
				crp->crp_etype = mvcesa_authentication(sc,
				    ses, alg, ses->ses_hmouter, digest,
				    512 + dlen, 0, dlen, (char *)digest, NULL,
				    NULL);
			if (crp->crp_etype != 0)
				break;

			/* Inject the authentication data */
			if (buf != NULL)
				memcpy(buf + crd->crd_inject, digest, dlen / 8);
			else if (m != NULL)
				m_copyback(m, crd->crd_inject, dlen / 8,
				    digest);
			else if (uio != NULL)
				memcpy(crp->crp_mac, digest, dlen / 8);
		}
		}
		if (crp->crp_etype != 0)
			break;
	}

done:
	DPRINTF(("request %08x done\n", (uint32_t)crp));
	crypto_done(crp);
	return 0;
}


static int
mvcesa_authentication(struct mvcesa_softc *sc, struct mvcesa_session *ses,
		      uint32_t alg, uint32_t *iv, uint32_t *digest,
		      uint64_t bits, int skip, int len, char *buf,
		      struct mbuf *m, struct uio *uio)
{
	uint32_t cmd, bswp, data = 0;
	int dlen, off, i, s;

	/*
	 * SHA/MD5 algorithms work in 512-bit chunks, equal to 16 words.
	 */

	KASSERT(!(len & (512 - 1)) || bits != 0);
	KASSERT(buf != NULL || m != NULL || uio != NULL);

	cmd = bus_space_read_4(sc->sc_iot, sc->sc_ioh, MVCESA_SHA1MD5I_AC);
	if (!(cmd & MVCESA_SHA1MD5I_AC_TERMINATION))
		return ERESTART;

	bswp = 0;
	if (alg == MVCESA_SHA1MD5I_AC_ALGORITHM_SHA1) {
		dlen = 160;
		bits = htobe64(bits);
#if BYTE_ORDER == LITTLE_ENDIAN
		bswp = MVCESA_SHA1MD5I_AC_DATABYTESWAP |
		    MVCESA_SHA1MD5I_AC_IVBYTESWAP;
#endif
	} else {	/* MVCESA_SHA1MD5I_AC_ALGORITHM_MD5 */
		dlen = 128;
		bits = htole64(bits);
#if BYTE_ORDER == BIG_ENDIAN
		bswp = MVCESA_SHA1MD5I_AC_DATABYTESWAP |
		    MVCESA_SHA1MD5I_AC_IVBYTESWAP;
#endif
	}
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVCESA_SHA1MD5I_AC,
	    alg | bswp | MVCESA_SHA1MD5I_AC_MODE_USEIV);

	if (iv != NULL)
		bus_space_write_region_4(sc->sc_iot, sc->sc_ioh,
		    MVCESA_SHA1MD5I_IVDA, iv, dlen / 4);

	off = i = 0;
	while (1 /* CONSTCOND */) {
		data = 0;
		if (buf != NULL)
			for (i = 0; i < 512 / 8 && off + i < len; i += s) {
				s = min(sizeof(data), len - off - i);
				memcpy(&data, buf + skip + off + i, s);
				if (s == sizeof(data))
					bus_space_write_4(sc->sc_iot,
					    sc->sc_ioh, MVCESA_SHA1MD5I_DI,
					    data);
			}
		else if (m != NULL)
			for (i = 0; i < 512 / 8 && off + i < len; i += s) {
				s = min(sizeof(data), len - off - i);
				m_copydata(m, skip + off + i, s, &data);
				if (s == sizeof(data))
					bus_space_write_4(sc->sc_iot,
					    sc->sc_ioh, MVCESA_SHA1MD5I_DI,
					    data);
			}
		else if (uio != NULL)
			for (i = 0; i < 512 / 8 && off + i < len; i += s) {
				s = min(sizeof(data), len - off - i);
				cuio_copydata(uio, skip + off + i, s, &data);
				if (s == sizeof(data))
					bus_space_write_4(sc->sc_iot,
					    sc->sc_ioh, MVCESA_SHA1MD5I_DI,
					    data);
			}

		off += i;
		if (i < 512 / 8)
			break;

		do {
			cmd = bus_space_read_4(sc->sc_iot, sc->sc_ioh,
			    MVCESA_SHA1MD5I_AC);
		} while (!(cmd & MVCESA_SHA1MD5I_AC_TERMINATION));

		bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVCESA_SHA1MD5I_AC,
		    alg | bswp | MVCESA_SHA1MD5I_AC_MODE_CONTINUE);
	}

	if (i < 512 / 8) {
		*((char *)&data + (i % 4)) = 0x80;
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVCESA_SHA1MD5I_DI,
		    data);
		i = (i & ~3) + 4;

		/* Do pad to 512 bits, if chunk size is more than 448 bits. */
		if (i > 448 / 8) {
			for (; i < 512 / 8; i += 4)
				bus_space_write_4(sc->sc_iot, sc->sc_ioh,
				    MVCESA_SHA1MD5I_DI, 0);
			do {
				cmd = bus_space_read_4(sc->sc_iot, sc->sc_ioh,
				    MVCESA_SHA1MD5I_AC);
			} while (!(cmd & MVCESA_SHA1MD5I_AC_TERMINATION));
			i = 0;
		}
		for (; i < 448 / 8; i += 4)
			bus_space_write_4(sc->sc_iot, sc->sc_ioh,
			    MVCESA_SHA1MD5I_DI, 0);

		/* Set total bits */
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVCESA_SHA1MD5I_BCL,
		    bits & 0xffffffff);
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVCESA_SHA1MD5I_BCH,
		    bits >> 32);
		do {
			cmd = bus_space_read_4(sc->sc_iot, sc->sc_ioh,
			    MVCESA_SHA1MD5I_AC);
		} while (!(cmd & MVCESA_SHA1MD5I_AC_TERMINATION));
	}

	if (digest != NULL) {
		/* Read digest */
		bus_space_read_region_4(sc->sc_iot, sc->sc_ioh,
		    MVCESA_SHA1MD5I_IVDA, digest, dlen / 8 / 4);
#if BYTE_ORDER == LITTLE_ENDIAN
		if (alg == MVCESA_SHA1MD5I_AC_ALGORITHM_SHA1)
			for (i = 0; i < dlen / 8 / 4; i++)
				digest[i] = be32toh(digest[i]);
#else
		if (alg == MVCESA_SHA1MD5I_AC_ALGORITHM_MD5)
			for (i = 0; i < dlen / 8 / 4; i++)
				digest[i] = le32toh(digest[i]);
#endif
	}
	return 0;
}

static int
mvcesa_des_encdec(struct mvcesa_softc *sc, struct mvcesa_session *ses,
		  uint32_t alg, uint32_t mode, uint32_t dir, uint32_t *iv,
		  int skip, int len, char *buf, struct mbuf *m, struct uio *uio)
{
	uint64_t iblk, oblk;
	uint32_t cmd, bswp = 0;
	int i, o, s;

	KASSERT(buf != NULL || m != NULL || uio != NULL);

	cmd = bus_space_read_4(sc->sc_iot, sc->sc_ioh, MVCESA_DESE_C);
	if (!(cmd & MVCESA_DESE_C_TERMINATION))
		return ERESTART;

#if BYTE_ORDER == LITTLE_ENDIAN
	bswp = MVCESA_DESE_C_DATABYTESWAP | MVCESA_DESE_C_IVBYTESWAP   |
	    MVCESA_DESE_C_OUTBYTESWAP;
#endif
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVCESA_DESE_C,
	    dir | alg | mode | bswp | MVCESA_DESE_C_ALLTERMINATION);

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVCESA_DESE_K0L,
	    ses->ses_key[1]);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVCESA_DESE_K0H,
	    ses->ses_key[0]);
	if (alg == MVCESA_DESE_C_ALGORITHM_3DES) {
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVCESA_DESE_K1L,
		    ses->ses_key[3]);
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVCESA_DESE_K1H,
		    ses->ses_key[2]);
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVCESA_DESE_K2L,
		    ses->ses_key[5]);
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVCESA_DESE_K2H,
		    ses->ses_key[4]);
	}

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVCESA_DESE_IVL, iv[1]);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVCESA_DESE_IVH, iv[0]);

	i = o = 0;
	while (i < len) {
		s = min(sizeof(iblk), len - i);
		iblk = 0;

		if (buf != NULL)
			memcpy(&iblk, buf + skip + i, s);
		else if (m != NULL)
			m_copydata(m, skip + i, s, &iblk);
		else if (uio != NULL)
			cuio_copydata(uio, skip + i, s, &iblk);

		/*
		 * We have the pipeline that two data enters.
		 */

		while (1 /* CONSTCOND */) {
			cmd = bus_space_read_4(sc->sc_iot, sc->sc_ioh,
			    MVCESA_DESE_C);
			if (cmd & MVCESA_DESE_C_ALLTERMINATION)
				/* Engine is ready.  Can write two data. */
				break;
			if (cmd & MVCESA_DESE_C_READALLOW) {
				oblk = bus_space_read_4(sc->sc_iot, sc->sc_ioh,
				    MVCESA_DESE_DOH);
				/* XXXX: needs barrier? */
				oblk |= (uint64_t)bus_space_read_4(sc->sc_iot,
				    sc->sc_ioh, MVCESA_DESE_DOL) << 32;

				if (buf != NULL)
					memcpy(buf + skip + o, &oblk,
					    sizeof(oblk));
				else if (m != NULL)
					m_copydata(m, skip + o, sizeof(oblk),
					    &oblk);
				else if (uio != NULL)
					cuio_copyback(uio, skip + o,
					    sizeof(oblk), &oblk);
				o += sizeof(oblk);

				/* Can write one data */
				break;
			}
		}

		/*
		 * Encryption/Decription calculation time is 9 cycles in DES
		 * mode and 25 cycles in 3DES mode.
		 */
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVCESA_DESE_DBL,
		    iblk >> 32);
		/* XXXX: needs barrier? */
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, MVCESA_DESE_DBH,
		    iblk & 0xffffffff);
		i += s;
	}

	while (1 /* CONSTCOND */) {
		cmd = bus_space_read_4(sc->sc_iot, sc->sc_ioh,
		    MVCESA_DESE_C);
		if (cmd & (MVCESA_DESE_C_READALLOW |
					MVCESA_DESE_C_ALLTERMINATION)) {
			oblk = bus_space_read_4(sc->sc_iot, sc->sc_ioh,
			    MVCESA_DESE_DOH);
			/* XXXX: needs barrier? */
			oblk |= (uint64_t)bus_space_read_4(sc->sc_iot,
			    sc->sc_ioh, MVCESA_DESE_DOL) << 32;

			if (cmd & MVCESA_DESE_C_ALLTERMINATION) {
				/* We can read IV from Data Out Registers. */
				if (dir == MVCESA_DESE_C_DIRECTION_ENC)
					o -= sizeof(oblk);
				else
					break;
			}
			if (buf != NULL)
				memcpy(buf + skip + o, &oblk, sizeof(oblk));
			else if (m != NULL)
				m_copydata(m, skip + o, sizeof(oblk), &oblk);
			else if (uio != NULL)
				cuio_copyback(uio, skip + o, sizeof(oblk),
				    &oblk);
			o += sizeof(oblk);
			if (cmd & MVCESA_DESE_C_ALLTERMINATION)
				break;
		}
	}

	if (dir == MVCESA_DESE_C_DIRECTION_ENC)
		memcpy(ses->ses_iv, iv, sizeof(ses->ses_iv));

	return 0;
}
