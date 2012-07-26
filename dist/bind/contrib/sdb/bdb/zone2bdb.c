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

/* $Id: zone2bdb.c,v 1.3 2009-09-01 00:22:26 jinmei Exp $ */

#include <stdio.h>

#include <isc/mem.h>
#include <isc/result.h>
#include <isc/types.h>
#include <isc/util.h>

#include <dns/db.h>
#include <dns/dbiterator.h>
#include <dns/fixedname.h>
#include <dns/name.h>
#include <dns/rdata.h>
#include <dns/rdataset.h>
#include <dns/rdatasetiter.h>
#include <dns/rdatatype.h>
#include <dns/ttl.h>
#include <dns/types.h>

#include <db.h>

#define MAX_RDATATEXT	63 + 4 + 65535 + 2	/* ttl + type + rdata + sep */

/*
 * Returns a valid 'DB' handle.
 *
 * Requires:
 *	'file' is a valid non-existant path.
 */
DB *
bdb_init(const char *file)
{
	DB *db;

	REQUIRE(db_create(&db, NULL, 0) == 0);
	REQUIRE(db->set_flags(db, DB_DUP) == 0);
	REQUIRE(db->open(db, file, NULL, DB_HASH, DB_CREATE | DB_EXCL, 0) == 0);

	return db;
}

/*
 * Puts resource record data on 'db'.
 */
isc_result_t
bdb_putrdata(DB *db, dns_name_t *name, dns_ttl_t ttl, dns_rdata_t *rdata)
{
	static DBT key, data;
	isc_buffer_t keybuf, databuf;
	char nametext[DNS_NAME_MAXTEXT];
	char rdatatext[MAX_RDATATEXT];

	isc_buffer_init(&keybuf, nametext, DNS_NAME_MAXTEXT);

	dns_name_totext(name, ISC_TRUE, &keybuf);

	key.data = isc_buffer_base(&keybuf);
	key.size = isc_buffer_usedlength(&keybuf);

	isc_buffer_init(&databuf, rdatatext, MAX_RDATATEXT);

	dns_ttl_totext(ttl, ISC_FALSE, &databuf);
	*(char *)isc_buffer_used(&databuf) = ' ';
	isc_buffer_add(&databuf, 1);

	dns_rdatatype_totext(rdata->type, &databuf);	/* XXX private data */
	*(char *)isc_buffer_used(&databuf) = ' ';
	isc_buffer_add(&databuf, 1);

	dns_rdata_totext(rdata, NULL, &databuf);

	data.data = isc_buffer_base(&databuf);
	data.size = isc_buffer_usedlength(&databuf);

	REQUIRE(db->put(db, NULL, &key, &data, 0) == 0);

	return ISC_R_SUCCESS;
}

isc_result_t
bdb_destroy(DB *db)
{

	return (db->close(db, 0) == 0) ? ISC_R_SUCCESS : ISC_R_FAILURE;
}

void
usage(const char *prog)
{

	fprintf(stderr, "Usage: %s <origin> <zonefile> <db>\n", prog);
	exit(1);
}

int
main(int argc, char *argv[])
{
	isc_mem_t *mctx = NULL;
	isc_buffer_t b;
	int n;
	dns_fixedname_t origin, name;
	dns_db_t *db = NULL;
	dns_dbiterator_t *dbiter = NULL;
	isc_result_t res;
	dns_dbnode_t *node = NULL;
	dns_rdataset_t rdataset;
	dns_rdatasetiter_t *rdatasetiter = NULL;
	dns_rdata_t rdata;
	DB *bdb;

	if (argc != 4) usage(*argv);

	REQUIRE(isc_mem_create(0, 0, &mctx) == ISC_R_SUCCESS);

	n = strlen(argv[1]);
	isc_buffer_init(&b, argv[1], n);
	isc_buffer_add(&b, n);

	dns_fixedname_init(&origin);

	REQUIRE(dns_name_fromtext(dns_fixedname_name(&origin), &b, dns_rootname,
				  0, NULL) == ISC_R_SUCCESS);
	REQUIRE(dns_db_create(mctx, "rbt", dns_fixedname_name(&origin),
			      dns_dbtype_zone, dns_rdataclass_in, 0, NULL,
			      &db) == ISC_R_SUCCESS);

	REQUIRE(dns_db_load(db, argv[2]) == ISC_R_SUCCESS);

	REQUIRE(dns_db_createiterator(db, 0, &dbiter) == ISC_R_SUCCESS);

	dns_rdataset_init(&rdataset);
	dns_rdata_init(&rdata);
	dns_fixedname_init(&name);
	bdb = bdb_init(argv[3]);

	for (res = dns_dbiterator_first(dbiter); res == ISC_R_SUCCESS;
	    res = dns_dbiterator_next(dbiter)) {
		dns_dbiterator_current(dbiter, &node, dns_fixedname_name(&name));
		REQUIRE(dns_db_allrdatasets(db, node, NULL, 0, &rdatasetiter)
		    == ISC_R_SUCCESS);

		for (res = dns_rdatasetiter_first(rdatasetiter);
		    res == ISC_R_SUCCESS;
		    res = dns_rdatasetiter_next(rdatasetiter)) {
			dns_rdatasetiter_current(rdatasetiter, &rdataset);

			res = dns_rdataset_first(&rdataset);
			while (res == ISC_R_SUCCESS) {
				dns_rdataset_current(&rdataset, &rdata);
				REQUIRE(bdb_putrdata(bdb,
						     dns_fixedname_name(&name),
						     rdataset.ttl, &rdata)
					== ISC_R_SUCCESS);

				dns_rdata_reset(&rdata);
				res = dns_rdataset_next(&rdataset);
			}

			dns_rdataset_disassociate(&rdataset);
		}
		dns_rdatasetiter_destroy(&rdatasetiter);
		dns_db_detachnode(db, &node);
	}
	dns_dbiterator_destroy(&dbiter);

	REQUIRE(bdb_destroy(bdb) == ISC_R_SUCCESS);

	return 0;
}
