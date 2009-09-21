

#include <minix/callnr.h>
#include <minix/com.h>
#include <minix/config.h>
#include <minix/const.h>
#include <minix/ds.h>
#include <minix/endpoint.h>
#include <minix/keymap.h>
#include <minix/minlib.h>
#include <minix/type.h>
#include <minix/ipc.h>
#include <minix/sysutil.h>
#include <minix/syslib.h>
#include <minix/const.h>

typedef struct pagerange {
	phys_bytes	addr;	/* in pages */
	phys_bytes	size;	/* in pages */

	/* AVL fields */
	struct pagerange *less, *greater;	/* children */
	int		factor;	/* AVL balance factor */
} pagerange_t;
