
#ifndef _YIELDED_H 
#define _YIELDED_H 1

#include <minix/type.h>

typedef struct yielded {
	u64_t		id;
	phys_bytes	addr, len;
	endpoint_t	owner;

	/* LRU fields */
	struct yielded	*younger, *older;

	/* AVL fields */
	struct yielded	*less, *greater;
	int		factor;
} yielded_t;

#endif
