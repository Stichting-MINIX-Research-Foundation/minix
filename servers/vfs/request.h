
/* Low level request messages are built and sent by wrapper functions.
 * This file contains the request and response structures for accessing
 * those wrappers functions.
 */

#include <sys/types.h>


/* Structure for response that contains inode details */
typedef struct node_details {
	endpoint_t fs_e;
	ino_t inode_nr;
	mode_t fmode;
	off_t fsize;
	uid_t uid;
	gid_t gid;

	/* For faster access */
	unsigned short inode_index;

	/* For char/block special files */
	dev_t dev;
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


/* Structure for REQ_CLONE_OPCL request */
typedef struct clone_opcl_req {
        int fs_e;
        dev_t dev;
} clone_opcl_req_t;


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


/* Structure for REQ_ request */
