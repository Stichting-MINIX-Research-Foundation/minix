#ifndef _MINIX_SYSUTIL_H
#define _MINIX_SYSUTIL_H 1

#include <minix/ipc.h>

/* Extra system library definitions to support device drivers and servers.
 *
 * Created:
 *	Mar 15, 2004 by Jorrit N. Herder
 *
 * Changes:
 *	May 31, 2005: added printf, kputc (relocated from syslib)
 *	May 31, 2005: added getuptime
 *	Mar 18, 2005: added tickdelay
 *	Oct 01, 2004: added env_parse, env_prefix, env_panic
 *	Jul 13, 2004: added fkey_ctl
 *	Apr 28, 2004: added report, panic 
 *	Mar 31, 2004: setup like other libraries, such as syslib
 */

/*==========================================================================* 
 * Miscellaneous helper functions.
 *==========================================================================*/ 

/* Environment parsing return values. */
#define EP_BUF_SIZE   128	/* local buffer for env value */
#define EP_UNSET	0	/* variable not set */
#define EP_OFF		1	/* var = off */
#define EP_ON		2	/* var = on (or field left blank) */
#define EP_SET		3	/* var = 1:2:3 (nonblank field) */
#define EP_EGETKENV	4	/* sys_getkenv() failed ... */

_PROTOTYPE( void env_setargs, (int argc, char *argv[])		        );
_PROTOTYPE( int env_get_param, (char *key, char *value, int max_size)	);
_PROTOTYPE( int env_prefix, (char *env, char *prefix)			);
_PROTOTYPE( void env_panic, (char *key)					);
_PROTOTYPE( int env_parse, (char *env, char *fmt, int field, long *param,
				long min, long max)			);

#define fkey_map(fkeys, sfkeys) fkey_ctl(FKEY_MAP, (fkeys), (sfkeys))
#define fkey_unmap(fkeys, sfkeys) fkey_ctl(FKEY_UNMAP, (fkeys), (sfkeys))
#define fkey_events(fkeys, sfkeys) fkey_ctl(FKEY_EVENTS, (fkeys), (sfkeys))
_PROTOTYPE( int fkey_ctl, (int req, int *fkeys, int *sfkeys)		);

_PROTOTYPE( int printf, (const char *fmt, ...));
_PROTOTYPE( void kputc, (int c));
_PROTOTYPE( void report, (char *who, char *mess, int num));
_PROTOTYPE( void panic, (char *who, char *mess, int num));
_PROTOTYPE( int getuptime, (clock_t *ticks));
_PROTOTYPE( int getuptime2, (clock_t *ticks, time_t *boottime));
_PROTOTYPE( int tickdelay, (clock_t ticks));
_PROTOTYPE( int micro_delay_calibrate, (void));
_PROTOTYPE( u32_t sys_hz, (void));
_PROTOTYPE( void util_stacktrace, (void));
_PROTOTYPE( void util_nstrcat, (char *str, unsigned long n) );
_PROTOTYPE( void util_stacktrace_strcat, (char *));
_PROTOTYPE( int micro_delay, (u32_t micros));
_PROTOTYPE( u32_t micros_to_ticks, (u32_t micros));
_PROTOTYPE( int asynsend, (endpoint_t ep, message *msg));

#define ASSERT(c) if(!(c)) { panic(__FILE__, "assert " #c " failed at line", __LINE__); }

#endif /* _MINIX_SYSUTIL_H */

