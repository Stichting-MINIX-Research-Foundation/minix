/* test97: mmap(2) of character devices, and the DRM-lite display device.
 *
 * Part 1 (always): a character device whose driver does not support mmap
 * must fail cleanly with ENODEV (VFS asks the driver with the FIOMMAP ioctl).
 *
 * Part 2 (only if /dev/drm0 can be opened, i.e. the bochs driver runs on a
 * QEMU "-vga std" machine and we are root/operator): dumb buffers, mmap of
 * video memory, mode setting, page flips with completion events, and
 * isolation between clients, PRIME (dma-buf fds, import between clients,
 * buffer lifetime).  "test97 -hold N" keeps a test card (four
 * colored quadrants) on the screen for N seconds after the flip, so that a
 * screendump can verify the pixels (see releasetools/x86_qemu.py).
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/ioc_mdrm.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

int max_error = 5;
#include "common.h"

#define DRM_DEV	"/dev/drm0"
#define W	800		/* pitch 3200: not a page multiple */
#define H	600

#define GRAY	0x00808080
#define RED	0x00ff0000
#define GREEN	0x0000ff00
#define BLUE	0x000000ff
#define WHITE	0x00ffffff

static int hold;

static void
test_nommap(void)
{
	void *p;
	int fd;

	subtest = 1;

	if ((fd = open("/dev/null", O_RDWR)) < 0) e(1);
	p = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (p != MAP_FAILED) e(2);
	if (errno != ENODEV) e(3);
	p = mmap(NULL, 4096, PROT_READ, MAP_PRIVATE, fd, 0);
	if (p != MAP_FAILED) e(4);
	if (errno != ENODEV) e(5);
	close(fd);

	/* The fd is still fine and the system did not wedge. */
	if ((fd = open("/dev/zero", O_RDONLY)) < 0) e(6);
	if (mmap(NULL, 4096, PROT_READ, MAP_SHARED, fd, 0) != MAP_FAILED)
		e(7);
	close(fd);
}

static int
drm_open(void)
{
	return open(DRM_DEV, O_RDWR);
}

static uint32_t
create(int fd, uint32_t *pitch, uint64_t *size)
{
	struct mdrm_create_dumb c;

	memset(&c, 0, sizeof(c));
	c.width = W;
	c.height = H;
	c.bpp = 32;
	if (ioctl(fd, MDRM_CREATE_DUMB, &c) != 0) { e(100); return 0; }
	if (c.handle == 0) e(101);
	if (c.pitch != W * 4) e(102);
	if (c.size < (uint64_t)c.pitch * H || c.size % 4096 != 0) e(103);
	*pitch = c.pitch;
	*size = c.size;
	return c.handle;
}

static uint64_t
map_offset(int fd, uint32_t handle)
{
	struct mdrm_map_dumb m;

	memset(&m, 0, sizeof(m));
	m.handle = handle;
	if (ioctl(fd, MDRM_MAP_DUMB, &m) != 0) { e(110); return 0; }
	if (m.offset % 4096 != 0) e(111);
	return m.offset;
}

static void
fill(uint32_t *px, uint32_t pitch, int quadrants)
{
	int x, y;
	uint32_t c;

	for (y = 0; y < H; y++) {
		for (x = 0; x < W; x++) {
			if (!quadrants)
				c = GRAY;
			else if (y < H / 2)
				c = (x < W / 2) ? RED : GREEN;
			else
				c = (x < W / 2) ? BLUE : WHITE;
			px[y * (pitch / 4) + x] = c;
		}
	}
}

static int
readable(int fd, int timeout_ms)
{
	struct pollfd pfd;

	pfd.fd = fd;
	pfd.events = POLLIN;
	pfd.revents = 0;
	if (poll(&pfd, 1, timeout_ms) < 0) return -1;
	return (pfd.revents & POLLIN) != 0;
}

static void
test_drm(void)
{
	struct mdrm_info info;
	struct mdrm_create_dumb c;
	struct mdrm_setcrtc sc;
	struct mdrm_page_flip pf;
	struct mdrm_destroy_dumb d;
	struct mdrm_map_dumb m;
	struct mdrm_event_vblank ev;
	uint32_t ha, hb, pitch;
	uint64_t size, offa, offb;
	uint32_t *pa, *pb;
	char small[16];
	int fd, fd2, i, found, status;
	pid_t pid;

	subtest = 2;

	if ((fd = drm_open()) < 0) {
		/* No device, no driver, or not privileged: nothing to test. */
		if (errno != ENOENT && errno != ENXIO && errno != EACCES &&
		    errno != ENODEV)
			e(1);
		return;
	}

	memset(&info, 0, sizeof(info));
	if (ioctl(fd, MDRM_GETINFO, &info) != 0) e(2);
	if (info.version != MDRM_VERSION) e(3);
	if (strcmp(info.name, "bochs") != 0) e(4);
	if (info.vram_size < 4 * 1024 * 1024) e(5);
	if (info.nmodes == 0 || info.nmodes > MDRM_MAX_MODES) e(6);
	for (found = 0, i = 0; i < (int)info.nmodes; i++)
		if (info.modes[i].width == W && info.modes[i].height == H)
			found = 1;
	if (!found) e(7);
	if (info.cur_width != 0 || info.is_master) e(8);	/* idle */

	/* Bad buffer requests. */
	memset(&c, 0, sizeof(c));
	c.width = W; c.height = H; c.bpp = 16;
	if (ioctl(fd, MDRM_CREATE_DUMB, &c) == 0 || errno != EINVAL) e(9);
	c.bpp = 32; c.flags = 1;
	if (ioctl(fd, MDRM_CREATE_DUMB, &c) == 0 || errno != EINVAL) e(10);

	subtest = 3;
	ha = create(fd, &pitch, &size);
	hb = create(fd, &pitch, &size);
	if (ha == hb) e(1);
	offa = map_offset(fd, ha);
	offb = map_offset(fd, hb);
	if (offa == offb) e(2);
	memset(&m, 0, sizeof(m));
	m.handle = 0xdead;
	if (ioctl(fd, MDRM_MAP_DUMB, &m) == 0 || errno != ENOENT) e(3);

	pa = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, offa);
	if (pa == MAP_FAILED) { e(4); return; }
	pb = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, offb);
	if (pb == MAP_FAILED) { e(5); return; }
	/* New buffers are cleared. */
	if (pa[0] != 0 || pb[(H - 1) * (pitch / 4) + W - 1] != 0) e(6);
	fill(pa, pitch, 0);
	fill(pb, pitch, 1);
	if (pa[pitch / 4 * 10 + 10] != GRAY) e(7);
	if (pb[(H - 1) * (pitch / 4) + W - 1] != WHITE) e(8);

	/* Invalid mappings. */
	if (mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, offa) != MAP_FAILED ||
	    errno != EINVAL) e(9);
	if (mmap(NULL, 4096, PROT_READ, MAP_SHARED, fd, 0) != MAP_FAILED ||
	    errno != EINVAL) e(10);		/* reserved: text console */
	if (mmap(NULL, size + 4096, PROT_READ, MAP_SHARED, fd, offa) !=
	    MAP_FAILED || errno != EINVAL) e(11);
	if (mmap(NULL, 4096, PROT_READ, MAP_SHARED, fd, offa + 100) !=
	    MAP_FAILED || errno != EINVAL) e(12);	/* unaligned */

	/* A second client can neither see nor map our buffers. */
	subtest = 4;
	if ((fd2 = drm_open()) < 0) { e(1); return; }
	memset(&m, 0, sizeof(m));
	m.handle = ha;
	if (ioctl(fd2, MDRM_MAP_DUMB, &m) == 0 || errno != ENOENT) e(2);
	if (mmap(NULL, 4096, PROT_READ, MAP_SHARED, fd2, offa) != MAP_FAILED ||
	    errno != EINVAL) e(3);

	/* Mode setting; the setter becomes master. */
	subtest = 5;
	memset(&sc, 0, sizeof(sc));
	sc.fb = ha; sc.width = W; sc.height = H;
	if (ioctl(fd, MDRM_SETCRTC, &sc) != 0) { e(1); return; }
	if (ioctl(fd, MDRM_GETINFO, &info) != 0) e(2);
	if (info.cur_width != W || info.cur_height != H) e(3);
	if (info.cur_fb != ha || !info.is_master) e(4);
	if (ioctl(fd2, MDRM_SETCRTC, &sc) == 0 || errno != EBUSY) e(5);
	memset(&pf, 0, sizeof(pf));
	pf.fb = hb;
	if (ioctl(fd2, MDRM_PAGE_FLIP, &pf) == 0 || errno != EBUSY) e(6);
	if (close(fd2) != 0) e(7);

	/* Page flip with a completion event. */
	subtest = 6;
	if (fcntl(fd, F_SETFL, O_NONBLOCK) != 0) e(1);
	if (read(fd, &ev, sizeof(ev)) != -1 || errno != EAGAIN) e(2);
	if (readable(fd, 0) != 0) e(3);
	memset(&pf, 0, sizeof(pf));
	pf.fb = hb;
	pf.flags = MDRM_FLIP_EVENT;
	pf.user_data = 0x123456789abcdefULL;
	pf.crtc_id = 42;
	if (ioctl(fd, MDRM_PAGE_FLIP, &pf) != 0) e(4);
	if (ioctl(fd, MDRM_PAGE_FLIP, &pf) == 0 || errno != EBUSY) e(5);
	if (readable(fd, 1000) != 1) e(6);
	if (read(fd, small, sizeof(small)) != -1 || errno != EINVAL) e(7);
	memset(&ev, 0, sizeof(ev));
	if (read(fd, &ev, sizeof(ev)) != sizeof(ev)) e(8);
	if (ev.type != MDRM_EVENT_FLIP_COMPLETE || ev.length != sizeof(ev))
		e(9);
	if (ev.user_data != 0x123456789abcdefULL || ev.crtc_id != 42) e(10);
	{
		/* Timestamps are CLOCK_MONOTONIC (compositors compare). */
		struct timespec now;
		int64_t t, n;

		clock_gettime(CLOCK_MONOTONIC, &now);
		t = (int64_t)ev.tv_sec * 1000000 + ev.tv_usec;
		n = (int64_t)now.tv_sec * 1000000 + now.tv_nsec / 1000;
		if (t > n || n - t > 5000000) e(18);
	}
	if (readable(fd, 0) != 0) e(11);
	if (ioctl(fd, MDRM_GETINFO, &info) != 0 || info.cur_fb != hb) e(12);

	/* Blocking read wakes up on the next event. */
	if (fcntl(fd, F_SETFL, 0) != 0) e(13);
	pf.fb = ha;
	pf.user_data = 7;
	if (ioctl(fd, MDRM_PAGE_FLIP, &pf) != 0) e(14);
	if (read(fd, &ev, sizeof(ev)) != sizeof(ev) || ev.user_data != 7)
		e(15);
	pf.fb = hb;
	pf.flags = 0;
	if (ioctl(fd, MDRM_PAGE_FLIP, &pf) != 0) e(16);	/* no event */
	if (readable(fd, 100) != 0) e(17);

	/* Device mappings are shared with a forked child. */
	subtest = 7;
	if ((pid = fork()) == 0) {
		pb[1] = 0x00abcdef;
		_exit(0);
	}
	if (pid < 0 || waitpid(pid, &status, 0) != pid || status != 0) e(1);
	if (pb[1] != 0x00abcdef) e(2);
	pb[1] = RED;

	if (hold > 0)
		sleep(hold);	/* screendump time: test card on screen */

	/* Buffer lifetime. */
	subtest = 8;
	if (munmap(pa, size) != 0) e(1);
	d.handle = ha;
	if (ioctl(fd, MDRM_DESTROY_DUMB, &d) != 0) e(2);
	if (ioctl(fd, MDRM_DESTROY_DUMB, &d) == 0 || errno != ENOENT) e(3);
	memset(&m, 0, sizeof(m));
	m.handle = ha;
	if (ioctl(fd, MDRM_MAP_DUMB, &m) == 0 || errno != ENOENT) e(4);
	if (mmap(NULL, 4096, PROT_READ, MAP_SHARED, fd, offa) != MAP_FAILED)
		e(5);

	/* Display off again. */
	memset(&sc, 0, sizeof(sc));
	if (ioctl(fd, MDRM_SETCRTC, &sc) != 0) e(6);
	if (ioctl(fd, MDRM_GETINFO, &info) != 0) e(7);
	if (info.cur_width != 0 || info.is_master) e(8);
	if (munmap(pb, size) != 0) e(9);
	if (close(fd) != 0) e(10);

	/* Closing a master device turns the display off and frees it. */
	subtest = 9;
	if ((fd = drm_open()) < 0) { e(1); return; }
	ha = create(fd, &pitch, &size);
	sc.fb = ha; sc.width = W; sc.height = H;
	if (ioctl(fd, MDRM_SETCRTC, &sc) != 0) e(2);
	if (close(fd) != 0) e(3);
	if ((fd = drm_open()) < 0) { e(4); return; }
	if (ioctl(fd, MDRM_GETINFO, &info) != 0) e(5);
	if (info.cur_width != 0 || info.cur_fb != 0) e(6);
	hb = create(fd, &pitch, &size);
	sc.fb = hb;
	if (ioctl(fd, MDRM_SETCRTC, &sc) != 0) e(7);	/* not busy */
	close(fd);
}

/* PRIME: dma-buf fds and buffer import between clients (MDRM_VERSION 2). */
static void
test_prime(void)
{
	struct mdrm_prime pr;
	struct mdrm_info info;
	struct mdrm_create_dumb c;
	struct mdrm_setcrtc sc;
	struct mdrm_destroy_dumb d;
	uint32_t h, h2, pitch, token;
	uint64_t size, off;
	uint32_t *p, *pd, *p2;
	int fd, fd2, df, df2;

	subtest = 10;

	if ((fd = drm_open()) < 0)
		return;		/* no device: test_drm() said so already */
	h = create(fd, &pitch, &size);
	off = map_offset(fd, h);
	p = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, off);
	if (p == MAP_FAILED) { e(1); close(fd); return; }
	p[0] = 0x11223344;

	/* Export: a stable token for the buffer. */
	memset(&pr, 0, sizeof(pr));
	pr.handle = h;
	if (ioctl(fd, MDRM_PRIME_EXPORT, &pr) != 0) e(2);
	token = pr.token;
	if (token == 0 || pr.size != size) e(3);
	if (ioctl(fd, MDRM_PRIME_EXPORT, &pr) != 0 || pr.token != token) e(4);
	pr.handle = 0xdead;
	if (ioctl(fd, MDRM_PRIME_EXPORT, &pr) == 0 || errno != ENOENT) e(5);

	/* A DRM client cannot become a dma-buf fd. */
	memset(&pr, 0, sizeof(pr));
	pr.token = token;
	if (ioctl(fd, MDRM_PRIME_BIND, &pr) == 0 || errno != EBUSY) e(6);

	/* Bind a fresh open: the dma-buf fd maps the buffer at offset 0. */
	if ((df = drm_open()) < 0) { e(7); return; }
	memset(&pr, 0, sizeof(pr));
	pr.token = 0x7fffffff;
	if (ioctl(df, MDRM_PRIME_BIND, &pr) == 0 || errno != ENOENT) e(8);
	pr.token = token;
	if (ioctl(df, MDRM_PRIME_BIND, &pr) != 0 || pr.size != size) e(9);
	memset(&pr, 0, sizeof(pr));
	if (ioctl(df, MDRM_PRIME_INFO, &pr) != 0) e(10);
	if (pr.token != token || pr.size != size) e(11);
	if (ioctl(fd, MDRM_PRIME_INFO, &pr) == 0 || errno != EINVAL) e(12);
	/* ... and does nothing else. */
	if (ioctl(df, MDRM_PRIME_BIND, &pr) == 0 || errno != EINVAL) e(13);
	if (ioctl(df, MDRM_GETINFO, &info) == 0 || errno != EINVAL) e(14);
	memset(&c, 0, sizeof(c));
	c.width = W; c.height = H; c.bpp = 32;
	if (ioctl(df, MDRM_CREATE_DUMB, &c) == 0 || errno != EINVAL) e(15);
	pd = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, df, 0);
	if (pd == MAP_FAILED) { e(16); return; }
	if (pd[0] != 0x11223344) e(17);
	pd[1] = 0x55667788;
	if (p[1] != 0x55667788) e(18);
	if (mmap(NULL, 4096, PROT_READ, MAP_SHARED, df, size) != MAP_FAILED)
		e(19);

	/* Import: another client gets its own handle, the same one each
	 * time; the exporter gets its own handle back. */
	if ((fd2 = drm_open()) < 0) { e(20); return; }
	memset(&pr, 0, sizeof(pr));
	pr.token = token;
	if (ioctl(fd2, MDRM_PRIME_IMPORT, &pr) != 0) e(21);
	h2 = pr.handle;
	if (h2 == 0 || pr.size != size) e(22);
	if (ioctl(fd2, MDRM_PRIME_IMPORT, &pr) != 0 || pr.handle != h2) e(23);
	if (ioctl(fd, MDRM_PRIME_IMPORT, &pr) != 0 || pr.handle != h) e(24);
	if (map_offset(fd2, h2) != off) e(25);
	p2 = mmap(NULL, size, PROT_READ, MAP_SHARED, fd2, off);
	if (p2 == MAP_FAILED || p2[1] != 0x55667788) e(26);

	/* A duplicated handle keeps the buffer when the original goes, and
	 * import still finds a handle of this client for it. */
	memset(&pr, 0, sizeof(pr));
	pr.handle = h2;
	if (ioctl(fd2, MDRM_DUP_HANDLE, &pr) != 0) e(41);
	if (pr.handle == 0 || pr.handle == h2) e(42);
	d.handle = h2;
	if (ioctl(fd2, MDRM_DESTROY_DUMB, &d) != 0) e(43);
	h2 = pr.handle;
	if (map_offset(fd2, h2) != off) e(44);
	pr.handle = 0xdead;
	if (ioctl(fd2, MDRM_DUP_HANDLE, &pr) == 0 || errno != ENOENT) e(45);
	memset(&pr, 0, sizeof(pr));
	pr.token = token;
	if (ioctl(fd2, MDRM_PRIME_IMPORT, &pr) != 0 || pr.handle != h2) e(46);

	/* The importer can scan the buffer out. */
	memset(&sc, 0, sizeof(sc));
	sc.fb = h2; sc.width = W; sc.height = H;
	if (ioctl(fd2, MDRM_SETCRTC, &sc) != 0) e(27);
	if (ioctl(fd2, MDRM_GETINFO, &info) != 0 || info.cur_fb != h2) e(28);
	if (ioctl(fd, MDRM_GETINFO, &info) != 0 || info.cur_fb != h) e(29);
	memset(&sc, 0, sizeof(sc));
	if (ioctl(fd2, MDRM_SETCRTC, &sc) != 0) e(30);

	/* Lifetime: the buffer lives while any handle or dma-buf fd does. */
	d.handle = h;
	if (ioctl(fd, MDRM_DESTROY_DUMB, &d) != 0) e(31);
	if (close(fd) != 0) e(32);
	if (close(fd2) != 0) e(33);
	if (pd[0] != 0x11223344) e(34);
	if ((df2 = drm_open()) < 0) { e(35); return; }
	memset(&pr, 0, sizeof(pr));
	pr.token = token;
	if (ioctl(df2, MDRM_PRIME_BIND, &pr) != 0) e(36);	/* still exported */
	if (close(df) != 0) e(37);
	if (close(df2) != 0) e(38);
	if ((fd = drm_open()) < 0) { e(39); return; }
	memset(&pr, 0, sizeof(pr));
	pr.token = token;
	if (ioctl(fd, MDRM_PRIME_IMPORT, &pr) == 0 || errno != ENOENT) e(40);
	close(fd);
	munmap(p, size);
	munmap(pd, size);
	if (p2 != MAP_FAILED) munmap(p2, size);
}

int
main(int argc, char **argv)
{
	if (argc == 3 && strcmp(argv[1], "-hold") == 0)
		hold = atoi(argv[2]);

	start(97);

	test_nommap();
	test_drm();
	test_prime();

	quit();
	return 0;
}
