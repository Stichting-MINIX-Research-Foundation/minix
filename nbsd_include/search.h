/*	$NetBSD: search.h,v 1.18 2005/07/06 15:47:15 drochner Exp $	*/

/*
 * Written by J.T. Conklin <jtc@NetBSD.org>
 * Public domain.
 */

#ifndef _SEARCH_H_
#define _SEARCH_H_

#include <sys/cdefs.h>
#include <machine/ansi.h>

#ifdef	_BSD_SIZE_T_
typedef	_BSD_SIZE_T_	size_t;
#undef	_BSD_SIZE_T_
#endif

typedef struct entry {
	char *key;
	void *data;
} ENTRY;

typedef enum {
	FIND, ENTER
} ACTION;

typedef enum {
	preorder,
	postorder,
	endorder,
	leaf
} VISIT;

#ifdef _SEARCH_PRIVATE
typedef struct node {
	char         *key;
	struct node  *llink, *rlink;
} node_t;
#endif

__BEGIN_DECLS
#ifndef __BSEARCH_DECLARED
#define __BSEARCH_DECLARED
/* also in stdlib.h */
void	*bsearch(const void *, const void *, size_t, size_t,
		      int (*)(const void *, const void *));
#endif /* __BSEARCH_DECLARED */
int	 hcreate(size_t);
void	 hdestroy(void);
ENTRY	*hsearch(ENTRY, ACTION);

void	*lfind(const void *, const void *, size_t *, size_t,
		      int (*)(const void *, const void *));
void	*lsearch(const void *, void *, size_t *, size_t,
		      int (*)(const void *, const void *));
void	 insque(void *, void *);
void	 remque(void *);

void	*tdelete(const void * __restrict, void ** __restrict,
		      int (*)(const void *, const void *));
void	*tfind(const void *, void * const *,
		      int (*)(const void *, const void *));
void	*tsearch(const void *, void **, 
		      int (*)(const void *, const void *));
void	 twalk(const void *, void (*)(const void *, VISIT, int));
__END_DECLS

#endif /* !_SEARCH_H_ */
