#ifndef lint
static char *rcsid = "$Id";
#endif

/*
 * Copyright (c) 2002 Japan Network Information Center.
 * All rights reserved.
 *  
 * By using this file, you agree to the terms and conditions set forth bellow.
 * 
 * 			LICENSE TERMS AND CONDITIONS 
 * 
 * The following License Terms and Conditions apply, unless a different
 * license is obtained from Japan Network Information Center ("JPNIC"),
 * a Japanese association, Kokusai-Kougyou-Kanda Bldg 6F, 2-3-4 Uchi-Kanda,
 * Chiyoda-ku, Tokyo 101-0047, Japan.
 * 
 * 1. Use, Modification and Redistribution (including distribution of any
 *    modified or derived work) in source and/or binary forms is permitted
 *    under this License Terms and Conditions.
 * 
 * 2. Redistribution of source code must retain the copyright notices as they
 *    appear in each source code file, this License Terms and Conditions.
 * 
 * 3. Redistribution in binary form must reproduce the Copyright Notice,
 *    this License Terms and Conditions, in the documentation and/or other
 *    materials provided with the distribution.  For the purposes of binary
 *    distribution the "Copyright Notice" refers to the following language:
 *    "Copyright (c) 2000-2002 Japan Network Information Center.  All rights reserved."
 * 
 * 4. The name of JPNIC may not be used to endorse or promote products
 *    derived from this Software without specific prior written approval of
 *    JPNIC.
 * 
 * 5. Disclaimer/Limitation of Liability: THIS SOFTWARE IS PROVIDED BY JPNIC
 *    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 *    PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL JPNIC BE LIABLE
 *    FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 *    BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 *    WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 *    OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 *    ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
 */

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <idn/result.h>
#include <idn/ucs4.h>
#include <testsuite.h>

typedef struct idn_testcase *idn_testcase_t;

struct idn_testcase {
	char *title;
	idn_testsuite_testproc_t proc;
};

struct idn_testsuite {
	idn_testcase_t testcases;
	int ntestcases;
	int testcase_size;

	int npassed;
	int nfailed;
	int nskipped;
	idn_testcase_t current_testcase;
	idn_teststatus_t current_status;

	idn_testsuite_msgproc_t msgproc;
	int verbose;
};

#define INITIAL_TESTCASE_SIZE	16
#define INITIAL_SETUP_SIZE	4
#define INITIAL_TEARDOWN_SIZE	4

static void run_internal(idn_testsuite_t ctx, char *titles[]);
static char *make_hex_string(const char *string);
static char *make_hex_ucs4string(const unsigned long *string);
static void put_failure_message(idn_testsuite_t ctx, const char *msg,
				const char *file, int lineno);
static void idn_testsuite_msgtostderr(const char *msg);

int
idn_testsuite_create(idn_testsuite_t *ctxp) {
	idn_testsuite_t ctx = NULL;

	assert(ctxp != NULL);

	ctx = (idn_testsuite_t) malloc(sizeof(struct idn_testsuite));
	if (ctx == NULL)
		goto error;

	ctx->testcases = NULL;
	ctx->ntestcases = 0;
	ctx->testcase_size = 0;
	ctx->npassed = 0;
	ctx->nfailed = 0;
	ctx->nskipped = 0;
	ctx->current_testcase = NULL;
	ctx->current_status = idn_teststatus_pass;
	ctx->msgproc = NULL;
	ctx->verbose = 0;

	ctx->testcases = (idn_testcase_t) malloc(sizeof(struct idn_testcase)
						 * INITIAL_TESTCASE_SIZE);
	if (ctx->testcases == NULL)
		goto error;
	ctx->testcase_size = INITIAL_TESTCASE_SIZE;

	*ctxp = ctx;
	return (1);

error:
	if (ctx != NULL)
		free(ctx->testcases);
	free(ctx);
	return (0);
}

void
idn_testsuite_destroy(idn_testsuite_t ctx) {
	int i;

	assert(ctx != NULL);

	for (i = 0; i < ctx->ntestcases; i++)
		free(ctx->testcases[i].title);

	free(ctx->testcases);
	free(ctx);
}

int
idn_testsuite_addtestcase(idn_testsuite_t ctx, const char *title,
			  idn_testsuite_testproc_t proc) {
	char *dup_title = NULL;
	idn_testcase_t new_buffer = NULL;
	idn_testcase_t new_testcase;
	int new_size;

	assert(ctx != NULL && title != NULL && proc != NULL);

	dup_title = (char *)malloc(strlen(title) + 1);
	if (dup_title == NULL)
		goto error;
	strcpy(dup_title, title);

	if (ctx->ntestcases == ctx->testcase_size) {
		new_size = ctx->testcase_size + INITIAL_TESTCASE_SIZE;
		new_buffer = (idn_testcase_t)
			     realloc(ctx->testcases,
				     sizeof(struct idn_testcase) * new_size);
		if (new_buffer == NULL)
			goto error;
		ctx->testcases = new_buffer;
		ctx->testcase_size = new_size;
	}

	new_testcase = ctx->testcases + ctx->ntestcases;
	new_testcase->title = dup_title;
	new_testcase->proc = proc;
	ctx->ntestcases++;
	return (1);

error:
	free(dup_title);
	free(new_buffer);
	return (0);
}

int
idn_testsuite_ntestcases(idn_testsuite_t ctx) {
	assert(ctx != NULL);
	return (ctx->ntestcases);
}

void
idn_testsuite_setverbose(idn_testsuite_t ctx) {
	assert(ctx != NULL);
	ctx->verbose = 1;
}

void
idn_testsuite_unsetverbose(idn_testsuite_t ctx) {
	assert(ctx != NULL);
	ctx->verbose = 0;
}

static void
run_internal(idn_testsuite_t ctx, char *titles[]) {
	int i, j;
	int run_testcase;
	const char *status;

	assert(ctx != NULL);

	ctx->npassed = 0;
	ctx->nfailed = 0;
	ctx->nskipped = 0;

	for (i = 0; i < ctx->ntestcases; i++) {
		ctx->current_testcase = ctx->testcases + i;
		ctx->current_status = idn_teststatus_pass;

		if (titles == NULL)
			run_testcase = 1;
		else {
			run_testcase = 0;
			for (j = 0; titles[j] != NULL; j++) {
				if (strcmp(ctx->current_testcase->title,
				    titles[j]) == 0) {
					run_testcase = 1;
					break;
				}
			}
		}

		if (!run_testcase) {
			ctx->nskipped++;
			continue;
		}
		if (ctx->verbose) {
			fprintf(stderr, "start testcase %d: %s\n", i + 1,
				ctx->testcases[i].title);
		}
		(ctx->testcases[i].proc)(ctx);
		status = idn_teststatus_tostring(ctx->current_status);
		if (ctx->verbose) {
			fprintf(stderr, "end testcase %d: %s\n", i + 1,
				status);
		}

		switch (ctx->current_status) {
		case idn_teststatus_pass:
			ctx->npassed++;
			break;
		case idn_teststatus_fail:
			ctx->nfailed++;
			break;
		case idn_teststatus_skip:
			ctx->nskipped++;
			break;
		}
	}
}

void
idn_testsuite_runall(idn_testsuite_t ctx) {
	assert(ctx != NULL);
	run_internal(ctx, NULL);
}

void
idn_testsuite_run(idn_testsuite_t ctx, char *titles[]) {
	assert(ctx != NULL && titles != NULL);
	run_internal(ctx, titles);
}

int
idn_testsuite_npassed(idn_testsuite_t ctx) {
	assert(ctx != NULL);
	return (ctx->npassed);
}

int
idn_testsuite_nfailed(idn_testsuite_t ctx) {
	assert(ctx != NULL);
	return (ctx->nfailed);
}

int
idn_testsuite_nskipped(idn_testsuite_t ctx) {
	assert(ctx != NULL);
	return (ctx->nskipped);
}

idn_teststatus_t
idn_testsuite_getstatus(idn_testsuite_t ctx) {
	assert(ctx != NULL);
	return (ctx->current_status);
}

void
idn_testsuite_setstatus(idn_testsuite_t ctx, idn_teststatus_t status) {
	assert(ctx != NULL);
	assert(status == idn_teststatus_pass ||
	       status == idn_teststatus_fail ||
	       status == idn_teststatus_skip);

	ctx->current_status = status;
}

const char *
idn_teststatus_tostring(idn_teststatus_t status) {
	assert(status == idn_teststatus_pass ||
	       status == idn_teststatus_fail ||
	       status == idn_teststatus_skip);

	switch (status) {
		case idn_teststatus_pass:
			return "pass";
			break;
		case idn_teststatus_fail:
			return "failed";
			break;
		case idn_teststatus_skip:
			return "skipped";
			break;
	}

	return "unknown";
}

void
idn_testsuite_assert(idn_testsuite_t ctx, const char *msg,
		     const char *file, int lineno) {
	assert(ctx != NULL && msg != NULL && file != NULL);

	if (idn_testsuite_getstatus(ctx) != idn_teststatus_pass)
		return;
	idn_testsuite_setstatus(ctx, idn_teststatus_fail);
	put_failure_message(ctx, msg, file, lineno);
}

void
idn_testsuite_assertint(idn_testsuite_t ctx, int gotten, int expected, 
			const char *file, int lineno) {
	char msg[256]; /* large enough */

	assert(ctx != NULL && file != NULL);

	if (idn_testsuite_getstatus(ctx) != idn_teststatus_pass)
		return;
	if (expected == gotten)
		return;
	idn_testsuite_setstatus(ctx, idn_teststatus_fail);

	sprintf(msg, "`%d' expected, but got `%d'", expected, gotten);
	put_failure_message(ctx, msg, file, lineno);
}

void
idn_testsuite_assertstring(idn_testsuite_t ctx,
			   const char *gotten, const char *expected,
			   const char *file, int lineno) {
	char *expected_hex = NULL;
	char *gotten_hex = NULL;
	char *msg;

	assert(ctx != NULL && gotten != NULL && expected != NULL &&
	       file != NULL);

	if (idn_testsuite_getstatus(ctx) != idn_teststatus_pass)
		return;
	if (strcmp(expected, gotten) == 0)
		return;
	idn_testsuite_setstatus(ctx, idn_teststatus_fail);

	msg = (char *)malloc(strlen(expected) * 4 + strlen(gotten) * 4 + 32);
	expected_hex = make_hex_string(expected);
	gotten_hex = make_hex_string(gotten);
	if (msg == NULL || expected_hex == NULL || gotten_hex == NULL) {
		msg = "";
	} else {
		sprintf(msg, "`%s' expected, but got `%s'",
			expected_hex, gotten_hex);
	}

	put_failure_message(ctx, msg, file, lineno);

	free(msg);
	free(expected_hex);
	free(gotten_hex);
}

void
idn_testsuite_assertptr(idn_testsuite_t ctx, const void *gotten,
			const void *expected, const char *file, int lineno) {
	char *msg;

	assert(ctx != NULL && file != NULL);

	if (idn_testsuite_getstatus(ctx) != idn_teststatus_pass)
		return;
	if (expected == gotten)
		return;
	idn_testsuite_setstatus(ctx, idn_teststatus_fail);

	if (expected == NULL)
		msg = "NULL expected, but got non-NULL";
	else if (gotten == NULL)
		msg = "non-NULL expected, but got NULL";
	else
		msg = "expected pointer != gotten pointer";
	put_failure_message(ctx, msg, file, lineno);
}

void
idn_testsuite_assertptrne(idn_testsuite_t ctx,
			  const void *gotten, const void *unexpected,
			  const char *file, int lineno) {
	char *msg;

	assert(ctx != NULL && file != NULL);

	if (idn_testsuite_getstatus(ctx) != idn_teststatus_pass)
		return;
	if (unexpected != gotten)
		return;
	idn_testsuite_setstatus(ctx, idn_teststatus_fail);

	if (unexpected == NULL)
		msg = "non-NULL unexpected, but got NULL";
	else if (gotten == NULL)
		msg = "non-NULL expected, but got NULL";
	else
		msg = "expected pointer == gotten pointer";
	put_failure_message(ctx, msg, file, lineno);
}

void
idn_testsuite_assertresult(idn_testsuite_t ctx,
			   idn_result_t gotten, idn_result_t expected,
			   const char *file, int lineno) {
	char msg[256]; /* large enough */

	assert(ctx != NULL && file != NULL);

	if (idn_testsuite_getstatus(ctx) != idn_teststatus_pass)
		return;
	if (expected == gotten)
		return;
	idn_testsuite_setstatus(ctx, idn_teststatus_fail);

	sprintf(msg, "`%s' expected, but got `%s'",
		idn_result_tostring(expected), idn_result_tostring(gotten));
	put_failure_message(ctx, msg, file, lineno);
}

void
idn_testsuite_assertucs4string(idn_testsuite_t ctx,
			       const unsigned long *gotten,
			       const unsigned long *expected, 
			       const char *file, int lineno) {
	char *expected_hex = NULL;
	char *gotten_hex = NULL;
	char *msg;

	assert(ctx != NULL && gotten != NULL && expected != NULL &&
	       file != NULL);

	if (idn_testsuite_getstatus(ctx) != idn_teststatus_pass)
		return;
	if (idn_ucs4_strcmp(expected, gotten) == 0)
		return;
	idn_testsuite_setstatus(ctx, idn_teststatus_fail);

	msg = (char *)malloc(idn_ucs4_strlen(expected) * 8 +
			     idn_ucs4_strlen(gotten) * 8 + 32);
	expected_hex = make_hex_ucs4string(expected);
	gotten_hex = make_hex_ucs4string(gotten);
	if (msg == NULL || expected_hex == NULL || gotten_hex == NULL) {
		msg = "";
	} else {
		sprintf(msg, "`%s' expected, but got `%s'",
			expected_hex, gotten_hex);
	}

	put_failure_message(ctx, msg, file, lineno);

	free(msg);
	free(expected_hex);
	free(gotten_hex);
}

static char *
make_hex_string(const char *string) {
	static const char hex[] = {"0123456789abcdef"};
	char *hex_string;
	const char *src;
	char *dst;

	hex_string = (char *)malloc((strlen(string)) * 4 + 1);
	if (hex_string == NULL)
		return NULL;

	for (src = string, dst = hex_string; *src != '\0'; src++) {
		if (0x20 <= *src && *src <= 0x7e && *src != '\\') {
			*dst++ = *src;
		} else {
			*dst++ = '\\';
			*dst++ = 'x';
			*dst++ = hex[*(const unsigned char *)src >> 4];
			*dst++ = hex[*src & 0x0f];
		}
	}
	*dst = '\0';

	return hex_string;
}

#define UCS4_MAX 0x10fffffUL

static char *
make_hex_ucs4string(const unsigned long *string) {
	static const char hex[] = {"0123456789abcdef"};
	char *hex_string;
	const unsigned long *src;
	char *dst;

	hex_string = (char *)malloc((idn_ucs4_strlen(string)) * 8 + 1);
	if (hex_string == NULL)
		return NULL;

	for (src = string, dst = hex_string; *src != '\0'; src++) {
		if (0x20 <= *src && *src <= 0x7e && *src != '\\') {
			*dst++ = *src;
		} else if (*src <= UCS4_MAX) {
			*dst++ = '\\';
			*dst++ = 'u';
			if (*src >= 0x100000) {
				*dst++ = hex[(*src >> 20) & 0x0f];
			}
			if (*src >= 0x10000) {
				*dst++ = hex[(*src >> 16) & 0x0f];
			}
			*dst++ = hex[(*src >> 12) & 0x0f];
			*dst++ = hex[(*src >> 8) & 0x0f];
			*dst++ = hex[(*src >> 4) & 0x0f];
			*dst++ = hex[*src & 0x0f];
		} else {
			*dst++ = '\\';
			*dst++ = 'u';
			*dst++ = '?';
			*dst++ = '?';
			*dst++ = '?';
			*dst++ = '?';
		}
	}
	*dst = '\0';

	return hex_string;
}

static void
put_failure_message(idn_testsuite_t ctx, const char *msg, const char *file,
		    int lineno) {
	idn_testsuite_msgproc_t proc;
	char buffer[256];
	const char *title;

        proc = (ctx->msgproc == NULL) ?
               idn_testsuite_msgtostderr : ctx->msgproc;
	title = (ctx->current_testcase != NULL &&
		 ctx->current_testcase->title != NULL) ?
		 ctx->current_testcase->title : "anonymous";

	sprintf(buffer, "%.100s: In test `%.100s':", file, title);
	(*proc)(buffer);

	sprintf(buffer, "%.100s:%d: failed (%.100s)", file, lineno, msg);
	(*proc)(buffer);
}


static void
idn_testsuite_msgtostderr(const char *msg) {
	fputs(msg, stderr);
	fputc('\n', stderr);
}
