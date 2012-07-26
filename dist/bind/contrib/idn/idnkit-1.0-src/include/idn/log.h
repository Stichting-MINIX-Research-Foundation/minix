/* $Id: log.h,v 1.1.1.1 2003-06-04 00:25:38 marka Exp $ */
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

#ifndef IDN_LOG_H
#define IDN_LOG_H 1

#ifdef __cplusplus
extern "C" {
#endif

/*
 * libidnkit logging facility.
 */

#include <idn/export.h>

/*
 * Log level definition.
 */
enum {
	idn_log_level_fatal = 0,
	idn_log_level_error = 1,
	idn_log_level_warning = 2,
	idn_log_level_info = 3,
	idn_log_level_trace = 4,
	idn_log_level_dump = 5
};

/*
 * Log handler type.
 */
typedef void	(*idn_log_proc_t)(int level, const char *msg);

/*
 * Log routines.
 */
IDN_EXPORT void	idn_log_fatal(const char *fmt, ...);
IDN_EXPORT void	idn_log_error(const char *fmt, ...);
IDN_EXPORT void	idn_log_warning(const char *fmt, ...);
IDN_EXPORT void	idn_log_info(const char *fmt, ...);
IDN_EXPORT void	idn_log_trace(const char *fmt, ...);
IDN_EXPORT void	idn_log_dump(const char *fmt, ...);

/*
 * Set/get log level.
 *
 * If log level has not been explicitly defined by 'idn_log_setlevel',
 * the default level is determined by the value of enrironment
 * variable 'IDN_LOG_LEVEL'.
 */
IDN_EXPORT void	idn_log_setlevel(int level);
IDN_EXPORT int	idn_log_getlevel(void);

/*
 * Set log handler.
 *
 * If no log handler is set, log goes to stderr by default.
 * You can reset the handler to the default one by specifying
 * NULL.
 */
IDN_EXPORT void	idn_log_setproc(idn_log_proc_t proc);

#ifdef __cplusplus
}
#endif

#endif /* IDN_LOG_H */
