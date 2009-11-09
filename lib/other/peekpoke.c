/* Peek and poke using /dev/mem.
 *
 * Callers now ought to check the return values.
 *
 * Calling peek() requires read permission on /dev/mem, and consumes
 * a file descriptor.  Calling poke() requires write permission, and
 * consumes another file descriptor.
 */

#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>

_PROTOTYPE( int peek, (unsigned segment, unsigned offset));
_PROTOTYPE( int poke, (unsigned segment, unsigned offset, unsigned value));

#define SEGSIZE 0x10

int peek(segment, offset)
unsigned segment;
unsigned offset;
{
  unsigned char chvalue;
  static int infd = -1;

  if (infd < 0) infd = open("/dev/mem", O_RDONLY);
  if (infd < 0 ||
      lseek(infd, (unsigned long) segment * SEGSIZE + offset, SEEK_SET) < 0 ||
      read(infd, (char *) &chvalue, (unsigned) 1) != 1)
	return(-1);
  return(chvalue);
}

int poke(segment, offset, value)
unsigned segment;
unsigned offset;
unsigned value;
{
  unsigned char chvalue;
  static int outfd = -1;

  chvalue = value;
  if (outfd < 0) outfd = open("/dev/mem", O_WRONLY);
  if (outfd < 0 ||
      lseek(outfd, (unsigned long) segment * SEGSIZE + offset, SEEK_SET) < 0 ||
      write(outfd, (char *) &chvalue, (unsigned) 1) != 1)
	return(-1);
  return(chvalue);
}
