#ifndef _DEVMAN_PROTO_H
#define _DEVMAN_PROTO_H

/* buf.c */
void buf_init(off_t start, size_t len);
void buf_printf(char *fmt, ...);
void buf_append(char *data, size_t len);
size_t buf_get(char **ptr);

/* message handlers */
int do_add_device(message *m);
int do_del_device(message *m);
int do_bind_device(message *m);
int do_unbind_device(message *m);

/* local helper functions */
void devman_init_devices();
struct devman_device* devman_find_device(int devid);
void devman_get_device(struct devman_device *dev);
void devman_put_device(struct devman_device *dev);

#endif /* _DEVMAN_PROTO_H */

