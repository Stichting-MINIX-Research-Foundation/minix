/*	genmap - output binary keymap			Author: Marcus Hampel
 */
#include <sys/types.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "include/minix/input.h"
#include "include/minix/keymap.h"

keymap_t keymap = {
#include KEYSRC
};

int main(void)
{
  /* This utility used to do compression, but the entire keymap fits in a
   * single 4K file system block now anyway, so who cares anymore?
   */
  if (write(1, KEY_MAGIC, 4) != 4) {
	perror("write");
	return EXIT_FAILURE;
  }
  if (write(1, keymap, sizeof(keymap)) != sizeof(keymap)) {
	perror("write");
	return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
