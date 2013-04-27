/* Extended V2 minixFS as defined by Linux
 * The difference with the normal V2 file systems as used on MINIX are the
 * size of the file names in the directoru entries, which are extended
 * to 30 characters (instead of 14.)
 */

/* Constants; unchanged from regular V2... */
#include "../v2/const.h"

/* ... except for magic number contained in super-block: */
#define SUPER_V2L	0x2478	/* magic # for "Linux" extended V2 minixFS */
#undef SUPER_MAGIC
#define SUPER_MAGIC	SUPER_V2L
