/*	$NetBSD: tls.c,v 1.11 2013/05/27 23:15:51 christos Exp $	*/

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
 * tls.c TLS related code for syslogd
 *
 * implements the TLS init and handshake callbacks with all required
 * checks from http://tools.ietf.org/html/draft-ietf-syslog-transport-tls-13
 *
 * Martin Schütte
 */

#include <sys/cdefs.h>
__RCSID("$NetBSD: tls.c,v 1.11 2013/05/27 23:15:51 christos Exp $");

#ifndef DISABLE_TLS
#include "syslogd.h"
#include "tls.h"
#include <netinet/in.h>
#include <ifaddrs.h>
#include "extern.h"

static unsigned getVerifySetting(const char *x509verifystring);

#if !defined(NDEBUG) && defined(__minix)
/* to output SSL error codes */
static const char *SSL_ERRCODE[] = {
	"SSL_ERROR_NONE",
	"SSL_ERROR_SSL",
	"SSL_ERROR_WANT_READ",
	"SSL_ERROR_WANT_WRITE",
	"SSL_ERROR_WANT_X509_LOOKUP",
	"SSL_ERROR_SYSCALL",
	"SSL_ERROR_ZERO_RETURN",
	"SSL_ERROR_WANT_CONNECT",
	"SSL_ERROR_WANT_ACCEPT"};
/* TLS connection states -- keep in sync with symbols in .h */
static const char *TLS_CONN_STATES[] = {
	"ST_NONE",
	"ST_TLS_EST",
	"ST_TCP_EST",
	"ST_CONNECTING",
	"ST_ACCEPTING",
	"ST_READING",
	"ST_WRITING",
	"ST_EOF",
	"ST_CLOSING0",
	"ST_CLOSING1",
	"ST_CLOSING2"};
#endif /* !defined(NDEBUG) && defined(__minix) */

DH *get_dh1024(void);
/* DH parameter precomputed with "openssl dhparam -C -2 1024" */
#ifndef HEADER_DH_H
#include <openssl/dh.h>
#endif
DH *
get_dh1024(void)
{
	static const unsigned char dh1024_p[]={
		0x94,0xBC,0xC4,0x71,0xD4,0xD3,0x2B,0x17,0x69,0xEA,0x82,0x1B,
		0x0F,0x86,0x45,0x57,0xF8,0x86,0x2C,0xC8,0xF5,0x37,0x1F,0x1F,
		0x12,0xDA,0x2C,0x62,0x4C,0xF6,0x95,0xF0,0xE4,0x6A,0x63,0x00,
		0x32,0x54,0x5F,0xA9,0xAA,0x2E,0xD2,0xD3,0xA5,0x7A,0x4E,0xCF,
		0xE8,0x2A,0xF6,0xAB,0xAF,0xD3,0x71,0x3E,0x75,0x9E,0x6B,0xF3,
		0x2E,0x6D,0x97,0x42,0xC2,0x45,0xC0,0x03,0xE1,0x17,0xA4,0x39,
		0xF6,0x36,0xA7,0x11,0xBD,0x30,0xF6,0x6F,0x21,0xBF,0x28,0xE4,
		0xF9,0xE1,0x1E,0x48,0x72,0x58,0xA9,0xC8,0x61,0x65,0xDB,0x66,
		0x36,0xA3,0x77,0x0A,0x81,0x79,0x2C,0x45,0x1E,0x97,0xA6,0xB1,
		0xD9,0x25,0x9C,0x28,0x96,0x91,0x40,0xF8,0xF6,0x86,0x11,0x9C,
		0x88,0xEC,0xA6,0xBA,0x9F,0x4F,0x85,0x43 };
	static const unsigned char dh1024_g[]={ 0x02 };
	DH *dh;

	if ((dh=DH_new()) == NULL)
		return NULL;
	dh->p = BN_bin2bn(dh1024_p, sizeof(dh1024_p), NULL);
	dh->g = BN_bin2bn(dh1024_g, sizeof(dh1024_g), NULL);
	if ((dh->p == NULL) || (dh->g == NULL)) {
		DH_free(dh);
		return NULL;
	}
	return dh;
}

#define ST_CHANGE(x, y) do {					\
	if ((x) != (y)) { 					\
		DPRINTF(D_TLS, "Change state: %s --> %s\n",	\
		    TLS_CONN_STATES[x], TLS_CONN_STATES[y]);	\
		(x) = (y);					\
	}							\
} while (/*CONSTCOND*/0)

static unsigned
getVerifySetting(const char *x509verifystring)
{
	if (!x509verifystring)
		return X509VERIFY_ALWAYS;

	if (!strcasecmp(x509verifystring, "off"))
		return X509VERIFY_NONE;
	else if (!strcasecmp(x509verifystring, "opt"))
		return X509VERIFY_IFPRESENT;
	else
		return X509VERIFY_ALWAYS;
}
/*
 * init OpenSSL lib and one context.
 * returns NULL if global context already exists.
 * returns a status message on successfull init (to be free()d by caller).
 * calls die() on serious error.
 */
char*
init_global_TLS_CTX(void)
{
	const char *keyfilename	  = tls_opt.keyfile;
	const char *certfilename  = tls_opt.certfile;
	const char *CAfile	  = tls_opt.CAfile;
	const char *CApath	  = tls_opt.CAdir;

	SSL_CTX *ctx;
	unsigned x509verify = X509VERIFY_ALWAYS;
	EVP_PKEY *pkey = NULL;
	X509	 *cert = NULL;
	FILE *certfile = NULL;
	FILE  *keyfile = NULL;
	unsigned long err;
	char *fp = NULL, *cn = NULL;

	char statusmsg[1024];

	if (tls_opt.global_TLS_CTX) /* already initialized */
		return NULL;

	x509verify = getVerifySetting(tls_opt.x509verify);
	if (x509verify != X509VERIFY_ALWAYS)
		loginfo("insecure configuration, peer authentication disabled");

	if (!(ctx = SSL_CTX_new(SSLv23_method()))) {
		logerror("Unable to initialize OpenSSL: %s",
		    ERR_error_string(ERR_get_error(), NULL));
		die(0,0,NULL);
	}

	if (!keyfilename)
		keyfilename = DEFAULT_X509_KEYFILE;
	if (!certfilename)
		certfilename = DEFAULT_X509_CERTFILE;

	/* TODO: would it be better to use stat() for access checking? */
	if (!(keyfile  = fopen(keyfilename,  "r"))
	 && !(certfile = fopen(certfilename, "r"))) {
		errno = 0;
		if (!tls_opt.gen_cert) {
			logerror("TLS certificate files \"%s\" and \"%s\""
			    "not readable. Please configure them with "
			    "\"tls_cert\" and \"tls_key\" or set "
			    "\"tls_gen_cert=1\" to generate a new "
			    "certificate", keyfilename, certfilename);
			die(0,0,NULL);
		}

		loginfo("Generating a self-signed certificate and writing "
		    "files \"%s\" and \"%s\"", keyfilename, certfilename);
		if (!mk_x509_cert(&cert, &pkey, TLS_GENCERT_BITS,
		    TLS_GENCERT_SERIAL, TLS_GENCERT_DAYS)) {
			logerror("Unable to generate new certificate.");
			die(0,0,NULL);
		}
		if (!write_x509files(pkey, cert,
		    keyfilename, certfilename)) {
			logerror("Unable to write certificate to files \"%s\""
			    " and \"%s\"", keyfilename, certfilename);
			/* not fatal */
		}
	}
	if (keyfile)
		(void)fclose(keyfile);
	if (certfile)
		(void)fclose(certfile);
	errno = 0;

	/* if generated, then use directly */
	if (cert && pkey) {
		if (!SSL_CTX_use_PrivateKey(ctx, pkey)
		    || !SSL_CTX_use_certificate(ctx, cert)) {
			logerror("Unable to use generated private "
			    "key and certificate: %s",
			    ERR_error_string(ERR_get_error(), NULL));
			die(0,0,NULL);	/* any better reaction? */
		 }
	} else {
		/* load keys and certs from files */
		if (!SSL_CTX_use_PrivateKey_file(ctx, keyfilename,
							SSL_FILETYPE_PEM)
		    || !SSL_CTX_use_certificate_chain_file(ctx, certfilename)) {
			logerror("Unable to load private key and "
			    "certificate from files \"%s\" and \"%s\": %s",
			    keyfilename, certfilename,
			    ERR_error_string(ERR_get_error(), NULL));
			die(0,0,NULL);	/* any better reaction? */
		}
	}
	if (!SSL_CTX_check_private_key(ctx)) {
		logerror("Private key \"%s\" does not match "
		    "certificate \"%s\": %s",
		    keyfilename, certfilename,
		    ERR_error_string(ERR_get_error(), NULL));
		die(0,0,NULL);
	}

	if (CAfile || CApath) {
		if (SSL_CTX_load_verify_locations(ctx, CAfile, CApath) != 1) {
			if (CAfile && CApath)
				logerror("unable to load trust anchors from "
				    "\"%s\" and \"%s\": %s\n",
				    CAfile, CApath, ERR_error_string(
				    ERR_get_error(), NULL));
			else
				logerror("unable to load trust anchors from "
				    "\"%s\": %s\n", (CAfile?CAfile:CApath),
				    ERR_error_string(
				    ERR_get_error(), NULL));
		} else {
			DPRINTF(D_TLS, "loaded trust anchors\n");
		}
	}

	/* options */
	(void)SSL_CTX_set_options(ctx,
	    SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_SINGLE_DH_USE);
	(void)SSL_CTX_set_mode(ctx, SSL_MODE_AUTO_RETRY);

	/* peer verification */
	if ((x509verify == X509VERIFY_NONE)
	    || (x509verify == X509VERIFY_IFPRESENT))
		/* ask for cert, but a client does not have to send one */
		SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, check_peer_cert);
	else
		/* default: ask for cert and check it */
		SSL_CTX_set_verify(ctx,
			SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT,
			check_peer_cert);

	if (SSL_CTX_set_tmp_dh(ctx, get_dh1024()) != 1)
		logerror("SSL_CTX_set_tmp_dh() failed: %s",
		    ERR_error_string(ERR_get_error(), NULL));

	/* make sure the OpenSSL error queue is empty */
	while ((err = ERR_get_error()) != 0)
		logerror("Unexpected OpenSSL error: %s",
		    ERR_error_string(err, NULL));


	/* On successful init the status message is not logged immediately
	 * but passed to the caller. The reason is that init() can continue
	 * to initialize syslog-sign. When the status message is logged
	 * after that it will get a valid signature and not cause errors
	 * with signature verification.
	 */
	if (cert || read_certfile(&cert, certfilename)) {
		get_fingerprint(cert, &fp, NULL);
		get_commonname(cert, &cn);
	}
	DPRINTF(D_TLS, "loaded and checked own certificate\n");
	snprintf(statusmsg, sizeof(statusmsg),
	    "Initialized TLS settings using library \"%s\". "
	    "Use certificate from file \"%s\" with CN \"%s\" "
	    "and fingerprint \"%s\"", SSLeay_version(SSLEAY_VERSION),
	    certfilename, cn, fp);
	free(cn);
	free(fp);

	tls_opt.global_TLS_CTX = ctx;
	return strdup(statusmsg);
}


/*
 * get fingerprint of cert
 * returnstring will be allocated and should be free()d by the caller
 * alg_name selects an algorithm, if it is NULL then DEFAULT_FINGERPRINT_ALG
 * (should be "sha-1") will be used
 * return value and non-NULL *returnstring indicate success
 */
bool
get_fingerprint(const X509 *cert, char **returnstring, const char *alg_name)
{
#define MAX_ALG_NAME_LENGTH 8
	unsigned char md[EVP_MAX_MD_SIZE];
	char fp_val[4];
	size_t memsize, i;
	unsigned len;
	const EVP_MD *digest;
	const char *openssl_algname;
	/* RFC nnnn uses hash function names from
	 * http://www.iana.org/assignments/hash-function-text-names/
	 * in certificate fingerprints.
	 * We have to map them to the hash function names used by OpenSSL.
	 * Actually we use the union of both namespaces to be RFC compliant
	 * and to let the user use "openssl -fingerprint ..."
	 *
	 * Intended behaviour is to prefer the IANA names,
	 * but allow the user to use OpenSSL names as well
	 * (e.g. for "RIPEMD160" wich has no IANA name)
	 */
	static const struct hash_alg_namemap {
		const char *iana;
		const char *openssl;
	} hash_alg_namemap[] = {
		{"md2",	    "MD2"   },
		{"md5",	    "MD5"   },
		{"sha-1",   "SHA1"  },
		{"sha-224", "SHA224"},
		{"sha-256", "SHA256"},
		{"sha-384", "SHA384"},
		{"sha-512", "SHA512"}
	};

	DPRINTF(D_TLS, "get_fingerprint(cert@%p, return@%p, alg \"%s\")\n",
	    cert, returnstring, alg_name);
	*returnstring = NULL;

	if (!alg_name)
		alg_name = DEFAULT_FINGERPRINT_ALG;
	openssl_algname = alg_name;
	for (i = 0; i < A_CNT(hash_alg_namemap); i++)
		if (!strcasecmp(alg_name, hash_alg_namemap[i].iana))
			openssl_algname = hash_alg_namemap[i].openssl;

	if (!(digest = (const EVP_MD *) EVP_get_digestbyname(
	    __UNCONST(openssl_algname)))) {
		DPRINTF(D_TLS, "unknown digest algorithm %s\n",
		    openssl_algname);
		return false;
	}
	if (!X509_digest(cert, digest, md, &len)) {
		DPRINTF(D_TLS, "cannot get %s digest\n", openssl_algname);
		return false;
	}

	/* 'normalise' and translate back to IANA name */
	alg_name = openssl_algname = OBJ_nid2sn(EVP_MD_type(digest));
	for (i = 0; i < A_CNT(hash_alg_namemap); i++)
		if (!strcasecmp(openssl_algname, hash_alg_namemap[i].openssl))
			alg_name = hash_alg_namemap[i].iana;

	/* needed memory: 3 string bytes for every binary byte with delimiter
	 *		  + max_iana_strlen with delimiter  */
	memsize = (len * 3) + strlen(alg_name) + 1;
	MALLOC(*returnstring, memsize);
	(void)strlcpy(*returnstring, alg_name, memsize);
	(void)strlcat(*returnstring, ":", memsize);
	/* append the fingeprint data */
	for (i = 0; i < len; i++) {
		(void)snprintf(fp_val, sizeof(fp_val),
			"%02X:", (unsigned) md[i]);
		(void)strlcat(*returnstring, fp_val, memsize);
	}
	return true;
}

/*
 * gets first CN from cert in returnstring (has to be freed by caller)
 * on failure it returns false and *returnstring is NULL
 */
bool
get_commonname(X509 *cert, char **returnstring)
{
	X509_NAME *x509name;
	X509_NAME_ENTRY *entry;
	unsigned char *ubuf;
	int len, i;

	x509name = X509_get_subject_name(cert);
	i = X509_NAME_get_index_by_NID(x509name, NID_commonName, -1);
	if (i != -1) {
		entry = X509_NAME_get_entry(x509name, i);
		len = ASN1_STRING_to_UTF8(&ubuf,
		    X509_NAME_ENTRY_get_data(entry));
		if (len > 0) {
			MALLOC(*returnstring, (size_t)len+1);
			strlcpy(*returnstring, (const char*)ubuf, len+1);
			OPENSSL_free(ubuf);
			return true;
		}
		OPENSSL_free(ubuf);
	}
	*returnstring = NULL;
	return false;
}
/*
 * test if cert matches as configured hostname or IP
 * checks a 'really used' hostname and optionally a second expected subject
 * against iPAddresses, dnsNames and commonNames
 *
 * TODO: wildcard matching for dnsNames is not implemented.
 *	 in transport-tls that is a MAY, and I do not trust them anyway.
 *	 but there might be demand for, so it's a todo item.
 */
bool
match_hostnames(X509 *cert, const char *hostname, const char *subject)
{
	int i, len, num;
	char *buf;
	unsigned char *ubuf;
	GENERAL_NAMES *gennames;
	GENERAL_NAME *gn;
	X509_NAME *x509name;
	X509_NAME_ENTRY *entry;
	ASN1_OCTET_STRING *asn1_ip, *asn1_cn_ip;
	int crit, idx;

	DPRINTF((D_TLS|D_CALL), "match_hostnames(%p, \"%s\", \"%s\")\n",
	    cert, hostname, subject);

	/* see if hostname is an IP */
	if ((subject  && (asn1_ip = a2i_IPADDRESS(subject )))
	 || (hostname && (asn1_ip = a2i_IPADDRESS(hostname))))
		/* nothing */;
	else
		asn1_ip = NULL;

	if (!(gennames = X509_get_ext_d2i(cert, NID_subject_alt_name,
	    &crit, &idx))) {
		DPRINTF(D_TLS, "X509_get_ext_d2i() returned (%p,%d,%d) "
		    "--> no subjectAltName\n", gennames, crit, idx);
	} else {
		num = sk_GENERAL_NAME_num(gennames);
		if (asn1_ip) {
			/* first loop: check IPs */
			for (i = 0; i < num; ++i) {
				gn = sk_GENERAL_NAME_value(gennames, i);
				if (gn->type == GEN_IPADD
				    && !ASN1_OCTET_STRING_cmp(asn1_ip,
					gn->d.iPAddress))
					return true;
			}
		}
		/* second loop: check DNS names */
		for (i = 0; i < num; ++i) {
			gn = sk_GENERAL_NAME_value(gennames, i);
			if (gn->type == GEN_DNS) {
				buf = (char *)ASN1_STRING_data(gn->d.ia5);
				len = ASN1_STRING_length(gn->d.ia5);
				if (!strncasecmp(subject, buf, len)
				    || !strncasecmp(hostname, buf, len))
					return true;
			}
		}
	}

	/* check commonName; not sure if more than one CNs possible, but we
	 * will look at all of them */
	x509name = X509_get_subject_name(cert);
	i = X509_NAME_get_index_by_NID(x509name, NID_commonName, -1);
	while (i != -1) {
		entry = X509_NAME_get_entry(x509name, i);
		len = ASN1_STRING_to_UTF8(&ubuf,
		    X509_NAME_ENTRY_get_data(entry));
		if (len > 0) {
			DPRINTF(D_TLS, "found CN: %.*s\n", len, ubuf);
			/* hostname */
			if ((subject && !strncasecmp(subject,
			    (const char*)ubuf, len))
			    || (hostname && !strncasecmp(hostname,
			    (const char*)ubuf, len))) {
				OPENSSL_free(ubuf);
				return true;
			}
			OPENSSL_free(ubuf);
			/* IP -- convert to ASN1_OCTET_STRING and compare then
			 * so that "10.1.2.3" and "10.01.02.03" are equal */
			if ((asn1_ip)
			    && subject
			    && (asn1_cn_ip = a2i_IPADDRESS(subject))
			    && !ASN1_OCTET_STRING_cmp(asn1_ip, asn1_cn_ip)) {
				return true;
			}
		}
		i = X509_NAME_get_index_by_NID(x509name, NID_commonName, i);
	}
	return false;
}

/*
 * check if certificate matches given fingerprint
 */
bool
match_fingerprint(const X509 *cert, const char *fingerprint)
{
#define MAX_ALG_NAME_LENGTH 8
	char alg[MAX_ALG_NAME_LENGTH];
	char *certfingerprint;
	char *p;
	const char *q;

	DPRINTF((D_TLS|D_CALL), "match_fingerprint(cert@%p, fp \"%s\")\n",
		cert, fingerprint);
	if (!fingerprint)
		return false;

	/* get algorithm */
	p = alg;
	q = fingerprint;
	while (*q != ':' && *q != '\0' && p < alg + MAX_ALG_NAME_LENGTH)
		*p++ = *q++;
	*p = '\0';

	if (!get_fingerprint(cert, &certfingerprint, alg)) {
		DPRINTF(D_TLS, "cannot get %s digest\n", alg);
		return false;
	}
	if (strncmp(certfingerprint, fingerprint, strlen(certfingerprint))) {
		DPRINTF(D_TLS, "fail: fingerprints do not match\n");
		free(certfingerprint);
		return false;
	}
	DPRINTF(D_TLS, "accepted: fingerprints match\n");
	free(certfingerprint);
	return true;
}

/*
 * check if certificate matches given certificate file
 */
bool
match_certfile(const X509 *cert1, const char *certfilename)
{
	X509 *cert2;
	char *fp1, *fp2;
	bool rc = false;
	errno = 0;

	if (read_certfile(&cert2, certfilename)
	    && get_fingerprint(cert1, &fp1, NULL)
	    && get_fingerprint(cert2, &fp2, NULL)) {
		if (!strcmp(fp1, fp2))
			rc = true;
		FREEPTR(fp1);
		FREEPTR(fp2);
	 }
	DPRINTF((D_TLS|D_CALL), "match_certfile(cert@%p, file \"%s\") "
	    "returns %d\n", cert1, certfilename, rc);
	return rc;
}

/*
 * reads X.509 certificate from file
 * caller has to free it later with 'OPENSSL_free(cert);'
 */
bool
read_certfile(X509 **cert, const char *certfilename)
{
	FILE *certfile;
	errno = 0;

	DPRINTF((D_TLS|D_CALL), "read_certfile(%p, \"%s\")\n",
		cert, certfilename);
	if (!cert || !certfilename)
		return false;

	if (!(certfile = fopen(certfilename, "rb"))) {
		logerror("Unable to open certificate file: %s", certfilename);
		return false;
	}

	/* either PEM or DER */
	if (!(*cert = PEM_read_X509(certfile, NULL, NULL, NULL))
	    && !(*cert = d2i_X509_fp(certfile, NULL))) {
		DPRINTF((D_TLS), "Unable to read certificate from %s\n",
			certfilename);
		(void)fclose(certfile);
		return false;
	}
	else {
		DPRINTF((D_TLS), "Read certificate from %s\n", certfilename);
		(void)fclose(certfile);
		return true;
	}
}

/* used for incoming connections in check_peer_cert() */
int
accept_cert(const char* reason, struct tls_conn_settings *conn_info,
	char *cur_fingerprint, char *cur_subjectline)
{
	/* When using DSA keys the callback gets called twice.
	 * This flag avoids multiple log messages for the same connection.
	 */
	if (!conn_info->accepted)
		loginfo("Established connection and accepted %s certificate "
		    "from %s due to %s. Subject is \"%s\", fingerprint is"
		    " \"%s\"", conn_info->incoming ? "server" : "client",
		    conn_info->hostname, reason, cur_subjectline,
		    cur_fingerprint);

	if (cur_fingerprint && !conn_info->fingerprint)
		conn_info->fingerprint = cur_fingerprint;
	else
		FREEPTR(cur_fingerprint);

	if (cur_subjectline && !conn_info->subject)
		conn_info->subject = cur_subjectline;
	else
		FREEPTR(cur_subjectline);

	conn_info->accepted = true;
	return 1;
}
int
deny_cert(struct tls_conn_settings *conn_info,
	char *cur_fingerprint, char *cur_subjectline)
{
	if (!conn_info->accepted)
		loginfo("Deny %s certificate from %s. "
		    "Subject is \"%s\", fingerprint is \"%s\"",
		    conn_info->incoming ? "client" : "server",
		    conn_info->hostname,
		    cur_subjectline, cur_fingerprint);
	else
		logerror("Error with TLS %s certificate authentication, "
		    "already approved certificate became invalid. "
		    "Subject is \"%s\", fingerprint is \"%s\"",
		    conn_info->incoming ? "client" : "server",
		    cur_subjectline, cur_fingerprint);
	FREEPTR(cur_fingerprint);
	FREEPTR(cur_subjectline);
	return 0;
}

/*
 * Callback after OpenSSL has verified a peer certificate,
 * gets called for every certificate in a chain (starting with root CA).
 * preverify_ok indicates a valid trust path (necessary),
 * then we check whether the hostname or configured subject matches the cert.
 */
int
check_peer_cert(int preverify_ok, X509_STORE_CTX *ctx)
{
	char *cur_subjectline = NULL;
	char *cur_fingerprint = NULL;
	char cur_issuerline[256];
	SSL *ssl;
	X509 *cur_cert;
	int cur_err, cur_depth;
	struct tls_conn_settings *conn_info;
	struct peer_cred *cred, *tmp_cred;

	/* read context info */
	cur_cert = X509_STORE_CTX_get_current_cert(ctx);
	cur_err = X509_STORE_CTX_get_error(ctx);
	cur_depth = X509_STORE_CTX_get_error_depth(ctx);
	ssl = X509_STORE_CTX_get_ex_data(ctx,
	    SSL_get_ex_data_X509_STORE_CTX_idx());
	conn_info = SSL_get_app_data(ssl);

	/* some info */
	(void)get_commonname(cur_cert, &cur_subjectline);
	(void)get_fingerprint(cur_cert, &cur_fingerprint, NULL);
	DPRINTF((D_TLS|D_CALL), "check cert for connection with %s. "
	    "depth is %d, preverify is %d, subject is %s, fingerprint "
	    "is %s, conn_info@%p%s\n", conn_info->hostname, cur_depth,
	    preverify_ok, cur_subjectline, cur_fingerprint, conn_info,
	    (conn_info->accepted ? ", cb was already called" : ""));

	if (Debug && !preverify_ok) {
		DPRINTF(D_TLS, "openssl verify error:"
		    "num=%d:%s:depth=%d:%s\t\n", cur_err,
		    X509_verify_cert_error_string(cur_err),
		    cur_depth, cur_subjectline);
		if (cur_err == X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT) {
			X509_NAME_oneline(
			    X509_get_issuer_name(ctx->current_cert),
			    cur_issuerline, sizeof(cur_issuerline));
			DPRINTF(D_TLS, "openssl verify error:missing "
			    "cert for issuer=%s\n", cur_issuerline);
		}
	}

	/*
	 * quite a lot of variables here,
	 * the big if/elseif covers all possible combinations.
	 *
	 * here is a list, ordered like the conditions below:
	 * - conn_info->x509verify
	 *   X509VERIFY_NONE:	   do not verify certificates,
	 *			   only log its subject and fingerprint
	 *   X509VERIFY_IFPRESENT: if we got her, then a cert is present,
	 *			   so check it normally
	 *   X509VERIFY_ALWAYS:	   normal certificate check
	 * - cur_depth:
	 *   > 0:  peer provided CA cert. remember if its valid,
	 *	   but always accept, because most checks work on depth 0
	 *   == 0: the peer's own cert. check this for final decision
	 * - preverify_ok:
	 *   true:  valid certificate chain from a trust anchor to this cert
	 *   false: no valid and trusted certificate chain
	 * - conn_info->incoming:
	 *   true:  we are the server, means we authenticate against all
	 *	    allowed attributes in tls_opt
	 *   false: otherwise we are client and conn_info has all attributes
	 *	    to check
	 * - conn_info->fingerprint (only if !conn_info->incoming)
	 *   NULL:  no fingerprint configured, only check certificate chain
	 *   !NULL: a peer cert with this fingerprint is trusted
	 *
	 */
	/* shortcut */
	if (cur_depth != 0) {
		FREEPTR(cur_fingerprint);
		FREEPTR(cur_subjectline);
		return 1;
	}

	if (conn_info->x509verify == X509VERIFY_NONE)
		return accept_cert("disabled verification", conn_info,
		    cur_fingerprint, cur_subjectline);

	/* implicit: (cur_depth == 0)
	 *	  && (conn_info->x509verify != X509VERIFY_NONE) */
	if (conn_info->incoming) {
		if (preverify_ok)
			return accept_cert("valid certificate chain",
			    conn_info, cur_fingerprint, cur_subjectline);

		/* else: now check allowed client fingerprints/certs */
		SLIST_FOREACH(cred, &tls_opt.fprint_head, entries) {
			if (match_fingerprint(cur_cert, cred->data)) {
				return accept_cert("matching fingerprint",
				    conn_info, cur_fingerprint,
				    cur_subjectline);
			}
		}
		SLIST_FOREACH_SAFE(cred, &tls_opt.cert_head,
			entries, tmp_cred) {
			if (match_certfile(cur_cert, cred->data))
				return accept_cert("matching certfile",
				    conn_info, cur_fingerprint,
				    cur_subjectline);
		}
		return deny_cert(conn_info, cur_fingerprint, cur_subjectline);
	}

	/* implicit: (cur_depth == 0)
	 *	  && (conn_info->x509verify != X509VERIFY_NONE)
	 *	  && !conn_info->incoming */
	if (!conn_info->incoming && preverify_ok) {
		/* certificate chain OK. check subject/hostname */
		if (match_hostnames(cur_cert, conn_info->hostname,
		    conn_info->subject))
			return accept_cert("matching hostname/subject",
			    conn_info, cur_fingerprint, cur_subjectline);
		else
			return deny_cert(conn_info, cur_fingerprint,
			    cur_subjectline);
	} else if (!conn_info->incoming && !preverify_ok) {
		/* chain not OK. check fingerprint/subject/hostname */
		if (match_fingerprint(cur_cert, conn_info->fingerprint))
			return accept_cert("matching fingerprint", conn_info,
			    cur_fingerprint, cur_subjectline);
		else if (match_certfile(cur_cert, conn_info->certfile))
			return accept_cert("matching certfile", conn_info,
			    cur_fingerprint, cur_subjectline);
		else
			return deny_cert(conn_info, cur_fingerprint,
			    cur_subjectline);
	}

	FREEPTR(cur_fingerprint);
	FREEPTR(cur_subjectline);
	return 0;
}

/*
 * Create TCP sockets for incoming TLS connections.
 * To be used like socksetup(), hostname and port are optional,
 * returns bound stream sockets.
 */
struct socketEvent *
socksetup_tls(const int af, const char *bindhostname, const char *port)
{
	struct addrinfo hints, *res, *r;
	int error, maxs;
	const int on = 1;
	struct socketEvent *s, *socks;

	if(!tls_opt.server
	|| !tls_opt.global_TLS_CTX)
		return NULL;

	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = AI_PASSIVE;
	hints.ai_family = af;
	hints.ai_socktype = SOCK_STREAM;

	error = getaddrinfo(bindhostname, (port ? port : "syslog-tls"),
	    &hints, &res);
	if (error) {
		logerror("%s", gai_strerror(error));
		errno = 0;
		die(0, 0, NULL);
	}

	/* Count max number of sockets we may open */
	for (maxs = 0, r = res; r; r = r->ai_next, maxs++)
		continue;
	socks = malloc((maxs+1) * sizeof(*socks));
	if (!socks) {
		logerror("Unable to allocate memory for sockets");
		die(0, 0, NULL);
	}

	socks->fd = 0;	 /* num of sockets counter at start of array */
	s = socks + 1;
	for (r = res; r; r = r->ai_next) {
		if ((s->fd = socket(r->ai_family, r->ai_socktype,
			r->ai_protocol)) == -1) {
			logerror("socket() failed: %s", strerror(errno));
			continue;
		}
		s->af = r->ai_family;
#if defined(__minix) && defined(INET6)
		if (r->ai_family == AF_INET6
		 && setsockopt(s->fd, IPPROTO_IPV6, IPV6_V6ONLY,
			&on, sizeof(on)) == -1) {
			logerror("setsockopt(IPV6_V6ONLY) failed: %s",
			    strerror(errno));
			close(s->fd);
			continue;
		}
#endif /* defined(__minix) && defined(INET6) */
		if (setsockopt(s->fd, SOL_SOCKET, SO_REUSEADDR,
			&on, sizeof(on)) == -1) {
			DPRINTF(D_NET, "Unable to setsockopt(): %s\n",
			    strerror(errno));
		}
		if ((error = bind(s->fd, r->ai_addr, r->ai_addrlen)) == -1) {
			logerror("bind() failed: %s", strerror(errno));
			/* is there a better way to handle a EADDRINUSE? */
			close(s->fd);
			continue;
		}
		if (listen(s->fd, TLSBACKLOG) == -1) {
			logerror("listen() failed: %s", strerror(errno));
			close(s->fd);
			continue;
		}
		s->ev = allocev();
		event_set(s->ev, s->fd, EV_READ | EV_PERSIST,
		    dispatch_socket_accept, s->ev);
		EVENT_ADD(s->ev);

		socks->fd = socks->fd + 1;  /* num counter */
		s++;
	}

	if (socks->fd == 0) {
		free (socks);
		if(Debug)
			return NULL;
		else
			die(0, 0, NULL);
	}
	if (res)
		freeaddrinfo(res);

	return socks;
}

/*
 * Dispatch routine for non-blocking SSL_connect()
 * Has to be idempotent in case of TLS_RETRY (~ EAGAIN),
 * so we can continue a slow handshake.
 */
/*ARGSUSED*/
void
dispatch_SSL_connect(int fd, short event, void *arg)
{
	struct tls_conn_settings *conn_info = (struct tls_conn_settings *) arg;
	SSL *ssl = conn_info->sslptr;
	int rc, error;
	sigset_t newmask, omask;
	struct timeval tv;

	BLOCK_SIGNALS(omask, newmask);
	DPRINTF((D_TLS|D_CALL), "dispatch_SSL_connect(conn_info@%p, fd %d)\n",
	    conn_info, fd);
	assert(conn_info->state == ST_TCP_EST
	    || conn_info->state == ST_CONNECTING);

	ST_CHANGE(conn_info->state, ST_CONNECTING);
	rc = SSL_connect(ssl);
	if (0 >= rc) {
		error = tls_examine_error("SSL_connect()",
		    conn_info->sslptr, NULL, rc);
		switch (error) {
		case TLS_RETRY_READ:
			event_set(conn_info->retryevent, fd, EV_READ,
			    dispatch_SSL_connect, conn_info);
			EVENT_ADD(conn_info->retryevent);
			break;
		case TLS_RETRY_WRITE:
			event_set(conn_info->retryevent, fd, EV_WRITE,
			    dispatch_SSL_connect, conn_info);
			EVENT_ADD(conn_info->retryevent);
			break;
		default: /* should not happen,
			  * ... but does if the cert is not accepted */
			logerror("Cannot establish TLS connection "
			    "to \"%s\" -- TLS handshake aborted "
			    "before certificate authentication.",
			    conn_info->hostname);
			ST_CHANGE(conn_info->state, ST_NONE);
			conn_info->reconnect = 5 * TLS_RECONNECT_SEC;
			tv.tv_sec = conn_info->reconnect;
			tv.tv_usec = 0;
			schedule_event(&conn_info->event, &tv,
			    tls_reconnect, conn_info);
			break;
		}
		RESTORE_SIGNALS(omask);
		return;
	}
	/* else */
	conn_info->reconnect = TLS_RECONNECT_SEC;
	event_set(conn_info->event, fd, EV_READ, dispatch_tls_eof, conn_info);
	EVENT_ADD(conn_info->event);

	DPRINTF(D_TLS, "TLS connection established.\n");
	ST_CHANGE(conn_info->state, ST_TLS_EST);

	send_queue(0, 0, get_f_by_conninfo(conn_info));
	RESTORE_SIGNALS(omask);
}

/*
 * establish TLS connection
 */
bool
tls_connect(struct tls_conn_settings *conn_info)
{
	struct addrinfo hints, *res, *res1;
	int    error, rc, sock;
	const int one = 1;
	char   buf[MAXLINE];
	SSL    *ssl = NULL;

	DPRINTF((D_TLS|D_CALL), "tls_connect(conn_info@%p)\n", conn_info);
	assert(conn_info->state == ST_NONE);

	if(!tls_opt.global_TLS_CTX)
		return false;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = 0;
	hints.ai_flags = AI_CANONNAME;
	error = getaddrinfo(conn_info->hostname,
	    (conn_info->port ? conn_info->port : "syslog-tls"), &hints, &res);
	if (error) {
		logerror("%s", gai_strerror(error));
		return false;
	}

	sock = -1;
	for (res1 = res; res1; res1 = res1->ai_next) {
		if ((sock = socket(res1->ai_family, res1->ai_socktype,
		    res1->ai_protocol)) == -1) {
			DPRINTF(D_NET, "Unable to open socket.\n");
			continue;
		}
		if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
			&one, sizeof(one)) == -1) {
			DPRINTF(D_NET, "Unable to setsockopt(): %s\n",
			    strerror(errno));
		}
		if (connect(sock, res1->ai_addr, res1->ai_addrlen) == -1) {
			DPRINTF(D_NET, "Unable to connect() to %s: %s\n",
			    res1->ai_canonname, strerror(errno));
			close(sock);
			sock = -1;
			continue;
		}
		ST_CHANGE(conn_info->state, ST_TCP_EST);

		if (!(ssl = SSL_new(tls_opt.global_TLS_CTX))) {
			ERR_error_string_n(ERR_get_error(), buf, sizeof(buf));
			DPRINTF(D_TLS, "Unable to establish TLS: %s\n", buf);
			close(sock);
			sock = -1;
			ST_CHANGE(conn_info->state, ST_NONE);
			continue;
		}
		if (!SSL_set_fd(ssl, sock)) {
			ERR_error_string_n(ERR_get_error(), buf, sizeof(buf));
			DPRINTF(D_TLS, "Unable to connect TLS to socket: %s\n",
			    buf);
			FREE_SSL(ssl);
			close(sock);
			sock = -1;
			ST_CHANGE(conn_info->state, ST_NONE);
			continue;
		}

		SSL_set_app_data(ssl, conn_info);
		SSL_set_connect_state(ssl);
		while ((rc = ERR_get_error()) != 0) {
			ERR_error_string_n(rc, buf, sizeof(buf));
			DPRINTF(D_TLS, "Found SSL error in queue: %s\n", buf);
		}
		errno = 0;  /* reset to be sure we get the right one later on */

		if ((fcntl(sock, F_SETFL, O_NONBLOCK)) == -1) {
			DPRINTF(D_NET, "Unable to fcntl(sock, O_NONBLOCK): "
			    "%s\n", strerror(errno));
		}

		/* now we have a TCP connection, so assume we can
		 * use that and do not have to try another res */
		conn_info->sslptr = ssl;

		assert(conn_info->state == ST_TCP_EST);
		assert(conn_info->event);
		assert(conn_info->retryevent);

		freeaddrinfo(res);
		dispatch_SSL_connect(sock, 0, conn_info);
		return true;
	}
	/* still no connection after for loop */
	DPRINTF((D_TLS|D_NET), "Unable to establish a TCP connection to %s\n",
	    conn_info->hostname);
	freeaddrinfo(res);

	assert(conn_info->state == ST_NONE);
	if (sock != -1)
		close(sock);
	if (ssl) {
		SSL_shutdown(ssl);
		SSL_free(ssl);
	}
	return false;
}

int
tls_examine_error(const char *functionname, const SSL *ssl,
	struct tls_conn_settings *tls_conn, const int rc)
{
	int ssl_error, err_error;

	ssl_error = SSL_get_error(ssl, rc);
	DPRINTF(D_TLS, "%s returned rc %d and error %s: %s\n", functionname,
		rc, SSL_ERRCODE[ssl_error], ERR_error_string(ssl_error, NULL));
	switch (ssl_error) {
	case SSL_ERROR_WANT_READ:
		return TLS_RETRY_READ;
	case SSL_ERROR_WANT_WRITE:
		return TLS_RETRY_WRITE;
	case SSL_ERROR_SYSCALL:
		DPRINTF(D_TLS, "SSL_ERROR_SYSCALL: ");
		err_error = ERR_get_error();
		if ((rc == -1) && (err_error == 0)) {
			DPRINTF(D_TLS, "socket I/O error: %s\n",
			    strerror(errno));
		} else if ((rc == 0) && (err_error == 0)) {
			DPRINTF(D_TLS, "unexpected EOF from %s\n",
			    tls_conn ? tls_conn->hostname : NULL);
		} else {
			DPRINTF(D_TLS, "no further info\n");
		}
		return TLS_PERM_ERROR;
	case SSL_ERROR_ZERO_RETURN:
		logerror("TLS connection closed by %s",
		    tls_conn ? tls_conn->hostname : NULL);
		return TLS_PERM_ERROR;
	case SSL_ERROR_SSL:
		logerror("internal SSL error, error queue gives %s",
		    ERR_error_string(ERR_get_error(), NULL));
		return TLS_PERM_ERROR;
	default:
		break;
	}
	if (tls_conn)
		tls_conn->errorcount++;
	/* TODO: is this ever reached? */
	return TLS_TEMP_ERROR;
}


bool
parse_tls_destination(const char *p, struct filed *f, size_t linenum)
{
	const char *q;

	if ((*p++ != '@') || *p++ != '[') {
		logerror("parse_tls_destination() on non-TLS action "
		    "in config line %zu", linenum);
		return false;
	}

	if (!(q = strchr(p, ']'))) {
		logerror("Unterminated [ "
		    "in config line %zu", linenum);
		return false;
	}

	if (!(f->f_un.f_tls.tls_conn =
		calloc(1, sizeof(*f->f_un.f_tls.tls_conn)))
	 || !(f->f_un.f_tls.tls_conn->event = allocev())
	 || !(f->f_un.f_tls.tls_conn->retryevent = allocev())) {
		if (f->f_un.f_tls.tls_conn)
			free(f->f_un.f_tls.tls_conn->event);
		free(f->f_un.f_tls.tls_conn);
		logerror("Couldn't allocate memory for TLS config");
		return false;
	}
	/* default values */
	f->f_un.f_tls.tls_conn->x509verify = X509VERIFY_ALWAYS;
	f->f_un.f_tls.tls_conn->reconnect = TLS_RECONNECT_SEC;

	if (!(copy_string(&(f->f_un.f_tls.tls_conn->hostname), p, q))) {
		logerror("Unable to read TLS server name"
		    "in config line %zu", linenum);
		free_tls_conn(f->f_un.f_tls.tls_conn);
		return false;
	}
	p = ++q;

	if (*p == ':') {
		p++; q++;
		while (isalnum((unsigned char)*q))
			q++;
		if (!(copy_string(&(f->f_un.f_tls.tls_conn->port), p, q))) {
			logerror("Unable to read TLS port or service name"
				" after ':' in config line %zu", linenum);
			free_tls_conn(f->f_un.f_tls.tls_conn);
			return false;
		}
		p = q;
	}
	/* allow whitespace for readability? */
	while (isblank((unsigned char)*p))
		p++;
	if (*p == '(') {
		p++;
		while (*p != ')') {
			if (copy_config_value_quoted("subject=\"",
			    &(f->f_un.f_tls.tls_conn->subject), &p)
			    || copy_config_value_quoted("fingerprint=\"",
			    &(f->f_un.f_tls.tls_conn->fingerprint), &p)
			    || copy_config_value_quoted("cert=\"",
			    &(f->f_un.f_tls.tls_conn->certfile), &p)) {
			/* nothing */
			} else if (!strcmp(p, "verify=")) {
				q = p += sizeof("verify=")-1;
				/* "" are optional */
				if (*p == '\"') { p++; q++; }
				while (isalpha((unsigned char)*q)) q++;
				f->f_un.f_tls.tls_conn->x509verify =
				    getVerifySetting(p);
				if (*q == '\"') q++;  /* "" are optional */
				p = q;
			} else {
				logerror("unknown keyword %s "
				    "in config line %zu", p, linenum);
			}
			while (*p == ',' || isblank((unsigned char)*p))
				p++;
			if (*p == '\0') {
				logerror("unterminated ("
				    "in config line %zu", linenum);
			}
		}
	}

	DPRINTF((D_TLS|D_PARSE),
	    "got TLS config: host %s, port %s, "
	    "subject: %s, certfile: %s, fingerprint: %s\n",
	    f->f_un.f_tls.tls_conn->hostname,
	    f->f_un.f_tls.tls_conn->port,
	    f->f_un.f_tls.tls_conn->subject,
	    f->f_un.f_tls.tls_conn->certfile,
	    f->f_un.f_tls.tls_conn->fingerprint);
	return true;
}

/*
 * Dispatch routine (triggered by timer) to reconnect to a lost TLS server
 */
/*ARGSUSED*/
void
tls_reconnect(int fd, short event, void *arg)
{
	struct tls_conn_settings *conn_info = (struct tls_conn_settings *) arg;

	DPRINTF((D_TLS|D_CALL|D_EVENT), "tls_reconnect(conn_info@%p, "
	    "server %s)\n", conn_info, conn_info->hostname);
	if (conn_info->sslptr) {
		conn_info->shutdown = true;
		free_tls_sslptr(conn_info);
	}
	assert(conn_info->state == ST_NONE);

	if (!tls_connect(conn_info)) {
		if (conn_info->reconnect > TLS_RECONNECT_GIVEUP) {
			logerror("Unable to connect to TLS server %s, "
			    "giving up now", conn_info->hostname);
			message_queue_freeall(get_f_by_conninfo(conn_info));
			/* free the message queue; but do not free the
			 * tls_conn_settings nor change the f_type to F_UNUSED.
			 * that way one can still trigger a reconnect
			 * with a SIGUSR1
			 */
		} else {
			struct timeval tv;
			logerror("Unable to connect to TLS server %s, "
			    "try again in %d sec", conn_info->hostname,
			    conn_info->reconnect);
			tv.tv_sec = conn_info->reconnect;
			tv.tv_usec = 0;
			schedule_event(&conn_info->event, &tv,
			    tls_reconnect, conn_info);
			TLS_RECONNECT_BACKOFF(conn_info->reconnect);
		}
	} else {
		assert(conn_info->state == ST_TLS_EST
		    || conn_info->state == ST_CONNECTING
		    || conn_info->state == ST_NONE);
	}
}
/*
 * Dispatch routine for accepting TLS connections.
 * Has to be idempotent in case of TLS_RETRY (~ EAGAIN),
 * so we can continue a slow handshake.
 */
/*ARGSUSED*/
void
dispatch_tls_accept(int fd, short event, void *arg)
{
	struct tls_conn_settings *conn_info = (struct tls_conn_settings *) arg;
	int rc, error;
	struct TLS_Incoming_Conn *tls_in;
	sigset_t newmask, omask;

	DPRINTF((D_TLS|D_CALL),
		"dispatch_tls_accept(conn_info@%p, fd %d)\n", conn_info, fd);
	assert(conn_info->event);
	assert(conn_info->retryevent);
	BLOCK_SIGNALS(omask, newmask);

	ST_CHANGE(conn_info->state, ST_ACCEPTING);
	rc = SSL_accept(conn_info->sslptr);
	if (0 >= rc) {
		error = tls_examine_error("SSL_accept()",
		    conn_info->sslptr, NULL, rc);
		switch (error) {
		case TLS_RETRY_READ:
			event_set(conn_info->retryevent, fd, EV_READ,
			    dispatch_tls_accept, conn_info);
			EVENT_ADD(conn_info->retryevent);
			break;
		case TLS_RETRY_WRITE:
			event_set(conn_info->retryevent, fd, EV_WRITE,
			    dispatch_tls_accept, conn_info);
			EVENT_ADD(conn_info->retryevent);
			break;
		default: /* should not happen */
			free_tls_conn(conn_info);
			break;
		}
		RESTORE_SIGNALS(omask);
		return;
	}
	/* else */
	CALLOC(tls_in, sizeof(*tls_in));
	CALLOC(tls_in->inbuf, (size_t)TLS_MIN_LINELENGTH);

	tls_in->tls_conn = conn_info;
	tls_in->socket = SSL_get_fd(conn_info->sslptr);
	tls_in->inbuf[0] = '\0';
	tls_in->inbuflen = TLS_MIN_LINELENGTH;
	SLIST_INSERT_HEAD(&TLS_Incoming_Head, tls_in, entries);

	event_set(conn_info->event, tls_in->socket, EV_READ | EV_PERSIST,
	    dispatch_tls_read, tls_in);
	EVENT_ADD(conn_info->event);
	ST_CHANGE(conn_info->state, ST_TLS_EST);

	loginfo("established TLS connection from %s with certificate "
	    "%s (%s)", conn_info->hostname, conn_info->subject,
	    conn_info->fingerprint);
	RESTORE_SIGNALS(omask);
	/*
	 * We could also listen to EOF kevents -- but I do not think
	 * that would be useful, because we still had to read() the buffer
	 * before closing the socket.
	 */
}

/*
 * Dispatch routine for accepting TCP connections and preparing
 * the tls_conn_settings object for a following SSL_accept().
 */
/*ARGSUSED*/
void
dispatch_socket_accept(int fd, short event, void *ev)
{
#ifdef LIBWRAP
	struct request_info req;
#endif
	struct sockaddr_storage frominet;
	socklen_t addrlen;
	int newsock, rc;
	sigset_t newmask, omask;
	SSL *ssl;
	struct tls_conn_settings *conn_info;
	char hbuf[NI_MAXHOST];
	char *peername;

	DPRINTF((D_TLS|D_NET), "incoming TCP connection\n");
	if (!tls_opt.global_TLS_CTX) {
		logerror("global_TLS_CTX not initialized!");
		return;
	}

	BLOCK_SIGNALS(omask, newmask);
	addrlen = sizeof(frominet);
	if ((newsock = accept(fd, (struct sockaddr *)&frominet,
	    &addrlen)) == -1) {
		logerror("Error in accept(): %s", strerror(errno));
		RESTORE_SIGNALS(omask);
		return;
	}
	/* TODO: do we want an IP or a hostname? maybe even both? */
	if ((rc = getnameinfo((struct sockaddr *)&frominet, addrlen,
	    hbuf, sizeof(hbuf), NULL, 0, NI_NUMERICHOST|NI_NUMERICSERV)) != 0) {
		DPRINTF(D_NET, "could not get peername: %s", gai_strerror(rc));
		peername = NULL;
	}
	else {
		size_t len = strlen(hbuf) + 1;
		MALLOC(peername, len);
		(void)memcpy(peername, hbuf, len);
	}

#ifdef LIBWRAP
	request_init(&req, RQ_DAEMON, appname, RQ_FILE, newsock, NULL);
	fromhost(&req);
	if (!hosts_access(&req)) {
		logerror("access from %s denied by hosts_access", peername);
		shutdown(newsock, SHUT_RDWR);
		close(newsock);
		RESTORE_SIGNALS(omask);
		return;
	}
#endif

	if ((fcntl(newsock, F_SETFL, O_NONBLOCK)) == -1) {
		DPRINTF(D_NET, "Unable to fcntl(sock, O_NONBLOCK): %s\n",
		    strerror(errno));
	}

	if (!(ssl = SSL_new(tls_opt.global_TLS_CTX))) {
		DPRINTF(D_TLS, "Unable to establish TLS: %s\n",
		    ERR_error_string(ERR_get_error(), NULL));
		close(newsock);
		RESTORE_SIGNALS(omask);
		return;
	}
	if (!SSL_set_fd(ssl, newsock)) {
		DPRINTF(D_TLS, "Unable to connect TLS to socket %d: %s\n",
			newsock, ERR_error_string(ERR_get_error(), NULL));
		SSL_free(ssl);
		close(newsock);
		RESTORE_SIGNALS(omask);
		return;
	}

	if (!(conn_info = calloc(1, sizeof(*conn_info)))
	    || !(conn_info->event = allocev())
	    || !(conn_info->retryevent = allocev())) {
		if (conn_info)
			free(conn_info->event);
		free(conn_info);
		SSL_free(ssl);
		close(newsock);
		logerror("Unable to allocate memory to accept incoming "
		    "TLS connection from %s", peername);
		RESTORE_SIGNALS(omask);
		return;
	}
	ST_CHANGE(conn_info->state, ST_NONE);
	/* store connection details inside ssl object, used to verify
	 * cert and immediately match against hostname */
	conn_info->hostname = peername;
	conn_info->sslptr = ssl;
	conn_info->x509verify = getVerifySetting(tls_opt.x509verify);
	conn_info->incoming = true;
	SSL_set_app_data(ssl, conn_info);
	SSL_set_accept_state(ssl);

	assert(conn_info->event);
	assert(conn_info->retryevent);

	ST_CHANGE(conn_info->state, ST_TCP_EST);
	DPRINTF(D_TLS, "socket connection from %s accept()ed with fd %d, "
		"calling SSL_accept()...\n",  peername, newsock);
	dispatch_tls_accept(newsock, 0, conn_info);
	RESTORE_SIGNALS(omask);
}

/*
 * Dispatch routine to read from outgoing TCP/TLS sockets.
 *
 * I do not know if libevent can tell us the difference
 * between available data and an EOF. But it does not matter
 * because there should not be any incoming data.
 * So we close the connection either because the peer closed its
 * side or because the peer broke the protocol by sending us stuff  ;-)
 */
void
dispatch_tls_eof(int fd, short event, void *arg)
{
	struct tls_conn_settings *conn_info = (struct tls_conn_settings *) arg;
	sigset_t newmask, omask;
	struct timeval tv;

	BLOCK_SIGNALS(omask, newmask);
	DPRINTF((D_TLS|D_EVENT|D_CALL), "dispatch_eof_tls(%d, %d, %p)\n",
	    fd, event, arg);
	assert(conn_info->state == ST_TLS_EST);
	ST_CHANGE(conn_info->state, ST_EOF);
	DEL_EVENT(conn_info->event);

	free_tls_sslptr(conn_info);

	/* this overwrites the EV_READ event */
	tv.tv_sec = conn_info->reconnect;
	tv.tv_usec = 0;
	schedule_event(&conn_info->event, &tv, tls_reconnect, conn_info);
	TLS_RECONNECT_BACKOFF(conn_info->reconnect);
	RESTORE_SIGNALS(omask);
}

/*
 * Dispatch routine to read from TCP/TLS sockets.
 * NB: This gets called when the TCP socket has data available, thus
 *     we can call SSL_read() on it. But that does not mean the SSL buffer
 *     holds a complete record and SSL_read() lets us read any data now.
 */
/*ARGSUSED*/
void
dispatch_tls_read(int fd_lib, short event, void *arg)
{
	struct TLS_Incoming_Conn *c = (struct TLS_Incoming_Conn *) arg;
	int fd = c->socket;
	int error;
	int rc;
	sigset_t newmask, omask;
	bool retrying;

	BLOCK_SIGNALS(omask, newmask);
	DPRINTF((D_TLS|D_EVENT|D_CALL), "active TLS socket %d\n", fd);
	DPRINTF(D_TLS, "calling SSL_read(%p, %p, %zu)\n", c->tls_conn->sslptr,
		&(c->inbuf[c->read_pos]), c->inbuflen - c->read_pos);
	retrying = (c->tls_conn->state == ST_READING);
	ST_CHANGE(c->tls_conn->state, ST_READING);
	rc = SSL_read(c->tls_conn->sslptr, &(c->inbuf[c->read_pos]),
		c->inbuflen - c->read_pos);
	if (rc <= 0) {
		error = tls_examine_error("SSL_read()", c->tls_conn->sslptr,
		    c->tls_conn, rc);
		switch (error) {
		case TLS_RETRY_READ:
			/* normal event loop will call us again */
			break;
		case TLS_RETRY_WRITE:
			if (!retrying)
				event_del(c->tls_conn->event);
			event_set(c->tls_conn->retryevent, fd,
				EV_WRITE, dispatch_tls_read, c);
			EVENT_ADD(c->tls_conn->retryevent);
			RESTORE_SIGNALS(omask);
			return;
		case TLS_TEMP_ERROR:
			if (c->tls_conn->errorcount < TLS_MAXERRORCOUNT)
				break;
			/* FALLTHROUGH */
		case TLS_PERM_ERROR:
			/* there might be data in the inbuf, so only
			 * mark for closing after message retrieval */
			c->closenow = true;
			break;
		default:
			break;
		}
	} else {
		DPRINTF(D_TLS, "SSL_read() returned %d\n", rc);
		c->errorcount = 0;
		c->read_pos += rc;
	}
	if (retrying)
		EVENT_ADD(c->tls_conn->event);
	tls_split_messages(c);
	if (c->closenow) {
		free_tls_conn(c->tls_conn);
		FREEPTR(c->inbuf);
		SLIST_REMOVE(&TLS_Incoming_Head, c, TLS_Incoming_Conn, entries);
		free(c);
	} else
		ST_CHANGE(c->tls_conn->state, ST_TLS_EST);
	RESTORE_SIGNALS(omask);
}

/* moved message splitting out of dispatching function.
 * now we can call it recursively.
 *
 * TODO: the code for oversized messages still needs testing,
 * especially for the skipping case.
 */
void
tls_split_messages(struct TLS_Incoming_Conn *c)
{
/* define only to make it better readable */
#define MSG_END_OFFSET (c->cur_msg_start + c->cur_msg_len)
	size_t offset = 0;
	size_t msglen = 0;
	char *newbuf;
	char buf_char;

	DPRINTF((D_TLS|D_CALL|D_DATA), "tls_split_messages() -- "
		"incoming status is msg_start %zu, msg_len %zu, pos %zu\n",
		c->cur_msg_start, c->cur_msg_len, c->read_pos);

	if (!c->read_pos)
		return;

	if (c->dontsave && c->read_pos < MSG_END_OFFSET) {
		c->cur_msg_len -= c->read_pos;
		c->read_pos = 0;
	} else if (c->dontsave && c->read_pos == MSG_END_OFFSET) {
		c->cur_msg_start = c->cur_msg_len = c->read_pos = 0;
		c->dontsave = false;
	} else if (c->dontsave && c->read_pos > MSG_END_OFFSET) {
		/* move remaining input to start of buffer */
		DPRINTF(D_DATA, "move inbuf of length %zu by %zu chars\n",
		    c->read_pos - (MSG_END_OFFSET),
		    MSG_END_OFFSET);
		memmove(&c->inbuf[0],
		    &c->inbuf[MSG_END_OFFSET],
		    c->read_pos - (MSG_END_OFFSET));
		c->read_pos -= (MSG_END_OFFSET);
		c->cur_msg_start = c->cur_msg_len = 0;
		c->dontsave = false;
	}
	if (c->read_pos < MSG_END_OFFSET) {
		return;
	}

	/* read length prefix, always at start of buffer */
	while (isdigit((unsigned char)c->inbuf[offset])
	    && offset < c->read_pos) {
		msglen *= 10;
		msglen += c->inbuf[offset] - '0';
		offset++;
	}
	if (offset == c->read_pos) {
		/* next invocation will have more data */
		return;
	}
	if (c->inbuf[offset] == ' ') {
		c->cur_msg_len = msglen;
		c->cur_msg_start = offset + 1;
		if (MSG_END_OFFSET+1 > c->inbuflen) {  /* +1 for the '\0' */
			newbuf = realloc(c->inbuf, MSG_END_OFFSET+1);
			if (newbuf) {
				DPRINTF(D_DATA, "Reallocated inbuf\n");
				c->inbuflen = MSG_END_OFFSET+1;
				c->inbuf = newbuf;
			} else {
				logerror("Couldn't reallocate buffer, "
				    "will skip this message");
				c->dontsave = true;
				c->cur_msg_len -= c->read_pos;
				c->cur_msg_start = 0;
				c->read_pos = 0;
			}
		}
	} else {
		/* found non-digit in prefix */
		/* Question: would it be useful to skip this message and
		 * try to find next message by looking for its beginning?
		 * IMHO not.
		 */
		logerror("Unable to handle TLS length prefix. "
		    "Protocol error? Closing connection now.");
		/* only set flag -- caller has to close then */
		c->closenow = true;
		return;
	}
	/* read one syslog message */
	if (c->read_pos >= MSG_END_OFFSET) {
		/* process complete msg */
		assert(MSG_END_OFFSET+1 <= c->inbuflen);
		/* message in c->inbuf is not NULL-terminated,
		 * so this avoids a complete copy */
		buf_char = c->inbuf[MSG_END_OFFSET];
		c->inbuf[MSG_END_OFFSET] = '\0';
		printline(c->tls_conn->hostname, &c->inbuf[c->cur_msg_start],
		    RemoteAddDate ? ADDDATE : 0);
		c->inbuf[MSG_END_OFFSET] = buf_char;

		if (MSG_END_OFFSET == c->read_pos) {
			/* no unprocessed data in buffer --> reset to empty */
			c->cur_msg_start = c->cur_msg_len = c->read_pos = 0;
		} else {
			/* move remaining input to start of buffer */
			DPRINTF(D_DATA, "move inbuf of length %zu by %zu "
			    "chars\n", c->read_pos - (MSG_END_OFFSET),
			    MSG_END_OFFSET);
			memmove(&c->inbuf[0], &c->inbuf[MSG_END_OFFSET],
			    c->read_pos - (MSG_END_OFFSET));
			c->read_pos -= (MSG_END_OFFSET);
			c->cur_msg_start = c->cur_msg_len = 0;
		}
	}

	/* shrink inbuf if too large */
	if ((c->inbuflen > TLS_PERSIST_LINELENGTH)
	 && (c->read_pos < TLS_LARGE_LINELENGTH)) {
		newbuf = realloc(c->inbuf, TLS_LARGE_LINELENGTH);
		if (newbuf) {
			DPRINTF(D_DATA, "Shrink inbuf\n");
			c->inbuflen = TLS_LARGE_LINELENGTH;
			c->inbuf = newbuf;
		} else {
			logerror("Couldn't shrink inbuf");
			/* no change necessary */
		}
	}
	DPRINTF(D_DATA, "return with status: msg_start %zu, msg_len %zu, "
	    "pos %zu\n", c->cur_msg_start, c->cur_msg_len, c->read_pos);

	/* try to read another message */
	if (c->read_pos > 10)
		tls_split_messages(c);
	return;
}

/*
 * wrapper for dispatch_tls_send()
 *
 * send one line with tls
 * f has to be of typ TLS
 *
 * returns false if message cannot be sent right now,
 *	caller is responsible to enqueue it
 * returns true if message passed to dispatch_tls_send()
 *	delivery is not garantueed, but likely
 */
#define DEBUG_LINELENGTH 40
bool
tls_send(struct filed *f, char *line, size_t len, struct buf_queue *qentry)
{
	struct tls_send_msg *smsg;

	DPRINTF((D_TLS|D_CALL), "tls_send(f=%p, line=\"%.*s%s\", "
	    "len=%zu) to %sconnected dest.\n", f,
	    (int)(len > DEBUG_LINELENGTH ? DEBUG_LINELENGTH : len),
	    line, (len > DEBUG_LINELENGTH ? "..." : ""),
	    len, f->f_un.f_tls.tls_conn->sslptr ? "" : "un");

	if(f->f_un.f_tls.tls_conn->state == ST_TLS_EST) {
		/* send now */
		if (!(smsg = calloc(1, sizeof(*smsg)))) {
			logerror("Unable to allocate memory, drop message");
			return false;
		}
		smsg->f = f;
		smsg->line = line;
		smsg->linelen = len;
		(void)NEWREF(qentry->msg);
		smsg->qentry = qentry;
		DPRINTF(D_DATA, "now sending line: \"%.*s\"\n",
		    (int)smsg->linelen, smsg->line);
		dispatch_tls_send(0, 0, smsg);
		return true;
	} else {
		/* other socket operation active, send later  */
		DPRINTF(D_DATA, "connection not ready to send: \"%.*s\"\n",
		    (int)len, line);
		return false;
	}
}

/*ARGSUSED*/
void
dispatch_tls_send(int fd, short event, void *arg)
{
	struct tls_send_msg *smsg = (struct tls_send_msg *) arg;
	struct tls_conn_settings *conn_info = smsg->f->f_un.f_tls.tls_conn;
	struct filed *f = smsg->f;
	int rc, error;
	sigset_t newmask, omask;
	bool retrying;
	struct timeval tv;

	BLOCK_SIGNALS(omask, newmask);
	DPRINTF((D_TLS|D_CALL), "dispatch_tls_send(f=%p, buffer=%p, "
	    "line@%p, len=%zu, offset=%zu) to %sconnected dest.\n",
	    smsg->f, smsg->qentry->msg, smsg->line,
	    smsg->linelen, smsg->offset,
		conn_info->sslptr ? "" : "un");
	assert(conn_info->state == ST_TLS_EST
	    || conn_info->state == ST_WRITING);

	retrying = (conn_info->state == ST_WRITING);
	ST_CHANGE(conn_info->state, ST_WRITING);
	rc = SSL_write(conn_info->sslptr,
	    (smsg->line + smsg->offset),
	    (smsg->linelen - smsg->offset));
	if (0 >= rc) {
		error = tls_examine_error("SSL_write()",
		    conn_info->sslptr,
		    conn_info, rc);
		switch (error) {
		case TLS_RETRY_READ:
			/* collides with eof event */
			if (!retrying)
				event_del(conn_info->event);
			event_set(conn_info->retryevent, fd, EV_READ,
				dispatch_tls_send, smsg);
			RETRYEVENT_ADD(conn_info->retryevent);
			break;
		case TLS_RETRY_WRITE:
			event_set(conn_info->retryevent, fd, EV_WRITE,
			    dispatch_tls_send, smsg);
			RETRYEVENT_ADD(conn_info->retryevent);
			break;
		case TLS_PERM_ERROR:
			/* no need to check active events */
			free_tls_send_msg(smsg);
			free_tls_sslptr(conn_info);
			tv.tv_sec = conn_info->reconnect;
			tv.tv_usec = 0;
			schedule_event(&conn_info->event, &tv,
			    tls_reconnect, conn_info);
			TLS_RECONNECT_BACKOFF(conn_info->reconnect);
			break;
		default:
			break;
		}
		RESTORE_SIGNALS(omask);
		return;
	} else if ((size_t)rc < smsg->linelen) {
		DPRINTF((D_TLS|D_DATA), "TLS: SSL_write() wrote %d out of %zu "
		    "bytes\n", rc, (smsg->linelen - smsg->offset));
		smsg->offset += rc;
		/* try again */
		if (retrying)
			EVENT_ADD(conn_info->event);
		dispatch_tls_send(0, 0, smsg);
		return;
	} else if ((size_t)rc == (smsg->linelen - smsg->offset)) {
		DPRINTF((D_TLS|D_DATA), "TLS: SSL_write() complete\n");
		ST_CHANGE(conn_info->state, ST_TLS_EST);
		free_tls_send_msg(smsg);
		send_queue(0, 0, f);

	} else {
		/* should not be reached */
		/*LINTED constcond */
		assert(0);
		DPRINTF((D_TLS|D_DATA), "unreachable code after SSL_write()\n");
		ST_CHANGE(conn_info->state, ST_TLS_EST);
		free_tls_send_msg(smsg);
		send_queue(0, 0, f);
	}
	if (retrying && conn_info->event->ev_events)
		EVENT_ADD(conn_info->event);
	RESTORE_SIGNALS(omask);
}

/*
 * Close a SSL connection and its queue and its tls_conn.
 */
void
free_tls_conn(struct tls_conn_settings *conn_info)
{
	DPRINTF(D_MEM, "free_tls_conn(conn_info@%p) with sslptr@%p\n",
		conn_info, conn_info->sslptr);

	if (conn_info->sslptr) {
		conn_info->shutdown = true;
		free_tls_sslptr(conn_info);
	}
	assert(conn_info->state == ST_NONE);

	FREEPTR(conn_info->port);
	FREEPTR(conn_info->subject);
	FREEPTR(conn_info->hostname);
	FREEPTR(conn_info->certfile);
	FREEPTR(conn_info->fingerprint);
	DEL_EVENT(conn_info->event);
	DEL_EVENT(conn_info->retryevent);
	FREEPTR(conn_info->event);
	FREEPTR(conn_info->retryevent);
	FREEPTR(conn_info);
	DPRINTF(D_MEM2, "free_tls_conn(conn_info@%p) returns\n", conn_info);
}

/*
 * Dispatch routine for non-blocking TLS shutdown
 */
/*ARGSUSED*/
void
dispatch_SSL_shutdown(int fd, short event, void *arg)
{
	struct tls_conn_settings *conn_info = (struct tls_conn_settings *) arg;
	int rc, error;
	sigset_t newmask, omask;
	bool retrying;

	BLOCK_SIGNALS(omask, newmask);
	DPRINTF((D_TLS|D_CALL),
	    "dispatch_SSL_shutdown(conn_info@%p, fd %d)\n", conn_info, fd);
	retrying = ((conn_info->state == ST_CLOSING0)
	     || (conn_info->state == ST_CLOSING1)
	     || (conn_info->state == ST_CLOSING2));
	if (!retrying)
		ST_CHANGE(conn_info->state, ST_CLOSING0);

	rc = SSL_shutdown(conn_info->sslptr);
	if (rc == 1) {	/* shutdown complete */
		DPRINTF((D_TLS|D_NET), "Closed TLS connection to %s\n",
		    conn_info->hostname);
		ST_CHANGE(conn_info->state, ST_TCP_EST);  /* check this */
		conn_info->accepted = false;
		/* closing TCP comes below */
	} else if (rc == 0) { /* unidirectional, now call a 2nd time */
		/* problem: when connecting as a client to rsyslogd this
		 * loops and I keep getting rc == 0
		 * maybe I hit this bug?
		 * http://www.mail-archive.com/openssl-dev@openssl.org/msg24105.html
		 *
		 * anyway, now I use three closing states to make sure I abort
		 * after two rc = 0.
		 */
		if (conn_info->state == ST_CLOSING0) {
			ST_CHANGE(conn_info->state, ST_CLOSING1);
			dispatch_SSL_shutdown(fd, 0, conn_info);
		} else if (conn_info->state == ST_CLOSING1) {
			ST_CHANGE(conn_info->state, ST_CLOSING2);
			dispatch_SSL_shutdown(fd, 0, conn_info);
		} else if (conn_info->state == ST_CLOSING2) {
			/* abort shutdown, jump to close TCP below */
		} else
			DPRINTF(D_TLS, "Unexpected connection state %d\n",
				conn_info->state);
			/* and abort here too*/
	} else if (rc == -1 && conn_info->shutdown ) {
		(void)tls_examine_error("SSL_shutdown()",
			conn_info->sslptr, NULL, rc);
		DPRINTF((D_TLS|D_NET), "Ignore error in SSL_shutdown()"
			" and force connection shutdown.");
		ST_CHANGE(conn_info->state, ST_TCP_EST);
		conn_info->accepted = false;
	} else if (rc == -1 && !conn_info->shutdown ) {
		error = tls_examine_error("SSL_shutdown()",
			conn_info->sslptr, NULL, rc);
		switch (error) {
		case TLS_RETRY_READ:
			if (!retrying)
				event_del(conn_info->event);
			event_set(conn_info->retryevent, fd, EV_READ,
			    dispatch_SSL_shutdown, conn_info);
			EVENT_ADD(conn_info->retryevent);
			RESTORE_SIGNALS(omask);
			return;
		case TLS_RETRY_WRITE:
			if (!retrying)
				event_del(conn_info->event);
			event_set(conn_info->retryevent, fd, EV_WRITE,
			    dispatch_SSL_shutdown, conn_info);
			EVENT_ADD(conn_info->retryevent);
			RESTORE_SIGNALS(omask);
			return;
		default:
			/* force close() on the TCP connection */
			ST_CHANGE(conn_info->state, ST_TCP_EST);
			conn_info->accepted = false;
			break;
		}
	}
	if ((conn_info->state != ST_TLS_EST)
	    && (conn_info->state != ST_NONE)
	    && (conn_info->state != ST_CLOSING0)
	    && (conn_info->state != ST_CLOSING1)) {
		int sock = SSL_get_fd(conn_info->sslptr);

		if (shutdown(sock, SHUT_RDWR) == -1)
			logerror("Cannot shutdown socket");
		DEL_EVENT(conn_info->retryevent);
		DEL_EVENT(conn_info->event);

		if (close(sock) == -1)
			logerror("Cannot close socket");
		DPRINTF((D_TLS|D_NET), "Closed TCP connection to %s\n",
		    conn_info->hostname);
		ST_CHANGE(conn_info->state, ST_NONE);
		FREE_SSL(conn_info->sslptr);
	 }
	RESTORE_SIGNALS(omask);
}

/*
 * Close a SSL object
 */
void
free_tls_sslptr(struct tls_conn_settings *conn_info)
{
	int sock;
	DPRINTF(D_MEM, "free_tls_sslptr(conn_info@%p)\n", conn_info);

	if (!conn_info->sslptr) {
		assert(conn_info->incoming == 1
		    || conn_info->state == ST_NONE);
		return;
	} else {
		sock = SSL_get_fd(conn_info->sslptr);
		dispatch_SSL_shutdown(sock, 0, conn_info);
	}
}

/* write self-generated certificates */
bool
write_x509files(EVP_PKEY *pkey, X509 *cert,
	const char *keyfilename, const char *certfilename)
{
	FILE *certfile, *keyfile;

	if (!(umask(0177),(keyfile  = fopen(keyfilename,  "a")))) {
		logerror("Unable to write to file \"%s\"", keyfilename);
		return false;
	}
	if (!(umask(0122),(certfile = fopen(certfilename, "a")))) {
		logerror("Unable to write to file \"%s\"", certfilename);
		(void)fclose(keyfile);
		return false;
	}
	if (!PEM_write_PrivateKey(keyfile, pkey, NULL, NULL, 0, NULL, NULL))
		logerror("Unable to write key to \"%s\"", keyfilename);
	if (!X509_print_fp(certfile, cert)
	    || !PEM_write_X509(certfile, cert))
		logerror("Unable to write certificate to \"%s\"",
		    certfilename);

	(void)fclose(keyfile);
	(void)fclose(certfile);
	return true;
}


/* adds all local IP addresses as subjectAltNames to cert x.
 * getifaddrs() should be quite portable among BSDs and Linux
 * but if not available the whole function can simply be removed.
 */
bool
x509_cert_add_subjectAltName(X509 *cert, X509V3_CTX *ctx)
{
	struct ifaddrs *ifa = NULL, *ifp = NULL;
	char ip[100];
	char subjectAltName[2048];
	int idx = 0;
	socklen_t salen;
	X509_EXTENSION *ext;
#ifdef notdef
	STACK_OF(X509_EXTENSION) *extlist;
	extlist = sk_X509_EXTENSION_new_null();
#endif

	if (getifaddrs (&ifp) == -1) {
		logerror("Unable to get list of local interfaces");
		return false;
	}

	idx = snprintf(subjectAltName, sizeof(subjectAltName),
	    "DNS:%s", LocalFQDN);

	for (ifa = ifp; ifa; ifa = ifa->ifa_next) {
		if(!ifa->ifa_addr)
			continue;

		/* only IP4 and IP6 addresses, but filter loopbacks */
		if (ifa->ifa_addr->sa_family == AF_INET) {
			struct sockaddr_in *addr =
			    (struct sockaddr_in *)ifa->ifa_addr;
			if (addr->sin_addr.s_addr == htonl(INADDR_LOOPBACK))
				continue;
			salen = sizeof(struct sockaddr_in);
		} else if (ifa->ifa_addr->sa_family == AF_INET6) {
			struct in6_addr *addr6 =
			    &((struct sockaddr_in6 *)ifa->ifa_addr)->sin6_addr;
			if (IN6_IS_ADDR_LOOPBACK(addr6))
				continue;
			salen = sizeof(struct sockaddr_in6);
		} else
			continue;

		if (getnameinfo(ifa->ifa_addr, salen, ip, sizeof(ip),
		    NULL, 0, NI_NUMERICHOST)) {
			continue;
		}

		/* add IP to list */
		idx += snprintf(&subjectAltName[idx],
		    sizeof(subjectAltName)-idx, ", IP:%s", ip);
	}
	freeifaddrs (ifp);

	ext = X509V3_EXT_conf_nid(NULL, ctx,
	    NID_subject_alt_name, subjectAltName);
	X509_add_ext(cert, ext, -1);
	X509_EXTENSION_free(ext);

	return true;
}

/*
 * generates a private key and a X.509 certificate
 */
bool
mk_x509_cert(X509 **x509p, EVP_PKEY **pkeyp, int bits, int serial, int days)
{
	X509	       *cert;
	EVP_PKEY       *pk;
	DSA	       *dsa;
	X509_NAME      *name = NULL;
	X509_EXTENSION *ex = NULL;
	X509V3_CTX	ctx;

	DPRINTF((D_CALL|D_TLS), "mk_x509_cert(%p, %p, %d, %d, %d)\n",
	    x509p, pkeyp, bits, serial, days);

	if (pkeyp && *pkeyp)
		pk = *pkeyp;
	else if ((pk = EVP_PKEY_new()) == NULL) {
		DPRINTF(D_TLS, "EVP_PKEY_new() failed\n");
		return false;
	}

	if (x509p && *x509p)
		cert = *x509p;
	else if ((cert = X509_new()) == NULL) {
		DPRINTF(D_TLS, "X509_new() failed\n");
		return false;
	}

	dsa = DSA_generate_parameters(bits, NULL, 0,
			    NULL, NULL, NULL, NULL);
	if (!DSA_generate_key(dsa)) {
		DPRINTF(D_TLS, "DSA_generate_key() failed\n");
		return false;
	}
	if (!EVP_PKEY_assign_DSA(pk, dsa)) {
		DPRINTF(D_TLS, "EVP_PKEY_assign_DSA() failed\n");
		return false;
	}

	X509_set_version(cert, 3);
	ASN1_INTEGER_set(X509_get_serialNumber(cert), serial);
	X509_gmtime_adj(X509_get_notBefore(cert), 0);
	X509_gmtime_adj(X509_get_notAfter(cert), (long)60 * 60 * 24 * days);

	if (!X509_set_pubkey(cert, pk)) {
		DPRINTF(D_TLS, "X509_set_pubkey() failed\n");
		return false;
	}

	/*
	 * This function creates and adds the entry, working out the correct
	 * string type and performing checks on its length. Normally we'd check
	 * the return value for errors...
	 */
	name = X509_get_subject_name(cert);
	/*
	X509_NAME_add_entry_by_txt(name, "O", MBSTRING_ASC,
	    (unsigned char *)"The NetBSD Project", -1, -1, 0);
	X509_NAME_add_entry_by_txt(name, "OU", MBSTRING_ASC,
	    (unsigned char *)"syslogd", -1, -1, 0);
	*/
	X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
	    (unsigned char *) LocalFQDN, -1, -1, 0);
	X509_set_issuer_name(cert, name);

	/*
	 * Add extension using V3 code: we can set the config file as NULL
	 * because we wont reference any other sections.
	 */
	X509V3_set_ctx(&ctx, cert, cert, NULL, NULL, 0);

	ex = X509V3_EXT_conf_nid(NULL, &ctx, NID_netscape_comment,
	    __UNCONST("auto-generated by the NetBSD syslogd"));
	X509_add_ext(cert, ex, -1);
	X509_EXTENSION_free(ex);

	ex = X509V3_EXT_conf_nid(NULL, &ctx, NID_netscape_ssl_server_name,
	    LocalFQDN);
	X509_add_ext(cert, ex, -1);
	X509_EXTENSION_free(ex);

	ex = X509V3_EXT_conf_nid(NULL, &ctx, NID_netscape_cert_type,
	    __UNCONST("server, client"));
	X509_add_ext(cert, ex, -1);
	X509_EXTENSION_free(ex);

	ex = X509V3_EXT_conf_nid(NULL, &ctx, NID_key_usage,
	    __UNCONST("keyAgreement, keyEncipherment, "
	    "nonRepudiation, digitalSignature"));
	X509_add_ext(cert, ex, -1);
	X509_EXTENSION_free(ex);

	ex = X509V3_EXT_conf_nid(NULL, &ctx, NID_basic_constraints,
	    __UNCONST("critical,CA:FALSE"));
	X509_add_ext(cert, ex, -1);
	X509_EXTENSION_free(ex);

	(void)x509_cert_add_subjectAltName(cert, &ctx);

	if (!X509_sign(cert, pk, EVP_dss1())) {
		DPRINTF(D_TLS, "X509_sign() failed\n");
		return false;
	}
	if (X509_verify(cert, pk) != 1) {
		DPRINTF(D_TLS, "X509_verify() failed\n");
		return false;
	}

	*x509p = cert;
	*pkeyp = pk;
	return true;
}

void
free_tls_send_msg(struct tls_send_msg *msg)
{
	if (!msg) {
		DPRINTF((D_DATA), "invalid tls_send_msg_free(NULL)\n");
		return;
	}
	DELREF(msg->qentry->msg);
	(void)message_queue_remove(msg->f, msg->qentry);
	FREEPTR(msg->line);
	FREEPTR(msg);
}
#endif /* !DISABLE_TLS */
