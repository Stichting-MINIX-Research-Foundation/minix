/*	$NetBSD: crypto.c,v 1.1.1.1 2011/04/13 18:14:47 elric Exp $	*/

/*
 * Copyright (c) 2010 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "netlogon.h"

static uint8_t zeros[4];

static void
_netlogon_encode_sequence_number(uint64_t SequenceNumber, uint8_t *p,
                                 int initiatorFlag)
{
    uint32_t LowPart, HighPart;

    LowPart  = (SequenceNumber >> 0 ) & 0xFFFFFFFF;
    HighPart = (SequenceNumber >> 32) & 0xFFFFFFFF;

    _gss_mg_encode_be_uint32(LowPart,  &p[0]);
    _gss_mg_encode_be_uint32(HighPart, &p[4]);

    if (initiatorFlag)
        p[4] |= 0x80;
}

static int
_netlogon_decode_sequence_number(void *ptr, uint64_t *n,
                                 int initiatorFlag)
{
    uint8_t *p = ptr;
    uint32_t LowPart, HighPart;
    int gotInitiatorFlag;

    gotInitiatorFlag = (p[4] & 0x80) != 0;
    if (gotInitiatorFlag != initiatorFlag)
        return -1;

    p[4] &= 0x7F; /* clear initiator bit */

    _gss_mg_decode_be_uint32(&p[0], &LowPart);
    _gss_mg_decode_be_uint32(&p[4], &HighPart);

    *n = (LowPart << 0) | ((uint64_t)HighPart << 32);

    return 0;
}

static inline size_t
_netlogon_checksum_length(NL_AUTH_SIGNATURE *sig)
{
#if 0
    return (sig->SignatureAlgorithm == NL_SIGN_ALG_SHA256) ? 32 : 8;
#else
    /* Owing to a bug in Windows it always uses the old value */
    return 8;
#endif
}

static inline size_t
_netlogon_signature_length(uint16_t alg, int conf_req_flag)
{
    return NL_AUTH_SIGNATURE_COMMON_LENGTH +
        (alg == NL_SIGN_ALG_SHA256 ? 32 : 8) +
        (conf_req_flag ? 8 : 0);
}

static inline uint8_t *
_netlogon_confounder(NL_AUTH_SIGNATURE *sig)
{
    size_t cksumlen = _netlogon_checksum_length(sig);

    return &sig->Checksum[cksumlen];
}

static int
_netlogon_encode_NL_AUTH_SIGNATURE(NL_AUTH_SIGNATURE *sig,
                                   uint8_t *p, size_t len)
{
    *p++ = (sig->SignatureAlgorithm >> 0) & 0xFF;
    *p++ = (sig->SignatureAlgorithm >> 8) & 0xFF;
    *p++ = (sig->SealAlgorithm      >> 0) & 0xFF;
    *p++ = (sig->SealAlgorithm      >> 8) & 0xFF;
    *p++ = (sig->Pad                >> 0) & 0xFF;
    *p++ = (sig->Pad                >> 8) & 0xFF;
    *p++ = (sig->Flags              >> 0) & 0xFF;
    *p++ = (sig->Flags              >> 8) & 0xFF;

    if (len > NL_AUTH_SIGNATURE_HEADER_LENGTH) {
        memcpy(p, sig->SequenceNumber, 8);
        p += 8;
    }

    if (len > NL_AUTH_SIGNATURE_COMMON_LENGTH) {
        size_t cksumlen = _netlogon_checksum_length(sig);

        memcpy(p, sig->Checksum, cksumlen);
        p += cksumlen;

        /* Confounder, if present, is immediately after checksum */
        if (sig->SealAlgorithm != NL_SEAL_ALG_NONE) {
            memcpy(p, &sig->Checksum[cksumlen], 8);
        }
    }

    return 0;
}

static int
_netlogon_decode_NL_AUTH_SIGNATURE(const uint8_t *ptr,
                                   size_t len,
                                   NL_AUTH_SIGNATURE *sig)
{
    const uint8_t *p = ptr;
    size_t cksumlen;

    if (len < NL_AUTH_SIGNATURE_COMMON_LENGTH)
        return KRB5_BAD_MSIZE;

    sig->SignatureAlgorithm = (p[0] << 0) | (p[1] << 8);
    sig->SealAlgorithm      = (p[2] << 0) | (p[3] << 8);
    sig->Pad                = (p[4] << 0) | (p[5] << 8);
    sig->Flags              = (p[6] << 0) | (p[7] << 8);
    p += 8;

    memcpy(sig->SequenceNumber, p, 8);
    p += 8;

    /* Validate signature algorithm is known and matches enctype */
    switch (sig->SignatureAlgorithm) {
    case NL_SIGN_ALG_HMAC_MD5:
        cksumlen = NL_AUTH_SIGNATURE_LENGTH;
        break;
    case NL_SIGN_ALG_SHA256:
        cksumlen = NL_AUTH_SHA2_SIGNATURE_LENGTH;
        break;
    default:
        return EINVAL;
        break;
    }

    if (sig->SealAlgorithm == NL_SEAL_ALG_NONE)
        cksumlen -= 8; /* confounder is optional if no sealing */

    if (len < cksumlen)
        return KRB5_BAD_MSIZE;

    /* Copy variable length checksum */
    cksumlen = _netlogon_checksum_length(sig);
    memcpy(sig->Checksum, p, cksumlen);
    p += cksumlen;

    /* Copy confounder in past checksum */
    if (sig->SealAlgorithm != NL_SEAL_ALG_NONE)
        memcpy(&sig->Checksum[cksumlen], p, 8);

    return 0;
}

static void
_netlogon_derive_rc4_hmac_key(uint8_t key[16],
                              uint8_t *salt,
                              size_t saltLength,
                              EVP_CIPHER_CTX *rc4Key,
                              int enc)
{
    uint8_t tmpData[MD5_DIGEST_LENGTH];
    uint8_t derivedKey[MD5_DIGEST_LENGTH];
    unsigned int len = MD5_DIGEST_LENGTH;

    HMAC(EVP_md5(), key, 16, zeros, sizeof(zeros), tmpData, &len);
    HMAC(EVP_md5(), tmpData, MD5_DIGEST_LENGTH,
         salt, saltLength, derivedKey, &len);

    assert(len == MD5_DIGEST_LENGTH);

    EVP_CipherInit_ex(rc4Key, EVP_rc4(), NULL, derivedKey, NULL, enc);

    memset(derivedKey, 0, sizeof(derivedKey));
}

static void
_netlogon_derive_rc4_seal_key(gssnetlogon_ctx ctx,
                              NL_AUTH_SIGNATURE *sig,
                              EVP_CIPHER_CTX *sealkey,
                              int enc)
{
    uint8_t xorKey[16];
    int i;

    for (i = 0; i < sizeof(xorKey); i++) {
        xorKey[i] = ctx->SessionKey[i] ^ 0xF0;
    }

    _netlogon_derive_rc4_hmac_key(xorKey,
        sig->SequenceNumber, sizeof(sig->SequenceNumber), sealkey, enc);

    memset(xorKey, 0, sizeof(xorKey));
}

static void
_netlogon_derive_rc4_seq_key(gssnetlogon_ctx ctx,
                             NL_AUTH_SIGNATURE *sig,
                             EVP_CIPHER_CTX *seqkey,
                             int enc)
{
    _netlogon_derive_rc4_hmac_key(ctx->SessionKey,
        sig->Checksum, sizeof(sig->Checksum), seqkey, enc);
}

static void
_netlogon_derive_aes_seal_key(gssnetlogon_ctx ctx,
                              NL_AUTH_SIGNATURE *sig,
                              EVP_CIPHER_CTX *sealkey,
                              int enc)
{
    uint8_t encryptionKey[16];
    uint8_t ivec[16];
    int i;

    for (i = 0; i < sizeof(encryptionKey); i++) {
        encryptionKey[i] = ctx->SessionKey[i] ^ 0xF0;
    }

    memcpy(&ivec[0], sig->SequenceNumber, 8);
    memcpy(&ivec[8], sig->SequenceNumber, 8);

    EVP_CipherInit_ex(sealkey, EVP_aes_128_cfb8(),
                      NULL, encryptionKey, ivec, enc);

    memset(encryptionKey, 0, sizeof(encryptionKey));
}

static void
_netlogon_derive_aes_seq_key(gssnetlogon_ctx ctx,
                             NL_AUTH_SIGNATURE *sig,
                             EVP_CIPHER_CTX *seqkey,
                             int enc)
{
    uint8_t ivec[16];

    memcpy(&ivec[0], sig->Checksum, 8);
    memcpy(&ivec[8], sig->Checksum, 8);

    EVP_CipherInit_ex(seqkey, EVP_aes_128_cfb8(),
                      NULL, ctx->SessionKey, ivec, enc);
}

static void
_netlogon_seal(gssnetlogon_ctx ctx,
               NL_AUTH_SIGNATURE *sig,
               gss_iov_buffer_desc *iov,
               int iov_count,
               int enc)
{
    EVP_CIPHER_CTX sealkey;
    int i;
    uint8_t *confounder = _netlogon_confounder(sig);

    EVP_CIPHER_CTX_init(&sealkey);

    if (sig->SealAlgorithm == NL_SEAL_ALG_AES128)
        _netlogon_derive_aes_seal_key(ctx, sig, &sealkey, enc);
    else
        _netlogon_derive_rc4_seal_key(ctx, sig, &sealkey, enc);

    EVP_Cipher(&sealkey, confounder, confounder, 8);

    /*
     * For RC4, Windows resets the cipherstate after encrypting
     * the confounder, thus defeating the purpose of the confounder
     */
    if (sig->SealAlgorithm == NL_SEAL_ALG_RC4) {
        EVP_CipherFinal_ex(&sealkey, NULL, &i);
        _netlogon_derive_rc4_seal_key(ctx, sig, &sealkey, enc);
    }

    for (i = 0; i < iov_count; i++) {
        gss_iov_buffer_t iovp = &iov[i];

        switch (GSS_IOV_BUFFER_TYPE(iovp->type)) {
        case GSS_IOV_BUFFER_TYPE_DATA:
        case GSS_IOV_BUFFER_TYPE_PADDING:
            EVP_Cipher(&sealkey, iovp->buffer.value, iovp->buffer.value,
                       iovp->buffer.length);
            break;
        default:
            break;
        }
    }

    EVP_CipherFinal_ex(&sealkey, NULL, &i);
    EVP_CIPHER_CTX_cleanup(&sealkey);
}

static void
_netlogon_seq(gssnetlogon_ctx ctx,
              NL_AUTH_SIGNATURE *sig,
              int enc)
{
    EVP_CIPHER_CTX seqkey;

    EVP_CIPHER_CTX_init(&seqkey);

    if (sig->SignatureAlgorithm == NL_SIGN_ALG_SHA256)
        _netlogon_derive_aes_seq_key(ctx, sig, &seqkey, enc);
    else
        _netlogon_derive_rc4_seq_key(ctx, sig, &seqkey, enc);

    EVP_Cipher(&seqkey, sig->SequenceNumber, sig->SequenceNumber, 8);

    EVP_CIPHER_CTX_cleanup(&seqkey);
}

static void
_netlogon_digest_md5(gssnetlogon_ctx ctx,
                     NL_AUTH_SIGNATURE *sig,
                     gss_iov_buffer_desc *iov,
                     int iov_count,
                     uint8_t *md)
{
    EVP_MD_CTX *md5;
    uint8_t header[NL_AUTH_SIGNATURE_HEADER_LENGTH];
    uint8_t digest[MD5_DIGEST_LENGTH];
    unsigned int md_len = MD5_DIGEST_LENGTH;
    int i;

    _netlogon_encode_NL_AUTH_SIGNATURE(sig, header, sizeof(header));

    md5 = EVP_MD_CTX_create();
    EVP_DigestInit_ex(md5, EVP_md5(), NULL);
    EVP_DigestUpdate(md5, zeros, sizeof(zeros));
    EVP_DigestUpdate(md5, header, sizeof(header));

    if (sig->SealAlgorithm != NL_SEAL_ALG_NONE) {
        EVP_DigestUpdate(md5, sig->Confounder, sizeof(sig->Confounder));
    }

    for (i = 0; i < iov_count; i++) {
        gss_iov_buffer_t iovp = &iov[i];

        switch (GSS_IOV_BUFFER_TYPE(iovp->type)) {
        case GSS_IOV_BUFFER_TYPE_DATA:
        case GSS_IOV_BUFFER_TYPE_PADDING:
        case GSS_IOV_BUFFER_TYPE_SIGN_ONLY:
            EVP_DigestUpdate(md5, iovp->buffer.value, iovp->buffer.length);
            break;
        default:
            break;
        }
    }

    EVP_DigestFinal_ex(md5, digest, NULL);
    EVP_MD_CTX_destroy(md5);

    HMAC(EVP_md5(), ctx->SessionKey, sizeof(ctx->SessionKey),
         digest, sizeof(digest), digest, &md_len);
    memcpy(md, digest, 8);
}

static void
_netlogon_digest_sha256(gssnetlogon_ctx ctx,
                        NL_AUTH_SIGNATURE *sig,
                        gss_iov_buffer_desc *iov,
                        int iov_count,
                        uint8_t *md)
{
    HMAC_CTX hmac;
    uint8_t header[NL_AUTH_SIGNATURE_HEADER_LENGTH];
    uint8_t digest[SHA256_DIGEST_LENGTH];
    unsigned int md_len = SHA256_DIGEST_LENGTH;
    int i;

    /* Encode first 8 bytes of signature into header */
    _netlogon_encode_NL_AUTH_SIGNATURE(sig, header, sizeof(header));

    HMAC_CTX_init(&hmac);
    HMAC_Init_ex(&hmac, ctx->SessionKey, sizeof(ctx->SessionKey),
                 EVP_sha256(), NULL);
    HMAC_Update(&hmac, header, sizeof(header));

    if (sig->SealAlgorithm != NL_SEAL_ALG_NONE) {
        /*
         * If the checksum length bug is ever fixed, then be sure to
         * update this code to point to &sig->Checksum[32] as that is
         * where the confounder is supposed to be.
         */
        HMAC_Update(&hmac, sig->Confounder, 8);
    }

    for (i = 0; i < iov_count; i++) {
        gss_iov_buffer_t iovp = &iov[i];

        switch (GSS_IOV_BUFFER_TYPE(iovp->type)) {
        case GSS_IOV_BUFFER_TYPE_DATA:
        case GSS_IOV_BUFFER_TYPE_PADDING:
        case GSS_IOV_BUFFER_TYPE_SIGN_ONLY:
            HMAC_Update(&hmac, iovp->buffer.value, iovp->buffer.length);
            break;
        default:
            break;
        }
    }

    HMAC_Final(&hmac, digest, &md_len);
    HMAC_CTX_cleanup(&hmac);
    memcpy(md, digest, 8);
}

static void
_netlogon_digest(gssnetlogon_ctx ctx,
                 NL_AUTH_SIGNATURE *sig,
                 gss_iov_buffer_desc *iov,
                 int iov_count,
                 uint8_t *md)
{
    if (sig->SignatureAlgorithm == NL_SIGN_ALG_SHA256)
        _netlogon_digest_sha256(ctx, sig, iov, iov_count, md);
    else
        _netlogon_digest_md5(ctx, sig, iov, iov_count, md);
}

OM_uint32
_netlogon_wrap_iov(OM_uint32 * minor_status,
                   gss_ctx_id_t  context_handle,
                   int conf_req_flag,
                   gss_qop_t qop_req,
                   int *conf_state,
                   gss_iov_buffer_desc *iov,
                   int iov_count)
{
    OM_uint32 ret;
    gss_iov_buffer_t header;
    NL_AUTH_SIGNATURE_U sigbuf = { { 0 } };
    NL_AUTH_SIGNATURE *sig = NL_AUTH_SIGNATURE_P(&sigbuf);
    gssnetlogon_ctx ctx = (gssnetlogon_ctx)context_handle;
    size_t size;
    uint8_t *seqdata;

    if (ctx->State != NL_AUTH_ESTABLISHED) {
        *minor_status = EINVAL;
        return GSS_S_FAILURE;
    }

    header = _gss_mg_find_buffer(iov, iov_count, GSS_IOV_BUFFER_TYPE_HEADER);
    if (header == NULL) {
        *minor_status = EINVAL;
        return GSS_S_FAILURE;
    }

    size = _netlogon_signature_length(ctx->SignatureAlgorithm, conf_req_flag);

    if (GSS_IOV_BUFFER_FLAGS(header->type) & GSS_IOV_BUFFER_FLAG_ALLOCATE) {
        ret = _gss_mg_allocate_buffer(minor_status, header, size);
        if (GSS_ERROR(ret))
            return ret;
    } else if (header->buffer.length < size) {
        *minor_status = KRB5_BAD_MSIZE;
        return GSS_S_FAILURE;
    } else {
        header->buffer.length = size;
    }

    memset(header->buffer.value, 0, header->buffer.length);

    sig->SignatureAlgorithm = ctx->SignatureAlgorithm;
    sig->SealAlgorithm = conf_req_flag ? ctx->SealAlgorithm : NL_SEAL_ALG_NONE;

    if (conf_req_flag)
        krb5_generate_random_block(_netlogon_confounder(sig), 8);

    sig->Pad = 0xFFFF;              /* [MS-NRPC] 3.3.4.2.1.3 */
    sig->Flags = 0;                 /* [MS-NRPC] 3.3.4.2.1.4 */
    HEIMDAL_MUTEX_lock(&ctx->Mutex);
    _netlogon_encode_sequence_number(ctx->SequenceNumber, sig->SequenceNumber,
                                     ctx->LocallyInitiated);
    ctx->SequenceNumber++;
    HEIMDAL_MUTEX_unlock(&ctx->Mutex);

    /* [MS-NRPC] 3.3.4.2.1.7: sign header, optional confounder and data  */
    _netlogon_digest(ctx, sig, iov, iov_count, sig->Checksum);

    /* [MS-NRPC] 3.3.4.2.1.8: optionally encrypt confounder and data */
    if (conf_req_flag)
        _netlogon_seal(ctx, sig, iov, iov_count, 1);

    /* [MS-NRPC] 3.3.4.2.1.9: encrypt sequence number */
    _netlogon_seq(ctx, sig, 1);

    _netlogon_encode_NL_AUTH_SIGNATURE(sig, header->buffer.value,
                                       header->buffer.length);

    if (conf_state != NULL)
        *conf_state = conf_req_flag;

    *minor_status = 0;
    return GSS_S_COMPLETE;
}

OM_uint32
_netlogon_unwrap_iov(OM_uint32 *minor_status,
                     gss_ctx_id_t context_handle,
                     int *conf_state,
                     gss_qop_t *qop_state,
                     gss_iov_buffer_desc *iov,
                     int iov_count)
{
    OM_uint32 ret;
    gss_iov_buffer_t header;
    NL_AUTH_SIGNATURE_U sigbuf;
    NL_AUTH_SIGNATURE *sig = NL_AUTH_SIGNATURE_P(&sigbuf);
    gssnetlogon_ctx ctx = (gssnetlogon_ctx)context_handle;
    uint8_t checksum[SHA256_DIGEST_LENGTH];
    uint64_t SequenceNumber;

    if (ctx->State != NL_AUTH_ESTABLISHED) {
        *minor_status = EINVAL;
        return GSS_S_FAILURE;
    }

    header = _gss_mg_find_buffer(iov, iov_count, GSS_IOV_BUFFER_TYPE_HEADER);
    if (header == NULL) {
        *minor_status = EINVAL;
        return GSS_S_FAILURE;
    }

    ret = _netlogon_decode_NL_AUTH_SIGNATURE(header->buffer.value,
                                             header->buffer.length,
                                             sig);
    if (ret != 0) {
        *minor_status = ret;
        return GSS_S_DEFECTIVE_TOKEN;
    }

    /* [MS-NRPC] 3.3.4.2.2.1: verify signature algorithm selection */
    if (sig->SignatureAlgorithm != ctx->SignatureAlgorithm)
        return GSS_S_BAD_SIG;

    /* [MS-NRPC] 3.3.4.2.2.2: verify encryption algorithm selection */
    if (sig->SealAlgorithm != NL_SEAL_ALG_NONE &&
        sig->SealAlgorithm != ctx->SealAlgorithm)
        return GSS_S_DEFECTIVE_TOKEN;

    /* [MS-NRPC] 3.3.4.2.2.3: verify Pad bytes */
    if (sig->Pad != 0xFFFF)
        return GSS_S_DEFECTIVE_TOKEN;

    /* [MS-NRPC] 3.3.4.2.2.5: decrypt sequence number */
    _netlogon_seq(ctx, sig, 0);

    /* [MS-NRPC] 3.3.4.2.2.6: decode sequence number */
    if (_netlogon_decode_sequence_number(sig->SequenceNumber, &SequenceNumber,
                                         !ctx->LocallyInitiated) != 0)
        return GSS_S_UNSEQ_TOKEN;

    /* [MS-NRPC] 3.3.4.2.2.9: decrypt confounder and data */
    if (sig->SealAlgorithm != NL_SEAL_ALG_NONE)
        _netlogon_seal(ctx, sig, iov, iov_count, 0);

    /* [MS-NRPC] 3.3.4.2.2.10: verify signature */
    _netlogon_digest(ctx, sig, iov, iov_count, checksum);
    if (memcmp(sig->Checksum, checksum, _netlogon_checksum_length(sig)) != 0)
        return GSS_S_BAD_SIG;

    HEIMDAL_MUTEX_lock(&ctx->Mutex);
    if (SequenceNumber != ctx->SequenceNumber) {
        /* [MS-NRPC] 3.3.4.2.2.7: check sequence number */
        ret = GSS_S_UNSEQ_TOKEN;
    } else {
        /* [MS-NRPC] 3.3.4.2.2.8: increment sequence number */
        ctx->SequenceNumber++;
        ret = GSS_S_COMPLETE;
    }
    HEIMDAL_MUTEX_unlock(&ctx->Mutex);

    if (conf_state != NULL)
        *conf_state = (sig->SealAlgorithm != NL_SEAL_ALG_NONE);
    if (qop_state != NULL)
        *qop_state = GSS_C_QOP_DEFAULT;

    *minor_status = 0;
    return ret;
}

OM_uint32
_netlogon_wrap_iov_length(OM_uint32 * minor_status,
        	          gss_ctx_id_t context_handle,
        	          int conf_req_flag,
        	          gss_qop_t qop_req,
        	          int *conf_state,
        	          gss_iov_buffer_desc *iov,
        	          int iov_count)
{
    OM_uint32 ret;
    gss_iov_buffer_t iovp;
    gssnetlogon_ctx ctx = (gssnetlogon_ctx)context_handle;
    size_t len;

    iovp = _gss_mg_find_buffer(iov, iov_count, GSS_IOV_BUFFER_TYPE_HEADER);
    if (iovp == NULL) {
        *minor_status = EINVAL;
        return GSS_S_FAILURE;
    }

    len = NL_AUTH_SIGNATURE_COMMON_LENGTH;
    if (ctx->SignatureAlgorithm == NL_SIGN_ALG_SHA256)
        len += 32;  /* SHA2 checksum size */
    else
        len += 8;   /* HMAC checksum size */
    if (conf_req_flag)
        len += 8;   /* counfounder */

    iovp->buffer.length = len;

    iovp = _gss_mg_find_buffer(iov, iov_count, GSS_IOV_BUFFER_TYPE_PADDING);
    if (iovp != NULL)
        iovp->buffer.length = 0;

    iovp = _gss_mg_find_buffer(iov, iov_count, GSS_IOV_BUFFER_TYPE_TRAILER);
    if (iovp != NULL)
        iovp->buffer.length = 0;

    if (conf_state != NULL)
        *conf_state = conf_req_flag;

    *minor_status = 0;
    return GSS_S_COMPLETE;
}

OM_uint32 _netlogon_get_mic
           (OM_uint32 * minor_status,
            const gss_ctx_id_t context_handle,
            gss_qop_t qop_req,
            const gss_buffer_t message_buffer,
            gss_buffer_t message_token
           )
{
    gss_iov_buffer_desc iov[2];
    OM_uint32 ret;

    iov[0].type = GSS_IOV_BUFFER_TYPE_DATA;
    iov[0].buffer = *message_buffer;
    iov[1].type = GSS_IOV_BUFFER_TYPE_HEADER | GSS_IOV_BUFFER_FLAG_ALLOCATE;
    iov[1].buffer.length = 0;
    iov[1].buffer.value = NULL;

    ret = _netlogon_wrap_iov(minor_status, context_handle, 0,
                             qop_req, NULL, iov, 2);
    if (ret == GSS_S_COMPLETE)
        *message_token = iov[1].buffer;

    return ret;
}

OM_uint32
_netlogon_verify_mic
           (OM_uint32 * minor_status,
            const gss_ctx_id_t context_handle,
            const gss_buffer_t message_buffer,
            const gss_buffer_t token_buffer,
            gss_qop_t * qop_state
            )
{
    gss_iov_buffer_desc iov[2];

    iov[0].type = GSS_IOV_BUFFER_TYPE_DATA;
    iov[0].buffer = *message_buffer;
    iov[1].type = GSS_IOV_BUFFER_TYPE_HEADER;
    iov[1].buffer = *token_buffer;

    return _netlogon_unwrap_iov(minor_status, context_handle,
                                NULL, qop_state, iov, 2);
}

OM_uint32
_netlogon_wrap_size_limit (
            OM_uint32 * minor_status,
            const gss_ctx_id_t context_handle,
            int conf_req_flag,
            gss_qop_t qop_req,
            OM_uint32 req_output_size,
            OM_uint32 *max_input_size
           )
{
    gss_iov_buffer_desc iov[1];
    OM_uint32 ret;

    iov[0].type = GSS_IOV_BUFFER_TYPE_HEADER;
    iov[0].buffer.length = 0;

    ret = _netlogon_wrap_iov_length(minor_status, context_handle,
                                    conf_req_flag, qop_req, NULL,
                                    iov, sizeof(iov)/sizeof(iov[0]));
    if (GSS_ERROR(ret))
        return ret;

    if (req_output_size < iov[0].buffer.length)
        *max_input_size = 0;
    else
        *max_input_size = req_output_size - iov[0].buffer.length;

    return GSS_S_COMPLETE;
}

