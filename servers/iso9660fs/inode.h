#include "const.h"

struct dir_record {
  u8_t length;			/* The length of the record */
  u8_t ext_attr_rec_length;
  u32_t loc_extent_l;		/* The same data (in this case loc_extent)is */
  u32_t loc_extent_m;		/* saved in two ways. The first puts the le- */
  u32_t data_length_l;		/* ast significant byte first, the second */
  u32_t data_length_m;		/* does the opposite */
  u8_t rec_date[7];		/* => recording date */
  u8_t file_flags;		/* => flags of the file */
  u8_t file_unit_size;		/* set of blocks in interleave mode */
  u8_t inter_gap_size;		/* gap between file units in interleave mode */
  u32_t vol_seq_number;		/* volume sequence number: not used */
  u8_t length_file_id;		/* Length name file */
  char file_id[ISO9660_MAX_FILE_ID_LEN]; /* file name */
  struct ext_attr_rec *ext_attr;

  /* Memory attrs */
  u8_t d_count;			/* Count if the dir_record is in use or not */
  mode_t d_mode;		/* file type, protection, etc. */
/*   struct hash_idi_entry *id; */	/* id associated */
  u32_t d_phy_addr;		/* physical address of this dir record */
  ino_t d_ino_nr;		/* inode number (identical to the address) */
  char d_mountpoint;		/* true if mounted on */
  struct dir_record *d_next;	/* In case the file consists in more file sections
				   this points to the next one */
  struct dir_record *d_prior;	/* The same as before, this points to the dir parent */
  u32_t d_file_size;		/* Total size of the file */

} dir_records[NR_DIR_RECORDS];

struct ext_attr_rec {
  u32_t own_id;
  u32_t group_id;
  u16_t permissions;
  char file_cre_date[ISO9660_SIZE_VOL_CRE_DATE];
  char file_mod_date[ISO9660_SIZE_VOL_MOD_DATE];
  char file_exp_date[ISO9660_SIZE_VOL_EXP_DATE];
  char file_eff_date[ISO9660_SIZE_VOL_EFF_DATE];
  u8_t rec_format;
  u8_t rec_attrs;
  u32_t rec_length;
  char system_id[ISO9660_SIZE_SYS_ID];
  char system_use[ISO9660_SIZE_SYSTEM_USE];
  u8_t ext_attr_rec_ver;
  u8_t len_esc_seq;

  int count;
} ext_attr_recs[NR_ATTR_RECS];

#define D_DIRECTORY 0x2
#define D_TYPE 0x8E

/* Vector with all the ids of the dir records */
/* PUBLIC struct hash_idi_entry { */
/*   u32_t h_phy_addr; */
/*   struct dir_record *h_dir_record; */
/* } hash_idi[NR_ID_INODES]; */

/* PUBLIC int size_hash_idi; */

/* #define ID_DIR_RECORD(id) id - hash_idi + 1 */
#define ID_DIR_RECORD(dir) dir->d_ino_nr

/* #define ASSIGN_ID 1 */
/* #define NOT_ASSIGN_ID 0 */
