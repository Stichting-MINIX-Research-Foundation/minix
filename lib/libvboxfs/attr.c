/* Part of libvboxfs - (c) 2012, D.C. van Moolenbroek */

#include "inc.h"

/*
 * Convert a VirtualBox timestamp to a POSIX timespec structure.
 * VirtualBox' timestamps are in nanoseconds since the UNIX epoch.
 */
static void
get_time(struct timespec *tsp, u64_t nsecs)
{

	tsp->tv_sec = div64u(nsecs, 1000000000);
	tsp->tv_nsec = rem64u(nsecs, 1000000000);
}

/*
 * Convert a POSIX timespec structure to a VirtualBox timestamp.
 */
static u64_t
set_time(struct timespec *tsp)
{

	return add64u(mul64u(tsp->tv_sec, 1000000000), tsp->tv_nsec);
}

/*
 * Fill the given attribute structure with VirtualBox object information.
 */
void
vboxfs_get_attr(struct sffs_attr *attr, vboxfs_objinfo_t *info)
{

	if (attr->a_mask & SFFS_ATTR_SIZE)
		attr->a_size = info->size;
	if (attr->a_mask & SFFS_ATTR_MODE)
		attr->a_mode = VBOXFS_GET_MODE(info->attr.mode);
	if (attr->a_mask & SFFS_ATTR_ATIME)
		get_time(&attr->a_atime, info->atime);
	if (attr->a_mask & SFFS_ATTR_MTIME)
		get_time(&attr->a_mtime, info->mtime);
	if (attr->a_mask & SFFS_ATTR_CTIME)
		get_time(&attr->a_ctime, info->ctime);
	if (attr->a_mask & SFFS_ATTR_CRTIME)
		get_time(&attr->a_crtime, info->crtime);
}

/*
 * Get file attributes.
 */
int
vboxfs_getattr(char *path, struct sffs_attr *attr)
{
	vbox_param_t param[3];
	vboxfs_path_t pathbuf;
	vboxfs_crinfo_t crinfo;
	int r;

	if ((r = vboxfs_set_path(&pathbuf, path)) != OK)
		return r;

	memset(&crinfo, 0, sizeof(crinfo));
	crinfo.flags = VBOXFS_CRFLAG_LOOKUP;
	/* crinfo.info.attr.add is not checked */

	vbox_set_u32(&param[0], vboxfs_root);
	vbox_set_ptr(&param[1], &pathbuf, vboxfs_get_path_size(&pathbuf),
	    VBOX_DIR_OUT);
	vbox_set_ptr(&param[2], &crinfo, sizeof(crinfo), VBOX_DIR_INOUT);

	r = vbox_call(vboxfs_conn, VBOXFS_CALL_CREATE, param, 3, NULL);
	if (r != OK)
		return r;

	switch (crinfo.result) {
	case VBOXFS_PATH_NOT_FOUND:
		/* This could also be ENOTDIR. See note in handle.c. */
	case VBOXFS_FILE_NOT_FOUND:
		return ENOENT;
	case VBOXFS_FILE_EXISTS:
		break;			/* success */
	default:
		return EIO;		/* should never happen */
	}

	vboxfs_get_attr(attr, &crinfo.info);

	return OK;
}

/*
 * Set file size.
 */
static int
set_size(char *path, u64_t size)
{
	vboxfs_objinfo_t info;
	vboxfs_handle_t h;
	int r;

	if ((r = vboxfs_open_file(path, O_WRONLY, S_IFREG, &h, NULL)) != OK)
		return r;

	memset(&info, 0, sizeof(info));
	info.size = size;

	r = vboxfs_getset_info(h, VBOXFS_INFO_SET | VBOXFS_INFO_SIZE, &info,
	    sizeof(info));

	vboxfs_close_file(h);

	return r;
}

/*
 * Set file attributes.
 */
int
vboxfs_setattr(char *path, struct sffs_attr *attr)
{
	vboxfs_objinfo_t info;
	vboxfs_handle_t h;
	int r;

	/*
	 * Setting the size of a path cannot be combined with other attribute
	 * modifications, because we cannot fail atomically.
	 */
	if (attr->a_mask & SFFS_ATTR_SIZE) {
		assert(attr->a_mask == SFFS_ATTR_SIZE);

		return set_size(path, attr->a_size);
	}

	/*
	 * By passing a pointer to an object information structure, we open the
	 * file for attribute manipulation.  Note that this call will open the
	 * file as a regular file.  This works on directories as well.
	 */
	if ((r = vboxfs_open_file(path, O_WRONLY, 0, &h, &info)) != OK)
		return r;

	info.attr.add = VBOXFS_OBJATTR_ADD_NONE;

	/* Update the file's permissions if requested. */
	if (attr->a_mask & SFFS_ATTR_MODE)
		info.attr.mode =
		    VBOXFS_SET_MODE(info.attr.mode & S_IFMT, attr->a_mode);

	/*
	 * Update various file times if requested.  Not all changes may
	 * be honered.  A zero time indicates no change.
	 */
	info.atime = (attr->a_mask & SFFS_ATTR_ATIME) ?
	    set_time(&attr->a_atime) : 0;
	info.mtime = (attr->a_mask & SFFS_ATTR_MTIME) ?
	    set_time(&attr->a_mtime) : 0;
	info.ctime = (attr->a_mask & SFFS_ATTR_CTIME) ?
	    set_time(&attr->a_ctime) : 0;
	info.crtime = (attr->a_mask & SFFS_ATTR_CRTIME) ?
	    set_time(&attr->a_crtime) : 0;

	r = vboxfs_getset_info(h, VBOXFS_INFO_SET | VBOXFS_INFO_FILE, &info,
	    sizeof(info));

	vboxfs_close_file(h);

	return r;
}
