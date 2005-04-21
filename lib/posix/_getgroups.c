/* getgroups.c						POSIX 4.2.3
 *	int getgroups(gidsetsize, grouplist);
 *
 *	This call relates to suplementary group ids, which are not
 *	supported in MINIX.
 */

#include <lib.h>
#define getgroups _getgroups
#include <unistd.h>
#include <time.h>

PUBLIC int getgroups(gidsetsize, grouplist)
int gidsetsize;
gid_t grouplist[];
{
  return(0);
}
