/*
 * Copyright (C) 2004, 2005, 2007-2009  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 2000-2002  Internet Software Consortium.
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

/* $Id: zonetodb.c,v 1.23 2009-09-02 23:48:01 tbox Exp $ */

#include <stdlib.h>
#include <string.h>

#include <isc/buffer.h>
#include <isc/entropy.h>
#include <isc/hash.h>
#include <isc/mem.h>
#include <isc/print.h>
#include <isc/result.h>

#include <dns/db.h>
#include <dns/dbiterator.h>
#include <dns/fixedname.h>
#include <dns/name.h>
#include <dns/rdata.h>
#include <dns/rdataset.h>
#include <dns/rdatasetiter.h>
#include <dns/rdatatype.h>
#include <dns/result.h>

#include <pgsql/libpq-fe.h>

/*
 * Generate a PostgreSQL table from a zone.
 *
 * This is compiled this with something like the following (assuming bind9 has
 * been installed):
 *
 * gcc -g `isc-config.sh --cflags isc dns` -c zonetodb.c
 * gcc -g -o zonetodb zonetodb.o `isc-config.sh --libs isc dns` -lpq
 */

PGconn *conn = NULL;
char *dbname, *dbtable;
char str[10240];

void
closeandexit(int status) {
	if (conn != NULL)
		PQfinish(conn);
	exit(status);
}

void
check_result(isc_result_t result, const char *message) {
	if (result != ISC_R_SUCCESS) {
		fprintf(stderr, "%s: %s\n", message,
			isc_result_totext(result));
		closeandexit(1);
	}
}

/*
 * Canonicalize a string before writing it to the database.
 * "dest" must be an array of at least size 2*strlen(source) + 1.
 */
static void
quotestring(const char *source, char *dest) {
	while (*source != 0) {
		if (*source == '\'')
			*dest++ = '\'';
		else if (*source == '\\')
			*dest++ = '\\';
		*dest++ = *source++;
	}
	*dest++ = 0;
}

void
addrdata(dns_name_t *name, dns_ttl_t ttl, dns_rdata_t *rdata) {
	unsigned char namearray[DNS_NAME_MAXTEXT + 1];
	unsigned char canonnamearray[2 * DNS_NAME_MAXTEXT + 1];
	unsigned char typearray[20];
	unsigned char canontypearray[40];
	unsigned char dataarray[2048];
	unsigned char canondataarray[4096];
	isc_buffer_t b;
	isc_result_t result;
	PGresult *res;

	isc_buffer_init(&b, namearray, sizeof(namearray) - 1);
	result = dns_name_totext(name, ISC_TRUE, &b);
	check_result(result, "dns_name_totext");
	namearray[isc_buffer_usedlength(&b)] = 0;
	quotestring(namearray, canonnamearray);

	isc_buffer_init(&b, typearray, sizeof(typearray) - 1);
	result = dns_rdatatype_totext(rdata->type, &b);
	check_result(result, "dns_rdatatype_totext");
	typearray[isc_buffer_usedlength(&b)] = 0;
	quotestring(typearray, canontypearray);

	isc_buffer_init(&b, dataarray, sizeof(dataarray) - 1);
	result = dns_rdata_totext(rdata, NULL, &b);
	check_result(result, "dns_rdata_totext");
	dataarray[isc_buffer_usedlength(&b)] = 0;
	quotestring(dataarray, canondataarray);

	snprintf(str, sizeof(str),
		 "INSERT INTO %s (NAME, TTL, RDTYPE, RDATA)"
		 " VALUES ('%s', %d, '%s', '%s')",
		 dbtable, canonnamearray, ttl, canontypearray, canondataarray);
	printf("%s\n", str);
	res = PQexec(conn, str);
	if (!res || PQresultStatus(res) != PGRES_COMMAND_OK) {
		fprintf(stderr, "INSERT INTO command failed: %s\n",
			PQresultErrorMessage(res));
		PQclear(res);
		closeandexit(1);
	}
	PQclear(res);
}

int
main(int argc, char **argv) {
	char *porigin, *zonefile;
	dns_fixedname_t forigin, fname;
	dns_name_t *origin, *name;
	dns_db_t *db = NULL;
	dns_dbiterator_t *dbiter;
	dns_dbnode_t *node;
	dns_rdatasetiter_t *rdsiter;
	dns_rdataset_t rdataset;
	dns_rdata_t rdata = DNS_RDATA_INIT;
	isc_mem_t *mctx = NULL;
	isc_entropy_t *ectx = NULL;
	isc_buffer_t b;
	isc_result_t result;
	PGresult *res;

	if (argc != 5) {
		printf("usage: %s origin file dbname dbtable\n", argv[0]);
		printf("Note that dbname must be an existing database.\n");
		exit(1);
	}

	porigin = argv[1];
	zonefile = argv[2];
	dbname = argv[3];
	dbtable = argv[4];

	dns_result_register();

	mctx = NULL;
	result = isc_mem_create(0, 0, &mctx);
	check_result(result, "isc_mem_create");

	result = isc_entropy_create(mctx, &ectx);
	check_result(result, "isc_entropy_create");

	result = isc_hash_create(mctx, ectx, DNS_NAME_MAXWIRE);
	check_result(result, "isc_hash_create");

	isc_buffer_init(&b, porigin, strlen(porigin));
	isc_buffer_add(&b, strlen(porigin));
	dns_fixedname_init(&forigin);
	origin = dns_fixedname_name(&forigin);
	result = dns_name_fromtext(origin, &b, dns_rootname, 0, NULL);
	check_result(result, "dns_name_fromtext");

	db = NULL;
	result = dns_db_create(mctx, "rbt", origin, dns_dbtype_zone,
			       dns_rdataclass_in, 0, NULL, &db);
	check_result(result, "dns_db_create");

	result = dns_db_load(db, zonefile);
	if (result == DNS_R_SEENINCLUDE)
		result = ISC_R_SUCCESS;
	check_result(result, "dns_db_load");

	printf("Connecting to '%s'\n", dbname);
	conn = PQsetdb(NULL, NULL, NULL, NULL, dbname);
	if (PQstatus(conn) == CONNECTION_BAD) {
		fprintf(stderr, "Connection to database '%s' failed: %s\n",
			dbname, PQerrorMessage(conn));
		closeandexit(1);
	}

	snprintf(str, sizeof(str),
		 "DROP TABLE %s", dbtable);
	printf("%s\n", str);
	res = PQexec(conn, str);
	if (!res || PQresultStatus(res) != PGRES_COMMAND_OK)
		fprintf(stderr, "DROP TABLE command failed: %s\n",
			PQresultErrorMessage(res));
	PQclear(res);

	snprintf(str, sizeof(str), "BEGIN");
	printf("%s\n", str);
	res = PQexec(conn, str);
	if (!res || PQresultStatus(res) != PGRES_COMMAND_OK) {
		fprintf(stderr, "BEGIN command failed: %s\n",
			PQresultErrorMessage(res));
		PQclear(res);
		closeandexit(1);
	}
	PQclear(res);

	snprintf(str, sizeof(str),
		 "CREATE TABLE %s "
		 "(NAME TEXT, TTL INTEGER, RDTYPE TEXT, RDATA TEXT)",
		 dbtable);
	printf("%s\n", str);
	res = PQexec(conn, str);
	if (!res || PQresultStatus(res) != PGRES_COMMAND_OK) {
		fprintf(stderr, "CREATE TABLE command failed: %s\n",
			PQresultErrorMessage(res));
		PQclear(res);
		closeandexit(1);
	}
	PQclear(res);

	dbiter = NULL;
	result = dns_db_createiterator(db, 0, &dbiter);
	check_result(result, "dns_db_createiterator()");

	result = dns_dbiterator_first(dbiter);
	check_result(result, "dns_dbiterator_first");

	dns_fixedname_init(&fname);
	name = dns_fixedname_name(&fname);
	dns_rdataset_init(&rdataset);
	dns_rdata_init(&rdata);

	while (result == ISC_R_SUCCESS) {
		node = NULL;
		result = dns_dbiterator_current(dbiter, &node, name);
		if (result == ISC_R_NOMORE)
			break;
		check_result(result, "dns_dbiterator_current");

		rdsiter = NULL;
		result = dns_db_allrdatasets(db, node, NULL, 0, &rdsiter);
		check_result(result, "dns_db_allrdatasets");

		result = dns_rdatasetiter_first(rdsiter);

		while (result == ISC_R_SUCCESS) {
			dns_rdatasetiter_current(rdsiter, &rdataset);
			result = dns_rdataset_first(&rdataset);
			check_result(result, "dns_rdataset_first");
			while (result == ISC_R_SUCCESS) {
				dns_rdataset_current(&rdataset, &rdata);
				addrdata(name, rdataset.ttl, &rdata);
				dns_rdata_reset(&rdata);
				result = dns_rdataset_next(&rdataset);
			}
			dns_rdataset_disassociate(&rdataset);
			result = dns_rdatasetiter_next(rdsiter);
		}
		dns_rdatasetiter_destroy(&rdsiter);
		dns_db_detachnode(db, &node);
		result = dns_dbiterator_next(dbiter);
	}

	snprintf(str, sizeof(str), "COMMIT TRANSACTION");
	printf("%s\n", str);
	res = PQexec(conn, str);
	if (!res || PQresultStatus(res) != PGRES_COMMAND_OK) {
		fprintf(stderr, "COMMIT command failed: %s\n",
			PQresultErrorMessage(res));
		PQclear(res);
		closeandexit(1);
	}
	PQclear(res);
	dns_dbiterator_destroy(&dbiter);
	dns_db_detach(&db);
	isc_hash_destroy();
	isc_entropy_detach(&ectx);
	isc_mem_destroy(&mctx);
	closeandexit(0);
	exit(0);
}
