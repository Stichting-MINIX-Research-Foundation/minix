/*	$NetBSD: parse.c,v 1.18 2013/07/17 15:42:03 christos Exp $	*/

/*-
 * Copyright (c) 2008 David Young.  All rights reserved.
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

#include <sys/cdefs.h>
#ifndef lint
__RCSID("$NetBSD: parse.c,v 1.18 2013/07/17 15:42:03 christos Exp $");
#endif /* not lint */

#include <err.h>
#include <errno.h>
#include <limits.h>
#include <netdb.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>
#include <sys/param.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <netatalk/at.h>

#include "env.h"
#include "parse.h"
#include "util.h"

#ifdef DEBUG
#define dbg_warnx(__fmt, ...)	warnx(__fmt, __VA_ARGS__)
#else
#define dbg_warnx(__fmt, ...)	/* empty */
#endif

static int parser_default_init(struct parser *);
static int pbranch_init(struct parser *);
static int pkw_init(struct parser *);

static int pterm_match(const struct parser *, const struct match *,
    struct match *, int, const char *);

static int paddr_match(const struct parser *, const struct match *,
    struct match *, int, const char *);

static int pbranch_match(const struct parser *, const struct match *,
    struct match *, int, const char *);

static int piface_match(const struct parser *, const struct match *,
    struct match *, int, const char *);

static int pstr_match(const struct parser *, const struct match *,
    struct match *, int, const char *);

static int pinteger_match(const struct parser *, const struct match *,
    struct match *, int, const char *);

static int pkw_match(const struct parser *, const struct match *,
    struct match *, int, const char *);

const struct parser_methods pterm_methods = {
	  .pm_match = pterm_match
	, .pm_init = NULL
};

const struct parser_methods pstr_methods = {
	  .pm_match = pstr_match
	, .pm_init = parser_default_init
};

const struct parser_methods pinteger_methods = {
	  .pm_match = pinteger_match
	, .pm_init = parser_default_init
};

const struct parser_methods paddr_methods = {
	  .pm_match = paddr_match
	, .pm_init = parser_default_init
};

const struct parser_methods piface_methods = {
	  .pm_match = piface_match
	, .pm_init = parser_default_init
};

const struct parser_methods pbranch_methods = {
	  .pm_match = pbranch_match
	, .pm_init = pbranch_init
};

const struct parser_methods pkw_methods = {
	  .pm_match = pkw_match
	, .pm_init = pkw_init
};

static int
match_setenv(const struct match *im, struct match *om, const char *key,
    prop_object_t o)
{
	if (im == NULL)
		om->m_env = prop_dictionary_create();
	else
		om->m_env = prop_dictionary_copy(im->m_env);

	if (om->m_env == NULL)
		goto delobj;

	if (key != NULL && !prop_dictionary_set(om->m_env, key, o))
		goto deldict;

	if (o != NULL)
		prop_object_release((prop_object_t)o);

	return 0;
deldict:
	prop_object_release((prop_object_t)om->m_env);
	om->m_env = NULL;
delobj:
	prop_object_release((prop_object_t)o);
	errno = ENOMEM;
	return -1;
}

int
pstr_match(const struct parser *p, const struct match *im, struct match *om,
    int argidx, const char *arg)
{
	prop_object_t o;
	const struct pstr *ps = (const struct pstr *)p;
	uint8_t buf[128];
	int len;

	if (arg == NULL) {
		errno = EINVAL;
		return -1;
	}

	len = (int)sizeof(buf);
	if (get_string(arg, NULL, buf, &len, ps->ps_hexok) == NULL) {
		errno = EINVAL;
		return -1;
	}

	o = (prop_object_t)prop_data_create_data(buf, len);

	if (o == NULL) {
		errno = ENOMEM;
		return -1;
	}

	if (match_setenv(im, om, ps->ps_key, o) == -1)
		return -1;

	om->m_argidx = argidx;
	om->m_parser = p;
	om->m_nextparser = p->p_nextparser;

	return 0;
}

int
pinteger_match(const struct parser *p, const struct match *im, struct match *om,
    int argidx, const char *arg)
{
	prop_object_t o;
	const struct pinteger *pi = (const struct pinteger *)p;
	char *end;
	int64_t val;

	if (arg == NULL) {
		errno = EINVAL;
		return -1;
	}

	val = strtoimax(arg, &end, pi->pi_base);
	if ((val == INTMAX_MIN || val == INTMAX_MAX) && errno == ERANGE)
		return -1;

	if (*end != '\0') {
		errno = EINVAL;
		return -1;
	}

	if (val < pi->pi_min || val > pi->pi_max) {
		errno = ERANGE;
		return -1;
	}

	o = (prop_object_t)prop_number_create_integer(val);

	if (o == NULL) {
		errno = ENOMEM;
		return -1;
	}

	if (match_setenv(im, om, pi->pi_key, o) == -1)
		return -1;

	om->m_argidx = argidx;
	om->m_parser = p;
	om->m_nextparser = p->p_nextparser;

	return 0;
}

static int
parse_linkaddr(const char *addr, struct sockaddr_storage *ss)
{
	static const size_t maxlen =
	    sizeof(*ss) - offsetof(struct sockaddr_dl, sdl_data[0]);
	enum {
		LLADDR_S_INITIAL = 0,
		LLADDR_S_ONE_OCTET = 1,
		LLADDR_S_TWO_OCTETS = 2,
		LLADDR_S_COLON = 3
	} state = LLADDR_S_INITIAL;
	uint8_t octet = 0, val;
	struct sockaddr_dl *sdl;
	const char *p;
	size_t i;

	memset(ss, 0, sizeof(*ss));
	ss->ss_family = AF_LINK;
	sdl = (struct sockaddr_dl *)ss;

	for (i = 0, p = addr; i < maxlen; p++) {
		dbg_warnx("%s.%d: *p == %c, state %d", __func__, __LINE__, *p,
		    state);
		if (*p == '\0') {
			dbg_warnx("%s.%d", __func__, __LINE__);
			if (state != LLADDR_S_ONE_OCTET &&
			    state != LLADDR_S_TWO_OCTETS)
				return -1;
			dbg_warnx("%s.%d", __func__, __LINE__);
			sdl->sdl_data[i++] = octet;
			sdl->sdl_len = offsetof(struct sockaddr_dl, sdl_data)
			    + i * sizeof(sdl->sdl_data[0]);
			sdl->sdl_alen = i;
			return 0;
		}
		if (*p == ':') {
			dbg_warnx("%s.%d", __func__, __LINE__);
			if (state != LLADDR_S_ONE_OCTET &&
			    state != LLADDR_S_TWO_OCTETS)
				return -1;
			dbg_warnx("%s.%d", __func__, __LINE__);
			sdl->sdl_data[i++] = octet;
			state = LLADDR_S_COLON;
			continue;
		}
		if ('a' <= *p && *p <= 'f')
			val = 10 + *p - 'a';
		else if ('A' <= *p && *p <= 'F')
			val = 10 + *p - 'A';
		else if ('0' <= *p && *p <= '9')
			val = *p - '0';
		else
			return -1;

		dbg_warnx("%s.%d", __func__, __LINE__);
		if (state == LLADDR_S_ONE_OCTET) {
			state = LLADDR_S_TWO_OCTETS;
			octet <<= 4;
			octet |= val;
		} else if (state != LLADDR_S_INITIAL && state != LLADDR_S_COLON)
			return -1;
		else {
			state = LLADDR_S_ONE_OCTET;
			octet = val;
		}
		dbg_warnx("%s.%d", __func__, __LINE__);
	}
	return -1;
}

static int
paddr_match(const struct parser *p, const struct match *im, struct match *om,
    int argidx, const char *arg0)
{
	unsigned int net, node;
	int nread;
	union {
		struct sockaddr sa;
		struct sockaddr_at sat;
		struct sockaddr_in sin;
		struct sockaddr_storage ss;
	} u;
	const struct paddr *pa = (const struct paddr *)p;
	prop_data_t d;
	prop_object_t o;
	int64_t af0;
	int af, rc;
	struct paddr_prefix *pfx, *mask;
	const struct sockaddr *sa = NULL;
	struct addrinfo hints, *result = NULL;
	char *arg, *end, *plen = NULL, *servname0;
	const char *servname;
	long prefixlen = -1;
	size_t len;

	if (arg0 == NULL) {
		errno = EINVAL;
		return -1;
	}

	if (pa->pa_activator != NULL &&
	    prop_dictionary_get(im->m_env, pa->pa_activator) == NULL)
		return -1;

	if (pa->pa_deactivator != NULL &&
	    prop_dictionary_get(im->m_env, pa->pa_deactivator) != NULL)
		return -1;

	if (!prop_dictionary_get_int64(im->m_env, "af", &af0))
		af = AF_UNSPEC;
	else
		af = af0;

	memset(&u, 0, sizeof(u));

	switch (af) {
	case AF_UNSPEC:
	case AF_INET:
	case AF_INET6:
		if ((arg = strdup(arg0)) == NULL)
			return -1;

		servname0 = arg;
		(void)strsep(&servname0, ",");
		servname = (servname0 == NULL) ? "0" : servname0;

		if (pa->pa_maskkey == NULL)
			;
		else if ((plen = strrchr(arg, '/')) != NULL)
			*plen++ = '\0';

		memset(&hints, 0, sizeof(hints));

		hints.ai_flags = AI_NUMERICHOST | AI_PASSIVE;
		hints.ai_family = af;
		hints.ai_socktype = SOCK_DGRAM;

		for (;;) {
			rc = getaddrinfo(arg, servname, &hints, &result);
			if (rc == 0) {
				if (result->ai_next == NULL)
					sa = result->ai_addr;
				else
					errno = EMLINK;
				break;
			} else if ((hints.ai_flags & AI_NUMERICHOST) != 0 &&
			    (af == AF_INET || af == AF_UNSPEC) &&
			    inet_aton(arg, &u.sin.sin_addr) == 1) {
				u.sin.sin_family = AF_INET;
				u.sin.sin_len = sizeof(u.sin);
				sa = &u.sa;
				break;
			} else if ((hints.ai_flags & AI_NUMERICHOST) == 0 ||
				 rc != EAI_NONAME) {
				errno = ENOENT;
				break;
			}
			hints.ai_flags &= ~AI_NUMERICHOST;
		}


		if (plen == NULL)
			prefixlen = -1;
		else {
			prefixlen = strtol(plen, &end, 10);
			if (end != NULL && *end != '\0')
				sa = NULL;
			if (prefixlen < 0 || prefixlen >= UINT8_MAX) {
				errno = ERANGE;
				sa = NULL;
			}
		}

		free(arg);
		if (sa != NULL || af != AF_UNSPEC)
			break;
		/*FALLTHROUGH*/
	case AF_APPLETALK:
		if (sscanf(arg0, "%u.%u%n", &net, &node, &nread) == 2 &&
		    net != 0 && net <= 0xffff && node != 0 && node <= 0xfe &&
		    arg0[nread] == '\0') {
			u.sat.sat_family = AF_APPLETALK;
			u.sat.sat_len = sizeof(u.sat);
			u.sat.sat_addr.s_net = htons(net);
			u.sat.sat_addr.s_node = node;
			sa = &u.sa;
		}
		break;
	case AF_LINK:
		if (parse_linkaddr(arg0, &u.ss) == -1)
			sa = NULL;
		else
			sa = &u.sa;
		break;
	}

	if (sa == NULL)
		return -1;

	len = offsetof(struct paddr_prefix, pfx_addr) + sa->sa_len;

	if ((pfx = malloc(len)) == NULL)
		return -1;

#if 0
	{
		int i;

		for (i = 0; i < sa->sa_len; i++)
			printf(" %02x", ((const uint8_t *)sa)[i]);
		printf("\n");
	}
#endif

	pfx->pfx_len = (int16_t)prefixlen;
	memcpy(&pfx->pfx_addr, sa, sa->sa_len);
	af = sa->sa_family;

	if (result != NULL)
		freeaddrinfo(result);

	o = (prop_object_t)prop_data_create_data(pfx, len);

	free(pfx);

	if (o == NULL)
		return -1;

	if (match_setenv(im, om, pa->pa_addrkey, o) == -1)
		return -1;

	if (pa->pa_maskkey != NULL && plen != NULL) {
		size_t masklen;

		if ((mask = prefixlen_to_mask(af, prefixlen)) == NULL) {
			err(EXIT_FAILURE, "%s: prefixlen_to_mask(%d, %ld)",
			    __func__, af, prefixlen);
			return -1;
		}

		masklen = paddr_prefix_size(mask);

		d = prop_data_create_data(mask, masklen);
		free(mask);

		if (d == NULL) {
			err(EXIT_FAILURE, "%s: prop_data_create_data",
			    __func__);
			return -1;
		}

		rc = prop_dictionary_set(om->m_env, pa->pa_maskkey,
		    (prop_object_t)d) ? 0 : -1;

		prop_object_release((prop_object_t)d);

		if (rc != 0) {
			err(EXIT_FAILURE, "%s: prop_dictionary_set", __func__);
			return rc;
		}
	}

	om->m_argidx = argidx;
	om->m_parser = p;
	om->m_nextparser = p->p_nextparser;
	return 0;
}

static int
pterm_match(const struct parser *p, const struct match *im,
    struct match *om, int argidx, const char *arg)
{
	const struct pterm *pt = (const struct pterm *)p;
	prop_bool_t b;

	if (arg != NULL) {
		errno = EINVAL;
		return -1;
	}
	b = prop_bool_create(true);

	if (match_setenv(im, om, pt->pt_key, (prop_object_t)b) == -1)
		return -1;

	om->m_argidx = argidx;
	om->m_parser = p;
	om->m_nextparser = NULL;
	return 0;
}

static int
piface_match(const struct parser *p, const struct match *im,
    struct match *om, int argidx, const char *arg)
{
	const struct piface *pif = (const struct piface *)p;
	prop_object_t o;

	if (arg == NULL || strlen(arg) > IFNAMSIZ) {
		errno = EINVAL;
		return -1;
	}

	if ((o = (prop_object_t)prop_string_create_cstring(arg)) == NULL) {
		errno = ENOMEM;
		return -1;
	}

	if (match_setenv(im, om, pif->pif_key, o) == -1)
		return -1;

	om->m_argidx = argidx;
	om->m_parser = p;
	om->m_nextparser = p->p_nextparser;
	return 0;
}

static void
match_cleanup(struct match *dst)
{
	if (dst->m_env != NULL)
		prop_object_release((prop_object_t)dst->m_env);
	memset(dst, 0, sizeof(*dst));
}

static void
match_copy(struct match *dst, const struct match *src)
{
	match_cleanup(dst);

	prop_object_retain((prop_object_t)src->m_env);
	*dst = *src;
}

static int
pbranch_match(const struct parser *p, const struct match *im,
    struct match *om, int argidx, const char *arg)
{
	const struct parser *nextp;
	struct branch *b;
	const struct pbranch *pb = (const struct pbranch *)p;
	struct match tmpm;
	int nforbid = 0, nmatch = 0, rc;
	parser_match_t matchfunc;

	memset(&tmpm, 0, sizeof(tmpm));

	SIMPLEQ_FOREACH(b, &pb->pb_branches, b_next) {
		nextp = b->b_nextparser;
		dbg_warnx("%s: b->b_nextparser %p [%s]", __func__,
		    nextp, nextp ? nextp->p_name : "(null)");
		if (nextp == NULL) {
			if (arg == NULL) {
				nmatch++;
				match_setenv(im, om, NULL, NULL);
				om->m_nextparser = NULL;
				om->m_parser = p;
				om->m_argidx = argidx;
			}
			continue;
		}
		matchfunc = nextp->p_methods->pm_match;
		rc = (*matchfunc)(nextp, im, &tmpm, argidx, arg);
		if (rc == 0) {
			match_copy(om, &tmpm);
			match_cleanup(&tmpm);
			nmatch++;
			dbg_warnx("%s: branch %s ok", __func__, nextp->p_name); 
			if (pb->pb_match_first)
				break;
		} else if (rc == 1) {
			nforbid++;
			if (pb->pb_match_first)
				break;
		} else {
			dbg_warnx("%s: fail branch %s", __func__,
			    nextp->p_name); 
		}
	}
	switch (nmatch) {
	case 0:
		errno = ENOENT;
		return (nforbid == 0) ? -1 : 1;
	case 1:
		dbg_warnx("%s: branch ok", __func__); 
		return 0;
	default:
		match_cleanup(om);
		errno = EMLINK;
		return -1;
	}
}

static int
pkw_match(const struct parser *p, const struct match *im,
    struct match *om, int argidx, const char *arg)
{
	prop_object_t o = NULL;
	struct kwinst *k;
	union kwval *u = NULL;
	const struct pkw *pk = (const struct pkw *)p;

	if (arg == NULL) {
		errno = EINVAL;
		return -1;
	}

	SIMPLEQ_FOREACH(k, &pk->pk_keywords, k_next) {
		if (k->k_act != NULL &&
		    prop_dictionary_get(im->m_env, k->k_act) == NULL)
			continue;

		if (k->k_neg && arg[0] == '-' &&
		    strcmp(k->k_word, arg + 1) == 0)
			u = &k->k_negu;
		else if (strcmp(k->k_word, arg) == 0)
			u = &k->k_u;
		else
			continue;

		if (k->k_altdeact != NULL &&
		    prop_dictionary_get(im->m_env, k->k_altdeact) != NULL)
			return 1;

		if (k->k_deact != NULL &&
		    prop_dictionary_get(im->m_env, k->k_deact) != NULL)
			return 1;
		break;
	}
	if (k == NULL) {
		errno = ENOENT;
		return -1;
	}
	switch (k->k_type) {
	case KW_T_NONE:
		break;
	case KW_T_BOOL:
		o = (prop_object_t)prop_bool_create(u->u_bool);
		if (o == NULL)
			goto err;
		break;
	case KW_T_INT:
		o = (prop_object_t)prop_number_create_integer(u->u_sint);
		if (o == NULL)
			goto err;
		break;
	case KW_T_UINT:
		o = (prop_object_t)prop_number_create_unsigned_integer(
		    u->u_uint);
		if (o == NULL)
			goto err;
		break;
	case KW_T_OBJ:
		o = u->u_obj;
		break;
	case KW_T_STR:
		o = (prop_object_t)prop_string_create_cstring_nocopy(u->u_str);
		if (o == NULL)
			goto err;
		break;
	default:
		errx(EXIT_FAILURE, "unknown keyword type %d", k->k_type);
	}

	if (match_setenv(im, om, (o == NULL) ? NULL : k->k_key, o) == -1)
		return -1;

	om->m_argidx = argidx;
	om->m_parser = p;
	om->m_nextparser = k->k_nextparser;
	om->m_exec = k->k_exec;
	return 0;
err:
	errno = ENOMEM;
	return -1;
}

struct paddr *
paddr_create(const char *name, parser_exec_t pexec, const char *addrkey,
    const char *maskkey, struct parser *next)
{
	struct paddr *pa;

	if ((pa = calloc(sizeof(*pa), 1)) == NULL)
		return NULL;

	pa->pa_parser.p_methods = &paddr_methods;
	pa->pa_parser.p_exec = pexec;
	pa->pa_parser.p_name = name;
	pa->pa_parser.p_nextparser = next;

	pa->pa_addrkey = addrkey;
	pa->pa_maskkey = maskkey;

	return pa;
}

struct piface *
piface_create(const char *name, parser_exec_t pexec, const char *defkey,
    struct parser *defnext)
{
	struct piface *pif;

	if ((pif = calloc(sizeof(*pif), 1)) == NULL)
		return NULL;

	pif->pif_parser.p_methods = &piface_methods;
	pif->pif_parser.p_exec = pexec;
	pif->pif_parser.p_name = name;
	pif->pif_parser.p_nextparser = defnext;

	pif->pif_key = defkey;

	return pif;
}

int
pbranch_addbranch(struct pbranch *pb, struct parser *p)
{
	struct branch *b;

	if ((b = malloc(sizeof(*b))) == NULL)
		return -1;
	b->b_nextparser = p;
	SIMPLEQ_INSERT_HEAD(&pb->pb_branches, b, b_next);
	pb->pb_parser.p_initialized = false;
	return parser_init(&pb->pb_parser);
}

int
pbranch_setbranches(struct pbranch *pb, const struct branch *brs, size_t nbr)
{
	struct branch *b;
	size_t i;

	dbg_warnx("%s: nbr %zu", __func__, nbr);

	while ((b = SIMPLEQ_FIRST(&pb->pb_branches)) != NULL) {
		SIMPLEQ_REMOVE_HEAD(&pb->pb_branches, b_next);
		free(b);
	}

	for (i = 0; i < nbr; i++) {
		if ((b = malloc(sizeof(*b))) == NULL)
			goto err;
		*b = brs[i];
		dbg_warnx("%s: b->b_nextparser %p [%s]", __func__,
		    b->b_nextparser, b->b_nextparser ? b->b_nextparser->p_name
		    : "(null)");
		SIMPLEQ_INSERT_TAIL(&pb->pb_branches, b, b_next);
	}

	return 0;
err:
	while ((b = SIMPLEQ_FIRST(&pb->pb_branches)) != NULL) {
		SIMPLEQ_REMOVE_HEAD(&pb->pb_branches, b_next);
		free(b);
	}
	return -1;
}

static int
pbranch_init(struct parser *p)
{
	struct branch *b;
	struct pbranch *pb = (struct pbranch *)p;
	struct parser *np;

	if (pb->pb_nbrinit == 0)
		;
	else if (pbranch_setbranches(pb, pb->pb_brinit, pb->pb_nbrinit) == -1)
		return -1;

	pb->pb_nbrinit = 0;

	SIMPLEQ_FOREACH(b, &pb->pb_branches, b_next) {
		np = b->b_nextparser;
		if (np != NULL && parser_init(np) == -1)
			return -1;
	}
	return 0;
}

struct pbranch *
pbranch_create(const char *name, const struct branch *brs, size_t nbr,
    bool match_first)
{
	struct pbranch *pb;

	dbg_warnx("%s: nbr %zu", __func__, nbr);

	if ((pb = calloc(1, sizeof(*pb))) == NULL)
		return NULL;

	pb->pb_parser.p_methods = &pbranch_methods;
	pb->pb_parser.p_name = name;

	SIMPLEQ_INIT(&pb->pb_branches);

	if (pbranch_setbranches(pb, brs, nbr) == -1)
		goto post_pb_err;

	pb->pb_match_first = match_first;
	return pb;
post_pb_err:
	free(pb);
	return NULL;
}

static int
parser_default_init(struct parser *p)
{
	struct parser *np;

	np = p->p_nextparser;
	if (np != NULL && parser_init(np) == -1)
		return -1;

	return 0;
}

static int
pkw_setwords(struct pkw *pk, parser_exec_t defexec, const char *defkey,
    const struct kwinst *kws, size_t nkw, struct parser *defnext)
{
	struct kwinst *k;
	size_t i;

	for (i = 0; i < nkw; i++) {
		if (kws[i].k_word == NULL)
			continue;
		if ((k = malloc(sizeof(*k))) == NULL)
			goto post_pk_err;
		*k = kws[i];
		if (k->k_nextparser == NULL)
			k->k_nextparser = defnext;
		if (k->k_key == NULL)
			k->k_key = defkey;
		if (k->k_exec == NULL)
			k->k_exec = defexec;
		SIMPLEQ_INSERT_TAIL(&pk->pk_keywords, k, k_next);
	}
	return 0;

post_pk_err:
	while ((k = SIMPLEQ_FIRST(&pk->pk_keywords)) != NULL) {
		SIMPLEQ_REMOVE_HEAD(&pk->pk_keywords, k_next);
		free(k);
	}
	return -1;
}

static int
pkw_init(struct parser *p)
{
	struct kwinst *k;
	struct pkw *pk = (struct pkw *)p;
	struct parser *np;

	if (pk->pk_nkwinit == 0)
		;
	else if (pkw_setwords(pk, pk->pk_execinit, pk->pk_keyinit,
	    pk->pk_kwinit, pk->pk_nkwinit, pk->pk_nextinit) == -1)
		return -1;

	pk->pk_nkwinit = 0;

	SIMPLEQ_FOREACH(k, &pk->pk_keywords, k_next) {
		np = k->k_nextparser;
		if (np != NULL && parser_init(np) == -1)
			return -1;
	}
	return 0;
}

struct pkw *
pkw_create(const char *name, parser_exec_t defexec, const char *defkey,
    const struct kwinst *kws, size_t nkw, struct parser *defnext)
{
	struct pkw *pk;

	if ((pk = calloc(1, sizeof(*pk))) == NULL)
		return NULL;

	pk->pk_parser.p_methods = &pkw_methods;
	pk->pk_parser.p_exec = defexec;
	pk->pk_parser.p_name = name;

	SIMPLEQ_INIT(&pk->pk_keywords);

	if (pkw_setwords(pk, defexec, defkey, kws, nkw, defnext) == -1)
		goto err;

	return pk;
err:
	free(pk);
	return NULL;
}

int
parse(int argc, char **argv, const struct parser *p0, struct match *matches,
    size_t *nmatch, int *narg)
{
	int i, rc = 0;
	struct match *lastm = NULL, *m = matches;
	const struct parser *p = p0;

	for (i = 0; i < argc && p != NULL; i++) {
		if ((size_t)(m - matches) >= *nmatch) {
			errno = EFBIG;
			rc = -1;
			break;
		}
		rc = (*p->p_methods->pm_match)(p, lastm, m, i, argv[i]);
		if (rc != 0)
			goto out;
		p = m->m_nextparser;
		lastm = m++;
	}
	for (; (size_t)(m - matches) < *nmatch && p != NULL; ) {
		rc = (*p->p_methods->pm_match)(p, lastm, m, i, NULL);
		if (rc != 0)
			break;
		p = m->m_nextparser;
		lastm = m++;
	}
out:
	*nmatch = m - matches;
	*narg = i;
	return rc;
}

int
matches_exec(const struct match *matches, prop_dictionary_t oenv, size_t nmatch)
{
	size_t i;
	int rc = 0;
	const struct match *m;
	parser_exec_t pexec;
	prop_dictionary_t d;

	for (i = 0; i < nmatch; i++) {
		m = &matches[i];
		dbg_warnx("%s.%d: i %zu", __func__, __LINE__, i);
		pexec = (m->m_parser->p_exec != NULL)
		    ? m->m_parser->p_exec : m->m_exec;
		if (pexec == NULL)
			continue;
		dbg_warnx("%s.%d: m->m_parser->p_name %s", __func__, __LINE__,
		    m->m_parser->p_name);
		d = prop_dictionary_augment(m->m_env, oenv);
		rc = (*pexec)(d, oenv);
		prop_object_release((prop_object_t)d);
		if (rc == -1)
			break;
	}
	return rc;
}

int
parser_init(struct parser *p)
{
	if (p->p_initialized)
		return 0;
	p->p_initialized = true;
	if (p->p_methods->pm_init == NULL)
		return 0;
	return (*p->p_methods->pm_init)(p);
}
