/* Prototypes and definitions for DS interface. */

#ifndef _MINIX_DS_H
#define _MINIX_DS_H

#include <sys/types.h>
#include <minix/endpoint.h>

/* Flags. */
#define DSF_IN_USE		0x001	/* entry is in use */
#define DSF_PRIV_RETRIEVE	0x002	/* only owner can retrieve */
#define DSF_PRIV_OVERWRITE	0x004	/* only owner can overwrite */
#define DSF_PRIV_SNAPSHOT	0x004	/* only owner can take a snapshot */
#define DSF_PRIV_SUBSCRIBE	0x008	/* only owner can subscribe */
#define DSF_TYPE_U32		0x010	/* u32 data type */
#define DSF_TYPE_STR		0x020	/* string data type */
#define DSF_TYPE_MEM		0x040	/* memory range data type */
#define DSF_TYPE_LABEL		0x100	/* label data type */

#define DSF_MASK_TYPE		0xFF0	/* mask for type flags. */
#define DSF_MASK_INTERNAL	0xFFF	/* mask for internal flags. */

#define DSF_OVERWRITE		0x01000	/* overwrite if entry exists */
#define DSF_INITIAL		0x02000	/* check subscriptions immediately */

#define DSMF_COPY_MAPPED	0x20000	/* copy mapped memory range */
#define DSMF_COPY_SNAPSHOT	0x40000	/* copy snapshot */

/* DS constants. */
#define DS_MAX_KEYLEN 80        /* Max length of a key, including '\0'. */

/* DS events. */
#define DS_DRIVER_UP		1

/* ds.c */

/* U32 */
int ds_publish_u32(const char *name, u32_t val, int flags);
int ds_retrieve_u32(const char *name, u32_t *val);
int ds_delete_u32(const char *ds_name);

/* STRING */
int ds_publish_str(const char *name, char *val, int flags);
int ds_retrieve_str(const char *name, char *val, size_t len);
int ds_delete_str(const char *ds_name);

/* MEM */
int ds_publish_mem(const char *ds_name, void *vaddr, size_t length, int
	flags);
int ds_retrieve_mem(const char *ds_name, char *vaddr, size_t *length);
int ds_delete_mem(const char *ds_name);

/* MAP */
int ds_publish_map(const char *ds_name, void *vaddr, size_t length, int
	flags);
int ds_snapshot_map(const char *ds_name, int *nr_snapshot);
int ds_retrieve_map(const char *ds_name, char *vaddr, size_t *length,
	int nr_snapshot, int flags);
int ds_delete_map(const char *ds_name);

/* LABEL */
int ds_publish_label(const char *ds_name, endpoint_t endpoint, int
	flags);
int ds_retrieve_label_name(char *ds_name, endpoint_t endpoint);
int ds_retrieve_label_endpt(const char *ds_name, endpoint_t *endpoint);
int ds_delete_label(const char *ds_name);

/* Subscribe and check. */
int ds_subscribe(const char *regex, int flags);
int ds_check(char *ds_name, int *type, endpoint_t *owner_e);

#endif /* _MINIX_DS_H */

