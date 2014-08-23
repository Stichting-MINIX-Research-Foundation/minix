#ifndef _MINIX_VBOX_H
#define _MINIX_VBOX_H

#include <minix/vboxtype.h>

typedef int vbox_conn_t;

extern int vbox_init(void);

extern vbox_conn_t vbox_open(const char *name);
extern int vbox_close(vbox_conn_t conn);
extern int vbox_call(vbox_conn_t conn, u32_t function, vbox_param_t *param,
	int count, int *code);

extern void vbox_set_u32(vbox_param_t *param, u32_t value);
extern void vbox_set_u64(vbox_param_t *param, u64_t value);
extern void vbox_set_ptr(vbox_param_t *param, void *ptr, size_t size,
	unsigned int dir);
extern void vbox_set_grant(vbox_param_t *param, endpoint_t endpt,
	cp_grant_id_t grant, size_t off, size_t size, unsigned int dir);

extern u32_t vbox_get_u32(vbox_param_t *param);
extern u64_t vbox_get_u64(vbox_param_t *param);

extern void vbox_put(vbox_param_t *param, int count);

#endif /* _MINIX_VBOX_H */
