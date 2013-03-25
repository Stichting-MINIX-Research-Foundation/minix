/*	loadkeys - load national keyboard map		Author: Marcus Hampel
 */
#include <sys/types.h>
#include <sys/ioctl.h>
#include <minix/keymap.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#define KBD_DEVICE	"/dev/console"

void fatal(char *say)
{
  fprintf(stderr, "loadkeys: %s: %s\n", say, strerror(errno));
  exit(EXIT_FAILURE);
}


void usage(void)
{
  fprintf(stderr, "usage: loadkeys <mapfile>\n");
  exit(EXIT_FAILURE);
}


int main(int argc, char *argv[])
{
  char sig[4];
  keymap_t keymap;
  int fd;

  if (argc != 2)
	usage();

  if ((fd = open(argv[1], O_RDONLY)) < 0) fatal(argv[1]);

  if (read(fd, sig, sizeof(sig)) < sizeof(sig)) fatal(argv[1]);

  if (memcmp(sig, KEY_MAGIC, sizeof(sig)) != 0) {
	fprintf(stderr, "loadkeys: %s: not a keymap file\n", argv[1]);
	return EXIT_FAILURE;
  }

  if (read(fd, keymap, sizeof(keymap)) < sizeof(keymap)) fatal(argv[1]);

  close(fd);

  if ((fd = open(KBD_DEVICE, O_WRONLY)) < 0) fatal(KBD_DEVICE);

  if (ioctl(fd, KIOCSMAP, keymap) < 0) fatal(KBD_DEVICE);

  return EXIT_SUCCESS;
}
