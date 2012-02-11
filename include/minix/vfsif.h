#ifndef __MINIX_VFSIF_H
#define __MINIX_VFSIF_H

#include <sys/types.h>
#include <limits.h>

/* VFS/FS request fields */
#define REQ_ACTIME		m9_l2
#define REQ_COUNT		m9_l2
#define REQ_DEV			m9_l5
#define REQ_DEV2		m9_l1
#define REQ_DIR_INO   		m9_l3
#define REQ_FLAGS		m9_s3
#define REQ_GID			m9_s1
#define REQ_GRANT		m9_l2
#define REQ_GRANT2		m9_l1 
#define REQ_GRANT3		m9_l3
#define REQ_INODE_NR		m9_l1
#define REQ_MEM_SIZE		m9_l5
#define REQ_MODE		m9_s3
#define REQ_MODTIME		m9_l3
#define REQ_NBYTES		m9_l5
#define REQ_PATH_LEN		m9_s2
#define REQ_PATH_SIZE		m9_l5
#define REQ_REN_GRANT_NEW	m9_l1
#define REQ_REN_GRANT_OLD	m9_l2
#define REQ_REN_LEN_NEW		m9_s2
#define REQ_REN_LEN_OLD		m9_s1
#define REQ_REN_NEW_DIR		m9_l4
#define REQ_REN_OLD_DIR		m9_l3
#define REQ_ROOT_INO		m9_l4
#define REQ_SEEK_POS_HI		m9_l3
#define REQ_SEEK_POS_LO		m9_l4
#define REQ_TRC_END_HI		m9_l4
#define REQ_TRC_END_LO		m9_l5
#define REQ_TRC_START_HI	m9_l2
#define REQ_TRC_START_LO	m9_l3
#define REQ_UCRED_SIZE		m9_s4 
#define REQ_UID			m9_s4


/* VFS/FS reply fields */
#define RES_DEV			m9_l4
#define RES_GID			m9_s1
#define RES_INODE_NR		m9_l1
#define RES_FILE_SIZE_HI	m9_l2
#define RES_FILE_SIZE_LO	m9_l3
#define RES_MODE		m9_s2
#define RES_NBYTES		m9_l5
#define RES_OFFSET		m9_s2
#define RES_SEEK_POS_HI		m9_l3
#define RES_SEEK_POS_LO		m9_l4
#define RES_SYMLOOP		m9_s3
#define RES_UID			m9_s4
#define RES_CONREQS		m9_s3

/* VFS/FS flags */
#define REQ_RDONLY		001
#define REQ_ISROOT		002
#define PATH_NOFLAGS		000
#define PATH_RET_SYMLINK	010	/* Return a symlink object (i.e.
					 * do not continue with the contents
					 * of the symlink if it is the last
					 * component in a path). */
#define PATH_GET_UCRED		020	/* Request provides a grant ID in m9_l1
					 * and struct ucred size in m9_s4 (as
					 * opposed to a REQ_UID). */

/* VFS/FS error messages */
#define EENTERMOUNT              (-301)
#define ELEAVEMOUNT              (-302)
#define ESYMLINK                 (-303)

/* VFS/FS types */

/* User credential structure */
typedef struct {
	uid_t vu_uid;
	gid_t vu_gid;
	int vu_ngroups;
	gid_t vu_sgroups[NGROUPS_MAX];
} vfs_ucred_t;

#define NGROUPS_MAX_OLD 	8
/* User credential structure before increasing
 * uid_t and gid_t u8_t */
typedef struct {
	short vu_uid;
	char vu_gid;
	int vu_ngroups;
	char vu_sgroups[NGROUPS_MAX_OLD];
} vfs_ucred_old_t;

/* Request numbers */
#define REQ_GETNODE	(VFS_BASE + 1)	/* Should be removed */
#define REQ_PUTNODE	(VFS_BASE + 2)
#define REQ_SLINK	(VFS_BASE + 3)
#define REQ_FTRUNC	(VFS_BASE + 4)
#define REQ_CHOWN	(VFS_BASE + 5)
#define REQ_CHMOD	(VFS_BASE + 6)
#define REQ_INHIBREAD	(VFS_BASE + 7)
#define REQ_STAT	(VFS_BASE + 8)
#define REQ_UTIME	(VFS_BASE + 9)
#define REQ_FSTATFS	(VFS_BASE + 10)
#define REQ_BREAD	(VFS_BASE + 11)
#define REQ_BWRITE	(VFS_BASE + 12)
#define REQ_UNLINK	(VFS_BASE + 13)
#define REQ_RMDIR	(VFS_BASE + 14)
#define REQ_UNMOUNT	(VFS_BASE + 15)
#define REQ_SYNC	(VFS_BASE + 16)
#define REQ_NEW_DRIVER	(VFS_BASE + 17)
#define REQ_FLUSH	(VFS_BASE + 18)
#define REQ_READ	(VFS_BASE + 19)
#define REQ_WRITE	(VFS_BASE + 20)
#define REQ_MKNOD	(VFS_BASE + 21)
#define REQ_MKDIR	(VFS_BASE + 22)
#define REQ_CREATE	(VFS_BASE + 23)
#define REQ_LINK	(VFS_BASE + 24)
#define REQ_RENAME	(VFS_BASE + 25)
#define REQ_LOOKUP	(VFS_BASE + 26)
#define REQ_MOUNTPOINT  (VFS_BASE + 27)
#define REQ_READSUPER	(VFS_BASE + 28) 
#define REQ_NEWNODE	(VFS_BASE + 29)
#define REQ_RDLINK	(VFS_BASE + 30)
#define REQ_GETDENTS	(VFS_BASE + 31)
#define REQ_STATVFS	(VFS_BASE + 32)

#define NREQS			    33

#define IS_VFS_RQ(type) (((type) & ~0xff) == VFS_BASE)

#define TRNS_GET_ID(t)		((t) & 0xFFFF)
#define TRNS_ADD_ID(t,id)	(((t) << 16) | ((id) & 0xFFFF))
#define TRNS_DEL_ID(t)		((short)((t) >> 16))

#define PFS_BASE		(VFS_BASE + 100)

#define PFS_REQ_CHECK_PERMS	(PFS_BASE + 1)
#define PFS_REQ_VERIFY_FD	(PFS_BASE + 2)
#define PFS_REQ_SET_FILP	(PFS_BASE + 3)
#define PFS_REQ_COPY_FILP	(PFS_BASE + 4)
#define PFS_REQ_PUT_FILP	(PFS_BASE + 5)
#define PFS_REQ_CANCEL_FD	(PFS_BASE + 6)

#define PFS_NREQS		7

#define IS_PFS_VFS_RQ(type)	(type >= PFS_BASE && \
					type < (PFS_BASE + PFS_NREQS))

#endif

