/*
 * Copyright (C) 2004-2011  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 2000-2003  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: nsupdate.c,v 1.193.12.3 2011-05-23 22:12:14 each Exp $ */

/*! \file */

#include <config.h>

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>

#include <isc/app.h>
#include <isc/base64.h>
#include <isc/buffer.h>
#include <isc/commandline.h>
#include <isc/entropy.h>
#include <isc/event.h>
#include <isc/file.h>
#include <isc/hash.h>
#include <isc/lex.h>
#include <isc/log.h>
#include <isc/mem.h>
#include <isc/parseint.h>
#include <isc/print.h>
#include <isc/random.h>
#include <isc/region.h>
#include <isc/sockaddr.h>
#include <isc/socket.h>
#include <isc/stdio.h>
#include <isc/string.h>
#include <isc/task.h>
#include <isc/timer.h>
#include <isc/types.h>
#include <isc/util.h>

#include <isccfg/namedconf.h>

#include <dns/callbacks.h>
#include <dns/dispatch.h>
#include <dns/dnssec.h>
#include <dns/events.h>
#include <dns/fixedname.h>
#include <dns/log.h>
#include <dns/masterdump.h>
#include <dns/message.h>
#include <dns/name.h>
#include <dns/rcode.h>
#include <dns/rdata.h>
#include <dns/rdataclass.h>
#include <dns/rdatalist.h>
#include <dns/rdataset.h>
#include <dns/rdatastruct.h>
#include <dns/rdatatype.h>
#include <dns/request.h>
#include <dns/result.h>
#include <dns/tkey.h>
#include <dns/tsig.h>

#include <dst/dst.h>

#include <lwres/lwres.h>
#include <lwres/net.h>

#ifdef GSSAPI
#include <dst/gssapi.h>
#include ISC_PLATFORM_KRB5HEADER
#endif
#include <bind9/getaddresses.h>


#ifdef HAVE_ADDRINFO
#ifdef HAVE_GETADDRINFO
#ifdef HAVE_GAISTRERROR
#define USE_GETADDRINFO
#endif
#endif
#endif

#ifndef USE_GETADDRINFO
#ifndef ISC_PLATFORM_NONSTDHERRNO
extern int h_errno;
#endif
#endif

#define MAXCMD (4 * 1024)
#define MAXWIRE (64 * 1024)
#define PACKETSIZE ((64 * 1024) - 1)
#define INITTEXT (2 * 1024)
#define MAXTEXT (128 * 1024)
#define FIND_TIMEOUT 5
#define TTL_MAX 2147483647U	/* Maximum signed 32 bit integer. */

#define DNSDEFAULTPORT 53

static isc_uint16_t dnsport = DNSDEFAULTPORT;

#ifndef RESOLV_CONF
#define RESOLV_CONF "/etc/resolv.conf"
#endif

static isc_boolean_t debugging = ISC_FALSE, ddebugging = ISC_FALSE;
static isc_boolean_t memdebugging = ISC_FALSE;
static isc_boolean_t have_ipv4 = ISC_FALSE;
static isc_boolean_t have_ipv6 = ISC_FALSE;
static isc_boolean_t is_dst_up = ISC_FALSE;
static isc_boolean_t usevc = ISC_FALSE;
static isc_boolean_t usegsstsig = ISC_FALSE;
static isc_boolean_t use_win2k_gsstsig = ISC_FALSE;
static isc_boolean_t tried_other_gsstsig = ISC_FALSE;
static isc_boolean_t local_only = ISC_FALSE;
static isc_taskmgr_t *taskmgr = NULL;
static isc_task_t *global_task = NULL;
static isc_event_t *global_event = NULL;
static isc_log_t *lctx = NULL;
static isc_mem_t *mctx = NULL;
static dns_dispatchmgr_t *dispatchmgr = NULL;
static dns_requestmgr_t *requestmgr = NULL;
static isc_socketmgr_t *socketmgr = NULL;
static isc_timermgr_t *timermgr = NULL;
static dns_dispatch_t *dispatchv4 = NULL;
static dns_dispatch_t *dispatchv6 = NULL;
static dns_message_t *updatemsg = NULL;
static dns_fixedname_t fuserzone;
static dns_name_t *userzone = NULL;
static dns_name_t *zonename = NULL;
static dns_name_t tmpzonename;
static dns_name_t restart_master;
static dns_tsig_keyring_t *gssring = NULL;
static dns_tsigkey_t *tsigkey = NULL;
static dst_key_t *sig0key = NULL;
static lwres_context_t *lwctx = NULL;
static lwres_conf_t *lwconf;
static isc_sockaddr_t *servers;
static int ns_inuse = 0;
static int ns_total = 0;
static isc_sockaddr_t *userserver = NULL;
static isc_sockaddr_t *localaddr = NULL;
static isc_sockaddr_t *serveraddr = NULL;
static isc_sockaddr_t tempaddr;
static const char *keyfile = NULL;
static char *keystr = NULL;
static isc_entropy_t *entropy = NULL;
static isc_boolean_t shuttingdown = ISC_FALSE;
static FILE *input;
static isc_boolean_t interactive = ISC_TRUE;
static isc_boolean_t seenerror = ISC_FALSE;
static const dns_master_style_t *style;
static int requests = 0;
static unsigned int logdebuglevel = 0;
static unsigned int timeout = 300;
static unsigned int udp_timeout = 3;
static unsigned int udp_retries = 3;
static dns_rdataclass_t defaultclass = dns_rdataclass_in;
static dns_rdataclass_t zoneclass = dns_rdataclass_none;
static dns_message_t *answer = NULL;
static isc_uint32_t default_ttl = 0;
static isc_boolean_t default_ttl_set = ISC_FALSE;

typedef struct nsu_requestinfo {
	dns_message_t *msg;
	isc_sockaddr_t *addr;
} nsu_requestinfo_t;

static void
sendrequest(isc_sockaddr_t *srcaddr, isc_sockaddr_t *destaddr,
	    dns_message_t *msg, dns_request_t **request);

ISC_PLATFORM_NORETURN_PRE static void
fatal(const char *format, ...)
ISC_FORMAT_PRINTF(1, 2) ISC_PLATFORM_NORETURN_POST;

static void
debug(const char *format, ...) ISC_FORMAT_PRINTF(1, 2);

static void
ddebug(const char *format, ...) ISC_FORMAT_PRINTF(1, 2);

#ifdef GSSAPI
static dns_fixedname_t fkname;
static isc_sockaddr_t *kserver = NULL;
static char *realm = NULL;
static char servicename[DNS_NAME_FORMATSIZE];
static dns_name_t *keyname;
typedef struct nsu_gssinfo {
	dns_message_t *msg;
	isc_sockaddr_t *addr;
	gss_ctx_id_t context;
} nsu_gssinfo_t;

static void
start_gssrequest(dns_name_t *master);
static void
send_gssrequest(isc_sockaddr_t *srcaddr, isc_sockaddr_t *destaddr,
		dns_message_t *msg, dns_request_t **request,
		gss_ctx_id_t context);
static void
recvgss(isc_task_t *task, isc_event_t *event);
#endif /* GSSAPI */

static void
error(const char *format, ...) ISC_FORMAT_PRINTF(1, 2);

#define STATUS_MORE	(isc_uint16_t)0
#define STATUS_SEND	(isc_uint16_t)1
#define STATUS_QUIT	(isc_uint16_t)2
#define STATUS_SYNTAX	(isc_uint16_t)3

typedef struct entropysource entropysource_t;

struct entropysource {
	isc_entropysource_t *source;
	isc_mem_t *mctx;
	ISC_LINK(entropysource_t) link;
};

static ISC_LIST(entropysource_t) sources;

static void
setup_entropy(isc_mem_t *mctx, const char *randomfile, isc_entropy_t **ectx)
{
	isc_result_t result;
	isc_entropysource_t *source = NULL;
	entropysource_t *elt;
	int usekeyboard = ISC_ENTROPY_KEYBOARDMAYBE;

	REQUIRE(ectx != NULL);

	if (*ectx == NULL) {
		result = isc_entropy_create(mctx, ectx);
		if (result != ISC_R_SUCCESS)
			fatal("could not create entropy object");
		ISC_LIST_INIT(sources);
	}

	if (randomfile != NULL && strcmp(randomfile, "keyboard") == 0) {
		usekeyboard = ISC_ENTROPY_KEYBOARDYES;
		randomfile = NULL;
	}

	result = isc_entropy_usebestsource(*ectx, &source, randomfile,
					   usekeyboard);

	if (result != ISC_R_SUCCESS)
		fatal("could not initialize entropy source: %s",
		      isc_result_totext(result));

	if (source != NULL) {
		elt = isc_mem_get(mctx, sizeof(*elt));
		if (elt == NULL)
			fatal("out of memory");
		elt->source = source;
		elt->mctx = mctx;
		ISC_LINK_INIT(elt, link);
		ISC_LIST_APPEND(sources, elt, link);
	}
}

static void
cleanup_entropy(isc_entropy_t **ectx) {
	entropysource_t *source;
	while (!ISC_LIST_EMPTY(sources)) {
		source = ISC_LIST_HEAD(sources);
		ISC_LIST_UNLINK(sources, source, link);
		isc_entropy_destroysource(&source->source);
		isc_mem_put(source->mctx, source, sizeof(*source));
	}
	isc_entropy_detach(ectx);
}


static dns_rdataclass_t
getzoneclass(void) {
	if (zoneclass == dns_rdataclass_none)
		zoneclass = defaultclass;
	return (zoneclass);
}

static isc_boolean_t
setzoneclass(dns_rdataclass_t rdclass) {
	if (zoneclass == dns_rdataclass_none ||
	    rdclass == dns_rdataclass_none)
		zoneclass = rdclass;
	if (zoneclass != rdclass)
		return (ISC_FALSE);
	return (ISC_TRUE);
}

static void
fatal(const char *format, ...) {
	va_list args;

	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
	fprintf(stderr, "\n");
	exit(1);
}

static void
error(const char *format, ...) {
	va_list args;

	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
	fprintf(stderr, "\n");
}

static void
debug(const char *format, ...) {
	va_list args;

	if (debugging) {
		va_start(args, format);
		vfprintf(stderr, format, args);
		va_end(args);
		fprintf(stderr, "\n");
	}
}

static void
ddebug(const char *format, ...) {
	va_list args;

	if (ddebugging) {
		va_start(args, format);
		vfprintf(stderr, format, args);
		va_end(args);
		fprintf(stderr, "\n");
	}
}

static inline void
check_result(isc_result_t result, const char *msg) {
	if (result != ISC_R_SUCCESS)
		fatal("%s: %s", msg, isc_result_totext(result));
}

static void *
mem_alloc(void *arg, size_t size) {
	return (isc_mem_get(arg, size));
}

static void
mem_free(void *arg, void *mem, size_t size) {
	isc_mem_put(arg, mem, size);
}

static char *
nsu_strsep(char **stringp, const char *delim) {
	char *string = *stringp;
	char *s;
	const char *d;
	char sc, dc;

	if (string == NULL)
		return (NULL);

	for (; *string != '\0'; string++) {
		sc = *string;
		for (d = delim; (dc = *d) != '\0'; d++) {
			if (sc == dc)
				break;
		}
		if (dc == 0)
			break;
	}

	for (s = string; *s != '\0'; s++) {
		sc = *s;
		for (d = delim; (dc = *d) != '\0'; d++) {
			if (sc == dc) {
				*s++ = '\0';
				*stringp = s;
				return (string);
			}
		}
	}
	*stringp = NULL;
	return (string);
}

static void
reset_system(void) {
	isc_result_t result;

	ddebug("reset_system()");
	/* If the update message is still around, destroy it */
	if (updatemsg != NULL)
		dns_message_reset(updatemsg, DNS_MESSAGE_INTENTRENDER);
	else {
		result = dns_message_create(mctx, DNS_MESSAGE_INTENTRENDER,
					    &updatemsg);
		check_result(result, "dns_message_create");
	}
	updatemsg->opcode = dns_opcode_update;
	if (usegsstsig) {
		if (tsigkey != NULL)
			dns_tsigkey_detach(&tsigkey);
		if (gssring != NULL)
			dns_tsigkeyring_detach(&gssring);
		tried_other_gsstsig = ISC_FALSE;
	}
}

static isc_uint16_t
parse_hmac(dns_name_t **hmac, const char *hmacstr, size_t len) {
	isc_uint16_t digestbits = 0;
	isc_result_t result;
	char buf[20];

	REQUIRE(hmac != NULL && *hmac == NULL);
	REQUIRE(hmacstr != NULL);

	if (len >= sizeof(buf))
		fatal("unknown key type '%.*s'", (int)(len), hmacstr);

	strncpy(buf, hmacstr, len);
	buf[len] = 0;

	if (strcasecmp(buf, "hmac-md5") == 0) {
		*hmac = DNS_TSIG_HMACMD5_NAME;
	} else if (strncasecmp(buf, "hmac-md5-", 9) == 0) {
		*hmac = DNS_TSIG_HMACMD5_NAME;
		result = isc_parse_uint16(&digestbits, &buf[9], 10);
		if (result != ISC_R_SUCCESS || digestbits > 128)
			fatal("digest-bits out of range [0..128]");
		digestbits = (digestbits +7) & ~0x7U;
	} else if (strcasecmp(buf, "hmac-sha1") == 0) {
		*hmac = DNS_TSIG_HMACSHA1_NAME;
	} else if (strncasecmp(buf, "hmac-sha1-", 10) == 0) {
		*hmac = DNS_TSIG_HMACSHA1_NAME;
		result = isc_parse_uint16(&digestbits, &buf[10], 10);
		if (result != ISC_R_SUCCESS || digestbits > 160)
			fatal("digest-bits out of range [0..160]");
		digestbits = (digestbits +7) & ~0x7U;
	} else if (strcasecmp(buf, "hmac-sha224") == 0) {
		*hmac = DNS_TSIG_HMACSHA224_NAME;
	} else if (strncasecmp(buf, "hmac-sha224-", 12) == 0) {
		*hmac = DNS_TSIG_HMACSHA224_NAME;
		result = isc_parse_uint16(&digestbits, &buf[12], 10);
		if (result != ISC_R_SUCCESS || digestbits > 224)
			fatal("digest-bits out of range [0..224]");
		digestbits = (digestbits +7) & ~0x7U;
	} else if (strcasecmp(buf, "hmac-sha256") == 0) {
		*hmac = DNS_TSIG_HMACSHA256_NAME;
	} else if (strncasecmp(buf, "hmac-sha256-", 12) == 0) {
		*hmac = DNS_TSIG_HMACSHA256_NAME;
		result = isc_parse_uint16(&digestbits, &buf[12], 10);
		if (result != ISC_R_SUCCESS || digestbits > 256)
			fatal("digest-bits out of range [0..256]");
		digestbits = (digestbits +7) & ~0x7U;
	} else if (strcasecmp(buf, "hmac-sha384") == 0) {
		*hmac = DNS_TSIG_HMACSHA384_NAME;
	} else if (strncasecmp(buf, "hmac-sha384-", 12) == 0) {
		*hmac = DNS_TSIG_HMACSHA384_NAME;
		result = isc_parse_uint16(&digestbits, &buf[12], 10);
		if (result != ISC_R_SUCCESS || digestbits > 384)
			fatal("digest-bits out of range [0..384]");
		digestbits = (digestbits +7) & ~0x7U;
	} else if (strcasecmp(buf, "hmac-sha512") == 0) {
		*hmac = DNS_TSIG_HMACSHA512_NAME;
	} else if (strncasecmp(buf, "hmac-sha512-", 12) == 0) {
		*hmac = DNS_TSIG_HMACSHA512_NAME;
		result = isc_parse_uint16(&digestbits, &buf[12], 10);
		if (result != ISC_R_SUCCESS || digestbits > 512)
			fatal("digest-bits out of range [0..512]");
		digestbits = (digestbits +7) & ~0x7U;
	} else
		fatal("unknown key type '%s'", buf);
	return (digestbits);
}

static int
basenamelen(const char *file) {
	int len = strlen(file);

	if (len > 1 && file[len - 1] == '.')
		len -= 1;
	else if (len > 8 && strcmp(file + len - 8, ".private") == 0)
		len -= 8;
	else if (len > 4 && strcmp(file + len - 4, ".key") == 0)
		len -= 4;
	return (len);
}

static void
setup_keystr(void) {
	unsigned char *secret = NULL;
	int secretlen;
	isc_buffer_t secretbuf;
	isc_result_t result;
	isc_buffer_t keynamesrc;
	char *secretstr;
	char *s, *n;
	dns_fixedname_t fkeyname;
	dns_name_t *keyname;
	char *name;
	dns_name_t *hmacname = NULL;
	isc_uint16_t digestbits = 0;

	dns_fixedname_init(&fkeyname);
	keyname = dns_fixedname_name(&fkeyname);

	debug("Creating key...");

	s = strchr(keystr, ':');
	if (s == NULL || s == keystr || s[1] == 0)
		fatal("key option must specify [hmac:]keyname:secret");
	secretstr = s + 1;
	n = strchr(secretstr, ':');
	if (n != NULL) {
		if (n == secretstr || n[1] == 0)
			fatal("key option must specify [hmac:]keyname:secret");
		name = secretstr;
		secretstr = n + 1;
		digestbits = parse_hmac(&hmacname, keystr, s - keystr);
	} else {
		hmacname = DNS_TSIG_HMACMD5_NAME;
		name = keystr;
		n = s;
	}

	isc_buffer_init(&keynamesrc, name, n - name);
	isc_buffer_add(&keynamesrc, n - name);

	debug("namefromtext");
	result = dns_name_fromtext(keyname, &keynamesrc, dns_rootname, 0, NULL);
	check_result(result, "dns_name_fromtext");

	secretlen = strlen(secretstr) * 3 / 4;
	secret = isc_mem_allocate(mctx, secretlen);
	if (secret == NULL)
		fatal("out of memory");

	isc_buffer_init(&secretbuf, secret, secretlen);
	result = isc_base64_decodestring(secretstr, &secretbuf);
	if (result != ISC_R_SUCCESS) {
		fprintf(stderr, "could not create key from %s: %s\n",
			keystr, isc_result_totext(result));
		goto failure;
	}

	secretlen = isc_buffer_usedlength(&secretbuf);

	debug("keycreate");
	result = dns_tsigkey_create(keyname, hmacname, secret, secretlen,
				    ISC_FALSE, NULL, 0, 0, mctx, NULL,
				    &tsigkey);
	if (result != ISC_R_SUCCESS)
		fprintf(stderr, "could not create key from %s: %s\n",
			keystr, dns_result_totext(result));
	else
		dst_key_setbits(tsigkey->key, digestbits);
 failure:
	if (secret != NULL)
		isc_mem_free(mctx, secret);
}

/*
 * Get a key from a named.conf format keyfile
 */
static isc_result_t
read_sessionkey(isc_mem_t *mctx, isc_log_t *lctx) {
	cfg_parser_t *pctx = NULL;
	cfg_obj_t *sessionkey = NULL;
	const cfg_obj_t *key = NULL;
	const cfg_obj_t *secretobj = NULL;
	const cfg_obj_t *algorithmobj = NULL;
	const char *keyname;
	const char *secretstr;
	const char *algorithm;
	isc_result_t result;
	int len;

	if (! isc_file_exists(keyfile))
		return (ISC_R_FILENOTFOUND);

	result = cfg_parser_create(mctx, lctx, &pctx);
	if (result != ISC_R_SUCCESS)
		goto cleanup;

	result = cfg_parse_file(pctx, keyfile, &cfg_type_sessionkey,
				&sessionkey);
	if (result != ISC_R_SUCCESS)
		goto cleanup;

	result = cfg_map_get(sessionkey, "key", &key);
	if (result != ISC_R_SUCCESS)
		goto cleanup;

	(void) cfg_map_get(key, "secret", &secretobj);
	(void) cfg_map_get(key, "algorithm", &algorithmobj);
	if (secretobj == NULL || algorithmobj == NULL)
		fatal("key must have algorithm and secret");

	keyname = cfg_obj_asstring(cfg_map_getname(key));
	secretstr = cfg_obj_asstring(secretobj);
	algorithm = cfg_obj_asstring(algorithmobj);

	len = strlen(algorithm) + strlen(keyname) + strlen(secretstr) + 3;
	keystr = isc_mem_allocate(mctx, len);
	snprintf(keystr, len, "%s:%s:%s", algorithm, keyname, secretstr);
	setup_keystr();

 cleanup:
	if (pctx != NULL) {
		if (sessionkey != NULL)
			cfg_obj_destroy(pctx, &sessionkey);
		cfg_parser_destroy(&pctx);
	}

	if (keystr != NULL)
		isc_mem_free(mctx, keystr);

	return (result);
}

static void
setup_keyfile(isc_mem_t *mctx, isc_log_t *lctx) {
	dst_key_t *dstkey = NULL;
	isc_result_t result;
	dns_name_t *hmacname = NULL;

	debug("Creating key...");

	if (sig0key != NULL)
		dst_key_free(&sig0key);

	/* Try reading the key from a K* pair */
	result = dst_key_fromnamedfile(keyfile, NULL,
				       DST_TYPE_PRIVATE | DST_TYPE_KEY, mctx,
				       &dstkey);

	/* If that didn't work, try reading it as a session.key keyfile */
	if (result != ISC_R_SUCCESS) {
		result = read_sessionkey(mctx, lctx);
		if (result == ISC_R_SUCCESS)
			return;
	}

	if (result != ISC_R_SUCCESS) {
		fprintf(stderr, "could not read key from %.*s.{private,key}: "
				"%s\n", basenamelen(keyfile), keyfile,
				isc_result_totext(result));
		return;
	}

	switch (dst_key_alg(dstkey)) {
	case DST_ALG_HMACMD5:
		hmacname = DNS_TSIG_HMACMD5_NAME;
		break;
	case DST_ALG_HMACSHA1:
		hmacname = DNS_TSIG_HMACSHA1_NAME;
		break;
	case DST_ALG_HMACSHA224:
		hmacname = DNS_TSIG_HMACSHA224_NAME;
		break;
	case DST_ALG_HMACSHA256:
		hmacname = DNS_TSIG_HMACSHA256_NAME;
		break;
	case DST_ALG_HMACSHA384:
		hmacname = DNS_TSIG_HMACSHA384_NAME;
		break;
	case DST_ALG_HMACSHA512:
		hmacname = DNS_TSIG_HMACSHA512_NAME;
		break;
	}
	if (hmacname != NULL) {
		result = dns_tsigkey_createfromkey(dst_key_name(dstkey),
						   hmacname, dstkey, ISC_FALSE,
						   NULL, 0, 0, mctx, NULL,
						   &tsigkey);
		dst_key_free(&dstkey);
		if (result != ISC_R_SUCCESS) {
			fprintf(stderr, "could not create key from %s: %s\n",
				keyfile, isc_result_totext(result));
			return;
		}
	} else {
		dst_key_attach(dstkey, &sig0key);
		dst_key_free(&dstkey);
	}
}

static void
doshutdown(void) {
	isc_task_detach(&global_task);

	if (userserver != NULL)
		isc_mem_put(mctx, userserver, sizeof(isc_sockaddr_t));

	if (localaddr != NULL)
		isc_mem_put(mctx, localaddr, sizeof(isc_sockaddr_t));

	if (tsigkey != NULL) {
		ddebug("Freeing TSIG key");
		dns_tsigkey_detach(&tsigkey);
	}

	if (sig0key != NULL) {
		ddebug("Freeing SIG(0) key");
		dst_key_free(&sig0key);
	}

	if (updatemsg != NULL)
		dns_message_destroy(&updatemsg);

	if (is_dst_up) {
		ddebug("Destroy DST lib");
		dst_lib_destroy();
		is_dst_up = ISC_FALSE;
	}

	cleanup_entropy(&entropy);

	lwres_conf_clear(lwctx);
	lwres_context_destroy(&lwctx);

	isc_mem_put(mctx, servers, ns_total * sizeof(isc_sockaddr_t));

	ddebug("Destroying request manager");
	dns_requestmgr_detach(&requestmgr);

	ddebug("Freeing the dispatchers");
	if (have_ipv4)
		dns_dispatch_detach(&dispatchv4);
	if (have_ipv6)
		dns_dispatch_detach(&dispatchv6);

	ddebug("Shutting down dispatch manager");
	dns_dispatchmgr_destroy(&dispatchmgr);

}

static void
maybeshutdown(void) {
	ddebug("Shutting down request manager");
	dns_requestmgr_shutdown(requestmgr);

	if (requests != 0)
		return;

	doshutdown();
}

static void
shutdown_program(isc_task_t *task, isc_event_t *event) {
	REQUIRE(task == global_task);
	UNUSED(task);

	ddebug("shutdown_program()");
	isc_event_free(&event);

	shuttingdown = ISC_TRUE;
	maybeshutdown();
}

static void
setup_system(void) {
	isc_result_t result;
	isc_sockaddr_t bind_any, bind_any6;
	lwres_result_t lwresult;
	unsigned int attrs, attrmask;
	int i;
	isc_logconfig_t *logconfig = NULL;

	ddebug("setup_system()");

	dns_result_register();

	result = isc_net_probeipv4();
	if (result == ISC_R_SUCCESS)
		have_ipv4 = ISC_TRUE;

	result = isc_net_probeipv6();
	if (result == ISC_R_SUCCESS)
		have_ipv6 = ISC_TRUE;

	if (!have_ipv4 && !have_ipv6)
		fatal("could not find either IPv4 or IPv6");

	result = isc_log_create(mctx, &lctx, &logconfig);
	check_result(result, "isc_log_create");

	isc_log_setcontext(lctx);
	dns_log_init(lctx);
	dns_log_setcontext(lctx);

	result = isc_log_usechannel(logconfig, "default_debug", NULL, NULL);
	check_result(result, "isc_log_usechannel");

	isc_log_setdebuglevel(lctx, logdebuglevel);

	lwresult = lwres_context_create(&lwctx, mctx, mem_alloc, mem_free, 1);
	if (lwresult != LWRES_R_SUCCESS)
		fatal("lwres_context_create failed");

	(void)lwres_conf_parse(lwctx, RESOLV_CONF);
	lwconf = lwres_conf_get(lwctx);

	ns_total = lwconf->nsnext;
	if (ns_total <= 0) {
		/* No name servers in resolv.conf; default to loopback. */
		struct in_addr localhost;
		ns_total = 1;
		servers = isc_mem_get(mctx, ns_total * sizeof(isc_sockaddr_t));
		if (servers == NULL)
			fatal("out of memory");
		localhost.s_addr = htonl(INADDR_LOOPBACK);
		isc_sockaddr_fromin(&servers[0], &localhost, dnsport);
	} else {
		servers = isc_mem_get(mctx, ns_total * sizeof(isc_sockaddr_t));
		if (servers == NULL)
			fatal("out of memory");
		for (i = 0; i < ns_total; i++) {
			if (lwconf->nameservers[i].family == LWRES_ADDRTYPE_V4) {
				struct in_addr in4;
				memcpy(&in4, lwconf->nameservers[i].address, 4);
				isc_sockaddr_fromin(&servers[i], &in4, dnsport);
			} else {
				struct in6_addr in6;
				memcpy(&in6, lwconf->nameservers[i].address, 16);
				isc_sockaddr_fromin6(&servers[i], &in6,
						     dnsport);
			}
		}
	}

	setup_entropy(mctx, NULL, &entropy);

	result = isc_hash_create(mctx, entropy, DNS_NAME_MAXWIRE);
	check_result(result, "isc_hash_create");
	isc_hash_init();

	result = dns_dispatchmgr_create(mctx, entropy, &dispatchmgr);
	check_result(result, "dns_dispatchmgr_create");

	result = isc_socketmgr_create(mctx, &socketmgr);
	check_result(result, "dns_socketmgr_create");

	result = isc_timermgr_create(mctx, &timermgr);
	check_result(result, "dns_timermgr_create");

	result = isc_taskmgr_create(mctx, 1, 0, &taskmgr);
	check_result(result, "isc_taskmgr_create");

	result = isc_task_create(taskmgr, 0, &global_task);
	check_result(result, "isc_task_create");

	result = isc_task_onshutdown(global_task, shutdown_program, NULL);
	check_result(result, "isc_task_onshutdown");

	result = dst_lib_init(mctx, entropy, 0);
	check_result(result, "dst_lib_init");
	is_dst_up = ISC_TRUE;

	attrmask = DNS_DISPATCHATTR_UDP | DNS_DISPATCHATTR_TCP;
	attrmask |= DNS_DISPATCHATTR_IPV4 | DNS_DISPATCHATTR_IPV6;

	if (have_ipv6) {
		attrs = DNS_DISPATCHATTR_UDP;
		attrs |= DNS_DISPATCHATTR_MAKEQUERY;
		attrs |= DNS_DISPATCHATTR_IPV6;
		isc_sockaddr_any6(&bind_any6);
		result = dns_dispatch_getudp(dispatchmgr, socketmgr, taskmgr,
					     &bind_any6, PACKETSIZE,
					     4, 2, 3, 5,
					     attrs, attrmask, &dispatchv6);
		check_result(result, "dns_dispatch_getudp (v6)");
	}

	if (have_ipv4) {
		attrs = DNS_DISPATCHATTR_UDP;
		attrs |= DNS_DISPATCHATTR_MAKEQUERY;
		attrs |= DNS_DISPATCHATTR_IPV4;
		isc_sockaddr_any(&bind_any);
		result = dns_dispatch_getudp(dispatchmgr, socketmgr, taskmgr,
					     &bind_any, PACKETSIZE,
					     4, 2, 3, 5,
					     attrs, attrmask, &dispatchv4);
		check_result(result, "dns_dispatch_getudp (v4)");
	}

	result = dns_requestmgr_create(mctx, timermgr,
				       socketmgr, taskmgr, dispatchmgr,
				       dispatchv4, dispatchv6, &requestmgr);
	check_result(result, "dns_requestmgr_create");

	if (keystr != NULL)
		setup_keystr();
	else if (local_only) {
		result = read_sessionkey(mctx, lctx);
		if (result != ISC_R_SUCCESS)
			fatal("can't read key from %s: %s\n",
			      keyfile, isc_result_totext(result));
	} else if (keyfile != NULL)
		setup_keyfile(mctx, lctx);
}

static void
get_address(char *host, in_port_t port, isc_sockaddr_t *sockaddr) {
	int count;
	isc_result_t result;

	isc_app_block();
	result = bind9_getaddresses(host, port, sockaddr, 1, &count);
	isc_app_unblock();
	if (result != ISC_R_SUCCESS)
		fatal("couldn't get address for '%s': %s",
		      host, isc_result_totext(result));
	INSIST(count == 1);
}

#define PARSE_ARGS_FMT "dDML:y:ghlovk:p:rR::t:u:"

static void
pre_parse_args(int argc, char **argv) {
	int ch;

	while ((ch = isc_commandline_parse(argc, argv, PARSE_ARGS_FMT)) != -1) {
		switch (ch) {
		case 'M': /* was -dm */
			debugging = ISC_TRUE;
			ddebugging = ISC_TRUE;
			memdebugging = ISC_TRUE;
			isc_mem_debugging = ISC_MEM_DEBUGTRACE |
					    ISC_MEM_DEBUGRECORD;
			break;

		case '?':
		case 'h':
			if (isc_commandline_option != '?')
				fprintf(stderr, "%s: invalid argument -%c\n",
					argv[0], isc_commandline_option);
			fprintf(stderr, "usage: nsupdate [-dD] [-L level] [-l]"
				"[-g | -o | -y keyname:secret | -k keyfile] "
				"[-v] [filename]\n");
			exit(1);

		default:
			break;
		}
	}
	isc_commandline_reset = ISC_TRUE;
	isc_commandline_index = 1;
}

static void
parse_args(int argc, char **argv, isc_mem_t *mctx, isc_entropy_t **ectx) {
	int ch;
	isc_uint32_t i;
	isc_result_t result;

	debug("parse_args");
	while ((ch = isc_commandline_parse(argc, argv, PARSE_ARGS_FMT)) != -1) {
		switch (ch) {
		case 'd':
			debugging = ISC_TRUE;
			break;
		case 'D': /* was -dd */
			debugging = ISC_TRUE;
			ddebugging = ISC_TRUE;
			break;
		case 'M':
			break;
		case 'l':
			local_only = ISC_TRUE;
			break;
		case 'L':
			result = isc_parse_uint32(&i, isc_commandline_argument,
						  10);
			if (result != ISC_R_SUCCESS) {
				fprintf(stderr, "bad library debug value "
					"'%s'\n", isc_commandline_argument);
				exit(1);
			}
			logdebuglevel = i;
			break;
		case 'y':
			keystr = isc_commandline_argument;
			break;
		case 'v':
			usevc = ISC_TRUE;
			break;
		case 'k':
			keyfile = isc_commandline_argument;
			break;
		case 'g':
			usegsstsig = ISC_TRUE;
			use_win2k_gsstsig = ISC_FALSE;
			break;
		case 'o':
			usegsstsig = ISC_TRUE;
			use_win2k_gsstsig = ISC_TRUE;
			break;
		case 'p':
			result = isc_parse_uint16(&dnsport,
						  isc_commandline_argument, 10);
			if (result != ISC_R_SUCCESS) {
				fprintf(stderr, "bad port number "
					"'%s'\n", isc_commandline_argument);
				exit(1);
			}
			break;
		case 't':
			result = isc_parse_uint32(&timeout,
						  isc_commandline_argument, 10);
			if (result != ISC_R_SUCCESS) {
				fprintf(stderr, "bad timeout '%s'\n",						isc_commandline_argument);
				exit(1);
			}
			if (timeout == 0)
				timeout = UINT_MAX;
			break;
		case 'u':
			result = isc_parse_uint32(&udp_timeout,
						  isc_commandline_argument, 10);
			if (result != ISC_R_SUCCESS) {
				fprintf(stderr, "bad udp timeout '%s'\n",						isc_commandline_argument);
				exit(1);
			}
			if (udp_timeout == 0)
				udp_timeout = UINT_MAX;
			break;
		case 'r':
			result = isc_parse_uint32(&udp_retries,
						  isc_commandline_argument, 10);
			if (result != ISC_R_SUCCESS) {
				fprintf(stderr, "bad udp retries '%s'\n",						isc_commandline_argument);
				exit(1);
			}
			break;

		case 'R':
			setup_entropy(mctx, isc_commandline_argument, ectx);
			break;

		default:
			fprintf(stderr, "%s: unhandled option: %c\n",
				argv[0], isc_commandline_option);
			exit(1);
		}
	}
	if (keyfile != NULL && keystr != NULL) {
		fprintf(stderr, "%s: cannot specify both -k and -y\n",
			argv[0]);
		exit(1);
	}

	if (local_only) {
		struct in_addr localhost;

		if (keyfile == NULL)
			keyfile = SESSION_KEYFILE;

		if (userserver == NULL) {
			userserver = isc_mem_get(mctx, sizeof(isc_sockaddr_t));
			if (userserver == NULL)
				fatal("out of memory");
		}

		localhost.s_addr = htonl(INADDR_LOOPBACK);
		isc_sockaddr_fromin(userserver, &localhost, dnsport);
	}

#ifdef GSSAPI
	if (usegsstsig && (keyfile != NULL || keystr != NULL)) {
		fprintf(stderr, "%s: cannot specify -g with -k or -y\n",
			argv[0]);
		exit(1);
	}
#else
	if (usegsstsig) {
		fprintf(stderr, "%s: cannot specify -g	or -o, " \
			"program not linked with GSS API Library\n",
			argv[0]);
		exit(1);
	}
#endif

	if (argv[isc_commandline_index] != NULL) {
		if (strcmp(argv[isc_commandline_index], "-") == 0) {
			input = stdin;
		} else {
			result = isc_stdio_open(argv[isc_commandline_index],
						"r", &input);
			if (result != ISC_R_SUCCESS) {
				fprintf(stderr, "could not open '%s': %s\n",
					argv[isc_commandline_index],
					isc_result_totext(result));
				exit(1);
			}
		}
		interactive = ISC_FALSE;
	}
}

static isc_uint16_t
parse_name(char **cmdlinep, dns_message_t *msg, dns_name_t **namep) {
	isc_result_t result;
	char *word;
	isc_buffer_t *namebuf = NULL;
	isc_buffer_t source;

	word = nsu_strsep(cmdlinep, " \t\r\n");
	if (*word == 0) {
		fprintf(stderr, "could not read owner name\n");
		return (STATUS_SYNTAX);
	}

	result = dns_message_gettempname(msg, namep);
	check_result(result, "dns_message_gettempname");
	result = isc_buffer_allocate(mctx, &namebuf, DNS_NAME_MAXWIRE);
	check_result(result, "isc_buffer_allocate");
	dns_name_init(*namep, NULL);
	dns_name_setbuffer(*namep, namebuf);
	dns_message_takebuffer(msg, &namebuf);
	isc_buffer_init(&source, word, strlen(word));
	isc_buffer_add(&source, strlen(word));
	result = dns_name_fromtext(*namep, &source, dns_rootname, 0, NULL);
	check_result(result, "dns_name_fromtext");
	isc_buffer_invalidate(&source);
	return (STATUS_MORE);
}

static isc_uint16_t
parse_rdata(char **cmdlinep, dns_rdataclass_t rdataclass,
	    dns_rdatatype_t rdatatype, dns_message_t *msg,
	    dns_rdata_t *rdata)
{
	char *cmdline = *cmdlinep;
	isc_buffer_t source, *buf = NULL, *newbuf = NULL;
	isc_region_t r;
	isc_lex_t *lex = NULL;
	dns_rdatacallbacks_t callbacks;
	isc_result_t result;

	while (*cmdline != 0 && isspace((unsigned char)*cmdline))
		cmdline++;

	if (*cmdline != 0) {
		dns_rdatacallbacks_init(&callbacks);
		result = isc_lex_create(mctx, strlen(cmdline), &lex);
		check_result(result, "isc_lex_create");
		isc_buffer_init(&source, cmdline, strlen(cmdline));
		isc_buffer_add(&source, strlen(cmdline));
		result = isc_lex_openbuffer(lex, &source);
		check_result(result, "isc_lex_openbuffer");
		result = isc_buffer_allocate(mctx, &buf, MAXWIRE);
		check_result(result, "isc_buffer_allocate");
		result = dns_rdata_fromtext(NULL, rdataclass, rdatatype, lex,
					    dns_rootname, 0, mctx, buf,
					    &callbacks);
		isc_lex_destroy(&lex);
		if (result == ISC_R_SUCCESS) {
			isc_buffer_usedregion(buf, &r);
			result = isc_buffer_allocate(mctx, &newbuf, r.length);
			check_result(result, "isc_buffer_allocate");
			isc_buffer_putmem(newbuf, r.base, r.length);
			isc_buffer_usedregion(newbuf, &r);
			dns_rdata_fromregion(rdata, rdataclass, rdatatype, &r);
			isc_buffer_free(&buf);
			dns_message_takebuffer(msg, &newbuf);
		} else {
			fprintf(stderr, "invalid rdata format: %s\n",
				isc_result_totext(result));
			isc_buffer_free(&buf);
			return (STATUS_SYNTAX);
		}
	} else {
		rdata->flags = DNS_RDATA_UPDATE;
	}
	*cmdlinep = cmdline;
	return (STATUS_MORE);
}

static isc_uint16_t
make_prereq(char *cmdline, isc_boolean_t ispositive, isc_boolean_t isrrset) {
	isc_result_t result;
	char *word;
	dns_name_t *name = NULL;
	isc_textregion_t region;
	dns_rdataset_t *rdataset = NULL;
	dns_rdatalist_t *rdatalist = NULL;
	dns_rdataclass_t rdataclass;
	dns_rdatatype_t rdatatype;
	dns_rdata_t *rdata = NULL;
	isc_uint16_t retval;

	ddebug("make_prereq()");

	/*
	 * Read the owner name
	 */
	retval = parse_name(&cmdline, updatemsg, &name);
	if (retval != STATUS_MORE)
		return (retval);

	/*
	 * If this is an rrset prereq, read the class or type.
	 */
	if (isrrset) {
		word = nsu_strsep(&cmdline, " \t\r\n");
		if (*word == 0) {
			fprintf(stderr, "could not read class or type\n");
			goto failure;
		}
		region.base = word;
		region.length = strlen(word);
		result = dns_rdataclass_fromtext(&rdataclass, &region);
		if (result == ISC_R_SUCCESS) {
			if (!setzoneclass(rdataclass)) {
				fprintf(stderr, "class mismatch: %s\n", word);
				goto failure;
			}
			/*
			 * Now read the type.
			 */
			word = nsu_strsep(&cmdline, " \t\r\n");
			if (*word == 0) {
				fprintf(stderr, "could not read type\n");
				goto failure;
			}
			region.base = word;
			region.length = strlen(word);
			result = dns_rdatatype_fromtext(&rdatatype, &region);
			if (result != ISC_R_SUCCESS) {
				fprintf(stderr, "invalid type: %s\n", word);
				goto failure;
			}
		} else {
			rdataclass = getzoneclass();
			result = dns_rdatatype_fromtext(&rdatatype, &region);
			if (result != ISC_R_SUCCESS) {
				fprintf(stderr, "invalid type: %s\n", word);
				goto failure;
			}
		}
	} else
		rdatatype = dns_rdatatype_any;

	result = dns_message_gettemprdata(updatemsg, &rdata);
	check_result(result, "dns_message_gettemprdata");

	dns_rdata_init(rdata);

	if (isrrset && ispositive) {
		retval = parse_rdata(&cmdline, rdataclass, rdatatype,
				     updatemsg, rdata);
		if (retval != STATUS_MORE)
			goto failure;
	} else
		rdata->flags = DNS_RDATA_UPDATE;

	result = dns_message_gettemprdatalist(updatemsg, &rdatalist);
	check_result(result, "dns_message_gettemprdatalist");
	result = dns_message_gettemprdataset(updatemsg, &rdataset);
	check_result(result, "dns_message_gettemprdataset");
	dns_rdatalist_init(rdatalist);
	rdatalist->type = rdatatype;
	if (ispositive) {
		if (isrrset && rdata->data != NULL)
			rdatalist->rdclass = rdataclass;
		else
			rdatalist->rdclass = dns_rdataclass_any;
	} else
		rdatalist->rdclass = dns_rdataclass_none;
	rdatalist->covers = 0;
	rdatalist->ttl = 0;
	rdata->rdclass = rdatalist->rdclass;
	rdata->type = rdatatype;
	ISC_LIST_INIT(rdatalist->rdata);
	ISC_LIST_APPEND(rdatalist->rdata, rdata, link);
	dns_rdataset_init(rdataset);
	dns_rdatalist_tordataset(rdatalist, rdataset);
	ISC_LIST_INIT(name->list);
	ISC_LIST_APPEND(name->list, rdataset, link);
	dns_message_addname(updatemsg, name, DNS_SECTION_PREREQUISITE);
	return (STATUS_MORE);

 failure:
	if (name != NULL)
		dns_message_puttempname(updatemsg, &name);
	return (STATUS_SYNTAX);
}

static isc_uint16_t
evaluate_prereq(char *cmdline) {
	char *word;
	isc_boolean_t ispositive, isrrset;

	ddebug("evaluate_prereq()");
	word = nsu_strsep(&cmdline, " \t\r\n");
	if (*word == 0) {
		fprintf(stderr, "could not read operation code\n");
		return (STATUS_SYNTAX);
	}
	if (strcasecmp(word, "nxdomain") == 0) {
		ispositive = ISC_FALSE;
		isrrset = ISC_FALSE;
	} else if (strcasecmp(word, "yxdomain") == 0) {
		ispositive = ISC_TRUE;
		isrrset = ISC_FALSE;
	} else if (strcasecmp(word, "nxrrset") == 0) {
		ispositive = ISC_FALSE;
		isrrset = ISC_TRUE;
	} else if (strcasecmp(word, "yxrrset") == 0) {
		ispositive = ISC_TRUE;
		isrrset = ISC_TRUE;
	} else {
		fprintf(stderr, "incorrect operation code: %s\n", word);
		return (STATUS_SYNTAX);
	}
	return (make_prereq(cmdline, ispositive, isrrset));
}

static isc_uint16_t
evaluate_server(char *cmdline) {
	char *word, *server;
	long port;

	if (local_only) {
		fprintf(stderr, "cannot reset server in localhost-only mode\n");
		return (STATUS_SYNTAX);
	}

	word = nsu_strsep(&cmdline, " \t\r\n");
	if (*word == 0) {
		fprintf(stderr, "could not read server name\n");
		return (STATUS_SYNTAX);
	}
	server = word;

	word = nsu_strsep(&cmdline, " \t\r\n");
	if (*word == 0)
		port = dnsport;
	else {
		char *endp;
		port = strtol(word, &endp, 10);
		if (*endp != 0) {
			fprintf(stderr, "port '%s' is not numeric\n", word);
			return (STATUS_SYNTAX);
		} else if (port < 1 || port > 65535) {
			fprintf(stderr, "port '%s' is out of range "
				"(1 to 65535)\n", word);
			return (STATUS_SYNTAX);
		}
	}

	if (userserver == NULL) {
		userserver = isc_mem_get(mctx, sizeof(isc_sockaddr_t));
		if (userserver == NULL)
			fatal("out of memory");
	}

	get_address(server, (in_port_t)port, userserver);

	return (STATUS_MORE);
}

static isc_uint16_t
evaluate_local(char *cmdline) {
	char *word, *local;
	long port;
	struct in_addr in4;
	struct in6_addr in6;

	word = nsu_strsep(&cmdline, " \t\r\n");
	if (*word == 0) {
		fprintf(stderr, "could not read server name\n");
		return (STATUS_SYNTAX);
	}
	local = word;

	word = nsu_strsep(&cmdline, " \t\r\n");
	if (*word == 0)
		port = 0;
	else {
		char *endp;
		port = strtol(word, &endp, 10);
		if (*endp != 0) {
			fprintf(stderr, "port '%s' is not numeric\n", word);
			return (STATUS_SYNTAX);
		} else if (port < 1 || port > 65535) {
			fprintf(stderr, "port '%s' is out of range "
				"(1 to 65535)\n", word);
			return (STATUS_SYNTAX);
		}
	}

	if (localaddr == NULL) {
		localaddr = isc_mem_get(mctx, sizeof(isc_sockaddr_t));
		if (localaddr == NULL)
			fatal("out of memory");
	}

	if (have_ipv6 && inet_pton(AF_INET6, local, &in6) == 1)
		isc_sockaddr_fromin6(localaddr, &in6, (in_port_t)port);
	else if (have_ipv4 && inet_pton(AF_INET, local, &in4) == 1)
		isc_sockaddr_fromin(localaddr, &in4, (in_port_t)port);
	else {
		fprintf(stderr, "invalid address %s", local);
		return (STATUS_SYNTAX);
	}

	return (STATUS_MORE);
}

static isc_uint16_t
evaluate_key(char *cmdline) {
	char *namestr;
	char *secretstr;
	isc_buffer_t b;
	isc_result_t result;
	dns_fixedname_t fkeyname;
	dns_name_t *keyname;
	int secretlen;
	unsigned char *secret = NULL;
	isc_buffer_t secretbuf;
	dns_name_t *hmacname = NULL;
	isc_uint16_t digestbits = 0;
	char *n;

	namestr = nsu_strsep(&cmdline, " \t\r\n");
	if (*namestr == 0) {
		fprintf(stderr, "could not read key name\n");
		return (STATUS_SYNTAX);
	}

	dns_fixedname_init(&fkeyname);
	keyname = dns_fixedname_name(&fkeyname);

	n = strchr(namestr, ':');
	if (n != NULL) {
		digestbits = parse_hmac(&hmacname, namestr, n - namestr);
		namestr = n + 1;
	} else
		hmacname = DNS_TSIG_HMACMD5_NAME;

	isc_buffer_init(&b, namestr, strlen(namestr));
	isc_buffer_add(&b, strlen(namestr));
	result = dns_name_fromtext(keyname, &b, dns_rootname, 0, NULL);
	if (result != ISC_R_SUCCESS) {
		fprintf(stderr, "could not parse key name\n");
		return (STATUS_SYNTAX);
	}

	secretstr = nsu_strsep(&cmdline, "\r\n");
	if (*secretstr == 0) {
		fprintf(stderr, "could not read key secret\n");
		return (STATUS_SYNTAX);
	}
	secretlen = strlen(secretstr) * 3 / 4;
	secret = isc_mem_allocate(mctx, secretlen);
	if (secret == NULL)
		fatal("out of memory");

	isc_buffer_init(&secretbuf, secret, secretlen);
	result = isc_base64_decodestring(secretstr, &secretbuf);
	if (result != ISC_R_SUCCESS) {
		fprintf(stderr, "could not create key from %s: %s\n",
			secretstr, isc_result_totext(result));
		isc_mem_free(mctx, secret);
		return (STATUS_SYNTAX);
	}
	secretlen = isc_buffer_usedlength(&secretbuf);

	if (tsigkey != NULL)
		dns_tsigkey_detach(&tsigkey);
	result = dns_tsigkey_create(keyname, hmacname, secret, secretlen,
				    ISC_FALSE, NULL, 0, 0, mctx, NULL,
				    &tsigkey);
	isc_mem_free(mctx, secret);
	if (result != ISC_R_SUCCESS) {
		fprintf(stderr, "could not create key from %s %s: %s\n",
			namestr, secretstr, dns_result_totext(result));
		return (STATUS_SYNTAX);
	}
	dst_key_setbits(tsigkey->key, digestbits);
	return (STATUS_MORE);
}

static isc_uint16_t
evaluate_zone(char *cmdline) {
	char *word;
	isc_buffer_t b;
	isc_result_t result;

	word = nsu_strsep(&cmdline, " \t\r\n");
	if (*word == 0) {
		fprintf(stderr, "could not read zone name\n");
		return (STATUS_SYNTAX);
	}

	dns_fixedname_init(&fuserzone);
	userzone = dns_fixedname_name(&fuserzone);
	isc_buffer_init(&b, word, strlen(word));
	isc_buffer_add(&b, strlen(word));
	result = dns_name_fromtext(userzone, &b, dns_rootname, 0, NULL);
	if (result != ISC_R_SUCCESS) {
		userzone = NULL; /* Lest it point to an invalid name */
		fprintf(stderr, "could not parse zone name\n");
		return (STATUS_SYNTAX);
	}

	return (STATUS_MORE);
}

static isc_uint16_t
evaluate_realm(char *cmdline) {
#ifdef GSSAPI
	char *word;
	char buf[1024];

	word = nsu_strsep(&cmdline, " \t\r\n");
	if (*word == 0) {
		if (realm != NULL)
			isc_mem_free(mctx, realm);
		realm = NULL;
		return (STATUS_MORE);
	}

	snprintf(buf, sizeof(buf), "@%s", word);
	realm = isc_mem_strdup(mctx, buf);
	if (realm == NULL)
		fatal("out of memory");
	return (STATUS_MORE);
#else
	UNUSED(cmdline);
	return (STATUS_SYNTAX);
#endif
}

static isc_uint16_t
evaluate_ttl(char *cmdline) {
	char *word;
	isc_result_t result;
	isc_uint32_t ttl;

	word = nsu_strsep(&cmdline, " \t\r\n");
	if (*word == 0) {
		fprintf(stderr, "could not ttl\n");
		return (STATUS_SYNTAX);
	}

	if (!strcasecmp(word, "none")) {
		default_ttl = 0;
		default_ttl_set = ISC_FALSE;
		return (STATUS_MORE);
	}

	result = isc_parse_uint32(&ttl, word, 10);
	if (result != ISC_R_SUCCESS)
		return (STATUS_SYNTAX);

	if (ttl > TTL_MAX) {
		fprintf(stderr, "ttl '%s' is out of range (0 to %u)\n",
			word, TTL_MAX);
		return (STATUS_SYNTAX);
	}
	default_ttl = ttl;
	default_ttl_set = ISC_TRUE;

	return (STATUS_MORE);
}

static isc_uint16_t
evaluate_class(char *cmdline) {
	char *word;
	isc_textregion_t r;
	isc_result_t result;
	dns_rdataclass_t rdclass;

	word = nsu_strsep(&cmdline, " \t\r\n");
	if (*word == 0) {
		fprintf(stderr, "could not read class name\n");
		return (STATUS_SYNTAX);
	}

	r.base = word;
	r.length = strlen(word);
	result = dns_rdataclass_fromtext(&rdclass, &r);
	if (result != ISC_R_SUCCESS) {
		fprintf(stderr, "could not parse class name: %s\n", word);
		return (STATUS_SYNTAX);
	}
	switch (rdclass) {
	case dns_rdataclass_none:
	case dns_rdataclass_any:
	case dns_rdataclass_reserved0:
		fprintf(stderr, "bad default class: %s\n", word);
		return (STATUS_SYNTAX);
	default:
		defaultclass = rdclass;
	}

	return (STATUS_MORE);
}

static isc_uint16_t
update_addordelete(char *cmdline, isc_boolean_t isdelete) {
	isc_result_t result;
	dns_name_t *name = NULL;
	isc_uint32_t ttl;
	char *word;
	dns_rdataclass_t rdataclass;
	dns_rdatatype_t rdatatype;
	dns_rdata_t *rdata = NULL;
	dns_rdatalist_t *rdatalist = NULL;
	dns_rdataset_t *rdataset = NULL;
	isc_textregion_t region;
	isc_uint16_t retval;

	ddebug("update_addordelete()");

	/*
	 * Read the owner name.
	 */
	retval = parse_name(&cmdline, updatemsg, &name);
	if (retval != STATUS_MORE)
		return (retval);

	result = dns_message_gettemprdata(updatemsg, &rdata);
	check_result(result, "dns_message_gettemprdata");

	dns_rdata_init(rdata);

	/*
	 * If this is an add, read the TTL and verify that it's in range.
	 * If it's a delete, ignore a TTL if present (for compatibility).
	 */
	word = nsu_strsep(&cmdline, " \t\r\n");
	if (*word == 0) {
		if (!isdelete) {
			fprintf(stderr, "could not read owner ttl\n");
			goto failure;
		}
		else {
			ttl = 0;
			rdataclass = dns_rdataclass_any;
			rdatatype = dns_rdatatype_any;
			rdata->flags = DNS_RDATA_UPDATE;
			goto doneparsing;
		}
	}
	result = isc_parse_uint32(&ttl, word, 10);
	if (result != ISC_R_SUCCESS) {
		if (isdelete) {
			ttl = 0;
			goto parseclass;
		} else if (default_ttl_set) {
			ttl = default_ttl;
			goto parseclass;
		} else {
			fprintf(stderr, "ttl '%s': %s\n", word,
				isc_result_totext(result));
			goto failure;
		}
	}

	if (isdelete)
		ttl = 0;
	else if (ttl > TTL_MAX) {
		fprintf(stderr, "ttl '%s' is out of range (0 to %u)\n",
			word, TTL_MAX);
		goto failure;
	}

	/*
	 * Read the class or type.
	 */
	word = nsu_strsep(&cmdline, " \t\r\n");
 parseclass:
	if (*word == 0) {
		if (isdelete) {
			rdataclass = dns_rdataclass_any;
			rdatatype = dns_rdatatype_any;
			rdata->flags = DNS_RDATA_UPDATE;
			goto doneparsing;
		} else {
			fprintf(stderr, "could not read class or type\n");
			goto failure;
		}
	}
	region.base = word;
	region.length = strlen(word);
	rdataclass = dns_rdataclass_any;
	result = dns_rdataclass_fromtext(&rdataclass, &region);
	if (result == ISC_R_SUCCESS && rdataclass != dns_rdataclass_any) {
		if (!setzoneclass(rdataclass)) {
			fprintf(stderr, "class mismatch: %s\n", word);
			goto failure;
		}
		/*
		 * Now read the type.
		 */
		word = nsu_strsep(&cmdline, " \t\r\n");
		if (*word == 0) {
			if (isdelete) {
				rdataclass = dns_rdataclass_any;
				rdatatype = dns_rdatatype_any;
				rdata->flags = DNS_RDATA_UPDATE;
				goto doneparsing;
			} else {
				fprintf(stderr, "could not read type\n");
				goto failure;
			}
		}
		region.base = word;
		region.length = strlen(word);
		result = dns_rdatatype_fromtext(&rdatatype, &region);
		if (result != ISC_R_SUCCESS) {
			fprintf(stderr, "'%s' is not a valid type: %s\n",
				word, isc_result_totext(result));
			goto failure;
		}
	} else {
		rdataclass = getzoneclass();
		result = dns_rdatatype_fromtext(&rdatatype, &region);
		if (result != ISC_R_SUCCESS) {
			fprintf(stderr, "'%s' is not a valid class or type: "
				"%s\n", word, isc_result_totext(result));
			goto failure;
		}
	}

	retval = parse_rdata(&cmdline, rdataclass, rdatatype, updatemsg,
			     rdata);
	if (retval != STATUS_MORE)
		goto failure;

	if (isdelete) {
		if ((rdata->flags & DNS_RDATA_UPDATE) != 0)
			rdataclass = dns_rdataclass_any;
		else
			rdataclass = dns_rdataclass_none;
	} else {
		if ((rdata->flags & DNS_RDATA_UPDATE) != 0) {
			fprintf(stderr, "could not read rdata\n");
			goto failure;
		}
	}

 doneparsing:

	result = dns_message_gettemprdatalist(updatemsg, &rdatalist);
	check_result(result, "dns_message_gettemprdatalist");
	result = dns_message_gettemprdataset(updatemsg, &rdataset);
	check_result(result, "dns_message_gettemprdataset");
	dns_rdatalist_init(rdatalist);
	rdatalist->type = rdatatype;
	rdatalist->rdclass = rdataclass;
	rdatalist->covers = rdatatype;
	rdatalist->ttl = (dns_ttl_t)ttl;
	ISC_LIST_INIT(rdatalist->rdata);
	ISC_LIST_APPEND(rdatalist->rdata, rdata, link);
	dns_rdataset_init(rdataset);
	dns_rdatalist_tordataset(rdatalist, rdataset);
	ISC_LIST_INIT(name->list);
	ISC_LIST_APPEND(name->list, rdataset, link);
	dns_message_addname(updatemsg, name, DNS_SECTION_UPDATE);
	return (STATUS_MORE);

 failure:
	if (name != NULL)
		dns_message_puttempname(updatemsg, &name);
	dns_message_puttemprdata(updatemsg, &rdata);
	return (STATUS_SYNTAX);
}

static isc_uint16_t
evaluate_update(char *cmdline) {
	char *word;
	isc_boolean_t isdelete;

	ddebug("evaluate_update()");
	word = nsu_strsep(&cmdline, " \t\r\n");
	if (*word == 0) {
		fprintf(stderr, "could not read operation code\n");
		return (STATUS_SYNTAX);
	}
	if (strcasecmp(word, "delete") == 0)
		isdelete = ISC_TRUE;
	else if (strcasecmp(word, "add") == 0)
		isdelete = ISC_FALSE;
	else {
		fprintf(stderr, "incorrect operation code: %s\n", word);
		return (STATUS_SYNTAX);
	}
	return (update_addordelete(cmdline, isdelete));
}

static void
setzone(dns_name_t *zonename) {
	isc_result_t result;
	dns_name_t *name = NULL;
	dns_rdataset_t *rdataset = NULL;

	result = dns_message_firstname(updatemsg, DNS_SECTION_ZONE);
	if (result == ISC_R_SUCCESS) {
		dns_message_currentname(updatemsg, DNS_SECTION_ZONE, &name);
		dns_message_removename(updatemsg, name, DNS_SECTION_ZONE);
		for (rdataset = ISC_LIST_HEAD(name->list);
		     rdataset != NULL;
		     rdataset = ISC_LIST_HEAD(name->list)) {
			ISC_LIST_UNLINK(name->list, rdataset, link);
			dns_rdataset_disassociate(rdataset);
			dns_message_puttemprdataset(updatemsg, &rdataset);
		}
		dns_message_puttempname(updatemsg, &name);
	}

	if (zonename != NULL) {
		result = dns_message_gettempname(updatemsg, &name);
		check_result(result, "dns_message_gettempname");
		dns_name_init(name, NULL);
		dns_name_clone(zonename, name);
		result = dns_message_gettemprdataset(updatemsg, &rdataset);
		check_result(result, "dns_message_gettemprdataset");
		dns_rdataset_makequestion(rdataset, getzoneclass(),
					  dns_rdatatype_soa);
		ISC_LIST_INIT(name->list);
		ISC_LIST_APPEND(name->list, rdataset, link);
		dns_message_addname(updatemsg, name, DNS_SECTION_ZONE);
	}
}

static void
show_message(FILE *stream, dns_message_t *msg, const char *description) {
	isc_result_t result;
	isc_buffer_t *buf = NULL;
	int bufsz;

	ddebug("show_message()");

	setzone(userzone);

	bufsz = INITTEXT;
	do {
		if (bufsz > MAXTEXT) {
			fprintf(stderr, "could not allocate large enough "
				"buffer to display message\n");
			exit(1);
		}
		if (buf != NULL)
			isc_buffer_free(&buf);
		result = isc_buffer_allocate(mctx, &buf, bufsz);
		check_result(result, "isc_buffer_allocate");
		result = dns_message_totext(msg, style, 0, buf);
		bufsz *= 2;
	} while (result == ISC_R_NOSPACE);
	if (result != ISC_R_SUCCESS) {
		fprintf(stderr, "could not convert message to text format.\n");
		isc_buffer_free(&buf);
		return;
	}
	fprintf(stream, "%s\n%.*s", description,
	       (int)isc_buffer_usedlength(buf), (char*)isc_buffer_base(buf));
	isc_buffer_free(&buf);
}


static isc_uint16_t
get_next_command(void) {
	char cmdlinebuf[MAXCMD];
	char *cmdline;
	char *word;

	ddebug("get_next_command()");
	if (interactive) {
		fprintf(stdout, "> ");
		fflush(stdout);
	}
	isc_app_block();
	cmdline = fgets(cmdlinebuf, MAXCMD, input);
	isc_app_unblock();
	if (cmdline == NULL)
		return (STATUS_QUIT);
	word = nsu_strsep(&cmdline, " \t\r\n");

	if (feof(input))
		return (STATUS_QUIT);
	if (*word == 0)
		return (STATUS_SEND);
	if (word[0] == ';')
		return (STATUS_MORE);
	if (strcasecmp(word, "quit") == 0)
		return (STATUS_QUIT);
	if (strcasecmp(word, "prereq") == 0)
		return (evaluate_prereq(cmdline));
	if (strcasecmp(word, "update") == 0)
		return (evaluate_update(cmdline));
	if (strcasecmp(word, "server") == 0)
		return (evaluate_server(cmdline));
	if (strcasecmp(word, "local") == 0)
		return (evaluate_local(cmdline));
	if (strcasecmp(word, "zone") == 0)
		return (evaluate_zone(cmdline));
	if (strcasecmp(word, "class") == 0)
		return (evaluate_class(cmdline));
	if (strcasecmp(word, "send") == 0)
		return (STATUS_SEND);
	if (strcasecmp(word, "debug") == 0) {
		if (debugging)
			ddebugging = ISC_TRUE;
		else
			debugging = ISC_TRUE;
		return (STATUS_MORE);
	}
	if (strcasecmp(word, "ttl") == 0)
		return (evaluate_ttl(cmdline));
	if (strcasecmp(word, "show") == 0) {
		show_message(stdout, updatemsg, "Outgoing update query:");
		return (STATUS_MORE);
	}
	if (strcasecmp(word, "answer") == 0) {
		if (answer != NULL)
			show_message(stdout, answer, "Answer:");
		return (STATUS_MORE);
	}
	if (strcasecmp(word, "key") == 0) {
		usegsstsig = ISC_FALSE;
		return (evaluate_key(cmdline));
	}
	if (strcasecmp(word, "realm") == 0)
		return (evaluate_realm(cmdline));
	if (strcasecmp(word, "gsstsig") == 0) {
#ifdef GSSAPI
		usegsstsig = ISC_TRUE;
		use_win2k_gsstsig = ISC_FALSE;
#else
		fprintf(stderr, "gsstsig not supported\n");
#endif
		return (STATUS_MORE);
	}
	if (strcasecmp(word, "oldgsstsig") == 0) {
#ifdef GSSAPI
		usegsstsig = ISC_TRUE;
		use_win2k_gsstsig = ISC_TRUE;
#else
		fprintf(stderr, "gsstsig not supported\n");
#endif
		return (STATUS_MORE);
	}
	if (strcasecmp(word, "help") == 0) {
		fprintf(stdout,
"local address [port]      (set local resolver)\n"
"server address [port]     (set master server for zone)\n"
"send                      (send the update request)\n"
"show                      (show the update request)\n"
"answer                    (show the answer to the last request)\n"
"quit                      (quit, any pending update is not sent\n"
"help                      (display this message_\n"
"key [hmac:]keyname secret (use TSIG to sign the request)\n"
"gsstsig                   (use GSS_TSIG to sign the request)\n"
"oldgsstsig                (use Microsoft's GSS_TSIG to sign the request)\n"
"zone name                 (set the zone to be updated)\n"
"class CLASS               (set the zone's DNS class, e.g. IN (default), CH)\n"
"prereq nxdomain name      (does this name not exist)\n"
"prereq yxdomain name      (does this name exist)\n"
"prereq nxrrset ....       (does this RRset exist)\n"
"prereq yxrrset ....       (does this RRset not exist)\n"
"update add ....           (add the given record to the zone)\n"
"update delete ....        (remove the given record(s) from the zone)\n");
		return (STATUS_MORE);
	}
	fprintf(stderr, "incorrect section name: %s\n", word);
	return (STATUS_SYNTAX);
}

static isc_boolean_t
user_interaction(void) {
	isc_uint16_t result = STATUS_MORE;

	ddebug("user_interaction()");
	while ((result == STATUS_MORE) || (result == STATUS_SYNTAX)) {
		result = get_next_command();
		if (!interactive && result == STATUS_SYNTAX)
			fatal("syntax error");
	}
	if (result == STATUS_SEND)
		return (ISC_TRUE);
	return (ISC_FALSE);

}

static void
done_update(void) {
	isc_event_t *event = global_event;
	ddebug("done_update()");
	isc_task_send(global_task, &event);
}

static void
check_tsig_error(dns_rdataset_t *rdataset, isc_buffer_t *b) {
	isc_result_t result;
	dns_rdata_t rdata = DNS_RDATA_INIT;
	dns_rdata_any_tsig_t tsig;

	result = dns_rdataset_first(rdataset);
	check_result(result, "dns_rdataset_first");
	dns_rdataset_current(rdataset, &rdata);
	result = dns_rdata_tostruct(&rdata, &tsig, NULL);
	check_result(result, "dns_rdata_tostruct");
	if (tsig.error != 0) {
		if (isc_buffer_remaininglength(b) < 1)
		      check_result(ISC_R_NOSPACE, "isc_buffer_remaininglength");
		isc__buffer_putstr(b, "(" /*)*/);
		result = dns_tsigrcode_totext(tsig.error, b);
		check_result(result, "dns_tsigrcode_totext");
		if (isc_buffer_remaininglength(b) < 1)
		      check_result(ISC_R_NOSPACE, "isc_buffer_remaininglength");
		isc__buffer_putstr(b,  /*(*/ ")");
	}
}

static void
update_completed(isc_task_t *task, isc_event_t *event) {
	dns_requestevent_t *reqev = NULL;
	isc_result_t result;
	dns_request_t *request;

	UNUSED(task);

	ddebug("update_completed()");

	requests--;

	REQUIRE(event->ev_type == DNS_EVENT_REQUESTDONE);
	reqev = (dns_requestevent_t *)event;
	request = reqev->request;

	if (shuttingdown) {
		dns_request_destroy(&request);
		isc_event_free(&event);
		maybeshutdown();
		return;
	}

	if (reqev->result != ISC_R_SUCCESS) {
		fprintf(stderr, "; Communication with server failed: %s\n",
			isc_result_totext(reqev->result));
		seenerror = ISC_TRUE;
		goto done;
	}

	result = dns_message_create(mctx, DNS_MESSAGE_INTENTPARSE, &answer);
	check_result(result, "dns_message_create");
	result = dns_request_getresponse(request, answer,
					 DNS_MESSAGEPARSE_PRESERVEORDER);
	switch (result) {
	case ISC_R_SUCCESS:
		if (answer->verify_attempted)
			ddebug("tsig verification successful");
		break;
	case DNS_R_CLOCKSKEW:
	case DNS_R_EXPECTEDTSIG:
	case DNS_R_TSIGERRORSET:
	case DNS_R_TSIGVERIFYFAILURE:
	case DNS_R_UNEXPECTEDTSIG:
	case ISC_R_FAILURE:
#if 0
		if (usegsstsig && answer->rcode == dns_rcode_noerror) {
			/*
			 * For MS DNS that violates RFC 2845, section 4.2
			 */
			break;
		}
#endif
		fprintf(stderr, "; TSIG error with server: %s\n",
			isc_result_totext(result));
		seenerror = ISC_TRUE;
		break;
	default:
		check_result(result, "dns_request_getresponse");
	}

	if (answer->rcode != dns_rcode_noerror) {
		seenerror = ISC_TRUE;
		if (!debugging) {
			char buf[64];
			isc_buffer_t b;
			dns_rdataset_t *rds;

			isc_buffer_init(&b, buf, sizeof(buf) - 1);
			result = dns_rcode_totext(answer->rcode, &b);
			check_result(result, "dns_rcode_totext");
			rds = dns_message_gettsig(answer, NULL);
			if (rds != NULL)
				check_tsig_error(rds, &b);
			fprintf(stderr, "update failed: %.*s\n",
				(int)isc_buffer_usedlength(&b), buf);
		}
	}
	if (debugging)
		show_message(stderr, answer, "\nReply from update query:");

 done:
	dns_request_destroy(&request);
	if (usegsstsig) {
		dns_name_free(&tmpzonename, mctx);
		dns_name_free(&restart_master, mctx);
	}
	isc_event_free(&event);
	done_update();
}

static void
send_update(dns_name_t *zonename, isc_sockaddr_t *master,
	    isc_sockaddr_t *srcaddr)
{
	isc_result_t result;
	dns_request_t *request = NULL;
	unsigned int options = DNS_REQUESTOPT_CASE;

	ddebug("send_update()");

	setzone(zonename);

	if (usevc)
		options |= DNS_REQUESTOPT_TCP;
	if (tsigkey == NULL && sig0key != NULL) {
		result = dns_message_setsig0key(updatemsg, sig0key);
		check_result(result, "dns_message_setsig0key");
	}
	if (debugging) {
		char addrbuf[ISC_SOCKADDR_FORMATSIZE];

		isc_sockaddr_format(master, addrbuf, sizeof(addrbuf));
		fprintf(stderr, "Sending update to %s\n", addrbuf);
	}

	/* Windows doesn't like the tsig name to be compressed. */
	if (updatemsg->tsigname)
		updatemsg->tsigname->attributes |= DNS_NAMEATTR_NOCOMPRESS;

	result = dns_request_createvia3(requestmgr, updatemsg, srcaddr,
					master, options, tsigkey, timeout,
					udp_timeout, udp_retries, global_task,
					update_completed, NULL, &request);
	check_result(result, "dns_request_createvia3");

	if (debugging)
		show_message(stdout, updatemsg, "Outgoing update query:");

	requests++;
}

static void
recvsoa(isc_task_t *task, isc_event_t *event) {
	dns_requestevent_t *reqev = NULL;
	dns_request_t *request = NULL;
	isc_result_t result, eresult;
	dns_message_t *rcvmsg = NULL;
	dns_section_t section;
	dns_name_t *name = NULL;
	dns_rdataset_t *soaset = NULL;
	dns_rdata_soa_t soa;
	dns_rdata_t soarr = DNS_RDATA_INIT;
	int pass = 0;
	dns_name_t master;
	nsu_requestinfo_t *reqinfo;
	dns_message_t *soaquery = NULL;
	isc_sockaddr_t *addr;
	isc_boolean_t seencname = ISC_FALSE;
	dns_name_t tname;
	unsigned int nlabels;

	UNUSED(task);

	ddebug("recvsoa()");

	requests--;

	REQUIRE(event->ev_type == DNS_EVENT_REQUESTDONE);
	reqev = (dns_requestevent_t *)event;
	request = reqev->request;
	eresult = reqev->result;
	reqinfo = reqev->ev_arg;
	soaquery = reqinfo->msg;
	addr = reqinfo->addr;

	if (shuttingdown) {
		dns_request_destroy(&request);
		dns_message_destroy(&soaquery);
		isc_mem_put(mctx, reqinfo, sizeof(nsu_requestinfo_t));
		isc_event_free(&event);
		maybeshutdown();
		return;
	}

	if (eresult != ISC_R_SUCCESS) {
		char addrbuf[ISC_SOCKADDR_FORMATSIZE];

		isc_sockaddr_format(addr, addrbuf, sizeof(addrbuf));
		fprintf(stderr, "; Communication with %s failed: %s\n",
			addrbuf, isc_result_totext(eresult));
		if (userserver != NULL)
			fatal("could not talk to specified name server");
		else if (++ns_inuse >= lwconf->nsnext)
			fatal("could not talk to any default name server");
		ddebug("Destroying request [%p]", request);
		dns_request_destroy(&request);
		dns_message_renderreset(soaquery);
		dns_message_settsigkey(soaquery, NULL);
		sendrequest(localaddr, &servers[ns_inuse], soaquery, &request);
		isc_mem_put(mctx, reqinfo, sizeof(nsu_requestinfo_t));
		isc_event_free(&event);
		setzoneclass(dns_rdataclass_none);
		return;
	}

	isc_mem_put(mctx, reqinfo, sizeof(nsu_requestinfo_t));
	reqinfo = NULL;
	isc_event_free(&event);
	reqev = NULL;

	ddebug("About to create rcvmsg");
	result = dns_message_create(mctx, DNS_MESSAGE_INTENTPARSE, &rcvmsg);
	check_result(result, "dns_message_create");
	result = dns_request_getresponse(request, rcvmsg,
					 DNS_MESSAGEPARSE_PRESERVEORDER);
	if (result == DNS_R_TSIGERRORSET && userserver != NULL) {
		dns_message_destroy(&rcvmsg);
		ddebug("Destroying request [%p]", request);
		dns_request_destroy(&request);
		reqinfo = isc_mem_get(mctx, sizeof(nsu_requestinfo_t));
		if (reqinfo == NULL)
			fatal("out of memory");
		reqinfo->msg = soaquery;
		reqinfo->addr = addr;
		dns_message_renderreset(soaquery);
		ddebug("retrying soa request without TSIG");
		result = dns_request_createvia3(requestmgr, soaquery,
						localaddr, addr, 0, NULL,
						FIND_TIMEOUT * 20,
						FIND_TIMEOUT, 3,
						global_task, recvsoa, reqinfo,
						&request);
		check_result(result, "dns_request_createvia");
		requests++;
		return;
	}
	check_result(result, "dns_request_getresponse");
	section = DNS_SECTION_ANSWER;
	POST(section);
	if (debugging)
		show_message(stderr, rcvmsg, "Reply from SOA query:");

	if (rcvmsg->rcode != dns_rcode_noerror &&
	    rcvmsg->rcode != dns_rcode_nxdomain)
		fatal("response to SOA query was unsuccessful");

	if (userzone != NULL && rcvmsg->rcode == dns_rcode_nxdomain) {
		char namebuf[DNS_NAME_FORMATSIZE];
		dns_name_format(userzone, namebuf, sizeof(namebuf));
		error("specified zone '%s' does not exist (NXDOMAIN)",
		      namebuf);
		dns_message_destroy(&rcvmsg);
		dns_request_destroy(&request);
		dns_message_destroy(&soaquery);
		ddebug("Out of recvsoa");
		done_update();
		return;
	}

 lookforsoa:
	if (pass == 0)
		section = DNS_SECTION_ANSWER;
	else if (pass == 1)
		section = DNS_SECTION_AUTHORITY;
	else
		goto droplabel;

	result = dns_message_firstname(rcvmsg, section);
	if (result != ISC_R_SUCCESS) {
		pass++;
		goto lookforsoa;
	}
	while (result == ISC_R_SUCCESS) {
		name = NULL;
		dns_message_currentname(rcvmsg, section, &name);
		soaset = NULL;
		result = dns_message_findtype(name, dns_rdatatype_soa, 0,
					      &soaset);
		if (result == ISC_R_SUCCESS)
			break;
		if (section == DNS_SECTION_ANSWER) {
			dns_rdataset_t *tset = NULL;
			if (dns_message_findtype(name, dns_rdatatype_cname, 0,
						 &tset) == ISC_R_SUCCESS ||
			    dns_message_findtype(name, dns_rdatatype_dname, 0,
						 &tset) == ISC_R_SUCCESS ) {
				seencname = ISC_TRUE;
				break;
			}
		}

		result = dns_message_nextname(rcvmsg, section);
	}

	if (soaset == NULL && !seencname) {
		pass++;
		goto lookforsoa;
	}

	if (seencname)
		goto droplabel;

	if (debugging) {
		char namestr[DNS_NAME_FORMATSIZE];
		dns_name_format(name, namestr, sizeof(namestr));
		fprintf(stderr, "Found zone name: %s\n", namestr);
	}

	result = dns_rdataset_first(soaset);
	check_result(result, "dns_rdataset_first");

	dns_rdata_init(&soarr);
	dns_rdataset_current(soaset, &soarr);
	result = dns_rdata_tostruct(&soarr, &soa, NULL);
	check_result(result, "dns_rdata_tostruct");

	dns_name_init(&master, NULL);
	dns_name_clone(&soa.origin, &master);

	if (userzone != NULL)
		zonename = userzone;
	else
		zonename = name;

	if (debugging) {
		char namestr[DNS_NAME_FORMATSIZE];
		dns_name_format(&master, namestr, sizeof(namestr));
		fprintf(stderr, "The master is: %s\n", namestr);
	}

	if (userserver != NULL)
		serveraddr = userserver;
	else {
		char serverstr[DNS_NAME_MAXTEXT+1];
		isc_buffer_t buf;

		isc_buffer_init(&buf, serverstr, sizeof(serverstr));
		result = dns_name_totext(&master, ISC_TRUE, &buf);
		check_result(result, "dns_name_totext");
		serverstr[isc_buffer_usedlength(&buf)] = 0;
		get_address(serverstr, dnsport, &tempaddr);
		serveraddr = &tempaddr;
	}
	dns_rdata_freestruct(&soa);

#ifdef GSSAPI
	if (usegsstsig) {
		dns_name_init(&tmpzonename, NULL);
		dns_name_dup(zonename, mctx, &tmpzonename);
		dns_name_init(&restart_master, NULL);
		dns_name_dup(&master, mctx, &restart_master);
		start_gssrequest(&master);
	} else {
		send_update(zonename, serveraddr, localaddr);
		setzoneclass(dns_rdataclass_none);
	}
#else
	send_update(zonename, serveraddr, localaddr);
	setzoneclass(dns_rdataclass_none);
#endif

	dns_message_destroy(&soaquery);
	dns_request_destroy(&request);

 out:
	dns_message_destroy(&rcvmsg);
	ddebug("Out of recvsoa");
	return;

 droplabel:
	result = dns_message_firstname(soaquery, DNS_SECTION_QUESTION);
	INSIST(result == ISC_R_SUCCESS);
	name = NULL;
	dns_message_currentname(soaquery, DNS_SECTION_QUESTION, &name);
	nlabels = dns_name_countlabels(name);
	if (nlabels == 1)
		fatal("could not find enclosing zone");
	dns_name_init(&tname, NULL);
	dns_name_getlabelsequence(name, 1, nlabels - 1, &tname);
	dns_name_clone(&tname, name);
	dns_request_destroy(&request);
	dns_message_renderreset(soaquery);
	dns_message_settsigkey(soaquery, NULL);
	if (userserver != NULL)
		sendrequest(localaddr, userserver, soaquery, &request);
	else
		sendrequest(localaddr, &servers[ns_inuse], soaquery, &request);
	goto out;
}

static void
sendrequest(isc_sockaddr_t *srcaddr, isc_sockaddr_t *destaddr,
	    dns_message_t *msg, dns_request_t **request)
{
	isc_result_t result;
	nsu_requestinfo_t *reqinfo;

	reqinfo = isc_mem_get(mctx, sizeof(nsu_requestinfo_t));
	if (reqinfo == NULL)
		fatal("out of memory");
	reqinfo->msg = msg;
	reqinfo->addr = destaddr;
	result = dns_request_createvia3(requestmgr, msg, srcaddr, destaddr, 0,
					(userserver != NULL) ? tsigkey : NULL,
					FIND_TIMEOUT * 20, FIND_TIMEOUT, 3,
					global_task, recvsoa, reqinfo, request);
	check_result(result, "dns_request_createvia");
	requests++;
}

#ifdef GSSAPI

/*
 * Get the realm from the users kerberos ticket if possible
 */
static void
get_ticket_realm(isc_mem_t *mctx)
{
	krb5_context ctx;
	krb5_error_code rc;
	krb5_ccache ccache;
	krb5_principal princ;
	char *name, *ticket_realm;

	rc = krb5_init_context(&ctx);
	if (rc != 0)
		return;

	rc = krb5_cc_default(ctx, &ccache);
	if (rc != 0) {
		krb5_free_context(ctx);
		return;
	}

	rc = krb5_cc_get_principal(ctx, ccache, &princ);
	if (rc != 0) {
		krb5_cc_close(ctx, ccache);
		krb5_free_context(ctx);
		return;
	}

	rc = krb5_unparse_name(ctx, princ, &name);
	if (rc != 0) {
		krb5_free_principal(ctx, princ);
		krb5_cc_close(ctx, ccache);
		krb5_free_context(ctx);
		return;
	}

	ticket_realm = strrchr(name, '@');
	if (ticket_realm != NULL) {
		realm = isc_mem_strdup(mctx, ticket_realm);
	}

	free(name);
	krb5_free_principal(ctx, princ);
	krb5_cc_close(ctx, ccache);
	krb5_free_context(ctx);
	if (realm != NULL && debugging)
		fprintf(stderr, "Found realm from ticket: %s\n", realm+1);
}


static void
start_gssrequest(dns_name_t *master) {
	gss_ctx_id_t context;
	isc_buffer_t buf;
	isc_result_t result;
	isc_uint32_t val = 0;
	dns_message_t *rmsg;
	dns_request_t *request = NULL;
	dns_name_t *servname;
	dns_fixedname_t fname;
	char namestr[DNS_NAME_FORMATSIZE];
	char keystr[DNS_NAME_FORMATSIZE];
	char *err_message = NULL;

	debug("start_gssrequest");
	usevc = ISC_TRUE;

	if (gssring != NULL)
		dns_tsigkeyring_detach(&gssring);
	gssring = NULL;
	result = dns_tsigkeyring_create(mctx, &gssring);

	if (result != ISC_R_SUCCESS)
		fatal("dns_tsigkeyring_create failed: %s",
		      isc_result_totext(result));

	dns_name_format(master, namestr, sizeof(namestr));
	if (kserver == NULL) {
		kserver = isc_mem_get(mctx, sizeof(isc_sockaddr_t));
		if (kserver == NULL)
			fatal("out of memory");
	}
	if (userserver == NULL)
		get_address(namestr, dnsport, kserver);
	else
		(void)memcpy(kserver, userserver, sizeof(isc_sockaddr_t));

	dns_fixedname_init(&fname);
	servname = dns_fixedname_name(&fname);

	if (realm == NULL)
		get_ticket_realm(mctx);

	result = isc_string_printf(servicename, sizeof(servicename),
				   "DNS/%s%s", namestr, realm ? realm : "");
	if (result != ISC_R_SUCCESS)
		fatal("isc_string_printf(servicename) failed: %s",
		      isc_result_totext(result));
	isc_buffer_init(&buf, servicename, strlen(servicename));
	isc_buffer_add(&buf, strlen(servicename));
	result = dns_name_fromtext(servname, &buf, dns_rootname, 0, NULL);
	if (result != ISC_R_SUCCESS)
		fatal("dns_name_fromtext(servname) failed: %s",
		      isc_result_totext(result));

	dns_fixedname_init(&fkname);
	keyname = dns_fixedname_name(&fkname);

	isc_random_get(&val);
	result = isc_string_printf(keystr, sizeof(keystr), "%u.sig-%s",
				   val, namestr);
	if (result != ISC_R_SUCCESS)
		fatal("isc_string_printf(keystr) failed: %s",
		      isc_result_totext(result));
	isc_buffer_init(&buf, keystr, strlen(keystr));
	isc_buffer_add(&buf, strlen(keystr));

	result = dns_name_fromtext(keyname, &buf, dns_rootname, 0, NULL);
	if (result != ISC_R_SUCCESS)
		fatal("dns_name_fromtext(keyname) failed: %s",
		      isc_result_totext(result));

	/* Windows doesn't recognize name compression in the key name. */
	keyname->attributes |= DNS_NAMEATTR_NOCOMPRESS;

	rmsg = NULL;
	result = dns_message_create(mctx, DNS_MESSAGE_INTENTRENDER, &rmsg);
	if (result != ISC_R_SUCCESS)
		fatal("dns_message_create failed: %s",
		      isc_result_totext(result));

	/* Build first request. */
	context = GSS_C_NO_CONTEXT;
	result = dns_tkey_buildgssquery(rmsg, keyname, servname, NULL, 0,
					&context, use_win2k_gsstsig,
					mctx, &err_message);
	if (result == ISC_R_FAILURE)
		fatal("tkey query failed: %s",
		      err_message != NULL ? err_message : "unknown error");
	if (result != ISC_R_SUCCESS)
		fatal("dns_tkey_buildgssquery failed: %s",
		      isc_result_totext(result));

	send_gssrequest(localaddr, kserver, rmsg, &request, context);
}

static void
send_gssrequest(isc_sockaddr_t *srcaddr, isc_sockaddr_t *destaddr,
		dns_message_t *msg, dns_request_t **request,
		gss_ctx_id_t context)
{
	isc_result_t result;
	nsu_gssinfo_t *reqinfo;
	unsigned int options = 0;

	debug("send_gssrequest");
	reqinfo = isc_mem_get(mctx, sizeof(nsu_gssinfo_t));
	if (reqinfo == NULL)
		fatal("out of memory");
	reqinfo->msg = msg;
	reqinfo->addr = destaddr;
	reqinfo->context = context;

	options |= DNS_REQUESTOPT_TCP;
	result = dns_request_createvia3(requestmgr, msg, srcaddr, destaddr,
					options, tsigkey, FIND_TIMEOUT * 20,
					FIND_TIMEOUT, 3, global_task, recvgss,
					reqinfo, request);
	check_result(result, "dns_request_createvia3");
	if (debugging)
		show_message(stdout, msg, "Outgoing update query:");
	requests++;
}

static void
recvgss(isc_task_t *task, isc_event_t *event) {
	dns_requestevent_t *reqev = NULL;
	dns_request_t *request = NULL;
	isc_result_t result, eresult;
	dns_message_t *rcvmsg = NULL;
	nsu_gssinfo_t *reqinfo;
	dns_message_t *tsigquery = NULL;
	isc_sockaddr_t *addr;
	gss_ctx_id_t context;
	isc_buffer_t buf;
	dns_name_t *servname;
	dns_fixedname_t fname;
	char *err_message = NULL;

	UNUSED(task);

	ddebug("recvgss()");

	requests--;

	REQUIRE(event->ev_type == DNS_EVENT_REQUESTDONE);
	reqev = (dns_requestevent_t *)event;
	request = reqev->request;
	eresult = reqev->result;
	reqinfo = reqev->ev_arg;
	tsigquery = reqinfo->msg;
	context = reqinfo->context;
	addr = reqinfo->addr;

	if (shuttingdown) {
		dns_request_destroy(&request);
		dns_message_destroy(&tsigquery);
		isc_mem_put(mctx, reqinfo, sizeof(nsu_gssinfo_t));
		isc_event_free(&event);
		maybeshutdown();
		return;
	}

	if (eresult != ISC_R_SUCCESS) {
		char addrbuf[ISC_SOCKADDR_FORMATSIZE];

		isc_sockaddr_format(addr, addrbuf, sizeof(addrbuf));
		fprintf(stderr, "; Communication with %s failed: %s\n",
			addrbuf, isc_result_totext(eresult));
		if (userserver != NULL)
			fatal("could not talk to specified name server");
		else if (++ns_inuse >= lwconf->nsnext)
			fatal("could not talk to any default name server");
		ddebug("Destroying request [%p]", request);
		dns_request_destroy(&request);
		dns_message_renderreset(tsigquery);
		sendrequest(localaddr, &servers[ns_inuse], tsigquery,
			    &request);
		isc_mem_put(mctx, reqinfo, sizeof(nsu_gssinfo_t));
		isc_event_free(&event);
		return;
	}
	isc_mem_put(mctx, reqinfo, sizeof(nsu_gssinfo_t));

	isc_event_free(&event);
	reqev = NULL;

	ddebug("recvgss creating rcvmsg");
	result = dns_message_create(mctx, DNS_MESSAGE_INTENTPARSE, &rcvmsg);
	check_result(result, "dns_message_create");

	result = dns_request_getresponse(request, rcvmsg,
					 DNS_MESSAGEPARSE_PRESERVEORDER);
	check_result(result, "dns_request_getresponse");

	if (debugging)
		show_message(stderr, rcvmsg,
			     "recvmsg reply from GSS-TSIG query");

	if (rcvmsg->rcode == dns_rcode_formerr && !tried_other_gsstsig) {
		ddebug("recvgss trying %s GSS-TSIG",
		       use_win2k_gsstsig ? "Standard" : "Win2k");
		if (use_win2k_gsstsig)
			use_win2k_gsstsig = ISC_FALSE;
		else
			use_win2k_gsstsig = ISC_TRUE;
		tried_other_gsstsig = ISC_TRUE;
		start_gssrequest(&restart_master);
		goto done;
	}

	if (rcvmsg->rcode != dns_rcode_noerror &&
	    rcvmsg->rcode != dns_rcode_nxdomain)
		fatal("response to GSS-TSIG query was unsuccessful");


	dns_fixedname_init(&fname);
	servname = dns_fixedname_name(&fname);
	isc_buffer_init(&buf, servicename, strlen(servicename));
	isc_buffer_add(&buf, strlen(servicename));
	result = dns_name_fromtext(servname, &buf, dns_rootname, 0, NULL);
	check_result(result, "dns_name_fromtext");

	tsigkey = NULL;
	result = dns_tkey_gssnegotiate(tsigquery, rcvmsg, servname,
				       &context, &tsigkey, gssring,
				       use_win2k_gsstsig,
				       &err_message);
	switch (result) {

	case DNS_R_CONTINUE:
		send_gssrequest(localaddr, kserver, tsigquery, &request,
				context);
		break;

	case ISC_R_SUCCESS:
		/*
		 * XXXSRA Waaay too much fun here.  There's no good
		 * reason why we need a TSIG here (the people who put
		 * it into the spec admitted at the time that it was
		 * not a security issue), and Windows clients don't
		 * seem to work if named complies with the spec and
		 * includes the gratuitous TSIG.  So we're in the
		 * bizarre situation of having to choose between
		 * complying with a useless requirement in the spec
		 * and interoperating.  This is nuts.  If we can
		 * confirm this behavior, we should ask the WG to
		 * consider removing the requirement for the
		 * gratuitous TSIG here.  For the moment, we ignore
		 * the TSIG -- this too is a spec violation, but it's
		 * the least insane thing to do.
		 */
#if 0
		/*
		 * Verify the signature.
		 */
		rcvmsg->state = DNS_SECTION_ANY;
		dns_message_setquerytsig(rcvmsg, NULL);
		result = dns_message_settsigkey(rcvmsg, tsigkey);
		check_result(result, "dns_message_settsigkey");
		result = dns_message_checksig(rcvmsg, NULL);
		ddebug("tsig verification: %s", dns_result_totext(result));
		check_result(result, "dns_message_checksig");
#endif /* 0 */

		send_update(&tmpzonename, serveraddr, localaddr);
		setzoneclass(dns_rdataclass_none);
		break;

	default:
		fatal("dns_tkey_negotiategss: %s %s",
		      isc_result_totext(result),
		      err_message != NULL ? err_message : "");
	}

 done:
	dns_request_destroy(&request);
	dns_message_destroy(&tsigquery);

	dns_message_destroy(&rcvmsg);
	ddebug("Out of recvgss");
}
#endif

static void
start_update(void) {
	isc_result_t result;
	dns_rdataset_t *rdataset = NULL;
	dns_name_t *name = NULL;
	dns_request_t *request = NULL;
	dns_message_t *soaquery = NULL;
	dns_name_t *firstname;
	dns_section_t section = DNS_SECTION_UPDATE;

	ddebug("start_update()");

	if (answer != NULL)
		dns_message_destroy(&answer);

	if (userzone != NULL && userserver != NULL && ! usegsstsig) {
		send_update(userzone, userserver, localaddr);
		setzoneclass(dns_rdataclass_none);
		return;
	}

	result = dns_message_create(mctx, DNS_MESSAGE_INTENTRENDER,
				    &soaquery);
	check_result(result, "dns_message_create");

	if (userserver == NULL)
		soaquery->flags |= DNS_MESSAGEFLAG_RD;

	result = dns_message_gettempname(soaquery, &name);
	check_result(result, "dns_message_gettempname");

	result = dns_message_gettemprdataset(soaquery, &rdataset);
	check_result(result, "dns_message_gettemprdataset");

	dns_rdataset_makequestion(rdataset, getzoneclass(), dns_rdatatype_soa);

	if (userzone != NULL) {
		dns_name_init(name, NULL);
		dns_name_clone(userzone, name);
	} else {
		dns_rdataset_t *tmprdataset;
		result = dns_message_firstname(updatemsg, section);
		if (result == ISC_R_NOMORE) {
			section = DNS_SECTION_PREREQUISITE;
			result = dns_message_firstname(updatemsg, section);
		}
		if (result != ISC_R_SUCCESS) {
			dns_message_puttempname(soaquery, &name);
			dns_rdataset_disassociate(rdataset);
			dns_message_puttemprdataset(soaquery, &rdataset);
			dns_message_destroy(&soaquery);
			done_update();
			return;
		}
		firstname = NULL;
		dns_message_currentname(updatemsg, section, &firstname);
		dns_name_init(name, NULL);
		dns_name_clone(firstname, name);
		/*
		 * Looks to see if the first name references a DS record
		 * and if that name is not the root remove a label as DS
		 * records live in the parent zone so we need to start our
		 * search one label up.
		 */
		tmprdataset = ISC_LIST_HEAD(firstname->list);
		if (section == DNS_SECTION_UPDATE &&
		    !dns_name_equal(firstname, dns_rootname) &&
		    tmprdataset->type == dns_rdatatype_ds) {
		    unsigned int labels = dns_name_countlabels(name);
		    dns_name_getlabelsequence(name, 1, labels - 1, name);
		}
	}

	ISC_LIST_INIT(name->list);
	ISC_LIST_APPEND(name->list, rdataset, link);
	dns_message_addname(soaquery, name, DNS_SECTION_QUESTION);

	if (userserver != NULL)
		sendrequest(localaddr, userserver, soaquery, &request);
	else {
		ns_inuse = 0;
		sendrequest(localaddr, &servers[ns_inuse], soaquery, &request);
	}
}

static void
cleanup(void) {
	ddebug("cleanup()");

	if (answer != NULL)
		dns_message_destroy(&answer);

#ifdef GSSAPI
	if (tsigkey != NULL) {
		ddebug("detach tsigkey x%p", tsigkey);
		dns_tsigkey_detach(&tsigkey);
	}
	if (gssring != NULL) {
		ddebug("Detaching GSS-TSIG keyring");
		dns_tsigkeyring_detach(&gssring);
	}
	if (kserver != NULL) {
		isc_mem_put(mctx, kserver, sizeof(isc_sockaddr_t));
		kserver = NULL;
	}
	if (realm != NULL) {
		isc_mem_free(mctx, realm);
		realm = NULL;
	}
#endif

	if (sig0key != NULL)
		dst_key_free(&sig0key);

	ddebug("Shutting down task manager");
	isc_taskmgr_destroy(&taskmgr);

	ddebug("Destroying event");
	isc_event_free(&global_event);

	ddebug("Shutting down socket manager");
	isc_socketmgr_destroy(&socketmgr);

	ddebug("Shutting down timer manager");
	isc_timermgr_destroy(&timermgr);

	ddebug("Destroying hash context");
	isc_hash_destroy();

	ddebug("Destroying name state");
	dns_name_destroy();

	ddebug("Removing log context");
	isc_log_destroy(&lctx);

	ddebug("Destroying memory context");
	if (memdebugging)
		isc_mem_stats(mctx, stderr);
	isc_mem_destroy(&mctx);
}

static void
getinput(isc_task_t *task, isc_event_t *event) {
	isc_boolean_t more;

	UNUSED(task);

	if (shuttingdown) {
		maybeshutdown();
		return;
	}

	if (global_event == NULL)
		global_event = event;

	reset_system();
	more = user_interaction();
	if (!more) {
		isc_app_shutdown();
		return;
	}
	start_update();
	return;
}

int
main(int argc, char **argv) {
	isc_result_t result;
	style = &dns_master_style_debug;

	input = stdin;

	interactive = ISC_TF(isatty(0));

	isc_app_start();

	pre_parse_args(argc, argv);

	result = isc_mem_create(0, 0, &mctx);
	check_result(result, "isc_mem_create");

	parse_args(argc, argv, mctx, &entropy);

	setup_system();

	result = isc_app_onrun(mctx, global_task, getinput, NULL);
	check_result(result, "isc_app_onrun");

	(void)isc_app_run();

	cleanup();

	isc_app_finish();

	if (seenerror)
		return (2);
	else
		return (0);
}
