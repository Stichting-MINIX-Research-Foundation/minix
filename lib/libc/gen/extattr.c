/*	$NetBSD: extattr.c,v 1.4 2012/03/13 21:13:34 christos Exp $	*/

/*-
 * Copyright (c) 2001 Robert N. M. Watson
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * TrustedBSD: Utility functions for extended attributes.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: extattr.c,v 1.4 2012/03/13 21:13:34 christos Exp $");
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"
#include <sys/types.h>
#include <sys/param.h>
#include <sys/extattr.h>

#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

const int extattr_namespaces[] = {
	EXTATTR_NAMESPACE_USER,
	EXTATTR_NAMESPACE_SYSTEM,
	0,
};  

int
extattr_namespace_to_string(int attrnamespace, char **string)
{

	switch(attrnamespace) {
	case EXTATTR_NAMESPACE_USER:
		if (string != NULL) {
			if ((*string =
			     strdup(EXTATTR_NAMESPACE_USER_STRING)) == NULL)
				return (-1);
		}
		return (0);

	case EXTATTR_NAMESPACE_SYSTEM:
		if (string != NULL)
			if ((*string =
			     strdup(EXTATTR_NAMESPACE_SYSTEM_STRING)) == NULL)
				return (-1);
		return (0);

	default:
		errno = EINVAL;
		return (-1);
	}
}

int
extattr_string_to_namespace(const char *string, int *attrnamespace)
{

	if (strcmp(string, EXTATTR_NAMESPACE_USER_STRING) == 0) {
		if (attrnamespace != NULL)
			*attrnamespace = EXTATTR_NAMESPACE_USER;
		return (0);
	} else if (strcmp(string, EXTATTR_NAMESPACE_SYSTEM_STRING) == 0) {
		if (attrnamespace != NULL)
			*attrnamespace = EXTATTR_NAMESPACE_SYSTEM;
		return (0);
	} else {
		errno = EINVAL;
		return (-1);
	}
}


int
extattr_copy_fd(int from_fd, int to_fd, int namespace)
{
	ssize_t llen, vlen, maxvlen;
	size_t alen;
	void *alist = NULL;
	void *aval = NULL;
	size_t i;
	int error = -1;

	llen = extattr_list_fd(from_fd, namespace, NULL, 0);
	if (llen == -1) {
		/* Silently ignore when EA are not supported */
		if (errno == EOPNOTSUPP)
			error = 0;
		goto out;
	}

	if (llen == 0) {
		error = 0;
		goto out;
	}

	if ((alist = malloc((size_t)llen)) == NULL)
		goto out;

	llen = extattr_list_fd(from_fd, namespace, alist, (size_t)llen);
	if (llen == -1)
		goto out;

	maxvlen = 1024;
	if ((aval = malloc((size_t)maxvlen)) == NULL)
		goto out;

	for (i = 0; i < (size_t)llen; i += alen + 1) {
		char aname[NAME_MAX + 1];
		char *ap;

		alen = ((uint8_t *)alist)[i];
		ap = ((char *)alist) + i + 1;
		(void)memcpy(aname, ap, alen);
		aname[alen] = '\0';

		vlen = extattr_get_fd(from_fd, namespace, aname, NULL, 0);
		if (vlen == -1)
			goto out;

		if (vlen > maxvlen) {
			if ((aval = realloc(aval, (size_t)vlen)) == NULL)
				goto out;
			maxvlen = vlen;
		}

		if ((vlen = extattr_get_fd(from_fd, namespace, aname,
				      aval, (size_t)vlen)) == -1)
			goto out;
	
		if (extattr_set_fd(to_fd, namespace, aname,
				   aval, (size_t)vlen) != vlen)
			goto out;
	}

	error = 0;
out:
	if (aval != NULL)
		free(aval);
	
	if (alist != NULL)
		free(alist);
	
	return error;
}

int
extattr_copy_file(const char *from, const char *to, int namespace)
{
	ssize_t llen, vlen, maxvlen;
	size_t alen;
	void *alist = NULL;
	void *aval = NULL;
	size_t i;
	int error = -1;

	llen = extattr_list_file(from, namespace, NULL, 0);
	if (llen == -1) {
		/* Silently ignore when EA are not supported */
		if (errno == EOPNOTSUPP)
			error = 0;
		goto out;
	}

	if (llen == 0) {
		error = 0;
		goto out;
	}

	if ((alist = malloc((size_t)llen)) == NULL)
		goto out;

	llen = extattr_list_file(from, namespace, alist, (size_t)llen);
	if (llen == -1)
		goto out;

	maxvlen = 1024;
	if ((aval = malloc((size_t)maxvlen)) == NULL)
		goto out;

	for (i = 0; i < (size_t)llen; i += alen + 1) {
		char aname[NAME_MAX + 1];
		char *ap;

		alen = ((uint8_t *)alist)[i];
		ap = ((char *)alist) + i + 1;
		(void)memcpy(aname, ap, alen);
		aname[alen] = '\0';

		vlen = extattr_get_file(from, namespace, aname, NULL, 0);
		if (vlen == -1)
			goto out;

		if (vlen > maxvlen) {
			if ((aval = realloc(aval, (size_t)vlen)) == NULL)
				goto out;
			maxvlen = vlen;
		}

		if ((vlen = extattr_get_file(from, namespace, aname,							     aval, (size_t)vlen)) == -1)
			goto out;
	
		if (extattr_set_file(to, namespace, aname,
				     aval, (size_t)vlen) != vlen)
			goto out;
	}

	error = 0;
out:
	if (aval != NULL)
		free(aval);
	
	if (alist != NULL)
		free(alist);
	
	return error;
}

int
extattr_copy_link(const char *from, const char *to, int namespace)
{
	ssize_t llen, vlen, maxvlen;
	size_t alen;
	void *alist = NULL;
	void *aval = NULL;
	size_t i;
	int error = -1;

	llen = extattr_list_link(from, namespace, NULL, 0);
	if (llen == -1) {
		/* Silently ignore when EA are not supported */
		if (errno == EOPNOTSUPP)
			error = 0;
		goto out;
	}

	if (llen == 0) {
		error = 0;
		goto out;
	}

	if ((alist = malloc((size_t)llen)) == NULL)
		goto out;

	llen = extattr_list_link(from, namespace, alist, (size_t)llen);
	if (llen == -1)
		goto out;

	maxvlen = 1024;
	if ((aval = malloc((size_t)maxvlen)) == NULL)
		goto out;

	for (i = 0; i < (size_t)llen; i += alen + 1) {
		char aname[NAME_MAX + 1];
		char *ap;

		alen = ((uint8_t *)alist)[i];
		ap = ((char *)alist) + i + 1;
		(void)memcpy(aname, ap, alen);
		aname[alen] = '\0';

		vlen = extattr_get_link(from, namespace, aname, NULL, 0);
		if (vlen == -1)
			goto out;

		if (vlen > maxvlen) {
			if ((aval = realloc(aval, (size_t)vlen)) == NULL)
				goto out;
			maxvlen = vlen;
		}

		if ((vlen = extattr_get_link(from, namespace, aname,
					     aval, (size_t)vlen)) == -1)
			goto out;
	
		if (extattr_set_link(to, namespace, aname,
				     aval, (size_t)vlen) != vlen)
			goto out;
	}

	error = 0;
out:
	if (aval != NULL)
		free(aval);
	
	if (alist != NULL)
		free(alist);
	
	return error;
}

static int
extattr_namespace_access(int namespace, int mode)
{
	switch (namespace) {
	case EXTATTR_NAMESPACE_SYSTEM:
		if ((mode & (R_OK|W_OK)) && getuid() != 0)
			return -1;
		break;
	default:
		break;
	}
	
	return 0;
}

int
fcpxattr(int from_fd, int to_fd)
{
	const int *ns;
	int error;

	for (ns = extattr_namespaces; *ns; ns++) {
		if (extattr_namespace_access(*ns, R_OK|W_OK) != 0)
			continue;
	
		if ((error = extattr_copy_fd(from_fd, to_fd, *ns)) != 0)
			return error;
	}

	return 0;
}

int
cpxattr(const char *from, const char *to)
{
	const int *ns;
	int error;

	for (ns = extattr_namespaces; *ns; ns++) {
		if (extattr_namespace_access(*ns, R_OK|W_OK) != 0)
			continue;
	
		if ((error = extattr_copy_file(from, to, *ns)) != 0)
			return error;
	}

	return 0;
}

int
lcpxattr(const char *from, const char *to)
{
	const int *ns;
	int error;

	for (ns = extattr_namespaces; *ns; ns++) {
		if (extattr_namespace_access(*ns, R_OK|W_OK) != 0)
			continue;
	
		if ((error = extattr_copy_link(from, to, *ns)) != 0)
			return error;
	}

	return 0;
}
