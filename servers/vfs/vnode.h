

EXTERN struct vnode {
  endpoint_t v_fs_e;            /* FS process' endpoint number */
  endpoint_t v_mapfs_e;		/* mapped FS process' endpoint number */
  ino_t v_inode_nr;		/* inode number on its (minor) device */
  ino_t v_mapinode_nr;		/* mapped inode number of mapped FS. */
  mode_t v_mode;		/* file type, protection, etc. */
  uid_t v_uid;			/* uid of inode. */
  gid_t v_gid;			/* gid of inode. */
  off_t v_size;			/* current file size in bytes */
  int v_ref_count;		/* # times vnode used; 0 means slot is free */
  int v_fs_count;		/* # reference at the underlying FS */
  int v_mapfs_count;		/* # reference at the underlying mapped FS */
#if 0
  int v_ref_check;		/* for consistency checks */
#endif
  char v_pipe;			/* set to I_PIPE if pipe */
  off_t v_pipe_rd_pos;
  off_t v_pipe_wr_pos;
  endpoint_t v_bfs_e;		/* endpoint number for the FS proces in case
				   of a block special file */
  dev_t v_dev;                  /* device number on which the corresponding 
                                   inode resides */
  dev_t v_sdev;                 /* device number for special files */
  struct vmnt *v_vmnt;          /* vmnt object of the partition */
} vnode[NR_VNODES];


/* Field values. */
#define NO_PIPE            0	/* i_pipe is NO_PIPE if inode is not a pipe */
#define I_PIPE             1	/* i_pipe is I_PIPE if inode is a pipe */

