/* Part of libvboxfs - (c) 2012, D.C. van Moolenbroek */

#include "inc.h"

/*
 * Create or open a file or directory.
 */
int
vboxfs_open_file(char *path, int flags, int mode, vboxfs_handle_t *handlep,
	vboxfs_objinfo_t *infop)
{
	vbox_param_t param[3];
	vboxfs_path_t pathbuf;
	vboxfs_crinfo_t crinfo;
	int r, dir, rflag, wflag;

	if ((r = vboxfs_set_path(&pathbuf, path)) != OK)
		return r;

	memset(&crinfo, 0, sizeof(crinfo));

	/*
	 * Note that the mode may not be set at all.  If no new file may be
	 * created, this is not a problem.  The following test succeeds only if
	 * the caller explicitly specified that a directory is involved.
	 */
	dir = S_ISDIR(mode);

	/* Convert open(2) flags to VirtualBox creation flags. */
	if (flags & O_APPEND)
		return EINVAL;	/* not supported at this time */

	if (flags & O_CREAT) {
		crinfo.flags = VBOXFS_CRFLAG_CREATE_IF_NEW;

		if (flags & O_EXCL)
			crinfo.flags |= VBOXFS_CRFLAG_FAIL_IF_EXISTS;
		else if (flags & O_TRUNC)
			crinfo.flags |= VBOXFS_CRFLAG_TRUNC_IF_EXISTS;
		else
			crinfo.flags |= VBOXFS_CRFLAG_OPEN_IF_EXISTS;
	} else {
		crinfo.flags = VBOXFS_CRFLAG_FAIL_IF_NEW;

		if (flags & O_TRUNC)
			crinfo.flags |= VBOXFS_CRFLAG_TRUNC_IF_EXISTS;
		else
			crinfo.flags |= VBOXFS_CRFLAG_OPEN_IF_EXISTS;
	}

	/*
	 * If an object information structure is given, open the file only to
	 * retrieve or change its attributes.
	 */
	if (infop != NULL) {
		rflag = VBOXFS_CRFLAG_READ_ATTR;
		wflag = VBOXFS_CRFLAG_WRITE_ATTR;
	} else {
		rflag = VBOXFS_CRFLAG_READ;
		wflag = VBOXFS_CRFLAG_WRITE;
	}

	switch (flags & O_ACCMODE) {
	case O_RDONLY:	crinfo.flags |= rflag;		break;
	case O_WRONLY:	crinfo.flags |= wflag;		break;
	case O_RDWR:	crinfo.flags |= rflag | wflag;	break;
	default:	return EINVAL;
	}

	if (S_ISDIR(mode))
		crinfo.flags |= VBOXFS_CRFLAG_DIRECTORY;

	crinfo.info.attr.mode = VBOXFS_SET_MODE(dir ? S_IFDIR : S_IFREG, mode);
	crinfo.info.attr.add = VBOXFS_OBJATTR_ADD_NONE;

	vbox_set_u32(&param[0], vboxfs_root);
	vbox_set_ptr(&param[1], &pathbuf, vboxfs_get_path_size(&pathbuf),
	    VBOX_DIR_OUT);
	vbox_set_ptr(&param[2], &crinfo, sizeof(crinfo), VBOX_DIR_INOUT);

	r = vbox_call(vboxfs_conn, VBOXFS_CALL_CREATE, param, 3, NULL);
	if (r != OK)
		return r;

	if (crinfo.handle == VBOXFS_INVALID_HANDLE) {
		switch (crinfo.result) {
		case VBOXFS_PATH_NOT_FOUND:
			/*
			 * This could also mean ENOTDIR, but there does not
			 * appear to be any way to distinguish that case.
			 * Verifying with extra lookups seems overkill.
			 */
		case VBOXFS_FILE_NOT_FOUND:
			return ENOENT;
		case VBOXFS_FILE_EXISTS:
			return EEXIST;
		default:
			return EIO;		/* should never happen */
		}
	}

	*handlep = crinfo.handle;
	if (infop != NULL)
		*infop = crinfo.info;
	return OK;
}

/*
 * Close an open file handle.
 */
void
vboxfs_close_file(vboxfs_handle_t handle)
{
	vbox_param_t param[2];

	vbox_set_u32(&param[0], vboxfs_root);
	vbox_set_u64(&param[1], handle);

	/* Ignore errors here. We cannot do anything with them anyway. */
	(void) vbox_call(vboxfs_conn, VBOXFS_CALL_CLOSE, param, 2, NULL);
}
