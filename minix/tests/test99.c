/*
 * test99: Linux evdev compatible input devices (/dev/input/event[01], see
 * <minix/evdev.h>).
 *
 * Without arguments, checks the device interface: identification and
 * capability ioctls, exclusive open, non-blocking reads, select, LED
 * writes.  With "-input", it also reads real input: it prints
 * "HARNESS:INPUT:keys" and "HARNESS:INPUT:mouse" lines, upon which the QEMU
 * harness (releasetools/x86_qemu.py --input) must inject:
 *
 *   keys:  sendkey a; sendkey shift-b
 *   mouse: mouse_move 10 5; mouse_button 1; mouse_button 0
 *
 * Needs root (setuid via run's "setuids"): the devices are root:operator.
 */
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/time.h>
#include <minix/evdev.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

int max_error = 5;
#include "common.h"

#define KBD_DEV		"/dev/input/event0"
#define MOUSE_DEV	"/dev/input/event1"

#define KEY_A		30
#define KEY_B		48
#define KEY_LEFTSHIFT	42

#define TEST_BIT(a, b)	((a)[(b) / 8] & (1 << ((b) % 8)))

static int
open_dev(const char *path, int flags)
{
	int fd;

	if ((fd = open(path, flags)) < 0) {
		if (errno == ENOENT)
			printf("(%s missing: run MAKEDEV input) ", path);
		return -1;
	}
	return fd;
}

/*
 * Identification and capabilities.
 */
static void
test_ident(void)
{
	struct evdev_id id;
	uint8_t bits[EVDEV_KEY_MAX / 8 + 1];
	char name[64];
	int fd, fd2, v;

	subtest = 1;

	if ((fd = open_dev(KBD_DEV, O_RDWR)) < 0) { e(1); return; }

	if (ioctl(fd, EVDEV_IOCGVERSION, &v) != 0) e(2);
	if (v != EVDEV_VERSION) e(3);
	if (ioctl(fd, EVDEV_IOCGID, &id) != 0) e(4);
	if (id.bustype != EVDEV_BUS_I8042) e(5);
	memset(name, 'x', sizeof(name));
	if (ioctl(fd, EVDEV_IOCGNAME(sizeof(name)), name) < 0) e(6);
	if (strcmp(name, "MINIX keyboard") != 0) e(7);
	/* A short buffer gets a prefix, like on Linux. */
	memset(name, 'x', sizeof(name));
	if (ioctl(fd, EVDEV_IOCGNAME(5), name) < 0) e(8);
	if (memcmp(name, "MINIXx", 6) != 0) e(9);
	if (ioctl(fd, EVDEV_IOCGPHYS(sizeof(name)), name) != -1 ||
	    errno != ENOENT) e(10);

	memset(bits, 0, sizeof(bits));
	if (ioctl(fd, EVDEV_IOCGBIT(0, sizeof(bits)), bits) < 0) e(11);
	if (!TEST_BIT(bits, EVDEV_EV_SYN) || !TEST_BIT(bits, EVDEV_EV_KEY) ||
	    !TEST_BIT(bits, EVDEV_EV_LED) || TEST_BIT(bits, EVDEV_EV_REL))
		e(12);
	memset(bits, 0, sizeof(bits));
	if (ioctl(fd, EVDEV_IOCGBIT(EVDEV_EV_KEY, sizeof(bits)), bits) < 0)
		e(13);
	if (!TEST_BIT(bits, KEY_A) || !TEST_BIT(bits, KEY_LEFTSHIFT) ||
	    TEST_BIT(bits, EVDEV_BTN_LEFT)) e(14);
	memset(bits, 0, sizeof(bits));
	if (ioctl(fd, EVDEV_IOCGBIT(EVDEV_EV_LED, sizeof(bits)), bits) < 0)
		e(15);
	if (!TEST_BIT(bits, EVDEV_LED_CAPSL)) e(16);
	memset(bits, 0xff, sizeof(bits));
	if (ioctl(fd, EVDEV_IOCGKEY(sizeof(bits)), bits) < 0) e(17);
	if (TEST_BIT(bits, KEY_A)) e(18);		/* nothing held */

	/* Exclusive open. */
	if ((fd2 = open(KBD_DEV, O_RDONLY)) != -1 || errno != EBUSY) e(19);
	if (fd2 >= 0) close(fd2);

	/* Clock selection; grabbing is implied but accepted. */
	v = CLOCK_REALTIME;
	if (ioctl(fd, EVDEV_IOCSCLOCKID, &v) != 0) e(20);
	v = 12345;
	if (ioctl(fd, EVDEV_IOCSCLOCKID, &v) != -1 || errno != EINVAL) e(21);
	v = 1;
	if (ioctl(fd, EVDEV_IOCGRAB, &v) != 0) e(22);
	if (ioctl(fd, _IOR('E', 0x40, int), &v) != -1) e(23);	/* GABS */
	if (ioctl(fd, _IOR('Q', 0x01, int), &v) != -1 ||
	    errno != ENOTTY) e(24);

	if (close(fd) != 0) e(25);

	/* Can be opened again after close. */
	if ((fd = open_dev(KBD_DEV, O_RDONLY)) < 0) e(26);
	else close(fd);

	if ((fd = open_dev(MOUSE_DEV, O_RDWR)) < 0) { e(27); return; }
	if (ioctl(fd, EVDEV_IOCGNAME(sizeof(name)), name) < 0) e(28);
	if (strcmp(name, "MINIX mouse") != 0) e(29);
	memset(bits, 0, sizeof(bits));
	if (ioctl(fd, EVDEV_IOCGBIT(0, sizeof(bits)), bits) < 0) e(30);
	if (!TEST_BIT(bits, EVDEV_EV_REL) || !TEST_BIT(bits, EVDEV_EV_KEY))
		e(31);
	memset(bits, 0, sizeof(bits));
	if (ioctl(fd, EVDEV_IOCGBIT(EVDEV_EV_REL, sizeof(bits)), bits) < 0)
		e(32);
	if (!TEST_BIT(bits, EVDEV_REL_X) || !TEST_BIT(bits, EVDEV_REL_Y))
		e(33);
	memset(bits, 0, sizeof(bits));
	if (ioctl(fd, EVDEV_IOCGBIT(EVDEV_EV_KEY, sizeof(bits)), bits) < 0)
		e(34);
	if (!TEST_BIT(bits, EVDEV_BTN_LEFT) || TEST_BIT(bits, KEY_A)) e(35);
	if (close(fd) != 0) e(36);
}

/*
 * Reading without input, select, writes.
 */
static void
test_io(void)
{
	struct evdev_event ev[4];
	struct pollfd pfd;
	fd_set rfds;
	struct timeval tv;
	int fd;

	subtest = 2;

	if ((fd = open_dev(KBD_DEV, O_RDWR | O_NONBLOCK)) < 0) { e(1); return; }

	if (read(fd, ev, sizeof(ev)) != -1 || errno != EAGAIN) e(2);
	if (read(fd, ev, sizeof(ev[0]) - 1) != -1 || errno != EINVAL) e(3);

	FD_ZERO(&rfds);
	FD_SET(fd, &rfds);
	tv.tv_sec = 0;
	tv.tv_usec = 100000;
	if (select(fd + 1, &rfds, NULL, NULL, &tv) != 0) e(4);

	pfd.fd = fd;
	pfd.events = POLLIN | POLLOUT;
	if (poll(&pfd, 1, 0) != 1 || pfd.revents != POLLOUT) e(5);

	/* Keyboard LEDs: set caps lock, read the state back, clear it. */
	memset(ev, 0, sizeof(ev));
	ev[0].type = EVDEV_EV_LED;
	ev[0].code = EVDEV_LED_CAPSL;
	ev[0].value = 1;
	ev[1].type = EVDEV_EV_SYN;
	ev[1].code = EVDEV_SYN_REPORT;
	if (write(fd, ev, 2 * sizeof(ev[0])) != 2 * sizeof(ev[0])) e(6);
	{
		uint8_t leds[2] = { 0, 0 };
		if (ioctl(fd, EVDEV_IOCGLED(sizeof(leds)), leds) < 0) e(7);
		if (leds[0] != (1 << EVDEV_LED_CAPSL)) e(8);
	}
	ev[0].value = 0;
	if (write(fd, ev, 2 * sizeof(ev[0])) != 2 * sizeof(ev[0])) e(9);
	if (write(fd, ev, sizeof(ev[0]) + 1) != -1 || errno != EINVAL) e(10);

	if (close(fd) != 0) e(11);
}

/*
 * Read events until 'n' have been collected or 'secs' passed.
 */
static int
collect(int fd, struct evdev_event *evs, int n, int secs)
{
	struct pollfd pfd;
	ssize_t r;
	int got;

	got = 0;
	while (got < n) {
		pfd.fd = fd;
		pfd.events = POLLIN;
		if (poll(&pfd, 1, secs * 1000) != 1)
			break;
		r = read(fd, &evs[got], (n - got) * sizeof(evs[0]));
		if (r < 0) {
			if (errno == EAGAIN)
				continue;
			break;
		}
		if (r % sizeof(evs[0]) != 0)
			return -1;
		got += r / sizeof(evs[0]);
	}
	return got;
}

static int
is_syn(const struct evdev_event *ev)
{

	return ev->type == EVDEV_EV_SYN && ev->code == EVDEV_SYN_REPORT;
}

/*
 * Real input, injected by the harness.
 */
static void
test_input(void)
{
	struct evdev_event ev[64];
	struct timespec now;
	int fd, n, i, j, keys, presses, releases, shift, dx, dy, down, up;
	int frames;
	int64_t t, prev;

	subtest = 3;

	if ((fd = open_dev(KBD_DEV, O_RDONLY | O_NONBLOCK)) < 0) { e(1); return; }

	/*
	 * "sendkey a; sendkey shift-b": a down, a up, shift down, b down,
	 * b up, shift up.  Events of one clock tick may share a frame, but a
	 * frame never holds two events for the same key.
	 */
	printf("\nHARNESS:INPUT:keys\n");
	fflush(stdout);
	n = keys = 0;
	while (n < 32 && (keys < 6 || !is_syn(&ev[n - 1]))) {
		if (collect(fd, &ev[n], 1, 10) != 1)
			break;
		if (ev[n].type == EVDEV_EV_KEY)
			keys++;
		n++;
	}
	if (keys != 6) { printf("got %d key events\n", keys); e(2); }
	presses = releases = shift = frames = 0;
	prev = 0;
	for (i = 0; i < n; i++) {
		t = (int64_t)ev[i].time.tv_sec * 1000000 + ev[i].time.tv_usec;
		if (t < prev) e(3);	/* monotonic timestamps */
		prev = t;
		if (is_syn(&ev[i])) {
			frames++;
			if (i == 0 || is_syn(&ev[i - 1])) e(4);	/* empty */
			continue;
		}
		if (ev[i].type != EVDEV_EV_KEY) e(5);
		for (j = i - 1; j >= 0 && !is_syn(&ev[j]); j--)
			if (ev[j].code == ev[i].code) e(6);	/* same frame */
		if (ev[i].code == KEY_LEFTSHIFT) { shift++; continue; }
		if (ev[i].code != KEY_A && ev[i].code != KEY_B) e(7);
		if (ev[i].value == 1) presses++;
		else if (ev[i].value == 0) releases++;
		else e(8);
	}
	if (presses != 2 || releases != 2 || shift != 2) e(9);
	if (frames < 2 || frames > 6) e(10);
	/* The first event is "a" pressed. */
	if (n > 0 && (ev[0].code != KEY_A || ev[0].value != 1)) e(11);

	/* Timestamps are CLOCK_MONOTONIC: recent, not in the future. */
	clock_gettime(CLOCK_MONOTONIC, &now);
	t = (int64_t)now.tv_sec * 1000000 + now.tv_nsec / 1000;
	if (prev > t || t - prev > 15 * 1000000) e(12);

	/* While the evdev device is open, nobody else gets keys: nothing
	 * may have reached the console (TTY) - checked by the harness log. */
	if (close(fd) != 0) e(13);

	if ((fd = open_dev(MOUSE_DEV, O_RDONLY | O_NONBLOCK)) < 0) { e(14); return; }

	/* "mouse_move 10 5; mouse_button 1; mouse_button 0". */
	printf("\nHARNESS:INPUT:mouse\n");
	fflush(stdout);
	dx = dy = down = up = 0;
	for (i = 0; i < 20 && (down == 0 || up == 0); ) {
		n = collect(fd, &ev[i], 1, 10);
		if (n != 1)
			break;
		if (ev[i].type == EVDEV_EV_REL && ev[i].code == EVDEV_REL_X)
			dx += ev[i].value;
		if (ev[i].type == EVDEV_EV_REL && ev[i].code == EVDEV_REL_Y)
			dy += ev[i].value;
		if (ev[i].type == EVDEV_EV_KEY &&
		    ev[i].code == EVDEV_BTN_LEFT) {
			if (ev[i].value) down++; else up++;
		}
		i++;
	}
	if (dx <= 0 || dy <= 0) {
		/* QEMU's mouse_move dy > 0 is downward: REL_Y > 0. */
		printf("dx %d dy %d\n", dx, dy);
		e(15);
	}
	if (down != 1 || up != 1) e(16);
	if (close(fd) != 0) e(17);
}

int
main(int argc, char **argv)
{
	int input;

	input = (argc == 2 && strcmp(argv[1], "-input") == 0);

	start(99);

	test_ident();
	test_io();
	if (input)
		test_input();

	quit();
	return 0;
}
