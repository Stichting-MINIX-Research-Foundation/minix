/* sync - flush the file system buffers.  Author: Andy Tanenbaum */

#include <sys/types.h>
#include <unistd.h>

_PROTOTYPE(int main, (void));

int main()
{
/* First prize in shortest useful program contest. */
/* Highest comment/code ratio */
  sync();
  return(0);
}
