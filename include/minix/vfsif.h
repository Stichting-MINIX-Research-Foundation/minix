
/* Fields of VFS/FS request messages */
#define REQ_INODE_NR             m6_l1
#define REQ_CHROOT_NR            m6_l2
#define REQ_DEVx                 m6_l2
#define REQ_GRANT2               m6_l2
#define REQ_UID                  m6_s1
#define REQ_GID                  m6_c1
#define REQ_MODE                 m6_s3
#define REQ_PATH                 m6_p1
#define REQ_PATH_LEN             m6_s2
#define REQ_FLAGS                m6_l3
#define REQ_DEV                  m6_l3
#define REQ_WHO_E                m6_l3
#define REQ_GRANT                m6_l3
#define REQ_USER_ADDR            m6_p2
#define REQ_LENGTH               m6_l3
#define REQ_SYMLOOP              m6_c2
#define REQ_COUNT		 m6_l2

#define REQ_NEW_UID              m6_s3
#define REQ_NEW_GID              m6_c2

#define REQ_INODE_INDEX          m6_l3

#define REQ_ACTIME               m6_l2
#define REQ_MODTIME              m6_l3

#define REQ_VMNT_IND             m6_c2
#define REQ_SLINK_STORAGE        m6_p1
#define REQ_BOOTTIME             m6_l1
#define REQ_DRIVER_E             m6_l2
#define REQ_READONLY             m6_c1
#define REQ_ISROOT		 m6_c2

#define REQ_REMOUNT              m6_c2

#define REQ_LINKED_FILE          m6_l1
#define REQ_LINK_PARENT          m6_l2

#define REQ_OLD_DIR              m6_l2
#define REQ_NEW_DIR              m6_l3
#define REQ_SLENGTH              m6_s3

#define REQ_PIPE_POS             m6_l1

#define REQ_FD_INODE_NR          m2_i1
#define REQ_FD_WHO_E             m2_i2
#define REQ_FD_GID               m2_i2
#define REQ_FD_POS               m2_i3
#define REQ_FD_NBYTES            m2_l1
#define REQ_FD_SEG               m2_l2
#define REQ_FD_INODE_INDEX       m2_s1

#define REQ_FD_USER_ADDR         m2_p1
#define REQ_FD_LENGTH            m2_i2
#define REQ_FD_START             m2_i2
#define REQ_FD_END               m2_i3

#define REQ_FD_BDRIVER_E         m2_i1

#define REQ_XFD_BDEV              m2_i1
#define REQ_XFD_WHO_E             m2_i2
#define REQ_XFD_GID               m2_i2
#define REQ_XFD_NBYTES            m2_i3
#define REQ_XFD_POS_LO            m2_l1
#define REQ_XFD_POS_HI            m2_l2
#define REQ_XFD_USER_ADDR         m2_p1
/* #define REQ_XFD_BLOCK_SIZE        m2_s1 */

#define REQ_L_GRANT		m9_l1
#define REQ_L_PATH_LEN		m9_s1
#define REQ_L_PATH_SIZE		m9_s2
#define REQ_L_PATH_OFF		m9_l2
#define REQ_L_DIR_INO		m9_l3
#define REQ_L_ROOT_INO		m9_l4
#define REQ_L_FLAGS		m9_c1
#define REQ_L_UID		m9_s3
#define REQ_L_GID		m9_c2

/* For REQ_GETDENTS */
#define REQ_GDE_INODE		 m2_i1
#define REQ_GDE_GRANT		 m2_i2
#define REQ_GDE_SIZE		 m2_i3
#define REQ_GDE_POS		 m2_l1

/* For REQ_RENAME_S */
#define REQ_REN_OLD_DIR		m2_l1
#define REQ_REN_NEW_DIR		m2_l2
#define REQ_REN_GRANT_OLD	m2_i1
#define REQ_REN_LEN_OLD		m2_i2
#define REQ_REN_GRANT_NEW	m2_i3
#define REQ_REN_LEN_NEW		m2_s1

/* Fields of VFS/FS respons messages */
#define RES_MOUNTED              m6_s1
#define RES_OFFSET               m6_s2
#define RES_INODE_NR             m6_l1
#define RES_MODE                 m6_s1
#define RES_FILE_SIZE            m6_l2
#define RES_DEV                  m6_l3
#define RES_INODE_INDEX          m6_s2
#define RES_NLINKS               m6_s3
#define RES_SYMLOOP              m6_c1
#define RES_SYMLOOP2             m6_c2

#define RES_UID                  m6_s3
#define RES_GID                  m6_c1
#define RES_CTIME                m6_l3 	/* Should be removed */

#define RES_FD_POS               m2_i1
#define RES_FD_CUM_IO            m2_i2
#define RES_FD_SIZE              m2_i3

#define RES_XFD_POS_LO           m2_l1
#define RES_XFD_POS_HI           m2_l2
#define RES_XFD_CUM_IO           m2_i1

#define RES_DIR                  m6_l1
#define RES_FILE                 m6_l2

#define RES_MAXSIZE              m6_l3	/* Should be removed */
#define RES_BLOCKSIZE            m6_s2	/* Should be removed */

/* For REQ_GETDENTS */
#define RES_GDE_POS_CHANGE	 m2_l1

/* Request numbers */
#define REQ_GETNODE	(VFS_BASE + 1)	/* Should be removed */
#define REQ_PUTNODE	(VFS_BASE + 2)
#define REQ_SLINK_S	(VFS_BASE + 3)
#define REQ_PIPE	(VFS_BASE + 4)	/* Replaced with REQ_NEWNODE */
#define REQ_READ_O	(VFS_BASE + 5)	/* Replaced with REQ_READ_S */
#define REQ_WRITE_O	(VFS_BASE + 6)	/* Replaced with REQ_WRITE_S */
#define REQ_CLONE_OPCL	(VFS_BASE + 7)	/* Replaced with REQ_NEWNODE */
#define REQ_FTRUNC	(VFS_BASE + 8)
#define REQ_CHOWN	(VFS_BASE + 9)
#define REQ_CHMOD	(VFS_BASE + 10)
#define REQ_ACCESS_O	(VFS_BASE + 11)	/* Removed */
#define REQ_MKNOD_O	(VFS_BASE + 12)	/* Replaced with REQ_MKNOD_S */
#define REQ_MKDIR_O	(VFS_BASE + 13)	/* Replaced with REQ_MKDIR_S */
#define REQ_INHIBREAD	(VFS_BASE + 14)
#define REQ_STAT	(VFS_BASE + 15)
#define REQ_CREATE_O	(VFS_BASE + 16)	/* Replaced with REQ_CREATE_S */
#define REQ_UNLINK_O	(VFS_BASE + 17)	/* Replaced with REQ_UNLINK_S */
#define REQ_RMDIR_O	(VFS_BASE + 18)	/* Replaced with REQ_RMDIR_S */
#define REQ_UTIME	(VFS_BASE + 19)
#define REQ_RDLINK_S	(VFS_BASE + 20)
#define REQ_FSTATFS	(VFS_BASE + 21)
#define REQ_BREAD_S	(VFS_BASE + 22)
#define REQ_BWRITE_S	(VFS_BASE + 23)
#define REQ_UNLINK_S	(VFS_BASE + 24)
#define REQ_LINK_O	(VFS_BASE + 25)	/* Replaced with REQ_LINK_S */
#define REQ_SLINK_O	(VFS_BASE + 26)	/* Replaced with REQ_SLINK_S */
#define REQ_RDLINK_O	(VFS_BASE + 27)	/* Replaced with REQ_RDLINK_S */
#define REQ_RENAME_O	(VFS_BASE + 28)	/* Replaced with REQ_RENAME_S */
#define REQ_RMDIR_S	(VFS_BASE + 29)
#define REQ_MOUNTPOINT_O (VFS_BASE + 30)	/* Replaced with REQ_MOUNTPOINT_S */
#define REQ_READSUPER_O	(VFS_BASE + 31)	/* Replaced with REQ_READSUPER_S */
#define REQ_UNMOUNT	(VFS_BASE + 32)
#define REQ_TRUNC	(VFS_BASE + 33)	/* Should be removed */
#define REQ_SYNC	(VFS_BASE + 34)
#define REQ_LOOKUP_O	(VFS_BASE + 35)	/* Replaced with REQ_LOOKUP_S */
#define REQ_STIME	(VFS_BASE + 36)	/* To be removed */
#define REQ_NEW_DRIVER	(VFS_BASE + 37)
#define REQ_BREAD_O	(VFS_BASE + 38)	/* Replaced with REQ_BREAD_S */
#define REQ_BWRITE_O	(VFS_BASE + 39)	/* Replaced with REQ_BWRITE_S */
#define REQ_GETDENTS	(VFS_BASE + 40)
#define REQ_FLUSH	(VFS_BASE + 41)
#define REQ_READ_S	(VFS_BASE + 42)
#define REQ_WRITE_S	(VFS_BASE + 43)
#define REQ_MKNOD_S	(VFS_BASE + 44)
#define REQ_MKDIR_S	(VFS_BASE + 45)
#define REQ_CREATE_S	(VFS_BASE + 46)
#define REQ_LINK_S	(VFS_BASE + 47)
#define REQ_RENAME_S	(VFS_BASE + 48)
#define REQ_LOOKUP_S	(VFS_BASE + 49)
#define REQ_MOUNTPOINT_S (VFS_BASE + 50)
#define REQ_READSUPER_S	(VFS_BASE + 51)
#define REQ_NEWNODE	(VFS_BASE + 52)

#define NREQS                    53


#define EENTERMOUNT              301 
#define ELEAVEMOUNT              302
#define ESYMLINK                 303

/* REQ_L_FLAGS */
#define PATH_RET_SYMLINK	1	/* Return a symlink object (i.e.
					 * do not continue with the contents
					 * of the symlink if it is the last
					 * component in a path).
					 */
