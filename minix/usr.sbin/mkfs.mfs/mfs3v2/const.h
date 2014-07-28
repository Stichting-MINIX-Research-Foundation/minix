/* "V2" minixFS as handled by MINIX 3.x
 *
 * The difference with the normal V2 file systems is due to the use of
 * the V3 declaration for the super block (struct super) and the directory
 * entries (struct direct); this allows to use more than 65,535 inodes
 * and filenames of up to 60 characters.
 * A normal MINIX 2.0.x installation cannot read these file systems.
 *
 * The differences with a V3 file system with a block-size of 1024 are
 * limited to the use of a different magic number, since the inodes
 * have the same layout in both V2 and V3 file systems.
 */

/* Constants; unchanged from regular V2... */
#include "../v2/const.h"
