/*	genmap - output binary keymap			Author: Marcus Hampel
 */
#include <sys/types.h>
#include <minix/keymap.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include KEYSRC

u8_t comprmap[4 + NR_SCAN_CODES * MAP_COLS * 9/8 * 2 + 1];

void tell(const char *s)
{
  write(2, s, strlen(s));
}

int main(void)
{
  u8_t *cm, *fb;
  u16_t *km;
  int n;

  /* Compress the keymap. */
  memcpy(comprmap, KEY_MAGIC, 4);
  cm = comprmap + 4;
  n = 8;
  for (km = keymap; km < keymap + NR_SCAN_CODES * MAP_COLS; km++) {
	if (n == 8) {
		/* Allocate a new flag byte. */
		fb = cm;
		*cm++ = 0;
		n= 0;
	}
	*cm++ = (*km & 0x00FF);		/* Low byte. */
	if (*km & 0xFF00) {
		*cm++ = (*km >> 8);	/* High byte only when set. */
		*fb |= (1 << n);	/* Set a flag if so. */
	}
	n++;
  }

  /* Don't store trailing zeros. */
  while (cm > comprmap && cm[-1] == 0) cm--;

  /* Emit the compressed keymap. */
  if (write(1, comprmap, cm - comprmap) < 0) {
	int err = errno;

	tell("genmap: ");
	tell(strerror(err));
	tell("\n");
	exit(1);
  }
  exit(0);
}
