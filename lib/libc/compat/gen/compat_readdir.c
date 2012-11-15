/*	$NetBSD: compat_readdir.c,v 1.3 2012/02/08 12:10:17 mbalmer Exp $	*/

#define __LIBC12_SOURCE__
#include "namespace.h"
#include <sys/param.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <compat/include/dirent.h>

#ifdef __weak_alias
__weak_alias(readdir,_readdir)
__weak_alias(readdir_r,_readdir_r)
#endif

#ifdef __warn_references
__warn_references(readdir,
    "warning: reference to compatibility readdir(); include <dirent.h> for correct reference")
__warn_references(readdir_r,
    "warning: reference to compatibility readdir_r(); include <dirent.h> for correct reference")
#endif

static struct dirent12 *
direnttodirent12(struct dirent12 *d12, const struct dirent *d)
{
	if (d == NULL)
		return NULL;

	if (d->d_fileno > UINT_MAX || d->d_namlen >= sizeof(d12->d_name)) {
		errno = ERANGE;
		return NULL;
	}
	d12->d_fileno = (uint32_t)d->d_fileno;
	d12->d_reclen = (uint16_t)d->d_reclen;
	d12->d_namlen = (uint8_t)MIN(d->d_namlen, sizeof(d->d_name) - 1);
	d12->d_type = (uint8_t)d->d_type;
	memcpy(d12->d_name, d->d_name, (size_t)d12->d_namlen);
	d12->d_name[d12->d_namlen] = '\0';
	return d12;
}

struct dirent12 *
readdir(DIR *dirp)
{
	static struct dirent12 d12;
	return direnttodirent12(&d12, __readdir30(dirp));
}

int
readdir_r(DIR *dirp, struct dirent12 *entry, struct dirent12 **result)
{
	int error;
	struct dirent e, *ep;

	if ((error = __readdir_r30(dirp, &e, &ep)) != 0)
		return error;

	*result = entry;
	(void)direnttodirent12(entry, &e);
	return 0;
}
