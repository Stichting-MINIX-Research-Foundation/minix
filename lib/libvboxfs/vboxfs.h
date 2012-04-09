/* Part of libvboxfs - (c) 2012, D.C. van Moolenbroek */

#ifndef _VBOXFS_VBOXFS_H
#define _VBOXFS_VBOXFS_H

#define VBOXFS_CALL_CREATE		 3	/* create, open, lookup */
#define VBOXFS_CALL_CLOSE		 4	/* close handle */
#define VBOXFS_CALL_READ		 5	/* read from file */
#define VBOXFS_CALL_WRITE		 6	/* write to file */
#define VBOXFS_CALL_LIST		 8	/* list directory contents */
#define VBOXFS_CALL_INFO		 9	/* get/set file information */
#define VBOXFS_CALL_REMOVE		11	/* remove file or directory */
#define VBOXFS_CALL_UNMAP_FOLDER	13	/* unmap folder */
#define VBOXFS_CALL_RENAME		14	/* rename file or directory */
#define VBOXFS_CALL_SET_UTF8		16	/* switch to UTF8 */
#define VBOXFS_CALL_MAP_FOLDER		17	/* map folder */

#define VBOXFS_INVALID_HANDLE	((vboxfs_handle_t) ~0LL)

typedef u32_t vboxfs_root_t;
typedef u64_t vboxfs_handle_t;

typedef struct {
	u16_t size;
	u16_t len;
	char data[PATH_MAX];
} vboxfs_path_t;

#define VBOXFS_NO_RESULT		0
#define VBOXFS_PATH_NOT_FOUND		1
#define VBOXFS_FILE_NOT_FOUND		2
#define VBOXFS_FILE_EXISTS		3
#define VBOXFS_FILE_CREATED		4
#define VBOXFS_FILE_REPLACED		5

#define VBOXFS_OBJATTR_ADD_NONE		1	/* no other attributes */
#define VBOXFS_OBJATTR_ADD_UNIX		2	/* POSIX attributes */
#define VBOXFS_OBJATTR_ADD_EATTR	3	/* extended attributes */

typedef struct {
	u32_t mode;
	u32_t add;
	union {
		struct {
			u32_t uid;
			u32_t gid;
			u32_t nlinks;
			u32_t dev;
			u64_t inode;
			u32_t flags;
			u32_t gen;
			u32_t rdev;
		};
		struct {
			u64_t easize;
		};
	};
} vboxfs_objattr_t;

/* Thankfully, MINIX uses the universal UNIX mode values. */
#define VBOXFS_GET_MODE(mode)		((mode) & 0xffff)
#define VBOXFS_SET_MODE(type, perm)	((type) | ((perm) & ALLPERMS))

typedef struct {
	u64_t size;
	u64_t disksize;
	u64_t atime;
	u64_t mtime;
	u64_t ctime;
	u64_t crtime;
	vboxfs_objattr_t attr;
} vboxfs_objinfo_t;

#define VBOXFS_CRFLAG_LOOKUP		0x00000001
#define VBOXFS_CRFLAG_DIRECTORY		0x00000004
#define VBOXFS_CRFLAG_OPEN_IF_EXISTS	0x00000000
#define VBOXFS_CRFLAG_FAIL_IF_EXISTS	0x00000010
#define VBOXFS_CRFLAG_REPLACE_IF_EXISTS	0x00000020
#define VBOXFS_CRFLAG_TRUNC_IF_EXISTS	0x00000030
#define VBOXFS_CRFLAG_CREATE_IF_NEW	0x00000000
#define VBOXFS_CRFLAG_FAIL_IF_NEW	0x00000100
#define VBOXFS_CRFLAG_READ		0x00001000
#define VBOXFS_CRFLAG_WRITE		0x00002000
#define VBOXFS_CRFLAG_APPEND		0x00004000
#define VBOXFS_CRFLAG_READ_ATTR		0x00010000
#define VBOXFS_CRFLAG_WRITE_ATTR	0x00020000

typedef struct {
	vboxfs_handle_t handle;
	u32_t result;
	u32_t flags;
	vboxfs_objinfo_t info;
} vboxfs_crinfo_t;

typedef struct {
	vboxfs_objinfo_t info;
	u16_t shortlen;
	u16_t shortname[14];
	vboxfs_path_t name;	/* WARNING: name data size is dynamic! */
} vboxfs_dirinfo_t;

#define VBOXFS_INFO_GET		0x00	/* get file information */
#define VBOXFS_INFO_SET		0x01	/* set file information */

#define VBOXFS_INFO_SIZE	0x04	/* get/set file size */
#define VBOXFS_INFO_FILE	0x08	/* get/set file attributes */
#define VBOXFS_INFO_VOLUME	0x10	/* get volume information */

#define VBOXFS_REMOVE_FILE	0x01	/* remove file */
#define VBOXFS_REMOVE_DIR	0x02	/* remove directory */
#define VBOXFS_REMOVE_SYMLINK	0x04	/* remove symbolic link */

#define VBOXFS_RENAME_FILE	0x01	/* rename file */
#define VBOXFS_RENAME_DIR	0x02	/* rename directory */
#define VBOXFS_RENAME_REPLACE	0x04	/* replace target if it exists */

typedef struct {
	u32_t namemax;
	u8_t remote;
	u8_t casesens;
	u8_t readonly;
	u8_t unicode;
	u8_t fscomp;
	u8_t filecomp;
	u16_t reserved;
} vboxfs_fsprops_t;

typedef struct {
	u64_t total;
	u64_t free;
	u32_t blocksize;
	u32_t sectorsize;
	u32_t serial;
	vboxfs_fsprops_t props;
} vboxfs_volinfo_t;

#endif /* !_VBOXFS_VBOXFS_H */
