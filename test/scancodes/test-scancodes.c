#define _MINIX

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

/* list of scancodes to demonstrate whether the keycodes are correct;
 * source: http://www.win.tue.nl/~aeb/linux/kbd/scancodes-1.html
 */
static char *keydescr[] = {
	NULL,		/* 0x00 */
	"Esc",		/* 0x01 */
	"1!",		/* 0x02 */
	"2@",		/* 0x03 */
	"3#",		/* 0x04 */
	"4$",		/* 0x05 */
	"5%",		/* 0x06 */
	"6^",		/* 0x07 */
	"7&",		/* 0x08 */
	"8*",		/* 0x09 */
	"9(",		/* 0x0a */
	"0)",		/* 0x0b */
	"-_",		/* 0x0c */
	"=+",		/* 0x0d */
	"Backspace",	/* 0x0e */
	"Tab",		/* 0x0f */
	"Q",		/* 0x10 */
	"W",		/* 0x11 */
	"E",		/* 0x12 */
	"R",		/* 0x13 */
	"T",		/* 0x14 */
	"Y",		/* 0x15 */
	"U",		/* 0x16 */
	"I",		/* 0x17 */
	"O",		/* 0x18 */
	"P",		/* 0x19 */
	"[{",		/* 0x1a */
	"]}",		/* 0x1b */
	"Enter",	/* 0x1c */
	"LCtrl",	/* 0x1d */
	"A",		/* 0x1e */
	"S",		/* 0x1f */
	"D",		/* 0x20 */
	"F",		/* 0x21 */
	"G",		/* 0x22 */
	"H",		/* 0x23 */
	"J",		/* 0x24 */
	"K",		/* 0x25 */
	"L",		/* 0x26 */
	";:",		/* 0x27 */
	"'\"",		/* 0x28 */
	"`~",		/* 0x29 */
	"LShift",	/* 0x2a */
	"\\|",		/* 0x2b */
	"Z",		/* 0x2c */
	"X",		/* 0x2d */
	"C",		/* 0x2e */
	"V",		/* 0x2f */
	"B",		/* 0x30 */
	"N",		/* 0x31 */
	"M",		/* 0x32 */
	",<",		/* 0x33 */
	".>",		/* 0x34 */
	"/?",		/* 0x35 */
	"RShift",	/* 0x36 */
	"Keypad-*",	/* 0x37 */
	"LAlt",		/* 0x38 */
	"Space bar",	/* 0x39 */
	"CapsLock",	/* 0x3a */
	"F1",		/* 0x3b */
	"F2",		/* 0x3c */
	"F3",		/* 0x3d */
	"F4",		/* 0x3e */
	"F5",		/* 0x3f */
	"F6",		/* 0x40 */
	"F7",		/* 0x41 */
	"F8",		/* 0x42 */
	"F9",		/* 0x43 */
	"F10",		/* 0x44 */
	"NumLock",	/* 0x45 */
	"ScrollLock",	/* 0x46 */
	"Keypad-7/Home",/* 0x47 */
	"Keypad-8/Up",	/* 0x48 */
	"Keypad-9/PgUp",/* 0x49 */
	"Keypad--",	/* 0x4a */
	"Keypad-4/Left",/* 0x4b */
	"Keypad-5",	/* 0x4c */
	"Keypad-6/Right",/* 0x4d */
	"Keypad-+",	/* 0x4e */
	"Keypad-1/End",	/* 0x4f */
	"Keypad-2/Down",/* 0x50 */
	"Keypad-3/PgDn",/* 0x51 */
	"Keypad-0/Ins",	/* 0x52 */
	"Keypad-./Del",	/* 0x53 */
	"Alt-SysRq",	/* 0x54 */
	NULL,		/* 0x55 */
	NULL,		/* 0x56 */
	"F11",		/* 0x57 */
	"F12",		/* 0x58 */
	NULL,		/* 0x59 */
	NULL,		/* 0x5a */
	NULL,		/* 0x5b */
	NULL,		/* 0x5c */
	NULL,		/* 0x5d */
	NULL,		/* 0x5e */
	NULL,		/* 0x5f */
	NULL,		/* 0x60 */
	NULL,		/* 0x61 */
	NULL,		/* 0x62 */
	NULL,		/* 0x63 */
	NULL,		/* 0x64 */
	NULL,		/* 0x65 */
	NULL,		/* 0x66 */
	NULL,		/* 0x67 */
	NULL,		/* 0x68 */
	NULL,		/* 0x69 */
	NULL,		/* 0x6a */
	NULL,		/* 0x6b */
	NULL,		/* 0x6c */
	NULL,		/* 0x6d */
	NULL,		/* 0x6e */
	NULL,		/* 0x6f */
	NULL,		/* 0x70 */
	NULL,		/* 0x71 */
	NULL,		/* 0x72 */
	NULL,		/* 0x73 */
	NULL,		/* 0x74 */
	NULL,		/* 0x75 */
	NULL,		/* 0x76 */
	NULL,		/* 0x77 */
	NULL,		/* 0x78 */
	NULL,		/* 0x79 */
	NULL,		/* 0x7a */
	NULL,		/* 0x7b */
	NULL,		/* 0x7c */
	NULL,		/* 0x7d */
	NULL,		/* 0x7e */
	NULL,		/* 0x7f */
};

static char *keydescresc[] = {
	NULL,		/* 0xe0 0x00 */
	NULL,		/* 0xe0 0x01 */
	NULL,		/* 0xe0 0x02 */
	NULL,		/* 0xe0 0x03 */
	NULL,		/* 0xe0 0x04 */
	NULL,		/* 0xe0 0x05 */
	NULL,		/* 0xe0 0x06 */
	NULL,		/* 0xe0 0x07 */
	NULL,		/* 0xe0 0x08 */
	NULL,		/* 0xe0 0x09 */
	NULL,		/* 0xe0 0x0a */
	NULL,		/* 0xe0 0x0b */
	NULL,		/* 0xe0 0x0c */
	NULL,		/* 0xe0 0x0d */
	NULL,		/* 0xe0 0x0e */
	NULL,		/* 0xe0 0x0f */
	NULL,		/* 0xe0 0x10 */
	NULL,		/* 0xe0 0x11 */
	NULL,		/* 0xe0 0x12 */
	NULL,		/* 0xe0 0x13 */
	NULL,		/* 0xe0 0x14 */
	NULL,		/* 0xe0 0x15 */
	NULL,		/* 0xe0 0x16 */
	NULL,		/* 0xe0 0x17 */
	NULL,		/* 0xe0 0x18 */
	NULL,		/* 0xe0 0x19 */
	NULL,		/* 0xe0 0x1a */
	NULL,		/* 0xe0 0x1b */
	"Keypad Enter",	/* 0xe0 0x1c */
	"RCtrl",	/* 0xe0 0x1d */
	NULL,		/* 0xe0 0x1e */
	NULL,		/* 0xe0 0x1f */
	NULL,		/* 0xe0 0x20 */
	NULL,		/* 0xe0 0x21 */
	NULL,		/* 0xe0 0x22 */
	NULL,		/* 0xe0 0x23 */
	NULL,		/* 0xe0 0x24 */
	NULL,		/* 0xe0 0x25 */
	NULL,		/* 0xe0 0x26 */
	NULL,		/* 0xe0 0x27 */
	NULL,		/* 0xe0 0x28 */
	NULL,		/* 0xe0 0x29 */
	"fake LShift",	/* 0xe0 0x2a */
	NULL,		/* 0xe0 0x2b */
	NULL,		/* 0xe0 0x2c */
	NULL,		/* 0xe0 0x2d */
	NULL,		/* 0xe0 0x2e */
	NULL,		/* 0xe0 0x2f */
	NULL,		/* 0xe0 0x30 */
	NULL,		/* 0xe0 0x31 */
	NULL,		/* 0xe0 0x32 */
	NULL,		/* 0xe0 0x33 */
	NULL,		/* 0xe0 0x34 */
	"Keypad-/",	/* 0xe0 0x35 */
	"fake RShift",	/* 0xe0 0x36 */
	"Ctrl-PrtScn",	/* 0xe0 0x37 */
	"RAlt",		/* 0xe0 0x38 */
	NULL,		/* 0xe0 0x39 */
	NULL,		/* 0xe0 0x3a */
	NULL,		/* 0xe0 0x3b */
	NULL,		/* 0xe0 0x3c */
	NULL,		/* 0xe0 0x3d */
	NULL,		/* 0xe0 0x3e */
	NULL,		/* 0xe0 0x3f */
	NULL,		/* 0xe0 0x40 */
	NULL,		/* 0xe0 0x41 */
	NULL,		/* 0xe0 0x42 */
	NULL,		/* 0xe0 0x43 */
	NULL,		/* 0xe0 0x44 */
	NULL,		/* 0xe0 0x45 */
	"Ctrl-Break",	/* 0xe0 0x46 */
	"Grey Home",	/* 0xe0 0x47 */
	"Grey Up",	/* 0xe0 0x48 */
	"Grey PgUp",	/* 0xe0 0x49 */
	NULL,		/* 0xe0 0x4a */
	"Grey Left",	/* 0xe0 0x4b */
	NULL,		/* 0xe0 0x4c */
	"Grey Right",	/* 0xe0 0x4d */
	NULL,		/* 0xe0 0x4e */
	"Grey End",	/* 0xe0 0x4f */
	"Grey Down",	/* 0xe0 0x50 */
	"Grey PgDn",	/* 0xe0 0x51 */
	"Grey Insert",	/* 0xe0 0x52 */
	"Grey Delete",	/* 0xe0 0x53 */
	NULL,		/* 0xe0 0x54 */
	NULL,		/* 0xe0 0x55 */
	NULL,		/* 0xe0 0x56 */
	NULL,		/* 0xe0 0x57 */
	NULL,		/* 0xe0 0x58 */
	NULL,		/* 0xe0 0x59 */
	NULL,		/* 0xe0 0x5a */
	"LeftWindow",	/* 0xe0 0x5b */
	"RightWindow",	/* 0xe0 0x5c */
	"Menu",		/* 0xe0 0x5d */ 
	"Power",	/* 0xe0 0x5e */
	"Sleep",	/* 0xe0 0x5f */
	NULL,		/* 0xe0 0x60 */
	NULL,		/* 0xe0 0x61 */
	NULL,		/* 0xe0 0x62 */
	"Wake",		/* 0xe0 0x63 */
	NULL,		/* 0xe0 0x64 */
	NULL,		/* 0xe0 0x65 */
	NULL,		/* 0xe0 0x66 */
	NULL,		/* 0xe0 0x67 */
	NULL,		/* 0xe0 0x68 */
	NULL,		/* 0xe0 0x69 */
	NULL,		/* 0xe0 0x6a */
	NULL,		/* 0xe0 0x6b */
	NULL,		/* 0xe0 0x6c */
	NULL,		/* 0xe0 0x6d */
	NULL,		/* 0xe0 0x6e */
	NULL,		/* 0xe0 0x6f */
	NULL,		/* 0xe0 0x70 */
	NULL,		/* 0xe0 0x71 */
	NULL,		/* 0xe0 0x72 */
	NULL,		/* 0xe0 0x73 */
	NULL,		/* 0xe0 0x74 */
	NULL,		/* 0xe0 0x75 */
	NULL,		/* 0xe0 0x76 */
	NULL,		/* 0xe0 0x77 */
	NULL,		/* 0xe0 0x78 */
	NULL,		/* 0xe0 0x79 */
	NULL,		/* 0xe0 0x7a */
	NULL,		/* 0xe0 0x7b */
	NULL,		/* 0xe0 0x7c */
	NULL,		/* 0xe0 0x7d */
	NULL,		/* 0xe0 0x7e */
	NULL,		/* 0xe0 0x7f */
};

#define CHECK(r) check((r), #r, __FILE__, __LINE__)

int check(long r, const char *expr, const char *file, int line)
{
	char buffer[256];
	if (r < 0) {
		snprintf(buffer, sizeof(buffer), "%s:%d: %s: result %ld, %s", 
			file, line, expr, r, strerror(errno));
		exit(-1);
	}
	return r;	
}

#define SCODE_ESC	0xe0
#define SCODE_BREAK	0x80

static int testscancode(int fd)
{
	static int escape, lctrl, rctrl;
	ssize_t count;
	unsigned char scode;
	char *scodedescr;

	/* read a scancode and test for EOF */
	CHECK(count = read(fd, &scode, sizeof(scode)));
	if (count < sizeof(scode)) {
		return 0;
	}

	/* print scancode */
	printf("0x%.2x ", scode);
	fflush(stdout);

	/* test for escape */
	if (!escape && scode == SCODE_ESC) {
		escape = 1;
		return 1;
	}

	/* describe scancode */
	scodedescr = (escape ? keydescresc : keydescr)[scode & ~SCODE_BREAK];
	if (scodedescr)
		printf("[%s] ", scodedescr);

	if (scode & SCODE_BREAK)
		printf("up\n");
	else
		printf("down\n");

	fflush(stdout);

	/* exit on ctrl-C */
	if ((scode & ~SCODE_BREAK) == 0x1d) {
		if (escape)
			rctrl = !(scode & SCODE_BREAK);
		else
			lctrl = !(scode & SCODE_BREAK);
	}
	if ((lctrl || rctrl) && !escape && scode == 0x2e) {
		return 0;
	}

	/* next key is not escaped */
	escape = 0;

	return 1;
}

static volatile int terminate;

static void set_terminate(int signum)
{
	terminate = signum;
}

static int testscancodes(int fd)
{
	struct termios termios_old, termios_scan;

	/* this test only works with a TTY as stdin */
	if (!CHECK(isatty(fd))) {
		printf("warning: this test can only be run from a console\n");
		return 0;
	}

	/* catch fatal signals to restore the console */
	CHECK((signal(SIGHUP, set_terminate) == SIG_ERR) ? -1 : 0);
	CHECK((signal(SIGINT, set_terminate) == SIG_ERR) ? -1 : 0);
	CHECK((signal(SIGQUIT, set_terminate) == SIG_ERR) ? -1 : 0);
	CHECK((signal(SIGABRT, set_terminate) == SIG_ERR) ? -1 : 0);
	CHECK((signal(SIGPIPE, set_terminate) == SIG_ERR) ? -1 : 0);
	CHECK((signal(SIGTERM, set_terminate) == SIG_ERR) ? -1 : 0);
	
	/* configure tty in raw input mode with scancodes and no echo */
	CHECK(tcgetattr(fd, &termios_old));
	termios_scan = termios_old;
	termios_scan.c_iflag &= ~(BRKINT | ICRNL | IGNBRK | IGNCR | IGNPAR);
	termios_scan.c_iflag &= ~(INLCR | INPCK | ISTRIP); 
	termios_scan.c_iflag &= ~(IXOFF | IXON | PARMRK); 
	termios_scan.c_iflag |= SCANCODES; 
	termios_scan.c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHONL);
	termios_scan.c_lflag &= ~(ICANON | IEXTEN | ISIG | NOFLSH);
	CHECK(tcsetattr(fd, TCSANOW, &termios_scan));
	
	/* test: is scancode input supported? */
	CHECK(tcgetattr(fd, &termios_scan));
	if (termios_scan.c_iflag & SCANCODES) {
		while (!terminate && CHECK(testscancode(fd))) ;
	} else {
		printf("warning: cannot enable SCANCODES "
			"(are you running from a console?)\n");
	}

	/* report if closed by a signal */
	if (terminate) {
		printf("received signal %d, shutting down\n", terminate);
	}

	/* restore original input mode */
	CHECK(tcsetattr(fd, TCSANOW, &termios_old));

	/* clear buffered input */
	CHECK(tcflush(fd, TCIFLUSH));
}

int main(void)
{
	printf("try out some keys to find out whether SCANCODES works\n");
	printf("press CTRL+C to end this test\n");
	printf("please note that this test only works from a console tty\n");

	/* perform test using stdin */
	if (testscancodes(STDIN_FILENO) < 0)
		return -1;
	else
		return 0;
}
