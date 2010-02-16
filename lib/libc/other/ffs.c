#include <lib.h>
/*  ffs(3)
 *
 *  Author: Terrence W. Holm          Sep. 1988
 */
_PROTOTYPE( int ffs, (int word));

int ffs(word)
int word;
{
  int i;

  if (word == 0) return(0);

  for (i = 1;; ++i, word >>= 1)
	if (word & 1) return(i);
}
