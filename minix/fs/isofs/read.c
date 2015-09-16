#include "inc.h"

static char getdents_buf[GETDENTS_BUFSIZ];

ssize_t fs_read(ino_t ino_nr, struct fsdriver_data *data, size_t bytes,
	off_t pos, int __unused call)
{
	size_t off, chunk, block_size, cum_io;
	off_t f_size;
	struct inode *i_node;
	struct buf *bp;
	int r;

	/* Try to get inode according to its index. */
	if ((i_node = get_inode(ino_nr)) == NULL)
		return EINVAL; /* No inode found. */

	f_size = i_node->i_stat.st_size;
	if (pos >= f_size)
		return 0; /* EOF */

	/* Limit the request to the remainder of the file size. */
	if ((off_t)bytes > f_size - pos)
		bytes = (size_t)(f_size - pos);

	block_size = v_pri.logical_block_size_l;
	cum_io = 0;

	r = OK;

	/* Split the transfer into chunks that don't span two blocks. */
	while (bytes > 0) {
		off = pos % block_size;

		chunk = block_size - off;
		if (chunk > bytes)
			chunk = bytes;

		/* Read 'chunk' bytes. */
		bp = read_extent_block(&i_node->extent, pos);
		if (bp == NULL)
			panic("bp not valid in rw_chunk; this can't happen");

		r = fsdriver_copyout(data, cum_io, b_data(bp)+off, chunk);

		lmfs_put_block(bp);

		if (r != OK)
			break;

		/* Update counters and pointers. */
		bytes -= chunk;		/* Bytes yet to be read. */
		cum_io += chunk;	/* Bytes read so far. */
		pos += chunk;		/* Position within the file. */
	}

	return (r == OK) ? cum_io : r;
}

ssize_t fs_getdents(ino_t ino_nr, struct fsdriver_data *data, size_t bytes,
	off_t *pos)
{
	struct fsdriver_dentry fsdentry;
	struct inode *i_node;
	off_t cur_pos;
	int r, len;
	char *cp;

	if ((i_node = get_inode(ino_nr)) == NULL)
		return EINVAL;

	if (*pos < 0 || *pos > SSIZE_MAX)
		return EINVAL;

	r = read_directory(i_node);
	if (r != OK)
		return r;

	fsdriver_dentry_init(&fsdentry, data, bytes, getdents_buf,
	    sizeof(getdents_buf));

	r = OK;

	for (cur_pos = *pos; cur_pos < i_node->dir_size; cur_pos++) {
		/* Compute the length of the name */
		cp = memchr(i_node->dir_contents[cur_pos].name, '\0', NAME_MAX);
		if (cp == NULL)
			len = NAME_MAX;
		else
			len = cp - i_node->dir_contents[cur_pos].name;

		r = fsdriver_dentry_add(&fsdentry,
		    i_node->dir_contents[cur_pos].i_node->i_stat.st_ino,
		    i_node->dir_contents[cur_pos].name, len,
		    IFTODT(i_node->dir_contents[cur_pos].i_node->i_stat.st_mode));

		if (r <= 0)
			break;
	}

	if (r >= 0 && (r = fsdriver_dentry_finish(&fsdentry)) >= 0)
		*pos = cur_pos;

	return r;
}
