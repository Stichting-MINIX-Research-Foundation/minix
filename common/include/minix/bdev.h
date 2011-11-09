#ifndef __MINIX_BDEV_H
#define __MINIX_BDEV_H

#define BDEV_NOFLAGS	0

extern void bdev_driver(dev_t dev, endpoint_t endpt);

extern int bdev_open(dev_t dev, int access);
extern int bdev_close(dev_t dev);

extern int bdev_read(dev_t dev, u64_t pos, char *buf, int count, int flags);
extern int bdev_write(dev_t dev, u64_t pos, char *buf, int count, int flags);
extern int bdev_gather(dev_t dev, u64_t pos, iovec_t *vec, int count,
	int flags, vir_bytes *size);
extern int bdev_scatter(dev_t dev, u64_t pos, iovec_t *vec, int count,
	int flags, vir_bytes *size);
extern int bdev_ioctl(dev_t dev, int request, void *buf);

#endif /* __MINIX_BDEV_H */
