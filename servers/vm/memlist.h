
#ifndef _MEMLIST_H
#define _MEMLIST_H 1

struct memlist {
	struct memlist *next;
	phys_bytes	phys;	/* physical address in bytes */
	phys_bytes	length;	/* length in bytes */
};

#endif
