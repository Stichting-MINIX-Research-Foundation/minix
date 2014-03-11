#ifndef _DS_STORE_H_
#define _DS_STORE_H_

/* Type definitions for the Data Store Server. */
#include <sys/types.h>
#include <minix/config.h>
#include <minix/ds.h>
#include <minix/bitmap.h>
#include <minix/param.h>
#include <regex.h>

#define NR_DS_KEYS	(2*NR_SYS_PROCS)	/* number of entries */
#define NR_DS_SUBS	(4*NR_SYS_PROCS)	/* number of subscriptions */

/* Base 'class' for the following 3 structs. */
struct data_store {
	int	flags;
	char	key[DS_MAX_KEYLEN];	/* key to lookup information */
	char	owner[DS_MAX_KEYLEN];

	union dsi_u {
		unsigned u32;
		struct dsi_mem {
			void *data;
			size_t length;
			size_t reallen;
		} mem;
	} u;
};

struct subscription {
	int		flags;
	char		owner[DS_MAX_KEYLEN];
	regex_t		regex;
	bitchunk_t	old_subs[BITMAP_CHUNKS(NR_DS_KEYS)];	
};

#endif /* _DS_STORE_H_ */
