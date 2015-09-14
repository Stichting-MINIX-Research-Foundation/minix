#ifndef _MINLIB
#define _MINLIB

#include <sys/mount.h>

/* Miscellaneous BSD. */
char *itoa(int _n);

/* Miscellaneous MINIX. */
void std_err(const char *_s);
void prints(const char *_s, ...);
int fsversion(char *_dev, char *_prog);
int getprocessor(void);
void _cpuid(u32_t *eax, u32_t *ebx, u32_t *ecx, u32_t *edx);
int load_mtab(char *_prog_name);
int get_mtab_entry(char dev[PATH_MAX], char mount_point[PATH_MAX],
			char type[MNTNAMELEN], char flags[MNTFLAGLEN]);
int servxcheck(unsigned long peer, const char *service,
	void (*logf)(int pass, const char *name));
const char *servxfile(const char *file);

/* read_tsc() and friends */
void read_tsc(u32_t *hi, u32_t *lo);
void read_tsc_64(u64_t *t);

/* return values for fsversion */
#define FSVERSION_MFS1		0x00001
#define FSVERSION_MFS2		0x00002
#define FSVERSION_MFS3		0x00003
#define FSVERSION_EXT2		0x10002
#define FSVERSION_ISO9660	0x20001

#endif
