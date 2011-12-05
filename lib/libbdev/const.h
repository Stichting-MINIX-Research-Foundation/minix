#ifndef _BDEV_CONST_H
#define _BDEV_CONST_H

#define NR_CALLS 	256	/* maximum number of concurrent async calls */

#define NO_ID		(-1)	/* ID for synchronous requests */

#define DS_NR_TRIES	100	/* number of times to check endpoint in DS */
#define DS_DELAY	50000	/* delay time (us) between DS checks */

#define DRIVER_TRIES	10	/* after so many tries, give up on a driver */
#define RECOVER_TRIES	2	/* tolerated nr of restarts during recovery */
#define TRANSFER_TRIES	5	/* number of times to try transfers on EIO */

#define NR_OPEN_DEVS	4	/* maximum different opened minor devices */

#endif /* _BDEV_CONST_H */
