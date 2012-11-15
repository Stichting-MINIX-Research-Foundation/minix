/*	$NetBSD: tdelete.c,v 1.6 2012/06/25 22:32:45 abs Exp $	*/

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
__RCSID("$NetBSD: tdelete.c,v 1.6 2012/06/25 22:32:45 abs Exp $");
#endif /* LIBC_SCCS and not lint */

#include <assert.h>
#define _SEARCH_PRIVATE
#include <search.h>
#include <stdlib.h>


/* find a node with key "vkey" in tree "vrootp" */
void *
tdelete(const void *vkey, void **vrootp,
    int (*compar)(const void *, const void *))
{
	node_t **rootp = (node_t **)vrootp;
	node_t *p, *q, *r;
	int  cmp;

	_DIAGASSERT(vkey != NULL);
	_DIAGASSERT(compar != NULL);

	if (rootp == NULL || (p = *rootp) == NULL)
		return NULL;

	while ((cmp = (*compar)(vkey, (*rootp)->key)) != 0) {
		p = *rootp;
		rootp = (cmp < 0) ?
		    &(*rootp)->llink :		/* follow llink branch */
		    &(*rootp)->rlink;		/* follow rlink branch */
		if (*rootp == NULL)
			return NULL;		/* key not found */
	}
	r = (*rootp)->rlink;			/* D1: */
	if ((q = (*rootp)->llink) == NULL)	/* Left NULL? */
		q = r;
	else if (r != NULL) {			/* Right link is NULL? */
		if (r->llink == NULL) {		/* D2: Find successor */
			r->llink = q;
			q = r;
		} else {			/* D3: Find NULL link */
			for (q = r->llink; q->llink != NULL; q = r->llink)
				r = q;
			r->llink = q->rlink;
			q->llink = (*rootp)->llink;
			q->rlink = (*rootp)->rlink;
		}
	}
	if (p != *rootp)
		free(*rootp);			/* D4: Free node */
	*rootp = q;				/* link parent to new node */
	return p;
}
