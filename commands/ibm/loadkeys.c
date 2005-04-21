/*	loadkeys - load national keyboard map		Author: Marcus Hampel
 */
#include <sys/types.h>
#include <sys/ioctl.h>
#include <minix/keymap.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#if __minix_vmd
#define KBD_DEVICE	"/dev/kbd"
#else
#define KBD_DEVICE	"/dev/console"
#endif

u16_t keymap[NR_SCAN_CODES * MAP_COLS];
u8_t comprmap[4 + NR_SCAN_CODES * MAP_COLS * 9/8 * 2 + 1];


void tell(char *s)
{
  write(2, s, strlen(s));
}


void fatal(char *say)
{
  int err = errno;
  tell("loadkeys: ");
  if (say != NULL) {
	tell(say);
	tell(": ");
  }
  tell(strerror(err));
  tell("\n");
  exit(1);
}


void usage(void)
{
  tell("Usage: loadkeys mapfile\n");
  exit(1);
}


int main(int argc, char *argv[])
{
  u8_t *cm;
  u16_t *km;
  int fd, n, fb;

  if (argc != 2)
	usage();

  if ((fd = open(argv[1], O_RDONLY)) < 0) fatal(argv[1]);

  if (read(fd, comprmap, sizeof(comprmap)) < 0) fatal(argv[1]);

  if (memcmp(comprmap, KEY_MAGIC, 4) != 0) {
	tell("loadkeys: ");
	tell(argv[1]);
	tell(": not a keymap file\n");
	exit(1);
  }
  close(fd);

  /* Decompress the keymap data. */
  cm = comprmap + 4;
  n = 8;
  for (km = keymap; km < keymap + NR_SCAN_CODES * MAP_COLS; km++) {
	if (n == 8) {
		/* Need a new flag byte. */
		fb = *cm++;
		n = 0;
	}
	*km = *cm++;			/* Low byte. */
	if (fb & (1 << n)) {
		*km |= (*cm++ << 8);	/* One of the few special keys. */
	}
	n++;
  }

  if ((fd = open(KBD_DEVICE, O_WRONLY)) < 0) fatal(KBD_DEVICE);

  if (ioctl(fd, KIOCSMAP, keymap) < 0) fatal(KBD_DEVICE);

  return 0;
}
