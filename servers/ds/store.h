/* Type definitions for the Data Store Server. */
struct data_store {
  int ds_flags;			/* flags for this store */
  int ds_key;			/* key to lookup information */
  long ds_val_l1;		/* data associated with key */
  long ds_val_l2;		/* data associated with key */
  long ds_auth;			/* secret given by owner of data */
  int ds_nr_subs;		/* number of subscribers for key */ 
};

/* Flag values. */
#define DS_IN_USE	0x01
#define DS_PUBLIC	0x02

/* Constants for the Data Store Server. */
#define NR_DS_KEYS	  64	/* reserve space for so many items */



