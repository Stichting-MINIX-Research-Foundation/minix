/*	$NetBSD: tsearch.c,v 1.7 2012/06/25 22:32:45 abs Exp $	*/

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
__RCSID("$NetBSD: tsearch.c,v 1.7 2012/06/25 22:32:45 abs Exp $");
#endif /* LIBC_SCCS and not lint */

#include <assert.h>
#define _SEARCH_PRIVATE
#include <search.h>
#include <stdlib.h>

/* find or insert datum into search tree */
void *
tsearch(const void *vkey, void **vrootp,
    int (*compar)(const void *, const void *))
{
	node_t *q;
	node_t **rootp = (node_t **)vrootp;

	_DIAGASSERT(vkey != NULL);
	_DIAGASSERT(compar != NULL);

	if (rootp == NULL)
		return NULL;

	while (*rootp != NULL) {	/* Knuth's T1: */
		int r;

		if ((r = (*compar)(vkey, (*rootp)->key)) == 0)	/* T2: */
			return *rootp;		/* we found it! */

		rootp = (r < 0) ?
		    &(*rootp)->llink :		/* T3: follow left branch */
		    &(*rootp)->rlink;		/* T4: follow right branch */
	}

	q = malloc(sizeof(node_t));		/* T5: key not found */
	if (q != 0) {				/* make new node */
		*rootp = q;			/* link new node to old */
		q->key = __UNCONST(vkey);	/* initialize new node */
		q->llink = q->rlink = NULL;
	}
	return q;
}
