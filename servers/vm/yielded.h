
#ifndef _YIELDED_H 
#define _YIELDED_H 1

#include <minix/type.h>

typedef struct block_id {
	u64_t		id;
	endpoint_t	owner;
} block_id_t;

typedef struct yielded {
	/* the owner-given id and owner together
	 * uniquely identify a yielded block.
	 */
	block_id_t	id;
	phys_bytes	physaddr;
	int		pages;

	/* LRU fields */
	struct yielded	*younger, *older;

	/* AVL fields */
	struct yielded	*less, *greater;
	int		factor;
} yielded_t;

#endif
