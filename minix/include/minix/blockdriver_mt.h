#ifndef _MINIX_BLOCKDRIVER_MT_H
#define _MINIX_BLOCKDRIVER_MT_H

#define BLOCKDRIVER_MT_API 1	/* do not expose the singlethreaded API */
#include <minix/blockdriver.h>

#define BLOCKDRIVER_MAX_DEVICES		32

void blockdriver_mt_task(struct blockdriver *driver_tab);
void blockdriver_mt_sleep(void);
void blockdriver_mt_wakeup(thread_id_t id);
void blockdriver_mt_terminate(void);
void blockdriver_mt_set_workers(device_id_t id, int workers);
thread_id_t blockdriver_mt_get_tid(void);

#endif /* _MINIX_BLOCKDRIVER_MT_H */
