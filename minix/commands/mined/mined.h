/*========================================================================*
 *				Mined.h					  *
 *========================================================================*/

#include <minix/config.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>

#ifndef YMAX
#ifdef UNIX
#include <stdio.h>
#undef putchar
#undef getchar
#undef NULL
#undef EOF
extern char *CE, *VS, *SO, *SE, *CL, *AL, *CM;
#define YMAX		49
#else
#define YMAX		24		/* Maximum y coordinate starting at 0 */
/* Escape sequences. */
extern char *enter_string;	/* String printed on entering mined */
extern char *rev_video;		/* String for starting reverse video */
extern char *normal_video;	/* String for leaving reverse video */
extern char *rev_scroll;	/* String for reverse scrolling */
extern char *pos_string;	/* Absolute cursor positioning */
#define X_PLUS	' '		/* To be added to x for cursor sequence */
#define Y_PLUS	' '		/* To be added to y for cursor sequence */
#endif /* UNIX */

#define XMAX		79		/* Maximum x coordinate starting at 0*/
#define SCREENMAX	(YMAX - 1)	/* Number of lines displayed */
#define XBREAK		(XMAX - 0)	/* Line shift at this coordinate */
#define SHIFT_SIZE	25		/* Number of chars to shift */
#define SHIFT_MARK	'!'		/* Char indicating line continues */
#define MAX_CHARS	1024		/* Maximum chars on one line */

/* LINE_START must be rounded up to the lowest SHIFT_SIZE */
#define LINE_START	(((-MAX_CHARS - 1) / SHIFT_SIZE) * SHIFT_SIZE \
  				   - SHIFT_SIZE)
#define LINE_END	(MAX_CHARS + 1)	/* Highest x-coordinate for line */

#define LINE_LEN	(XMAX + 1)	/* Number of characters on line */
#define SCREEN_SIZE	(XMAX * YMAX)	/* Size of I/O buffering */
#define BLOCK_SIZE	1024

/* Return values of functions */
#define ERRORS		-1
#define NO_LINE		(ERRORS - 1)	/* Must be < 0 */
#define FINE	 	(ERRORS + 1)
#define NO_INPUT	(ERRORS + 2)

#define STD_OUT	 	1		/* File descriptor for terminal */

#if defined(__i386__)
#define MEMORY_SIZE	(50 * 1024)	/* Size of data space to malloc */
#endif

#define REPORT	2			/* Report change of lines on # lines */

typedef int FLAG;

/* General flags */
#define	FALSE		0
#define	TRUE		1
#define	NOT_VALID	2
#define	VALID		3
#define	OFF		4
#define	ON		5

/* Expression flags */
#define	FORWARD		6
#define	REVERSE		7

/* Yank flags */
#define	SMALLER		8
#define	BIGGER		9
#define	SAME		10
#define	EMPTY		11
#define	NO_DELETE	12
#define	DELETE		13
#define	READ		14
#define	WRITE		15

/*
 * The Line structure.  Each line entry contains a pointer to the next line,
 * a pointer to the previous line, a pointer to the text and an unsigned char
 * telling at which offset of the line printing should start (usually 0).
 */
struct Line {
  struct Line *next;
  struct Line *prev;
  char *text;
  unsigned char shift_count;
};

typedef struct Line LINE;

/* Dummy line indicator */
#define DUMMY		0x80
#define DUMMY_MASK	0x7F

/* Expression definitions */
#define NO_MATCH	0
#define MATCH		1
#define REG_ERROR	2

#define BEGIN_LINE	(2 * REG_ERROR)
#define END_LINE	(2 * BEGIN_LINE)

/*
 * The regex structure. Status can be any of 0, BEGIN_LINE or REG_ERROR. In
 * the last case, the result.err_mess field is assigned. Start_ptr and end_ptr
 * point to the match found. For more details see the documentation file.
 */
struct regex {
  union {
  	char *err_mess;
  	int *expression;
  } result;
  char status;
  char *start_ptr;
  char *end_ptr;
};

typedef struct regex REGEX;

/* NULL definitions */

/*
 * Forward declarations
 */
extern int nlines;		/* Number of lines in file */
extern LINE *header;		/* Head of line list */
extern LINE *tail;		/* Last line in line list */
extern LINE *top_line;		/* First line of screen */
extern LINE *bot_line;		/* Last line of screen */
extern LINE *cur_line;		/* Current line in use */
extern char *cur_text;		/* Pointer to char on current line in use */
extern int last_y;		/* Last y of screen. Usually SCREENMAX */
extern int ymax;
extern int screenmax;
extern char screen[SCREEN_SIZE];/* Output buffer for "writes" and "reads" */

extern int x, y;			/* x, y coordinates on screen */
extern FLAG modified;			/* Set when file is modified */
extern FLAG stat_visible;		/* Set if status_line is visible */
extern FLAG writable;			/* Set if file cannot be written */
extern FLAG quit;			/* Set when quit character is typed */
extern FLAG rpipe;		/* Set if file should be read from stdin */
extern int input_fd;			/* Fd for command input */
extern FLAG loading;			/* Set if we're loading a file */
extern int out_count;			/* Index in output buffer */
extern char file_name[LINE_LEN];	/* Name of file in use */
extern char text_buffer[MAX_CHARS];	/* Buffer for modifying text */
extern char *blank_line;		/* Clear line to end */

extern char yank_file[];		/* Temp file for buffer */
extern FLAG yank_status;		/* Status of yank_file */
extern long chars_saved;		/* Nr of chars saved in buffer */

/*
 * Empty output buffer
 */
#define clear_buffer()			(out_count = 0)

/*
 * Print character on terminal
 */
#define putchar(c)			(void) write_char(STD_OUT, (c))

/*
 * Ring bell on terminal
 */
#define ring_bell()			putchar('\07')

/*
 * Print string on terminal
 */
#define string_print(str)		(void) writeline(STD_OUT, (str))

/*
 * Flush output buffer
 */
#define flush()				(void) flush_buffer(STD_OUT)

/*
 * Convert cnt to nearest tab position
 */
#define tab(cnt)			(((cnt) + 8) & ~07)
#define is_tab(c)			((c) == '\t')

/*
 * Word defenitions
 */
#define white_space(c)	((c) == ' ' || (c) == '\t')
#define alpha(c)	((c) != ' ' && (c) != '\t' && (c) != '\n')

/*
 * Print line on terminal at offset 0 and clear tail of line
 */
#define line_print(line)		put_line(line, 0, TRUE)

/*
 * Move to coordinates and set textp. (Don't use address)
 */
#define move_to(nx, ny)			move((nx), NULL, (ny))

/*
 * Move to coordinates on screen as indicated by textp.
 */
#define move_address(address)		move(0, (address), y)

/*
 * Functions handling status_line. ON means in reverse video.
 */
#define status_line(str1, str2)	(void) bottom_line(ON, (str1), \
						    (str2), NULL, FALSE)
#define error(str1, str2)	(void) bottom_line(ON, (str1), \
						    (str2), NULL, FALSE)
#define get_string(str1,str2, fl) bottom_line(ON, (str1), NULL, (str2), fl)
#define clear_status()		(void) bottom_line(OFF, NULL, NULL, \
						    NULL, FALSE)

/*
 * Print info about current file and buffer.
 */
#define fstatus(mess, cnt)	file_status((mess), (cnt), file_name, \
					     nlines, writable, modified)

/*
 * Get real shift value.
 */
#define get_shift(cnt)		((cnt) & DUMMY_MASK)

#endif /* YMAX */

/* mined1.c */

void FS(void);
void VI(void);
int WT(void);
void XWT(void);
void SH(void);
LINE *proceed(LINE *line, int count );
int bottom_line(FLAG revfl, char *s1, char *s2, char *inbuf, FLAG statfl
	);
int count_chars(LINE *line );
void move(int new_x, char *new_address, int new_y );
int find_x(LINE *line, char *address );
char *find_address(LINE *line, int x_coord, int *old_x );
int length_of(char *string );
void copy_string(char *to, char *from );
void reset(LINE *head_line, int screen_y );
void set_cursor(int nx, int ny );
void open_device(void);
int getchar(void);
void display(int x_coord, int y_coord, LINE *line, int count );
int write_char(int fd, int c );
int writeline(int fd, char *text );
void put_line(LINE *line, int offset, FLAG clear_line );
int flush_buffer(int fd );
void bad_write(int fd );
void catch(int sig );
void abort_mined(void);
void raw_mode(FLAG state );
void panic(char *message );
char *alloc(int bytes );
void free_space(char *p );
/*
#ifdef UNIX
void(*key_map [128]) (void);
#else
void(*key_map [256]) (void);
#endif
*/
void initialize(void);
char *basename(char *path );
void load_file(char *file );
int get_line(int fd, char *buffer );
LINE *install_line(char *buffer, int length );
int main(int argc, char *argv []);
void RD(void);
void I(void);
void XT(void);
void ESC(void);
int ask_save(void);
int line_number(void);
void file_status(char *message, long count, char *file, int lines, FLAG
	writefl, FLAG changed );
#if __STDC__
void build_string(char *buf, char *fmt, ...);
#else
void build_string();
#endif
char *num_out(long number );
int get_number(char *message, int *result );
int input(char *inbuf, FLAG clearfl );
int get_file(char *message, char *file );
int _getchar(void);
void _flush(void);
void _putchar(int c );
void get_term(void);

/* mined2.c */

void UP(void);
void DN(void);
void LF(void);
void RT(void);
void HIGH(void);
void LOW(void);
void BL(void);
void EL(void);
void GOTO(void);
void PD(void);
void PU(void);
void HO(void);
void EF(void);
void SU(void);
void SD(void);
int forward_scroll(void);
int reverse_scroll(void);
void MP(void);
void move_previous_word(FLAG remove );
void MN(void);
void move_next_word(FLAG remove );
void DCC(void);
void DPC(void);
void DLN(void);
void DNW(void);
void DPW(void);
void S(int character );
void CTL(void);
void LIB(void);
LINE *line_insert(LINE *line, char *string, int len );
int insert(LINE *line, char *location, char *string );
LINE *line_delete(LINE *line );
void delete(LINE *start_line, char *start_textp, LINE *end_line, char
	*end_textp );
void PT(void);
void IF(void);
void file_insert(int fd, FLAG old_pos );
void WB(void);
void MA(void);
void YA(void);
void DT(void);
void set_up(FLAG remove );
FLAG checkmark(void);
int legal(void);
void yank(LINE *start_line, char *start_textp, LINE *end_line, char
	*end_textp, FLAG remove );
int scratch_file(FLAG mode );
void SF(void);
void SR(void);
REGEX *get_expression(char *message );
void GR(void);
void LR(void);
void change(char *message, FLAG file );
char *substitute(LINE *line, REGEX *program, char *replacement );
void search(char *message, FLAG method );
int find_y(LINE *match_line );
void finished(REGEX *program, int *last_exp );
void compile(char *pattern, REGEX *program );
LINE *match(REGEX *program, char *string, FLAG method );
int line_check(REGEX *program, char *string, FLAG method );
int check_string(REGEX *program, char *string, int *expression );
int star(REGEX *program, char *end_position, char *string, int
	*expression );
int in_list(int *list, int c, int list_length, int opcode );
void dummy_line(void);
