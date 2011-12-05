#ifndef _MINIX_BDEV_H
#define _MINIX_BDEV_H

/* Common API. */
extern void bdev_driver(dev_t dev, char *label);

/* Synchronous API. */
extern int bdev_open(dev_t dev, int access);
extern int bdev_close(dev_t dev);

extern ssize_t bdev_read(dev_t dev, u64_t pos, char *buf, size_t count,
	int flags);
extern ssize_t bdev_write(dev_t dev, u64_t pos, char *buf, size_t count,
	int flags);
extern ssize_t bdev_gather(dev_t dev, u64_t pos, iovec_t *vec, int count,
	int flags);
extern ssize_t bdev_scatter(dev_t dev, u64_t pos, iovec_t *vec, int count,
	int flags);
extern int bdev_ioctl(dev_t dev, int request, void *buf);

/* Asynchronous API. */
typedef int bdev_id_t;
typedef void *bdev_param_t;

typedef void (*bdev_callback_t)(dev_t dev, bdev_id_t id, bdev_param_t param,
	int result);

extern void bdev_flush_asyn(dev_t dev);

extern bdev_id_t bdev_read_asyn(dev_t dev, u64_t pos, char *buf, size_t count,
	int flags, bdev_callback_t callback, bdev_param_t param);
extern bdev_id_t bdev_write_asyn(dev_t dev, u64_t pos, char *buf, size_t count,
	int flags, bdev_callback_t callback, bdev_param_t param);
extern bdev_id_t bdev_gather_asyn(dev_t dev, u64_t pos, iovec_t *vec,
	int count, int flags, bdev_callback_t callback, bdev_param_t param);
extern bdev_id_t bdev_scatter_asyn(dev_t dev, u64_t pos, iovec_t *vec,
	int count, int flags, bdev_callback_t callback, bdev_param_t param);
extern bdev_id_t bdev_ioctl_asyn(dev_t dev, int request, void *buf,
	bdev_callback_t callback, bdev_param_t param);

extern int bdev_wait_asyn(bdev_id_t id);

extern void bdev_reply_asyn(message *m);

#endif /* _MINIX_BDEV_H */
