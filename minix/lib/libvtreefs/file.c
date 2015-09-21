/* VTreeFS - file.c - file and directory I/O */

#include "inc.h"
#include <dirent.h>

#define GETDENTS_BUFSIZ 4096

static char *buf = NULL;
static size_t bufsize = 0;

/*
 * Initialize the main buffer used for I/O.  Return OK or an error code.
 */
int
init_buf(size_t size)
{

	/* A default buffer size, for at least getdents. */
	if (size < GETDENTS_BUFSIZ)
		size = GETDENTS_BUFSIZ;

	if ((buf = malloc(size)) == NULL)
		return ENOMEM;

	bufsize = size;
	return OK;
}

/*
 * Free up the I/O buffer.
 */
void
cleanup_buf(void)
{

	free(buf);

	buf = NULL;
	bufsize = 0;
}

/*
 * Read from a file.
 */
ssize_t
fs_read(ino_t ino_nr, struct fsdriver_data * data, size_t bytes,
	off_t pos, int __unused call)
{
	struct inode *node;
	size_t off, chunk;
	ssize_t r, len;

	/* Try to get inode by its inode number. */
	if ((node = find_inode(ino_nr)) == NULL)
		return EINVAL;

	/* Check whether the node is a regular file. */
	if (!S_ISREG(node->i_stat.mode))
		return EINVAL;

	/* For deleted files or with no read hook, feign an empty file. */
	if (is_inode_deleted(node) || vtreefs_hooks->read_hook == NULL)
		return 0; /* EOF */

	assert(buf != NULL);
	assert(bufsize > 0);

	/*
	 * Call the read hook to fill the result buffer, repeatedly for as long
	 * as 1) the request is not yet fully completed, and 2) the read hook
	 * fills the entire buffer.
	 */
	for (off = 0; off < bytes; ) {
		/* Get the next result chunk by calling the read hook. */
		chunk = bytes - off;
		if (chunk > bufsize)
			chunk = bufsize;

		len = vtreefs_hooks->read_hook(node, buf, chunk, pos,
		    get_inode_cbdata(node));

		/* Copy any resulting data to user space. */
		if (len > 0)
			r = fsdriver_copyout(data, off, buf, len);
		else
			r = len; /* EOF or error */

		/*
		 * If an error occurred, but we already produced some output,
		 * return a partial result.  Otherwise return the error.
		 */
		if (r < 0)
			return (off > 0) ? (ssize_t)off : r;

		off += len;
		pos += len;

		if ((size_t)len < bufsize)
			break;
	}

	return off;
}

/*
 * Write to a file.
 */
ssize_t
fs_write(ino_t ino_nr, struct fsdriver_data * data, size_t bytes, off_t pos,
	int __unused call)
{
	struct inode *node;
	size_t off, chunk;
	ssize_t r;

	if ((node = find_inode(ino_nr)) == NULL)
		return EINVAL;

	if (!S_ISREG(node->i_stat.mode))
		return EINVAL;

	if (is_inode_deleted(node) || vtreefs_hooks->write_hook == NULL)
		return EACCES;

	if (bytes == 0)
		return 0;

	assert(buf != NULL);
	assert(bufsize > 0);

	/*
	 * Call the write hook to process the incoming data, repeatedly for as
	 * long as 1) the request is not yet fully completed, and 2) the write
	 * hook processes at least some of the given data.
	 */
	for (off = 0; off < bytes; ) {
		chunk = bytes - off;
		if (chunk > bufsize)
			chunk = bufsize;

		/* Copy the data from user space. */
		r = fsdriver_copyin(data, off, buf, chunk);

		/* Call the write hook for the chunk. */
		if (r == OK)
			r = vtreefs_hooks->write_hook(node, buf, chunk, pos,
			    get_inode_cbdata(node));

		/*
		 * If an error occurred, but we already processed some input,
		 * return a partial result.  Otherwise return the error.
		 */
		if (r < 0)
			return (off > 0) ? (ssize_t)off : r;

		off += r;
		pos += r;

		if ((size_t)r == 0)
			break;
	}

	return off;
}

/*
 * Truncate a file.
 */
int
fs_trunc(ino_t ino_nr, off_t start_pos, off_t end_pos)
{
	struct inode *node;

	if ((node = find_inode(ino_nr)) == NULL)
		return EINVAL;

	if (!S_ISREG(node->i_stat.mode))
		return EINVAL;

	if (is_inode_deleted(node) || vtreefs_hooks->trunc_hook == NULL)
		return EACCES;

	/* TODO: translate this case into all-zeroes write callbacks. */
	if (end_pos != 0)
		return EINVAL;

	return vtreefs_hooks->trunc_hook(node, start_pos,
	    get_inode_cbdata(node));
}

/*
 * Retrieve directory entries.
 */
ssize_t
fs_getdents(ino_t ino_nr, struct fsdriver_data * data, size_t bytes,
	off_t * posp)
{
	struct fsdriver_dentry fsdentry;
	struct inode *node, *child;
	const char *name;
	off_t pos;
	int r, skip, get_next, indexed;

	if (*posp >= ULONG_MAX)
		return EIO;

	if ((node = find_inode(ino_nr)) == NULL)
		return EINVAL;

	indexed = node->i_indexed;
	get_next = FALSE;
	child = NULL;

	/* Call the getdents hook, if any, to "refresh" the directory. */
	if (!is_inode_deleted(node) && vtreefs_hooks->getdents_hook != NULL) {
		r = vtreefs_hooks->getdents_hook(node, get_inode_cbdata(node));
		if (r != OK)
			return r;
	}

	assert(buf != NULL);
	assert(bufsize > 0);

	fsdriver_dentry_init(&fsdentry, data, bytes, buf, bufsize);

	for (;;) {
		/* Determine which inode and name to use for this entry. */
		pos = (*posp)++;

		if (pos == 0) {
			/* The "." entry. */
			child = node;
			name = ".";
		} else if (pos == 1) {
			/* The ".." entry. */
			child = get_parent_inode(node);
			if (child == NULL)
				child = node;
			name = "..";
		} else if (pos - 2 < indexed) {
			/* All indexed entries. */
			child = get_inode_by_index(node, pos - 2);

			/*
			 * If there is no inode with this particular index,
			 * continue with the next index number.
			 */
			if (child == NULL) continue;

			name = child->i_name;
		} else {
			/* All non-indexed entries. */
			/*
			 * If this is the first loop iteration, first get to
			 * the non-indexed child identified by the current
			 * position.
			 */
			if (get_next == FALSE) {
				skip = pos - indexed - 2;
				child = get_first_inode(node);

				/* Skip indexed children. */
				while (child != NULL &&
				    child->i_index != NO_INDEX)
					child = get_next_inode(child);

				/* Skip to the right position. */
				while (child != NULL && skip-- > 0)
					child = get_next_inode(child);

				get_next = TRUE;
			} else
				child = get_next_inode(child);

			/* No more children? Then stop. */
			if (child == NULL)
				break;

			assert(!is_inode_deleted(child));

			name = child->i_name;
		}

		/* Add the directory entry to the output. */
		r = fsdriver_dentry_add(&fsdentry,
		    (ino_t)get_inode_number(child), name, strlen(name),
		    IFTODT(child->i_stat.mode));
		if (r < 0)
			return r;
		if (r == 0)
			break;
	}

	return fsdriver_dentry_finish(&fsdentry);
}
