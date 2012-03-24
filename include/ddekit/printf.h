#ifndef _DDEKIT_PRINTF_H
#define _DDEKIT_PRINTF_H

#include <ddekit/ddekit.h>
#include <stdarg.h>

/** Print message.
 * \ingroup DDEKit_util
 */
int ddekit_print(const char *);

/** Print message with format.
 * \ingroup DDEKit_util
 */
int ddekit_printf(const char *fmt, ...);

/** Print message with format list.
 * \ingroup DDEKit_util
 */
int ddekit_vprintf(const char *fmt, va_list va);

/** Log function and message.
 * \ingroup DDEKit_util
 */
#define ddekit_log(doit, msg...) \
	do {                                       \
		if (doit) {                            \
			ddekit_printf("%s(): ", __func__); \
			ddekit_printf(msg);                \
			ddekit_printf("\n");               \
		}                                      \
	} while(0);

#endif
