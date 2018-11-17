/*	$NetBSD: undef.h,v 1.2 2017/01/28 21:31:47 christos Exp $	*/

/*
 * Copyright (c) 2016 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
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

#ifndef HEIM_UNDEF_H
#define HEIM_UNDEF_H 1

#undef BIGNUM
#undef BN_CTX
#undef BN_BLINDING
#undef BN_MONT_CTX
#undef BN_GENCB
#undef DH
#undef DH_METHOD
#undef DSA
#undef DSA_METHOD
#undef RSA
#undef RSA_METHOD
#undef RAND_METHOD
#undef ENGINE
#undef BN_GENCB_call
#undef BN_GENCB_set
#undef BN_CTX_new
#undef BN_CTX_free
#undef BN_CTX_start
#undef BN_CTX_get
#undef BN_CTX_end
#undef BN_is_negative
#undef BN_rand
#undef BN_num_bits
#undef BN_num_bytes
#undef BN_new
#undef BN_clear_free
#undef BN_bin2bn
#undef BN_bn2bin
#undef BN_uadd
#undef BN_set_negative
#undef BN_set_word
#undef BN_get_word
#undef BN_cmp
#undef BN_free
#undef BN_is_bit_set
#undef BN_clear
#undef BN_dup
#undef BN_set_bit
#undef BN_clear_bit
#undef BN_bn2hex
#undef BN_hex2bn
#undef EVP_CIPHER_CTX_block_size
#undef EVP_CIPHER_CTX_cipher
#undef EVP_CIPHER_CTX_cleanup
#undef EVP_CIPHER_CTX_flags
#undef EVP_CIPHER_CTX_get_app_data
#undef EVP_CIPHER_CTX_init
#undef EVP_CIPHER_CTX_iv_length
#undef EVP_CIPHER_CTX_key_length
#undef EVP_CIPHER_CTX_mode
#undef EVP_CIPHER_CTX_set_app_data
#undef EVP_CIPHER_CTX_set_key_length
#undef EVP_CIPHER_CTX_set_padding
#undef EVP_CIPHER_block_size
#undef EVP_CIPHER_iv_length
#undef EVP_CIPHER_key_length
#undef EVP_Cipher
#undef EVP_CipherInit_ex
#undef EVP_CipherUpdate
#undef EVP_CipherFinal_ex
#undef EVP_Digest
#undef EVP_DigestFinal_ex
#undef EVP_DigestInit_ex
#undef EVP_DigestUpdate
#undef EVP_MD_CTX_block_size
#undef EVP_MD_CTX_cleanup
#undef EVP_MD_CTX_create
#undef EVP_MD_CTX_init
#undef EVP_MD_CTX_destroy
#undef EVP_MD_CTX_md
#undef EVP_MD_CTX_size
#undef EVP_MD_block_size
#undef EVP_MD_size
#undef EVP_aes_128_cbc
#undef EVP_aes_192_cbc
#undef EVP_aes_256_cbc
#undef EVP_aes_128_cfb8
#undef EVP_aes_192_cfb8
#undef EVP_aes_256_cfb8
#undef EVP_des_cbc
#undef EVP_des_ede3_cbc
#undef EVP_enc_null
#undef EVP_md2
#undef EVP_md4
#undef EVP_md5
#undef EVP_md_null
#undef EVP_rc2_40_cbc
#undef EVP_rc2_64_cbc
#undef EVP_rc2_cbc
#undef EVP_rc4
#undef EVP_rc4_40
#undef EVP_camellia_128_cbc
#undef EVP_camellia_192_cbc
#undef EVP_camellia_256_cbc
#undef EVP_sha
#undef EVP_sha1
#undef EVP_sha256
#undef EVP_sha384
#undef EVP_sha512
#undef PKCS5_PBKDF2_HMAC
#undef PKCS5_PBKDF2_HMAC_SHA1
#undef EVP_BytesToKey
#undef EVP_get_cipherbyname
#undef OpenSSL_add_all_algorithms
#undef OpenSSL_add_all_algorithms_conf
#undef OpenSSL_add_all_algorithms_noconf
#undef EVP_CIPHER_CTX_ctrl
#undef EVP_CIPHER_CTX_rand_key
#undef hcrypto_validate
#undef EVP_MD_CTX
#undef EVP_PKEY
#undef EVP_MD
#undef EVP_CIPHER
#undef EVP_CIPHER_CTX
#undef EVP_CIPH_STREAM_CIPHER
#undef EVP_CIPH_CBC_MODE
#undef EVP_CIPH_CFB8_MODE
#undef EVP_CIPH_MODE
#undef EVP_CIPH_CTRL_INIT
#undef EVP_CTRL_INIT
#undef EVP_CIPH_VARIABLE_LENGTH
#undef EVP_CIPH_ALWAYS_CALL_INIT
#undef EVP_CIPH_RAND_KEY
#undef EVP_CTRL_RAND_KEY
#undef NID_md2
#undef NID_md4
#undef NID_md5
#undef NID_sha1
#undef NID_sha256
#undef NID_sha384
#undef NID_sha512

#endif /* HEIM_UNDEF_H */
