#ifndef lint
static char *rcsid = "$Id: iconvchk.c,v 1.1.1.1 2003-06-04 00:26:54 marka Exp $";
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

#include <stdio.h>
#include <stdlib.h>

#include <idn/api.h>
#include <idn/converter.h>
#include <idn/result.h>

#include "codeset.h"

#define IDN_UTF8_ENCODING_NAME	"UTF-8"

void
eucjp_check(void)
{
	idn_result_t r;
	idn_converter_t eucjp_ctx = NULL;

	r = idn_nameinit(0);
	if (r != idn_success) {
		fprintf(stderr, "idn_nameinit(): failed\n");
		exit (1);
	}

	r = idn_converter_create(EUCJP_ENCODING_NAME, &eucjp_ctx, 0);

	if (eucjp_ctx != NULL) {
		idn_converter_destroy(eucjp_ctx);
	}

	if (r != idn_success) {
		if (r == idn_invalid_name) {
			fprintf(stderr, \
				"\"%s\" is invalid codeset name, edit codeset.h\n", \
				EUCJP_ENCODING_NAME);
			exit (1);
		} else {
			fprintf(stderr, \
				"idn_converter_create() failed with error \"%s\"\n", \
				idn_result_tostring(r));
			exit (1);
		}
	}
}

void
sjis_check(void)
{
	idn_result_t r;
	idn_converter_t sjis_ctx = NULL;

	r = idn_nameinit(0);
	if (r != idn_success) {
		fprintf(stderr, "idn_nameinit(): failed\n");
		exit (1);
	}

	r = idn_converter_create(SJIS_ENCODING_NAME, &sjis_ctx, 0);

	if (sjis_ctx != NULL) {
		idn_converter_destroy(sjis_ctx);
	}

	if (r != idn_success) {
		if (r == idn_invalid_name) {
			fprintf(stderr, \
				"\"%s\" is invalid codeset name, edit codeset.h\n", \
				SJIS_ENCODING_NAME);
			exit (1);
		} else {
			fprintf(stderr, \
				"idn_converter_create() failed with error \"%s\"\n", \
				idn_result_tostring(r));
			exit (1);
		}
	}
}

int
main (int ac, char **av)
{
	eucjp_check();
	sjis_check();

	exit (0);
}
