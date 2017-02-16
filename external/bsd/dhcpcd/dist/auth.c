#include <sys/cdefs.h>
 __RCSID("$NetBSD: auth.c,v 1.10 2015/07/09 10:15:34 roy Exp $");

/*
 * dhcpcd - DHCP client daemon
 * Copyright (c) 2006-2015 Roy Marples <roy@marples.name>
 * All rights reserved

 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 */

#include <sys/file.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "config.h"
#include "auth.h"
#include "crypt/crypt.h"
#include "dhcp.h"
#include "dhcp6.h"
#include "dhcpcd.h"

#ifdef __sun
#define htonll
#define ntohll
#endif

#ifndef htonll
#if (BYTE_ORDER == LITTLE_ENDIAN)
static inline uint64_t
htonll(uint64_t x)
{

	return (uint64_t)htonl((uint32_t)(x >> 32)) |
	    (uint64_t)htonl((uint32_t)(x & 0xffffffff)) << 32;
}
#else	/* (BYTE_ORDER == LITTLE_ENDIAN) */
#define htonll(x) (x)
#endif
#endif  /* htonll */

#ifndef ntohll
#if (BYTE_ORDER == LITTLE_ENDIAN)
static inline uint64_t
ntohll(uint64_t x)
{

	return (uint64_t)ntohl((uint32_t)(x >> 32)) |
	    (uint64_t)ntohl((uint32_t)(x & 0xffffffff)) << 32;
}
#else	/* (BYTE_ORDER == LITTLE_ENDIAN) */
#define ntohll(x) (x)
#endif
#endif  /* ntohll */

#define HMAC_LENGTH	16

void
dhcp_auth_reset(struct authstate *state)
{

	state->replay = 0;
	if (state->token) {
		free(state->token->key);
		free(state->token->realm);
		free(state->token);
		state->token = NULL;
	}
	if (state->reconf) {
		free(state->reconf->key);
		free(state->reconf->realm);
		free(state->reconf);
		state->reconf = NULL;
	}
}

/*
 * Authenticate a DHCP message.
 * m and mlen refer to the whole message.
 * t is the DHCP type, pass it 4 or 6.
 * data and dlen refer to the authentication option within the message.
 */
const struct token *
dhcp_auth_validate(struct authstate *state, const struct auth *auth,
    const uint8_t *m, size_t mlen, int mp,  int mt,
    const uint8_t *data, size_t dlen)
{
	uint8_t protocol, algorithm, rdm, *mm, type;
	uint64_t replay;
	uint32_t secretid;
	const uint8_t *d, *realm;
	size_t realm_len;
	const struct token *t;
	time_t now;
	uint8_t hmac[HMAC_LENGTH];

	if (dlen < 3 + sizeof(replay)) {
		errno = EINVAL;
		return NULL;
	}

	/* Ensure that d is inside m which *may* not be the case for DHPCPv4 */
	if (data < m || data > m + mlen || data + dlen > m + mlen) {
		errno = ERANGE;
		return NULL;
	}

	d = data;
	protocol = *d++;
	algorithm = *d++;
	rdm = *d++;
	if (!(auth->options & DHCPCD_AUTH_SEND)) {
		/* If we didn't send any authorisation, it can only be a
		 * reconfigure key */
		if (protocol != AUTH_PROTO_RECONFKEY) {
			errno = EINVAL;
			return NULL;
		}
	} else if (protocol != auth->protocol ||
		    algorithm != auth->algorithm ||
		    rdm != auth->rdm)
	{
		/* As we don't require authentication, we should still
		 * accept a reconfigure key */
		if (protocol != AUTH_PROTO_RECONFKEY ||
		    auth->options & DHCPCD_AUTH_REQUIRE)
		{
			errno = EPERM;
			return NULL;
		}
	}
	dlen -= 3;

	memcpy(&replay, d, sizeof(replay));
	replay = ntohll(replay);
	if (state->token) {
		if (state->replay == (replay ^ 0x8000000000000000ULL)) {
			/* We don't know if the singular point is increasing
			 * or decreasing. */
			errno = EPERM;
			return NULL;
		}
		if ((uint64_t)(replay - state->replay) <= 0) {
			/* Replay attack detected */
			errno = EPERM;
			return NULL;
		}
	}
	d+= sizeof(replay);
	dlen -= sizeof(replay);

	realm = NULL;
	realm_len = 0;

	/* Extract realm and secret.
	 * Rest of data is MAC. */
	switch (protocol) {
	case AUTH_PROTO_TOKEN:
		secretid = 0;
		break;
	case AUTH_PROTO_DELAYED:
		if (dlen < sizeof(secretid) + sizeof(hmac)) {
			errno = EINVAL;
			return NULL;
		}
		memcpy(&secretid, d, sizeof(secretid));
		d += sizeof(secretid);
		dlen -= sizeof(secretid);
		break;
	case AUTH_PROTO_DELAYEDREALM:
		if (dlen < sizeof(secretid) + sizeof(hmac)) {
			errno = EINVAL;
			return NULL;
		}
		realm_len = dlen - (sizeof(secretid) + sizeof(hmac));
		if (realm_len) {
			realm = d;
			d += realm_len;
			dlen -= realm_len;
		}
		memcpy(&secretid, d, sizeof(secretid));
		d += sizeof(secretid);
		dlen -= sizeof(secretid);
		break;
	case AUTH_PROTO_RECONFKEY:
		if (dlen != 1 + 16) {
			errno = EINVAL;
			return NULL;
		}
		type = *d++;
		dlen--;
		switch (type) {
		case 1:
			if ((mp == 4 && mt == DHCP_ACK) ||
			    (mp == 6 && mt == DHCP6_REPLY))
			{
				if (state->reconf == NULL) {
					state->reconf =
					    malloc(sizeof(*state->reconf));
					if (state->reconf == NULL)
						return NULL;
					state->reconf->key = malloc(16);
					if (state->reconf->key == NULL) {
						free(state->reconf);
						state->reconf = NULL;
						return NULL;
					}
					state->reconf->secretid = 0;
					state->reconf->expire = 0;
					state->reconf->realm = NULL;
					state->reconf->realm_len = 0;
					state->reconf->key_len = 16;
				}
				memcpy(state->reconf->key, d, 16);
			} else {
				errno = EINVAL;
				return NULL;
			}
			if (state->reconf == NULL)
				errno = ENOENT;
			/* Free the old token so we log acceptance */
			if (state->token) {
				free(state->token);
				state->token = NULL;
			}
			/* Nothing to validate, just accepting the key */
			return state->reconf;
		case 2:
			if (!((mp == 4 && mt == DHCP_FORCERENEW) ||
			    (mp == 6 && mt == DHCP6_RECONFIGURE)))
			{
				errno = EINVAL;
				return NULL;
			}
			if (state->reconf == NULL) {
				errno = ENOENT;
				return NULL;
			}
			t = state->reconf;
			goto gottoken;
		default:
			errno = EINVAL;
			return NULL;
		}
	default:
		errno = ENOTSUP;
		return NULL;
	}

	/* Find a token for the realm and secret */
	secretid = ntohl(secretid);
	TAILQ_FOREACH(t, &auth->tokens, next) {
		if (t->secretid == secretid &&
		    t->realm_len == realm_len &&
		    (t->realm_len == 0 ||
		    memcmp(t->realm, realm, t->realm_len) == 0))
			break;
	}
	if (t == NULL) {
		errno = ESRCH;
		return NULL;
	}
	if (t->expire) {
		if (time(&now) == -1)
			return NULL;
		if (t->expire < now) {
			errno = EFAULT;
			return NULL;
		}
	}

gottoken:
	/* First message from the server */
	if (state->token &&
	    (state->token->secretid != t->secretid ||
	    state->token->realm_len != t->realm_len ||
	    memcmp(state->token->realm, t->realm, t->realm_len)))
	{
		errno = EPERM;
		return NULL;
	}

	/* Special case as no hashing needs to be done. */
	if (protocol == AUTH_PROTO_TOKEN) {
		if (dlen != t->key_len || memcmp(d, t->key, dlen)) {
			errno = EPERM;
			return NULL;
		}
		goto finish;
	}

	/* Make a duplicate of the message, but zero out the MAC part */
	mm = malloc(mlen);
	if (mm == NULL)
		return NULL;
	memcpy(mm, m, mlen);
	memset(mm + (d - m), 0, dlen);

	/* RFC3318, section 5.2 - zero giaddr and hops */
	if (mp == 4) {
		*(mm + offsetof(struct dhcp_message, hwopcount)) = '\0';
		memset(mm + offsetof(struct dhcp_message, giaddr), 0, 4);
	}

	memset(hmac, 0, sizeof(hmac));
	switch (algorithm) {
	case AUTH_ALG_HMAC_MD5:
		hmac_md5(mm, mlen, t->key, t->key_len, hmac);
		break;
	default:
		errno = ENOSYS;
		free(mm);
		return NULL;
	}

	free(mm);
	if (memcmp(d, &hmac, dlen)) {
		errno = EPERM;
		return NULL;
	}

finish:
	/* If we got here then authentication passed */
	state->replay = replay;
	if (state->token == NULL) {
		/* We cannot just save a pointer because a reconfigure will
		 * recreate the token list. So we duplicate it. */
		state->token = malloc(sizeof(*state->token));
		if (state->token) {
			state->token->secretid = t->secretid;
			state->token->key = malloc(t->key_len);
			if (state->token->key) {
				state->token->key_len = t->key_len;
				memcpy(state->token->key, t->key, t->key_len);
			} else {
				free(state->token);
				state->token = NULL;
				return NULL;
			}
			if (t->realm_len) {
				state->token->realm = malloc(t->realm_len);
				if (state->token->realm) {
					state->token->realm_len = t->realm_len;
					memcpy(state->token->realm, t->realm,
					    t->realm_len);
				} else {
					free(state->token->key);
					free(state->token);
					state->token = NULL;
					return NULL;
				}
			} else {
				state->token->realm = NULL;
				state->token->realm_len = 0;
			}
		}
		/* If we cannot save the token, we must invalidate */
		if (state->token == NULL)
			return NULL;
	}

	return t;
}

static uint64_t
get_next_rdm_monotonic_counter(struct auth *auth)
{
	FILE *fp;
	uint64_t rdm;
#ifdef LOCK_EX
	int flocked;
#endif

	fp = fopen(RDM_MONOFILE, "r+");
	if (fp == NULL) {
		if (errno != ENOENT)
			return ++auth->last_replay; /* report error? */
		fp = fopen(RDM_MONOFILE, "w");
		if (fp == NULL)
			return ++auth->last_replay; /* report error? */
#ifdef LOCK_EX
		flocked = flock(fileno(fp), LOCK_EX);
#endif
		rdm = 0;
	} else {
#ifdef LOCK_EX
		flocked = flock(fileno(fp), LOCK_EX);
#endif
		if (fscanf(fp, "0x%016" PRIu64, &rdm) != 1)
			rdm = 0; /* truncated? report error? */
	}

	rdm++;
	if (fseek(fp, 0, SEEK_SET) == -1 ||
	    ftruncate(fileno(fp), 0) == -1 ||
	    fprintf(fp, "0x%016" PRIu64 "\n", rdm) != 19 ||
	    fflush(fp) == EOF)
	{
		if (!auth->last_replay_set) {
			auth->last_replay = rdm;
			auth->last_replay_set = 1;
		} else
			rdm = ++auth->last_replay;
		/* report error? */
	}
#ifdef LOCK_EX
	if (flocked == 0)
		flock(fileno(fp), LOCK_UN);
#endif
	fclose(fp);
	return rdm;
}

#define JAN_1970       2208988800U    /* 1970 - 1900 in seconds */
static uint64_t
get_next_rdm_monotonic_clock(struct auth *auth)
{
	struct timespec ts;
	uint32_t pack[2];
	double frac;
	uint64_t rdm;

	if (clock_gettime(CLOCK_REALTIME, &ts) != 0)
		return ++auth->last_replay; /* report error? */
	pack[0] = htonl((uint32_t)ts.tv_sec + JAN_1970);
	frac = ((double)ts.tv_nsec / 1e9 * 0x100000000ULL);
	pack[1] = htonl((uint32_t)frac);

	memcpy(&rdm, &pack, sizeof(rdm));
	return rdm;
}

static uint64_t
get_next_rdm_monotonic(struct auth *auth)
{

	if (auth->options & DHCPCD_AUTH_RDM_COUNTER)
		return get_next_rdm_monotonic_counter(auth);
	return get_next_rdm_monotonic_clock(auth);
}

/*
 * Encode a DHCP message.
 * Either we know which token to use from the server response
 * or we are using a basic configuration token.
 * token is the token to encrypt with.
 * m and mlen refer to the whole message.
 * mp is the DHCP type, pass it 4 or 6.
 * mt is the DHCP message type.
 * data and dlen refer to the authentication option within the message.
 */
ssize_t
dhcp_auth_encode(struct auth *auth, const struct token *t,
    uint8_t *m, size_t mlen, int mp, int mt,
    uint8_t *data, size_t dlen)
{
	uint64_t rdm;
	uint8_t hmac[HMAC_LENGTH];
	time_t now;
	uint8_t hops, *p, info;
	uint32_t giaddr, secretid;

	if (auth->protocol == 0 && t == NULL) {
		TAILQ_FOREACH(t, &auth->tokens, next) {
			if (t->secretid == 0 &&
			    t->realm_len == 0)
			break;
		}
		if (t == NULL) {
			errno = EINVAL;
			return -1;
		}
		if (t->expire) {
			if (time(&now) == -1)
				return -1;
			if (t->expire < now) {
				errno = EPERM;
				return -1;
			}
		}
	}

	switch(auth->protocol) {
	case AUTH_PROTO_TOKEN:
	case AUTH_PROTO_DELAYED:
	case AUTH_PROTO_DELAYEDREALM:
		/* We don't ever send a reconf key */
		break;
	default:
		errno = ENOTSUP;
		return -1;
	}

	switch(auth->algorithm) {
	case AUTH_ALG_HMAC_MD5:
		break;
	default:
		errno = ENOTSUP;
		return -1;
	}

	switch(auth->rdm) {
	case AUTH_RDM_MONOTONIC:
		break;
	default:
		errno = ENOTSUP;
		return -1;
	}

	/* DISCOVER or INFORM messages don't write auth info */
	if ((mp == 4 && (mt == DHCP_DISCOVER || mt == DHCP_INFORM)) ||
	    (mp == 6 && (mt == DHCP6_SOLICIT || mt == DHCP6_INFORMATION_REQ)))
		info = 0;
	else
		info = 1;

	/* Work out the auth area size.
	 * We only need to do this for DISCOVER messages */
	if (data == NULL) {
		dlen = 1 + 1 + 1 + 8;
		switch(auth->protocol) {
		case AUTH_PROTO_TOKEN:
			dlen += t->key_len;
			break;
		case AUTH_PROTO_DELAYEDREALM:
			if (info && t)
				dlen += t->realm_len;
			/* FALLTHROUGH */
		case AUTH_PROTO_DELAYED:
			if (info && t)
				dlen += sizeof(t->secretid) + sizeof(hmac);
			break;
		}
		return (ssize_t)dlen;
	}

	if (dlen < 1 + 1 + 1 + 8) {
		errno = ENOBUFS;
		return -1;
	}

	/* Ensure that d is inside m which *may* not be the case for DHPCPv4 */
	if (data < m || data > m + mlen || data + dlen > m + mlen) {
		errno = ERANGE;
		return -1;
	}

	/* Write out our option */
	*data++ = auth->protocol;
	*data++ = auth->algorithm;
	*data++ = auth->rdm;
	switch (auth->rdm) {
	case AUTH_RDM_MONOTONIC:
		rdm = get_next_rdm_monotonic(auth);
		break;
	default:
		/* This block appeases gcc, clang doesn't need it */
		rdm = get_next_rdm_monotonic(auth);
		break;
	}
	rdm = htonll(rdm);
	memcpy(data, &rdm, 8);
	data += 8;
	dlen -= 1 + 1 + 1 + 8;

	/* Special case as no hashing needs to be done. */
	if (auth->protocol == AUTH_PROTO_TOKEN) {
		/* Should be impossible, but still */
		if (t == NULL) {
			errno = EINVAL;
			return -1;
		}
		if (dlen < t->key_len) {
			errno =	ENOBUFS;
			return -1;
		}
		memcpy(data, t->key, t->key_len);
		return (ssize_t)(dlen - t->key_len);
	}

	/* DISCOVER or INFORM messages don't write auth info */
	if (!info)
		return (ssize_t)dlen;

	/* Loading a saved lease without an authentication option */
	if (t == NULL)
		return 0;

	/* Write out the Realm */
	if (auth->protocol == AUTH_PROTO_DELAYEDREALM) {
		if (dlen < t->realm_len) {
			errno = ENOBUFS;
			return -1;
		}
		memcpy(data, t->realm, t->realm_len);
		data += t->realm_len;
		dlen -= t->realm_len;
	}

	/* Write out the SecretID */
	if (auth->protocol == AUTH_PROTO_DELAYED ||
	    auth->protocol == AUTH_PROTO_DELAYEDREALM)
	{
		if (dlen < sizeof(t->secretid)) {
			errno = ENOBUFS;
			return -1;
		}
		secretid = htonl(t->secretid);
		memcpy(data, &secretid, sizeof(secretid));
		data += sizeof(secretid);
		dlen -= sizeof(secretid);
	}

	/* Zero what's left, the MAC */
	memset(data, 0, dlen);

	/* RFC3318, section 5.2 - zero giaddr and hops */
	if (mp == 4) {
		p = m + offsetof(struct dhcp_message, hwopcount);
		hops = *p;
		*p = '\0';
		p = m + offsetof(struct dhcp_message, giaddr);
		memcpy(&giaddr, p, sizeof(giaddr));
		memset(p, 0, sizeof(giaddr));
	} else {
		/* appease GCC again */
		hops = 0;
		giaddr = 0;
	}

	/* Create our hash and write it out */
	switch(auth->algorithm) {
	case AUTH_ALG_HMAC_MD5:
		hmac_md5(m, mlen, t->key, t->key_len, hmac);
		memcpy(data, hmac, sizeof(hmac));
		break;
	}

	/* RFC3318, section 5.2 - restore giaddr and hops */
	if (mp == 4) {
		p = m + offsetof(struct dhcp_message, hwopcount);
		*p = hops;
		p = m + offsetof(struct dhcp_message, giaddr);
		memcpy(p, &giaddr, sizeof(giaddr));
	}

	/* Done! */
	return (int)(dlen - sizeof(hmac)); /* should be zero */
}
