#ifndef lint
static char *rcsid = "$Id: api.c,v 1.1.1.1 2003-06-04 00:25:48 marka Exp $";
#endif

/*
 * Copyright (c) 2001,2002 Japan Network Information Center.
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

#include <config.h>

#include <string.h>
#include <stdlib.h>

#include <idn/result.h>
#include <idn/assert.h>
#include <idn/log.h>
#include <idn/logmacro.h>
#include <idn/resconf.h>
#include <idn/api.h>
#include <idn/debug.h>
#include <idn/res.h>

static int initialized;
static idn_resconf_t default_conf;

static char *conf_file;

void
idn_enable(int on_off) {
	idn_res_enable(on_off);
}

idn_result_t
idn__setconffile(const char *file) {
	idn_result_t r;
	char *s;

	TRACE(("idn__setconffile(%s)\n", (file == NULL) ? "<null>" : file));

	if (initialized) {
		r = idn_failure;
		goto ret;
	}

	if (file == NULL)
		s = NULL;
	else {
		s = (char *)malloc(strlen(file) + 1);
		if (s == NULL) {
			r = idn_nomemory;
			goto ret;
		}
		strcpy(s, file);
	}
	free(conf_file);
	conf_file = s;

	r = idn_success;
ret:
	TRACE(("idn__setconffile(): %s\n", idn_result_tostring(r)));
	return (r);
}

idn_result_t
idn_nameinit(int load_file) {
	idn_result_t r;

	TRACE(("idn_nameinit()\n"));

	if (initialized) {
		r = idn_success;
		goto ret;
	}

	idn_resconf_initialize();

	r = idn_resconf_create(&default_conf);
	if (r != idn_success)
		goto ret;

	if (load_file)
		r = idn_resconf_loadfile(default_conf, conf_file);
	else
		r = idn_resconf_setdefaults(default_conf);
	if (r != idn_success)
		goto ret;

	initialized = 1;

ret:
	if (r != idn_success && default_conf != NULL) {
		idn_resconf_destroy(default_conf);
		default_conf = NULL;
	}
	TRACE(("idn_nameinit(): %s\n", idn_result_tostring(r)));
	return (r);
}

idn_result_t
idn_encodename(idn_action_t actions, const char *from, char *to, size_t tolen) {
	idn_result_t r;

	assert(from != NULL && to != NULL);

	TRACE(("idn_encodename(actions=%s, from=\"%s\")\n",
	       idn__res_actionstostring(actions),
	       idn__debug_xstring(from, 50)));

	if (!initialized && ((r = idn_nameinit(0)) != idn_success))
		goto ret;

	r = idn_res_encodename(default_conf, actions, from, to, tolen);

ret:
	if (r == idn_success) {
		TRACE(("idn_encodename(): success (to=\"%s\")\n",
		       idn__debug_xstring(to, 50)));
	} else {
		TRACE(("idn_encodename(): %s\n", idn_result_tostring(r)));
	}
	return (r);
}

idn_result_t
idn_decodename(idn_action_t actions, const char *from, char *to, size_t tolen) {
	idn_result_t r;

	assert(from != NULL && to != NULL);

	TRACE(("idn_decodename(actions=%s, from=\"%s\", tolen=%d)\n",
	       idn__res_actionstostring(actions),
	       idn__debug_xstring(from, 50), (int)tolen));

	if (!initialized && ((r = idn_nameinit(0)) != idn_success))
		goto ret;

	r = idn_res_decodename(default_conf, actions, from, to, tolen);

ret:
	if (r == idn_success) {
		TRACE(("idn_decodename(): success (to=\"%s\")\n",
		       idn__debug_xstring(to, 50)));
	} else {
		TRACE(("idn_decodename(): %s\n", idn_result_tostring(r)));
	}
	return (r);
}

idn_result_t
idn_decodename2(idn_action_t actions, const char *from, char *to, size_t tolen,
		const char *auxencoding) {
	idn_result_t r;

	assert(from != NULL && to != NULL);

	TRACE(("idn_decodename2(actions=%s, from=\"%s\", tolen=%d)\n",
	       idn__res_actionstostring(actions),
	       idn__debug_xstring(from, 50), (int)tolen));

	if (!initialized && ((r = idn_nameinit(0)) != idn_success))
		goto ret;

	r = idn_res_decodename2(default_conf, actions, from, to, tolen,
				auxencoding);

ret:
	if (r == idn_success) {
		TRACE(("idn_decodename2(): success (to=\"%s\")\n",
		       idn__debug_xstring(to, 50)));
	} else {
		TRACE(("idn_decodename2(): %s\n", idn_result_tostring(r)));
	}
	return (r);
}

/*
 * These functions are for backward compatibility.
 */
#ifdef ENABLE_MDNKIT_COMPAT

idn_result_t
mdn_nameinit(void) {
	return idn_nameinit(1);
}

idn_result_t
mdn_encodename(int actions, const char *from, char *to, size_t tolen) {
	idn_result_t r;

	assert(from != NULL && to != NULL);

	TRACE(("mdn_encodename(actions=%s, from=\"%s\")\n",
	       idn__res_actionstostring(actions),
	       idn__debug_xstring(from, 50)));

	if (!initialized && ((r = idn_nameinit(1)) != idn_success))
		return (r);

	return (idn_res_encodename(default_conf, actions, from, to, tolen));
}

idn_result_t
mdn_decodename(int actions, const char *from, char *to, size_t tolen) {
	idn_result_t r;

	assert(from != NULL && to != NULL);

	TRACE(("idn_decodename(actions=%s, from=\"%s\", tolen=%d)\n",
	       idn__res_actionstostring(actions),
	       idn__debug_xstring(from, 50), (int)tolen));

	if (!initialized && ((r = idn_nameinit(1)) != idn_success))
		return (r);

	return (idn_res_decodename(default_conf, actions, from, to, tolen));
}

#endif /* ENABLE_MDNKIT_COMPAT */
