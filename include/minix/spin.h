/* Prototypes for condition spinning helper functions (part of libsys). */
#ifndef _MINIX_SPIN_H
#define _MINIX_SPIN_H

/* Opaque spin state structure. */
typedef struct {
	int s_state;
	u32_t s_usecs;
	u64_t s_base_tsc;
	clock_t s_base_uptime;
	int s_timeout;
} spin_t;

/* Functions. */
void spin_init(spin_t *s, u32_t usecs);
int spin_check(spin_t *s);

/* Macros. */

/* Execute a loop for at least 'u' microseconds, using spin object 's'.
 * The body of the loop is guaranteed to be executed at least once.
 */
#define SPIN_FOR(s,u)							\
	for (spin_init((s), (u)); spin_check((s)); )

/* Return whether spin object 's' timed out after a loop. */
#define SPIN_TIMEOUT(s) ((s)->s_timeout)

/* Spin until the given condition becomes true, or 'u' microseconds expired.
 * The condition is guaranteed to be checked at least once.
 */
#define SPIN_UNTIL(c,u) do {						\
	spin_t s;							\
	SPIN_FOR(&s,(u))						\
		if (c) break;						\
} while (0)

#endif /* _MINIX_SPIN_H */
