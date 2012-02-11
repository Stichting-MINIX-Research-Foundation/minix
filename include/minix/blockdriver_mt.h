#ifndef _MINIX_BLOCKDRIVER_MT_H
#define _MINIX_BLOCKDRIVER_MT_H

#define BLOCKDRIVER_MT_API 1	/* do not expose the singlethreaded API */
#include <minix/blockdriver.h>

_PROTOTYPE( void blockdriver_mt_task, (struct blockdriver *driver_tab) );
_PROTOTYPE( void blockdriver_mt_sleep, (void) );
_PROTOTYPE( void blockdriver_mt_wakeup, (thread_id_t id) );
_PROTOTYPE( void blockdriver_mt_terminate, (void) );
_PROTOTYPE( void blockdriver_mt_set_workers, (device_id_t id, int workers) );
_PROTOTYPE( thread_id_t blockdriver_mt_get_tid, (void) );

#endif /* _MINIX_BLOCKDRIVER_MT_H */
