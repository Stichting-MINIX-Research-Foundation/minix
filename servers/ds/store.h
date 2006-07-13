/* Type definitions for the Data Store Server. */

#include <sys/types.h>
#include <minix/sys_config.h>
#include <minix/ds.h>
#include <minix/bitmap.h>
#include <regex.h>

/* Constants for the Data Store Server. */
#define NR_DS_KEYS               64	/* reserve space for so many items */
#define NR_DS_SUBS   (4*_NR_SYS_PROCS)	/* .. and so many subscriptions */

/* Types. */

struct data_store {
  int ds_flags;			/* flags for this store, includes type info */
  char ds_key[DS_MAX_KEYLEN];	/* key to lookup information */
  union {
    u32_t ds_val_u32;			/* u32 data (DS_TYPE_U32) */
    char  ds_val_str[DS_MAX_VALLEN];	/* string data (DS_TYPE_STR) */
  } ds_val;

  /* out of date subscribers. */
  bitchunk_t ds_old_subs[BITMAP_CHUNKS(NR_DS_SUBS)];	
};

struct subscription {
  int sub_flags;		/* flags for this subscription */
  regex_t sub_regex;		/* regular expression agains keys */
  endpoint_t sub_owner;		/* who is subscribed */
};


