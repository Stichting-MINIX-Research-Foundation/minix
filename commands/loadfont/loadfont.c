/* loadfont.c - Load custom font into EGA, VGA video card
 *
 * Author: Hrvoje Stipetic (hs@hck.hr) Jun-1995.
 *
 */

#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <termios.h>
#include <sys/ioctl.h>

void tell(char *s)
{
  write(2, s, strlen(s));
}

char *itoa(unsigned i)
{
  static char a[3*sizeof(int)];
  char *p = a+sizeof(a)-1;

  do {
	*--p = '0' + i%10;
  } while ((i /= 10) > 0);

  return p;
}

void report(char *say)
{
  int err = errno;
  tell("loadfont: ");
  if (say != NULL) {
	tell(say);
	tell(": ");
  }
  tell(strerror(err));
  tell("\n");
}

void usage(void)
{
  tell("Usage: loadfont fontfile\n");
  exit(1);
}


int main(int argc, char *argv[])
{
  static u8_t font[256][32];
  static u8_t font_file[256 * (16+14+8) + 1];
  u8_t *ff;
  int fd, size, tsize, ch, ln;
  struct winsize ws;


  if (argc != 2)
	usage();

  if ((fd = open(argv[1], O_RDONLY)) < 0) {
	report(argv[1]);
	exit(1);
  }

  switch (read(fd, font_file, sizeof(font_file))) {
  case 256 * 8:
  	size = 8;
  	break;
  case 256 * 14:
  	size = 14;
  	break;
  case 256 * 16:
  	size = 16;
  	break;
  case 256 * (16+14+8):
  	size = 0;
  	break;
  case -1:
	report(argv[1]);
	exit(1);
  default:
	tell("loadfont: ");
	tell(argv[1]);
	tell(": fontfile is not an 8x8, 8x14, 8x16, or compound font\n");
	exit(1);
  }
  close(fd);

  if (ioctl(0, TIOCGWINSZ, &ws) < 0 || (errno= ENOTTY, ws.ws_row == 0)) {
	report(NULL);
	exit(1);
  }
  tsize = ws.ws_ypixel / ws.ws_row;

  if (size == 0) {
	if (tsize >= 16) {
		ff = font_file + 256 * (0);
	} else
	if (tsize >= 14) {
		ff = font_file + 256 * (16);
	} else {
		ff = font_file + 256 * (16 + 14);
	}
	size = tsize;
  } else {
	ff = font_file;
  }

  for (ch = 0; ch < 256; ch++) {
	for (ln = 0; ln < size; ln++) font[ch][ln] = ff[ch * size + ln]; 
  }

  if (ioctl(0, TIOCSFON, font) < 0) {
	report(NULL);
	exit(1);
  }
  exit(0);
}
