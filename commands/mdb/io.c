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

PRIVATE int forceupper = FALSE;
PRIVATE int someupper = FALSE;
PRIVATE int stringcount = 0;
PRIVATE char *string_ptr = NULL;	/* stringptr ambiguous at 8th char */
PRIVATE char *stringstart = NULL;

PRIVATE char outbuf[OUTBUFSIZE];
PRIVATE FILE *cmdfile = stdin;
PRIVATE FILE *outfile = stdout;
PRIVATE FILE *logfile;
PRIVATE int lineno;

_PROTOTYPE( int _doprnt, (const char *format, va_list ap, FILE *stream ));

PUBLIC char *get_cmd(cbuf, csize)
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

PUBLIC void openin(s)
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
PUBLIC int Printf(const char *format, ...)
{
	va_list ap;
	int retval;
	FILE tmp_stream;

	va_start(ap, format);

	tmp_stream._fd     = -1;
	tmp_stream._flags  = _IOWRITE + _IONBF + _IOWRITING;
	tmp_stream._buf    = (unsigned char *) outbuf;
	tmp_stream._ptr    = (unsigned char *) outbuf;
	tmp_stream._count  = 512;

	retval = _doprnt(format, ap, &tmp_stream);
	putc('\0',&tmp_stream);

	va_end(ap);

        outstr(outbuf);

	return retval;
}

/* 
 * Set logging options 
 */
PUBLIC void logging( c, name )
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
PUBLIC void do_error(m)
char *m;
{
    outstr(m);
    outstr(": ");
    outstr(strerror(errno));   
    outstr("\n");
}

PUBLIC void closestring()
{
/* close string device */

    stringcount = 0;
    stringstart = string_ptr = NULL;
}

PUBLIC int mytolower(ch)
int ch;
{
/* convert char to lower case */

    if (ch >= 'A' && ch <= 'Z')
	ch += 'a' - 'A';
    return ch;
}


PUBLIC void openstring(string)
char *string;
{
/* open string device */

    stringcount = 0;
    stringstart = string_ptr = string;
}

PUBLIC void outbyte(byte)
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


PUBLIC void outcomma()
{
/* print comma */

    outbyte(',');
}

PRIVATE char hexdigits[] = "0123456789ABCDEF";
PUBLIC void outh4(num)
unsigned num;
{
/* print 4 bits hex */

    outbyte(hexdigits[num % 16]);
}

PUBLIC void outh8(num)
unsigned num;
{
/* print 8 bits hex */

    outh4(num / 16);
    outh4(num);
}

PUBLIC void outh16(num)
unsigned num;
{
/* print 16 bits hex */

    outh8(num / 256);
    outh8(num);
}

PUBLIC void outh32(num)
unsigned num;
{
/* print 32 bits hex */

    outh16((u16_t) (num >> 16));
    outh16((u16_t) num);
}

PUBLIC void outspace()
{
/* print space */

    outbyte(' ');
}

PUBLIC void outstr(s)
register char *s;
{
/* print string */

    while (*s)
	outbyte(*s++);
}

PUBLIC void outtab()
{
/* print tab */

    outbyte('\t');
}

PUBLIC void outustr(s)
register char *s;
{
/* print string, perhaps converting case to upper */

    forceupper = someupper;
    while (*s)
	outbyte(*s++);
    forceupper = FALSE;
}


PUBLIC int stringpos()
{
/* return current offset of string device */

    return string_ptr - stringstart;
}

PUBLIC int stringtab()
{
/* return current "tab" spot of string device */

    return stringcount;
}

