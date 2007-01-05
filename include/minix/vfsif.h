
/* Fields of VFS/FS request messages */
#define REQ_INODE_NR             m6_l1
#define REQ_CHROOT_NR            m6_l2
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
#define REQ_XFD_NBYTES            m2_i3
#define REQ_XFD_POS_LO            m2_l1
#define REQ_XFD_POS_HI            m2_l2
#define REQ_XFD_USER_ADDR         m2_p1
#define REQ_XFD_BLOCK_SIZE        m2_s1

/* For REQ_GETDENTS */
#define REQ_GDE_INODE		 m2_i1
#define REQ_GDE_GRANT		 m2_i2
#define REQ_GDE_SIZE		 m2_i3
#define REQ_GDE_POS		 m2_l1

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

#define RES_UID                  m6_s3
#define RES_GID                  m6_c1
#define RES_CTIME                m6_l3

#define RES_FD_POS               m2_i1
#define RES_FD_CUM_IO            m2_i2
#define RES_FD_SIZE              m2_i3

#define RES_XFD_POS_LO           m2_l1
#define RES_XFD_POS_HI           m2_l2
#define RES_XFD_CUM_IO           m2_i1

#define RES_DIR                  m6_l1
#define RES_FILE                 m6_l2

#define RES_MAXSIZE              m6_l3
#define RES_BLOCKSIZE            m6_s2

/* For REQ_GETDENTS */
#define RES_GDE_POS_CHANGE	 m2_l1

/* Request numbers (offset in the fs callvector) */
#define REQ_GETNODE              1
#define REQ_PUTNODE              2
#define REQ_OPEN                 3
#define REQ_PIPE                 4
#define REQ_READ                 5
#define REQ_WRITE                6
#define REQ_CLONE_OPCL           7
#define REQ_FTRUNC               8
#define REQ_CHOWN                9
#define REQ_CHMOD                10
#define REQ_ACCESS               11
#define REQ_MKNOD                12
#define REQ_MKDIR                13
#define REQ_INHIBREAD            14
#define REQ_STAT                 15

#define REQ_CREATE		 16

#define REQ_UNLINK               17
#define REQ_RMDIR                18
#define REQ_UTIME                19

#define REQ_FSTATFS              21

#define REQ_LINK                 25

#define REQ_SLINK                26
#define REQ_RDLINK               27

#define REQ_RENAME               28

#define REQ_MOUNTPOINT           30
#define REQ_READSUPER            31
#define REQ_UNMOUNT              32
#define REQ_TRUNC                33
#define REQ_SYNC                 34

#define REQ_LOOKUP               35
#define REQ_STIME		 36
#define REQ_NEW_DRIVER           37

#define REQ_BREAD                38
#define REQ_BWRITE               39
#define REQ_GETDENTS		 40
#define REQ_FLUSH		 41

#define NREQS                    42

#define FS_READY                 57

#define EENTERMOUNT              301 
#define ELEAVEMOUNT              302
#define ESYMLINK                 303

