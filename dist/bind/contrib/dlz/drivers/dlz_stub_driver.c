/*
 * Copyright (C) 2002 Stichting NLnet, Netherlands, stichting@nlnet.nl.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND STICHTING NLNET
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
 * STICHTING NLNET BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE
 * USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * The development of Dynamically Loadable Zones (DLZ) for Bind 9 was
 * conceived and contributed by Rob Butler.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ROB BUTLER
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
 * ROB BUTLER BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE
 * USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Copyright (C) 1999-2001  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
 * INTERNET SOFTWARE CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
 * FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifdef DLZ_STUB

#include <config.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <dns/log.h>
#include <dns/sdlz.h>
#include <dns/result.h>

#include <isc/mem.h>
#include <isc/print.h>
#include <isc/result.h>
#include <isc/util.h>

#include <named/globals.h>

#include <dlz/dlz_stub_driver.h>

static dns_sdlzimplementation_t *dlz_stub = NULL;

typedef struct config_data {
	char		*myzone;
	char		*myname;
	char		*myip;
	isc_mem_t	*mctx;
} config_data_t;

/*
 * SDLZ methods
 */

static isc_result_t
stub_dlz_allnodes(const char *zone, void *driverarg, void *dbdata,
		  dns_sdlzallnodes_t *allnodes)
{
	config_data_t *cd;
	isc_result_t result;

	UNUSED(zone);
	UNUSED(driverarg);

	cd = (config_data_t *) dbdata;

	result = dns_sdlz_putnamedrr(allnodes, cd->myname, "soa", 86400,
				     "web root.localhost. "
				     "0 28800 7200 604800 86400");
	if (result != ISC_R_SUCCESS)
		return (ISC_R_FAILURE);
	result = dns_sdlz_putnamedrr(allnodes, "ns", "ns", 86400, cd->myname);
	if (result != ISC_R_SUCCESS)
		return (ISC_R_FAILURE);
	result = dns_sdlz_putnamedrr(allnodes, cd->myname, "a", 1, cd->myip);
	if (result != ISC_R_SUCCESS)
		return (ISC_R_FAILURE);
	return (ISC_R_SUCCESS);
}

static isc_result_t
stub_dlz_allowzonexfr(void *driverarg, void *dbdata, const char *name,
		      const char *client)
{
	UNUSED(driverarg);
	UNUSED(dbdata);
	UNUSED(name);
	UNUSED(client);
	return ISC_R_SUCCESS;
}

static isc_result_t
stub_dlz_authority(const char *zone, void *driverarg, void *dbdata,
		   dns_sdlzlookup_t *lookup)
{
	isc_result_t result;
	config_data_t *cd;

	UNUSED(driverarg);

	cd = (config_data_t *) dbdata;

	if (strcmp(zone, cd->myzone) == 0) {
		result = dns_sdlz_putsoa(lookup, cd->myname,
					 "root.localhost.", 0);
		if (result != ISC_R_SUCCESS)
			return (ISC_R_FAILURE);

		result = dns_sdlz_putrr(lookup, "ns", 86400, cd->myname);
		if (result != ISC_R_SUCCESS)
			return (ISC_R_FAILURE);

		return (ISC_R_SUCCESS);
	}
	return (ISC_R_NOTFOUND);
}

static isc_result_t
stub_dlz_findzonedb(void *driverarg, void *dbdata, const char *name)
{

	config_data_t *cd;

	UNUSED(driverarg);

	cd = (config_data_t *) dbdata;

	/* Write info message to log */
	isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
		      DNS_LOGMODULE_DLZ, ISC_LOG_DEBUG(2),
		      "dlz_stub findzone looking for '%s'", name);

	if (strcmp(cd->myzone, name) == 0)
		return (ISC_R_SUCCESS);
	else
		return (ISC_R_NOTFOUND);
}


static isc_result_t
stub_dlz_lookup(const char *zone, const char *name, void *driverarg,
		void *dbdata, dns_sdlzlookup_t *lookup)
{
	isc_result_t result;
	config_data_t *cd;

	UNUSED(zone);
	UNUSED(driverarg);

	cd = (config_data_t *) dbdata;

	if (strcmp(name, cd->myname) == 0) {
		result = dns_sdlz_putrr(lookup, "a", 1, cd->myip);
		if (result != ISC_R_SUCCESS)
			return (ISC_R_FAILURE);

		return (ISC_R_SUCCESS);
	}
	return (ISC_R_FAILURE);

}


static isc_result_t
stub_dlz_create(const char *dlzname, unsigned int argc, char *argv[],
		void *driverarg, void **dbdata)
{

	config_data_t *cd;

	UNUSED(driverarg);

	if (argc < 4)
		return (ISC_R_FAILURE);
	/*
	 * Write info message to log
	 */
	isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
		      DNS_LOGMODULE_DLZ, ISC_LOG_INFO,
		      "Loading '%s' using DLZ_stub driver. "
		      "Zone: %s, Name: %s IP: %s",
		      dlzname, argv[1], argv[2], argv[3]);

	cd = isc_mem_get(ns_g_mctx, sizeof(config_data_t));
	if ((cd) == NULL) {
		return (ISC_R_NOMEMORY);
	}

	memset(cd, 0, sizeof(config_data_t));

	cd->myzone = isc_mem_strdup(ns_g_mctx, argv[1]);
	if (cd->myzone == NULL) {
		isc_mem_put(ns_g_mctx, cd, sizeof(config_data_t));
		return (ISC_R_NOMEMORY);
	}

	cd->myname = isc_mem_strdup(ns_g_mctx, argv[2]);
	if (cd->myname == NULL) {
		isc_mem_put(ns_g_mctx, cd, sizeof(config_data_t));
		isc_mem_free(ns_g_mctx, cd->myzone);
		return (ISC_R_NOMEMORY);
	}

	cd->myip = isc_mem_strdup(ns_g_mctx, argv[3]);
	if (cd->myip == NULL) {
		isc_mem_put(ns_g_mctx, cd, sizeof(config_data_t));
		isc_mem_free(ns_g_mctx, cd->myname);
		isc_mem_free(ns_g_mctx, cd->myzone);
		return (ISC_R_NOMEMORY);
	}

	isc_mem_attach(ns_g_mctx, &cd->mctx);

	*dbdata = cd;

	return(ISC_R_SUCCESS);
}

static void
stub_dlz_destroy(void *driverarg, void *dbdata)
{
	config_data_t *cd;
	isc_mem_t *mctx;

	UNUSED(driverarg);

	cd = (config_data_t *) dbdata;

	/*
	 * Write debugging message to log
	 */
	isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
		      DNS_LOGMODULE_DLZ, ISC_LOG_DEBUG(2),
		      "Unloading DLZ_stub driver.");

	isc_mem_free(ns_g_mctx, cd->myzone);
	isc_mem_free(ns_g_mctx, cd->myname);
	isc_mem_free(ns_g_mctx, cd->myip);
	mctx = cd->mctx;
	isc_mem_put(mctx, cd, sizeof(config_data_t));
	isc_mem_detach(&mctx);
}

static dns_sdlzmethods_t dlz_stub_methods = {
	stub_dlz_create,
	stub_dlz_destroy,
	stub_dlz_findzonedb,
	stub_dlz_lookup,
	stub_dlz_authority,
	stub_dlz_allnodes,
	stub_dlz_allowzonexfr,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
};

/*%
 * Wrapper around dns_sdlzregister().
 */
isc_result_t
dlz_stub_init(void) {
	isc_result_t result;

	/*
	 * Write debugging message to log
	 */
	isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
		      DNS_LOGMODULE_DLZ, ISC_LOG_DEBUG(2),
		      "Registering DLZ_stub driver.");

	result = dns_sdlzregister("dlz_stub", &dlz_stub_methods, NULL,
				  DNS_SDLZFLAG_RELATIVEOWNER |
				  DNS_SDLZFLAG_RELATIVERDATA,
				  ns_g_mctx, &dlz_stub);
	if (result != ISC_R_SUCCESS) {
		UNEXPECTED_ERROR(__FILE__, __LINE__,
				 "dns_sdlzregister() failed: %s",
				 isc_result_totext(result));
		result = ISC_R_UNEXPECTED;
	}


	return result;
}

/*
 * Wrapper around dns_sdlzunregister().
 */
void
dlz_stub_clear(void) {

	/*
	 * Write debugging message to log
	 */
	isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
		      DNS_LOGMODULE_DLZ, ISC_LOG_DEBUG(2),
		      "Unregistering DLZ_stub driver.");

	if (dlz_stub != NULL)
		dns_sdlzunregister(&dlz_stub);
}

#endif
