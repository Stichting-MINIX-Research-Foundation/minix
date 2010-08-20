#ifndef _MINLIB
#define _MINLIB

#ifndef _ANSI_H
#include <ansi.h>
#endif

/* Miscellaneous BSD. */
_PROTOTYPE(void swab, (char *_from, char *_to, int _count));
_PROTOTYPE(char *itoa, (int _n));
_PROTOTYPE(char *getpass, (const char *_prompt));

/* Miscellaneous MINIX. */
_PROTOTYPE(void std_err, (const char *_s));
_PROTOTYPE(void prints, (const char *_s, ...));
_PROTOTYPE(int fsversion, (char *_dev, char *_prog));
_PROTOTYPE(int getprocessor, (void));
_PROTOTYPE(void _cpuid, (u32_t *eax, u32_t *ebx, u32_t *ecx, u32_t *edx));
_PROTOTYPE(int load_mtab, (char *_prog_name));
_PROTOTYPE(int rewrite_mtab, (char *_prog_name));
_PROTOTYPE(int get_mtab_entry, (char *_s1, char *_s2, char *_s3, char *_s4));
_PROTOTYPE(int put_mtab_entry, (char *_s1, char *_s2, char *_s3, char *_s4));

/* read_tsc() and friends */
_PROTOTYPE(void read_tsc, (u32_t *hi, u32_t *lo));
_PROTOTYPE(void read_tsc_64, (u64_t *t));

/* return values for fsversion */
#define FSVERSION_MFS1	0x00001
#define FSVERSION_MFS2	0x00002
#define FSVERSION_MFS3	0x00003
#define FSVERSION_EXT2	0x10002

#endif
