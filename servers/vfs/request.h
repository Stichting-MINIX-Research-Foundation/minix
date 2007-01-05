
/* Low level request messages are built and sent by wrapper functions.
 * This file contains the request and response structures for accessing
 * those wrappers functions.
 */

#include <sys/types.h>


/* Structure for REQ_GETNODE and REQ_PUTNODE requests */
typedef struct node_req {
	endpoint_t fs_e;
	ino_t inode_nr;
} node_req_t;


/* Structure for response that contains inode details */
typedef struct node_details {
	endpoint_t fs_e;
	ino_t inode_nr;
	mode_t fmode;
	off_t fsize;
	unsigned short inode_index;
	/* For char/block special files */
	dev_t dev;
	
	/* Fields used by the exec() syscall */
	uid_t uid;
	gid_t gid;
	time_t ctime;
} node_details_t;


/* Structure for REQ_OPEN request */
typedef struct open_req {
        endpoint_t fs_e;
        ino_t inode_nr;
	char *lastc;
	int oflags;
	mode_t omode;	
	uid_t uid;
	gid_t gid;
} open_req_t;


/* Structure for REQ_READ and REQ_WRITE request */
typedef struct readwrite_req {
	int rw_flag;
	endpoint_t fs_e;
        endpoint_t user_e;
	ino_t inode_nr;
	unsigned short inode_index;
	int seg;
	u64_t pos;
	unsigned int num_of_bytes;
	char *user_addr;
} readwrite_req_t;


/* Structure for response of REQ_READ and REQ_WRITE */
typedef struct readwrite_res {
	u64_t new_pos;
	unsigned int cum_io;
} readwrite_res_t;


/* Structure for REQ_PIPE request */
typedef struct pipe_req {
        int fs_e;
        uid_t uid;
        gid_t gid;
} pipe_req_t;


/* Structure for REQ_CLONE_OPCL request */
typedef struct clone_opcl_req {
        int fs_e;
        dev_t dev;
} clone_opcl_req_t;


/* Structure for REQ_FTRUNC request */
typedef struct ftrunc_req {
        int fs_e;
        ino_t inode_nr;
        off_t start;
        off_t end;
} ftrunc_req_t;


/* Structure for REQ_CHOWN request */
typedef struct chown_req {
        int fs_e;
        ino_t inode_nr;
        uid_t uid;
        gid_t gid;
        uid_t newuid;
        gid_t newgid;
} chown_req_t;


/* Structure for REQ_CHMOD request */
typedef struct chmod_req {
        int fs_e;
        ino_t inode_nr;
        uid_t uid;
        gid_t gid;
        mode_t rmode;
} chmod_req_t;


/* Structure for REQ_ACCESS request */
typedef struct access_req {
        int fs_e;
        ino_t inode_nr;
        uid_t uid;
        gid_t gid;
        mode_t amode;
} access_req_t;


/* Structure for REQ_MKNOD request */
typedef struct mknod_req {
        int fs_e;
        ino_t inode_nr;
        uid_t uid;
        gid_t gid;
        mode_t rmode;
        dev_t dev;
        char *lastc;
} mknod_req_t;


/* Structure for REQ_MKDIR request */
typedef struct mkdir_req {
        int fs_e;
        ino_t d_inode_nr;
        uid_t uid;
        gid_t gid;
        mode_t rmode;
        char *lastc;
} mkdir_req_t;


/* Structure for REQ_UNLINK request */
typedef struct unlink_req {
        int fs_e;
        ino_t d_inode_nr;
        uid_t uid;
        gid_t gid;
        char *lastc;
} unlink_req_t;


/* Structure for REQ_UTIME request */
typedef struct utime_req {
        int fs_e;
        ino_t inode_nr;
        uid_t uid;
        gid_t gid;
        time_t actime;
        time_t modtime;
} utime_req_t;


/* Structure for REQ_LINK request */
typedef struct link_req {
	endpoint_t fs_e;
        ino_t linked_file;
        ino_t link_parent;
        uid_t uid;
        gid_t gid;
        char *lastc;
} link_req_t;


/* Structure for REQ_SLINK request */
typedef struct slink_req {
	endpoint_t fs_e;
        ino_t parent_dir;
        uid_t uid;
        gid_t gid;
        char *lastc;
        endpoint_t who_e;
        char *path_addr;
        unsigned short path_length;
} slink_req_t;


/* Structure for REQ_RDLINK request */
typedef struct rdlink_req {
	endpoint_t fs_e;
        ino_t inode_nr;
        uid_t uid;
        gid_t gid;
        endpoint_t who_e;
        char *path_buffer;
        unsigned short max_length;
} rdlink_req_t;


/* Structure for REQ_RENAME request */
typedef struct rename_req {
	endpoint_t fs_e;
        ino_t old_dir;
        ino_t new_dir;
        uid_t uid;
        gid_t gid;
        char *old_name;
        char *new_name;
} rename_req_t;


/* Structure for REQ_MOUNTPOINT request */
typedef struct mountpoint_req {
	endpoint_t fs_e;
        ino_t inode_nr;
        uid_t uid;
        gid_t gid;
} mountpoint_req_t;


/* Structure for REQ_READSUPER request */
typedef struct readsuper_req {
	endpoint_t fs_e;
        time_t boottime;
        endpoint_t driver_e;
        dev_t dev;
        char *slink_storage;
        char isroot;
        char readonly;
} readsuper_req_t;

/* Structure for response of READSUPER request */
typedef struct readsuper_res {
	endpoint_t fs_e;
	ino_t inode_nr;
	mode_t fmode;
	off_t fsize;
        int blocksize;
        off_t maxsize;
} readsuper_res_t;

        
/* Structure for REQ_TRUNC request */
typedef struct trunc_req {
	endpoint_t fs_e;
	ino_t inode_nr;
        uid_t uid;
        gid_t gid;
        off_t length;
} trunc_req_t;


/* Structure for REQ_LOOKUP request */
typedef struct lookup_req {
        /* Fields filled in by the caller */
        char *path;
        char *lastc;
        int flags;
	/* Fields filled in by the path name traversal method */
        endpoint_t fs_e;
        ino_t start_dir;
        ino_t root_dir;     /* process' root directory */
        uid_t uid;
        gid_t gid;
        unsigned char symloop;
} lookup_req_t;


/* Structure for a lookup response */
typedef struct lookup_res {
	endpoint_t fs_e;
	ino_t inode_nr;
	mode_t fmode;
	off_t fsize;
	uid_t uid;
	gid_t gid;
	/* For char/block special files */
	dev_t dev;
	
	/* Fields used for handling mount point and symbolic links */
	int char_processed;
	unsigned char symloop;
} lookup_res_t;


/* Structure for REQ_BREAD and REQ_BWRITE request (block spec files) */
typedef struct breadwrite_req {
	int rw_flag;
        short blocksize;
	endpoint_t fs_e;
        endpoint_t user_e;
        endpoint_t driver_e;
        dev_t dev;
	u64_t pos;
	unsigned int num_of_bytes;
	char *user_addr;
} breadwrite_req_t;



/* Structure for REQ_ request */
