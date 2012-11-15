/*	$NetBSD: sysctlgetmibinfo.c,v 1.10 2012/03/13 21:13:37 christos Exp $ */

/*-
 * Copyright (c) 2003,2004 The NetBSD Foundation, Inc.
 *	All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Andrew Brown.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: sysctlgetmibinfo.c,v 1.10 2012/03/13 21:13:37 christos Exp $");
#endif /* LIBC_SCCS and not lint */

#ifndef RUMP_ACTION
#include "namespace.h"
#ifdef _REENTRANT
#include "reentrant.h"
#endif /* _REENTRANT */
#endif /* RUMP_ACTION */
#include <sys/param.h>
#include <sys/sysctl.h>

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#ifdef RUMP_ACTION
#include <rump/rump_syscalls.h>
#define sysctl(a,b,c,d,e,f) rump_sys___sysctl(a,b,c,d,e,f)
#else
#ifdef __weak_alias
__weak_alias(__learn_tree,___learn_tree)
__weak_alias(sysctlgetmibinfo,_sysctlgetmibinfo)
#endif
#endif

/*
 * the place where we attach stuff we learn on the fly, not
 * necessarily used.
 */
static struct sysctlnode sysctl_mibroot = {
#if defined(lint)
	/*
	 * lint doesn't like my initializers
	 */
	0
#else /* !lint */
	.sysctl_flags = SYSCTL_VERSION|CTLFLAG_ROOT|CTLTYPE_NODE,
	sysc_init_field(_sysctl_size, sizeof(struct sysctlnode)),
	.sysctl_name = "(root)",
#endif /* !lint */
};

/*
 * routines to handle learning and cleanup
 */
static int compar(const void *, const void *);
static void free_children(struct sysctlnode *);
static void relearnhead(void);

/*
 * specifically not static since sysctl(8) "borrows" it.
 */
int __learn_tree(int *, u_int, struct sysctlnode *);

/*
 * for ordering nodes -- a query may or may not be given them in
 * numeric order
 */
static int
compar(const void *a, const void *b)
{

	return (((const struct sysctlnode *)a)->sysctl_num -
		((const struct sysctlnode *)b)->sysctl_num);
}

/*
 * recursively nukes a branch or an entire tree from the given node
 */
static void
free_children(struct sysctlnode *rnode) 
{
	struct sysctlnode *node;

	if (rnode == NULL ||
	    SYSCTL_TYPE(rnode->sysctl_flags) != CTLTYPE_NODE ||
	    rnode->sysctl_child == NULL)
		return;

	for (node = rnode->sysctl_child;
	     node < &rnode->sysctl_child[rnode->sysctl_clen];
	     node++) {
		free_children(node);
	}
	free(rnode->sysctl_child);
	rnode->sysctl_child = NULL;
}

/*
 * verifies that the head of the tree in the kernel is the same as the
 * head of the tree we already got, integrating new stuff and removing
 * old stuff, if it's not.
 */
static void
relearnhead(void)
{
	struct sysctlnode *h, *i, *o, qnode;
	size_t si, so;
	int rc, name;
	size_t nlen, olen, ni, oi;
	uint32_t t;

	/*
	 * if there's nothing there, there's no need to expend any
	 * effort
	 */
	if (sysctl_mibroot.sysctl_child == NULL)
		return;

	/*
	 * attempt to pull out the head of the tree, starting with the
	 * size we have now, and looping if we need more (or less)
	 * space
	 */
	si = 0;
	so = sysctl_mibroot.sysctl_clen * sizeof(struct sysctlnode);
	name = CTL_QUERY;
	memset(&qnode, 0, sizeof(qnode));
	qnode.sysctl_flags = SYSCTL_VERSION;
	do {
		si = so;
		h = malloc(si);
		rc = sysctl(&name, 1, h, &so, &qnode, sizeof(qnode));
		if (rc == -1 && errno != ENOMEM)
			return;
		if (si < so)
			free(h);
	} while (si < so);

	/*
	 * order the new copy of the head
	 */
	nlen = so / sizeof(struct sysctlnode);
	qsort(h, nlen, sizeof(struct sysctlnode), compar);

	/*
	 * verify that everything is the same.  if it is, we don't
	 * need to do any more work here.
	 */
	olen = sysctl_mibroot.sysctl_clen;
	rc = (nlen == olen) ? 0 : 1;
	o = sysctl_mibroot.sysctl_child;
	for (ni = 0; rc == 0 && ni < nlen; ni++) {
		if (h[ni].sysctl_num != o[ni].sysctl_num ||
		    h[ni].sysctl_ver != o[ni].sysctl_ver)
			rc = 1;
	}
	if (rc == 0) {
		free(h);
		return;
	}

	/*
	 * something changed.  h will become the new head, and we need
	 * pull over any subtrees we already have if they're the same
	 * version.
	 */
	i = h;
	ni = oi = 0;
	while (ni < nlen && oi < olen) {
		/*
		 * something was inserted or deleted
		 */
		if (SYSCTL_TYPE(i[ni].sysctl_flags) == CTLTYPE_NODE)
			i[ni].sysctl_child = NULL;
		if (i[ni].sysctl_num != o[oi].sysctl_num) {
			if (i[ni].sysctl_num < o[oi].sysctl_num) {
				ni++;
			}
			else {
				free_children(&o[oi]);
				oi++;
			}
			continue;
		}

		/*
		 * same number, but different version, so throw away
		 * any accumulated children
		 */
		if (i[ni].sysctl_ver != o[oi].sysctl_ver)
			free_children(&o[oi]);

		/*
		 * this node is the same, but we only need to
		 * move subtrees.
		 */
		else if (SYSCTL_TYPE(i[ni].sysctl_flags) == CTLTYPE_NODE) {	
			/*
			 * move subtree to new parent
			 */
			i[ni].sysctl_clen = o[oi].sysctl_clen;
			i[ni].sysctl_csize = o[oi].sysctl_csize;
			i[ni].sysctl_child = o[oi].sysctl_child;
			/*
			 * reparent inherited subtree
			 */
			for (t = 0;
			     i[ni].sysctl_child != NULL &&
				     t < i[ni].sysctl_clen;
			     t++)
				i[ni].sysctl_child[t].sysctl_parent = &i[ni];
		}
		ni++;
		oi++;
	}

	/*
	 * left over new nodes need to have empty subtrees cleared
	 */
	while (ni < nlen) {
		if (SYSCTL_TYPE(i[ni].sysctl_flags) == CTLTYPE_NODE)
			i[ni].sysctl_child = NULL;
		ni++;
	}

	/*
	 * left over old nodes need to be cleaned out
	 */
	while (oi < olen) {
		free_children(&o[oi]);
		oi++;
	}

	/*
	 * pop new head in
	 */
	_DIAGASSERT(__type_fit(uint32_t, nlen));
	sysctl_mibroot.sysctl_csize =
	    sysctl_mibroot.sysctl_clen = (uint32_t)nlen;
	sysctl_mibroot.sysctl_child = h;
	free(o);
}

/*
 * sucks in the children at a given level and attaches it to the tree.
 */
int
__learn_tree(int *name, u_int namelen, struct sysctlnode *pnode)
{
	struct sysctlnode qnode;
	uint32_t rc;
	size_t sz;

	if (pnode == NULL)
		pnode = &sysctl_mibroot;
	if (SYSCTL_TYPE(pnode->sysctl_flags) != CTLTYPE_NODE) {
		errno = EINVAL;
		return (-1);
	}
	if (pnode->sysctl_child != NULL)
		return (0);

	if (pnode->sysctl_clen == 0)
		sz = SYSCTL_DEFSIZE * sizeof(struct sysctlnode);
	else
		sz = pnode->sysctl_clen * sizeof(struct sysctlnode);
	pnode->sysctl_child = malloc(sz);
	if (pnode->sysctl_child == NULL)
		return (-1);

	name[namelen] = CTL_QUERY;
	pnode->sysctl_clen = 0;
	pnode->sysctl_csize = 0;
	memset(&qnode, 0, sizeof(qnode));
	qnode.sysctl_flags = SYSCTL_VERSION;
	rc = sysctl(name, namelen + 1, pnode->sysctl_child, &sz,
		    &qnode, sizeof(qnode));
	if (sz == 0) {
		free(pnode->sysctl_child);
		pnode->sysctl_child = NULL;
		return (rc);
	}
	if (rc) {
		free(pnode->sysctl_child);
		pnode->sysctl_child = NULL;
		if ((sz % sizeof(struct sysctlnode)) != 0)
			errno = EINVAL;
		if (errno != ENOMEM)
			return (rc);
	}

	if (pnode->sysctl_child == NULL) {
		pnode->sysctl_child = malloc(sz);
		if (pnode->sysctl_child == NULL)
			return (-1);

		rc = sysctl(name, namelen + 1, pnode->sysctl_child, &sz,
			    &qnode, sizeof(qnode));
		if (rc) {
			free(pnode->sysctl_child);
			pnode->sysctl_child = NULL;
			return (rc);
		}
	}

	/*
	 * how many did we get?
	 */
	sz /= sizeof(struct sysctlnode);
	pnode->sysctl_csize = pnode->sysctl_clen = (uint32_t)sz;
	if (pnode->sysctl_clen != sz) {
		free(pnode->sysctl_child);
		pnode->sysctl_child = NULL;
		errno = EINVAL;
		return (-1);
	}

	/*
	 * you know, the kernel doesn't really keep them in any
	 * particular order...just like entries in a directory
	 */
	qsort(pnode->sysctl_child, pnode->sysctl_clen, 
	    sizeof(struct sysctlnode), compar);

	/*
	 * rearrange parent<->child linkage
	 */
	for (rc = 0; rc < pnode->sysctl_clen; rc++) {
		pnode->sysctl_child[rc].sysctl_parent = pnode;
		if (SYSCTL_TYPE(pnode->sysctl_child[rc].sysctl_flags) ==
		    CTLTYPE_NODE) {
			/*
			 * these nodes may have children, but we
			 * haven't discovered that yet.
			 */
			pnode->sysctl_child[rc].sysctl_child = NULL;
		}
		pnode->sysctl_child[rc].sysctl_desc = NULL;
	}

	return (0);
}

/*
 * that's "given name" as a string, the integer form of the name fit
 * to be passed to sysctl(), "canonicalized name" (optional), and a
 * pointer to the length of the integer form.  oh, and then a pointer
 * to the node, in case you (the caller) care.  you can leave them all
 * NULL except for gname, though that might be rather pointless,
 * unless all you wanna do is verify that a given name is acceptable.
 *
 * returns either 0 (everything was fine) or -1 and sets errno
 * accordingly.  if errno is set to EAGAIN, we detected a change to
 * the mib while parsing, and you should try again.  in the case of an
 * invalid node name, cname will be set to contain the offending name.
 */
#ifdef _REENTRANT
static mutex_t sysctl_mutex = MUTEX_INITIALIZER;
static int sysctlgetmibinfo_unlocked(const char *, int *, u_int *, char *,
				     size_t *, struct sysctlnode **, int);
#endif /* __REENTRANT */

int
sysctlgetmibinfo(const char *gname, int *iname, u_int *namelenp,
		 char *cname, size_t *csz, struct sysctlnode **rnode, int v)
#ifdef _REENTRANT
{
	int rc;

	mutex_lock(&sysctl_mutex);
	rc = sysctlgetmibinfo_unlocked(gname, iname, namelenp, cname, csz,
				       rnode, v);
	mutex_unlock(&sysctl_mutex);

	return (rc);
}

static int
sysctlgetmibinfo_unlocked(const char *gname, int *iname, u_int *namelenp,
			  char *cname, size_t *csz, struct sysctlnode **rnode,
			  int v)
#endif /* _REENTRANT */
{
	struct sysctlnode *pnode, *node;
	int name[CTL_MAXNAME], n, haven;
	u_int ni, nl;
	intmax_t q;
	char sep[2], token[SYSCTL_NAMELEN],
		pname[SYSCTL_NAMELEN * CTL_MAXNAME + CTL_MAXNAME];
	const char *piece, *dot;
	char *t;
	size_t l;

	if (rnode != NULL) {
		if (*rnode == NULL) {
			/* XXX later deal with dealing back a sub version */
			if (v != SYSCTL_VERSION)
				return (EINVAL);

			pnode = &sysctl_mibroot;
		}
		else {
			/* this is just someone being silly */
			if (SYSCTL_VERS((*rnode)->sysctl_flags) != (uint32_t)v)
				return (EINVAL);

			/* XXX later deal with other people's trees */
			if (SYSCTL_VERS((*rnode)->sysctl_flags) !=
			    SYSCTL_VERSION)
				return (EINVAL);

			pnode = *rnode;
		}
	}
	else
		pnode = &sysctl_mibroot;

	if (pnode == &sysctl_mibroot)
		relearnhead();

	nl = ni = 0;
	token[0] = '\0';
	pname[0] = '\0';
	node = NULL;

	/*
	 * default to using '.' as the separator, but allow '/' as
	 * well, and then allow a leading separator
	 */
	if ((dot = strpbrk(gname, "./")) == NULL)
		sep[0] = '.';
	else
		sep[0] = dot[0];
	sep[1] = '\0';
	if (gname[0] == sep[0]) {
		strlcat(pname, sep, sizeof(pname));
		gname++;
	}

#define COPY_OUT_DATA(t, c, cs, nlp, l) do {			\
		if ((c) != NULL && (cs) != NULL)		\
			*(cs) = strlcpy((c), (t), *(cs));	\
		else if ((cs) != NULL)				\
			*(cs) = strlen(t) + 1;			\
		if ((nlp) != NULL)				\
			*(nlp) = (l);				\
	} while (/*CONSTCOND*/0)

	piece = gname;
	while (piece != NULL && *piece != '\0') {
		/*
		 * what was i looking for?
		 */
		dot = strchr(piece, sep[0]);
		if (dot == NULL) {
			l = strlcpy(token, piece, sizeof(token));
			if (l > sizeof(token)) {
				COPY_OUT_DATA(piece, cname, csz, namelenp, nl);
				errno = ENAMETOOLONG;
				return (-1);
			}
		}
		else if (dot - piece > (intptr_t)(sizeof(token) - 1)) {
			COPY_OUT_DATA(token, cname, csz, namelenp, nl);
			errno = ENAMETOOLONG;
			return (-1);
		}
		else {
			strncpy(token, piece, (size_t)(dot - piece));
			token[dot - piece] = '\0';
		}

		/*
		 * i wonder if this "token" is an integer?
		 */
		errno = 0;
		q = strtoimax(token, &t, 0);
		n = (int)q;
		if (errno != 0 || *t != '\0')
			haven = 0;
		else if (q < INT_MIN || q > UINT_MAX)
			haven = 0;
		else
			haven = 1;

		/*
		 * make sure i have something to look at
		 */
		if (SYSCTL_TYPE(pnode->sysctl_flags) != CTLTYPE_NODE) {
			if (haven && nl > 0) {
				strlcat(pname, sep, sizeof(pname));
				goto just_numbers;
			}
			COPY_OUT_DATA(token, cname, csz, namelenp, nl);
			errno = ENOTDIR;
			return (-1);
		}
		if (pnode->sysctl_child == NULL) {
			if (__learn_tree(name, nl, pnode) == -1) {
				COPY_OUT_DATA(token, cname, csz, namelenp, nl);
				return (-1);
			}
		}
		node = pnode->sysctl_child;
		if (node == NULL) {
			COPY_OUT_DATA(token, cname, csz, namelenp, nl);
			errno = ENOENT;
			return (-1);
		}

		/*
		 * now...is it there?
		 */
		for (ni = 0; ni < pnode->sysctl_clen; ni++)
			if ((haven && ((n == node[ni].sysctl_num) ||
			    (node[ni].sysctl_flags & CTLFLAG_ANYNUMBER))) ||
			    strcmp(token, node[ni].sysctl_name) == 0)
				break;
		if (ni >= pnode->sysctl_clen) {
			COPY_OUT_DATA(token, cname, csz, namelenp, nl);
			errno = ENOENT;
			return (-1);
		}

		/*
		 * ah...it is.
		 */
		pnode = &node[ni];
		if (nl > 0)
			strlcat(pname, sep, sizeof(pname));
		if (haven && n != pnode->sysctl_num) {
 just_numbers:
			strlcat(pname, token, sizeof(pname));
			name[nl] = n;
		}
		else {
			strlcat(pname, pnode->sysctl_name, sizeof(pname));
			name[nl] = pnode->sysctl_num;
		}
		piece = (dot != NULL) ? dot + 1 : NULL;
		nl++;
		if (nl == CTL_MAXNAME) {
			COPY_OUT_DATA(token, cname, csz, namelenp, nl);
			errno = ERANGE;
			return (-1);
		}
	}

	if (nl == 0) {
		if (namelenp != NULL)
			*namelenp = 0;
		errno = EINVAL;
		return (-1);
	}

	COPY_OUT_DATA(pname, cname, csz, namelenp, nl);
	if (iname != NULL && namelenp != NULL)
		memcpy(iname, &name[0], MIN(nl, *namelenp) * sizeof(int));
	if (namelenp != NULL)
		*namelenp = nl;
	if (rnode != NULL) {
		if (*rnode != NULL)
			/*
			 * they gave us a private tree to work in, so
			 * we give back a pointer into that private
			 * tree
			 */
			*rnode = pnode;
		else {
			/*
			 * they gave us a place to put the node data,
			 * so give them a copy
			 */
			*rnode = malloc(sizeof(struct sysctlnode));
			if (*rnode != NULL) {
				**rnode = *pnode;
				(*rnode)->sysctl_child = NULL;
				(*rnode)->sysctl_parent = NULL;
			}
		}
	}

	return (0);
}
