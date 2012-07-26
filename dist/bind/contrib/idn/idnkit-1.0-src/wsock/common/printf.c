/*
 * printf.c - printf like debug print function
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
#include <process.h>

#include "wrapcommon.h"

/*
 * Debug Tracer for DLL
 */

static char	logfile_name[256];
static int	log_level = -1;
static char	log_header[30];

void
idnPrintf(char *fmt, ...) {
	va_list arg_ptr;
	FILE *fp;
	char msg[512];

	if (log_level < 0 || logfile_name[0] == '\0')
		return;

	va_start(arg_ptr, fmt);
	vsprintf(msg, fmt, arg_ptr);
	va_end(arg_ptr);
    
	if ((fp = fopen(logfile_name, "a")) != NULL) {
		fputs(log_header, fp);
		fputs(msg, fp);
		fclose(fp);
	}
}

void
idnLogPrintf(int level, char *fmt, ...) {
	va_list arg_ptr;
	FILE *fp;
	char msg[512];

	if (level > log_level || logfile_name[0] == '\0')
		return;

	va_start(arg_ptr, fmt);
	vsprintf(msg, fmt, arg_ptr);
	va_end(arg_ptr);
    
	if ((fp = fopen(logfile_name, "a")) != NULL) {
		fputs(log_header, fp);
		fputs(msg, fp);
		fclose(fp);
	}
}

static void
log_proc(int level, const char *msg) {
	FILE *fp;

	if (log_level < 0 || logfile_name[0] == '\0')
		return;

	if ((fp = fopen(logfile_name, "a")) != NULL) {
		fputs(msg, fp);
		fclose(fp);
	}
}

void
idnLogInit(const char *title) {
	log_level = idnGetLogLevel();
	/* If log file is not stored in the registry, don't do logging. */
	if (idnGetLogFile(logfile_name, sizeof(logfile_name)) == FALSE) {
		log_level = -1;
	}
	sprintf(log_header, "%08x %-.16s: ", getpid(), title);
	idn_log_setproc(log_proc);
	idn_log_setlevel(log_level < 0 ? 0 : log_level);
}

void
idnLogReset(void) {
	idn_log_setproc(log_proc);
}

void
idnLogFinish(void) {
	idn_log_setproc(NULL);
	/* idn_log_setlevel(0); */
}
