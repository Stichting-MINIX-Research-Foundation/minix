/* Part of libvboxfs - (c) 2012, D.C. van Moolenbroek */

#include "inc.h"

/*
 * Get or set file information.
 */
int
vboxfs_getset_info(vboxfs_handle_t handle, u32_t flags, void *data,
	size_t size)
{
	vbox_param_t param[5];

	vbox_set_u32(&param[0], vboxfs_root);
	vbox_set_u64(&param[1], handle);
	vbox_set_u32(&param[2], flags);
	vbox_set_u32(&param[3], size);
	vbox_set_ptr(&param[4], data, size, VBOX_DIR_INOUT);

	return vbox_call(vboxfs_conn, VBOXFS_CALL_INFO, param, 5, NULL);
}

/*
 * Query volume information.
 */
int
vboxfs_query_vol(char *path, vboxfs_volinfo_t *volinfo)
{
	vboxfs_handle_t h;
	int r;

	if ((r = vboxfs_open_file(path, O_RDONLY, 0, &h, NULL)) != OK)
		return r;

	r = vboxfs_getset_info(h, VBOXFS_INFO_GET | VBOXFS_INFO_VOLUME,
	    volinfo, sizeof(*volinfo));

	vboxfs_close_file(h);

	return r;
}

/*
 * Query volume information.
 */
int
vboxfs_queryvol(char *path, u64_t *free, u64_t *total)
{
	vboxfs_volinfo_t volinfo;
	int r;

	if ((r = vboxfs_query_vol(path, &volinfo)) != OK)
		return r;

	*free = volinfo.free;
	*total = volinfo.total;
	return OK;
}
