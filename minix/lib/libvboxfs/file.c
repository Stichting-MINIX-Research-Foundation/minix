/* Part of libvboxfs - (c) 2012, D.C. van Moolenbroek */

#include "inc.h"

/*
 * We perform all file I/O using a local, intermediate buffer.  While in theory
 * it would be possible to perform direct DMA from/to the user process, this
 * does not work in practice: on short reads, VirtualBox copies back the entire
 * provided buffer rather than only the part actually filled, resulting in the
 * unused part of the buffer being clobbered.  Marking the buffer as bi-
 * directional would solve this, except it would also eliminate all the
 * zero-copy benefits for reads; in addition, it is prevented by the protection
 * set on the given grant.
 */

#define VBOXFS_MAX_FILEIO	65536	/* maximum I/O chunk size */

static char iobuf[VBOXFS_MAX_FILEIO];

/*
 * Open a file.
 */
int
vboxfs_open(const char *path, int flags, int mode, sffs_file_t *handle)
{
	vboxfs_handle_t *handlep;
	int r;

	handlep = (vboxfs_handle_t *) malloc(sizeof(*handlep));

	if ((r = vboxfs_open_file(path, flags, mode, handlep, NULL)) != OK) {
		free(handlep);

		return r;
	}

	*handle = (sffs_file_t) handlep;

	return OK;
}

/*
 * Read or write a chunk from or to a file.
 */
static ssize_t
read_write(vboxfs_handle_t handle, char *buf, size_t size, u64_t pos,
	int write)
{
	vbox_param_t param[5];
	int r, dir, call;

	dir = write ? VBOX_DIR_OUT : VBOX_DIR_IN;
	call = write ? VBOXFS_CALL_WRITE : VBOXFS_CALL_READ;

	vbox_set_u32(&param[0], vboxfs_root);
	vbox_set_u64(&param[1], handle);
	vbox_set_u64(&param[2], pos);
	vbox_set_u32(&param[3], size);
	vbox_set_ptr(&param[4], buf, size, dir);

	if ((r = vbox_call(vboxfs_conn, call, param, 5, NULL)) != OK)
		return r;

	return vbox_get_u32(&param[3]);
}

/*
 * Read from a file.
 */
ssize_t
vboxfs_read(sffs_file_t handle, char *buf, size_t size, u64_t pos)
{
	vboxfs_handle_t *handlep;

	handlep = (vboxfs_handle_t *) handle;

	return read_write(*handlep, buf, size, pos, FALSE /*write*/);
}

/*
 * Write to a file.
 */
ssize_t
vboxfs_write(sffs_file_t handle, char *buf, size_t len, u64_t pos)
{
	vboxfs_handle_t *handlep;

	handlep = (vboxfs_handle_t *) handle;

	return read_write(*handlep, buf, len, pos, TRUE /*write*/);
}

/*
 * Close a file handle.
 */
int
vboxfs_close(sffs_file_t handle)
{
	vboxfs_handle_t *handlep;

	handlep = (vboxfs_handle_t *) handle;

	vboxfs_close_file(*handlep);

	free(handlep);

	return OK;
}

/*
 * Return an internal buffer address and size for I/O operations.
 */
size_t
vboxfs_buffer(char **ptr)
{

	*ptr = iobuf;
	return sizeof(iobuf);
}
