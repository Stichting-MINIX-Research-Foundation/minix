/*
 * Copyright (C) 2004, 2007  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 2000, 2001  Internet Software Consortium.
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

/* $Id: dirdb.c,v 1.12 2007-06-19 23:47:07 tbox Exp $ */

/*
 * A simple database driver that returns basic information about
 * files and directories in the Unix file system as DNS data.
 */

#include <config.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>

#include <isc/mem.h>
#include <isc/print.h>
#include <isc/result.h>
#include <isc/util.h>

#include <dns/sdb.h>

#include <named/globals.h>

#include "dirdb.h"

static dns_sdbimplementation_t *dirdb = NULL;

#define CHECK(op)						\
	do { result = (op);					\
		if (result != ISC_R_SUCCESS) return (result);	\
	} while (0)

#define CHECKN(op)						\
	do { n = (op);						\
		if (n < 0) return (ISC_R_FAILURE);		\
	} while (0)


/*
 * This database operates on relative names.
 *
 * Any name will be interpreted as a pathname offset from the directory
 * specified in the configuration file.
 */
static isc_result_t
dirdb_lookup(const char *zone, const char *name, void *dbdata,
	      dns_sdblookup_t *lookup)
{
	char filename[255];
	char filename2[255];
	char buf[1024];
	struct stat statbuf;
	isc_result_t result;
	int n;

	UNUSED(zone);
	UNUSED(dbdata);

	if (strcmp(name, "@") == 0)
		snprintf(filename, sizeof(filename), "%s", (char *)dbdata);
	else
		snprintf(filename, sizeof(filename), "%s/%s",
			 (char *)dbdata, name);
	CHECKN(lstat(filename, &statbuf));
	
	if (S_ISDIR(statbuf.st_mode))
		CHECK(dns_sdb_putrr(lookup, "txt", 3600, "dir"));
	else if (S_ISCHR(statbuf.st_mode) || S_ISBLK(statbuf.st_mode)) {
		CHECKN(snprintf(buf, sizeof(buf),
				"\"%sdev\" \"major %d\" \"minor %d\"",
				S_ISCHR(statbuf.st_mode) ? "chr" : "blk",
				major(statbuf.st_rdev),
				minor(statbuf.st_rdev)));
		CHECK(dns_sdb_putrr(lookup, "txt", 3600, buf));
	} else if (S_ISFIFO(statbuf.st_mode))
		CHECK(dns_sdb_putrr(lookup, "txt", 3600, "pipe"));
	else if (S_ISSOCK(statbuf.st_mode))
		CHECK(dns_sdb_putrr(lookup, "txt", 3600, "socket"));
	else if (S_ISLNK(statbuf.st_mode)) {
		CHECKN(readlink(filename, filename2, sizeof(filename2) - 1));
		buf[n] = 0;
		CHECKN(snprintf(buf, sizeof(buf), "\"symlink\" \"%s\"",
				filename2));
		CHECK(dns_sdb_putrr(lookup, "txt", 3600, buf));
	} else if (!S_ISREG(statbuf.st_mode))
		CHECK(dns_sdb_putrr(lookup, "txt", 3600, "unknown"));
	else {
		CHECKN(snprintf(buf, sizeof(buf), "\"file\" \"size = %u\"",
			 (unsigned int)statbuf.st_size));
		CHECK(dns_sdb_putrr(lookup, "txt", 3600, buf));
	}

	return (ISC_R_SUCCESS);
}

/*
 * lookup () does not return SOA or NS records, so authority() must be defined.
 */
static isc_result_t
dirdb_authority(const char *zone, void *dbdata, dns_sdblookup_t *lookup) {
	isc_result_t result;

	UNUSED(zone);
	UNUSED(dbdata);

	result = dns_sdb_putsoa(lookup, "ns", "hostmaster", 0);
	INSIST(result == ISC_R_SUCCESS);
	result = dns_sdb_putrr(lookup, "ns", 86400, "ns1");
	INSIST(result == ISC_R_SUCCESS);
	result = dns_sdb_putrr(lookup, "ns", 86400, "ns2");
	INSIST(result == ISC_R_SUCCESS);
	return (ISC_R_SUCCESS);
}

/*
 * Each database stores the top-level directory as the dbdata opaque
 * object.  The create() function allocates it.  argv[0] holds the top
 * level directory.
 */
static isc_result_t
dirdb_create(const char *zone, int argc, char **argv,
	     void *driverdata, void **dbdata)
{
	UNUSED(zone);
	UNUSED(driverdata);

	if (argc < 1)
		return (ISC_R_FAILURE);
	*dbdata = isc_mem_strdup((isc_mem_t *)driverdata, argv[0]);
	if (*dbdata == NULL)
		return (ISC_R_NOMEMORY);
	return (ISC_R_SUCCESS);
}

/*
 * The destroy() function frees the memory allocated by create().
 */
static void
dirdb_destroy(const char *zone, void *driverdata, void **dbdata) {
	UNUSED(zone);
	UNUSED(driverdata);
	isc_mem_free((isc_mem_t *)driverdata, *dbdata);
}

/*
 * This zone does not support zone transfer, so allnodes() is NULL.
 */
static dns_sdbmethods_t dirdb_methods = {
	dirdb_lookup,
	dirdb_authority,
	NULL, /* allnodes */
	dirdb_create,
	dirdb_destroy
};

/*
 * Wrapper around dns_sdb_register().  Note that the first ns_g_mctx is
 * being passed as the "driverdata" parameter, so that will it will be
 * passed to create() and destroy().
 */
isc_result_t
dirdb_init(void) {
	unsigned int flags;
	flags = DNS_SDBFLAG_RELATIVEOWNER | DNS_SDBFLAG_RELATIVERDATA |
		DNS_SDBFLAG_THREADSAFE;
	return (dns_sdb_register("dir", &dirdb_methods, ns_g_mctx, flags,
				 ns_g_mctx, &dirdb));
}

/*
 * Wrapper around dns_sdb_unregister().
 */
void
dirdb_clear(void) {
	if (dirdb != NULL)
		dns_sdb_unregister(&dirdb);
}
