#include "inc.h"
#include <minix/com.h>
#include <minix/vfsif.h>
#include <fcntl.h>
#ifdef __NBSD_LIBC
#include <stddef.h>
#endif
#include "buf.h"

static char getdents_buf[GETDENTS_BUFSIZ];


/*===========================================================================*
 *				fs_read					     *
 *===========================================================================*/
int fs_read(void) {
  int r, chunk, block_size;
  int nrbytes;
  cp_grant_id_t gid;
  off_t position, f_size, bytes_left;
  unsigned int off, cum_io;
  int completed;
  struct dir_record *dir;
  
  r = OK;
  
  /* Try to get inode according to its index */
  dir = get_dir_record(fs_m_in.REQ_INODE_NR);
  if (dir == NULL) return(EINVAL); /* no inode found */

  position = fs_m_in.REQ_SEEK_POS_LO; 
  nrbytes = (unsigned) fs_m_in.REQ_NBYTES; /* number of bytes to read */
  block_size = v_pri.logical_block_size_l;
  gid = fs_m_in.REQ_GRANT;
  f_size = dir->d_file_size;

  rdwt_err = OK;		/* set to EIO if disk error occurs */
  
  cum_io = 0;
  /* Split the transfer into chunks that don't span two blocks. */
  while (nrbytes != 0) {
	off = (unsigned int) (position % block_size);
          
	chunk = MIN(nrbytes, block_size - off);
	if (chunk < 0) chunk = block_size - off;

	bytes_left = f_size - position;
	if (position >= f_size) break;	/* we are beyond EOF */
	if (chunk > bytes_left) chunk = (int) bytes_left;
      
	/* Read or write 'chunk' bytes. */
	r = read_chunk(dir, cvul64(position), off, chunk, (unsigned) nrbytes, 
		       gid, cum_io, block_size, &completed);

	if (r != OK) break;	/* EOF reached */
	if (rdwt_err < 0) break;

	/* Update counters and pointers. */
	nrbytes -= chunk;	/* bytes yet to be read */
	cum_io += chunk;	/* bytes read so far */
	position += chunk;	/* position within the file */
  }

  fs_m_out.RES_SEEK_POS_LO = position; 
  
  if (rdwt_err != OK) r = rdwt_err;	/* check for disk error */
  if (rdwt_err == END_OF_FILE) r = OK;
  
  fs_m_out.RES_NBYTES = cum_io; /*dir->d_file_size;*/
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
  gid = fs_m_in.REQ_GRANT;
  position = make64(fs_m_in.REQ_SEEK_POS_LO, fs_m_in.REQ_SEEK_POS_HI);
  nrbytes = (unsigned) fs_m_in.REQ_NBYTES;
  block_size = v_pri.logical_block_size_l;
  dir = v_pri.dir_rec_root;

  if(rw_flag == WRITING) return (EIO);	/* Not supported */
  rdwt_err = OK;		/* set to EIO if disk error occurs */
  
  cum_io = 0;
  /* Split the transfer into chunks that don't span two blocks. */
  while (nrbytes != 0) {
    off = rem64u(position, block_size);	/* offset in blk*/
    
    chunk = MIN(nrbytes, block_size - off);
    if (chunk < 0) chunk = block_size - off;

	/* Read 'chunk' bytes. */
    r = read_chunk(dir, position, off, chunk, (unsigned) nrbytes, 
		   gid, cum_io, block_size, &completed);
    
    if (r != OK) break;	/* EOF reached */
    if (rdwt_err < 0) break;
    
    /* Update counters and pointers. */
    nrbytes -= chunk;	        /* bytes yet to be read */
    cum_io += chunk;	        /* bytes read so far */
    position= add64ul(position, chunk);	/* position within the file */
  }
  
  fs_m_out.RES_SEEK_POS_LO = ex64lo(position); 
  fs_m_out.RES_SEEK_POS_HI = ex64hi(position); 
  
  if (rdwt_err != OK) r = rdwt_err;	/* check for disk error */
  if (rdwt_err == END_OF_FILE) r = OK;

  fs_m_out.RES_NBYTES = cum_io;
  
  return(r);
}


/*===========================================================================*
 *				fs_getdents				     *
 *===========================================================================*/
int fs_getdents(void) {
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
  ino = fs_m_in.REQ_INODE_NR;
  gid = fs_m_in.REQ_GRANT;
  pos = fs_m_in.REQ_SEEK_POS_LO;

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
			if (cp == NULL) len = NAME_MAX;
			else len= cp - name;

			/* Compute record length */
			reclen = offsetof(struct dirent, d_name) + len + 1;
			o = (reclen % sizeof(long));
			if (o != 0)
				reclen += sizeof(long) - o;

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
			dirp->d_ino = (ino_t)(b_data(bp) + block_pos);
			dirp->d_off= cur_pos;
			dirp->d_reclen= reclen;
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
  
  fs_m_out.RES_NBYTES = userbuf_off;
  fs_m_out.RES_SEEK_POS_LO = cur_pos;

  release_dir_record(dir);		/* release the inode */
  return(OK);
}


/*===========================================================================*
 *				read_chunk				     *
 *===========================================================================*/
int read_chunk(dir, position, off, chunk, left, gid, buf_off, block_size, completed)
register struct dir_record *dir;/* pointer to inode for file to be rd/wr */
u64_t position;			/* position within file to read or write */
unsigned off;			/* off within the current block */
int chunk;			/* number of bytes to read or write */
unsigned left;			/* max number of bytes wanted after position */
cp_grant_id_t gid;		/* grant */
unsigned buf_off;		/* offset in grant */
int block_size;			/* block size of FS operating on */
int *completed;			/* number of bytes copied */
{

  register struct buf *bp;
  register int r = OK;
  block_t b;
  int file_unit, rel_block, offset;

  *completed = 0;

  if ((ex64lo(position) <= dir->d_file_size) && 
  				(ex64lo(position) > dir->data_length_l)) {
    while ((dir->d_next != NULL) && (ex64lo(position) > dir->data_length_l)) {
      position = sub64ul(position, dir->data_length_l);
      dir = dir->d_next;
    }
  }

  if (dir->inter_gap_size != 0) {
    rel_block = div64u(position, block_size);
    file_unit = rel_block / dir->data_length_l;
    offset = rel_block % dir->file_unit_size;
    b = dir->loc_extent_l + (dir->file_unit_size +
    				 dir->inter_gap_size) * file_unit + offset;
  } else {
    b = dir->loc_extent_l + div64u(position, block_size); /* Physical position
							    * to read. */
  }

  bp = get_block(b);

  /* In all cases, bp now points to a valid buffer. */
  if (bp == NULL) {
    panic("bp not valid in rw_chunk; this can't happen");
  }
  
  r = sys_safecopyto(VFS_PROC_NR, gid, buf_off,
		     (vir_bytes) (b_data(bp)+off), (phys_bytes) chunk);

  put_block(bp);

  return(r);
}

