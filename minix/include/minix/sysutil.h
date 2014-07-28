#ifndef _MINIX_SYSUTIL_H
#define _MINIX_SYSUTIL_H 1

#include <minix/ipc.h>
#include <sys/cdefs.h>

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

void env_setargs(int argc, char *argv[]);
int env_get_param(const char *key, char *value, int max_size);
int env_prefix(char *env, char *prefix);
void env_panic(const char *key);
int env_parse(const char *env, const char *fmt, int field,
	long *param, long min, long max);

#define fkey_map(fkeys, sfkeys) fkey_ctl(FKEY_MAP, (fkeys), (sfkeys))
#define fkey_unmap(fkeys, sfkeys) fkey_ctl(FKEY_UNMAP, (fkeys), (sfkeys))
#define fkey_events(fkeys, sfkeys) fkey_ctl(FKEY_EVENTS, (fkeys), (sfkeys))
int fkey_ctl(int req, int *fkeys, int *sfkeys);

int printf(const char *fmt, ...);
void kputc(int c);
__dead void panic(const char *fmt, ...)
     __attribute__((__format__(__printf__,1,2)));
void panic_hook(void);
void __panic_hook(void);
int getuptime(clock_t *ticks, clock_t *realtime, time_t *boottime);
int getticks(clock_t *ticks);
int tickdelay(clock_t ticks);
int tsc_calibrate(void);
u32_t sys_hz(void);
double getidle(void);
void util_stacktrace(void);
int micro_delay(u32_t micros);
u32_t tsc_64_to_micros(u64_t tsc);
u32_t tsc_to_micros(u32_t low, u32_t high);
u32_t tsc_get_khz(void);
u32_t micros_to_ticks(u32_t micros);
#if defined(__arm__)
void read_frclock(u32_t *frclk);
u32_t delta_frclock(u32_t base, u32_t cur);
#endif
void read_frclock_64(u64_t *frclk);
u64_t delta_frclock_64(u64_t base, u64_t cur);
u32_t frclock_64_to_micros(u64_t tsc);
void ser_putc(char c);
void get_randomness(struct k_randomness *, int);
u32_t sqrt_approx(u32_t);

int stime(time_t *_top);

#define asynsend(ep, msg) asynsend3(ep, msg, 0)
int asynsend3(endpoint_t ep, message *msg, int flags);
int asyn_geterror(endpoint_t *dst, message *msg, int *err);

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

