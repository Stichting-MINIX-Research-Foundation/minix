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

/* $Id: tcldb.c,v 1.10 2007-06-19 23:47:10 tbox Exp $ */

/*
 * A simple database driver that calls a Tcl procedure to define
 * the contents of the DNS namespace.  The procedure is loaded
 * from the file lookup.tcl; look at the comments there for
 * more information.
 */

#include <config.h>

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

#include <isc/mem.h>
#include <isc/print.h>
#include <isc/result.h>
#include <isc/util.h>

#include <dns/log.h>
#include <dns/sdb.h>

#include <named/globals.h>

#include <tcl.h>

#include <tcldb.h>

#define CHECK(op)						\
	do { result = (op);					\
		if (result != ISC_R_SUCCESS) return (result);	\
	} while (0)

typedef struct tcldb_driver {
	isc_mem_t *mctx;
	Tcl_Interp *interp;
} tcldb_driver_t;

static tcldb_driver_t *the_driver = NULL;

static dns_sdbimplementation_t *tcldb = NULL;

static isc_result_t
tcldb_driver_create(isc_mem_t *mctx, tcldb_driver_t **driverp) {
	int tclres;
	isc_result_t result = ISC_R_SUCCESS;
	tcldb_driver_t *driver = isc_mem_get(mctx, sizeof(tcldb_driver_t));
	if (driver == NULL)
		return (ISC_R_NOMEMORY);
	driver->mctx = mctx;
	driver->interp = Tcl_CreateInterp();

	tclres = Tcl_EvalFile(driver->interp, (char *) "lookup.tcl");
	if (tclres != TCL_OK) {
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_GENERAL,
			      DNS_LOGMODULE_SDB, ISC_LOG_ERROR,
			      "initializing tcldb: "
			      "loading 'lookup.tcl' failed: %s",
			      driver->interp->result);
		result = ISC_R_FAILURE;
		goto cleanup;
	}
	*driverp = driver;
	return (ISC_R_SUCCESS);

 cleanup:
	isc_mem_put(mctx, driver, sizeof(tcldb_driver_t));
	return (result);
	
}

static void
tcldb_driver_destroy(tcldb_driver_t **driverp) {
	tcldb_driver_t *driver = *driverp;
	Tcl_DeleteInterp(driver->interp);
	isc_mem_put(driver->mctx, driver, sizeof(tcldb_driver_t));
}

/*
 * Perform a lookup, by invoking the Tcl procedure "lookup".
 */
static isc_result_t
tcldb_lookup(const char *zone, const char *name, void *dbdata,
	      dns_sdblookup_t *lookup)
{
	isc_result_t result = ISC_R_SUCCESS;
	int tclres;
	int rrc;	/* RR count */
	char **rrv;	/* RR vector */
	int i;
	char *cmdv[3];
	char *cmd;

	tcldb_driver_t *driver = (tcldb_driver_t *) dbdata;

	cmdv[0] = "lookup";
	cmdv[1] = zone;
	cmdv[2] = name;
	cmd = Tcl_Merge(3, cmdv);
	tclres = Tcl_Eval(driver->interp, cmd);
	Tcl_Free(cmd);

	if (tclres != TCL_OK) {
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_GENERAL,
			      DNS_LOGMODULE_SDB, ISC_LOG_ERROR,
			      "zone '%s': tcl lookup function failed: %s",
			      zone, driver->interp->result);
		return (ISC_R_FAILURE);
	}

	if (strcmp(driver->interp->result, "NXDOMAIN") == 0) {
		result = ISC_R_NOTFOUND;
		goto fail;
	}

	tclres = Tcl_SplitList(driver->interp, driver->interp->result,
			       &rrc, &rrv);
	if (tclres != TCL_OK)
		goto malformed;

	for (i = 0; i < rrc; i++) {
		isc_result_t tmpres;
		int fieldc;	/* Field count */
		char **fieldv;	/* Field vector */
		tclres = Tcl_SplitList(driver->interp, rrv[i],
				       &fieldc, &fieldv);
		if (tclres != TCL_OK) {
			tmpres = ISC_R_FAILURE;
			goto failrr;
		}
		if (fieldc != 3)
			goto malformed;
		tmpres = dns_sdb_putrr(lookup, fieldv[0], atoi(fieldv[1]),
				       fieldv[2]);
		Tcl_Free((char *) fieldv);
	failrr:
		if (tmpres != ISC_R_SUCCESS)
			result = tmpres;
	}
	Tcl_Free((char *) rrv);
	if (result == ISC_R_SUCCESS)
		return (result);

 malformed:
	isc_log_write(dns_lctx, DNS_LOGCATEGORY_GENERAL,
		      DNS_LOGMODULE_SDB, ISC_LOG_ERROR,
		      "zone '%s': "
		      "malformed return value from tcl lookup function: %s",
		      zone, driver->interp->result);
	result = ISC_R_FAILURE;
 fail:
	return (result);
}

/*
 * Set up per-zone state.  In our case, the database arguments of the
 * zone are collected into a Tcl list and assigned to an element of
 * the global array "dbargs".
 */
static isc_result_t
tcldb_create(const char *zone, int argc, char **argv,
	     void *driverdata, void **dbdata)
{
	tcldb_driver_t *driver = (tcldb_driver_t *) driverdata;

	char *list = Tcl_Merge(argc, argv);
	
	Tcl_SetVar2(driver->interp, (char *) "dbargs", (char *) zone, list, 0);

	Tcl_Free(list);

	*dbdata = driverdata;
	
	return (ISC_R_SUCCESS);
}

/*
 * This driver does not support zone transfer, so allnodes() is NULL.
 */
static dns_sdbmethods_t tcldb_methods = {
	tcldb_lookup,
	NULL, /* authority */
	NULL, /* allnodes */
	tcldb_create,
	NULL /* destroy */
};

/*
 * Initialize the tcldb driver.
 */
isc_result_t
tcldb_init(void) {
	isc_result_t result;
	int flags = DNS_SDBFLAG_RELATIVEOWNER | DNS_SDBFLAG_RELATIVERDATA;
	
	result = tcldb_driver_create(ns_g_mctx, &the_driver);
	if (result != ISC_R_SUCCESS)
		return (result);
	
	return (dns_sdb_register("tcl", &tcldb_methods, the_driver, flags,
				 ns_g_mctx, &tcldb));
}

/*
 * Wrapper around dns_sdb_unregister().
 */
void
tcldb_clear(void) {
	if (tcldb != NULL)
		dns_sdb_unregister(&tcldb);
	if (the_driver != NULL)
		tcldb_driver_destroy(&the_driver);
}
