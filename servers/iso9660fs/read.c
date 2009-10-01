
/* Functions to reads_file */

#include "inc.h"
#include <minix/com.h>
#include <minix/vfsif.h>
#include <fcntl.h>
#include "buf.h"

FORWARD _PROTOTYPE( int read_chunk_s, (struct dir_record *rip, off_t position,
				     unsigned off, int chunk, unsigned left,cp_grant_id_t gid,
				     unsigned buf_off, int block_size, int *completed));

/*===========================================================================*
 *				fs_read_s				     *
 *===========================================================================*/
PUBLIC int fs_read_s(void) {

  int r, chunk, block_size;
  int nrbytes;
  cp_grant_id_t gid;
  off_t position, f_size, bytes_left;
  unsigned int off, cum_io;
  int completed;
  struct dir_record *dir;
  
  r = OK;
  
  /* Try to get inode according to its index */
  dir = get_dir_record(fs_m_in.REQ_FD_INODE_NR);
  if (dir == NULL) return EINVAL; /* No inode found */


  position = fs_m_in.REQ_FD_POS; /* start reading from this position */
  nrbytes = (unsigned) fs_m_in.REQ_FD_NBYTES; /* number of bytes to read */
  block_size = v_pri.logical_block_size_l;
  gid = fs_m_in.REQ_FD_GID;
  f_size = dir->d_file_size;

  rdwt_err = OK;		/* set to EIO if disk error occurs */
  
  cum_io = 0;
  /* Split the transfer into chunks that don't span two blocks. */
  while (nrbytes != 0) {
      off = (unsigned int) (position % block_size);/* offset in blk*/
          
      chunk = MIN(nrbytes, block_size - off);
      if (chunk < 0) chunk = block_size - off;

      bytes_left = f_size - position;
      if (position >= f_size) break;	/* we are beyond EOF */
      if (chunk > bytes_left) chunk = (int) bytes_left;
      
      /* Read or write 'chunk' bytes. */
      r = read_chunk_s(dir, position, off, chunk, (unsigned) nrbytes, 
		   gid, cum_io, block_size, &completed);

      if (r != OK) break;	/* EOF reached */
      if (rdwt_err < 0) break;

      /* Update counters and pointers. */
      nrbytes -= chunk;	/* bytes yet to be read */
      cum_io += chunk;	/* bytes read so far */
      position += chunk;	/* position within the file */
  }

  fs_m_out.RES_FD_POS = position; /* It might change later and the VFS has
				     to know this value */
  
  if (rdwt_err != OK) r = rdwt_err;	/* check for disk error */
  if (rdwt_err == END_OF_FILE) r = OK;

/*   rip->i_update |= ATIME; */
  
  fs_m_out.RES_FD_CUM_IO = cum_io;
  fs_m_out.RES_FD_SIZE = dir->d_file_size;
  release_dir_record(dir);

  return(r);
}

/* Function that is called with the request read. It performs the read and
 * returns the answer. */
/*===========================================================================*
 *				fs_read		         		     *
 *===========================================================================*/
PUBLIC int fs_read(void) {
  struct dir_record *dir;
  int r,nrbytes,block_size, chunk, completed, seg, usr;
  char* user_addr;
  off_t bytes_left, position;
  unsigned int off, cum_io;

  r = OK;

  dir = get_dir_record(fs_m_in.REQ_FD_INODE_NR);
  if (dir == NULL) return EINVAL; /* No inode found */

  /* Get values for reading */
  position = fs_m_in.REQ_FD_POS; /* start reading from this position */
  nrbytes = (unsigned) fs_m_in.REQ_FD_NBYTES; /* number of bytes to read */
  user_addr = fs_m_in.REQ_FD_USER_ADDR;	/* user addr buffer */
  usr = fs_m_in.REQ_FD_WHO_E;
  seg = fs_m_in.REQ_FD_SEG;
  block_size = v_pri.logical_block_size_l;

  cum_io = 0;
  while (nrbytes != 0) {
    off = (unsigned int) (position % block_size); /* offset in blk*/

    chunk = MIN(nrbytes, block_size - off); 
    if (chunk < 0) chunk = block_size - off;
    bytes_left = dir->d_file_size - position;
    if (position >= dir->d_file_size) break;	/* we are beyond EOF */
    if (chunk > bytes_left) chunk = (int) bytes_left;

    /* Read chunk of block. */
    r = read_chunk(dir, cvul64(position), off, chunk,
		   user_addr, seg, usr, block_size, &completed);
    if (r != OK)
      break;			/* EOF reached */
    if (rdwt_err < 0) break;


    user_addr += chunk;	/* user buffer address */
    nrbytes -= chunk;	/* bytes yet to be read */
    cum_io += chunk;	/* bytes read so far */
    position += chunk;	/* position within the file */
  }

  fs_m_out.RES_FD_POS = position; /* return the position now within the file */
  fs_m_out.RES_FD_CUM_IO = cum_io;
  fs_m_out.RES_FD_SIZE = dir->data_length_l; /* returns the size of the file */

  release_dir_record(dir);	/* release the dir record. */
  return r;
}

/*===========================================================================*
 *				fs_bread_s				     *
 *===========================================================================*/
PUBLIC int fs_bread_s(void) {
  return fs_bread();
}

/*===========================================================================*
 *				fs_bread				     *
 *===========================================================================*/
PUBLIC int fs_bread(void)
{
  int r, usr, rw_flag, chunk, block_size, seg;
  int nrbytes;
  u64_t position;
  unsigned int off, cum_io;
  mode_t mode_word;
  int completed, r2 = OK;
  char *user_addr;
  
  /* This function is called when it is requested a raw reading without any
   * conversion. It is similar to fs_read but here the data is returned
   * without any preprocessing. */
  r = OK;
  
  /* Get the values from the request message */ 
  rw_flag = (fs_m_in.m_type == REQ_BREAD_S ? READING : WRITING);
  usr = fs_m_in.REQ_XFD_WHO_E;
  position = make64(fs_m_in.REQ_XFD_POS_LO, fs_m_in.REQ_XFD_POS_HI);
  nrbytes = (unsigned) fs_m_in.REQ_XFD_NBYTES;
  user_addr = fs_m_in.REQ_XFD_USER_ADDR;
  seg = fs_m_in.REQ_FD_SEG;
  
  block_size = v_pri.logical_block_size_l;

  rdwt_err = OK;		/* set to EIO if disk error occurs */
  
  cum_io = 0;
  /* Split the transfer into chunks that don't span two blocks. */
  while (nrbytes != 0) {
    off = rem64u(position, block_size);	/* offset in blk*/
    
    chunk = MIN(nrbytes, block_size - off);
    if (chunk < 0) chunk = block_size - off;
    
    /* Read or write 'chunk' bytes. */
    r = read_chunk(NULL, position, off, chunk,
		   user_addr, seg, usr, block_size, &completed);
    
    if (r != OK) break;	/* EOF reached */
    if (rdwt_err < 0) break;
    
    /* Update counters and pointers. */
    user_addr += chunk;	/* user buffer address */
    nrbytes -= chunk;	        /* bytes yet to be read */
    cum_io += chunk;	        /* bytes read so far */
    position= add64ul(position, chunk);	/* position within the file */
  }
  
  fs_m_out.RES_XFD_POS_LO = ex64lo(position); 
  fs_m_out.RES_XFD_POS_HI = ex64hi(position); 
  
  if (rdwt_err != OK) r = rdwt_err;	/* check for disk error */
  if (rdwt_err == END_OF_FILE) r = OK;

  fs_m_out.RES_XFD_CUM_IO = cum_io;
  
  return(r);
}

#define GETDENTS_BUFSIZ	257

PRIVATE char getdents_buf[GETDENTS_BUFSIZ];

/* This function returns the content of a directory using the ``standard"
 * data structure (that are non FS dependent). */
/*===========================================================================*
 *				fs_getdents				     *
 *===========================================================================*/
PUBLIC int fs_getdents(void) {
  struct dir_record *dir;
  ino_t ino;
  cp_grant_id_t gid;
  size_t size_to_read, block_size;
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
  ino= fs_m_in.REQ_GDE_INODE;
  gid= fs_m_in.REQ_GDE_GRANT;
  size_to_read = fs_m_in.REQ_GDE_SIZE;
  pos= fs_m_in.REQ_GDE_POS;

  block_size = v_pri.logical_block_size_l;
  cur_pos = pos;		/* The current position */
  tmpbuf_offset = 0;
  userbuf_off = 0;
  memset(getdents_buf, '\0', GETDENTS_BUFSIZ);	/* Avoid leaking any data */

  if ((dir = get_dir_record(ino)) == NULL) {
    printf("I9660FS(%d) get_dir_record by fs_getdents() failed\n", SELF_E);
    return(EINVAL);
  }

  block = dir->loc_extent_l;	/* First block of the directory */

  block += pos / block_size; 	/* Shift to the block where start to read */
  done = FALSE;

  while (cur_pos<dir->d_file_size) {
    bp = get_block(block);	/* Get physical block */

    if (bp == NIL_BUF) {
      release_dir_record(dir);
      return EINVAL;
    }
    
    block_pos = cur_pos % block_size; /* Position where to start read */

    while (block_pos<block_size) {
      dir_tmp = get_free_dir_record();
      create_dir_record(dir_tmp,bp->b_data + block_pos,
			block*block_size + block_pos);
      if (dir_tmp->length == 0) { /* EOF. I exit and return 0s */
	block_pos = block_size;
	done = TRUE;
	release_dir_record(dir_tmp);
      } else { 			/* The dir record is valid. Copy data... */
 	if (dir_tmp->file_id[0] == 0) strcpy(name,".");
 	else if (dir_tmp->file_id[0] == 1) strcpy(name,"..");
	else {
	  /* These next functions will extract the name from the field
	   * file_id */
	  strncpy(name,dir_tmp->file_id,dir_tmp->length_file_id);
	  name[dir_tmp->length_file_id] = 0;
	  cp = memchr(name, ';', NAME_MAX); /* Remove the final part of the
					     * dir name. */
	  if (cp != NULL)
	    name[cp - name] = 0;
	  
	  /* If there is no extension I remove the last '.' */
	  if (name[strlen(name) - 1] == '.')
	    name[strlen(name) - 1] = '\0';

	}

	if (strcmp(name_old,name) == 0) {
	  cur_pos += dir_tmp->length;
	  release_dir_record(dir_tmp);
	  continue;
	}
	strcpy(name_old,name);

	/* Compute the length of the name */
	cp= memchr(name, '\0', NAME_MAX);
	if (cp == NULL) len= NAME_MAX;
	else len= cp - name;

	/* Compute record length */
	reclen= offsetof(struct dirent, d_name) + len + 1;
	o= (reclen % sizeof(long));
	if (o != 0)
	  reclen += sizeof(long)-o;

	/* If the new record does not fit I copy the buffer and I start
	 * from the beginning. */
	if (tmpbuf_offset + reclen > GETDENTS_BUFSIZ) {
	  r= sys_safecopyto(FS_PROC_NR, gid, userbuf_off, 
			    (vir_bytes)getdents_buf, tmpbuf_offset, D);
	  if (r != OK)
	    panic(__FILE__,"fs_getdents: sys_safecopyto failed\n",r);
	  
	  userbuf_off += tmpbuf_offset;
	  tmpbuf_offset= 0;
	}

	/* The standard data structure is created using the data in the
	 * buffer. */
	dirp = (struct dirent *)&getdents_buf[tmpbuf_offset];
	dirp->d_ino = (ino_t)(bp->b_data + block_pos);
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
    r= sys_safecopyto(FS_PROC_NR, gid, userbuf_off,
		      (vir_bytes)getdents_buf, tmpbuf_offset, D);
    if (r != OK)
      panic(__FILE__, "fs_getdents: sys_safecopyto failed\n", r);
 
    userbuf_off += tmpbuf_offset;
  }
  
  r= ENOSYS;
  fs_m_out.RES_GDE_POS_CHANGE= 0;	/* No change in case of an error */

  /*  r= userbuf_off;*/
  fs_m_out.RES_GDE_CUM_IO = userbuf_off;
  if (cur_pos >= pos)
    fs_m_out.RES_GDE_POS_CHANGE= cur_pos-pos;
  else
    fs_m_out.RES_GDE_POS_CHANGE= 0;

  release_dir_record(dir);		/* release the inode */
  r= OK;
  return(r);
}

/*===========================================================================*
 *                           fs_getdents_s                                   *
 *===========================================================================*/
PUBLIC int fs_getdents_o(void)
{
  int r;
  r = fs_getdents();

  if(r == OK)
    r = fs_m_out.RES_GDE_CUM_IO;

  return(r);
}

/* Read a chunk of data that does not span in two blocks. */
PUBLIC int read_chunk(dir, position, off, chunk, buff, seg, usr, block_size, 
		      completed)
     struct  dir_record *dir;		/* file to read */
     u64_t position;			/* position within file to read or write */
     unsigned off;			/* off within the current block */
     int chunk;			        /* number of bytes to read or write */
     char *buff;			/* virtual address of the user buffer */
     int seg;			        /* T or D segment in user space */
     int usr;			        /* which user process */
     int block_size;			/* block size of FS operating on */
     int *completed;			/* number of bytes copied */
{
  register struct buf *bp;
  register int r = OK;
  block_t b;

  b = dir->loc_extent_l + div64u(position, block_size);	/* Physical position
							 * to read. */

  bp = get_block(b);		/* Get physical block */
  if (bp == NIL_BUF)
    return EINVAL;

  r = sys_vircopy(SELF_E, D, (phys_bytes) (bp->b_data+off),
		  usr, seg, (phys_bytes) buff,
		  (phys_bytes) chunk);

  if (r != OK)
    panic(__FILE__,"fs_getdents: sys_vircopy failed\n",r);

  put_block(bp);		/* Return the block */
  return OK;
}

/* Read a chunk of data that does not span in two blocks. */
/*===========================================================================*
 *				read_chunk_s				     *
 *===========================================================================*/
PRIVATE int read_chunk_s(dir, position, off, chunk, left, gid, buf_off, block_size, completed)
register struct dir_record *dir;/* pointer to inode for file to be rd/wr */
off_t position;			/* position within file to read or write */
unsigned off;			/* off within the current block */
int chunk;			/* number of bytes to read or write */
unsigned left;			/* max number of bytes wanted after position */
cp_grant_id_t gid;		/* grant */
unsigned buf_off;		/* offset in grant */
int block_size;			/* block size of FS operating on */
int *completed;			/* number of bytes copied */
{
/* Read or write (part of) a block. */

  register struct buf *bp;
  register int r = OK;
  int n;
  block_t b;
  dev_t dev;
  int file_unit, rel_block, offset;

  *completed = 0;

  if ((position <= dir->d_file_size) && (position > dir->data_length_l)) {
    while ((dir->d_next != NULL) && (position > dir->data_length_l)) {
      position -= dir->data_length_l;
      dir = dir->d_next;
    }
  }

  if (dir->inter_gap_size != 0) {
    rel_block = position / block_size;
    file_unit = rel_block / dir->data_length_l;
    offset = rel_block % dir->file_unit_size;
    b = dir->loc_extent_l + (dir->file_unit_size + dir->inter_gap_size) * file_unit + offset;
  } else {
    b = dir->loc_extent_l + position / block_size;	/* Physical position
							 * to read. */
  }

  bp = get_block(b);

  /* In all cases, bp now points to a valid buffer. */
  if (bp == NIL_BUF) {
    panic(__FILE__,"bp not valid in rw_chunk, this can't happen", NO_NUM);
  }
  
  r = sys_safecopyto(FS_PROC_NR, gid, buf_off,
		     (vir_bytes) (bp->b_data+off), (phys_bytes) chunk, D);

  put_block(bp);

  return(r);
}
