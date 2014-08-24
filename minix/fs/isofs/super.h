/* This file contains the definitions of ISO9660 volume descriptors. */
#include "inode.h"

#define VD_BOOT_RECORD 0
#define VD_PRIMARY 1
#define VD_SUPPL 2
#define VD_PART 3
#define VD_SET_TERM 255

#define MAX_ATTEMPTS 20         /* # attempts to read the volume descriptors.
                                 * After it gives up */

/* Structure for the primary volume descriptor. */
struct iso9660_vol_pri_desc {
	/*
	 * On-disk structure format of the primary volume descriptor,
	 * 2048 bytes long. See ISO specs for details.
	 */
	u8_t vd_type;
	char standard_id[ISO9660_SIZE_STANDARD_ID];
	u8_t vd_version;
	u8_t pad1;
	char system_id[ISO9660_SIZE_SYS_ID];
	char volume_id[ISO9660_SIZE_VOLUME_ID];
	u8_t pad2[8];
	u32_t volume_space_size_l;
	u32_t volume_space_size_m;
	u8_t pad3[32];
	u16_t volume_set_size_l;
	u16_t volume_set_size_m;
	u16_t volume_sequence_number_l;
	u16_t volume_sequence_number_m;
	u16_t logical_block_size_l;
	u16_t logical_block_size_m;
	u32_t path_table_size_l;
	u32_t path_table_size_m;
	u32_t loc_l_occ_path_table;
	u32_t loc_opt_l_occ_path_table;
	u32_t loc_m_occ_path_table;
	u32_t loc_opt_m_occ_path_table;
	u8_t root_directory[34];
	char volume_set_id[ISO9660_SIZE_VOLUME_SET_ID];
	char publisher_id[ISO9660_SIZE_PUBLISHER_ID];
	char data_preparer_id[ISO9660_SIZE_DATA_PREP_ID];
	char application_id[ISO9660_SIZE_APPL_ID];
	char copyright_file_id[ISO9660_SIZE_COPYRIGHT_FILE_ID];
	char abstract_file_id[ISO9660_SIZE_ABSTRACT_FILE_ID];
	char bibl_file_id[ISO9660_SIZE_BIBL_FILE_ID];
	char volume_cre_date[ISO9660_SIZE_DATE17];
	char volume_mod_date[ISO9660_SIZE_DATE17];
	char volume_exp_date[ISO9660_SIZE_DATE17];
	char volume_eff_date[ISO9660_SIZE_DATE17];
	u8_t file_struct_ver;
	u8_t reserved1;
	u8_t application_use[512];
	u8_t reserved2[652];

	/* End of the on-disk structure format. */

	struct inode *inode_root;
	int i_count;
} __attribute__((packed)) v_pri;

