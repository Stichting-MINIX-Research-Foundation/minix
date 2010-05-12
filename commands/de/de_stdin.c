/****************************************************************/
/*								*/
/*	de_stdin.c						*/
/*								*/
/*		Processing input from the "de" user.		*/
/*								*/
/****************************************************************/
/*  origination         1989-Jan-15        Terrence W. Holm	*/
/****************************************************************/


#include <sys/types.h>
#include <termios.h>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>

#include <minix/config.h>
#include <minix/const.h>
#include "mfs/const.h"
#include "mfs/inode.h"

#include "de.h"

FORWARD _PROTOTYPE(int Timed_Get_Char , (int time ));
FORWARD _PROTOTYPE(void Timed_Out , (int sig));




/****************************************************************/
/*								*/
/*	Save_Term()						*/
/*								*/
/*		Save the current terminal characteristics.	*/
/*								*/
/*								*/
/*	Set_Term()						*/
/*								*/
/*		Set up the terminal characteristics.		*/
/*								*/
/*								*/
/*	Reset_Term()						*/
/*								*/
/*		Restore the terminal characteristics.		*/
/*								*/
/****************************************************************/


static struct termios saved_term;


void Save_Term()

  {
  tcgetattr( 0, &saved_term );
  }




void Set_Term()

  {
  struct termios term;

  term = saved_term;


  /*  No tab expansion, no echo, don't map ^M to ^J, cbreak mode  */

  term.c_iflag &= ~ICRNL;
  term.c_oflag &= ~OPOST;
  term.c_lflag &= ~ICANON & ~ECHO;


  /*  Change the interrupt character to ^C  */

  term.c_cc[VINTR] = '\003';

  tcsetattr( 0, TCSANOW, &term );
  }




void Reset_Term()

  {
  tcsetattr( 0, TCSANOW, &saved_term );
  }






/****************************************************************/
/*								*/
/*	Get_Char()						*/
/*								*/
/*		Return the next input character. Escape		*/
/*		sequences are mapped to special codes.		*/
/*								*/
/****************************************************************/


int Get_Char()
  {
  int c;
  static int unget_char = EOF;


  /*  Flush the output to the screen before waiting  */
  /*  for input from the user.			     */

  fflush( stdout );

  if ( unget_char == EOF )
    {
    while ( (c = Timed_Get_Char( 60 * 60 )) < EOF )
      printf( "%c", BELL );
    }
  else
    {
    c = unget_char;
    unget_char = EOF;
    }

  if ( c == EOF )
    return( EOF );

  if ( c != ESCAPE )
    return( c );

  if ( (c = Timed_Get_Char( 1 )) <= EOF )
    return( ESCAPE );

  if ( c != '[' )
    {
    unget_char = c;
    return( ESCAPE );
    }

  if ( (c = Timed_Get_Char( 1 )) <= EOF )
    {
    unget_char = '[';
    return( ESCAPE );
    }

  return( c | 0x80 );   /* Flag ESC [ x  */
  }






int Timed_Get_Char( time )
  int time;

  {
  char c;
  int  count;

  signal( SIGALRM, Timed_Out );

  alarm( time );
  count = read( 0, &c, 1 );
  alarm( 0 );

  if ( count <= 0 )
    return( EOF + count );

  return( c & 0x7f );
  }






/****************************************************************/
/*								*/
/*	Get_Line()						*/
/*								*/
/*		Read a line from the user. Returns a pointer	*/
/*		to a local buffer, or NULL if DEL or a non-	*/
/*		ASCII character was typed. Processes ^H and	*/
/*		^U. ^M terminates the input.			*/
/*								*/
/****************************************************************/


char *Get_Line()

  {
  int c;
  int i;
  static char line[ MAX_STRING + 1 ];

  for ( i = 0;  i <= MAX_STRING;  ++i )
    {
    c = Get_Char();

    if ( c == EOF  ||  c == DEL  ||  (c & 0x80) )
	return( NULL );

    if ( c == BS )
	{
	if ( --i >= 0 )
	  {
	  printf( "\b \b" );
	  --i;
	  }
	}

    else if ( c == CTRL_U )
	{
	for ( --i;  i >= 0;  --i )
	  printf( "\b \b" );
	}

    else if ( c == '\r' )
	{
	line[ i ] = '\0';
	return( line );
	}

    else if ( i < MAX_STRING )
	{
	line[ i ] = c;
	Print_Ascii( c );
	}

    else  /*  Line buffer is full, don't add any more to it.  */
	{
	putchar( BELL );
	--i;
	}
    }

  Error( "Internal fault (line buffer overflow)" );

  /* NOTREACHED */
  return( NULL );
  }






/****************************************************************/
/*								*/
/*	Arrow_Esc( char )					*/
/*								*/
/*		If the keyboard does not generate Ansi escape	*/
/*		codes for the arrow keys, but does generate	*/
/*		single byte control codes, then map these	*/
/*		codes to the special characters we are using	*/
/*		to denote the Ansi escape codes.		*/
/*								*/
/****************************************************************/


extern  char   Kup;		/* (ku) - Up arrow key		*/
extern  char   Kdown;		/* (kd) - Down arrow key	*/
extern  char   Kleft;		/* (kl) - Left arrow key	*/
extern  char   Kright;		/* (kr) - Right arrow key	*/


int Arrow_Esc( c )
  int c;

  {
  if ( c == Kup )
    return( ESC_UP );

  if ( c == Kdown )
    return( ESC_DOWN );

  if ( c == Kleft )
    return( ESC_LEFT );

  if ( c == Kright )
    return( ESC_RIGHT );

  return( c );
  }

void Timed_Out(sig)
int sig;
  {}

/*
 * $PchHeader: /mount/hd2/minix/sys/cmd/de/RCS/de_stdin.c,v 1.3 1995/02/10 08:01:30 philip Exp $
 */
