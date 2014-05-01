#include "inc.h"
#include <minix/com.h>
#include <minix/vfsif.h>
#include <minix/minlib.h>
#include <fcntl.h>
#include <stddef.h>
#include "buf.h"

static char getdents_buf[GETDENTS_BUFSIZ];


/*===========================================================================*
 *				fs_read					     *
 *===========================================================================*/
int fs_read(void) {
  int r, chunk, block_size;
  size_t nrbytes;
  cp_grant_id_t gid;
  off_t position, f_size, bytes_left;
  unsigned int off, cum_io;
  int completed;
  struct dir_record *dir;
  int rw;

  switch(fs_m_in.m_type) {
  	case REQ_READ: rw = READING; break;
	case REQ_PEEK: rw = PEEKING; break;
	default: panic("odd m_type");
  }
  
  r = OK;
  
  /* Try to get inode according to its index */
  dir = get_dir_record(fs_m_in.m_vfs_fs_readwrite.inode);
  if (dir == NULL) return(EINVAL); /* no inode found */

  position = fs_m_in.m_vfs_fs_readwrite.seek_pos;
  nrbytes = fs_m_in.m_vfs_fs_readwrite.nbytes; /* number of bytes to read */
  block_size = v_pri.logical_block_size_l;
  gid = fs_m_in.m_vfs_fs_readwrite.grant;
  f_size = dir->d_file_size;

  rdwt_err = OK;		/* set to EIO if disk error occurs */
  
  cum_io = 0;
  /* Split the transfer into chunks that don't span two blocks. */
  while (nrbytes != 0) {
	off = position % block_size;
          
	chunk = MIN(nrbytes, block_size - off);
	if (chunk < 0) chunk = block_size - off;

	bytes_left = f_size - position;
	if (position >= f_size) break;	/* we are beyond EOF */
	if (chunk > bytes_left) chunk = (int32_t) bytes_left;
      
	/* Read or write 'chunk' bytes. */
	r = read_chunk(dir, position, off, chunk,
			(uint32_t) nrbytes, gid, cum_io, block_size,
			&completed, rw);

	if (r != OK) break;	/* EOF reached */
	if (rdwt_err < 0) break;

	/* Update counters and pointers. */
	nrbytes -= chunk;	/* bytes yet to be read */
	cum_io += chunk;	/* bytes read so far */
	position += chunk;	/* position within the file */
  }

  fs_m_out.m_fs_vfs_readwrite.seek_pos = position;
  
  if (rdwt_err != OK) r = rdwt_err;	/* check for disk error */
  if (rdwt_err == END_OF_FILE) r = OK;
  
  fs_m_out.m_fs_vfs_readwrite.nbytes = cum_io; /*dir->d_file_size;*/
  release_dir_record(dir);

  return(r);
}


/*===========================================================================*
 *				fs_bread				     *
 *===========================================================================*/
int fs_bread(void)
{
  int r, rw_flag, chunk, block_size;
  cp_grant_id_t gid;
  int nrbytes;
  u64_t position;
  unsigned int off, cum_io;
  int completed;
  struct dir_record *dir;
  
  r = OK;
  
  rw_flag = (fs_m_in.m_type == REQ_BREAD ? READING : WRITING);
  gid = fs_m_in.m_vfs_fs_breadwrite.grant;
  position = fs_m_in.m_vfs_fs_breadwrite.seek_pos;
  nrbytes = fs_m_in.m_vfs_fs_breadwrite.nbytes;
  block_size = v_pri.logical_block_size_l;
  dir = v_pri.dir_rec_root;

  if(rw_flag == WRITING) return (EIO);	/* Not supported */
  rdwt_err = OK;		/* set to EIO if disk error occurs */
  
  cum_io = 0;
  /* Split the transfer into chunks that don't span two blocks. */
  while (nrbytes != 0) {
    off = (unsigned int)(position % block_size);	/* offset in blk*/
    
    chunk = MIN(nrbytes, block_size - off);
    if (chunk < 0) chunk = block_size - off;

	/* Read 'chunk' bytes. */
    r = read_chunk(dir, position, off, chunk, (unsigned) nrbytes, 
		   gid, cum_io, block_size, &completed, READING);
    
    if (r != OK) break;	/* EOF reached */
    if (rdwt_err < 0) break;
    
    /* Update counters and pointers. */
    nrbytes -= chunk;	/* bytes yet to be read */
    cum_io += chunk;	/* bytes read so far */
    position += chunk;	/* position within the file */
  }
  
  fs_m_out.m_fs_vfs_breadwrite.seek_pos = position;
  
  if (rdwt_err != OK) r = rdwt_err;	/* check for disk error */
  if (rdwt_err == END_OF_FILE) r = OK;

  fs_m_out.m_fs_vfs_breadwrite.nbytes = cum_io;
  
  return(r);
}


/*===========================================================================*
 *				fs_getdents				     *
 *===========================================================================*/
int fs_getdents(void)
{
  struct dir_record *dir;
  ino_t ino;
  cp_grant_id_t gid;
  size_t block_size;
  off_t pos, block_pos, block, cur_pos, tmpbuf_offset, userbuf_off;
  struct buf *bp;
  struct dir_record *dir_tmp;
  struct dirent *dirp;
  int r,done,o,len,reclen;
  char *cp;
  char name[NAME_MAX + 1];
  char name_old[NAME_MAX + 1];

  /* Initialize the tmp arrays */
  memset(name,'\0',NAME_MAX);
  memset(name_old,'\0',NAME_MAX);

  /* Get input parameters */
  ino = fs_m_in.m_vfs_fs_getdents.inode;
  gid = fs_m_in.m_vfs_fs_getdents.grant;
  pos = fs_m_in.m_vfs_fs_getdents.seek_pos;

  block_size = v_pri.logical_block_size_l;
  cur_pos = pos;		/* The current position */
  tmpbuf_offset = 0;
  userbuf_off = 0;
  memset(getdents_buf, '\0', GETDENTS_BUFSIZ);	/* Avoid leaking any data */

  if ((dir = get_dir_record(ino)) == NULL) return(EINVAL);

  block = dir->loc_extent_l;	/* First block of the directory */
  block += pos / block_size; 	/* Shift to the block where start to read */
  done = FALSE;

  while (cur_pos<dir->d_file_size) {
	bp = get_block(block);	/* Get physical block */

	if (bp == NULL) {
		release_dir_record(dir);
		return(EINVAL);
	}
    
	block_pos = cur_pos % block_size; /* Position where to start read */

	while (block_pos < block_size) {
		dir_tmp = get_free_dir_record();
		create_dir_record(dir_tmp,b_data(bp) + block_pos,
				  block*block_size + block_pos);
		if (dir_tmp->length == 0) { /* EOF. I exit and return 0s */
			block_pos = block_size;
			done = TRUE;
			release_dir_record(dir_tmp);
		} else { 	/* The dir record is valid. Copy data... */
			if (dir_tmp->file_id[0] == 0)
				strlcpy(name, ".", NAME_MAX + 1);
			else if (dir_tmp->file_id[0] == 1)
				strlcpy(name, "..", NAME_MAX + 1);
			else {
				/* Extract the name from the field file_id */
				strncpy(name, dir_tmp->file_id,
					dir_tmp->length_file_id);
				name[dir_tmp->length_file_id] = 0;
	  
				/* Tidy up file name */
				cp = memchr(name, ';', NAME_MAX); 
				if (cp != NULL) name[cp - name] = 0;
	  
				/*If no file extension, then remove final '.'*/
				if (name[strlen(name) - 1] == '.')
					name[strlen(name) - 1] = '\0';
			}

			if (strcmp(name_old, name) == 0) {
				cur_pos += dir_tmp->length;
				release_dir_record(dir_tmp);
				continue;
			}

			strlcpy(name_old, name, NAME_MAX + 1);

			/* Compute the length of the name */
			cp = memchr(name, '\0', NAME_MAX);
			if (cp == NULL)
				len = NAME_MAX;
			else
				len= cp - name;

			/* Compute record length; also does alignment. */
			reclen = _DIRENT_RECLEN(dirp, len);

			/* If the new record does not fit, then copy the buffer
			 * and start from the beginning. */
			if (tmpbuf_offset + reclen > GETDENTS_BUFSIZ) {
				r = sys_safecopyto(VFS_PROC_NR, gid, userbuf_off, 
				    (vir_bytes)getdents_buf, tmpbuf_offset);

				if (r != OK)
					panic("fs_getdents: sys_safecopyto failed: %d", r);
				userbuf_off += tmpbuf_offset;
				tmpbuf_offset= 0;
			}
	
			/* The standard data structure is created using the
			 * data in the buffer. */
			dirp = (struct dirent *) &getdents_buf[tmpbuf_offset];
			dirp->d_fileno = (u32_t)(b_data(bp) + (size_t)block_pos);
			dirp->d_reclen= reclen;
			dirp->d_type = fs_mode_to_type(dir_tmp->d_mode);
			dirp->d_namlen = len;
			memcpy(dirp->d_name, name, len);
			dirp->d_name[len]= '\0';
			tmpbuf_offset += reclen;
	
			cur_pos += dir_tmp->length;
			release_dir_record(dir_tmp);
		}

		block_pos += dir_tmp->length;
  	}
    
	put_block(bp);		/* release the block */
	if (done == TRUE) break;
    
	cur_pos += block_size - cur_pos;
	block++;			/* read the next one */
  }

  if (tmpbuf_offset != 0) {
	r = sys_safecopyto(VFS_PROC_NR, gid, userbuf_off,
			   (vir_bytes) getdents_buf, tmpbuf_offset);
	if (r != OK)
		panic("fs_getdents: sys_safecopyto failed: %d", r);
 
	userbuf_off += tmpbuf_offset;
  }
  
  fs_m_out.m_fs_vfs_getdents.nbytes = userbuf_off;
  fs_m_out.m_fs_vfs_getdents.seek_pos = cur_pos;

  release_dir_record(dir);		/* release the inode */
  return(OK);
}


/*===========================================================================*
 *				read_chunk				     *
 *===========================================================================*/
int read_chunk(dir, position, off, chunk, left, gid, buf_off, block_size, completed, rw)
register struct dir_record *dir;/* pointer to inode for file to be rd/wr */
u64_t position;			/* position within file to read or write */
unsigned off;			/* off within the current block */
int chunk;			/* number of bytes to read or write */
unsigned left;			/* max number of bytes wanted after position */
cp_grant_id_t gid;		/* grant */
unsigned buf_off;		/* offset in grant */
int block_size;			/* block size of FS operating on */
int *completed;			/* number of bytes copied */
int rw;				/* READING or PEEKING */
{

  register struct buf *bp;
  register int r = OK;
  block_t b;
  int file_unit, rel_block, offset;

  *completed = 0;

  if ((ex64lo(position) <= dir->d_file_size) && 
  				(ex64lo(position) > dir->data_length_l)) {
    while ((dir->d_next != NULL) && (ex64lo(position) > dir->data_length_l)) {
      position -= dir->data_length_l;
      dir = dir->d_next;
    }
  }

  if (dir->inter_gap_size != 0) {
    rel_block = (unsigned long)(position / block_size);
    file_unit = rel_block / dir->data_length_l;
    offset = rel_block % dir->file_unit_size;
    b = dir->loc_extent_l + (dir->file_unit_size +
    				 dir->inter_gap_size) * file_unit + offset;
  } else {
    b = dir->loc_extent_l + (unsigned long)(position / block_size); 
    					    /* Physical position to read. */
  }

  bp = get_block(b);

  /* In all cases, bp now points to a valid buffer. */
  if (bp == NULL) {
    panic("bp not valid in rw_chunk; this can't happen");
  }
  
  if(rw == READING) {
 	 r = sys_safecopyto(VFS_PROC_NR, gid, buf_off,
		     (vir_bytes) (b_data(bp)+off), (phys_bytes) chunk);
  }

  put_block(bp);

  return(r);
}

