/*	$NetBSD: sign.h,v 1.2 2008/11/07 07:36:38 minskim Exp $	*/

/*-
 * Copyright (c) 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Martin Schütte.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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
 * sign.h
 *
 */
#ifndef SIGN_H_
#define SIGN_H_

#include <netinet/in.h>
#include <resolv.h>
#include <openssl/x509v3.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/pem.h>

/* default Signature Group value,
 * defines signature strategy:
 * 0 one global SG
 * 1 one SG per PRI
 * 2 SGs for PRI ranges
 * 3 other (SGs not defined by PRI)
 *
 * We use '3' and assign one SG to every destination (=struct filed)
 */
#define SIGN_SG 3

/* maximum value for several counters in -sign */
#define SIGN_MAX_COUNT	9999999999

/*
 * many of these options could be made user configurable if desired,
 * but I do not see the need for that
 */

/* redundancy options */
/*
 * note on the implementation of redundancy:
 * - certificate blocks: sending the first CB just before first SB.
 *   after that domark() triggers resends until resend count is reached.
 * - signature blocks: to send every hash n times I use a sliding window.
 *   the hashes in every SB are grouped into n divisions:
 *   * the 1st hashcount/n hashes are sent for the 1st time
 *   * the 2nd hashcount/n hashes are sent for the 2nd time
 *   * ...
 *   * the n-th hashcount/n hashes are sent for the n-th time
 *     (and deleted thereafter)
 */
#define SIGN_RESENDCOUNT_CERTBLOCK  2
#define SIGN_RESENDCOUNT_HASHES	    3

/* maximum length of syslog-sign messages should be <= 2048 by standard
 * and should be >= 1024 to be long enough.
 * be careful with small values because there is no check for a lower bound
 * thus the following derived values would become negative.
 */
#define SIGN_MAX_LENGTH 2048
/* the length we can use for the SD and keep the
 * message length with header below 2048 octets */
#define SIGN_MAX_SD_LENGTH (SIGN_MAX_LENGTH - 1 - HEADER_LEN_MAX)
/* length of signature, currently only for DSA */
#define SIGN_B64SIGLEN_DSS 64+1
/* the maximum length of one payload fragment:
 * max.SD len - text - max. field lengths - sig len */
#define SIGN_MAX_FRAG_LENGTH (SIGN_MAX_SD_LENGTH - 82 - 38 - SIGN_B64SIGLEN_DSS)
/* the maximum length of one signature block:
 * max.SD len - text - max. field lens - sig len */
#define SIGN_MAX_SB_LENGTH (SIGN_MAX_SD_LENGTH - 72 - 40 - SIGN_B64SIGLEN_DSS)
/* the maximum number of hashes pec signature block */
#define SIGN_MAX_HASH_NUM (SIGN_MAX_SB_LENGTH / (GlobalSign.md_len_b64+1))
/* number of hashes in one signature block */
#define SIGN_HASH_NUM_WANT 100
/* make sure to consider SIGN_MAX_HASH_NUM and
 * to have a SIGN_HASH_NUM that is a multiple of SIGN_HASH_DIVISION_NUM */
#define SIGN_HASH_DIVISION_NUM (MIN(SIGN_HASH_NUM_WANT, SIGN_MAX_HASH_NUM) \
				/ SIGN_RESENDCOUNT_HASHES)
#define SIGN_HASH_NUM (SIGN_HASH_DIVISION_NUM * SIGN_RESENDCOUNT_HASHES)

/* the length of payload strings
 * since the payload is fragmented there is no technical limit
 * it just has to be big enough to hold big b64 encoded PKIX certificates
 */
#define SIGN_MAX_PAYLOAD_LENGTH 20480

/* length of generated DSA keys for signing */
#define SIGN_GENCERT_BITS 1024

#define SSL_CHECK_ONE(exp) do {						\
	if ((exp) != 1) {				     		\
		DPRINTF(D_SIGN, #exp " failed in %d: %s\n", __LINE__,	\
		    ERR_error_string(ERR_get_error(), NULL));	     	\
		return 1;					     	\
	}								\
} while (/*CONSTCOND*/0)

/* structs use uint_fast64_t in different places because the standard
 * requires values in interval [0:9999999999 = SIGN_MAX_COUNT] */

/* queue of C-Strings (here used for hashes) */
struct string_queue {
	uint_fast64_t  key;
	char	      *data;
	STAILQ_ENTRY(string_queue) entries;
};
STAILQ_HEAD(string_queue_head, string_queue);

/* queue of destinations (used associate SGs and fileds) */
struct filed_queue {
	struct filed		 *f;
	STAILQ_ENTRY(filed_queue) entries;
};
STAILQ_HEAD(filed_queue_head, filed_queue);

/* queue of Signature Groups */
struct signature_group_t {
	unsigned		       spri;
	unsigned		       resendcount;
	uint_fast64_t		       last_msg_num;
	struct string_queue_head       hashes;
	struct filed_queue_head	       files;
	STAILQ_ENTRY(signature_group_t) entries;
};
STAILQ_HEAD(signature_group_head, signature_group_t);

/* all global variables for sign */
/* note that there is one object of this type which might only be
 * partially filled.
 * The fields .sg and .sig2_delims are set by init() and are always
 * valid. A value >0 in field .rsid indicates whether the rest of the
 * structure was already set by sign_global_init().
 */
struct sign_global_t {
	/* params for signature block, named as in RFC nnnn */
	const char   *ver;
	uint_fast64_t rsid;
	int	      sg;
	uint_fast64_t gbc;
	struct signature_group_head SigGroups;
	struct string_queue_head    sig2_delims;

	EVP_PKEY     *privkey;
	EVP_PKEY     *pubkey;
	char	     *pubkey_b64;
	char	      keytype;

	EVP_MD_CTX   *mdctx;	   /* hashing context */
	const EVP_MD *md;	   /* hashing method/algorithm */
	unsigned      md_len_b64;  /* length of b64 hash value */

	EVP_MD_CTX   *sigctx;	   /* signature context */
	const EVP_MD *sig;	   /* signature method/algorithm */
	unsigned      sig_len_b64; /* length of b64 signature */
};

bool	 sign_global_init(struct filed*);
bool	 sign_sg_init(struct filed*);
bool	 sign_get_keys(void);
void	 sign_global_free(void);
struct signature_group_t* sign_get_sg(int, struct filed*);
bool	 sign_send_certificate_block(struct signature_group_t*);
unsigned sign_send_signature_block(struct signature_group_t*, bool);
void	 sign_free_hashes(struct signature_group_t*);
void	 sign_free_string_queue(struct string_queue_head*);
bool	 sign_msg_hash(char*, char**);
bool	 sign_append_hash(char*, struct signature_group_t*);
bool	 sign_msg_sign(struct buf_msg**, char*, size_t);
bool	 sign_string_sign(char*, char**);
void	 sign_new_reboot_session(void);
void	 sign_inc_gbc(void);
uint_fast64_t sign_assign_msg_num(struct signature_group_t*);

#endif /* SIGN_H_ */
