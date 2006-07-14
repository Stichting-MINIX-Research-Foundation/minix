/* Prototypes and definitions for DS interface. */

#ifndef _MINIX_DS_H
#define _MINIX_DS_H

#include <sys/types.h>

/* DS Flag values. */
#define DS_IN_USE       0x0001	/* Internal use only. */
#define DS_PUBLIC       0x0002	/* Publically retrievable value. */
#define DS_INITIAL      0x0004	/* On subscription, send initial contents. */

/* These type flags are mutually exclusive. Give as args to ds_subscribe. */
#define DS_TYPE_U32     0x0100
#define DS_TYPE_STR     0x0200
#define DS_TYPE_MASK    0xff00	/* All type info. */

/* DS constants. */
#define DS_MAX_KEYLEN 80        /* Max length for a key, including '\0'. */
#define DS_MAX_VALLEN 100	/* Max legnth for a str value, incl '\0'. */ 

/* ds.c */
_PROTOTYPE( int ds_subscribe, (char *name_regexp, int type, int flags));

/* publish/update item */
_PROTOTYPE( int ds_publish_u32, (char *name, u32_t val));
_PROTOTYPE( int ds_publish_str, (char *name, char *val));

/* retrieve item by name + type */
_PROTOTYPE( int ds_retrieve_u32, (char *name, u32_t *val)          );
_PROTOTYPE( int ds_retrieve_str, (char *name, char *val, size_t len));

/* retrieve updates for item */
_PROTOTYPE( int ds_check_u32, (char *n, size_t namelen, u32_t *val));
_PROTOTYPE( int ds_check_str, (char *n, size_t namelen, char *v, size_t vlen));

#endif /* _MINIX_DS_H */

