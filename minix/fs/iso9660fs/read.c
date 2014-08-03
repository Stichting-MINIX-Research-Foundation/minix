#include "inc.h"
#include <minix/com.h>
#include <minix/vfsif.h>
#include <minix/minlib.h>
#include <fcntl.h>
#include <stddef.h>

static char getdents_buf[GETDENTS_BUFSIZ];

int fs_read(void)
{
	int r = OK, chunk, block_size, completed, rw;
	size_t nrbytes, off, cum_io;
	cp_grant_id_t gid;
	off_t position, f_size, bytes_left;
	struct inode *i_node;

	switch(fs_m_in.m_type) {
		case REQ_READ: rw = READING; break;
		case REQ_PEEK: rw = PEEKING; break;
		default: panic("odd m_type");
	}

	/* Try to get inode according to its index. */
	i_node = find_inode(fs_m_in.m_vfs_fs_readwrite.inode);
	if (i_node == NULL)
		return EINVAL; /* No inode found. */

	position        = fs_m_in.m_vfs_fs_readwrite.seek_pos;
	nrbytes         = fs_m_in.m_vfs_fs_readwrite.nbytes; /* Number of bytes to read. */
	block_size      = v_pri.logical_block_size_l;
	gid             = fs_m_in.m_vfs_fs_readwrite.grant;
	f_size          = i_node->i_stat.st_size;
	rdwt_err        = OK;   /* Set to EIO if disk error occurs. */
	cum_io          = 0;

	/* Split the transfer into chunks that don't span two blocks. */
	while (nrbytes != 0) {
		off = position % block_size;

		chunk = MIN(nrbytes, block_size - off);
		if (chunk < 0)
			chunk = block_size - off;

		bytes_left = f_size - position;
		if (position >= f_size)
			break;  /* We are beyond EOF. */
		if (chunk > bytes_left)
			chunk = (int32_t) bytes_left;

		/* Read or write 'chunk' bytes. */
		r = read_chunk(i_node, position, off, chunk,
				(uint32_t) nrbytes, gid, cum_io, block_size,
				&completed, rw);

		if (r != OK)
			break;  /* EOF reached. */
		if (rdwt_err < 0)
			break;

		/* Update counters and pointers. */
		nrbytes -= chunk;	/* Bytes yet to be read. */
		cum_io += chunk;	/* Bytes read so far. */
		position += chunk;	/* Position within the file. */
	}

	fs_m_out.m_fs_vfs_readwrite.seek_pos = position;

	if (rdwt_err != OK)
		r = rdwt_err;	/* Check for disk error. */
	if (rdwt_err == END_OF_FILE)
		r = OK;

	fs_m_out.m_fs_vfs_readwrite.nbytes = cum_io;
	return r;
}

int fs_bread(void)
{
	int r = OK,  rw_flag, chunk, block_size, completed;
	size_t nrbytes, off, cum_io;
	cp_grant_id_t gid;
	off_t position;
	struct inode *i_node;

	r = OK;

	rw_flag = (fs_m_in.m_type == REQ_BREAD ? READING : WRITING);
	gid = fs_m_in.m_vfs_fs_breadwrite.grant;
	position = fs_m_in.m_vfs_fs_breadwrite.seek_pos;
	nrbytes = fs_m_in.m_vfs_fs_breadwrite.nbytes;
	block_size = v_pri.logical_block_size_l;
	i_node = v_pri.inode_root;

	if(rw_flag == WRITING)
		return EIO;	/* Not supported */

	rdwt_err = OK;		/* set to EIO if disk error occurs */

	cum_io = 0;
	/* Split the transfer into chunks that don't span two blocks. */
	while (nrbytes != 0) {
		off = (unsigned int)(position % block_size);	/* offset in blk*/

		chunk = MIN(nrbytes, block_size - off);
		if (chunk < 0)
			chunk = block_size - off;

		/* Read 'chunk' bytes. */
		r = read_chunk(i_node, position, off, chunk, (unsigned) nrbytes, 
		               gid, cum_io, block_size, &completed, READING);

		if (r != OK)
			break;  /* EOF reached */
		if (rdwt_err < 0)
			break;

		/* Update counters and pointers. */
		nrbytes -= chunk;       /* bytes yet to be read */
		cum_io += chunk;        /* bytes read so far */
		position += chunk;      /* position within the file */
	}

	fs_m_out.m_fs_vfs_breadwrite.seek_pos = position;

	if (rdwt_err != OK)
		r = rdwt_err;           /* check for disk error */
	if (rdwt_err == END_OF_FILE)
		r = OK;

	fs_m_out.m_fs_vfs_breadwrite.nbytes = cum_io;

	return r;
}

int fs_getdents(void)
{
	struct inode *i_node, *i_node_tmp;
	ino_t ino;
	cp_grant_id_t gid;
	size_t old_pos = 0, cur_pos, new_pos, tmpbuf_off = 0, userbuf_off = 0, grant_size;
	struct dirent *dirp;
	int r, len, reclen;
	char *cp;

	/* Get input parameters */
	ino        = fs_m_in.m_vfs_fs_getdents.inode;
	gid        = fs_m_in.m_vfs_fs_getdents.grant;
	cur_pos    = fs_m_in.m_vfs_fs_getdents.seek_pos;
	grant_size = fs_m_in.m_vfs_fs_getdents.mem_size;

	//memset(getdents_buf, '\0', GETDENTS_BUFSIZ);	/* Avoid leaking any data */

	if ((i_node = get_inode(ino)) == NULL)
		return EINVAL;

	while (TRUE) {
		i_node_tmp = alloc_inode();
		r = read_inode(i_node_tmp, i_node->extent, cur_pos, &new_pos);
		if ((r != OK) || (new_pos >= i_node->i_stat.st_size)) {
			put_inode(i_node);
			put_inode(i_node_tmp);
			break;
		}
		cur_pos = new_pos;

		/* Compute the length of the name */
		cp = memchr(i_node_tmp->i_name, '\0', NAME_MAX);
		if (cp == NULL)
			len = NAME_MAX;
		else
			len = cp - i_node_tmp->i_name;

		/* Compute record length; also does alignment. */
		reclen = _DIRENT_RECLEN(dirp, len);

		/* If the new record does not fit, then copy the buffer
		 * and start from the beginning. */
		if (tmpbuf_off + reclen > GETDENTS_BUFSIZ ||
		    userbuf_off + tmpbuf_off + reclen > grant_size) {
			r = sys_safecopyto(VFS_PROC_NR, gid, userbuf_off, 
			    (vir_bytes)getdents_buf, tmpbuf_off);

			if (r != OK)
				panic("fs_getdents: sys_safecopyto failed: %d", r);

			/* Check if the user grant buffer is filled. */
			if (userbuf_off + tmpbuf_off + reclen > grant_size) {
				fs_m_out.m_fs_vfs_getdents.nbytes = userbuf_off + tmpbuf_off;
				fs_m_out.m_fs_vfs_getdents.seek_pos = old_pos;
				return OK;
			}

			userbuf_off += tmpbuf_off;
			tmpbuf_off = 0;
		}

		/* The standard data structure is created using the
		 * data in the buffer. */
		dirp = (struct dirent *) &getdents_buf[tmpbuf_off];
		dirp->d_fileno = i_node_tmp->i_stat.st_ino;
		dirp->d_reclen = reclen;
		dirp->d_type   = fs_mode_to_type(i_node_tmp->i_stat.st_mode);
		dirp->d_namlen = len;

		memcpy(dirp->d_name, i_node_tmp->i_name, len);
		dirp->d_name[len]= '\0';

		tmpbuf_off += reclen;
		put_inode(i_node_tmp);

		old_pos = cur_pos;
	}

	if (tmpbuf_off != 0) {
		r = sys_safecopyto(VFS_PROC_NR, gid, userbuf_off,
				   (vir_bytes) getdents_buf, tmpbuf_off);

		if (r != OK)
			panic("fs_getdents: sys_safecopyto failed: %d", r);

		userbuf_off += tmpbuf_off;
	}

	fs_m_out.m_fs_vfs_getdents.nbytes = userbuf_off;
	fs_m_out.m_fs_vfs_getdents.seek_pos = cur_pos;

	return OK;
}

int read_chunk(
struct inode *i_node,		/* pointer to inode for file to be rd/wr */
u64_t position,			/* position within file to read or write */
unsigned off,			/* off within the current block */
int chunk,			/* number of bytes to read or write */
unsigned left,			/* max number of bytes wanted after position */
cp_grant_id_t gid,		/* grant */
unsigned buf_off,		/* offset in grant */
int block_size,			/* block size of FS operating on */
int *completed,			/* number of bytes copied */
int rw)				/* READING or PEEKING */
{
	struct buf *bp;
	int r = OK;

	*completed = 0;

	bp = read_extent_block(i_node->extent, position / block_size);
	if (bp == NULL)
		panic("bp not valid in rw_chunk; this can't happen");

	if(rw == READING) {
		r = sys_safecopyto(VFS_PROC_NR, gid, buf_off,
		                  (vir_bytes) (b_data(bp)+off),
		                  (phys_bytes) chunk);
	}

	put_block(bp);
	return r;
}

