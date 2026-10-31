/* ftruncate-grow edge cases: growing a file must leave the new region reading
 * as zero, including growth that stays within the last (fragment) block and
 * growth that enlarges the trailing fragment. bsize=32768 fsize=4096. */
#include <minixshim.h>
#include <sys/statvfs.h>
#include <unistd.h>
#include <fcntl.h>

int g_fd;
int  fs_mount(dev_t, unsigned int, struct fsdriver_node *, unsigned int *);
void fs_unmount(void);
int  fs_lookup(ino_t, char *, struct fsdriver_node *, int *);
int  fs_create(ino_t, char *, mode_t, uid_t, gid_t, struct fsdriver_node *);
ssize_t fs_readwrite(ino_t, struct fsdriver_data *, size_t, off_t, int);
int  fs_trunc(ino_t, off_t, off_t);
int  fs_putnode(ino_t, unsigned int);
void fs_sync(void);
void init_inode_cache(void);

#define DEV 1
static int pass = 0, fail = 0;
static void check(const char *what, int ok) {
	printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
	if (ok) pass++; else fail++;
}

static int region_zero(ino_t ino, off_t from, off_t to) {
	static char rb[65536];
	off_t p = from;
	while (p < to) {
		size_t n = (size_t)((to - p) > (off_t)sizeof(rb) ? (off_t)sizeof(rb) : (to - p));
		struct fsdriver_data d = { rb, n };
		ssize_t got = fs_readwrite(ino, &d, n, p, FSC_READ);
		if (got != (ssize_t)n) return 0;
		for (size_t i = 0; i < n; i++) if (rb[i]) return 0;
		p += n;
	}
	return 1;
}

int main(int argc, char **argv) {
	struct fsdriver_node root, n; unsigned int rf; int r, mp; ssize_t w;
	if (argc != 2) { fprintf(stderr, "usage: %s image\n", argv[0]); return 2; }
	if ((g_fd = open(argv[1], O_RDWR)) < 0) { perror("open"); return 2; }
	init_inode_cache();
	r = fs_mount(DEV, 0, &root, &rf);
	check("fs_mount OK", r == OK);
	ino_t ROOT = root.fn_ino_nr;

	r = fs_create(ROOT, "f", I_REGULAR | 0644, 0, 0, &n);
	check("create OK", r == OK);
	ino_t ino = n.fn_ino_nr;

	/* Write 100 bytes of 0xFF -> one 4096-byte fragment allocated. */
	static char ff[100]; memset(ff, 0xFF, sizeof(ff));
	{ struct fsdriver_data d = { ff, sizeof(ff) };
	  w = fs_readwrite(ino, &d, sizeof(ff), 0, FSC_WRITE); }
	check("write 100 bytes", w == 100);

	/* Grow within the same fragment (100 -> 3000). Gap must be zero. */
	r = fs_trunc(ino, 3000, 0);
	check("grow to 3000 (same frag)", r == OK);
	check("gap [100,3000) reads zero", region_zero(ino, 100, 3000));

	/* Grow enlarging the trailing fragment (3000 -> 10000, still block 0,
	 * fragment 4096 -> 12288). Gap must be zero. */
	r = fs_trunc(ino, 10000, 0);
	check("grow to 10000 (enlarge frag)", r == OK);
	check("gap [100,10000) reads zero", region_zero(ino, 100, 10000));

	/* Grow into a later block (10000 -> 100000) — sparse hole, reads zero. */
	r = fs_trunc(ino, 100000, 0);
	check("grow to 100000 (new blocks)", r == OK);
	check("gap [100,100000) reads zero", region_zero(ino, 100, 100000));

	/* The original 100 bytes of 0xFF must be untouched. */
	{ static char rb[100]; struct fsdriver_data d = { rb, sizeof(rb) };
	  ssize_t got = fs_readwrite(ino, &d, sizeof(rb), 0, FSC_READ);
	  int ok = (got == 100); for (int i=0;i<100;i++) if ((unsigned char)rb[i]!=0xFF) ok=0;
	  check("original 100 bytes intact", ok);
	}

	fs_putnode(ino, 1);
	fs_unmount();
	printf("==== truncgrow: %d passed, %d failed ====\n", pass, fail);
	return fail ? 1 : 0;
}
