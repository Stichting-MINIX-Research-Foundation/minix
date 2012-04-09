#ifndef _MINIX_VBOXTYPE_H
#define _MINIX_VBOXTYPE_H

/* This header declares the type definitions shared between VBOX driver, the
 * interface in libsys, and any caller of those interface functions.
 */

/* Call parameter type. */
typedef enum {
  VBOX_TYPE_INVALID,		/* invalid type */
  VBOX_TYPE_U32,		/* 32-bit value */
  VBOX_TYPE_U64,		/* 64-bit value */
  VBOX_TYPE_PTR			/* pointer to granted memory area */
} vbox_type_t;

/* Call parameter transfer direction. */
#define VBOX_DIR_IN	0x01	/* from host to guest */
#define VBOX_DIR_OUT	0x02	/* from guest to host */
#define VBOX_DIR_INOUT	(VBOX_DIR_IN | VBOX_DIR_OUT)

/* Call parameter. */
typedef struct {
  vbox_type_t type;
  union {
	u32_t u32;
	u64_t u64;
	struct {
		cp_grant_id_t grant;
		size_t off;
		size_t size;
		unsigned int dir;
	} ptr;
  };
} vbox_param_t;

#endif /*_MINIX_VBOXTYPE_H */
