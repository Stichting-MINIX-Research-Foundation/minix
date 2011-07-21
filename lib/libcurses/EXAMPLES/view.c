/*
 * view.c -- a silly little viewer program
 *
 * written by Eric S. Raymond <esr@snark.thyrsus.com> December 1994
 * to test the scrolling code in ncurses.
 *
 * modified by Thomas Dickey <dickey@clark.net> July 1995 to demonstrate
 * the use of 'resizeterm()', and May 2000 to illustrate wide-character
 * handling.
 *
 * Takes a filename argument.  It's a simple file-viewer with various
 * scroll-up and scroll-down commands.
 *
 * n	-- scroll one line forward
 * p	-- scroll one line back
 *
 * Either command accepts a numeric prefix interpreted as a repeat count.
 * Thus, typing `5n' should scroll forward 5 lines in the file.
 *
 * The way you can tell this is working OK is that, in the trace file,
 * there should be one scroll operation plus a small number of line
 * updates, as opposed to a whole-page update.  This means the physical
 * scroll operation worked, and the refresh() code only had to do a
 * partial repaint.
 *
 * $Id: view.c,v 1.2 2007/05/28 15:01:58 blymn Exp $
 */

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#ifdef NCURSES
#define _XOPEN_SOURCE_EXTENDED
#include <ncurses.h>
#include <term.h>
#else
#include <curses.h>
#endif /* NCURSES */
#include <locale.h>
#include <assert.h>
#include <ctype.h>
#include <termios.h>
#include <util.h>
#include <unistd.h>
#ifdef HAVE_WCHAR
#include <wchar.h>
#endif /* HAVE_WCHAR */
#ifdef DEBUG
#include <syslog.h>
#endif /* DEBUG */

#define UChar(c)    ((unsigned char)(c))
#define SIZEOF(table)	(sizeof(table)/sizeof(table[0]))
#define typeMalloc(type,n) (type *) malloc((n) * sizeof(type))

#define my_pair 1

#undef CURSES_CH_T
#ifdef HAVE_WCHAR
#define CURSES_CH_T cchar_t
#else
#define CURSES_CH_T chtype
#endif /* HAVE_WCHAR */

static void finish(int sig);
static void show_all(const char *tag);

static int shift = 0;
static bool try_color = FALSE;

static char *fname;
static CURSES_CH_T **my_lines;
static CURSES_CH_T **lptr;
static unsigned num_lines;

static void usage(void)
{
    static const char *msg[] = {
	    "Usage: view [options] file"
	    ,""
	    ,"Options:"
	    ," -c       use color if terminal supports it"
	    ," -i       ignore INT, QUIT, TERM signals"
	    ," -n NUM   specify maximum number of lines (default 1000)"
#if defined(KEY_RESIZE)
	    ," -r       use old-style sigwinch handler rather than KEY_RESIZE"
#endif
#ifdef TRACE
	    ," -t       trace screen updates"
	    ," -T NUM   specify trace mask"
#endif
    };
    size_t n;
    for (n = 0; n < SIZEOF(msg); n++)
	    fprintf(stderr, "%s\n", msg[n]);
    exit( 1 );
}

static int ch_len(CURSES_CH_T * src)
{
    int result = 0;

#ifdef HAVE_WCHAR
    while (getcchar(src++, NULL, NULL, NULL, NULL) > 0)
	    result++;
#else
    while (*src++)
	result++;
#endif
    return result;
}

/*
 * Allocate a string into an array of chtype's.  If UTF-8 mode is
 * active, translate the string accordingly.
 */
static CURSES_CH_T * ch_dup(char *src)
{
    unsigned len = strlen(src);
    CURSES_CH_T *dst = typeMalloc(CURSES_CH_T, len + 1);
    unsigned j, k;
#ifdef HAVE_WCHAR
    wchar_t wstr[CCHARW_MAX + 1];
    wchar_t wch;
    int l = 0;
    mbstate_t state;
    size_t rc;
    int width;
#endif

#ifdef HAVE_WCHAR
    mbrtowc( NULL, NULL, 1, &state );
#endif
    for (j = k = 0; j < len; j++) {
#ifdef HAVE_WCHAR
	    rc = mbrtowc(&wch, src + j, len - j, &state);
#ifdef DEBUG
        syslog( LOG_INFO, "[ch_dup]mbrtowc() returns %d", rc );
#endif /* DEBUG */
	    if (rc == (size_t) -1 || rc == (size_t) -2)
	        break;
	    j += rc - 1;
	    if ((width = wcwidth(wch)) < 0)
	        break;
	    if ((width > 0 && l > 0) || l == CCHARW_MAX) {
	        wstr[l] = L'\0';
	        l = 0;
	        if (setcchar(dst + k, wstr, 0, 0, NULL) != OK)
		        break;
	        ++k;
	    }
	    if (width == 0 && l == 0)
	        wstr[l++] = L' ';
	    wstr[l++] = wch;
#ifdef DEBUG
        syslog( LOG_INFO, "[ch_dup]wch=%x", wch );
#endif /* DEBUG */
#else
	    dst[k++] = src[j];
#endif
    }
#ifdef HAVE_WCHAR
    if (l > 0) {
	    wstr[l] = L'\0';
	    if (setcchar(dst + k, wstr, 0, 0, NULL) == OK)
	        ++k;
    }
    setcchar(dst + k, L"", 0, 0, NULL);
#else
    dst[k] = 0;
#endif
    return dst;
}

int main(int argc, char *argv[])
{
    int MAXLINES = 1000;
    FILE *fp;
    char buf[BUFSIZ];
    int i;
    int my_delay = 0;
    CURSES_CH_T **olptr;
    int length = 0;
    int value = 0;
    bool done = FALSE;
    bool got_number = FALSE;
    const char *my_label = "Input";
#ifdef HAVE_WCHAR
    cchar_t icc;
#endif /* HAVE_WCHAR */

    setlocale(LC_ALL, "");

    (void) signal(SIGINT, finish);	/* arrange interrupts to terminate */

    while ((i = getopt(argc, argv, "cin:rtT:")) != EOF) {
	    switch (i) {
	        case 'c':
	            try_color = TRUE;
	            break;
	        case 'i':
	            signal(SIGINT, SIG_IGN);
	            signal(SIGQUIT, SIG_IGN);
	            signal(SIGTERM, SIG_IGN);
	            break;
	        case 'n':
	            if ((MAXLINES = atoi(optarg)) < 1)
		        usage();
	            break;
#ifdef TRACE
	        case 'T':
	            trace(atoi(optarg));
	            break;
	        case 't':
	            trace(TRACE_CALLS);
	            break;
#endif
	        default:
	            usage();
	    }
    }
    if (optind + 1 != argc)
	    usage();

    if ((my_lines = typeMalloc(CURSES_CH_T *, MAXLINES + 2)) == 0)
	    usage();

    fname = argv[optind];
    if ((fp = fopen(fname, "r")) == 0) {
	    perror(fname);
	    exit( 1 );
    }

    /* slurp the file */
    num_lines = 0;
    for (lptr = &my_lines[0]; (lptr - my_lines) < MAXLINES; lptr++) {
	    char temp[BUFSIZ], *s, *d;
	    int col;

	    if (fgets(buf, sizeof(buf), fp) == 0)
	        break;

	    /* convert tabs so that shift will work properly */
	    for (s = buf, d = temp, col = 0; (*d = *s) != '\0'; s++) {
	        if (*d == '\n') {
		        *d = '\0';
		        break;
	        } else if (*d == '\t') {
		        col = (col | 7) + 1;
		        while ((d - temp) != col)
		            *d++ = ' ';
	        } else
#ifdef HAVE_WCHAR
		        col++, d++;
#else
	            if (isprint(UChar(*d))) {
		            col++;
		            d++;
	            } else {
		            sprintf(d, "\\%03o", UChar(*s));
		            d += strlen(d);
		            col = (d - temp);
	            }
#endif
	    }
	    *lptr = ch_dup(temp);
	    num_lines++;
    }
    (void) fclose(fp);
    length = lptr - my_lines;

    (void) initscr();		/* initialize the curses library */
    keypad(stdscr, TRUE);	/* enable keyboard mapping */
    (void) nonl();	 /* tell curses not to do NL->CR/NL on output */
    (void) cbreak(); /* take input chars one at a time, no wait for \n */
    (void) noecho();		/* don't echo input */
    nodelay(stdscr, TRUE);
    idlok(stdscr, TRUE);	/* allow use of insert/delete line */

    if (try_color) {
	    if (has_colors()) {
	        start_color();
	        init_pair(my_pair, COLOR_WHITE, COLOR_BLUE);
	        bkgd(COLOR_PAIR(my_pair));
	    } else {
	        try_color = FALSE;
	    }
    }

    lptr = my_lines;
    while (!done) {
	    int n;
#ifdef HAVE_WCHAR
        wint_t c = 0;
        int ret;
#else
        int c = 0;
#endif /* HAVE_WCHAR */

	    if (!got_number)
	        show_all(my_label);

	    n = 0;
	    for (;;) {
            c = 0;
#ifdef HAVE_WCHAR
            ret = get_wch( &c );
            if ( ret == ERR ) {
	            if (!my_delay)
		            napms(50);
                continue;
            }
#ifdef DEBUG
            else if ( ret == KEY_CODE_YES )
                syslog( LOG_INFO, "[main]Func key(%x)", c );
            else
                syslog( LOG_INFO, "[main]c=%x", c );
#endif /* DEBUG */
#else
	        c = getch();
#ifdef DEBUG
            syslog( LOG_INFO, "[main]c='%c'", c );
#endif /* DEBUG */
#endif /* HAVE_WCHAR */
	        if ((c < 127) && isdigit(c)) {
		        if (!got_number) {
		            mvprintw(0, 0, "Count: ");
		            clrtoeol();
		        }
		        addch(c);
		        value = 10 * value + (c - '0');
		        got_number = TRUE;
	        } else
		        break;
	    }
	    if (got_number && value) {
	        n = value;
	    } else {
	        n = 1;
	    }

#ifdef HAVE_WCHAR
	    if (ret != ERR)
            my_label = key_name( c );
        else
	        if (!my_delay)
		        napms(50);
#else
	    if (c != ERR)
	        my_label = keyname(c);
#endif /* HAVE_WCHAR */
	    switch (c) {
	        case KEY_DOWN:
#ifdef HAVE_WCHAR
            case L'n':
#else
	        case 'n':
#endif /* HAVE_WCHAR */
	            olptr = lptr;
	            for (i = 0; i < n; i++)
		            if ((lptr - my_lines) < (length - LINES + 1))
		                lptr++;
		            else
		                break;
	            wscrl(stdscr, lptr - olptr);
	            break;

	        case KEY_UP:
#ifdef HAVE_WCHAR
            case L'p':
#else
	        case 'p':
#endif /* HAVE_WCHAR */
	            olptr = lptr;
	            for (i = 0; i < n; i++)
		            if (lptr > my_lines)
		                lptr--;
		            else
		                break;
	            wscrl(stdscr, lptr - olptr);
	            break;

#ifdef HAVE_WCHAR
            case L'h':
#else
	        case 'h':
#endif /* HAVE_WCHAR */
	        case KEY_HOME:
	            lptr = my_lines;
	            break;

#ifdef HAVE_WCHAR
            case L'e':
#else
	        case 'e':
#endif /* HAVE_WCHAR */
	        case KEY_END:
	            if (length > LINES)
		            lptr = my_lines + length - LINES + 1;
	            else
		            lptr = my_lines;
	            break;

#ifdef HAVE_WCHAR
            case L'r':
#else
	        case 'r':
#endif /* HAVE_WCHAR */
	        case KEY_RIGHT:
	            shift += n;
	            break;

#ifdef HAVE_WCHAR
            case L'l':
#else
	        case 'l':
#endif /* HAVE_WCHAR */
	        case KEY_LEFT:
	            shift -= n;
	            if (shift < 0) {
		            shift = 0;
		            beep();
	            }
	            break;

#ifdef HAVE_WCHAR
            case L'q':
#else
	        case 'q':
#endif /* HAVE_WCHAR */
	            done = TRUE;
	            break;

#ifdef KEY_RESIZE
	        case KEY_RESIZE:
                //refresh();
	            break;
#endif
#ifdef HAVE_WCHAR
	        case L's':
#else
            case 's':
#endif /* HAVE_WCHAR */
	            if (got_number) {
		            halfdelay(my_delay = n);
	            } else {
		            nodelay(stdscr, FALSE);
		            my_delay = -1;
	            }
	            break;
#ifdef HAVE_WCHAR
            case L' ':
#else
	        case ' ':
#endif /* HAVE_WCHAR */
	            nodelay(stdscr, TRUE);
	            my_delay = 0;
	            break;
#ifndef HAVE_WCHAR
	        case ERR:
	            if (!my_delay)
		            napms(50);
	            break;
#endif /* HAVE_WCHAR */
	        default:
	            beep();
	            break;
	    }
	    if (c >= KEY_MIN || (c > 0 && !isdigit(c))) {
	        got_number = FALSE;
	        value = 0;
	    }
    }

    finish(0);			/* we're done */
}

static void finish(int sig)
{
    endwin();
    exit(sig != 0 ?  1 : 0 );
}

static void show_all(const char *tag)
{
    int i;
    char temp[BUFSIZ];
    CURSES_CH_T *s;
    time_t this_time;

    sprintf(temp, "%s (%3dx%3d) col %d ", tag, LINES, COLS, shift);
    i = strlen(temp);
    sprintf(temp + i, "view %.*s", (int) (sizeof(temp) - 7 - i), fname);
    move(0, 0);
    printw("%.*s", COLS, temp);
    clrtoeol();
    this_time = time((time_t *) 0);
    strcpy(temp, ctime(&this_time));
    if ((i = strlen(temp)) != 0) {
	    temp[--i] = 0;
	    if (move(0, COLS - i - 2) != ERR)
	        printw("  %s", temp);
    }

    scrollok(stdscr, FALSE);	/* prevent screen from moving */
    for (i = 1; i < LINES; i++) {
	    move(i, 0);
	    printw("%3ld:", (long) (lptr + i - my_lines));
	    clrtoeol();
	    if ((s = lptr[i - 1]) != 0) {
		    if (i < num_lines) {
			    int len = ch_len(s);
			    if (len > shift) {
#ifdef HAVE_WCHAR
				    add_wchstr(s + shift);
#else
				    addchstr(s + shift);
#endif
			    }
		    }
	    }
    }
    setscrreg(1, LINES - 1);
    scrollok(stdscr, TRUE);
    refresh();
}
