#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <ohash.h>

void *xalloc(size_t sz, void *arg) { return(calloc(sz,1)); }
void xfree(void *p, size_t sz, void *arg) { free(p); }

int
main(void)
{
	struct ohash h;
	struct ohash_info i;
	i.halloc = i.alloc = xalloc;
	i.hfree = xfree;
	ohash_init(&h, 2, &i);
	ohash_delete(&h);
	return 0;
}
