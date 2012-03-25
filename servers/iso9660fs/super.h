/* This file contains the definitions of a ISO9660 structures */
#include "inode.h"

#define VD_BOOT_RECORD 0
#define VD_PRIMARY 1
#define VD_SUPPL 2
#define VD_PART 3
#define VD_SET_TERM 255

#define MAX_ATTEMPTS 20 	/* # attempts to read the volume descriptors.
				 * After it gives up */
#define ROOT_INO_NR 1

/* Structure for the primary volume descriptor */
struct iso9660_vd_pri {
  u8_t vd_type;
  char standard_id[ISO9660_SIZE_STANDARD_ID];
  u8_t vd_version;
  char system_id[ISO9660_SIZE_SYS_ID];
  char volume_id[ISO9660_SIZE_VOLUME_ID];
  u32_t volume_space_size_l;
  u32_t volume_space_size_m;
  u32_t volume_set_size;
  u32_t volume_sequence_number;
  u16_t logical_block_size_l;
  u16_t logical_block_size_m;
  u32_t path_table_size_l;
  u32_t path_table_size_m;
  u32_t loc_l_occ_path_table;
  u32_t loc_opt_l_occ_path_table;
  u32_t loc_m_occ_path_table;
  u32_t loc_opt_m_occ_path_table;
  struct dir_record *dir_rec_root;
  char volume_set_id[ISO9660_SIZE_VOLUME_SET_ID];
  char publisher_id[ISO9660_SIZE_PUBLISHER_ID];
  char data_preparer_id[ISO9660_SIZE_DATA_PREP_ID];
  char application_id[ISO9660_SIZE_APPL_ID];
  char copyright_file_id[ISO9660_SIZE_COPYRIGHT_FILE_ID];
  char abstract_file_id[ISO9660_SIZE_ABSTRACT_FILE_ID];
  char bibl_file_id[ISO9660_SIZE_BIBL_FILE_ID];
  char volume_cre_date[ISO9660_SIZE_VOL_CRE_DATE];
  char volume_mod_date[ISO9660_SIZE_VOL_MOD_DATE];
  char volume_exp_date[ISO9660_SIZE_VOL_EXP_DATE];
  char volume_eff_date[ISO9660_SIZE_VOL_EFF_DATE];
  u8_t file_struct_ver;
  /* The rest is either not specified or reserved */
  u8_t count;
} v_pri;
