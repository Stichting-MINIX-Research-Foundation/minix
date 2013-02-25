/* 
 * io.c for mdb
 * all the i/o is here
 * NB: Printf()
 */
#include "mdb.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>
#include "proto.h"

#define OUTBUFSIZE	512
#define PAGESIZE	24

static int forceupper = FALSE;
static int someupper = FALSE;
static int stringcount = 0;
static char *string_ptr = NULL;	/* stringptr ambiguous at 8th char */
static char *stringstart = NULL;

static char outbuf[OUTBUFSIZE];
static FILE *cmdfile = stdin;
static FILE *outfile = stdout;
static FILE *logfile;
static int lineno;

int _doprnt(const char *format, va_list ap, FILE *stream );

char *get_cmd(cbuf, csize)
char *cbuf;
int csize;
{
char *r;

  fflush(stdout);
  if( cmdfile == stdin && outfile == stdout )
	printf("* ");
  r = fgets(cbuf, csize, cmdfile);
  if ( r == NULL && cmdfile != stdin ) {
	cmdfile = stdin;
	return get_cmd(cbuf, csize);
  }

  if ( logfile != NULL ) { 
	fprintf( logfile, "%s", cbuf );		
	lineno++;
  }

  return r;
}

void openin(s)
char *s;
{
char *t;

  if ((t = strchr(s,'\n')) != NULL) *t = '\0';
  if ((t = strchr(s,' ')) != NULL) *t = '\0';
  cmdfile = fopen(s,"r");
  if (cmdfile == NULL) {
	Printf("Cannot open %s for input\n",s);
	cmdfile = stdin; 
  }
}


/* Special version of printf 
 * really sprintf()
 * from MINIX library
 * followed by outstr()
 */
int Printf(const char *format, ...)
{
	va_list ap;
	int retval;
	va_start(ap, format);

	retval = vsnprintf(outbuf, OUTBUFSIZE, format, ap);
	va_end(ap);

        outstr(outbuf);

	return retval;
}

/* 
 * Set logging options 
 */
void logging( c, name )
int c;
char *name;
{
char *t;

  if ( c == 'q' && logfile != NULL ) {
	fclose(logfile);
	return;
  }

  if ((t = strchr(name,'\n')) != NULL) *t = '\0';
  if ((t = strchr(name,' ' )) != NULL) *t = '\0';
  if ( logfile != NULL ) fclose(logfile);
 
  if ( strlen(name) > 0 ) {
	logfile = fopen(name,"w");

	if (logfile == NULL) {
		Printf("Cannot open %s for output\n",name);
		return; 
	}

	/* Close standard output file for L */
  	if ( c == 'L' ) {
		fclose(outfile);
		outfile = NULL;
  	}
  }
  else 
  /* Reset */
  {
	if ( logfile != NULL ) fclose(logfile);
	outfile = stdout;
	outbyte('\n');
  }

}

/* Output system error string */
void do_error(m)
char *m;
{
    outstr(m);
    outstr(": ");
    outstr(strerror(errno));   
    outstr("\n");
}

void closestring()
{
/* close string device */

    stringcount = 0;
    stringstart = string_ptr = NULL;
}

int mytolower(ch)
int ch;
{
/* convert char to lower case */

    if (ch >= 'A' && ch <= 'Z')
	ch += 'a' - 'A';
    return ch;
}


void openstring(string)
char *string;
{
/* open string device */

    stringcount = 0;
    stringstart = string_ptr = string;
}

void outbyte(byte)
int byte;
{
/* print char to currently open output devices */

    if (forceupper && byte >= 'a' && byte <= 'z')
	byte += 'A' - 'a';
    if (string_ptr != NULL)
    {
	if ((*string_ptr++ = byte) == '\t')
	    stringcount = 8 * (stringcount / 8 + 1);
	else
	    ++stringcount;
    }
    else 
    {
	if ( paging && byte == '\n' ) {
		lineno++;		
		if ( lineno >= PAGESIZE) {
			if ( cmdfile == stdin ) {
			     printf("\nMore...any key to continue");
			     fgets( outbuf, OUTBUFSIZE-1, cmdfile );
		     	}
		}
		lineno = 0;
	}

	if ( outfile != NULL )  
		putc(byte,outfile);
	/* Do not log CR */
	if ( logfile != NULL && byte != '\r' ) 	
		putc(byte,logfile); 
    }
}


void outcomma()
{
/* print comma */

    outbyte(',');
}

static char hexdigits[] = "0123456789ABCDEF";
void outh4(num)
unsigned num;
{
/* print 4 bits hex */

    outbyte(hexdigits[num % 16]);
}

void outh8(num)
unsigned num;
{
/* print 8 bits hex */

    outh4(num / 16);
    outh4(num);
}

void outh16(num)
unsigned num;
{
/* print 16 bits hex */

    outh8(num / 256);
    outh8(num);
}

void outh32(num)
unsigned num;
{
/* print 32 bits hex */

    outh16((u16_t) (num >> 16));
    outh16((u16_t) num);
}

void outspace()
{
/* print space */

    outbyte(' ');
}

void outstr(s)
register char *s;
{
/* print string */

    while (*s)
	outbyte(*s++);
}

void outtab()
{
/* print tab */

    outbyte('\t');
}

void outustr(s)
register char *s;
{
/* print string, perhaps converting case to upper */

    forceupper = someupper;
    while (*s)
	outbyte(*s++);
    forceupper = FALSE;
}


int stringpos()
{
/* return current offset of string device */

    return string_ptr - stringstart;
}

int stringtab()
{
/* return current "tab" spot of string device */

    return stringcount;
}

