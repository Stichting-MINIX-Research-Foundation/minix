/* Part of libvboxfs - (c) 2012, D.C. van Moolenbroek */

#include "inc.h"

vbox_conn_t vboxfs_conn;
vboxfs_root_t vboxfs_root;

static struct sffs_table vboxfs_table = {
	.t_open		= vboxfs_open,
	.t_read		= vboxfs_read,
	.t_write	= vboxfs_write,
	.t_close	= vboxfs_close,

	.t_readbuf	= vboxfs_buffer,
	.t_writebuf	= vboxfs_buffer,

	.t_opendir	= vboxfs_opendir,
	.t_readdir	= vboxfs_readdir,
	.t_closedir	= vboxfs_closedir,

	.t_getattr	= vboxfs_getattr,
	.t_setattr	= vboxfs_setattr,

	.t_mkdir	= vboxfs_mkdir,
	.t_unlink	= vboxfs_unlink,
	.t_rmdir	= vboxfs_rmdir,
	.t_rename	= vboxfs_rename,

	.t_queryvol	= vboxfs_queryvol
};

/*
 * Initialize communication with the VBOX driver, and map the given share.
 */
int
vboxfs_init(char *share, const struct sffs_table **tablep, int *case_insens,
	int *read_only)
{
	vbox_param_t param[4];
	vboxfs_path_t path;
	vboxfs_volinfo_t volinfo;
	int r;

	if ((r = vboxfs_set_path(&path, share)) != OK)
		return r;

	if ((r = vbox_init()) != OK)
		return r;

	if ((vboxfs_conn = r = vbox_open("VBoxSharedFolders")) < 0)
		return r;

	r = vbox_call(vboxfs_conn, VBOXFS_CALL_SET_UTF8, NULL, 0, NULL);
	if (r != OK) {
		vbox_close(vboxfs_conn);

		return r;
	}

	vbox_set_ptr(&param[0], &path, vboxfs_get_path_size(&path),
	    VBOX_DIR_OUT);
	vbox_set_u32(&param[1], 0);
	vbox_set_u32(&param[2], '/');	/* path separator */
	vbox_set_u32(&param[3], TRUE);	/* case sensitivity - no effect? */

	r = vbox_call(vboxfs_conn, VBOXFS_CALL_MAP_FOLDER, param, 4, NULL);
	if (r != OK) {
		vbox_close(vboxfs_conn);

		return r;
	}

	vboxfs_root = vbox_get_u32(&param[1]);

	/* Gather extra information about the mapped shared. */
	if (vboxfs_query_vol("", &volinfo) == OK) {
		*case_insens = !volinfo.props.casesens;
		*read_only = !!volinfo.props.readonly;
	}

	*tablep = &vboxfs_table;
	return OK;
}

/*
 * Unmap the share, and disconnect from the VBOX driver.
 */
void
vboxfs_cleanup(void)
{
	vbox_param_t param[1];

	vbox_set_u32(&param[0], vboxfs_root);

	vbox_call(vboxfs_conn, VBOXFS_CALL_UNMAP_FOLDER, param, 1, NULL);

	vbox_close(vboxfs_conn);
}
