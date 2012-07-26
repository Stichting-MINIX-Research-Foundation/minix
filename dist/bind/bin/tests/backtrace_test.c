/*
 * Copyright (C) 2009  Internet Systems Consortium, Inc. ("ISC")
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

/* $Id: backtrace_test.c,v 1.4 2009-09-02 23:48:01 tbox Exp $ */

#include <config.h>

#include <stdio.h>
#include <string.h>

#include <isc/backtrace.h>
#include <isc/result.h>

const char *expected_symbols[] = {
	"func3",
	"func2",
	"func1",
	"main"
};

static int
func3() {
	void *tracebuf[16];
	int i, nframes;
	int error = 0;
	const char *fname;
	isc_result_t result;
	unsigned long offset;

	result = isc_backtrace_gettrace(tracebuf, 16, &nframes);
	if (result != ISC_R_SUCCESS) {
		printf("isc_backtrace_gettrace failed: %s\n",
		       isc_result_totext(result));
		return (1);
	}

	if (nframes < 4)
		error++;

	for (i = 0; i < 4 && i < nframes; i++) {
		fname = NULL;
		result = isc_backtrace_getsymbol(tracebuf[i], &fname, &offset);
		if (result != ISC_R_SUCCESS) {
			error++;
			continue;
		}
		if (strcmp(fname, expected_symbols[i]) != 0)
			error++;
	}

	if (error) {
		printf("Unexpected result:\n");
		printf("  # of frames: %d (expected: at least 4)\n", nframes);
		printf("  symbols:\n");
		for (i = 0; i < nframes; i++) {
			fname = NULL;
			result = isc_backtrace_getsymbol(tracebuf[i], &fname,
							 &offset);
			if (result == ISC_R_SUCCESS)
				printf("  [%d] %s\n", i, fname);
			else {
				printf("  [%d] getsymbol failed: %s\n", i,
				       isc_result_totext(result));
			}
		}
	}

	return (error);
}

static int
func2() {
	return (func3());
}

static int
func1() {
	return (func2());
}

int
main() {
	return (func1());
}
