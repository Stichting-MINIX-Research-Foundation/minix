#ifndef _MINLIB
#define _MINLIB

/* Miscellaneous BSD. */
char *itoa(int _n);
#ifndef __NBSD_LIBC
char *getpass(const char *_prompt);
#ifdef __ACK__
void swab(char *_from, char *_to, int _count);
#endif
#endif /* !_NBSD_LIBC */

/* Miscellaneous MINIX. */
void std_err(const char *_s);
void prints(const char *_s, ...);
int fsversion(char *_dev, char *_prog);
int getprocessor(void);
void _cpuid(u32_t *eax, u32_t *ebx, u32_t *ecx, u32_t *edx);
int load_mtab(char *_prog_name);
int rewrite_mtab(char *_prog_name);
int get_mtab_entry(char *_s1, char *_s2, char *_s3, char *_s4);
int put_mtab_entry(char *_s1, char *_s2, char *_s3, char *_s4);

/* read_tsc() and friends */
void read_tsc(u32_t *hi, u32_t *lo);
void read_tsc_64(u64_t *t);

/* return values for fsversion */
#define FSVERSION_MFS1	0x00001
#define FSVERSION_MFS2	0x00002
#define FSVERSION_MFS3	0x00003
#define FSVERSION_EXT2	0x10002

#endif
