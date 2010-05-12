/* update - do sync periodically		Author: Andy Tanenbaum */

#include <sys/types.h>
#include <signal.h>
#include <unistd.h>

_PROTOTYPE(int main, (void));

int main()
{
  /* Release all (?) open file descriptors. */
  close(0);
  close(1);
  close(2);

  /* Release current directory to avoid locking current device. */
  chdir("/");

  /* Flush the cache every 30 seconds. */
  while (1) {
	sync();
	sleep(30);
  }
}
