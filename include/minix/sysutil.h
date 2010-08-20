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

extern int env_argc;
extern char **env_argv;

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
_PROTOTYPE( __dead void panic, (const char *fmt, ...));
_PROTOTYPE( int getuptime, (clock_t *ticks));
_PROTOTYPE( int getuptime2, (clock_t *ticks, time_t *boottime));
_PROTOTYPE( int tickdelay, (clock_t ticks));
_PROTOTYPE( int tsc_calibrate, (void));
_PROTOTYPE( u32_t sys_hz, (void));
_PROTOTYPE( double getidle, (void));
_PROTOTYPE( void util_stacktrace, (void));
_PROTOTYPE( void util_nstrcat, (char *str, unsigned long n) );
_PROTOTYPE( void util_stacktrace_strcat, (char *));
_PROTOTYPE( int micro_delay, (u32_t micros));
_PROTOTYPE( u32_t tsc_64_to_micros, (u64_t tsc));
_PROTOTYPE( u32_t tsc_to_micros, (u32_t low, u32_t high));
_PROTOTYPE( u32_t tsc_get_khz, (void));
_PROTOTYPE( u32_t micros_to_ticks, (u32_t micros));
_PROTOTYPE( void ser_putc, (char c));
_PROTOTYPE( void get_randomness, (struct k_randomness *, int));

#define asynsend(ep, msg) asynsend3(ep, msg, 0)
_PROTOTYPE( int asynsend3, (endpoint_t ep, message *msg, int flags));

#define ASSERT(c) if(!(c)) { panic("%s:%d: assert %s failed", __FILE__, __LINE__, #c); }

/* timing library */
#define TIMING_CATEGORIES       20

#define TIMING_POINTS           20      /* timing resolution */
#define TIMING_CATEGORIES       20
#define TIMING_NAME             10

struct util_timingdata {
        char names[TIMING_NAME];
        unsigned long lock_timings[TIMING_POINTS]; 
        unsigned long lock_timings_range[2];
        unsigned long binsize, resets, misses, measurements;
	unsigned long starttimes[2];	/* nonzero if running */
};

typedef struct util_timingdata util_timingdata_t;

#endif /* _MINIX_SYSUTIL_H */

