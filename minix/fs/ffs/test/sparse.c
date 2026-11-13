/* Reaches the double- and triple-indirect allocation/free recursion using
 * sparse writes at high offsets: only a handful of real blocks are used, so a
 * small image suffices, but balloc_indir(num=2/3) and indir_free(level=1/2)
 * are exercised.  bsize=32768 => nindir=4096. */
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
#define BS 32768
#define NIND 4096
static int pass = 0, fail = 0;
static void check(const char *what, int ok) {
	printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
	if (ok) pass++; else fail++;
}

/* first logical block of each indirect tier */
#define LBN_SINGLE 12
#define LBN_DOUBLE (12 + NIND)
#define LBN_TRIPLE (12 + NIND + (off_t)NIND*NIND)

static int test_offset(ino_t ROOT, const char *name, off_t lbn, uint64_t bfree0) {
	struct fsdriver_node n; int r, mp; ssize_t w;
	struct statvfs sv;
	off_t at = lbn * (off_t)BS + 777;
	char wbuf[4096], rbuf[4096];
	for (int i = 0; i < (int)sizeof(wbuf); i++) wbuf[i] = (char)(i ^ (int)lbn);

	r = fs_create(ROOT, (char*)name, I_REGULAR | 0644, 0, 0, &n);
	check("create OK", r == OK);
	ino_t ino = n.fn_ino_nr;

	{ struct fsdriver_data d = { wbuf, sizeof(wbuf) };
	  w = fs_readwrite(ino, &d, sizeof(wbuf), at, FSC_WRITE); }
	check("sparse write full length", w == (ssize_t)sizeof(wbuf));
	fs_putnode(ino, 1);
	fs_sync();

	r = fs_lookup(ROOT, (char*)name, &n, &mp);
	check("lookup OK", r == OK);
	check("size == off+len", n.fn_size == at + (off_t)sizeof(wbuf));
	ino = n.fn_ino_nr;

	/* A read from a hole (offset 0) must be zeros. */
	memset(rbuf, 0xAA, sizeof(rbuf));
	{ struct fsdriver_data d = { rbuf, sizeof(rbuf) };
	  ssize_t got = fs_readwrite(ino, &d, sizeof(rbuf), 0, FSC_READ);
	  int zero = 1; for (size_t i=0;i<sizeof(rbuf);i++) if (rbuf[i]) zero=0;
	  check("hole reads as zero", got == (ssize_t)sizeof(rbuf) && zero);
	}
	/* The written data must read back intact. */
	memset(rbuf, 0, sizeof(rbuf));
	{ struct fsdriver_data d = { rbuf, sizeof(rbuf) };
	  ssize_t got = fs_readwrite(ino, &d, sizeof(rbuf), at, FSC_READ);
	  check("sparse data read back", got == (ssize_t)sizeof(rbuf) &&
		memcmp(rbuf, wbuf, sizeof(wbuf)) == 0);
	}

	fs_putnode(ino, 1);
	r = fs_unlink(ROOT, (char*)name, FSC_UNLINK);
	check("unlink OK", r == OK);
	fs_sync();

	fs_statvfs(&sv);
	check("blocks fully reclaimed (indirect tree freed)", sv.f_bfree == bfree0);
	printf("  [%s] start=%llu end=%llu\n", name,
		(unsigned long long)bfree0, (unsigned long long)sv.f_bfree);
	return 0;
}

int main(int argc, char **argv) {
	struct fsdriver_node root; unsigned int rf; int r;
	struct statvfs sv;
	if (argc != 2) { fprintf(stderr, "usage: %s image\n", argv[0]); return 2; }
	if ((g_fd = open(argv[1], O_RDWR)) < 0) { perror("open"); return 2; }
	init_inode_cache();
	r = fs_mount(DEV, 0, &root, &rf);
	check("fs_mount OK", r == OK);
	ino_t ROOT = root.fn_ino_nr;
	fs_statvfs(&sv);
	uint64_t bfree0 = sv.f_bfree;

	printf("== single indirect ==\n");  test_offset(ROOT, "s", LBN_SINGLE, bfree0);
	printf("== double indirect ==\n");  test_offset(ROOT, "d", LBN_DOUBLE, bfree0);
	printf("== triple indirect ==\n");  test_offset(ROOT, "t", LBN_TRIPLE, bfree0);

	fs_unmount();
	printf("==== sparse: %d passed, %d failed ====\n", pass, fail);
	return fail ? 1 : 0;
}
