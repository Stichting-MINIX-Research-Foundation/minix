#ifndef MTOOLS_MAINLOOP_H
#define MTOOLS_MAINLOOP_H

#ifndef OS_Minix
#include <sys/param.h>
#endif
#include "vfat.h"
#include "mtoolsDirent.h"

typedef struct MainParam_t {
	/* stuff needing to be initialised by the caller */
	int (*loop)(Stream_t *Dir, struct MainParam_t *mp, 
		    const char *filename);
	int (*dirCallback)(direntry_t *, struct MainParam_t *);
	int (*callback)(direntry_t *, struct MainParam_t *);
	int (*unixcallback)(struct MainParam_t *mp);

	void *arg; /* command-specific parameters 
		    * to be passed to callback */

       	int openflags; /* flags used to open disk */
	int lookupflags; /* flags used to lookup up using vfat_lookup */
	int fast_quit; /* for commands manipulating multiple files, quit
			* as soon as even _one_ file has a problem */

	char *shortname; /* where to put the short name of the matched file */
	char *longname; /* where to put the long name of the matched file */

	/* out parameters */
	Stream_t *File;

	direntry_t *direntry;  /* dir of this entry */
	char *unixSourceName;  /* filename of the last opened Unix source 
				* file (Unix equiv of Dos direntry) */

	Stream_t *targetDir; /* directory where to place files */
	char *unixTarget; /* directory on Unix where to put files */

	const char *targetName; /* basename of target file, or NULL if same
				 * basename as source should be conserved */

	char *originalArg; /* original argument, complete with wildcards */
	int basenameHasWildcard; /* true if there are wildcards in the
				  * basename */


	/* internal data */
	char mcwd[MAX_PATH+4];

	char *fileName; /* resolved Unix filename */
} MainParam_t;

void init_mp(MainParam_t *MainParam);
int main_loop(MainParam_t *MainParam, char **argv, int argc);

int target_lookup(MainParam_t *mp, const char *arg);

Stream_t *open_root_dir(char *drivename, int flags);

const char *mpGetBasename(MainParam_t *mp); /* statically allocated 
					     * string */

void mpPrintFilename(FILE *file, MainParam_t *mp);
const char *mpPickTargetName(MainParam_t *mp); /* statically allocated string */

char *mpBuildUnixFilename(MainParam_t *mp); /* dynamically allocated, must
					     * be freed */

int isSpecial(const char *name);

#define MISSED_ONE 2  /* set if one cmd line argument didn't match any files */
#define GOT_ONE 4     /* set if a match was found, used for exit status */
#define NO_CWD 8     /* file not found while looking for current working 
		      * directory */
#define ERROR_ONE 16 /* flat out error, such as problems with target file, 
			interrupt by user, etc. */
#define STOP_NOW 32 /* stop as soon as possible, not necessarily an error */

#endif
