#ifndef __MINIX_BDEV_H
#define __MINIX_BDEV_H

extern void bdev_driver(dev_t dev, char *label);

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

#endif /* __MINIX_BDEV_H */
