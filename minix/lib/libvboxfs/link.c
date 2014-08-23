/* Part of libvboxfs - (c) 2012, D.C. van Moolenbroek */

#include "inc.h"

/*
 * Create a directory.
 */
int
vboxfs_mkdir(const char *path, int mode)
{
	vboxfs_handle_t h;
	int r;

	assert(S_ISDIR(mode));

	if ((r = vboxfs_open_file(path, O_CREAT | O_EXCL | O_WRONLY, mode, &h,
	   NULL)) != OK)
		return r;

	vboxfs_close_file(h);

	return r;
}

/*
 * Remove a file or directory.
 */
static int
remove_file(const char *path, int dir)
{
	vbox_param_t param[3];
	vboxfs_path_t pathbuf;
	int r;

	if ((r = vboxfs_set_path(&pathbuf, path)) != OK)
		return r;

	/* FIXME: symbolic links are not supported at all yet */
	vbox_set_u32(&param[0], vboxfs_root);
	vbox_set_ptr(&param[1], &pathbuf, vboxfs_get_path_size(&pathbuf),
	    VBOX_DIR_OUT);
	vbox_set_u32(&param[2], dir ? VBOXFS_REMOVE_DIR : VBOXFS_REMOVE_FILE);

	return vbox_call(vboxfs_conn, VBOXFS_CALL_REMOVE, param, 3, NULL);
}

/*
 * Unlink a file.
 */
int
vboxfs_unlink(const char *path)
{

	return remove_file(path, FALSE /*dir*/);
}

/*
 * Remove a directory.
 */
int
vboxfs_rmdir(const char *path)
{

	return remove_file(path, TRUE /*dir*/);
}

/*
 * Rename a file or directory.
 */
static int
rename_file(const char *opath, const char *npath, int dir)
{
	vbox_param_t param[4];
	vboxfs_path_t opathbuf, npathbuf;
	u32_t flags;
	int r;

	if ((r = vboxfs_set_path(&opathbuf, opath)) != OK)
		return r;

	if ((r = vboxfs_set_path(&npathbuf, npath)) != OK)
		return r;

	flags = dir ? VBOXFS_RENAME_DIR : VBOXFS_RENAME_FILE;
	flags |= VBOXFS_RENAME_REPLACE;

	vbox_set_u32(&param[0], vboxfs_root);
	vbox_set_ptr(&param[1], &opathbuf, vboxfs_get_path_size(&opathbuf),
	    VBOX_DIR_OUT);
	vbox_set_ptr(&param[2], &npathbuf, vboxfs_get_path_size(&npathbuf),
	    VBOX_DIR_OUT);
	vbox_set_u32(&param[3], flags);

	return vbox_call(vboxfs_conn, VBOXFS_CALL_RENAME, param, 4, NULL);
}

/*
 * Rename a file or directory.
 */
int
vboxfs_rename(const char *opath, const char *npath)
{
	int r;

	if ((r = rename_file(opath, npath, FALSE /*dir*/)) != EISDIR)
		return r;

	return rename_file(opath, npath, TRUE /*dir*/);
}
