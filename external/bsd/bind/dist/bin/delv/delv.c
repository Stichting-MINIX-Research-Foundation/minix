/*	$NetBSD: delv.c,v 1.4 2015/07/08 17:28:54 christos Exp $	*/

/*
 * Copyright (C) 2014, 2015  Internet Systems Consortium, Inc. ("ISC")
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

#include <config.h>
#include <bind.keys.h>

#ifndef WIN32
#include <sys/types.h>
#include <sys/socket.h>
#include <signal.h>

#include <netinet/in.h>

#include <arpa/inet.h>

#include <netdb.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <isc/app.h>
#include <isc/base64.h>
#include <isc/buffer.h>
#include <isc/lib.h>
#include <isc/log.h>
#include <isc/mem.h>
#ifdef WIN32
#include <isc/ntpaths.h>
#endif
#include <isc/parseint.h>
#include <isc/print.h>
#include <isc/sockaddr.h>
#include <isc/socket.h>
#include <isc/string.h>
#include <isc/task.h>
#include <isc/timer.h>
#include <isc/util.h>

#include <irs/resconf.h>
#include <irs/netdb.h>

#include <isccfg/log.h>
#include <isccfg/namedconf.h>

#include <dns/byaddr.h>
#include <dns/client.h>
#include <dns/fixedname.h>
#include <dns/keytable.h>
#include <dns/keyvalues.h>
#include <dns/lib.h>
#include <dns/log.h>
#include <dns/masterdump.h>
#include <dns/name.h>
#include <dns/rdata.h>
#include <dns/rdataclass.h>
#include <dns/rdataset.h>
#include <dns/rdatastruct.h>
#include <dns/rdatatype.h>
#include <dns/result.h>
#include <dns/secalg.h>
#include <dns/view.h>

#include <dst/dst.h>
#include <dst/result.h>

#define CHECK(r) \
	do { \
		result = (r); \
		if (result != ISC_R_SUCCESS) \
			goto cleanup; \
	} while (/*CONSTCOND*/0)

#define MAXNAME (DNS_NAME_MAXTEXT+1)

/* Variables used internally by delv. */
char *progname;
static isc_mem_t *mctx = NULL;
static isc_log_t *lctx = NULL;

/* Configurables */
static char *server = NULL;
static const char *port = "53";
static isc_sockaddr_t *srcaddr4 = NULL, *srcaddr6 = NULL;
static isc_sockaddr_t a4, a6;
static char *curqname = NULL, *qname = NULL;
static isc_boolean_t classset = ISC_FALSE;
static dns_rdatatype_t qtype = dns_rdatatype_none;
static isc_boolean_t typeset = ISC_FALSE;

static unsigned int styleflags = 0;
static isc_uint32_t splitwidth = 0xffffffff;
static isc_boolean_t
	showcomments = ISC_TRUE,
	showdnssec = ISC_TRUE,
	showtrust = ISC_TRUE,
	rrcomments = ISC_TRUE,
	noclass = ISC_FALSE,
	nocrypto = ISC_FALSE,
	nottl = ISC_FALSE,
	multiline = ISC_FALSE,
	short_form = ISC_FALSE;

static isc_boolean_t
	resolve_trace = ISC_FALSE,
	validator_trace = ISC_FALSE,
	message_trace = ISC_FALSE;

static isc_boolean_t
	use_ipv4 = ISC_TRUE,
	use_ipv6 = ISC_TRUE;

static isc_boolean_t
	cdflag = ISC_FALSE,
	no_sigs = ISC_FALSE,
	root_validation = ISC_TRUE,
	dlv_validation = ISC_TRUE;

static char *anchorfile = NULL;
static char *trust_anchor = NULL;
static char *dlv_anchor = NULL;
static int trusted_keys = 0;

static dns_fixedname_t afn, dfn;
static dns_name_t *anchor_name = NULL, *dlv_name = NULL;

/* Default bind.keys contents */
static char anchortext[] = MANAGED_KEYS;

/*
 * Static function prototypes
 */
static isc_result_t
get_reverse(char *reverse, size_t len, char *value, isc_boolean_t strict);

static isc_result_t
parse_uint(isc_uint32_t *uip, const char *value, isc_uint32_t max,
	   const char *desc);

static void
usage(void) {
	fputs(
"Usage:  delv [@server] {q-opt} {d-opt} [domain] [q-type] [q-class]\n"
"Where:  domain	  is in the Domain Name System\n"
"        q-class  is one of (in,hs,ch,...) [default: in]\n"
"        q-type   is one of (a,any,mx,ns,soa,hinfo,axfr,txt,...) [default:a]\n"
"        q-opt    is one of:\n"
"                 -x dot-notation     (shortcut for reverse lookups)\n"
"                 -d level            (set debugging level)\n"
"                 -a anchor-file      (specify root and dlv trust anchors)\n"
"                 -b address[#port]   (bind to source address/port)\n"
"                 -p port             (specify port number)\n"
"                 -q name             (specify query name)\n"
"                 -t type             (specify query type)\n"
"                 -c class            (specify query class)\n"
"                 -4                  (use IPv4 query transport only)\n"
"                 -6                  (use IPv6 query transport only)\n"
"                 -i                  (disable DNSSEC validation)\n"
"                 -m                  (enable memory usage debugging)\n"
"        d-opt    is of the form +keyword[=value], where keyword is:\n"
"                 +[no]all            (Set or clear all display flags)\n"
"                 +[no]class          (Control display of class)\n"
"                 +[no]crypto         (Control display of cryptographic\n"
"                                      fields in records)\n"
"                 +[no]multiline      (Print records in an expanded format)\n"
"                 +[no]comments       (Control display of comment lines)\n"
"                 +[no]rrcomments     (Control display of per-record "
				       "comments)\n"
"                 +[no]short          (Short form answer)\n"
"                 +[no]split=##       (Split hex/base64 fields into chunks)\n"
"                 +[no]ttl            (Control display of ttls in records)\n"
"                 +[no]trust          (Control display of trust level)\n"
"                 +[no]rtrace         (Trace resolver fetches)\n"
"                 +[no]mtrace         (Trace messages received)\n"
"                 +[no]vtrace         (Trace validation process)\n"
"                 +[no]dlv            (DNSSEC lookaside validation anchor)\n"
"                 +[no]root           (DNSSEC validation trust anchor)\n"
"                 +[no]dnssec         (Display DNSSEC records)\n"
"        -h                           (print help and exit)\n"
"        -v                           (print version and exit)\n",
	stderr);
	exit(1);
}

ISC_PLATFORM_NORETURN_PRE static void
fatal(const char *format, ...)
ISC_FORMAT_PRINTF(1, 2) ISC_PLATFORM_NORETURN_POST;

static void
fatal(const char *format, ...) {
	va_list args;

	fflush(stdout);
	fprintf(stderr, "%s: ", progname);
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
	fprintf(stderr, "\n");
	exit(1);
}

static void
warn(const char *format, ...) ISC_FORMAT_PRINTF(1, 2);

static void
warn(const char *format, ...) {
	va_list args;

	fflush(stdout);
	fprintf(stderr, "%s: warning: ", progname);
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
	fprintf(stderr, "\n");
}

static isc_logcategory_t categories[] = {
	{ "delv",	     0 },
	{ NULL,		     0 }
};
#define LOGCATEGORY_DEFAULT		(&categories[0])
#define LOGMODULE_DEFAULT		(&modules[0])

static isc_logmodule_t modules[] = {
	{ "delv",	 		0 },
	{ NULL, 			0 }
};

static void
delv_log(int level, const char *fmt, ...) ISC_FORMAT_PRINTF(2, 3);

static void
delv_log(int level, const char *fmt, ...) {
	va_list ap;
	char msgbuf[2048];

	if (! isc_log_wouldlog(lctx, level))
		return;

	va_start(ap, fmt);

	vsnprintf(msgbuf, sizeof(msgbuf), fmt, ap);
	isc_log_write(lctx, LOGCATEGORY_DEFAULT, LOGMODULE_DEFAULT,
		      level, "%s", msgbuf);
	va_end(ap);
}

static int loglevel = 0;

static void
setup_logging(FILE *errout) {
	isc_result_t result;
	isc_logdestination_t destination;
	isc_logconfig_t *logconfig = NULL;

	result = isc_log_create(mctx, &lctx, &logconfig);
	if (result != ISC_R_SUCCESS)
		fatal("Couldn't set up logging");

	isc_log_registercategories(lctx, categories);
	isc_log_registermodules(lctx, modules);
	isc_log_setcontext(lctx);
	dns_log_init(lctx);
	dns_log_setcontext(lctx);
	cfg_log_init(lctx);

	destination.file.stream = errout;
	destination.file.name = NULL;
	destination.file.versions = ISC_LOG_ROLLNEVER;
	destination.file.maximum_size = 0;

	result = isc_log_createchannel(logconfig, "stderr",
				       ISC_LOG_TOFILEDESC, ISC_LOG_DYNAMIC,
				       &destination, ISC_LOG_PRINTPREFIX);
	if (result != ISC_R_SUCCESS)
		fatal("Couldn't set up log channel 'stderr'");

	isc_log_setdebuglevel(lctx, loglevel);

	result = isc_log_settag(logconfig, ";; ");
	if (result != ISC_R_SUCCESS)
		fatal("Couldn't set log tag");

	result = isc_log_usechannel(logconfig, "stderr",
				    ISC_LOGCATEGORY_DEFAULT, NULL);
	if (result != ISC_R_SUCCESS)
		fatal("Couldn't attach to log channel 'stderr'");

	if (resolve_trace && loglevel < 1) {
		result = isc_log_createchannel(logconfig, "resolver",
					       ISC_LOG_TOFILEDESC,
					       ISC_LOG_DEBUG(1),
					       &destination,
					       ISC_LOG_PRINTPREFIX);
		if (result != ISC_R_SUCCESS)
			fatal("Couldn't set up log channel 'resolver'");

		result = isc_log_usechannel(logconfig, "resolver",
					    DNS_LOGCATEGORY_RESOLVER,
					    DNS_LOGMODULE_RESOLVER);
		if (result != ISC_R_SUCCESS)
			fatal("Couldn't attach to log channel 'resolver'");
	}

	if (validator_trace && loglevel < 3) {
		result = isc_log_createchannel(logconfig, "validator",
					       ISC_LOG_TOFILEDESC,
					       ISC_LOG_DEBUG(3),
					       &destination,
					       ISC_LOG_PRINTPREFIX);
		if (result != ISC_R_SUCCESS)
			fatal("Couldn't set up log channel 'validator'");

		result = isc_log_usechannel(logconfig, "validator",
					    DNS_LOGCATEGORY_DNSSEC,
					    DNS_LOGMODULE_VALIDATOR);
		if (result != ISC_R_SUCCESS)
			fatal("Couldn't attach to log channel 'validator'");
	}

	if (message_trace && loglevel < 10) {
		result = isc_log_createchannel(logconfig, "messages",
					       ISC_LOG_TOFILEDESC,
					       ISC_LOG_DEBUG(10),
					       &destination,
					       ISC_LOG_PRINTPREFIX);
		if (result != ISC_R_SUCCESS)
			fatal("Couldn't set up log channel 'messages'");

		result = isc_log_usechannel(logconfig, "messages",
					    DNS_LOGCATEGORY_RESOLVER,
					    DNS_LOGMODULE_PACKETS);
		if (result != ISC_R_SUCCESS)
			fatal("Couldn't attach to log channel 'messagse'");
	}
}

static void
print_status(dns_rdataset_t *rdataset) {
	const char *astr = "", *tstr = "";

	REQUIRE(rdataset != NULL);

	if (!showtrust || !dns_rdataset_isassociated(rdataset))
		return;

	if ((rdataset->attributes & DNS_RDATASETATTR_NEGATIVE) != 0)
		astr = "negative response, ";

	switch (rdataset->trust) {
	case dns_trust_none:
		tstr = "untrusted";
		break;
	case dns_trust_pending_additional:
		tstr = "signed additional data, pending validation";
		break;
	case dns_trust_pending_answer:
		tstr = "signed answer, pending validation";
		break;
	case dns_trust_additional:
		tstr = "unsigned additional data";
		break;
	case dns_trust_glue:
		tstr = "glue data";
		break;
	case dns_trust_answer:
		if (root_validation || dlv_validation)
			tstr = "unsigned answer";
		else
			tstr = "answer not validated";
		break;
	case dns_trust_authauthority:
		tstr = "authority data";
		break;
	case dns_trust_authanswer:
		tstr = "authoritative";
		break;
	case dns_trust_secure:
		tstr = "fully validated";
		break;
	case dns_trust_ultimate:
		tstr = "ultimate trust";
		break;
	}

	printf("; %s%s\n", astr, tstr);
}

static isc_result_t
printdata(dns_rdataset_t *rdataset, dns_name_t *owner,
	  dns_master_style_t *style)
{
	isc_result_t result = ISC_R_SUCCESS;
	static dns_trust_t trust;
	static isc_boolean_t first = ISC_TRUE;
	isc_buffer_t target;
	isc_region_t r;
	char *t = NULL;
	int len = 2048;

	if (!dns_rdataset_isassociated(rdataset)) {
		char namebuf[DNS_NAME_FORMATSIZE];
		dns_name_format(owner, namebuf, sizeof(namebuf));
		delv_log(ISC_LOG_DEBUG(4),
			  "WARN: empty rdataset %s", namebuf);
		return (ISC_R_SUCCESS);
	}

	if (!showdnssec && rdataset->type == dns_rdatatype_rrsig)
		return (ISC_R_SUCCESS);

	if (first || rdataset->trust != trust) {
		if (!first && showtrust && !short_form)
			putchar('\n');
		print_status(rdataset);
		trust = rdataset->trust;
		first = ISC_FALSE;
	}

	do {
		t = isc_mem_get(mctx, len);
		if (t == NULL)
			return (ISC_R_NOMEMORY);

		isc_buffer_init(&target, t, len);
		if (short_form) {
			dns_rdata_t rdata = DNS_RDATA_INIT;
			for (result = dns_rdataset_first(rdataset);
			     result == ISC_R_SUCCESS;
			     result = dns_rdataset_next(rdataset))
			{
				if ((rdataset->attributes &
				     DNS_RDATASETATTR_NEGATIVE) != 0)
					continue;

				dns_rdataset_current(rdataset, &rdata);
				result = dns_rdata_tofmttext(&rdata,
							     dns_rootname,
							     styleflags,
							     0, 60, " ",
							     &target);
				if (result != ISC_R_SUCCESS)
					break;

				if (isc_buffer_availablelength(&target) < 1) {
					result = ISC_R_NOSPACE;
					break;
				}

				isc_buffer_putstr(&target, "\n");

				dns_rdata_reset(&rdata);
			}
		} else {
			if ((rdataset->attributes &
			     DNS_RDATASETATTR_NEGATIVE) != 0)
				isc_buffer_putstr(&target, "; ");

			result = dns_master_rdatasettotext(owner, rdataset,
							   style, &target);
		}

		if (result == ISC_R_NOSPACE) {
			isc_mem_put(mctx, t, len);
			len += 1024;
		} else if (result == ISC_R_NOMORE)
			result = ISC_R_SUCCESS;
		else
			CHECK(result);
	} while (result == ISC_R_NOSPACE);

	isc_buffer_usedregion(&target, &r);
	printf("%.*s", (int)r.length, (char *)r.base);

 cleanup:
	if (t != NULL)
		isc_mem_put(mctx, t, len);

	return (ISC_R_SUCCESS);
}

static isc_result_t
setup_style(dns_master_style_t **stylep) {
	isc_result_t result;
	dns_master_style_t *style = NULL;

	REQUIRE(stylep != NULL || *stylep == NULL);

	styleflags |= DNS_STYLEFLAG_REL_OWNER;
	if (showcomments)
		styleflags |= DNS_STYLEFLAG_COMMENT;
	if (rrcomments)
		styleflags |= DNS_STYLEFLAG_RRCOMMENT;
	if (nottl)
		styleflags |= DNS_STYLEFLAG_NO_TTL;
	if (noclass)
		styleflags |= DNS_STYLEFLAG_NO_CLASS;
	if (nocrypto)
		styleflags |= DNS_STYLEFLAG_NOCRYPTO;
	if (multiline) {
		styleflags |= DNS_STYLEFLAG_MULTILINE;
		styleflags |= DNS_STYLEFLAG_COMMENT;
	}

	if (multiline || (nottl && noclass))
		result = dns_master_stylecreate2(&style, styleflags,
						 24, 24, 24, 32, 80, 8,
						 splitwidth, mctx);
	else if (nottl || noclass)
		result = dns_master_stylecreate2(&style, styleflags,
						 24, 24, 32, 40, 80, 8,
						 splitwidth, mctx);
	else
		result = dns_master_stylecreate2(&style, styleflags,
						 24, 32, 40, 48, 80, 8,
						 splitwidth, mctx);

	if (result == ISC_R_SUCCESS)
		*stylep = style;
	return (result);
}

static isc_result_t
convert_name(dns_fixedname_t *fn, dns_name_t **name, const char *text) {
	isc_result_t result;
	isc_buffer_t b;
	dns_name_t *n;
	unsigned int len;

	REQUIRE(fn != NULL && name != NULL && text != NULL);
	len = strlen(text);

	isc_buffer_constinit(&b, text, len);
	isc_buffer_add(&b, len);
	dns_fixedname_init(fn);
	n = dns_fixedname_name(fn);

	result = dns_name_fromtext(n, &b, dns_rootname, 0, NULL);
	if (result != ISC_R_SUCCESS) {
		delv_log(ISC_LOG_ERROR, "failed to convert QNAME %s: %s",
			  text, isc_result_totext(result));
		return (result);
	}

	*name = n;
	return (ISC_R_SUCCESS);
}

static isc_result_t
key_fromconfig(const cfg_obj_t *key, dns_client_t *client) {
	dns_rdata_dnskey_t keystruct;
	isc_uint32_t flags, proto, alg;
	const char *keystr, *keynamestr;
	unsigned char keydata[4096];
	isc_buffer_t keydatabuf;
	unsigned char rrdata[4096];
	isc_buffer_t rrdatabuf;
	isc_region_t r;
	dns_fixedname_t fkeyname;
	dns_name_t *keyname;
	isc_result_t result;
	isc_boolean_t match_root, match_dlv;

	keynamestr = cfg_obj_asstring(cfg_tuple_get(key, "name"));
	CHECK(convert_name(&fkeyname, &keyname, keynamestr));

	if (!root_validation && !dlv_validation)
		return (ISC_R_SUCCESS);

	match_root = dns_name_equal(keyname, anchor_name);
	match_dlv = dns_name_equal(keyname, dlv_name);

	if (!match_root && !match_dlv)
		return (ISC_R_SUCCESS);
	if ((!root_validation && match_root) || (!dlv_validation && match_dlv))
		return (ISC_R_SUCCESS);

	if (match_root)
		delv_log(ISC_LOG_DEBUG(3), "adding trust anchor %s",
			  trust_anchor);
	if (match_dlv)
		delv_log(ISC_LOG_DEBUG(3), "adding DLV trust anchor %s",
			  dlv_anchor);

	flags = cfg_obj_asuint32(cfg_tuple_get(key, "flags"));
	proto = cfg_obj_asuint32(cfg_tuple_get(key, "protocol"));
	alg = cfg_obj_asuint32(cfg_tuple_get(key, "algorithm"));

	keystruct.common.rdclass = dns_rdataclass_in;
	keystruct.common.rdtype = dns_rdatatype_dnskey;
	/*
	 * The key data in keystruct is not dynamically allocated.
	 */
	keystruct.mctx = NULL;

	ISC_LINK_INIT(&keystruct.common, link);

	if (flags > 0xffff)
		CHECK(ISC_R_RANGE);
	if (proto > 0xff)
		CHECK(ISC_R_RANGE);
	if (alg > 0xff)
		CHECK(ISC_R_RANGE);

	keystruct.flags = (isc_uint16_t)flags;
	keystruct.protocol = (isc_uint8_t)proto;
	keystruct.algorithm = (isc_uint8_t)alg;

	isc_buffer_init(&keydatabuf, keydata, sizeof(keydata));
	isc_buffer_init(&rrdatabuf, rrdata, sizeof(rrdata));

	keystr = cfg_obj_asstring(cfg_tuple_get(key, "key"));
	CHECK(isc_base64_decodestring(keystr, &keydatabuf));
	isc_buffer_usedregion(&keydatabuf, &r);
	keystruct.datalen = r.length;
	keystruct.data = r.base;

	CHECK(dns_rdata_fromstruct(NULL,
				   keystruct.common.rdclass,
				   keystruct.common.rdtype,
				   &keystruct, &rrdatabuf));

	CHECK(dns_client_addtrustedkey(client, dns_rdataclass_in,
				       keyname, &rrdatabuf));
	trusted_keys++;

 cleanup:
	if (result == DST_R_NOCRYPTO)
		cfg_obj_log(key, lctx, ISC_LOG_ERROR, "no crypto support");
	else if (result == DST_R_UNSUPPORTEDALG) {
		cfg_obj_log(key, lctx, ISC_LOG_WARNING,
			    "skipping trusted key '%s': %s",
			    keynamestr, isc_result_totext(result));
		result = ISC_R_SUCCESS;
	} else if (result != ISC_R_SUCCESS) {
		cfg_obj_log(key, lctx, ISC_LOG_ERROR,
			    "failed to add trusted key '%s': %s",
			    keynamestr, isc_result_totext(result));
		result = ISC_R_FAILURE;
	}

	return (result);
}

static isc_result_t
load_keys(const cfg_obj_t *keys, dns_client_t *client) {
	const cfg_listelt_t *elt, *elt2;
	const cfg_obj_t *key, *keylist;
	isc_result_t result = ISC_R_SUCCESS;

	for (elt = cfg_list_first(keys);
	     elt != NULL;
	     elt = cfg_list_next(elt))
	{
		keylist = cfg_listelt_value(elt);

		for (elt2 = cfg_list_first(keylist);
		     elt2 != NULL;
		     elt2 = cfg_list_next(elt2))
		{
			key = cfg_listelt_value(elt2);
			CHECK(key_fromconfig(key, client));
		}
	}

 cleanup:
	if (result == DST_R_NOCRYPTO)
		result = ISC_R_SUCCESS;
	return (result);
}

static isc_result_t
setup_dnsseckeys(dns_client_t *client) {
	isc_result_t result;
	cfg_parser_t *parser = NULL;
	const cfg_obj_t *keys = NULL;
	const cfg_obj_t *managed_keys = NULL;
	cfg_obj_t *bindkeys = NULL;
	const char *filename = anchorfile;

	if (!root_validation && !dlv_validation)
		return (ISC_R_SUCCESS);

	if (filename == NULL) {
#ifndef WIN32
		filename = NS_SYSCONFDIR "/bind.keys";
#else
		static char buf[MAX_PATH];
		strlcpy(buf, isc_ntpaths_get(SYS_CONF_DIR), sizeof(buf));
		strlcat(buf, "\\bind.keys", sizeof(buf));
		filename = buf;
#endif
	}

	if (trust_anchor == NULL) {
		trust_anchor = isc_mem_strdup(mctx, ".");
		if (trust_anchor == NULL)
			fatal("out of memory");
	}

	if (dlv_anchor == NULL) {
		dlv_anchor = isc_mem_strdup(mctx, "dlv.isc.org");
		if (dlv_anchor == NULL)
			fatal("out of memory");
	}

	CHECK(convert_name(&afn, &anchor_name, trust_anchor));
	CHECK(convert_name(&dfn, &dlv_name, dlv_anchor));

	CHECK(cfg_parser_create(mctx, dns_lctx, &parser));

	if (access(filename, R_OK) != 0) {
		if (anchorfile != NULL)
			fatal("Unable to read key file '%s'", anchorfile);
	} else {
		result = cfg_parse_file(parser, filename,
					&cfg_type_bindkeys, &bindkeys);
		if (result != ISC_R_SUCCESS)
			if (anchorfile != NULL)
				fatal("Unable to load keys from '%s'",
				      anchorfile);
	}

	if (bindkeys == NULL) {
		isc_buffer_t b;

		isc_buffer_init(&b, anchortext, sizeof(anchortext) - 1);
		isc_buffer_add(&b, sizeof(anchortext) - 1);
		result = cfg_parse_buffer(parser, &b, &cfg_type_bindkeys,
					  &bindkeys);
		if (result != ISC_R_SUCCESS)
			fatal("Unable to parse built-in keys");
	}

	INSIST(bindkeys != NULL);
	cfg_map_get(bindkeys, "trusted-keys", &keys);
	cfg_map_get(bindkeys, "managed-keys", &managed_keys);

	if (keys != NULL)
		CHECK(load_keys(keys, client));
	if (managed_keys != NULL)
		CHECK(load_keys(managed_keys, client));
	result = ISC_R_SUCCESS;

	if (trusted_keys == 0)
		fatal("No trusted keys were loaded");

	if (dlv_validation)
		dns_client_setdlv(client, dns_rdataclass_in, dlv_anchor);

 cleanup:
	if (result != ISC_R_SUCCESS)
		delv_log(ISC_LOG_ERROR, "setup_dnsseckeys: %s",
			  isc_result_totext(result));
	return (result);
}

static isc_result_t
addserver(dns_client_t *client) {
	struct addrinfo hints, *res, *cur;
	int gai_error;
	struct in_addr in4;
	struct in6_addr in6;
	isc_sockaddr_t *sa;
	isc_sockaddrlist_t servers;
	isc_uint32_t destport;
	isc_result_t result;
	dns_name_t *name = NULL;

	result = parse_uint(&destport, port, 0xffff, "port");
	if (result != ISC_R_SUCCESS)
		fatal("Couldn't parse port number");

	ISC_LIST_INIT(servers);

	if (use_ipv4 && inet_pton(AF_INET, server, &in4) == 1) {
		sa = isc_mem_get(mctx, sizeof(*sa));
		if (sa == NULL)
			return (ISC_R_NOMEMORY);
		ISC_LINK_INIT(sa, link);
		isc_sockaddr_fromin(sa, &in4, destport);
		ISC_LIST_APPEND(servers, sa, link);
	} else if (use_ipv6 && inet_pton(AF_INET6, server, &in6) == 1) {
		sa = isc_mem_get(mctx, sizeof(*sa));
		if (sa == NULL)
			return (ISC_R_NOMEMORY);
		ISC_LINK_INIT(sa, link);
		isc_sockaddr_fromin6(sa, &in6, destport);
		ISC_LIST_APPEND(servers, sa, link);
	} else {
		memset(&hints, 0, sizeof(hints));
		if (!use_ipv6)
			hints.ai_family = AF_INET;
		else if (!use_ipv4)
			hints.ai_family = AF_INET6;
		else
			hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_DGRAM;
		hints.ai_protocol = IPPROTO_UDP;
		gai_error = getaddrinfo(server, port, &hints, &res);
		if (gai_error != 0) {
			delv_log(ISC_LOG_ERROR,
				  "getaddrinfo failed: %s",
				  gai_strerror(gai_error));
			return (ISC_R_FAILURE);
		}

		result = ISC_R_SUCCESS;
		for (cur = res; cur != NULL; cur = cur->ai_next) {
			if (cur->ai_family != AF_INET &&
			    cur->ai_family != AF_INET6)
				continue;
			sa = isc_mem_get(mctx, sizeof(*sa));
			if (sa == NULL) {
				result = ISC_R_NOMEMORY;
				break;
			}
			memset(sa, 0, sizeof(*sa));
			ISC_LINK_INIT(sa, link);
			memmove(&sa->type, cur->ai_addr, cur->ai_addrlen);
			sa->length = (unsigned int)cur->ai_addrlen;
			ISC_LIST_APPEND(servers, sa, link);
		}
		freeaddrinfo(res);
		CHECK(result);
	}


	CHECK(dns_client_setservers(client, dns_rdataclass_in, name, &servers));

 cleanup:
	while (!ISC_LIST_EMPTY(servers)) {
		sa = ISC_LIST_HEAD(servers);
		ISC_LIST_UNLINK(servers, sa, link);
		isc_mem_put(mctx, sa, sizeof(*sa));
	}

	if (result != ISC_R_SUCCESS)
		delv_log(ISC_LOG_ERROR, "addserver: %s",
			  isc_result_totext(result));

	return (result);
}

static isc_result_t
findserver(dns_client_t *client) {
	isc_result_t result;
	irs_resconf_t *resconf = NULL;
	isc_sockaddrlist_t *nameservers;
	isc_sockaddr_t *sa, *next;
	isc_uint32_t destport;

	result = parse_uint(&destport, port, 0xffff, "port");
	if (result != ISC_R_SUCCESS)
		fatal("Couldn't parse port number");

	result = irs_resconf_load(mctx, "/etc/resolv.conf", &resconf);
	if (result != ISC_R_SUCCESS && result != ISC_R_FILENOTFOUND) {
		delv_log(ISC_LOG_ERROR, "irs_resconf_load: %s",
			  isc_result_totext(result));
		goto cleanup;
	}

	/* Get nameservers from resolv.conf */
	nameservers = irs_resconf_getnameservers(resconf);
	for (sa = ISC_LIST_HEAD(*nameservers); sa != NULL; sa = next) {
		next = ISC_LIST_NEXT(sa, link);

		/* Set destination port */
		if (sa->type.sa.sa_family == AF_INET && use_ipv4) {
			sa->type.sin.sin_port = htons(destport);
			continue;
		}
		if (sa->type.sa.sa_family == AF_INET6 && use_ipv6) {
			sa->type.sin6.sin6_port = htons(destport);
			continue;
		}

		/* Incompatible protocol family */
		ISC_LIST_UNLINK(*nameservers, sa, link);
		isc_mem_put(mctx, sa, sizeof(*sa));
	}

	/* None found, use localhost */
	if (ISC_LIST_EMPTY(*nameservers)) {
		if (use_ipv4) {
			struct in_addr localhost;
			localhost.s_addr = htonl(INADDR_LOOPBACK);
			sa = isc_mem_get(mctx, sizeof(*sa));
			if (sa == NULL) {
				result = ISC_R_NOMEMORY;
				goto cleanup;
			}
			isc_sockaddr_fromin(sa, &localhost, destport);

			ISC_LINK_INIT(sa, link);
			ISC_LIST_APPEND(*nameservers, sa, link);
		}

		if (use_ipv6) {
			sa = isc_mem_get(mctx, sizeof(*sa));
			if (sa == NULL) {
				result = ISC_R_NOMEMORY;
				goto cleanup;
			}
			isc_sockaddr_fromin6(sa, &in6addr_loopback, destport);

			ISC_LINK_INIT(sa, link);
			ISC_LIST_APPEND(*nameservers, sa, link);
		}
	}

	result = dns_client_setservers(client, dns_rdataclass_in, NULL,
				       nameservers);
	if (result != ISC_R_SUCCESS)
		delv_log(ISC_LOG_ERROR, "dns_client_setservers: %s",
			  isc_result_totext(result));

cleanup:
	if (resconf != NULL)
		irs_resconf_destroy(&resconf);
	return (result);
}

static char *
next_token(char **stringp, const char *delim) {
	char *res;

	do {
		res = strsep(stringp, delim);
		if (res == NULL)
			break;
	} while (*res == '\0');
	return (res);
}

static isc_result_t
parse_uint(isc_uint32_t *uip, const char *value, isc_uint32_t max,
	   const char *desc) {
	isc_uint32_t n;
	isc_result_t result = isc_parse_uint32(&n, value, 10);
	if (result == ISC_R_SUCCESS && n > max)
		result = ISC_R_RANGE;
	if (result != ISC_R_SUCCESS) {
		printf("invalid %s '%s': %s\n", desc,
		       value, isc_result_totext(result));
		return (result);
	}
	*uip = n;
	return (ISC_R_SUCCESS);
}

static void
plus_option(char *option) {
	isc_result_t result;
	char option_store[256];
	char *cmd, *value, *ptr;
	isc_boolean_t state = ISC_TRUE;

	strncpy(option_store, option, sizeof(option_store));
	option_store[sizeof(option_store)-1]=0;
	ptr = option_store;
	cmd = next_token(&ptr,"=");
	if (cmd == NULL) {
		printf(";; Invalid option %s\n", option_store);
		return;
	}
	value = ptr;
	if (strncasecmp(cmd, "no", 2)==0) {
		cmd += 2;
		state = ISC_FALSE;
	}

#define FULLCHECK(A) \
	do { \
		size_t _l = strlen(cmd); \
		if (_l >= sizeof(A) || strncasecmp(cmd, A, _l) != 0) \
			goto invalid_option; \
	} while (/*CONSTCOND*/0)

	switch (cmd[0]) {
	case 'a': /* all */
		FULLCHECK("all");
		showcomments = state;
		rrcomments = state;
		showtrust = state;
		break;
	case 'c':
		switch (cmd[1]) {
		case 'd': /* cdflag */
			FULLCHECK("cdflag");
			cdflag = state;
			break;
		case 'l': /* class */
			FULLCHECK("class");
			noclass = ISC_TF(!state);
			break;
		case 'o': /* comments */
			FULLCHECK("comments");
			showcomments = state;
			break;
		case 'r': /* crypto */
			FULLCHECK("crypto");
			nocrypto = ISC_TF(!state);
			break;
		default:
			goto invalid_option;
		}
		break;
	case 'd':
		switch (cmd[1]) {
		case 'l': /* dlv */
			FULLCHECK("dlv");
			if (state && no_sigs)
				break;
			dlv_validation = state;
			if (value != NULL) {
				dlv_anchor = isc_mem_strdup(mctx, value);
				if (dlv_anchor == NULL)
					fatal("out of memory");
			}
			break;
		case 'n': /* dnssec */
			FULLCHECK("dnssec");
			showdnssec = state;
			break;
		default:
			goto invalid_option;
		}
		break;
	case 'm':
		switch (cmd[1]) {
		case 't': /* mtrace */
			message_trace = state;
			if (state)
				resolve_trace = state;
			break;
		case 'u': /* multiline */
			FULLCHECK("multiline");
			multiline = state;
			break;
		default:
			goto invalid_option;
		}
		break;
	case 'r':
		switch (cmd[1]) {
		case 'o': /* root */
			FULLCHECK("root");
			if (state && no_sigs)
				break;
			root_validation = state;
			if (value != NULL) {
				trust_anchor = isc_mem_strdup(mctx, value);
				if (trust_anchor == NULL)
					fatal("out of memory");
			}
			break;
		case 'r': /* rrcomments */
			FULLCHECK("rrcomments");
			rrcomments = state;
			break;
		case 't': /* rtrace */
			FULLCHECK("rtrace");
			resolve_trace = state;
			break;
		default:
			goto invalid_option;
		}
		break;
	case 's':
		switch (cmd[1]) {
		case 'h': /* short */
			FULLCHECK("short");
			short_form = state;
			if (short_form) {
				multiline = ISC_FALSE;
				showcomments = ISC_FALSE;
				showtrust = ISC_FALSE;
				showdnssec = ISC_FALSE;
			}
			break;
		case 'p': /* split */
			FULLCHECK("split");
			if (value != NULL && !state)
				goto invalid_option;
			if (!state) {
				splitwidth = 0;
				break;
			} else if (value == NULL)
				break;

			result = parse_uint(&splitwidth, value,
					    1023, "split");
			if (splitwidth % 4 != 0) {
				splitwidth = ((splitwidth + 3) / 4) * 4;
				warn("split must be a multiple of 4; "
				     "adjusting to %d", splitwidth);
			}
			/*
			 * There is an adjustment done in the
			 * totext_<rrtype>() functions which causes
			 * splitwidth to shrink.  This is okay when we're
			 * using the default width but incorrect in this
			 * case, so we correct for it
			 */
			if (splitwidth)
				splitwidth += 3;
			if (result != ISC_R_SUCCESS)
				fatal("Couldn't parse split");
			break;
		default:
			goto invalid_option;
		}
		break;
	case 't':
		switch (cmd[1]) {
		case 'r': /* trust */
			FULLCHECK("trust");
			showtrust = state;
			break;
		case 't': /* ttl */
			FULLCHECK("ttl");
			nottl = ISC_TF(!state);
			break;
		default:
			goto invalid_option;
		}
		break;
	case 'v': /* vtrace */
		FULLCHECK("vtrace");
		validator_trace = state;
		if (state)
			resolve_trace = state;
		break;
	default:
	invalid_option:
		/*
		 * We can also add a "need_value:" case here if we ever
		 * add a plus-option that requires a specified value
		 */
		fprintf(stderr, "Invalid option: +%s\n", option);
		usage();
	}
	return;
}

/*
 * options: "46a:b:c:d:himp:q:t:vx:";
 */
static const char *single_dash_opts = "46himv";
static isc_boolean_t
dash_option(char *option, char *next, isc_boolean_t *open_type_class) {
	char opt, *value;
	isc_result_t result;
	isc_boolean_t value_from_next;
	isc_textregion_t tr;
	dns_rdatatype_t rdtype;
	dns_rdataclass_t rdclass;
	char textname[MAXNAME];
	struct in_addr in4;
	struct in6_addr in6;
	in_port_t srcport;
	isc_uint32_t num;
	char *hash;

	while (strpbrk(option, single_dash_opts) == &option[0]) {
		/*
		 * Since the -[46himv] options do not take an argument,
		 * account for them (in any number and/or combination)
		 * if they appear as the first character(s) of a q-opt.
		 */
		opt = option[0];
		switch (opt) {
		case '4':
			if (isc_net_probeipv4() != ISC_R_SUCCESS)
				fatal("IPv4 networking not available");
			if (use_ipv6) {
				isc_net_disableipv6();
				use_ipv6 = ISC_FALSE;
			}
			break;
		case '6':
			if (isc_net_probeipv6() != ISC_R_SUCCESS)
				fatal("IPv6 networking not available");
			if (use_ipv4) {
				isc_net_disableipv4();
				use_ipv4 = ISC_FALSE;
			}
			break;
		case 'h':
			usage();
			exit(0);
			/* NOTREACHED */
		case 'i':
			no_sigs = ISC_TRUE;
			dlv_validation = ISC_FALSE;
			root_validation = ISC_FALSE;
			break;
		case 'm':
			/* handled in preparse_args() */
			break;
		case 'v':
			fputs("delv " VERSION "\n", stderr);
			exit(0);
			/* NOTREACHED */
		default:
			INSIST(0);
		}
		if (strlen(option) > 1U)
			option = &option[1];
		else
			return (ISC_FALSE);
	}
	opt = option[0];
	if (strlen(option) > 1U) {
		value_from_next = ISC_FALSE;
		value = &option[1];
	} else {
		value_from_next = ISC_TRUE;
		value = next;
	}
	if (value == NULL)
		goto invalid_option;
	switch (opt) {
	case 'a':
		anchorfile = isc_mem_strdup(mctx, value);
		if (anchorfile == NULL)
			fatal("out of memory");
		return (value_from_next);
	case 'b':
		hash = strchr(value, '#');
		if (hash != NULL) {
			result = parse_uint(&num, hash + 1, 0xffff, "port");
			if (result != ISC_R_SUCCESS)
				fatal("Couldn't parse port number");
			srcport = num;
			*hash = '\0';
		} else
			srcport = 0;

		if (inet_pton(AF_INET, value, &in4) == 1) {
			if (srcaddr4 != NULL)
				fatal("Only one local address per family "
				      "can be specified\n");
			isc_sockaddr_fromin(&a4, &in4, srcport);
			srcaddr4 = &a4;
		} else if (inet_pton(AF_INET6, value, &in6) == 1) {
			if (srcaddr6 != NULL)
				fatal("Only one local address per family "
				      "can be specified\n");
			isc_sockaddr_fromin6(&a6, &in6, srcport);
			srcaddr6 = &a6;
		} else {
			if (hash != NULL)
				*hash = '#';
			fatal("Invalid address %s", value);
		}
		if (hash != NULL)
			*hash = '#';
		return (value_from_next);
	case 'c':
		if (classset)
			warn("extra query class");

		*open_type_class = ISC_FALSE;
		tr.base = value;
		tr.length = strlen(value);
		result = dns_rdataclass_fromtext(&rdclass,
						 (isc_textregion_t *)&tr);
		if (result == ISC_R_SUCCESS)
			classset = ISC_TRUE;
		else if (rdclass != dns_rdataclass_in)
			warn("ignoring non-IN query class");
		else
			warn("ignoring invalid class");
		return (value_from_next);
	case 'd':
		result = parse_uint(&num, value, 99, "debug level");
		if (result != ISC_R_SUCCESS)
			fatal("Couldn't parse debug level");
		loglevel = num;
		return (value_from_next);
	case 'p':
		port = value;
		return (value_from_next);
	case 'q':
		if (curqname != NULL) {
			warn("extra query name");
			isc_mem_free(mctx, curqname);
		}
		curqname = isc_mem_strdup(mctx, value);
		if (curqname == NULL)
			fatal("out of memory");
		return (value_from_next);
	case 't':
		*open_type_class = ISC_FALSE;
		tr.base = value;
		tr.length = strlen(value);
		result = dns_rdatatype_fromtext(&rdtype,
					(isc_textregion_t *)&tr);
		if (result == ISC_R_SUCCESS) {
			if (typeset)
				warn("extra query type");
			if (rdtype == dns_rdatatype_ixfr ||
			    rdtype == dns_rdatatype_axfr)
				fatal("Transfer not supported");
			qtype = rdtype;
			typeset = ISC_TRUE;
		} else
			warn("ignoring invalid type");
		return (value_from_next);
	case 'x':
		result = get_reverse(textname, sizeof(textname), value,
				     ISC_FALSE);
		if (result == ISC_R_SUCCESS) {
			if (curqname != NULL) {
				isc_mem_free(mctx, curqname);
				warn("extra query name");
			}
			curqname = isc_mem_strdup(mctx, textname);
			if (curqname == NULL)
				fatal("out of memory");
			if (typeset)
				warn("extra query type");
			qtype = dns_rdatatype_ptr;
			typeset = ISC_TRUE;
		} else {
			fprintf(stderr, "Invalid IP address %s\n", value);
			exit(1);
		}
		return (value_from_next);
	invalid_option:
	default:
		fprintf(stderr, "Invalid option: -%s\n", option);
		usage();
	}
	/* NOTREACHED */
	return (ISC_FALSE);
}

/*
 * Check for -m first to determine whether to enable
 * memory debugging when setting up the memory context.
 */
static void
preparse_args(int argc, char **argv) {
	char *option;

	for (argc--, argv++; argc > 0; argc--, argv++) {
		if (argv[0][0] != '-')
			continue;
		option = &argv[0][1];
		while (strpbrk(option, single_dash_opts) == &option[0]) {
			if (option[0] == 'm') {
				isc_mem_debugging = ISC_MEM_DEBUGTRACE |
					ISC_MEM_DEBUGRECORD;
				return;
			}
			option = &option[1];
		}
	}
}

/*
 * Argument parsing is based on dig, but simplified: only one
 * QNAME/QCLASS/QTYPE tuple can be specified, and options have
 * been removed that aren't applicable to delv. The interface
 * should be familiar to dig users, however.
 */
static void
parse_args(int argc, char **argv) {
	isc_result_t result;
	isc_textregion_t tr;
	dns_rdatatype_t rdtype;
	dns_rdataclass_t rdclass;
	isc_boolean_t open_type_class = ISC_TRUE;

	for (; argc > 0; argc--, argv++) {
		if (argv[0][0] == '@') {
			server = &argv[0][1];
		} else if (argv[0][0] == '+') {
			plus_option(&argv[0][1]);
		} else if (argv[0][0] == '-') {
			if (argc <= 1) {
				if (dash_option(&argv[0][1], NULL,
						&open_type_class))
				{
					argc--;
					argv++;
				}
			} else {
				if (dash_option(&argv[0][1], argv[1],
						&open_type_class))
				{
					argc--;
					argv++;
				}
			}
		} else {
			/*
			 * Anything which isn't an option
			 */
			if (open_type_class) {
				tr.base = argv[0];
				tr.length = strlen(argv[0]);
				result = dns_rdatatype_fromtext(&rdtype,
					(isc_textregion_t *)&tr);
				if (result == ISC_R_SUCCESS) {
					if (typeset)
						warn("extra query type");
					if (rdtype == dns_rdatatype_ixfr ||
					    rdtype == dns_rdatatype_axfr)
						fatal("Transfer not supported");
					qtype = rdtype;
					typeset = ISC_TRUE;
					continue;
				}
				result = dns_rdataclass_fromtext(&rdclass,
						     (isc_textregion_t *)&tr);
				if (result == ISC_R_SUCCESS) {
					if (classset)
						warn("extra query class");
					else if (rdclass != dns_rdataclass_in)
						warn("ignoring non-IN "
						     "query class");
					continue;
				}
			}

			if (curqname == NULL) {
				curqname = isc_mem_strdup(mctx, argv[0]);
				if (curqname == NULL)
					fatal("out of memory");
			}
		}
	}

	/*
	 * If no qname or qtype specified, search for root/NS
	 * If no qtype specified, use A
	 */
	if (!typeset)
		qtype = dns_rdatatype_a;

	if (curqname == NULL) {
		qname = isc_mem_strdup(mctx, ".");
		if (qname == NULL)
			fatal("out of memory");

		if (!typeset)
			qtype = dns_rdatatype_ns;
	} else
		qname = curqname;
}

static isc_result_t
append_str(const char *text, int len, char **p, char *end) {
	if (len > end - *p)
		return (ISC_R_NOSPACE);
	memmove(*p, text, len);
	*p += len;
	return (ISC_R_SUCCESS);
}

static isc_result_t
reverse_octets(const char *in, char **p, char *end) {
	char *dot = strchr(in, '.');
	int len;
	if (dot != NULL) {
		isc_result_t result;
		result = reverse_octets(dot + 1, p, end);
		if (result != ISC_R_SUCCESS)
			return (result);
		result = append_str(".", 1, p, end);
		if (result != ISC_R_SUCCESS)
			return (result);
		len = (int)(dot - in);
	} else
		len = strlen(in);
	return (append_str(in, len, p, end));
}

static isc_result_t
get_reverse(char *reverse, size_t len, char *value, isc_boolean_t strict) {
	int r;
	isc_result_t result;
	isc_netaddr_t addr;

	addr.family = AF_INET6;
	r = inet_pton(AF_INET6, value, &addr.type.in6);
	if (r > 0) {
		/* This is a valid IPv6 address. */
		dns_fixedname_t fname;
		dns_name_t *name;
		unsigned int options = 0;

		dns_fixedname_init(&fname);
		name = dns_fixedname_name(&fname);
		result = dns_byaddr_createptrname2(&addr, options, name);
		if (result != ISC_R_SUCCESS)
			return (result);
		dns_name_format(name, reverse, (unsigned int)len);
		return (ISC_R_SUCCESS);
	} else {
		/*
		 * Not a valid IPv6 address.  Assume IPv4.
		 * If 'strict' is not set, construct the
		 * in-addr.arpa name by blindly reversing
		 * octets whether or not they look like integers,
		 * so that this can be used for RFC2317 names
		 * and such.
		 */
		char *p = reverse;
		char *end = reverse + len;
		if (strict && inet_pton(AF_INET, value, &addr.type.in) != 1)
			return (DNS_R_BADDOTTEDQUAD);
		result = reverse_octets(value, &p, end);
		if (result != ISC_R_SUCCESS)
			return (result);
		result = append_str(".in-addr.arpa.", 15, &p, end);
		if (result != ISC_R_SUCCESS)
			return (result);
		return (ISC_R_SUCCESS);
	}
}

int
main(int argc, char *argv[]) {
	dns_client_t *client = NULL;
	isc_result_t result;
	dns_fixedname_t qfn;
	dns_name_t *query_name, *response_name;
	dns_rdataset_t *rdataset;
	dns_namelist_t namelist;
	unsigned int resopt, clopt;
	isc_appctx_t *actx = NULL;
	isc_taskmgr_t *taskmgr = NULL;
	isc_socketmgr_t *socketmgr = NULL;
	isc_timermgr_t *timermgr = NULL;
	dns_master_style_t *style = NULL;
#ifndef WIN32
	struct sigaction sa;
#endif

	preparse_args(argc, argv);
	progname = argv[0];

	argc--;
	argv++;

	isc_lib_register();
	result = dns_lib_init();
	if (result != ISC_R_SUCCESS)
		fatal("dns_lib_init failed: %d", result);

	result = isc_mem_create(0, 0, &mctx);
	if (result != ISC_R_SUCCESS)
		fatal("failed to create mctx");

	CHECK(isc_appctx_create(mctx, &actx));
	CHECK(isc_taskmgr_createinctx(mctx, actx, 1, 0, &taskmgr));
	CHECK(isc_socketmgr_createinctx(mctx, actx, &socketmgr));
	CHECK(isc_timermgr_createinctx(mctx, actx, &timermgr));

	parse_args(argc, argv);

	CHECK(setup_style(&style));

	setup_logging(stderr);

	CHECK(isc_app_ctxstart(actx));

#ifndef WIN32
	/* Unblock SIGINT if it's been blocked by isc_app_ctxstart() */
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = SIG_DFL;
	if (sigfillset(&sa.sa_mask) != 0 || sigaction(SIGINT, &sa, NULL) < 0)
		fatal("Couldn't set up signal handler");
#endif

	/* Create client */
	clopt = DNS_CLIENTCREATEOPT_USECACHE;
	result = dns_client_createx2(mctx, actx, taskmgr, socketmgr, timermgr,
				     clopt, &client, srcaddr4, srcaddr6);
	if (result != ISC_R_SUCCESS) {
		delv_log(ISC_LOG_ERROR, "dns_client_create: %s",
			  isc_result_totext(result));
		goto cleanup;
	}

	/* Set the nameserver */
	if (server != NULL)
		addserver(client);
	else
		findserver(client);

	CHECK(setup_dnsseckeys(client));

	/* Construct QNAME */
	CHECK(convert_name(&qfn, &query_name, qname));

	/* Set up resolution options */
	resopt = DNS_CLIENTRESOPT_ALLOWRUN | DNS_CLIENTRESOPT_NOCDFLAG;
	if (no_sigs)
		resopt |= DNS_CLIENTRESOPT_NODNSSEC;
	if (!root_validation && !dlv_validation)
		resopt |= DNS_CLIENTRESOPT_NOVALIDATE;
	if (cdflag)
		resopt &= ~DNS_CLIENTRESOPT_NOCDFLAG;

	/* Perform resolution */
	ISC_LIST_INIT(namelist);
	result = dns_client_resolve(client, query_name, dns_rdataclass_in,
				    qtype, resopt, &namelist);
	if (result != ISC_R_SUCCESS)
		delv_log(ISC_LOG_ERROR, "resolution failed: %s",
			  isc_result_totext(result));

	for (response_name = ISC_LIST_HEAD(namelist);
	     response_name != NULL;
	     response_name = ISC_LIST_NEXT(response_name, link)) {
		for (rdataset = ISC_LIST_HEAD(response_name->list);
		     rdataset != NULL;
		     rdataset = ISC_LIST_NEXT(rdataset, link)) {
			result = printdata(rdataset, response_name, style);
			if (result != ISC_R_SUCCESS)
				delv_log(ISC_LOG_ERROR, "print data failed");
		}
	}

	dns_client_freeresanswer(client, &namelist);

cleanup:
	if (dlv_anchor != NULL)
		isc_mem_free(mctx, dlv_anchor);
	if (trust_anchor != NULL)
		isc_mem_free(mctx, trust_anchor);
	if (anchorfile != NULL)
		isc_mem_free(mctx, anchorfile);
	if (qname != NULL)
		isc_mem_free(mctx, qname);
	if (style != NULL)
		dns_master_styledestroy(&style, mctx);
	if (client != NULL)
		dns_client_destroy(&client);
	if (taskmgr != NULL)
		isc_taskmgr_destroy(&taskmgr);
	if (timermgr != NULL)
		isc_timermgr_destroy(&timermgr);
	if (socketmgr != NULL)
		isc_socketmgr_destroy(&socketmgr);
	if (actx != NULL)
		isc_appctx_destroy(&actx);
	if (lctx != NULL)
		isc_log_destroy(&lctx);
	isc_mem_detach(&mctx);

	dns_lib_shutdown();

	return (0);
}
