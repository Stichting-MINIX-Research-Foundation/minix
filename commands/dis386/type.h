/* type.h - types for db.
 *
 * $Id: type.h,v 1.1 1997/10/20 12:00:00 cwr Exp cwr $
 */

typedef unsigned long bigcount_t;
typedef unsigned long flags_t;
typedef int bool_pt;
typedef char bool_t;
typedef int char_pt;
typedef int char16_t;		/* ASCII character possibly with scan code */
typedef unsigned count_t;
typedef unsigned long offset_t;
typedef unsigned opcode_pt;	/* promote to unsigned and not int */
typedef int (*pfi_t)();
typedef void (*pfv_t)();
typedef unsigned long physoff_t;
typedef unsigned peekboff_t;
typedef unsigned peekoff_t;
typedef int peekseg_t;
typedef unsigned port_t;
typedef int reg_pt;
typedef unsigned char reg_t;
typedef unsigned segment_t;
typedef long soffset_t;
typedef int su8_pt;
typedef int su16_t;
typedef unsigned u4_pt;		/* promote to unsigned and not int */
typedef unsigned u8_pt;
typedef unsigned u16_pt;

struct address_s
{
    offset_t off;
    offset_t base;
};

struct desctableptr_s {
  u16_t limit;
  u32_t base;			/* really u24_t + pad for 286 */
};

struct regs_s
{
    offset_t ax;
    offset_t bx;
    offset_t cx;
    offset_t dx;
    offset_t si;
    offset_t di;
    offset_t bp;
    offset_t sp;
    offset_t dsbase;
    offset_t esbase;
    offset_t fsbase;
    offset_t gsbase;
    offset_t ssbase;
    offset_t csbase;
    offset_t ip;
    flags_t f;
    offset_t ds;
    offset_t es;
    offset_t fs;
    offset_t gs;
    offset_t ss;
    offset_t cs;
};

struct specregs_s
{
    u32_t cr0;			/* control regs, cr0 is msw + pad for 286 */
    u32_t cr2;
    u32_t cr3;
    u32_t dr0;			/* debug regs */
    u32_t dr1;
    u32_t dr2;
    u32_t dr3;
    u32_t dr6;
    u32_t dr7;
    u32_t tr6;			/* test regs */
    u32_t tr7;
    u16_t gdtlimit;
    u32_t gdtbase;		/* depend on 16-bit compiler so no long align */
    u16_t gdtpad;
    u16_t idtlimit;
    u32_t idtbase;
    u16_t idtpad;
    u16_t ldtlimit;
    u32_t ldtbase;
    u16_t ldt;
    u16_t tr;			/* task register */
    u16_t trpad;
};

/* prototypes */

#if __STDC__
#define P(x)		x
#else
#define P(x)		()
#endif

/* library, very few! */
void *memcpy P((void *dst, const void *src, unsigned size));
void *memmove P((void *dst, const void *src, unsigned size));
unsigned strlen P((const char *s));
char *strncpy P((char *dst, const char *src, unsigned size));

/* db.c */
void db_main P((void));
void get_kbd_state P(());
void get_scr_state P(());
void info P((void));
void reboot P((void));
void reset_kbd_state P(());

#ifndef __NBSD_LIBC
/* getline.c */
char *getline P((char *startline, unsigned maxlength, unsigned offset));
#endif

/* ihexload.c */
void ihexload P((void));

/* io.c */
void can_itty P((void));
void can_keyboard P((void));
void can_otty P((void));
void can_screen P((void));
void closeio P((void));
void closestring P((void));
void enab_itty P((void));
void enab_keyboard P((void));
void enab_otty P((void));
void enab_screen P((void));
void flipcase P((void));
u8_pt get8 P((void));
u16_pt get16 P((void));
u32_t get32 P((void));
char16_t inchar P((void));
char_pt mytolower P((char_pt ch));
void openio P((void));
void openstring P((char *string, int length));
void outbyte P((char_pt byte));
void outcomma P((void));
void outh4 P((u4_pt num));
void outh8 P((u8_pt num));
void outh16 P((u16_pt num));
void outh32 P((u32_t num));
bool_pt outnl P((void));
count_t outsegaddr P((struct address_s *ap, offset_t addr));
count_t outsegreg P((offset_t num));
void outspace P((void));
void outstr P((char *s));
void outtab P((void));
void outustr P((char *s));
void set_tty P((void));
void show_db_screen P((void));
void show_user_screen P((void));
count_t stringpos P((void));
count_t stringtab P((void));
char_pt testchar P((void));

/* lib88.s */
int get_privilege P((void));
unsigned get_processor P((void));
unsigned in16portb P((port_t port));
physoff_t linear2addr P((segment_t segment, u16_pt offset));
void oportb P((port_t port, u8_pt value));
u8_pt peek_byte P((physoff_t offset));
u16_pt peek_word P((physoff_t offset));
u32_t peek_dword P((physoff_t offset));
void poke_byte P((physoff_t offset, u8_pt value));
void poke_word P((physoff_t offset, u16_pt value));
#ifdef N_TEXT
void symswap P((struct nlist *left, struct nlist *right,
		 segment_t tableseg, unsigned length));
#endif

/* pcio.c */
void kbdclose P((void));
char_pt kbdin P((void));
void kbdioctl P((int command));
void kbdopen P((void));
void kbdout P((int c));

/* screen.s */
void scrclose P((void));
void scrioctl P((int command));
char_pt scrin P((void));
void scropen P((void));
void scrout P((char_pt c));

/* sym.c */
#ifdef N_TEXT
struct nlist *findsname P((char *name, int where, bool_pt allflag));
struct nlist *findsval P((offset_t value, int where));
struct nlist *findrval P((offset_t value, int where));
void outsym P((struct nlist *sp, offset_t off));
void outrel P((struct nlist *sp, offset_t off));
#endif
void setproc P((char_pt c, struct address_s *pdptr, struct address_s *pmptr));
void syminit P((void));

/* tty.s */
void ttyclose P((void));
void ttyioctl P((int command));
char_pt ttyin P((void));
void ttyopen P((void));
void ttyout P((char_pt c));

/* unasm.c */
bool_pt puti P((void));

