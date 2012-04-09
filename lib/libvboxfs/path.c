/* Part of libvboxfs - (c) 2012, D.C. van Moolenbroek */

#include "inc.h"

/*
 * Store a local path name in the given path object, performing any necessary
 * conversions.  The path object is expected to be used read-only, so the size
 * of the path object is set as small as possible.  If 'name' is NULL, the path
 * will be initialized to the empty string.
 */
int
vboxfs_set_path(vboxfs_path_t *path, char *name)
{
	size_t len;

	len = strlen(name);

	/* FIXME: missing UTF-8 conversion */

	if (len >= sizeof(path->data))
		return ENAMETOOLONG;

	strcpy(path->data, name);

	path->len = len;
	path->size = len + 1;

	return OK;
}

/*
 * Retrieve the path name from the given path object.  Make sure the name fits
 * in the given name buffer first.  The given size must include room for a
 * terminating null character.
 */
int
vboxfs_get_path(vboxfs_path_t *path, char *name, size_t size)
{

	/* FIXME: missing UTF-8 conversion */

	if (path->len >= size)
		return ENAMETOOLONG;

	assert(path->data[path->len] == 0);

	strcpy(name, path->data);

	return OK;
}

/*
 * Return the byte size of a previously initialized path object.
 */
size_t
vboxfs_get_path_size(vboxfs_path_t *path)
{

	return offsetof(vboxfs_path_t, data) + path->size;
}
