/* $Header$
 *
 * $Log$
 * Revision 1.1  2005/04/21 14:55:11  beng
 * Initial revision
 *
 * Revision 1.1.1.1  2005/04/20 13:33:20  beng
 * Initial import of minix 2.0.4
 *
 * Revision 2.0  86/09/17  15:40:11  lwall
 * Baseline for netwide release.
 * 
 */

#include "EXTERN.h"
#include "common.h"
#include "util.h"
#include "INTERN.h"
#include "patchlevel.h"
#include "version.h"

/* Print out the version number and die. */

void
version()
{
    extern char rcsid[];

#ifdef lint
    rcsid[0] = rcsid[0];
#else
    fatal3("%s\nPatch level: %d\n", rcsid, PATCHLEVEL);
#endif
}
