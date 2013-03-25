#ifndef _BLOCKDRIVER_CONST_H
#define _BLOCKDRIVER_CONST_H

/* Thread stack size. */
#define STACK_SIZE	8192

/* Maximum number of devices supported. */
#define MAX_DEVICES	BLOCKDRIVER_MAX_DEVICES

/* The maximum number of worker threads per device. */
#define MAX_WORKERS	32

#define MAX_THREADS	(MAX_DEVICES * MAX_WORKERS)	/* max nr of threads */
#define MAIN_THREAD	(MAX_THREADS)			/* main thread ID */
#define SINGLE_THREAD	(0)				/* single-thread ID */

#endif /* _BLOCKDRIVER_CONST_H */
