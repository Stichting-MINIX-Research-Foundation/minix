#include "const.h"
#include <sys/stat.h>

struct iso9660_dir_record {
	/*
	 * ISO standard directory record.
	 */
	u8_t length;                    /* The length of the record */
	u8_t ext_attr_rec_length;
	u32_t loc_extent_l;             /* The same data (in this case loc_extent)is */
	u32_t loc_extent_m;             /* saved in two ways. The first puts the le- */
	u32_t data_length_l;            /* ast significant byte first, the second */
	u32_t data_length_m;            /* does the opposite */
	u8_t rec_date[7];               /* => recording date */
	u8_t file_flags;                /* => flags of the file */
	u8_t file_unit_size;            /* set of blocks in interleave mode */
	u8_t inter_gap_size;            /* gap between file units in interleave mode */
	u32_t vol_seq_number;           /* volume sequence number: not used */
	u8_t length_file_id;            /* Length name file */
	char file_id[ISO9660_MAX_FILE_ID_LEN]; /* file name */
} __attribute__((packed));

struct rrii_dir_record {
	/*
	 * Rock Ridge directory record extensions.
	 */
	u8_t mtime[7];          /* stat.st_mtime */
	u8_t atime[7];          /* stat.st_atime */
	u8_t ctime[7];          /* stat.st_ctime */
	u8_t birthtime[7];      /* stat.st_birthtime */

	mode_t d_mode;          /* file mode */
	uid_t uid;              /* user ID of the file's owner */
	gid_t gid;              /* group ID of the file's group */
	dev_t rdev;             /* device major/minor */

	char file_id_rrip[ISO9660_RRIP_MAX_FILE_ID_LEN];        /* file name */
	char slink_rrip[ISO9660_RRIP_MAX_FILE_ID_LEN];          /* symbolic link */

	struct inode *reparented_inode;
} ;

struct dir_extent {
	/*
	 * Extent (contiguous array of logical sectors).
	 */
	u32_t location;
	u32_t length;
	struct dir_extent *next;
} ;

struct inode_dir_entry {
	struct inode *i_node;
	char *name;                     /* Pointer to real name */
	char i_name[ISO9660_MAX_FILE_ID_LEN+1]; /* ISO 9660 name */
	char *r_name;                   /* Rock Ridge name */
} ;

struct inode {
	int i_count;                    /* usage counter of this inode */
	int i_refcount;                 /* reference counter of this inode */
	int i_mountpoint;               /* flag for inode being used as a mount point */
	struct stat i_stat;             /* inode properties */
	struct dir_extent extent;      /* first extent of file */
	struct inode_dir_entry *dir_contents;	/* contents of directory */
	size_t dir_size;                /* number of inodes in this directory */
	char *s_name;                   /* Rock Ridge symbolic link */
	int skip;                       /* skip inode because of reparenting */
} ;

struct opt {
	/*
	 * Global mount options.
	 */
	int norock;                     /* Bool: dont use Rock Ridge */
} ;

#define D_DIRECTORY 0x2
#define D_NOT_LAST_EXTENT 0x80
#define D_TYPE 0x8E

