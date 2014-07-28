#ifndef __LWIP_PROTO_H__
#define __LWIP_PROTO_H__

#include <errno.h>
#include <minix/ipc.h>
#include <minix/endpoint.h>
#include <minix/syslib.h>
#include <minix/safecopies.h>
#include <minix/const.h>

#include <lwip/err.h>
#include <lwip/netif.h>

#if 0
#define debug_print(str, ...) printf("LWIP %s:%d : " str "\n", \
		__func__, __LINE__, ##__VA_ARGS__)
#else
#define debug_print(...)
#endif

/* driver .c */
void nic_assign_driver(const char * dev_type,
			unsigned int dev_num,
			const char * driver_name,
			unsigned int instance,
			int is_default);
void nic_init_all(void);
void driver_request(message * m);
void driver_up(const char * label, endpoint_t ep);
/* opens a raw NIC socket */
int nic_open(devminor_t minor);
int nic_default_ioctl(struct sock_req *req);

/* inet_config.c */
void inet_read_conf(void);

/* eth.c */
err_t ethernetif_init(struct netif *netif);

static inline int copy_from_user(endpoint_t proc,
				void * dst_ptr,
				size_t size,
				cp_grant_id_t gid,
				vir_bytes offset)
{
	return sys_safecopyfrom(proc, gid, offset, (vir_bytes)dst_ptr, size);
}

static inline int copy_to_user(endpoint_t proc,
				void * src_ptr,
				size_t size,
				cp_grant_id_t gid,
				vir_bytes offset)
{
	return sys_safecopyto(proc, gid, offset, (vir_bytes)src_ptr, size);
}

#endif /* __LWIP_PROTO_H__ */
