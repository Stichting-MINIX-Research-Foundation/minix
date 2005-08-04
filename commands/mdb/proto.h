/* 
 * proto.h for mdb 
 */

/* core.c */

_PROTOTYPE( void prtmap, (void) );
_PROTOTYPE( unsigned long core_init, (char *filename) );
_PROTOTYPE( unsigned long file_init, (char *filename) );
_PROTOTYPE( long read_core, (int req, long addr, long data) );

/* mdb.c */ 

_PROTOTYPE( void mdb_error, (char *s) );
_PROTOTYPE( long breakpt , (long addr , char *cmd ));
_PROTOTYPE( void tstart , (int req , int verbose , int val , int cnt ));

/* io.c */

_PROTOTYPE( char *get_cmd , (char *cbuf, int csize) );
_PROTOTYPE( void openin , (char *s ));
_PROTOTYPE( void logging, (int c, char *name) );
_PROTOTYPE( void do_error, (char *message) );
_PROTOTYPE( int Printf, (const char *format, ...));
_PROTOTYPE( void outbyte, (int byte) );
_PROTOTYPE( void outcomma, (void) );
_PROTOTYPE( void outh8,  (unsigned num) );
_PROTOTYPE( void outh16, (unsigned num) );
_PROTOTYPE( void outh32, (unsigned num) );
_PROTOTYPE( void outh4,  (unsigned num) );
_PROTOTYPE( void outspace, (void) );
_PROTOTYPE( void outstr, (char *s) );
_PROTOTYPE( void outtab, (void) );
_PROTOTYPE( void outustr, (char *s) );
_PROTOTYPE( void closestring, (void) );
_PROTOTYPE( int mytolower, (int ch) );
_PROTOTYPE( void openstring, (char *string) );
_PROTOTYPE( int stringpos, (void) );
_PROTOTYPE( int stringtab, (void) );

/* mdbdis86.c */

_PROTOTYPE( long dasm, (long addr, int count, int symflg) );

/* mdbexp.c */ 

_PROTOTYPE( char *getexp, (char *buf, long *exp_p, int *seg_p) );
_PROTOTYPE( char *skip, (char *s) );

/* kernel.c */
_PROTOTYPE( long get_reg, (int pid, long k) );
_PROTOTYPE( void set_reg, (int pid, long k, long value) );
_PROTOTYPE( long reg_addr, (char *s) );
_PROTOTYPE( int disp_regs, (void) );
_PROTOTYPE( int outsegreg, (off_t num) );
_PROTOTYPE( void update , (void));
_PROTOTYPE( void disp_maps , (void));

/* misc.c */

_PROTOTYPE( void dump_stack, (long count) );
_PROTOTYPE( off_t file_size, (int fd) );
_PROTOTYPE( void help_on, (int h) );
_PROTOTYPE( void version_info, (void) );
_PROTOTYPE( void help_page, (void) );

#if EXTRA_SYMBOLS
/* gnu_sym.c */
_PROTOTYPE( void gnu_init, (char *filename) );
_PROTOTYPE( long gnu_symbolvalue, (char *name, int is_text ) );
_PROTOTYPE( void gnu_symbolic, (off_t value, int separator) );
_PROTOTYPE( void gnu_listsym, (int tchar) );
_PROTOTYPE( int gnu_text_symbol, (off_t value) );
_PROTOTYPE( int gnu_finds_pc, (off_t pc) );
_PROTOTYPE( int gnu_finds_data, (off_t off, int data_seg) );
#endif /* EXTRA_SYMBOLS */

/* sym.c */
_PROTOTYPE( void syminit, (char *filename) );
_PROTOTYPE( long symbolvalue, (char *name, int is_text ) );
_PROTOTYPE( void printhex, (off_t v) );
_PROTOTYPE( void symbolic, (off_t value, int separator) );
_PROTOTYPE( void listsym, (char *cmd) );
_PROTOTYPE( int text_symbol, (off_t value) );
_PROTOTYPE( int finds_pc, (off_t pc) );
_PROTOTYPE( int finds_data, (off_t off, int data_seg) );

/* trace.c */
_PROTOTYPE( long mdbtrace, (int req, int pid, long addr, long data) );
_PROTOTYPE( u32_t peek_dword, (off_t addr));

#if SYSCALLS_SUPPORT

/* syscalls.c */
_PROTOTYPE( void start_syscall, (long addr) );
_PROTOTYPE( void do_syscall, (long addr) );

/* decode.c */
_PROTOTYPE( void decode_message, (unsigned addr) );
_PROTOTYPE( void decode_result, (void) );

/* ioctl.c */
_PROTOTYPE( void decode_ioctl, (int sr, message *m) );

#endif /* SYSCALLS_SUPPORT */
