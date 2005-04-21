#ifndef MTOOLS_NAMECLASH_H
#define MTOOLS_NAMECLASH_H

#include "stream.h"

typedef enum clash_action {
	NAMEMATCH_NONE,
	NAMEMATCH_AUTORENAME,
	NAMEMATCH_QUIT,
	NAMEMATCH_SKIP,
	NAMEMATCH_RENAME,
	NAMEMATCH_PRENAME, /* renaming of primary name */
	NAMEMATCH_OVERWRITE,
	NAMEMATCH_ERROR,
	NAMEMATCH_SUCCESS,
	NAMEMATCH_GREW
} clash_action;

/* clash handling structure */
typedef struct ClashHandling_t {
	clash_action action[2];
	clash_action namematch_default[2];
		
	int nowarn;	/* Don't ask, just do default action if name collision*/
	int got_slots;
	int mod_time;
	/* unsigned int dot; */
	char *myname;
	unsigned char *dosname;
	int single;

	int use_longname;
	int ignore_entry;
	int source; /* to prevent the source from overwriting itself */
	int source_entry; /* to account for the space freed up by the original 
					   * name */
	char * (*name_converter)(char *filename, int verbose, 
				 int *mangled, char *ans);
} ClashHandling_t;

/* write callback */
typedef int (write_data_callback)(char *,char *, void *, struct direntry_t *);

int mwrite_one(Stream_t *Dir,
	       const char *argname,
	       const char *shortname,
	       write_data_callback *cb,
	       void *arg,
	       ClashHandling_t *ch);

int handle_clash_options(ClashHandling_t *ch, char c);
void init_clash_handling(ClashHandling_t *ch);
Stream_t *createDir(Stream_t *Dir, const char *filename, ClashHandling_t *ch,
		    unsigned char attr, time_t mtime);


#endif
