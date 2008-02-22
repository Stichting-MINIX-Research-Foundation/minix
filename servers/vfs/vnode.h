

EXTERN struct vnode {
  endpoint_t v_fs_e;            /* FS process' endpoint number */
  ino_t v_inode_nr;		/* inode number on its (minor) device */
  mode_t v_mode;		/* file type, protection, etc. */
  uid_t v_uid;
  gid_t v_gid;
  off_t v_size;			/* current file size in bytes */
  int v_ref_count;		/* # times vnode used; 0 means slot is free */
  int v_fs_count;		/* # reference at the underlying FS */
  int v_ref_check;		/* for consistency checks */
  char v_pipe;			/* set to I_PIPE if pipe */
  off_t v_pipe_rd_pos;
  off_t v_pipe_wr_pos;
  endpoint_t v_bfs_e;		/* endpoint number for the FS proces in case
				   of a block special file */
  
  dev_t v_dev;                  /* device number on which the corresponding 
                                   inode resides */
  
  dev_t v_sdev;                 /* device number for special files */
  unsigned short v_index;       /* inode's index in the FS inode table */
  struct vmnt *v_vmnt;          /* vmnt object of the partition */

  /* For debugging */
  char *v_file;
  int v_line;
} vnode[NR_VNODES];

#define NIL_VNODE (struct vnode *) 0	/* indicates absence of vnode slot */

/* Field values.  Note that CLEAN and DIRTY are defined in "const.h" */
#define NO_PIPE            0	/* i_pipe is NO_PIPE if inode is not a pipe */
#define I_PIPE             1	/* i_pipe is I_PIPE if inode is a pipe */
#define NO_MOUNT           0	/* i_mount is NO_MOUNT if file not mounted on*/
#define I_MOUNT            1	/* i_mount is I_MOUNT if file mounted on */
#define NO_SEEK            0	/* i_seek = NO_SEEK if last op was not SEEK */
#define ISEEK              1	/* i_seek = ISEEK if last op was SEEK */


