/*	$NetBSD: cryptosoft.c,v 1.47 2015/08/20 14:40:19 christos Exp $ */
/*	$FreeBSD: src/sys/opencrypto/cryptosoft.c,v 1.2.2.1 2002/11/21 23:34:23 sam Exp $	*/
/*	$OpenBSD: cryptosoft.c,v 1.35 2002/04/26 08:43:50 deraadt Exp $	*/

/*
 * The author of this code is Angelos D. Keromytis (angelos@cis.upenn.edu)
 *
 * This code was written by Angelos D. Keromytis in Athens, Greece, in
 * February 2000. Network Security Technologies Inc. (NSTI) kindly
 * supported the development of this code.
 *
 * Copyright (c) 2000, 2001 Angelos D. Keromytis
 *
 * Permission to use, copy, and modify this software with or without fee
 * is hereby granted, provided that this entire notice is included in
 * all source code copies of any software which is or includes a copy or
 * modification of this software.
 *
 * THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTY. IN PARTICULAR, NONE OF THE AUTHORS MAKES ANY
 * REPRESENTATION OR WARRANTY OF ANY KIND CONCERNING THE
 * MERCHANTABILITY OF THIS SOFTWARE OR ITS FITNESS FOR ANY PARTICULAR
 * PURPOSE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: cryptosoft.c,v 1.47 2015/08/20 14:40:19 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/sysctl.h>
#include <sys/errno.h>
#include <sys/cprng.h>
#include <sys/module.h>
#include <sys/device.h>

#ifdef _KERNEL_OPT
#include "opt_ocf.h"
#endif

#include <opencrypto/cryptodev.h>
#include <opencrypto/cryptosoft.h>
#include <opencrypto/xform.h>

#include <opencrypto/cryptosoft_xform.c>

#include "ioconf.h"

union authctx {
	MD5_CTX md5ctx;
	SHA1_CTX sha1ctx;
	RMD160_CTX rmd160ctx;
	SHA256_CTX sha256ctx;
	SHA384_CTX sha384ctx;
	SHA512_CTX sha512ctx;
	aesxcbc_ctx aesxcbcctx;
	AES_GMAC_CTX aesgmacctx;
};

struct swcr_data **swcr_sessions = NULL;
u_int32_t swcr_sesnum = 0;
int32_t swcr_id = -1;

#define COPYBACK(x, a, b, c, d) \
	(x) == CRYPTO_BUF_MBUF ? m_copyback((struct mbuf *)a,b,c,d) \
	: cuio_copyback((struct uio *)a,b,c,d)
#define COPYDATA(x, a, b, c, d) \
	(x) == CRYPTO_BUF_MBUF ? m_copydata((struct mbuf *)a,b,c,d) \
	: cuio_copydata((struct uio *)a,b,c,d)

static	int swcr_encdec(struct cryptodesc *, const struct swcr_data *, void *, int);
static	int swcr_compdec(struct cryptodesc *, const struct swcr_data *, void *, int, int *);
static	int swcr_combined(struct cryptop *, int);
static	int swcr_process(void *, struct cryptop *, int);
static	int swcr_newsession(void *, u_int32_t *, struct cryptoini *);
static	int swcr_freesession(void *, u_int64_t);

/*
 * Apply a symmetric encryption/decryption algorithm.
 */
static int
swcr_encdec(struct cryptodesc *crd, const struct swcr_data *sw, void *bufv,
    int outtype)
{
	char *buf = bufv;
	unsigned char iv[EALG_MAX_BLOCK_LEN], blk[EALG_MAX_BLOCK_LEN], *idat;
	unsigned char *ivp, piv[EALG_MAX_BLOCK_LEN];
	const struct swcr_enc_xform *exf;
	int i, k, j, blks, ivlen;
	int count, ind;

	exf = sw->sw_exf;
	blks = exf->enc_xform->blocksize;
	ivlen = exf->enc_xform->ivsize;
	KASSERT(exf->reinit ? ivlen <= blks : ivlen == blks);

	/* Check for non-padded data */
	if (crd->crd_len % blks)
		return EINVAL;

	/* Initialize the IV */
	if (crd->crd_flags & CRD_F_ENCRYPT) {
		/* IV explicitly provided ? */
		if (crd->crd_flags & CRD_F_IV_EXPLICIT) {
			memcpy(iv, crd->crd_iv, ivlen);
			if (exf->reinit)
				exf->reinit(sw->sw_kschedule, iv, 0);
		} else if (exf->reinit) {
			exf->reinit(sw->sw_kschedule, 0, iv);
		} else {
			/* Get random IV */
			for (i = 0;
			    i + sizeof (u_int32_t) <= EALG_MAX_BLOCK_LEN;
			    i += sizeof (u_int32_t)) {
				u_int32_t temp = cprng_fast32();

				memcpy(iv + i, &temp, sizeof(u_int32_t));
			}
			/*
			 * What if the block size is not a multiple
			 * of sizeof (u_int32_t), which is the size of
			 * what arc4random() returns ?
			 */
			if (EALG_MAX_BLOCK_LEN % sizeof (u_int32_t) != 0) {
				u_int32_t temp = cprng_fast32();

				bcopy (&temp, iv + i,
				    EALG_MAX_BLOCK_LEN - i);
			}
		}

		/* Do we need to write the IV */
		if (!(crd->crd_flags & CRD_F_IV_PRESENT)) {
			COPYBACK(outtype, buf, crd->crd_inject, ivlen, iv);
		}

	} else {	/* Decryption */
			/* IV explicitly provided ? */
		if (crd->crd_flags & CRD_F_IV_EXPLICIT)
			memcpy(iv, crd->crd_iv, ivlen);
		else {
			/* Get IV off buf */
			COPYDATA(outtype, buf, crd->crd_inject, ivlen, iv);
		}
		if (exf->reinit)
			exf->reinit(sw->sw_kschedule, iv, 0);
	}

	ivp = iv;

	if (outtype == CRYPTO_BUF_CONTIG) {
		if (exf->reinit) {
			for (i = crd->crd_skip;
			     i < crd->crd_skip + crd->crd_len; i += blks) {
				if (crd->crd_flags & CRD_F_ENCRYPT) {
					exf->encrypt(sw->sw_kschedule, buf + i);
				} else {
					exf->decrypt(sw->sw_kschedule, buf + i);
				}
			}
		} else if (crd->crd_flags & CRD_F_ENCRYPT) {
			for (i = crd->crd_skip;
			    i < crd->crd_skip + crd->crd_len; i += blks) {
				/* XOR with the IV/previous block, as appropriate. */
				if (i == crd->crd_skip)
					for (k = 0; k < blks; k++)
						buf[i + k] ^= ivp[k];
				else
					for (k = 0; k < blks; k++)
						buf[i + k] ^= buf[i + k - blks];
				exf->encrypt(sw->sw_kschedule, buf + i);
			}
		} else {		/* Decrypt */
			/*
			 * Start at the end, so we don't need to keep the encrypted
			 * block as the IV for the next block.
			 */
			for (i = crd->crd_skip + crd->crd_len - blks;
			    i >= crd->crd_skip; i -= blks) {
				exf->decrypt(sw->sw_kschedule, buf + i);

				/* XOR with the IV/previous block, as appropriate */
				if (i == crd->crd_skip)
					for (k = 0; k < blks; k++)
						buf[i + k] ^= ivp[k];
				else
					for (k = 0; k < blks; k++)
						buf[i + k] ^= buf[i + k - blks];
			}
		}

		return 0;
	} else if (outtype == CRYPTO_BUF_MBUF) {
		struct mbuf *m = (struct mbuf *) buf;

		/* Find beginning of data */
		m = m_getptr(m, crd->crd_skip, &k);
		if (m == NULL)
			return EINVAL;

		i = crd->crd_len;

		while (i > 0) {
			/*
			 * If there's insufficient data at the end of
			 * an mbuf, we have to do some copying.
			 */
			if (m->m_len < k + blks && m->m_len != k) {
				m_copydata(m, k, blks, blk);

				/* Actual encryption/decryption */
				if (exf->reinit) {
					if (crd->crd_flags & CRD_F_ENCRYPT) {
						exf->encrypt(sw->sw_kschedule,
							     blk);
					} else {
						exf->decrypt(sw->sw_kschedule,
							     blk);
					}
				} else if (crd->crd_flags & CRD_F_ENCRYPT) {
					/* XOR with previous block */
					for (j = 0; j < blks; j++)
						blk[j] ^= ivp[j];

					exf->encrypt(sw->sw_kschedule, blk);

					/*
					 * Keep encrypted block for XOR'ing
					 * with next block
					 */
					memcpy(iv, blk, blks);
					ivp = iv;
				} else {	/* decrypt */
					/*
					 * Keep encrypted block for XOR'ing
					 * with next block
					 */
					if (ivp == iv)
						memcpy(piv, blk, blks);
					else
						memcpy(iv, blk, blks);

					exf->decrypt(sw->sw_kschedule, blk);

					/* XOR with previous block */
					for (j = 0; j < blks; j++)
						blk[j] ^= ivp[j];

					if (ivp == iv)
						memcpy(iv, piv, blks);
					else
						ivp = iv;
				}

				/* Copy back decrypted block */
				m_copyback(m, k, blks, blk);

				/* Advance pointer */
				m = m_getptr(m, k + blks, &k);
				if (m == NULL)
					return EINVAL;

				i -= blks;

				/* Could be done... */
				if (i == 0)
					break;
			}

			/* Skip possibly empty mbufs */
			if (k == m->m_len) {
				for (m = m->m_next; m && m->m_len == 0;
				    m = m->m_next)
					;
				k = 0;
			}

			/* Sanity check */
			if (m == NULL)
				return EINVAL;

			/*
			 * Warning: idat may point to garbage here, but
			 * we only use it in the while() loop, only if
			 * there are indeed enough data.
			 */
			idat = mtod(m, unsigned char *) + k;

			while (m->m_len >= k + blks && i > 0) {
				if (exf->reinit) {
					if (crd->crd_flags & CRD_F_ENCRYPT) {
						exf->encrypt(sw->sw_kschedule,
							     idat);
					} else {
						exf->decrypt(sw->sw_kschedule,
							     idat);
					}
				} else if (crd->crd_flags & CRD_F_ENCRYPT) {
					/* XOR with previous block/IV */
					for (j = 0; j < blks; j++)
						idat[j] ^= ivp[j];

					exf->encrypt(sw->sw_kschedule, idat);
					ivp = idat;
				} else {	/* decrypt */
					/*
					 * Keep encrypted block to be used
					 * in next block's processing.
					 */
					if (ivp == iv)
						memcpy(piv, idat, blks);
					else
						memcpy(iv, idat, blks);

					exf->decrypt(sw->sw_kschedule, idat);

					/* XOR with previous block/IV */
					for (j = 0; j < blks; j++)
						idat[j] ^= ivp[j];

					if (ivp == iv)
						memcpy(iv, piv, blks);
					else
						ivp = iv;
				}

				idat += blks;
				k += blks;
				i -= blks;
			}
		}

		return 0; /* Done with mbuf encryption/decryption */
	} else if (outtype == CRYPTO_BUF_IOV) {
		struct uio *uio = (struct uio *) buf;

		/* Find beginning of data */
		count = crd->crd_skip;
		ind = cuio_getptr(uio, count, &k);
		if (ind == -1)
			return EINVAL;

		i = crd->crd_len;

		while (i > 0) {
			/*
			 * If there's insufficient data at the end,
			 * we have to do some copying.
			 */
			if (uio->uio_iov[ind].iov_len < k + blks &&
			    uio->uio_iov[ind].iov_len != k) {
				cuio_copydata(uio, k, blks, blk);

				/* Actual encryption/decryption */
				if (exf->reinit) {
					if (crd->crd_flags & CRD_F_ENCRYPT) {
						exf->encrypt(sw->sw_kschedule,
							     blk);
					} else {
						exf->decrypt(sw->sw_kschedule,
							     blk);
					}
				} else if (crd->crd_flags & CRD_F_ENCRYPT) {
					/* XOR with previous block */
					for (j = 0; j < blks; j++)
						blk[j] ^= ivp[j];

					exf->encrypt(sw->sw_kschedule, blk);

					/*
					 * Keep encrypted block for XOR'ing
					 * with next block
					 */
					memcpy(iv, blk, blks);
					ivp = iv;
				} else {	/* decrypt */
					/*
					 * Keep encrypted block for XOR'ing
					 * with next block
					 */
					if (ivp == iv)
						memcpy(piv, blk, blks);
					else
						memcpy(iv, blk, blks);

					exf->decrypt(sw->sw_kschedule, blk);

					/* XOR with previous block */
					for (j = 0; j < blks; j++)
						blk[j] ^= ivp[j];

					if (ivp == iv)
						memcpy(iv, piv, blks);
					else
						ivp = iv;
				}

				/* Copy back decrypted block */
				cuio_copyback(uio, k, blks, blk);

				count += blks;

				/* Advance pointer */
				ind = cuio_getptr(uio, count, &k);
				if (ind == -1)
					return (EINVAL);

				i -= blks;

				/* Could be done... */
				if (i == 0)
					break;
			}

			/*
			 * Warning: idat may point to garbage here, but
			 * we only use it in the while() loop, only if
			 * there are indeed enough data.
			 */
			idat = ((char *)uio->uio_iov[ind].iov_base) + k;

			while (uio->uio_iov[ind].iov_len >= k + blks &&
			    i > 0) {
				if (exf->reinit) {
					if (crd->crd_flags & CRD_F_ENCRYPT) {
						exf->encrypt(sw->sw_kschedule,
							    idat);
					} else {
						exf->decrypt(sw->sw_kschedule,
							    idat);
					}
				} else if (crd->crd_flags & CRD_F_ENCRYPT) {
					/* XOR with previous block/IV */
					for (j = 0; j < blks; j++)
						idat[j] ^= ivp[j];

					exf->encrypt(sw->sw_kschedule, idat);
					ivp = idat;
				} else {	/* decrypt */
					/*
					 * Keep encrypted block to be used
					 * in next block's processing.
					 */
					if (ivp == iv)
						memcpy(piv, idat, blks);
					else
						memcpy(iv, idat, blks);

					exf->decrypt(sw->sw_kschedule, idat);

					/* XOR with previous block/IV */
					for (j = 0; j < blks; j++)
						idat[j] ^= ivp[j];

					if (ivp == iv)
						memcpy(iv, piv, blks);
					else
						ivp = iv;
				}

				idat += blks;
				count += blks;
				k += blks;
				i -= blks;
			}
		}
		return 0; /* Done with mbuf encryption/decryption */
	}

	/* Unreachable */
	return EINVAL;
}

/*
 * Compute keyed-hash authenticator.
 */
int
swcr_authcompute(struct cryptop *crp, struct cryptodesc *crd,
    const struct swcr_data *sw, void *buf, int outtype)
{
	unsigned char aalg[AALG_MAX_RESULT_LEN];
	const struct swcr_auth_hash *axf;
	union authctx ctx;
	int err;

	if (sw->sw_ictx == 0)
		return EINVAL;

	axf = sw->sw_axf;

	memcpy(&ctx, sw->sw_ictx, axf->ctxsize);

	switch (outtype) {
	case CRYPTO_BUF_CONTIG:
		axf->Update(&ctx, (char *)buf + crd->crd_skip, crd->crd_len);
		break;
	case CRYPTO_BUF_MBUF:
		err = m_apply((struct mbuf *) buf, crd->crd_skip, crd->crd_len,
		    (int (*)(void*, void *, unsigned int)) axf->Update,
		    (void *) &ctx);
		if (err)
			return err;
		break;
	case CRYPTO_BUF_IOV:
		err = cuio_apply((struct uio *) buf, crd->crd_skip,
		    crd->crd_len,
		    (int (*)(void *, void *, unsigned int)) axf->Update,
		    (void *) &ctx);
		if (err) {
			return err;
		}
		break;
	default:
		return EINVAL;
	}

	switch (sw->sw_alg) {
	case CRYPTO_MD5_HMAC:
	case CRYPTO_MD5_HMAC_96:
	case CRYPTO_SHA1_HMAC:
	case CRYPTO_SHA1_HMAC_96:
	case CRYPTO_SHA2_256_HMAC:
	case CRYPTO_SHA2_384_HMAC:
	case CRYPTO_SHA2_512_HMAC:
	case CRYPTO_RIPEMD160_HMAC:
	case CRYPTO_RIPEMD160_HMAC_96:
		if (sw->sw_octx == NULL)
			return EINVAL;

		axf->Final(aalg, &ctx);
		memcpy(&ctx, sw->sw_octx, axf->ctxsize);
		axf->Update(&ctx, aalg, axf->auth_hash->hashsize);
		axf->Final(aalg, &ctx);
		break;

	case CRYPTO_MD5_KPDK:
	case CRYPTO_SHA1_KPDK:
		if (sw->sw_octx == NULL)
			return EINVAL;

		axf->Update(&ctx, sw->sw_octx, sw->sw_klen);
		axf->Final(aalg, &ctx);
		break;

	case CRYPTO_NULL_HMAC:
	case CRYPTO_MD5:
	case CRYPTO_SHA1:
	case CRYPTO_AES_XCBC_MAC_96:
		axf->Final(aalg, &ctx);
		break;
	}

	/* Inject the authentication data */
	switch (outtype) {
	case CRYPTO_BUF_CONTIG:
		(void)memcpy((char *)buf + crd->crd_inject, aalg,
		    axf->auth_hash->authsize);
		break;
	case CRYPTO_BUF_MBUF:
		m_copyback((struct mbuf *) buf, crd->crd_inject,
		    axf->auth_hash->authsize, aalg);
		break;
	case CRYPTO_BUF_IOV:
		memcpy(crp->crp_mac, aalg, axf->auth_hash->authsize);
		break;
	default:
		return EINVAL;
	}
	return 0;
}

/*
 * Apply a combined encryption-authentication transformation
 */
static int
swcr_combined(struct cryptop *crp, int outtype)
{
	uint32_t blkbuf[howmany(EALG_MAX_BLOCK_LEN, sizeof(uint32_t))];
	u_char *blk = (u_char *)blkbuf;
	u_char aalg[AALG_MAX_RESULT_LEN];
	u_char iv[EALG_MAX_BLOCK_LEN];
	union authctx ctx;
	struct cryptodesc *crd, *crda = NULL, *crde = NULL;
	struct swcr_data *sw, *swa, *swe = NULL;
	const struct swcr_auth_hash *axf = NULL;
	const struct swcr_enc_xform *exf = NULL;
	void *buf = (void *)crp->crp_buf;
	uint32_t *blkp;
	int i, blksz = 0, ivlen = 0, len;

	for (crd = crp->crp_desc; crd; crd = crd->crd_next) {
		for (sw = swcr_sessions[crp->crp_sid & 0xffffffff];
		     sw && sw->sw_alg != crd->crd_alg;
		     sw = sw->sw_next)
			;
		if (sw == NULL)
			return (EINVAL);

		switch (sw->sw_alg) {
		case CRYPTO_AES_GCM_16:
		case CRYPTO_AES_GMAC:
			swe = sw;
			crde = crd;
			exf = swe->sw_exf;
			ivlen = exf->enc_xform->ivsize;
			break;
		case CRYPTO_AES_128_GMAC:
		case CRYPTO_AES_192_GMAC:
		case CRYPTO_AES_256_GMAC:
			swa = sw;
			crda = crd;
			axf = swa->sw_axf;
			if (swa->sw_ictx == 0)
				return (EINVAL);
			memcpy(&ctx, swa->sw_ictx, axf->ctxsize);
			blksz = axf->auth_hash->blocksize;
			break;
		default:
			return (EINVAL);
		}
	}
	if (crde == NULL || crda == NULL)
		return (EINVAL);
	if (outtype == CRYPTO_BUF_CONTIG)
		return (EINVAL);

	/* Initialize the IV */
	if (crde->crd_flags & CRD_F_ENCRYPT) {
		/* IV explicitly provided ? */
		if (crde->crd_flags & CRD_F_IV_EXPLICIT) {
			memcpy(iv, crde->crd_iv, ivlen);
			if (exf->reinit)
				exf->reinit(swe->sw_kschedule, iv, 0);
		} else if (exf->reinit)
			exf->reinit(swe->sw_kschedule, 0, iv);
		else
			cprng_fast(iv, ivlen);

		/* Do we need to write the IV */
		if (!(crde->crd_flags & CRD_F_IV_PRESENT))
			COPYBACK(outtype, buf, crde->crd_inject, ivlen, iv);

	} else {	/* Decryption */
			/* IV explicitly provided ? */
		if (crde->crd_flags & CRD_F_IV_EXPLICIT)
			memcpy(iv, crde->crd_iv, ivlen);
		else {
			/* Get IV off buf */
			COPYDATA(outtype, buf, crde->crd_inject, ivlen, iv);
		}
		if (exf->reinit)
			exf->reinit(swe->sw_kschedule, iv, 0);
	}

	/* Supply MAC with IV */
	if (axf->Reinit)
		axf->Reinit(&ctx, iv, ivlen);

	/* Supply MAC with AAD */
	for (i = 0; i < crda->crd_len; i += blksz) {
		len = MIN(crda->crd_len - i, blksz);
		COPYDATA(outtype, buf, crda->crd_skip + i, len, blk);
		axf->Update(&ctx, blk, len);
	}

	/* Do encryption/decryption with MAC */
	for (i = 0; i < crde->crd_len; i += blksz) {
		len = MIN(crde->crd_len - i, blksz);
		if (len < blksz)
			memset(blk, 0, blksz);
		COPYDATA(outtype, buf, crde->crd_skip + i, len, blk);
		if (crde->crd_flags & CRD_F_ENCRYPT) {
			exf->encrypt(swe->sw_kschedule, blk);
			axf->Update(&ctx, blk, len);
		} else {
			axf->Update(&ctx, blk, len);
			exf->decrypt(swe->sw_kschedule, blk);
		}
		COPYBACK(outtype, buf, crde->crd_skip + i, len, blk);
	}

	/* Do any required special finalization */
	switch (crda->crd_alg) {
		case CRYPTO_AES_128_GMAC:
		case CRYPTO_AES_192_GMAC:
		case CRYPTO_AES_256_GMAC:
			/* length block */
			memset(blk, 0, blksz);
			blkp = (uint32_t *)blk + 1;
			*blkp = htobe32(crda->crd_len * 8);
			blkp = (uint32_t *)blk + 3;
			*blkp = htobe32(crde->crd_len * 8);
			axf->Update(&ctx, blk, blksz);
			break;
	}

	/* Finalize MAC */
	axf->Final(aalg, &ctx);

	/* Inject the authentication data */
	if (outtype == CRYPTO_BUF_MBUF)
		COPYBACK(outtype, buf, crda->crd_inject, axf->auth_hash->authsize, aalg);
	else
		memcpy(crp->crp_mac, aalg, axf->auth_hash->authsize);

	return (0);
}

/*
 * Apply a compression/decompression algorithm
 */
static int
swcr_compdec(struct cryptodesc *crd, const struct swcr_data *sw,
    void *buf, int outtype, int *res_size)
{
	u_int8_t *data, *out;
	const struct swcr_comp_algo *cxf;
	int adj;
	u_int32_t result;

	cxf = sw->sw_cxf;

	/* We must handle the whole buffer of data in one time
	 * then if there is not all the data in the mbuf, we must
	 * copy in a buffer.
	 */

	data = malloc(crd->crd_len, M_CRYPTO_DATA, M_NOWAIT);
	if (data == NULL)
		return (EINVAL);
	COPYDATA(outtype, buf, crd->crd_skip, crd->crd_len, data);

	if (crd->crd_flags & CRD_F_COMP)
		result = cxf->compress(data, crd->crd_len, &out);
	else
		result = cxf->decompress(data, crd->crd_len, &out,
					 *res_size);

	free(data, M_CRYPTO_DATA);
	if (result == 0)
		return EINVAL;

	/* Copy back the (de)compressed data. m_copyback is
	 * extending the mbuf as necessary.
	 */
	*res_size = (int)result;
	/* Check the compressed size when doing compression */
	if (crd->crd_flags & CRD_F_COMP &&
	    sw->sw_alg == CRYPTO_DEFLATE_COMP_NOGROW &&
	    result >= crd->crd_len) {
			/* Compression was useless, we lost time */
			free(out, M_CRYPTO_DATA);
			return 0;
	}

	COPYBACK(outtype, buf, crd->crd_skip, result, out);
	if (result < crd->crd_len) {
		adj = result - crd->crd_len;
		if (outtype == CRYPTO_BUF_MBUF) {
			adj = result - crd->crd_len;
			m_adj((struct mbuf *)buf, adj);
		}
		/* Don't adjust the iov_len, it breaks the kmem_free */
	}
	free(out, M_CRYPTO_DATA);
	return 0;
}

/*
 * Generate a new software session.
 */
static int
swcr_newsession(void *arg, u_int32_t *sid, struct cryptoini *cri)
{
	struct swcr_data **swd;
	const struct swcr_auth_hash *axf;
	const struct swcr_enc_xform *txf;
	const struct swcr_comp_algo *cxf;
	u_int32_t i;
	int k, error;

	if (sid == NULL || cri == NULL)
		return EINVAL;

	if (swcr_sessions) {
		for (i = 1; i < swcr_sesnum; i++)
			if (swcr_sessions[i] == NULL)
				break;
	} else
		i = 1;		/* NB: to silence compiler warning */

	if (swcr_sessions == NULL || i == swcr_sesnum) {
		if (swcr_sessions == NULL) {
			i = 1; /* We leave swcr_sessions[0] empty */
			swcr_sesnum = CRYPTO_SW_SESSIONS;
		} else
			swcr_sesnum *= 2;

		swd = malloc(swcr_sesnum * sizeof(struct swcr_data *),
		    M_CRYPTO_DATA, M_NOWAIT);
		if (swd == NULL) {
			/* Reset session number */
			if (swcr_sesnum == CRYPTO_SW_SESSIONS)
				swcr_sesnum = 0;
			else
				swcr_sesnum /= 2;
			return ENOBUFS;
		}

		memset(swd, 0, swcr_sesnum * sizeof(struct swcr_data *));

		/* Copy existing sessions */
		if (swcr_sessions) {
			memcpy(swd, swcr_sessions,
			    (swcr_sesnum / 2) * sizeof(struct swcr_data *));
			free(swcr_sessions, M_CRYPTO_DATA);
		}

		swcr_sessions = swd;
	}

	swd = &swcr_sessions[i];
	*sid = i;

	while (cri) {
		*swd = malloc(sizeof **swd, M_CRYPTO_DATA, M_NOWAIT);
		if (*swd == NULL) {
			swcr_freesession(NULL, i);
			return ENOBUFS;
		}
		memset(*swd, 0, sizeof(struct swcr_data));

		switch (cri->cri_alg) {
		case CRYPTO_DES_CBC:
			txf = &swcr_enc_xform_des;
			goto enccommon;
		case CRYPTO_3DES_CBC:
			txf = &swcr_enc_xform_3des;
			goto enccommon;
		case CRYPTO_BLF_CBC:
			txf = &swcr_enc_xform_blf;
			goto enccommon;
		case CRYPTO_CAST_CBC:
			txf = &swcr_enc_xform_cast5;
			goto enccommon;
		case CRYPTO_SKIPJACK_CBC:
			txf = &swcr_enc_xform_skipjack;
			goto enccommon;
		case CRYPTO_RIJNDAEL128_CBC:
			txf = &swcr_enc_xform_rijndael128;
			goto enccommon;
		case CRYPTO_CAMELLIA_CBC:
			txf = &swcr_enc_xform_camellia;
			goto enccommon;
		case CRYPTO_AES_CTR:
			txf = &swcr_enc_xform_aes_ctr;
			goto enccommon;
		case CRYPTO_AES_GCM_16:
			txf = &swcr_enc_xform_aes_gcm;
			goto enccommon;
		case CRYPTO_AES_GMAC:
			txf = &swcr_enc_xform_aes_gmac;
			goto enccommon;
		case CRYPTO_NULL_CBC:
			txf = &swcr_enc_xform_null;
			goto enccommon;
		enccommon:
			error = txf->setkey(&((*swd)->sw_kschedule),
					cri->cri_key, cri->cri_klen / 8);
			if (error) {
				swcr_freesession(NULL, i);
				return error;
			}
			(*swd)->sw_exf = txf;
			break;

		case CRYPTO_MD5_HMAC:
			axf = &swcr_auth_hash_hmac_md5;
			goto authcommon;
		case CRYPTO_MD5_HMAC_96:
			axf = &swcr_auth_hash_hmac_md5_96;
			goto authcommon;
		case CRYPTO_SHA1_HMAC:
			axf = &swcr_auth_hash_hmac_sha1;
			goto authcommon;
		case CRYPTO_SHA1_HMAC_96:
			axf = &swcr_auth_hash_hmac_sha1_96;
			goto authcommon;
		case CRYPTO_SHA2_256_HMAC:
			axf = &swcr_auth_hash_hmac_sha2_256;
			goto authcommon;
		case CRYPTO_SHA2_384_HMAC:
			axf = &swcr_auth_hash_hmac_sha2_384;
			goto authcommon;
		case CRYPTO_SHA2_512_HMAC:
			axf = &swcr_auth_hash_hmac_sha2_512;
			goto authcommon;
		case CRYPTO_NULL_HMAC:
			axf = &swcr_auth_hash_null;
			goto authcommon;
		case CRYPTO_RIPEMD160_HMAC:
			axf = &swcr_auth_hash_hmac_ripemd_160;
			goto authcommon;
		case CRYPTO_RIPEMD160_HMAC_96:
			axf = &swcr_auth_hash_hmac_ripemd_160_96;
			goto authcommon;	/* leave this for safety */
		authcommon:
			(*swd)->sw_ictx = malloc(axf->ctxsize,
			    M_CRYPTO_DATA, M_NOWAIT);
			if ((*swd)->sw_ictx == NULL) {
				swcr_freesession(NULL, i);
				return ENOBUFS;
			}

			(*swd)->sw_octx = malloc(axf->ctxsize,
			    M_CRYPTO_DATA, M_NOWAIT);
			if ((*swd)->sw_octx == NULL) {
				swcr_freesession(NULL, i);
				return ENOBUFS;
			}

			for (k = 0; k < cri->cri_klen / 8; k++)
				cri->cri_key[k] ^= HMAC_IPAD_VAL;

			axf->Init((*swd)->sw_ictx);
			axf->Update((*swd)->sw_ictx, cri->cri_key,
			    cri->cri_klen / 8);
			axf->Update((*swd)->sw_ictx, hmac_ipad_buffer,
			    axf->auth_hash->blocksize - (cri->cri_klen / 8));

			for (k = 0; k < cri->cri_klen / 8; k++)
				cri->cri_key[k] ^= (HMAC_IPAD_VAL ^ HMAC_OPAD_VAL);

			axf->Init((*swd)->sw_octx);
			axf->Update((*swd)->sw_octx, cri->cri_key,
			    cri->cri_klen / 8);
			axf->Update((*swd)->sw_octx, hmac_opad_buffer,
			    axf->auth_hash->blocksize - (cri->cri_klen / 8));

			for (k = 0; k < cri->cri_klen / 8; k++)
				cri->cri_key[k] ^= HMAC_OPAD_VAL;
			(*swd)->sw_axf = axf;
			break;

		case CRYPTO_MD5_KPDK:
			axf = &swcr_auth_hash_key_md5;
			goto auth2common;

		case CRYPTO_SHA1_KPDK:
			axf = &swcr_auth_hash_key_sha1;
		auth2common:
			(*swd)->sw_ictx = malloc(axf->ctxsize,
			    M_CRYPTO_DATA, M_NOWAIT);
			if ((*swd)->sw_ictx == NULL) {
				swcr_freesession(NULL, i);
				return ENOBUFS;
			}

			/* Store the key so we can "append" it to the payload */
			(*swd)->sw_octx = malloc(cri->cri_klen / 8, M_CRYPTO_DATA,
			    M_NOWAIT);
			if ((*swd)->sw_octx == NULL) {
				swcr_freesession(NULL, i);
				return ENOBUFS;
			}

			(*swd)->sw_klen = cri->cri_klen / 8;
			memcpy((*swd)->sw_octx, cri->cri_key, cri->cri_klen / 8);
			axf->Init((*swd)->sw_ictx);
			axf->Update((*swd)->sw_ictx, cri->cri_key,
			    cri->cri_klen / 8);
			axf->Final(NULL, (*swd)->sw_ictx);
			(*swd)->sw_axf = axf;
			break;

		case CRYPTO_MD5:
			axf = &swcr_auth_hash_md5;
			goto auth3common;

		case CRYPTO_SHA1:
			axf = &swcr_auth_hash_sha1;
		auth3common:
			(*swd)->sw_ictx = malloc(axf->ctxsize,
			    M_CRYPTO_DATA, M_NOWAIT);
			if ((*swd)->sw_ictx == NULL) {
				swcr_freesession(NULL, i);
				return ENOBUFS;
			}

			axf->Init((*swd)->sw_ictx);
			(*swd)->sw_axf = axf;
			break;

		case CRYPTO_AES_XCBC_MAC_96:
			axf = &swcr_auth_hash_aes_xcbc_mac;
			goto auth4common;
		case CRYPTO_AES_128_GMAC:
			axf = &swcr_auth_hash_gmac_aes_128;
			goto auth4common;
		case CRYPTO_AES_192_GMAC:
			axf = &swcr_auth_hash_gmac_aes_192;
			goto auth4common;
		case CRYPTO_AES_256_GMAC:
			axf = &swcr_auth_hash_gmac_aes_256;
		auth4common:
			(*swd)->sw_ictx = malloc(axf->ctxsize,
			    M_CRYPTO_DATA, M_NOWAIT);
			if ((*swd)->sw_ictx == NULL) {
				swcr_freesession(NULL, i);
				return ENOBUFS;
			}
			axf->Init((*swd)->sw_ictx);
			axf->Setkey((*swd)->sw_ictx,
				cri->cri_key, cri->cri_klen / 8);
			(*swd)->sw_axf = axf;
			break;

		case CRYPTO_DEFLATE_COMP:
			cxf = &swcr_comp_algo_deflate;
			(*swd)->sw_cxf = cxf;
			break;

		case CRYPTO_DEFLATE_COMP_NOGROW:
			cxf = &swcr_comp_algo_deflate_nogrow;
			(*swd)->sw_cxf = cxf;
			break;

		case CRYPTO_GZIP_COMP:
			cxf = &swcr_comp_algo_gzip;
			(*swd)->sw_cxf = cxf;
			break;
		default:
			swcr_freesession(NULL, i);
			return EINVAL;
		}

		(*swd)->sw_alg = cri->cri_alg;
		cri = cri->cri_next;
		swd = &((*swd)->sw_next);
	}
	return 0;
}

/*
 * Free a session.
 */
static int
swcr_freesession(void *arg, u_int64_t tid)
{
	struct swcr_data *swd;
	const struct swcr_enc_xform *txf;
	const struct swcr_auth_hash *axf;
	u_int32_t sid = ((u_int32_t) tid) & 0xffffffff;

	if (sid > swcr_sesnum || swcr_sessions == NULL ||
	    swcr_sessions[sid] == NULL)
		return EINVAL;

	/* Silently accept and return */
	if (sid == 0)
		return 0;

	while ((swd = swcr_sessions[sid]) != NULL) {
		swcr_sessions[sid] = swd->sw_next;

		switch (swd->sw_alg) {
		case CRYPTO_DES_CBC:
		case CRYPTO_3DES_CBC:
		case CRYPTO_BLF_CBC:
		case CRYPTO_CAST_CBC:
		case CRYPTO_SKIPJACK_CBC:
		case CRYPTO_RIJNDAEL128_CBC:
		case CRYPTO_CAMELLIA_CBC:
		case CRYPTO_AES_CTR:
		case CRYPTO_AES_GCM_16:
		case CRYPTO_AES_GMAC:
		case CRYPTO_NULL_CBC:
			txf = swd->sw_exf;

			if (swd->sw_kschedule)
				txf->zerokey(&(swd->sw_kschedule));
			break;

		case CRYPTO_MD5_HMAC:
		case CRYPTO_MD5_HMAC_96:
		case CRYPTO_SHA1_HMAC:
		case CRYPTO_SHA1_HMAC_96:
		case CRYPTO_SHA2_256_HMAC:
		case CRYPTO_SHA2_384_HMAC:
		case CRYPTO_SHA2_512_HMAC:
		case CRYPTO_RIPEMD160_HMAC:
		case CRYPTO_RIPEMD160_HMAC_96:
		case CRYPTO_NULL_HMAC:
			axf = swd->sw_axf;

			if (swd->sw_ictx) {
				explicit_memset(swd->sw_ictx, 0, axf->ctxsize);
				free(swd->sw_ictx, M_CRYPTO_DATA);
			}
			if (swd->sw_octx) {
				explicit_memset(swd->sw_octx, 0, axf->ctxsize);
				free(swd->sw_octx, M_CRYPTO_DATA);
			}
			break;

		case CRYPTO_MD5_KPDK:
		case CRYPTO_SHA1_KPDK:
			axf = swd->sw_axf;

			if (swd->sw_ictx) {
				explicit_memset(swd->sw_ictx, 0, axf->ctxsize);
				free(swd->sw_ictx, M_CRYPTO_DATA);
			}
			if (swd->sw_octx) {
				explicit_memset(swd->sw_octx, 0, swd->sw_klen);
				free(swd->sw_octx, M_CRYPTO_DATA);
			}
			break;

		case CRYPTO_MD5:
		case CRYPTO_SHA1:
		case CRYPTO_AES_XCBC_MAC_96:
		case CRYPTO_AES_128_GMAC:
		case CRYPTO_AES_192_GMAC:
		case CRYPTO_AES_256_GMAC:
			axf = swd->sw_axf;

			if (swd->sw_ictx) {
				explicit_memset(swd->sw_ictx, 0, axf->ctxsize);
				free(swd->sw_ictx, M_CRYPTO_DATA);
			}
			break;

		case CRYPTO_DEFLATE_COMP:
		case CRYPTO_DEFLATE_COMP_NOGROW:
		case CRYPTO_GZIP_COMP:
			break;
		}

		free(swd, M_CRYPTO_DATA);
	}
	return 0;
}

/*
 * Process a software request.
 */
static int
swcr_process(void *arg, struct cryptop *crp, int hint)
{
	struct cryptodesc *crd;
	struct swcr_data *sw;
	u_int32_t lid;
	int type;

	/* Sanity check */
	if (crp == NULL)
		return EINVAL;

	if (crp->crp_desc == NULL || crp->crp_buf == NULL) {
		crp->crp_etype = EINVAL;
		goto done;
	}

	lid = crp->crp_sid & 0xffffffff;
	if (lid >= swcr_sesnum || lid == 0 || swcr_sessions[lid] == NULL) {
		crp->crp_etype = ENOENT;
		goto done;
	}

	if (crp->crp_flags & CRYPTO_F_IMBUF) {
		type = CRYPTO_BUF_MBUF;
	} else if (crp->crp_flags & CRYPTO_F_IOV) {
		type = CRYPTO_BUF_IOV;
	} else {
		type = CRYPTO_BUF_CONTIG;
	}

	/* Go through crypto descriptors, processing as we go */
	for (crd = crp->crp_desc; crd; crd = crd->crd_next) {
		/*
		 * Find the crypto context.
		 *
		 * XXX Note that the logic here prevents us from having
		 * XXX the same algorithm multiple times in a session
		 * XXX (or rather, we can but it won't give us the right
		 * XXX results). To do that, we'd need some way of differentiating
		 * XXX between the various instances of an algorithm (so we can
		 * XXX locate the correct crypto context).
		 */
		for (sw = swcr_sessions[lid];
		    sw && sw->sw_alg != crd->crd_alg;
		    sw = sw->sw_next)
			;

		/* No such context ? */
		if (sw == NULL) {
			crp->crp_etype = EINVAL;
			goto done;
		}

		switch (sw->sw_alg) {
		case CRYPTO_DES_CBC:
		case CRYPTO_3DES_CBC:
		case CRYPTO_BLF_CBC:
		case CRYPTO_CAST_CBC:
		case CRYPTO_SKIPJACK_CBC:
		case CRYPTO_RIJNDAEL128_CBC:
		case CRYPTO_CAMELLIA_CBC:
		case CRYPTO_AES_CTR:
			if ((crp->crp_etype = swcr_encdec(crd, sw,
			    crp->crp_buf, type)) != 0)
				goto done;
			break;
		case CRYPTO_NULL_CBC:
			crp->crp_etype = 0;
			break;
		case CRYPTO_MD5_HMAC:
		case CRYPTO_MD5_HMAC_96:
		case CRYPTO_SHA1_HMAC:
		case CRYPTO_SHA1_HMAC_96:
		case CRYPTO_SHA2_256_HMAC:
		case CRYPTO_SHA2_384_HMAC:
		case CRYPTO_SHA2_512_HMAC:
		case CRYPTO_RIPEMD160_HMAC:
		case CRYPTO_RIPEMD160_HMAC_96:
		case CRYPTO_NULL_HMAC:
		case CRYPTO_MD5_KPDK:
		case CRYPTO_SHA1_KPDK:
		case CRYPTO_MD5:
		case CRYPTO_SHA1:
		case CRYPTO_AES_XCBC_MAC_96:
			if ((crp->crp_etype = swcr_authcompute(crp, crd, sw,
			    crp->crp_buf, type)) != 0)
				goto done;
			break;

		case CRYPTO_AES_GCM_16:
		case CRYPTO_AES_GMAC:
		case CRYPTO_AES_128_GMAC:
		case CRYPTO_AES_192_GMAC:
		case CRYPTO_AES_256_GMAC:
			crp->crp_etype = swcr_combined(crp, type);
			goto done;

		case CRYPTO_DEFLATE_COMP:
		case CRYPTO_DEFLATE_COMP_NOGROW:
		case CRYPTO_GZIP_COMP:
			DPRINTF(("swcr_process: compdec for %d\n", sw->sw_alg));
			if ((crp->crp_etype = swcr_compdec(crd, sw,
			    crp->crp_buf, type, &crp->crp_olen)) != 0)
				goto done;
			break;

		default:
			/* Unknown/unsupported algorithm */
			crp->crp_etype = EINVAL;
			goto done;
		}
	}

done:
	DPRINTF(("request %p done\n", crp));
	crypto_done(crp);
	return 0;
}

static void
swcr_init(void)
{
	swcr_id = crypto_get_driverid(CRYPTOCAP_F_SOFTWARE);
	if (swcr_id < 0) {
		/* This should never happen */
		panic("Software crypto device cannot initialize!");
	}

	crypto_register(swcr_id, CRYPTO_DES_CBC,
	    0, 0, swcr_newsession, swcr_freesession, swcr_process, NULL);
#define	REGISTER(alg) \
	crypto_register(swcr_id, alg, 0, 0, NULL, NULL, NULL, NULL)

	REGISTER(CRYPTO_3DES_CBC);
	REGISTER(CRYPTO_BLF_CBC);
	REGISTER(CRYPTO_CAST_CBC);
	REGISTER(CRYPTO_SKIPJACK_CBC);
	REGISTER(CRYPTO_CAMELLIA_CBC);
	REGISTER(CRYPTO_AES_CTR);
	REGISTER(CRYPTO_AES_GCM_16);
	REGISTER(CRYPTO_AES_GMAC);
	REGISTER(CRYPTO_NULL_CBC);
	REGISTER(CRYPTO_MD5_HMAC);
	REGISTER(CRYPTO_MD5_HMAC_96);
	REGISTER(CRYPTO_SHA1_HMAC);
	REGISTER(CRYPTO_SHA1_HMAC_96);
	REGISTER(CRYPTO_SHA2_256_HMAC);
	REGISTER(CRYPTO_SHA2_384_HMAC);
	REGISTER(CRYPTO_SHA2_512_HMAC);
	REGISTER(CRYPTO_RIPEMD160_HMAC);
	REGISTER(CRYPTO_RIPEMD160_HMAC_96);
	REGISTER(CRYPTO_NULL_HMAC);
	REGISTER(CRYPTO_MD5_KPDK);
	REGISTER(CRYPTO_SHA1_KPDK);
	REGISTER(CRYPTO_MD5);
	REGISTER(CRYPTO_SHA1);
	REGISTER(CRYPTO_AES_XCBC_MAC_96);
	REGISTER(CRYPTO_AES_128_GMAC);
	REGISTER(CRYPTO_AES_192_GMAC);
	REGISTER(CRYPTO_AES_256_GMAC);
	REGISTER(CRYPTO_RIJNDAEL128_CBC);
	REGISTER(CRYPTO_DEFLATE_COMP);
	REGISTER(CRYPTO_DEFLATE_COMP_NOGROW);
	REGISTER(CRYPTO_GZIP_COMP);
#undef REGISTER
}


/*
 * Pseudo-device init routine for software crypto.
 */

void
swcryptoattach(int num)
{

	swcr_init();
}

void	swcrypto_attach(device_t, device_t, void *);

void
swcrypto_attach(device_t parent, device_t self, void *opaque)
{

	swcr_init();

	if (!pmf_device_register(self, NULL, NULL))
		aprint_error_dev(self, "couldn't establish power handler\n");
}

int	swcrypto_detach(device_t, int);

int
swcrypto_detach(device_t self, int flag)
{
	pmf_device_deregister(self);
	if (swcr_id >= 0)
		crypto_unregister_all(swcr_id);
	return 0;
}

int	swcrypto_match(device_t, cfdata_t, void *);

int
swcrypto_match(device_t parent, cfdata_t data, void *opaque)
{

        return 1;
}

MODULE(MODULE_CLASS_DRIVER, swcrypto,
	"opencrypto,zlib,blowfish,des,cast128,camellia,skipjack");

CFDRIVER_DECL(swcrypto, DV_DULL, NULL);

CFATTACH_DECL2_NEW(swcrypto, 0, swcrypto_match, swcrypto_attach,
    swcrypto_detach, NULL, NULL, NULL);

static int swcryptoloc[] = { -1, -1 };

static struct cfdata swcrypto_cfdata[] = {
	{
		.cf_name = "swcrypto",
		.cf_atname = "swcrypto",
		.cf_unit = 0,
		.cf_fstate = 0,
		.cf_loc = swcryptoloc,
		.cf_flags = 0,
		.cf_pspec = NULL,
	},
	{ NULL, NULL, 0, 0, NULL, 0, NULL }
};

static int
swcrypto_modcmd(modcmd_t cmd, void *arg)
{
	int error;

	switch (cmd) {
	case MODULE_CMD_INIT:
		error = config_cfdriver_attach(&swcrypto_cd);
		if (error) {
			return error;
		}

		error = config_cfattach_attach(swcrypto_cd.cd_name,
		    &swcrypto_ca);
		if (error) {
			config_cfdriver_detach(&swcrypto_cd);
			aprint_error("%s: unable to register cfattach\n",
				swcrypto_cd.cd_name);

			return error;
		}

		error = config_cfdata_attach(swcrypto_cfdata, 1);
		if (error) {
			config_cfattach_detach(swcrypto_cd.cd_name,
			    &swcrypto_ca);
			config_cfdriver_detach(&swcrypto_cd);
			aprint_error("%s: unable to register cfdata\n",
				swcrypto_cd.cd_name);

			return error;
		}

		(void)config_attach_pseudo(swcrypto_cfdata);

		return 0;
	case MODULE_CMD_FINI:
		error = config_cfdata_detach(swcrypto_cfdata);
		if (error) {
			return error;
		}

		config_cfattach_detach(swcrypto_cd.cd_name, &swcrypto_ca);
		config_cfdriver_detach(&swcrypto_cd);

		return 0;
	default:
		return ENOTTY;
	}
}
