#ifndef _MINIX_DRIVER_MT_H
#define _MINIX_DRIVER_MT_H

#define DRIVER_MT_API 1		/* do not expose the singlethreaded API */
#include <minix/driver.h>

/* The maximum number of worker threads. */
#define DRIVER_MT_MAX_WORKERS	32

_PROTOTYPE( void driver_mt_task, (struct driver *driver_p, int driver_type) );
_PROTOTYPE( void driver_mt_sleep, (void) );
_PROTOTYPE( void driver_mt_wakeup, (thread_id_t id) );
_PROTOTYPE( void driver_mt_stop, (void) );
_PROTOTYPE( void driver_mt_terminate, (void) );

#endif /* _MINIX_DRIVER_MT_H */
