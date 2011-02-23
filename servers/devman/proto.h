#ifndef _DEVMAN_PROTO_H
#define _DEVMAN_PROTO_H

/* buf.c */
_PROTOTYPE( void buf_init, (off_t start, size_t len)                    );
_PROTOTYPE( void buf_printf, (char *fmt, ...)                           );
_PROTOTYPE( void buf_append, (char *data, size_t len)                   );
_PROTOTYPE( size_t buf_get, (char **ptr)                                );

/* message handlers */
_PROTOTYPE(int  do_add_device, (message *m));
_PROTOTYPE(int  do_del_device, (message *m));
_PROTOTYPE(int  do_bind_device, (message *m));
_PROTOTYPE(int  do_unbind_device, (message *m));

/* local helper functions */
_PROTOTYPE(void devman_init_devices, ());
_PROTOTYPE(struct devman_device* devman_find_device,(int devid));
_PROTOTYPE(void devman_get_device, (struct devman_device *dev));
_PROTOTYPE(void devman_put_device, (struct devman_device *dev));

#endif /* _DEVMAN_PROTO_H */

