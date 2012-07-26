/*
 * Copyright (C) 2004, 2007, 2009  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 1999-2001  Internet Software Consortium.
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

/* $Id: master_test.c,v 1.32 2009-09-02 23:48:01 tbox Exp $ */

#include <config.h>

#include <stdlib.h>
#include <string.h>

#include <isc/buffer.h>
#include <isc/mem.h>
#include <isc/util.h>

#include <dns/callbacks.h>
#include <dns/master.h>
#include <dns/name.h>
#include <dns/rdataset.h>
#include <dns/result.h>

isc_mem_t *mctx;

static isc_result_t
print_dataset(void *arg, dns_name_t *owner, dns_rdataset_t *dataset) {
	char buf[64*1024];
	isc_buffer_t target;
	isc_result_t result;

	UNUSED(arg);

	isc_buffer_init(&target, buf, 64*1024);
	result = dns_rdataset_totext(dataset, owner, ISC_FALSE, ISC_FALSE,
				     &target);
	if (result == ISC_R_SUCCESS)
		fprintf(stdout, "%.*s\n", (int)target.used,
					  (char*)target.base);
	else
		fprintf(stdout, "dns_rdataset_totext: %s\n",
			dns_result_totext(result));

	return (ISC_R_SUCCESS);
}

int
main(int argc, char *argv[]) {
	isc_result_t result;
	dns_name_t origin;
	isc_buffer_t source;
	isc_buffer_t target;
	unsigned char name_buf[255];
	dns_rdatacallbacks_t callbacks;

	UNUSED(argc);

	RUNTIME_CHECK(isc_mem_create(0, 0, &mctx) == ISC_R_SUCCESS);

	if (argv[1]) {
		isc_buffer_init(&source, argv[1], strlen(argv[1]));
		isc_buffer_add(&source, strlen(argv[1]));
		isc_buffer_setactive(&source, strlen(argv[1]));
		isc_buffer_init(&target, name_buf, 255);
		dns_name_init(&origin, NULL);
		result = dns_name_fromtext(&origin, &source, dns_rootname,
					   0, &target);
		if (result != ISC_R_SUCCESS) {
			fprintf(stdout, "dns_name_fromtext: %s\n",
				dns_result_totext(result));
			exit(1);
		}

		dns_rdatacallbacks_init_stdio(&callbacks);
		callbacks.add = print_dataset;

		result = dns_master_loadfile(argv[1], &origin, &origin,
					     dns_rdataclass_in, 0,
					     &callbacks, mctx);
		fprintf(stdout, "dns_master_loadfile: %s\n",
			dns_result_totext(result));
	}
	return (0);
}
