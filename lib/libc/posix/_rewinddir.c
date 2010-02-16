/*	rewinddir()					Author: Kees J. Bot
 *								24 Apr 1989
 */
#define nil 0
#include <lib.h>
#define rewinddir _rewinddir
#define seekdir	_seekdir
#include <sys/types.h>
#include <dirent.h>

void rewinddir(DIR *dp)
{
	(void) seekdir(dp, 0);
}
