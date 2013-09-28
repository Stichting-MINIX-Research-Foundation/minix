/*	genmap - output binary keymap			Author: Marcus Hampel
 */
#include <sys/types.h>
#ifdef __minix
#include <minix/keymap.h>
#else
#include "../../../include/minix/keymap.h"
#endif
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

/*
 * if we crosscompile those might not be defined,
 */
#ifndef u16_t
#include <stdint.h>
typedef uint16_t u16_t;
#endif

#ifndef u8_t
#include <stdint.h>
typedef uint8_t u8_t;
#endif

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
