#ifndef _BDEV_PROTO_H
#define _BDEV_PROTO_H

/* bdev.c */
extern void bdev_callback_asyn(bdev_call_t *call, int result);
extern int bdev_restart_asyn(bdev_call_t *call);

/* driver.c */
extern void bdev_driver_init(void);
extern void bdev_driver_clear(dev_t dev);
extern endpoint_t bdev_driver_set(dev_t dev, char *label);
extern endpoint_t bdev_driver_get(dev_t dev);
extern endpoint_t bdev_driver_update(dev_t dev);

/* call.c */
extern bdev_call_t *bdev_call_alloc(int count);
extern void bdev_call_free(bdev_call_t *call);
extern bdev_call_t *bdev_call_get(bdev_id_t id);
extern bdev_call_t *bdev_call_find(dev_t dev);
extern bdev_call_t *bdev_call_iter_maj(dev_t dev, bdev_call_t *last,
  bdev_call_t **next);

/* ipc.c */
extern void bdev_update(dev_t dev, char *label);
extern int bdev_senda(dev_t dev, const message *m_orig, bdev_id_t num);
extern int bdev_sendrec(dev_t dev, const message *m_orig);

/* minor.c */
extern int bdev_minor_reopen(dev_t dev);
extern void bdev_minor_add(dev_t dev, int access);
extern void bdev_minor_del(dev_t dev);

#endif /* _BDEV_PROTO_H */
