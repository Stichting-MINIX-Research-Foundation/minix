/* Test for getdents().
 * Just tests whatever FS the test is executed from.
 *
 * This test creates lots of nodes of various types, verifies that readdir()
 * (and so getdents()) returns all of them exactly once, and nothing else 
 * (except for "." and ".."), and verifies the struct dirents are correct.
 */

#include <sys/types.h>
#include <sys/stat.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>

#include "common.h"

#define FILETYPES	6

#define LOOPS	30

struct filetype {
	char *fnbase;	/* type-unique base name */
	int dt;		/* dirent type that getdents() should return */
	mode_t modebit;	/* mode bit unique to this type */
} filetypes[FILETYPES] = {
	{ "fifo",	DT_FIFO, S_IFIFO },
	{ "chr",	DT_CHR,  S_IFCHR },
	{ "dir",	DT_DIR,  S_IFDIR },
	{ "blk",	DT_BLK,	 S_IFBLK },
	{ "reg",	DT_REG,  S_IFREG },
	{ "lnk",	DT_LNK,  S_IFLNK },
};

int seen[FILETYPES][LOOPS];

/* create a node named 'f' using code 'call'. check it doesn't exist
 * before hand, and does exist afterwards, and has the expected modebit.
 */

#define CR(f, call, modebit) do { 			\
	struct stat sb;					\
	if(lstat(f, &sb) >= 0) { e(1); }		\
	if((call) < 0) { e(2); } 			\
	if(lstat(f, &sb) < 0) { e(3); }			\
	if(!(sb.st_mode & (modebit))) { e(4); }		\
} while(0)

int
main(int argc, char **argv)
{
	int i, t;
	DIR *dir;
	struct dirent *de;

	start(78);

	/* create contents */

	for(i = 0; i < LOOPS; i++) {
		for(t = 0; t < FILETYPES; t++) {
			int c;
			char fn[2000];
			mode_t m = filetypes[t].modebit;

			/* think of a filename; do varying lengths to check
			 * dirent record length alignment issues
			 */

			snprintf(fn, sizeof(fn), "%d.%d.%d.", t, filetypes[t].dt, i);
			for(c = 0; c < i; c++) strcat(fn, "x");
			
			/* create the right type */

			switch(filetypes[t].dt) {
			  case DT_FIFO: CR(fn, mknod(fn, 0600|S_IFIFO, 0), m); break;
			  case DT_CHR:  CR(fn, mknod(fn, 0600|S_IFCHR, 0), m); break;
			  case DT_BLK:  CR(fn, mknod(fn, 0600|S_IFBLK, 0), m); break;
			  case DT_REG:  CR(fn, mknod(fn, 0600|S_IFREG, 0), m); break;
			  case DT_DIR:  CR(fn, mkdir(fn, 0600), m); break;
			  case DT_LNK:  CR(fn, symlink("dest", fn), m); break;
			  default: e(10); break;
			}
		}
	}

	/* Verify that readdir() returns dirent structs with reasonable contents */

	if(!(dir = opendir("."))) { e(20); return 1; }

	while((de = readdir(dir))) {
		int dt;
		struct stat sb;

		if(!strcmp(de->d_name, ".")) continue;
		if(!strcmp(de->d_name, "..")) continue;

		if(sscanf(de->d_name, "%d.%d.%d", &t, &dt, &i) != 3) {
			e(30);
			continue;
		}

		/* sanity check on filename numbers */

		if(t < 0 || dt < 0 || i < 0) { e(31); continue; }
		if(t >= FILETYPES || i >= LOOPS) { e(32); continue; }
		if(seen[t][i]) { e(33); continue; }
		seen[t][i] = 1;
		if(filetypes[t].dt != dt) { e(34); continue; }
		if(lstat(de->d_name, &sb) < 0) { e(35); continue; }
		if(!(sb.st_mode & filetypes[t].modebit)) { e(36); continue; }

		/* Now we know this file is ours and has the expected type;
		 * now we can verify the contents of the dirent struct. d_name
		 * is OK because sscanf and stat worked on it.
		 */

		if(de->d_type != dt) { e(37); }
		if(de->d_fileno != sb.st_ino) { e(38); }
		if(de->d_namlen != strlen(de->d_name)) { e(39); }
		if(de->d_reclen != _DIRENT_RECLEN(de, de->d_namlen)) { e(40); }
	}

	if(closedir(dir) < 0) e(50);

	/* Verify that we have seen all files we expected to see. */

	for(i = 0; i < LOOPS; i++) {
		for(t = 0; t < FILETYPES; t++) {
			if(!seen[t][i]) { e(60); break; }
		}
	}

	quit();
}
