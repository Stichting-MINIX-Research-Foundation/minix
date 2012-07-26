/*
 * convert.c - convert domain name
 */

/*
 * Copyright (c) 2000,2002 Japan Network Information Center.
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

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wrapcommon.h"

/*
 * prepare/dispose conversion context
 */
 
void
idnConvDone(idn_resconf_t ctx)
{
	if (ctx != NULL) {
		idnLogReset();
		idn_resconf_destroy(ctx);
	}
}

idn_resconf_t
idnConvInit(void)
{
	char encoding[256];
	idn_resconf_t ctx;
	idn_result_t r;
    
	idnLogReset();

	idnLogPrintf(idn_log_level_info, "idnkit version: %-.20s\n",
		     idn_version_getstring());

	/*
	 * Initialize.
	 */
	if ((r = idn_resconf_initialize()) != idn_success) {
		idnPrintf("idnConvInit: cannot initialize idn library: %s\n",
			  idn_result_tostring(r));
		return NULL;
	}
	if ((r = idn_resconf_create(&ctx)) != idn_success) {
		idnPrintf("idnConvInit: cannot create context: %s\n",
			  idn_result_tostring(r));
		return NULL;
	}
	/*
	 * load configuration file.
	 */
	if ((r = idn_resconf_loadfile(ctx, NULL)) != idn_success) {
		idnPrintf("idnConvInit: cannot read configuration file: %s\n",
			  idn_result_tostring(r));
		if ((r = idn_resconf_setdefaults(ctx)) != idn_success) {
			idnPrintf("idnConvInit: setting default configuration"
				  " failed: %s\n",
				  idn_result_tostring(r));
			idnConvDone(ctx);
			return (NULL);
		}
		idnPrintf("idnConvInit: using default configuration\n");
	}
	/*
	 * Set local codeset.
	 */
	if (idnGetPrgEncoding(encoding, sizeof(encoding)) == TRUE) {
		idnPrintf("Encoding PRG <%-.100s>\n", encoding);
		r = idn_resconf_setlocalconvertername(ctx, encoding,
						      IDN_CONVERTER_RTCHECK);
		if (r != idn_success) {
			idnPrintf("idnConvInit: invalid local codeset "
				  "\"%-.100s\": %s\n",
				  encoding, idn_result_tostring(r));
			idnConvDone(ctx);
			return NULL;
		}
	}
	return ctx;
}

/*
 * idnConvReq - convert domain name in a DNS request
 *
 *      convert local encoding to DNS encoding
 */
 
BOOL
idnConvReq(idn_resconf_t ctx, const char FAR *from, char FAR *to, size_t tolen)
{
	idn_result_t r;

	idnLogReset();

	idnLogPrintf(idn_log_level_trace, "idnConvReq(from=%-.100s)\n", from);
	if (ctx == NULL) {
		idnLogPrintf(idn_log_level_trace, "idnConvReq: ctx is NULL\n");
		if (strlen(from) >= tolen)
			return FALSE;
		strcpy(to, from);
		return TRUE;
	}

	r = idn_res_encodename(ctx, IDN_ENCODE_APP, from, to, tolen);

	if (r == idn_success) {
		return TRUE;
	} else {
		return FALSE;
	}
}

/*
 * idnConvRsp - convert domain name in a DNS response
 *
 *      convert DNS encoding to local encoding
 */

BOOL
idnConvRsp(idn_resconf_t ctx, const char FAR *from, char FAR *to, size_t tolen)
{
	idnLogReset();

	idnLogPrintf(idn_log_level_trace, "idnConvRsp(from=%-.100s)\n", from);
	if (ctx == NULL) {
		if (strlen(from) >= tolen)
			return FALSE;
		strcpy(to, from);
		return TRUE;
	} else if (idn_res_decodename(ctx, IDN_DECODE_APP,
				      from, to, tolen) == idn_success) {
		return TRUE;
	} else {
		return FALSE;
	}
}
