#ifndef _BLOCKDRIVER_DRIVER_MT_H
#define _BLOCKDRIVER_DRIVER_MT_H

/* These functions are used for live update. */
int blockdriver_mt_is_idle(void);
void blockdriver_mt_suspend(void);
void blockdriver_mt_resume(void);

#endif /* !_BLOCKDRIVER_DRIVER_MT_H */
