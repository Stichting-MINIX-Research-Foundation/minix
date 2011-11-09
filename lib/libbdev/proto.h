#ifndef _BDEV_PROTO_H
#define _BDEV_PROTO_H

/* driver.c */
extern void bdev_driver_init(void);
extern void bdev_driver_clear(dev_t dev);
extern void bdev_driver_set(dev_t dev, endpoint_t endpt);
extern endpoint_t bdev_driver_get(dev_t dev);

/* ipc.c */
extern void bdev_update(dev_t dev, endpoint_t endpt);
extern int bdev_sendrec(dev_t dev, const message *m_orig);

#endif /* _BDEV_PROTO_H */
