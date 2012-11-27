#include <sys/mount.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#define MAX_ERROR 0
#include "common.c"

#define TESTMNT		"testmnt"
#define TESTFILE	"test.txt"
#define TESTSTRING	"foobar"
#define RAMDISK		"/dev/ram5"
#define RAMDISK_SIZE	"2048"
#define SILENT		" > /dev/null 2>&1"

void basic_test(void);
void bomb(char const *msg);
void skip(char const *msg);
void create_partition(void);
void verify_tools(void);

void
basic_test(void)
{
/* Write a string to a file, read it back, and confirm it's identical */
	int status;
	char cmd_buf[1024];
	char file_buf[sizeof(TESTSTRING)*10];
	int fd;

	subtest = 3;

	/* Write test string to test file */
	snprintf(cmd_buf, sizeof(cmd_buf), "echo -n %s > %s/%s\n",
		TESTSTRING, TESTMNT, TESTFILE);
	status = system(cmd_buf);
	if (WEXITSTATUS(status) != 0)
		bomb("Unable to echo string to file");

	/* Flush to disk and unmount, remount */
	system("sync");
	system("umount " RAMDISK SILENT);
	snprintf(cmd_buf, sizeof(cmd_buf), "mount -t ntfs-3g %s %s %s",
		RAMDISK, TESTMNT, SILENT);
	status = system(cmd_buf);
	if (WEXITSTATUS(status != 0))
		bomb("Unable to mount NTFS partition (1)");

	/* Open file and verify contents */
	if ((fd = open(TESTMNT "/" TESTFILE, O_RDONLY)) < 0) e(1);
	if (read(fd, file_buf, sizeof(file_buf)) != strlen(TESTSTRING)) e(2);
	(void) close(fd);
	system("umount " RAMDISK SILENT);
	if (strncmp(file_buf, TESTSTRING, strlen(TESTSTRING))) e(3);
}

void
skip(char const *msg)
{
	system("umount " RAMDISK SILENT);
	printf("%s\n", msg);
	quit();
}

void
bomb(char const *msg)
{
	system("umount " RAMDISK SILENT);
	printf("%s\n", msg);
	e(99);
	quit();
}

void
create_partition(void)
{
	int status;
	char mntcmd[1024];

	subtest = 1;

	if (getuid() != 0 && setuid(0) != 0) e(1);
	status = system("ramdisk " RAMDISK_SIZE " " RAMDISK SILENT);
	if (WEXITSTATUS(status) != 0)
		bomb("Unable to create ramdisk");

	status = system("mkntfs " RAMDISK SILENT);
	if (WEXITSTATUS(status) != 0)
		bomb("Unable to create NTFS file system on " RAMDISK);

	if (mkdir(TESTMNT, 0755) != 0)
		bomb("Unable to create directory for mounting");

	snprintf(mntcmd, sizeof(mntcmd), "mount -t ntfs-3g %s %s %s",
		RAMDISK, TESTMNT, SILENT);
	status = system(mntcmd);
	if (WEXITSTATUS(status != 0))
		bomb("Unable to mount NTFS partition (1)");
}

void
verify_tools(void)
{
	int status;

	subtest = 1;
	status = system("which mkntfs > /dev/null 2>&1");
	if (WEXITSTATUS(status) != 0) {
		skip("mkntfs not found. Please install ntfsprogs (pkgin in "
			"ntfsprogs)");
	}
	status = system("which ntfs-3g > /dev/null 2>&1");
	if (WEXITSTATUS(status) != 0) {
		skip("ntfs-3g not found. Please install fuse-ntfs-3g-1.1120 "
			"(pkgin in fuse-ntfs-3g-1.1120)");
	}
}

int
main(int argc, char *argv[])
{
	start(65);
	verify_tools();
	create_partition();
	basic_test();
	quit();
	return(-1);	/* Unreachable */
}

