#ifndef _EXTRALIB_H
#define _EXTRALIB_H

/* Extra system library definitions to support device drivers and servers.
 *
 * Created:
 *	Mar 15, 2004 by Jorrit N. Herder
 *
 * Changes:
 *	Mar 18, 2005: added tick_delay
 *	Mar 15, 2005: added get_proc_nr
 *	Oct 01, 2004: added env_parse, env_prefix, env_panic
 *	Jul 13, 2004: added fkey_ctl
 *	Apr 28, 2004: added server_report, server_panic, server_assert 
 *	Mar 31, 2004: setup like other libraries, such as syslib
 */


/*==========================================================================* 
 * Miscellaneous helper functions.
 *==========================================================================*/ 

#include <minix/serverassert.h>

/* Environment parsing return values. */
#define EP_BUF_SIZE   128	/* local buffer for env value */
#define EP_UNSET	0	/* variable not set */
#define EP_OFF		1	/* var = off */
#define EP_ON		2	/* var = on (or field left blank) */
#define EP_SET		3	/* var = 1:2:3 (nonblank field) */
#define EP_EGETKENV	4	/* sys_getkenv() failed ... */

_PROTOTYPE(int get_mon_param, (char *key, char *value, int max_size)	);
_PROTOTYPE(int env_prefix, (char *env, char *prefix)			);
_PROTOTYPE(void env_panic, (char *key)					);
_PROTOTYPE(int env_parse, (char *env, char *fmt, int field, long *param,
				long min, long max)			);

#define fkey_enable(fkey) fkey_ctl(fkey, 1)
#define fkey_disable(fkey) fkey_ctl(fkey, 0)
_PROTOTYPE(int fkey_ctl, (int fkey_code, int enable_disable)		);

_PROTOTYPE(void server_report, (char *who, char *mess, int num)		);
_PROTOTYPE(void server_panic, (char *who, char *mess, int num)		);

_PROTOTYPE(int get_proc_nr, (int *proc_nr, char *proc_name) );

_PROTOTYPE(int getuptime, (clock_t *ticks));
_PROTOTYPE(int tick_delay, (clock_t ticks));

#endif /* _EXTRALIB_H */

