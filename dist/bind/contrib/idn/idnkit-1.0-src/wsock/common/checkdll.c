/*
 * checkdll.c - Winsock DLL/IDN processing status
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

static int winsock_idx;		/* index of winsock_info[] */

static struct winsock_type {
	char *version;		/* winsock version */
	char *name;		/* wrapper DLL name */
	char *original_name;	/* original DLL name */
} winsock_info[] = {
#define IDN_IDX_WS11 0
	{ "1.1", "WSOCK32", "WSOCK32O" },
#define IDN_IDX_WS20 1
	{ "2.0", "WS2_32",  "WS2_32O" },
	{ NULL, NULL, NULL },
};

static HINSTANCE	load_original_dll(void);
static BOOL		check_idn_processing(void);
static BOOL		check_dll(const char *name);

BOOL
idnWinsockVersion(const char *version) {
	int i;
	for (i = 0; winsock_info[i].version != NULL; i++) {
		if (strcmp(winsock_info[i].version, version) == 0) {
			winsock_idx = i;
			idnLogPrintf(idn_log_level_trace,
				     "idnWinsockVersion: version %s\n",
				     version);
			return (TRUE);
		}
	}
	idnLogPrintf(idn_log_level_fatal,
		     "idnWinsockVersion: unknown winsock version %s\n",
		     version);
	return (FALSE);
}

HINSTANCE
idnWinsockHandle(void) {
	static HINSTANCE dll_handle = NULL;
	static int initialized = 0;

	if (!initialized) {
		/* Get the handle of the original winsock DLL */
		idnLogPrintf(idn_log_level_trace,
			     "idnWinsockHandle: loading original DLL..\n");
		dll_handle = load_original_dll();
	}
	initialized = 1;
	return (dll_handle);
}

idn_resconf_t
idnGetContext(void) {
	static int initialized = 0;
	static idn_resconf_t ctx = NULL;

	if (!initialized) {
		/*
		 * Check whether IDN processing should be done
		 * in this wrapper DLL.
		 */
		idnLogPrintf(idn_log_level_trace,
			     "idnGetContext: checking IDN status..\n");
		if (check_idn_processing()) {
			/* Initialize idnkit */
			ctx = idnConvInit();
			idnLogPrintf(idn_log_level_info,
				     "Processing context: %08x\n", ctx);
		} else {
			idnLogPrintf(idn_log_level_info,
				     "NOT process IDN here\n");
			ctx = NULL;
		}
		initialized = 1;
	}

	return (ctx);
}

static HINSTANCE
load_original_dll(void) {
	/*
	 * Load Original DLL
	 */
	char dllpath[MAX_PATH];
	const char *dll_name = winsock_info[winsock_idx].original_name;
	HINSTANCE handle;

	/*
	 * Get idn wrapper's install directory, where the copies of
	 * the original winsock DLLs are saved.
	 */
	dllpath[0] = '\0';
	if (idnGetInstallDir(dllpath, sizeof(dllpath)) != TRUE) {
		idnLogPrintf(idn_log_level_fatal,
			     "idnWinsockHandle: cannot find idn wrapper's "
			     "install directory\n");
		abort();
		return (NULL);	/* for lint */
	}
	/* Strip the trailing backslash. */
	if (dllpath[0] != '\0' &&
	    dllpath[strlen(dllpath) - 1] == '\\') {
		dllpath[strlen(dllpath) - 1] = '\0';
	}
	/* Is the pathname is insanely long? */
	if (strlen(dllpath) + strlen(dll_name) + 1 + 4 >= sizeof(dllpath)) {
		idnLogPrintf(idn_log_level_fatal,
			     "idnWinsockHandle: idn wrapper's install path is "
			     "too long to be true\n");
		abort();
		return (NULL);	/* for lint */
	}
	/* Append the DLL name to form a full pathname of the DLL. */
	strcat(dllpath, "\\");
	strcat(dllpath, dll_name);
	strcat(dllpath, ".DLL");

	idnLogPrintf(idn_log_level_trace,
		     "idnWinsockHandle: loading original winsock DLL (%s)\n",
		     dllpath);
	if ((handle = LoadLibrary(dllpath)) == NULL) {
		idnLogPrintf(idn_log_level_fatal,
			     "idnWinsockHandle: no DLL %-.100s\n", dllpath);
		abort();
		return (NULL);	/* font lint */
	}
	return (handle);
}

static BOOL
check_idn_processing(void) {
	int where = idnEncodeWhere();
	BOOL here = FALSE;

	idnLogPrintf(idn_log_level_trace,
		     "idnGetContext: Winsock%s, where=%d\n",
		     winsock_info[winsock_idx].version, where);

	switch (winsock_idx) {
	case IDN_IDX_WS11:
		switch (where) {
		case IDN_ENCODE_ALWAYS:
		case IDN_ENCODE_ONLY11:
			return (TRUE);
		case IDN_ENCODE_CHECK:
			if (!check_dll(winsock_info[winsock_idx].name)) {
				return (TRUE);
			}
			break;
		}
		break;
	case IDN_IDX_WS20:
		switch (where) {
		case IDN_ENCODE_ALWAYS:
		case IDN_ENCODE_ONLY20:
		case IDN_ENCODE_CHECK:
			return (TRUE);
			break;
		}
		break;
	}
	return (FALSE);
}

static BOOL
check_dll(const char *name) {
	HINSTANCE hdll = NULL;

#if 1
	hdll = LoadLibrary(name);
#else
	/*
	 * Just check the existence of the named DLL, without taking
	 * the trouble of calling DllMain.
	 */
	hdll = LoadLibraryEx(name, NULL, LOAD_LIBRARY_AS_DATAFILE);
#endif
	if (hdll == NULL) {
		idnLogPrintf(idn_log_level_trace,
			     "idnGetContext: DLL %s does not exist\n");
		return (FALSE);
	} else {
		idnLogPrintf(idn_log_level_trace,
			     "idnGetContext: DLL %s exists\n");
		FreeLibrary(hdll);
		return (TRUE);
	}
}
