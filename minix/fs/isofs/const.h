
#define GETDENTS_BUFSIZ         1024

#define ISO9660_STANDARD_ID     "CD001" /* Standard code for ISO9660 FS */

/* Filesystem options support */
#define ISO9660_OPTION_ROCKRIDGE
/* TODO: Make MODE3 working. */
/*#define ISO9660_OPTION_MODE3*/

/* Below there are constant of the ISO9660 fs */
#define ISO9660_SUPER_BLOCK_POSITION    32768
#define ISO9660_MIN_BLOCK_SIZE          2048

/* SIZES FIELDS ISO9660 STRUCTURES */
#define ISO9660_SIZE_STANDARD_ID        5
#define ISO9660_SIZE_BOOT_SYS_ID        32
#define ISO9660_SIZE_BOOT_ID            32

#define ISO9660_SIZE_SYS_ID             32
#define ISO9660_SIZE_VOLUME_ID          32
#define ISO9660_SIZE_VOLUME_SET_ID      128
#define ISO9660_SIZE_PUBLISHER_ID       128
#define ISO9660_SIZE_DATA_PREP_ID       128
#define ISO9660_SIZE_APPL_ID            128
#define ISO9660_SIZE_COPYRIGHT_FILE_ID  37
#define ISO9660_SIZE_ABSTRACT_FILE_ID   37
#define ISO9660_SIZE_BIBL_FILE_ID       37

#define ISO9660_SIZE_DATE17             17
#define ISO9660_SIZE_DATE7             7

#define ISO9660_SIZE_ESCAPE_SQC         32
#define ISO9660_SIZE_PART_ID            32

#define ISO9660_SIZE_SYSTEM_USE         64

/* maximum size of length of name file used in dir records */
#define ISO9660_MAX_FILE_ID_LEN         32
#define ISO9660_RRIP_MAX_FILE_ID_LEN    256

/* Miscellaneous constants */
#define SYS_UID  ((uid_t) 0)            /* uid_t for processes PM and INIT */
#define SYS_GID  ((gid_t) 0)            /* gid_t for processes PM and INIT */
