#ifndef _BDEV_TYPE_H
#define _BDEV_TYPE_H

typedef struct {
  bdev_id_t		id;		/* call ID */
  dev_t			dev;		/* target device number */
  message		msg;		/* request message */
  bdev_callback_t	callback;	/* callback function */
  bdev_param_t		param;		/* callback parameter */
  int			driver_tries;	/* times retried on driver restarts */
  int			transfer_tries;	/* times retried on transfer errors */
  iovec_t		*vec;		/* original vector */
  iovec_s_t		gvec[1];	/* grant vector */
} bdev_call_t;

#endif /* _BDEV_TYPE_H */
