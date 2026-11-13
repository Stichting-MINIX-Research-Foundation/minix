/* Exercises fs_slink/fs_rdlink (untested by the main harness): both the fast
 * inline symlink (target in di_db) and the slow symlink (target in a data
 * block, target length >= fs_maxsymlinklen, typically 120 for UFS2). */
#include <minixshim.h>
#include <sys/statvfs.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

int g_fd;
int  fs_mount(dev_t, unsigned int, struct fsdriver_node *, unsigned int *);
void fs_unmount(void);
int  fs_lookup(ino_t, char *, struct fsdriver_node *, int *);
int  fs_slink(ino_t, char *, uid_t, gid_t, struct fsdriver_data *, size_t);
ssize_t fs_rdlink(ino_t, struct fsdriver_data *, size_t);
int  fs_putnode(ino_t, unsigned int);
void fs_sync(void);
void init_inode_cache(void);

#define DEV 1
static int pass = 0, fail = 0;
static void check(const char *what, int ok) {
	printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
	if (ok) pass++; else fail++;
}

static void one(ino_t ROOT, const char *name, const char *target) {
	struct fsdriver_node n; int r, mp;
	size_t tlen = strlen(target);
	{ struct fsdriver_data d = { (char*)target, tlen };
	  r = fs_slink(ROOT, (char*)name, 0, 0, &d, tlen); }
	check("slink OK", r == OK);
	fs_sync();

	r = fs_lookup(ROOT, (char*)name, &n, &mp);
	check("lookup symlink OK", r == OK);
	check("symlink size == target len", n.fn_size == (off_t)tlen);
	check("mode is symlink", (n.fn_mode & I_TYPE) == I_SYMBOLIC_LINK);

	char rb[1024]; memset(rb, 0, sizeof(rb));
	{ struct fsdriver_data d = { rb, sizeof(rb) };
	  ssize_t got = fs_rdlink(n.fn_ino_nr, &d, sizeof(rb));
	  check("rdlink length matches", got == (ssize_t)tlen);
	  check("rdlink target matches", memcmp(rb, target, tlen) == 0);
	}
	fs_putnode(n.fn_ino_nr, 1);
}

int main(int argc, char **argv) {
	struct fsdriver_node root; unsigned int rf; int r;
	if (argc != 2) { fprintf(stderr, "usage: %s image\n", argv[0]); return 2; }
	if ((g_fd = open(argv[1], O_RDWR)) < 0) { perror("open"); return 2; }
	init_inode_cache();
	r = fs_mount(DEV, 0, &root, &rf);
	check("fs_mount OK", r == OK);
	ino_t ROOT = root.fn_ino_nr;

	printf("== fast (inline) symlink ==\n");
	one(ROOT, "fast", "hello.txt");

	printf("== slow (data-block) symlink, 200-byte target ==\n");
	char big[201]; for (int i = 0; i < 200; i++) big[i] = 'a' + (i % 26); big[200] = 0;
	one(ROOT, "slow", big);

	fs_unmount();
	printf("==== slink: %d passed, %d failed ====\n", pass, fail);
	return fail ? 1 : 0;
}
