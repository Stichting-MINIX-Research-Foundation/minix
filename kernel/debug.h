#ifndef DEBUG_H
#define DEBUG_H

#include <minix/config.h>

#if ENABLE_LOCK_TIMING
_PROTOTYPE( void timer_start, (int cat, char *name) );
_PROTOTYPE( void timer_end, (int cat) );
#endif

#if ENABLE_K_DEBUGGING 					/* debugging */
_PROTOTYPE( void check_runqueues, (char *when) );
#endif

#endif	/* DEBUG_H */
