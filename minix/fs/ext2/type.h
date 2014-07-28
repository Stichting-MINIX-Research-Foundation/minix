#ifndef EXT2_TYPE_H
#define EXT2_TYPE_H

#include <minix/libminixfs.h>

/* On the disk all attributes are stored in little endian format.
 * Inode structure was taken from linux/include/linux/ext2_fs.h.
 */
typedef struct {
    u16_t  i_mode;         /* File mode */
    u16_t  i_uid;          /* Low 16 bits of Owner Uid */
    u32_t  i_size;         /* Size in bytes */
    u32_t  i_atime;        /* Access time */
    u32_t  i_ctime;        /* Creation time */
    u32_t  i_mtime;        /* Modification time */
    u32_t  i_dtime;        /* Deletion Time */
    u16_t  i_gid;          /* Low 16 bits of Group Id */
    u16_t  i_links_count;  /* Links count */
    u32_t  i_blocks;       /* Blocks count */
    u32_t  i_flags;        /* File flags */
    union {
        struct {
                u32_t  l_i_reserved1;
        } linux1;
        struct {
                u32_t  h_i_translator;
        } hurd1;
        struct {
                u32_t  m_i_reserved1;
        } masix1;
    } osd1;                         /* OS dependent 1 */
    u32_t  i_block[EXT2_N_BLOCKS];/* Pointers to blocks */
    u32_t  i_generation;   /* File version (for NFS) */
    u32_t  i_file_acl;     /* File ACL */
    u32_t  i_dir_acl;      /* Directory ACL */
    u32_t  i_faddr;        /* Fragment address */
    union {
        struct {
            u8_t    l_i_frag;       /* Fragment number */
            u8_t    l_i_fsize;      /* Fragment size */
            u16_t   i_pad1;
            u16_t  l_i_uid_high;   /* these 2 fields    */
            u16_t  l_i_gid_high;   /* were reserved2[0] */
            u32_t   l_i_reserved2;
        } linux2;
        struct {
            u8_t    h_i_frag;       /* Fragment number */
            u8_t    h_i_fsize;      /* Fragment size */
            u16_t  h_i_mode_high;
            u16_t  h_i_uid_high;
            u16_t  h_i_gid_high;
            u32_t  h_i_author;
        } hurd2;
        struct {
            u8_t    m_i_frag;       /* Fragment number */
            u8_t    m_i_fsize;      /* Fragment size */
            u16_t   m_pad1;
            u32_t   m_i_reserved2[2];
        } masix2;
    } osd2;                         /* OS dependent 2 */
} d_inode;


/* Part of on disk directory (entry description).
 * It includes all fields except name (since size is unknown.
 * In revision 0 name_len is u16_t (here is structure of rev >= 0.5,
 * where name_len was truncated with the upper 8 bit to add file_type).
 * MIN_DIR_ENTRY_SIZE depends on this structure.
 */
struct ext2_disk_dir_desc {
  u32_t     d_ino;
  u16_t     d_rec_len;
  u8_t      d_name_len;
  u8_t      d_file_type;
  char      d_name[1];
};

/* Current position in block */
#define CUR_DISC_DIR_POS(cur_desc, base)  ((char*)cur_desc - (char*)base)
/* Return pointer to the next dentry */
#define NEXT_DISC_DIR_DESC(cur_desc)	((struct ext2_disk_dir_desc*)\
					((char*)cur_desc + cur_desc->d_rec_len))
/* Return next dentry's position in block */
#define NEXT_DISC_DIR_POS(cur_desc, base) (cur_desc->d_rec_len +\
					   CUR_DISC_DIR_POS(cur_desc, base))
/* Structure with options affecting global behavior. */
struct opt {
  int use_orlov;		/* Bool: Use Orlov allocator */
  /* In ext2 there are reserved blocks, which can be used by super user only or
   * user specified by resuid/resgid. Right now we can't check what user
   * requested operation (VFS limitation), so it's a small warkaround.
   */
  int mfsalloc;			/* Bool: use mfslike allocator */
  int use_reserved_blocks;	/* Bool: small workaround */
  unsigned int block_with_super;/* Int: where to read super block,
                                 * uses 1k units. */
  int use_prealloc;		/* Bool: use preallocation */
};


#endif /* EXT2_TYPE_H */
