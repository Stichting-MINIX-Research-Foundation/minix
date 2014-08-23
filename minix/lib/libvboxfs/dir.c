/* Part of libvboxfs - (c) 2012, D.C. van Moolenbroek */

#include "inc.h"

/*
 * Directories will generally be accessed sequentially, but there is no
 * guarantee that this is actually the case.  In particular, multiple user
 * processes may iterate over the same directory concurrently, and since
 * process information is lost in the VFS/FS protocol, the result is a
 * nonsequential pattern on the same single directory handle.  Therefore, we
 * must support random access.  Since the VirtualBox shared folders interface
 * does not allow for random access (the resume point cannot be used for this),
 * we choose to read in the contents of the directory upon open, and cache it
 * until the directory is closed again.  The risk is that the cached contents
 * will go stale.
 *
 * The directory will always be closed once one reader finishes going through
 * the entire directory, so the problem is rather limited anyway.  Ideally, the
 * directory contents would be refreshed upon any invalidating local action as
 * well as after a certain time span (since the file system can be changed from
 * the host as well).  This is currently not implemented, because the odds of
 * things going wrong are pretty small, and the effects are not devastating.
 *
 * The calling functions may also request the same directory entry twice in a
 * row, because the entry does not fit in the user buffer the first time.  The
 * routines here optimize for repeat-sequential access, while supporting fully
 * random access as well.
 */

#define VBOXFS_DIRDATA_SIZE	8192	/* data allocation granularity */

typedef struct vboxfs_dirblock_s {
	STAILQ_ENTRY(vboxfs_dirblock_s) next;
	unsigned int count;
	char data[VBOXFS_DIRDATA_SIZE];
} vboxfs_dirblock_t;

typedef struct {
	STAILQ_HEAD(blocks, vboxfs_dirblock_s) blocks;
	unsigned int index;
	vboxfs_dirblock_t *block;
	unsigned int bindex;
	unsigned int bpos;
} vboxfs_dirdata_t;

/*
 * Free the memory allocated for the given directory contents storage.
 */
static void
free_dir(vboxfs_dirdata_t *dirdata)
{
	vboxfs_dirblock_t *block;

	while (!STAILQ_EMPTY(&dirdata->blocks)) {
		block = STAILQ_FIRST(&dirdata->blocks);

		STAILQ_REMOVE_HEAD(&dirdata->blocks, next);

		free(block);
	}

	free(dirdata);
}

/*
 * Read all the contents of the given directory, allocating memory as needed to
 * store the data.
 */
static int
read_dir(vboxfs_handle_t handle, sffs_dir_t *dirp)
{
	vboxfs_dirdata_t *dirdata;
	vboxfs_dirblock_t *block;
	vbox_param_t param[8];
	unsigned int count;
	int r;

	dirdata = (vboxfs_dirdata_t *) malloc(sizeof(vboxfs_dirdata_t));
	if (dirdata == NULL)
		return ENOMEM;

	memset(dirdata, 0, sizeof(*dirdata));
	STAILQ_INIT(&dirdata->blocks);

	r = OK;

	do {
		block =
		    (vboxfs_dirblock_t *) malloc(sizeof(vboxfs_dirblock_t));
		if (block == NULL) {
			r = ENOMEM;
			break;
		}

		vbox_set_u32(&param[0], vboxfs_root);
		vbox_set_u64(&param[1], handle);
		vbox_set_u32(&param[2], 0);		/* flags */
		vbox_set_u32(&param[3], sizeof(block->data));
		vbox_set_ptr(&param[4], NULL, 0, VBOX_DIR_OUT);
		vbox_set_ptr(&param[5], block->data, sizeof(block->data),
		    VBOX_DIR_IN);
		vbox_set_u32(&param[6], 0);		/* resume point */
		vbox_set_u32(&param[7], 0);		/* number of files */

		/* If the call fails, stop. */
		if ((r = vbox_call(vboxfs_conn, VBOXFS_CALL_LIST, param, 8,
		    NULL)) != OK) {
			free(block);
			break;
		}

		/* If the number of returned files is zero, stop. */
		if ((count = vbox_get_u32(&param[7])) == 0) {
			free(block);
			break;
		}

		/*
		 * Add the block to the list. We could realloc() the block to
		 * free unused tail space, but this is not likely to gain us
		 * much in general.
		 */
		block->count = count;
		STAILQ_INSERT_TAIL(&dirdata->blocks, block, next);

		/* Continue until a zero resume point is returned. */
	} while (vbox_get_u32(&param[6]) != 0);

	if (r != OK) {
		free_dir(dirdata);

		return r;
	}

	dirdata->block = STAILQ_FIRST(&dirdata->blocks);

	*dirp = (sffs_dir_t) dirdata;

	return OK;
}

/*
 * Open a directory.
 */
int
vboxfs_opendir(const char *path, sffs_dir_t *handle)
{
	vboxfs_handle_t h;
	int r;

	/* Open the directory. */
	if ((r = vboxfs_open_file(path, O_RDONLY, S_IFDIR, &h, NULL)) != OK)
		return r;

	/*
	 * Upon success, read in the full contents of the directory right away.
	 * If it succeeds, this will also set the caller's directory handle.
	 */
	r = read_dir(h, handle);

	/* We do not need the directory to be open anymore now. */
	vboxfs_close_file(h);

	return r;
}

/*
 * Read one entry from a directory.  On success, copy the name into buf, and
 * return the requested attributes.  If the name (including terminating null)
 * exceeds size, return ENAMETOOLONG.  Do not return dot and dot-dot entries.
 * Return ENOENT if the index exceeds the number of files.
 */
int
vboxfs_readdir(sffs_dir_t handle, unsigned int index, char *buf, size_t size,
	struct sffs_attr *attr)
{
	vboxfs_dirdata_t *dirdata;
	vboxfs_dirinfo_t *dirinfo;
	int r;

	dirdata = (vboxfs_dirdata_t *) handle;

	/*
	 * If the saved index exceeds the requested index, start from the
	 * beginning.
	 */
	if (dirdata->index > index) {
		dirdata->index = 0;
		dirdata->bindex = 0;
		dirdata->bpos = 0;
		dirdata->block = STAILQ_FIRST(&dirdata->blocks);
	}

	/* Loop until we find the requested entry or we run out of entries. */
	while (dirdata->block != NULL) {
		dirinfo =
		    (vboxfs_dirinfo_t *) &dirdata->block->data[dirdata->bpos];

		/* Consider all entries that are not dot or dot-dot. */
		if (dirinfo->name.len > 2 || dirinfo->name.data[0] != '.' ||
		    (dirinfo->name.len == 2 && dirinfo->name.data[1] != '.')) {

			if (dirdata->index == index)
				break;

			dirdata->index++;
		}

		/* Advance to the next entry. */
		dirdata->bpos += offsetof(vboxfs_dirinfo_t, name) +
		    offsetof(vboxfs_path_t, data) + dirinfo->name.size;
		if (++dirdata->bindex >= dirdata->block->count) {
			dirdata->block = STAILQ_NEXT(dirdata->block, next);
			dirdata->bindex = 0;
			dirdata->bpos = 0;
		}
	}

	/* Not enough files to satisfy the request? */
	if (dirdata->block == NULL)
		return ENOENT;

	/* Return the information for the file we found. */
	if ((r = vboxfs_get_path(&dirinfo->name, buf, size)) != OK)
		return r;

	vboxfs_get_attr(attr, &dirinfo->info);

	return OK;
}

/*
 * Close a directory.
 */
int
vboxfs_closedir(sffs_dir_t handle)
{
	vboxfs_dirdata_t *dirdata;

	dirdata = (vboxfs_dirdata_t *) handle;

	free_dir(dirdata);

	return OK;
}
