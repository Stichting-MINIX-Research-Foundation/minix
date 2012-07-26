#ifndef lint
static char *rcsid = "$Id: log.c,v 1.1.1.1 2003-06-04 00:25:53 marka Exp $";
#endif

/*
 * Copyright (c) 2000 Japan Network Information Center.  All rights reserved.
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

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <idn/log.h>

#define LOGLEVEL_ENV	"IDN_LOG_LEVEL"

#ifdef DEBUG
#define DEFAULT_LOG_LEVEL	idn_log_level_info
#else
#define DEFAULT_LOG_LEVEL	idn_log_level_error
#endif

static int		log_level = -1;
static idn_log_proc_t	log_proc;

static void	initialize(void);
static void	log(int level, const char *fmt, va_list args);
static void	log_to_stderr(int level, const char *buf);

void
idn_log_fatal(const char *fmt, ...) {
	va_list args;

	va_start(args, fmt);
	log(idn_log_level_fatal, fmt, args);
	va_end(args);
	exit(1);
}

void
idn_log_error(const char *fmt, ...) {
	va_list args;

	va_start(args, fmt);
	log(idn_log_level_error, fmt, args);
	va_end(args);
}

void
idn_log_warning(const char *fmt, ...) {
	va_list args;

	va_start(args, fmt);
	log(idn_log_level_warning, fmt, args);
	va_end(args);
}

void
idn_log_info(const char *fmt, ...) {
	va_list args;

	va_start(args, fmt);
	log(idn_log_level_info, fmt, args);
	va_end(args);
}

void
idn_log_trace(const char *fmt, ...) {
	va_list args;

	va_start(args, fmt);
	log(idn_log_level_trace, fmt, args);
	va_end(args);
}

void
idn_log_dump(const char *fmt, ...) {
	va_list args;

	va_start(args, fmt);
	log(idn_log_level_dump, fmt, args);
	va_end(args);
}

void
idn_log_setlevel(int level) {
	if (level >= 0)
		log_level = level;
}

int
idn_log_getlevel(void) {
	if (log_level < 0)
		initialize();
	return log_level;
}

void
idn_log_setproc(idn_log_proc_t proc) {
	if (proc == NULL)
		log_proc = log_to_stderr;
	else
		log_proc = proc;
}

static void
initialize(void) {
	char *s;

	if (log_level < 0) {
		if ((s = getenv(LOGLEVEL_ENV)) != NULL) {
			int level = atoi(s);
			if (level >= 0)
				log_level = level;
		}
		if (log_level < 0)
			log_level = DEFAULT_LOG_LEVEL;
	}

	if (log_proc == NULL)
		log_proc = log_to_stderr;
}

static void
log(int level, const char *fmt, va_list args) {
	char buf[1024];

	initialize();

	if (log_level < level)
		return;

#if HAVE_VSNPRINTF
	(void)vsnprintf(buf, sizeof(buf), fmt, args);
#else
	/* Let's hope 1024 is enough.. */
	(void)vsprintf(buf, fmt, args);
#endif
	(*log_proc)(level, buf);
}

static void
log_to_stderr(int level, const char *buf) {
	char *title;
	char tmp[20];

	switch (level) {
	case idn_log_level_fatal:
		title = "FATAL";
		break;
	case idn_log_level_error:
		title = "ERROR";
		break;
	case idn_log_level_warning:
		title = "WARNING";
		break;
	case idn_log_level_info:
		title = "INFO";
		break;
	case idn_log_level_trace:
		title = "TRACE";
		break;
	case idn_log_level_dump:
		title = "DUMP";
		break;
	default:
		(void)sprintf(tmp, "LEVEL%d", level);
		title = tmp;
		break;
	}
	fprintf(stderr, "%u: [%s] %s", (unsigned int)getpid(), title, buf);
}
