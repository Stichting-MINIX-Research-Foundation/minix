/*
 * Copyright (C) 2005  Internet Systems Consortium, Inc. ("ISC")
 *
 * Permission to use, copy, modify, and distribute this software for any
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

/* $Id: dlz_drivers.c,v 1.3.16.1 2011-03-10 04:29:17 each Exp $ */

/*! \file */

#include <config.h>

#include <isc/result.h>

/*
 * Pull in declarations for this module's functions.
 */

#include <dlz/dlz_drivers.h>

/*
 * Pull in driver-specific stuff.
 */

#ifdef DLZ_STUB
#include <dlz/dlz_stub_driver.h>
#endif

#ifdef DLZ_POSTGRES
#include <dlz/dlz_postgres_driver.h>
#endif

#ifdef DLZ_MYSQL
#include <dlz/dlz_mysql_driver.h>
#endif

#ifdef DLZ_FILESYSTEM
#include <dlz/dlz_filesystem_driver.h>
#endif

#ifdef DLZ_BDB
#include <dlz/dlz_bdb_driver.h>
#include <dlz/dlz_bdbhpt_driver.h>
#endif

#ifdef DLZ_LDAP
#include <dlz/dlz_ldap_driver.h>
#endif

#ifdef DLZ_ODBC
#include <dlz/dlz_odbc_driver.h>
#endif

/*%
 * Call init functions for all relevant DLZ drivers.
 */

isc_result_t
dlz_drivers_init(void) {

	isc_result_t result = ISC_R_SUCCESS;

#ifdef DLZ_STUB
	result = dlz_stub_init();
	if (result != ISC_R_SUCCESS)
		return (result);
#endif

#ifdef DLZ_POSTGRES
	result = dlz_postgres_init();
	if (result != ISC_R_SUCCESS)
		return (result);
#endif

#ifdef DLZ_MYSQL
	result = dlz_mysql_init();
	if (result != ISC_R_SUCCESS)
		return (result);
#endif

#ifdef DLZ_FILESYSTEM
	result = dlz_fs_init();
	if (result != ISC_R_SUCCESS)
		return (result);
#endif

#ifdef DLZ_BDB
	result = dlz_bdb_init();
	if (result != ISC_R_SUCCESS)
		return (result);
	result = dlz_bdbhpt_init();
	if (result != ISC_R_SUCCESS)
		return (result);
#endif

#ifdef DLZ_LDAP
	result = dlz_ldap_init();
	if (result != ISC_R_SUCCESS)
		return (result);
#endif

#ifdef DLZ_ODBC
	result = dlz_odbc_init();
	if (result != ISC_R_SUCCESS)
		return (result);
#endif

	return (result);
}

/*%
 * Call shutdown functions for all relevant DLZ drivers.
 */

void
dlz_drivers_clear(void) {

#ifdef DLZ_STUB
	dlz_stub_clear();
#endif

#ifdef DLZ_POSTGRES
        dlz_postgres_clear();
#endif

#ifdef DLZ_MYSQL
 	dlz_mysql_clear();
#endif

#ifdef DLZ_FILESYSTEM
        dlz_fs_clear();
#endif

#ifdef DLZ_BDB
        dlz_bdb_clear();
        dlz_bdbhpt_clear();
#endif

#ifdef DLZ_LDAP
        dlz_ldap_clear();
#endif

#ifdef DLZ_ODBC
        dlz_odbc_clear();
#endif

}
