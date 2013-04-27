/* Extended V1 minixFS as defined by Linux
 * The difference with the normal V1 file systems as used on MINIX are the
 * size of the file names in the directoru entries, which are extended
 * to 30 characters (instead of 14.)
 */

/* Constants; unchanged from regular V1... */
#include "../v1/const.h"

/* ... except for magic number contained in super-block: */
#define SUPER_V1L	0x138F	/* magic # for "Linux" extended V1 minixFS */
#undef SUPER_MAGIC
#define SUPER_MAGIC	SUPER_V1L
