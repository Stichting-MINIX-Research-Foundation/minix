/* 
 * proto.h for mdb 
 */

/* core.c */

void prtmap(void);
unsigned long core_init(char *filename);
unsigned long file_init(char *filename);
long read_core(int req, long addr, long data);

/* mdb.c */ 

void mdb_error(char *s);
long breakpt(long addr , char *cmd );
void tstart(int req , int verbose , int val , int cnt );

/* io.c */

char *get_cmd(char *cbuf, int csize);
void openin(char *s );
void logging(int c, char *name);
void do_error(char *message);
int Printf(const char *format, ...);
void outbyte(int byte);
void outcomma(void);
void outh8(unsigned num);
void outh16(unsigned num);
void outh32(unsigned num);
void outh4(unsigned num);
void outspace(void);
void outstr(char *s);
void outtab(void);
void outustr(char *s);
void closestring(void);
int mytolower(int ch);
void openstring(char *string);
int stringpos(void);
int stringtab(void);

/* mdbdis86.c */

long dasm(long addr, int count, int symflg);

/* mdbexp.c */ 

char *getexp(char *buf, long *exp_p, int *seg_p);
char *skip(char *s);

/* kernel.c */
long get_reg(int pid, long k);
void set_reg(int pid, long k, long value);
long reg_addr(char *s);
int disp_regs(void);
int outsegreg(off_t num);
void update(void);
void disp_maps(void);

/* misc.c */

void dump_stack(long count);
off_t file_size(int fd);
void help_on(int h);
void version_info(void);
void help_page(void);

#if EXTRA_SYMBOLS
/* gnu_sym.c */
void gnu_init(char *filename);
long gnu_symbolvalue(char *name, int is_text );
void gnu_symbolic(off_t value, int separator);
void gnu_listsym(int tchar);
int gnu_text_symbol(off_t value);
int gnu_finds_pc(off_t pc);
int gnu_finds_data(off_t off, int data_seg);
#endif /* EXTRA_SYMBOLS */

/* sym.c */
void syminit(char *filename);
long symbolvalue(char *name, int is_text );
void printhex(off_t v);
void symbolic(off_t value, int separator);
void listsym(char *cmd);
int text_symbol(off_t value);
int finds_pc(off_t pc);
int finds_data(off_t off, int data_seg);

/* trace.c */
long mdbtrace(int req, int pid, long addr, long data);
u32_t peek_dword(off_t addr);

#if SYSCALLS_SUPPORT

/* syscalls.c */
void start_syscall(long addr);
void do_syscall(long addr);

/* decode.c */
void decode_message(unsigned addr);
void decode_result(void);

/* ioctl.c */
void decode_ioctl(int sr, message *m);

#endif /* SYSCALLS_SUPPORT */
