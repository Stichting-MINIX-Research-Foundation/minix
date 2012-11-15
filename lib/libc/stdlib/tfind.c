/*	$NetBSD: tfind.c,v 1.7 2012/06/25 22:32:45 abs Exp $	*/

/*
 * Tree search generalized from Knuth (6.2.2) Algorithm T just like
 * the AT&T man page says.
 *
 * The node_t structure is for internal use only, lint doesn't grok it.
 *
 * Written by reading the System V Interface Definition, not the code.
 *
 * Totally public domain.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: tfind.c,v 1.7 2012/06/25 22:32:45 abs Exp $");
#endif /* LIBC_SCCS and not lint */

#include <assert.h>
#define _SEARCH_PRIVATE
#include <stdlib.h>
#include <search.h>

/* find a node by key "vkey" in tree "vrootp", or return 0 */
void *
tfind(const void *vkey, void * const *vrootp,
    int (*compar)(const void *, const void *))
{
	node_t * const *rootp = (node_t * const*)vrootp;

	_DIAGASSERT(vkey != NULL);
	_DIAGASSERT(compar != NULL);

	if (rootp == NULL)
		return NULL;

	while (*rootp != NULL) {		/* T1: */
		int r;

		if ((r = (*compar)(vkey, (*rootp)->key)) == 0)	/* T2: */
			return *rootp;		/* key found */
		rootp = (r < 0) ?
		    &(*rootp)->llink :		/* T3: follow left branch */
		    &(*rootp)->rlink;		/* T4: follow right branch */
	}
	return NULL;
}
