/* Drives the real FFS server handlers over the shim to exercise the write
 * path against a UFS2 image and verify results. */
#include <minixshim.h>
#include <sys/statvfs.h>
#include <unistd.h>
#include <fcntl.h>

int g_fd;

/* handlers from the server source files */
int  fs_mount(dev_t, unsigned int, struct fsdriver_node *, unsigned int *);
void fs_unmount(void);
int  fs_lookup(ino_t, char *, struct fsdriver_node *, int *);
int  fs_create(ino_t, char *, mode_t, uid_t, gid_t, struct fsdriver_node *);
int  fs_mkdir(ino_t, char *, mode_t, uid_t, gid_t);
ssize_t fs_readwrite(ino_t, struct fsdriver_data *, size_t, off_t, int);
ssize_t fs_getdents(ino_t, struct fsdriver_data *, size_t, off_t *);
int  fs_unlink(ino_t, char *, int);
int  fs_putnode(ino_t, unsigned int);
int  fs_statvfs(struct statvfs *);
void fs_sync(void);
void init_inode_cache(void);

extern ino_t h_dent_ino[]; extern char h_dent_name[][256];
extern unsigned int h_dent_type[]; extern int h_dent_count;

#define DEV 1
static int pass = 0, fail = 0;
static void check(const char *what, int ok) {
	printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
	if (ok) pass++; else fail++;
}

static int list_dir(ino_t ino, const char *label) {
	struct fsdriver_data d; char ubuf[8192]; off_t pos = 0;
	d.ptr = ubuf; d.len = sizeof(ubuf);
	h_dent_count = 0;
	(void)fs_getdents(ino, &d, sizeof(ubuf), &pos);
	printf("  %s (ino %llu): ", label, (unsigned long long)ino);
	for (int i = 0; i < h_dent_count; i++)
		printf("%s%s", h_dent_name[i], i+1<h_dent_count? ", ":"\n");
	if (h_dent_count == 0) printf("(empty)\n");
	return h_dent_count;
}
static int dir_has(const char *name) {
	for (int i = 0; i < h_dent_count; i++)
		if (!strcmp(h_dent_name[i], name)) return 1;
	return 0;
}

int main(int argc, char **argv) {
	struct fsdriver_node root, n, dn;
	unsigned int rf; int mp, r; ssize_t w;
	struct statvfs sv;

	if (argc != 2) { fprintf(stderr, "usage: %s image\n", argv[0]); return 2; }
	if ((g_fd = open(argv[1], O_RDWR)) < 0) { perror("open"); return 2; }

	init_inode_cache();

	printf("== mount (read-write) ==\n");
	r = fs_mount(DEV, 0, &root, &rf);
	check("fs_mount returns OK", r == OK);
	check("root is a directory", (root.fn_mode & I_TYPE) == I_DIRECTORY);
	ino_t ROOT = root.fn_ino_nr;

	printf("== initial root listing ==\n");
	list_dir(ROOT, "root");
	check("root has . and ..", h_dent_count == 2);

	fs_statvfs(&sv);
	uint64_t bfree0 = sv.f_bfree, ffree0 = sv.f_ffree;
	printf("  free: blocks=%llu inodes=%llu\n",
		(unsigned long long)bfree0, (unsigned long long)ffree0);

	printf("== create + write a file ==\n");
	const char *msg = "data written by the FFS server write path\n";
	size_t mlen = strlen(msg);
	r = fs_create(ROOT, "newfile", I_REGULAR | 0644, 0, 0, &n);
	check("fs_create newfile OK", r == OK);
	ino_t fino = n.fn_ino_nr;
	{ struct fsdriver_data d = { (char*)msg, mlen };
	  w = fs_readwrite(fino, &d, mlen, 0, FSC_WRITE); }
	check("write returns full length", w == (ssize_t)mlen);
	fs_putnode(fino, 1);

	printf("== make a dir + nested file, write a larger (multi-frag) file ==\n");
	r = fs_mkdir(ROOT, "newdir", I_DIRECTORY | 0755, 0, 0);
	check("fs_mkdir newdir OK", r == OK);
	r = fs_lookup(ROOT, "newdir", &dn, &mp);
	check("lookup newdir OK", r == OK);
	ino_t dino = dn.fn_ino_nr;
	r = fs_create(dino, "deep.bin", I_REGULAR | 0644, 0, 0, &n);
	check("create newdir/deep.bin OK", r == OK);
	ino_t deepino = n.fn_ino_nr;
	/* Write 20000 bytes -> exercises multiple fragments / a full block. */
	static char big[20000]; for (int i = 0; i < (int)sizeof(big); i++) big[i] = (char)(i*7+1);
	{ struct fsdriver_data d = { big, sizeof(big) };
	  w = fs_readwrite(deepino, &d, sizeof(big), 0, FSC_WRITE); }
	check("large write full length", w == (ssize_t)sizeof(big));
	fs_putnode(deepino, 1);
	fs_putnode(dino, 1);

	fs_sync();

	printf("== verify reads ==\n");
	list_dir(ROOT, "root");
	check("root now has newfile", dir_has("newfile"));
	check("root now has newdir", dir_has("newdir"));

	r = fs_lookup(ROOT, "newfile", &n, &mp);
	check("lookup newfile OK", r == OK);
	check("newfile size correct", n.fn_size == (off_t)mlen);
	{ char rb[256] = {0}; struct fsdriver_data d = { rb, sizeof(rb) };
	  ssize_t got = fs_readwrite(n.fn_ino_nr, &d, mlen, 0, FSC_READ);
	  check("read length matches", got == (ssize_t)mlen);
	  check("read content matches written", memcmp(rb, msg, mlen) == 0);
	}
	fs_putnode(n.fn_ino_nr, 1);

	r = fs_lookup(ROOT, "newdir", &dn, &mp);
	fs_lookup(dino = dn.fn_ino_nr, "deep.bin", &n, &mp);
	{ static char rb[20000]; struct fsdriver_data d = { rb, sizeof(rb) };
	  ssize_t got = fs_readwrite(n.fn_ino_nr, &d, sizeof(rb), 0, FSC_READ);
	  check("deep.bin read length matches", got == (ssize_t)sizeof(big));
	  check("deep.bin content matches (multi-frag)", memcmp(rb, big, sizeof(big)) == 0);
	}
	fs_putnode(n.fn_ino_nr, 1);
	list_dir(dn.fn_ino_nr, "newdir");
	check("newdir has . .. deep.bin (3 entries)", h_dent_count == 3);
	fs_putnode(dn.fn_ino_nr, 1);

	fs_statvfs(&sv);
	printf("  free after writes: blocks=%llu inodes=%llu (was %llu/%llu)\n",
		(unsigned long long)sv.f_bfree, (unsigned long long)sv.f_ffree,
		(unsigned long long)bfree0, (unsigned long long)ffree0);
	check("free inodes decreased by >=2", ffree0 - sv.f_ffree >= 2);
	check("free blocks decreased", sv.f_bfree < bfree0);

	printf("== delete: unlink file, remove nested file + rmdir ==\n");
	r = fs_unlink(ROOT, "newfile", FSC_UNLINK);
	check("unlink newfile OK", r == OK);
	r = fs_lookup(ROOT, "newdir", &dn, &mp);
	r = fs_unlink(dn.fn_ino_nr, "deep.bin", FSC_UNLINK);
	check("unlink newdir/deep.bin OK", r == OK);
	fs_putnode(dn.fn_ino_nr, 1);
	r = fs_unlink(ROOT, "newdir", FSC_RMDIR);
	check("rmdir newdir OK", r == OK);

	fs_sync();
	list_dir(ROOT, "root");
	check("newfile gone", !dir_has("newfile"));
	check("newdir gone", !dir_has("newdir"));
	check("root back to . and .. only", h_dent_count == 2);

	fs_statvfs(&sv);
	printf("  free after deletes: blocks=%llu inodes=%llu\n",
		(unsigned long long)sv.f_bfree, (unsigned long long)sv.f_ffree);
	check("inodes reclaimed back to start", sv.f_ffree == ffree0);
	check("blocks reclaimed back to start", sv.f_bfree == bfree0);

	printf("== unmount ==\n");
	fs_unmount();

	printf("\n==== %d passed, %d failed ====\n", pass, fail);
	return fail ? 1 : 0;
}
