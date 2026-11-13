/* Exercises the indirect-block read/write/truncate path, which the main
 * harness (max 20000-byte writes) never reaches: with bsize=32768 and
 * UFS_NDADDR=12, files only need indirect blocks past 393216 bytes. */
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
int  fs_unlink(ino_t, char *, int);
int  fs_putnode(ino_t, unsigned int);
int  fs_statvfs(struct statvfs *);
void fs_sync(void);
void init_inode_cache(void);

#define DEV 1
static int pass = 0, fail = 0;
static void check(const char *what, int ok) {
	printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
	if (ok) pass++; else fail++;
}

static unsigned char byteval(size_t i) { return (unsigned char)((i * 1103515245u + 12345u) >> 16); }

int main(int argc, char **argv) {
	struct fsdriver_node root, n;
	unsigned int rf; int r, mp; ssize_t w;
	struct statvfs sv;

	if (argc != 2) { fprintf(stderr, "usage: %s image\n", argv[0]); return 2; }
	if ((g_fd = open(argv[1], O_RDWR)) < 0) { perror("open"); return 2; }
	init_inode_cache();

	r = fs_mount(DEV, 0, &root, &rf);
	check("fs_mount OK", r == OK);
	ino_t ROOT = root.fn_ino_nr;

	fs_statvfs(&sv);
	uint64_t bfree0 = sv.f_bfree;

	/* 2 MiB: spans all 12 direct blocks and well into the single-indirect. */
	const size_t SZ = 2u * 1024 * 1024;
	static unsigned char *buf, *rb;
	buf = malloc(SZ); rb = malloc(SZ);
	for (size_t i = 0; i < SZ; i++) buf[i] = byteval(i);

	r = fs_create(ROOT, "big", I_REGULAR | 0644, 0, 0, &n);
	check("create big OK", r == OK);
	ino_t ino = n.fn_ino_nr;

	{ struct fsdriver_data d = { (char*)buf, SZ };
	  w = fs_readwrite(ino, &d, SZ, 0, FSC_WRITE); }
	check("indirect write full length", w == (ssize_t)SZ);

	fs_putnode(ino, 1);
	fs_sync();

	/* Re-lookup and read the whole thing back in one call. */
	r = fs_lookup(ROOT, "big", &n, &mp);
	check("lookup big OK", r == OK);
	check("size == 2 MiB", n.fn_size == (off_t)SZ);
	ino = n.fn_ino_nr;

	memset(rb, 0, SZ);
	{ struct fsdriver_data d = { (char*)rb, SZ };
	  ssize_t got = fs_readwrite(ino, &d, SZ, 0, FSC_READ);
	  check("read back full length", got == (ssize_t)SZ);
	  check("content matches (direct+indirect)", memcmp(rb, buf, SZ) == 0);
	}

	/* Read a chunk straddling the direct->indirect boundary (block 12). */
	memset(rb, 0, 65536);
	{ off_t at = 12 * 32768 - 1000;
	  struct fsdriver_data d = { (char*)rb, 65536 };
	  ssize_t got = fs_readwrite(ino, &d, 65536, at, FSC_READ);
	  check("boundary read length", got == 65536);
	  check("boundary content matches", memcmp(rb, buf + at, 65536) == 0);
	}

	/* Truncate back to a small size: must free the whole indirect tree. */
	r = fs_trunc(ino, 5000, 0);
	check("truncate to 5000 OK", r == OK);
	fs_putnode(ino, 1);
	fs_sync();

	r = fs_lookup(ROOT, "big", &n, &mp);
	check("size now 5000", n.fn_size == 5000);
	{ struct fsdriver_data d = { (char*)rb, 5000 };
	  ssize_t got = fs_readwrite(n.fn_ino_nr, &d, 5000, 0, FSC_READ);
	  check("post-trunc read length", got == 5000);
	  check("post-trunc content intact", memcmp(rb, buf, 5000) == 0);
	}

	/* Remove it; all blocks must come back. */
	r = fs_unlink(ROOT, "big", FSC_UNLINK);
	check("unlink big OK", r == OK);
	fs_putnode(n.fn_ino_nr, 1);
	fs_sync();

	fs_statvfs(&sv);
	printf("  free blocks: start=%llu end=%llu\n",
		(unsigned long long)bfree0, (unsigned long long)sv.f_bfree);
	check("all blocks reclaimed after indirect-file delete", sv.f_bfree == bfree0);

	fs_unmount();
	printf("==== bigfile: %d passed, %d failed ====\n", pass, fail);
	return fail ? 1 : 0;
}
