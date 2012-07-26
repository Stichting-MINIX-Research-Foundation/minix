/*
 * Copyright (C) 2002  Nuno M. Rodrigues.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND NUNO M. RODRIGUES
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
 * INTERNET SOFTWARE CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
 * FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: bdb.c,v 1.1 2002-05-16 04:25:22 marka Exp $ */

/*
 * BIND 9.1.x simple database driver
 * implementation, using Berkeley DB.
 */

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <isc/file.h>
#include <isc/log.h>
#include <isc/lib.h>
#include <isc/mem.h>
#include <isc/msgs.h>
#include <isc/msgcat.h>
#include <isc/region.h>
#include <isc/result.h>
#include <isc/types.h>
#include <isc/util.h>

#include <dns/sdb.h>
#include <dns/log.h>
#include <dns/lib.h>
#include <dns/ttl.h>

#include <named/bdb.h>
#include <named/globals.h>
#include <named/config.h>

#include <db.h>

#define DRIVERNAME	"bdb"

static dns_sdbimplementation_t *bdb_imp;

static isc_result_t
bdb_create(const char *zone, int argc, char **argv,
	   void *unused, void **dbdata)
{
	int ret;

	UNUSED(zone);
	UNUSED(unused);

	if (argc < 1)
		return ISC_R_FAILURE;	/* database path must be given */

	if (db_create((DB **)dbdata, NULL, 0) != 0) {
		/*
		 * XXX Should use dns_msgcat et al
		 * but seems to be unavailable.
		 */
		isc_log_iwrite(dns_lctx, DNS_LOGCATEGORY_DATABASE,
			       DNS_LOGMODULE_SDB, ISC_LOG_CRITICAL, isc_msgcat,
			       ISC_MSGSET_GENERAL, ISC_MSG_FATALERROR,
			       "db_create");
		return ISC_R_FAILURE;
	}

	if (isc_file_exists(*argv) != ISC_TRUE) {
		isc_log_iwrite(dns_lctx, DNS_LOGCATEGORY_DATABASE,
			       DNS_LOGMODULE_SDB, ISC_LOG_CRITICAL, isc_msgcat,
			       ISC_MSGSET_GENERAL, ISC_MSG_FATALERROR,
			       "isc_file_exists: %s", *argv);
		return ISC_R_FAILURE;
	}

	if ((ret = (*(DB **)dbdata)->open(*(DB **)dbdata, *argv, NULL, DB_HASH,
	    DB_RDONLY, 0)) != 0) {
			isc_log_iwrite(dns_lctx, DNS_LOGCATEGORY_DATABASE,
				       DNS_LOGMODULE_SDB, ISC_LOG_CRITICAL,
				       isc_msgcat, ISC_MSGSET_GENERAL,
				       ISC_MSG_FATALERROR, "DB->open: %s",
				       db_strerror(ret));
			return ISC_R_FAILURE;
	}
	return ISC_R_SUCCESS;
}

static isc_result_t
bdb_lookup(const char *zone, const char *name, void *dbdata,
	   dns_sdblookup_t *l)
{
	int ret;
	char *type, *rdata;
	dns_ttl_t ttl;
	isc_consttextregion_t ttltext;
	DBC *c;
	DBT key, data;

	UNUSED(zone);

	if ((ret = ((DB *)dbdata)->cursor((DB *)dbdata, NULL, &c, 0)) != 0) {
		isc_log_iwrite(dns_lctx, DNS_LOGCATEGORY_DATABASE,
			       DNS_LOGMODULE_SDB, ISC_LOG_ERROR,
			       isc_msgcat, ISC_MSGSET_GENERAL,
			       ISC_MSG_FAILED, "DB->cursor: %s",
			       db_strerror(ret));
		return ISC_R_FAILURE;
	}

	memset(&key, 0, sizeof(DBT));
	memset(&data, 0, sizeof(DBT));

	(const char *)key.data = name;
	key.size = strlen(name);

	ret = c->c_get(c, &key, &data, DB_SET);
	while (ret == 0) {
		((char *)key.data)[key.size] = 0;
		((char *)data.data)[data.size] = 0;
		ttltext.base = strtok((char *)data.data, " ");
		ttltext.length = strlen(ttltext.base);
		dns_ttl_fromtext((isc_textregion_t *)&ttltext, &ttl);
		type = strtok(NULL, " ");
		rdata = type + strlen(type) + 1;

		if (dns_sdb_putrr(l, type, ttl, rdata) != ISC_R_SUCCESS) {
			isc_log_iwrite(dns_lctx,
				       DNS_LOGCATEGORY_DATABASE,
				       DNS_LOGMODULE_SDB, ISC_LOG_ERROR,
				       isc_msgcat, ISC_MSGSET_GENERAL,
				       ISC_MSG_FAILED, "dns_sdb_putrr");
			return ISC_R_FAILURE;
		}
		ret = c->c_get(c, &key, &data, DB_NEXT_DUP);
	}

	c->c_close(c);
	return ISC_R_SUCCESS;
}

static isc_result_t
bdb_allnodes(const char *zone, void *dbdata, dns_sdballnodes_t *n)
{
	int ret;
	char *type, *rdata;
	dns_ttl_t ttl;
	isc_consttextregion_t ttltext;
	DBC *c;
	DBT key, data;

	UNUSED(zone);

	if ((ret = ((DB *)dbdata)->cursor((DB *)dbdata, NULL, &c, 0)) != 0) {
		isc_log_iwrite(dns_lctx, DNS_LOGCATEGORY_DATABASE,
			       DNS_LOGMODULE_SDB, ISC_LOG_ERROR,
			       isc_msgcat, ISC_MSGSET_GENERAL,
			       ISC_MSG_FAILED, "DB->cursor: %s",
			       db_strerror(ret));
		return ISC_R_FAILURE;
	}

	memset(&key, 0, sizeof(DBT));
	memset(&data, 0, sizeof(DBT));

	while (c->c_get(c, &key, &data, DB_NEXT) == 0) {
		((char *)key.data)[key.size] = 0;
		((char *)data.data)[data.size] = 0;
		ttltext.base = strtok((char *)data.data, " ");
		ttltext.length = strlen(ttltext.base);
		dns_ttl_fromtext((isc_textregion_t *)&ttltext, &ttl);
		type = strtok(NULL, " ");
		rdata = type + strlen(type) + 1;

		if (dns_sdb_putnamedrr(n, key.data, type, ttl, rdata) !=
		    ISC_R_SUCCESS) {
			isc_log_iwrite(dns_lctx,
				       DNS_LOGCATEGORY_DATABASE,
				       DNS_LOGMODULE_SDB, ISC_LOG_ERROR,
				       isc_msgcat, ISC_MSGSET_GENERAL,
				       ISC_MSG_FAILED, "dns_sdb_putnamedrr");
			return ISC_R_FAILURE;
		}

	}

	c->c_close(c);
	return ISC_R_SUCCESS;
}

static isc_result_t
bdb_destroy(const char *zone, void *unused, void **dbdata)
{

	UNUSED(zone);
	UNUSED(unused);

	(*(DB **)dbdata)->close(*(DB **)dbdata, 0);

	return ISC_R_SUCCESS;
}

isc_result_t
bdb_init(void)
{
	static dns_sdbmethods_t bdb_methods = {
		bdb_lookup,
		NULL,
		bdb_allnodes,
		bdb_create,
		bdb_destroy
	};

	return dns_sdb_register(DRIVERNAME, &bdb_methods, NULL, 0, ns_g_mctx,
				&bdb_imp);
}

void
bdb_clear(void)
{

	if (bdb_imp != NULL)
		dns_sdb_unregister(&bdb_imp);
}
