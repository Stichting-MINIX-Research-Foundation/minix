#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <string.h>
#include <sys/mount.h>
#include <stdlib.h>
#include <stdio.h>

int __mount50(const char *type, const char *dir, int flags, void *data,
	size_t len)
{
	errno = ENOTSUP;
	return -1;
}
