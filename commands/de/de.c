/****************************************************************/
/*								*/
/*	de.c							*/
/*								*/
/*		Main loop of the "Disk editor".			*/
/*								*/
/****************************************************************/
/*  origination         1989-Jan-15        Terrence W. Holm	*/
/****************************************************************/


#include <minix/config.h>
#include <sys/types.h>
#include <sys/dir.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <ctype.h>
#include <errno.h>
#undef ERROR			/* arrgghh, errno.h has this pollution */
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <minix/const.h>
#include <minix/type.h>
#include "mfs/const.h"
#include "mfs/type.h"
#include "mfs/inode.h"

#include "de.h"

static char copyright[] = "de  (c) Terrence W. Holm 1989";


_PROTOTYPE(void Push , (de_state *s ));
_PROTOTYPE(int Get_Base , (int *base ));
_PROTOTYPE(int Get_Filename , (de_state *s ));
_PROTOTYPE(int Get_Count , (char *units , unsigned long *result ));
_PROTOTYPE(void Exec_Shell , (void));
_PROTOTYPE(void Sigint , (int));



/****************************************************************/
/*								*/
/*	main()							*/
/*								*/
/*		Initialize. Handle the "-r" recovery option if	*/
/*		specified, else enter the main processing loop.	*/
/*								*/
/****************************************************************/


int main( argc, argv )
  int   argc;
  char *argv[];

  {
  static de_state s;		/* it is safer not to put it on the stack
				 * and some things probably now rely on zero
				 * initialization
				 */  
  char *command_name = argv[0];
  int   recover = 0;


  s.device_mode = O_RDONLY;


  /*  Parse arguments  */

  if ( argc == 3  &&  strcmp( argv[1], "-r" ) == 0 )
    {
    recover = 1;
    --argc;
    ++argv;
    }
  else if ( argc == 3  &&  strcmp( argv[1], "-w" ) == 0 )
    {
    s.device_mode = O_RDWR;
    --argc;
    ++argv;
    }

  if ( argc != 2  ||  *argv[1] == '-' )
    {
    fprintf( stderr, "Usage: %s [-w] /dev/device\n", command_name );
    fprintf( stderr, "       %s -r lost_file_name\n", command_name );
    exit( 1 );
    }


  /*  Set the effective id to the real id. This eliminates	*/
  /*  any increase in privilege done by a set-uid bit on the	*/
  /*  executable file. We want to be "root" for recovering 	*/
  /*  files, because we must be able to read the device.	*/
  /*  However, in normal usage, de(1) should not let just 	*/
  /*  anyone look at a file system, thus we drop the privilege.	*/
  /*								*/
  /*  NOTE: There is a security hole when using "-r" with a	*/
  /*  set-uid de(1). Do not use set-uid root if there is any	*/
  /*  way to externally access your Minix system.		*/

  if ( ! recover )
    {
    setuid( getuid() );
    setgid( getgid() );
    }


  /*  Set terminal characteristics, and ^C interrupt handler  */

  Save_Term();

  if ( signal( SIGINT, SIG_IGN ) != SIG_IGN )
    {
    signal( SIGINT,  Sigint );
    signal( SIGQUIT, Sigint );
    }

  Set_Term();

  if ( ! Init_Termcap() )
    Error( "Requires a termcap entry" );



  /*  Get the device file name. If recovering, also open an output file.  */

  if ( recover )
    {
    char *dir_name;
    char *file_name;
    struct stat device_stat;
    struct stat tmp_stat;

    /*  Split the path name into a directory and a file name.  */

    if ( strlen(argv[1]) > MAX_STRING )
      Error( "Path name too long" );

    if ( ! Path_Dir_File( argv[1], &dir_name, &file_name ) )
      Error( "Recover aborted" );

    /*  Find the device holding the directory.  */

    if ( (s.device_name = File_Device( dir_name )) == NULL )
      Error( "Recover aborted" );


    /*  The output file will be in /tmp with the same file name.  */

    strcpy( s.file_name, TMP );
    strcat( s.file_name, "/" );
    strcat( s.file_name, file_name );


    /*  Make sure /tmp is not on the same device as the file we	   */
    /*  are trying to recover (we don't want to use up the free	   */
    /*  i-node and blocks before we get a chance to recover them). */

    if ( stat( s.device_name, &device_stat ) == -1 )
      Error( "Can not stat(2) device %s", s.device_name );

    if ( stat( TMP, &tmp_stat ) == -1 )
      Error( "Can not stat(2) directory %s", TMP );

    if ( device_stat.st_rdev == tmp_stat.st_dev )
      Error( "Will not recover files on the same device as %s", TMP );

    if ( access( s.file_name, F_OK ) == 0 )
      Error( "Will not overwrite file %s", s.file_name );


    /*  Open the output file.  */

    if ( (s.file_f = fopen( s.file_name, "w" )) == NULL )
      Error( "Can not open file %s", s.file_name );

    /*  Don't let anyone else look at the recovered file  */

    chmod( s.file_name, 0700 );

    /*  If running as root then change the owner of the  */
    /*  restored file. If not running as root then the   */
    /*  chown(2) will fail.				 */

    chown( s.file_name, getuid(), getgid() );
    }
  else
    {
    s.device_name = argv[1];
    s.file_name[ 0 ] = '\0';
    }


  /*  Open the device file.  */

  {
  struct stat device_stat;
  off_t size;

  if ( stat( s.device_name, &device_stat ) == -1 )
    Error( "Can not find file %s", s.device_name );

  if ( (device_stat.st_mode & S_IFMT) != S_IFBLK  &&
       (device_stat.st_mode & S_IFMT) != S_IFREG )
    Error( "Can only edit block special or regular files" );


  if ( (s.device_d = open( s.device_name, s.device_mode )) == -1 )
    Error( "Can not open %s", s.device_name );

  if ( (size = lseek( s.device_d, 0L, SEEK_END )) == -1 )
    Error( "Error seeking %s", s.device_name );

  if ( size % K != 0 )
    {
    Warning( "Device size is not a multiple of 1024" );
    Warning( "The (partial) last block will not be accessible" );
    }
  }


  /*  Initialize the rest of the state record  */

  s.mode = WORD;
  s.output_base = 10;
  s.search_string[ 0 ] = '\0';

  {
  int i;

  for ( i = 0;  i < MAX_PREV;  ++i )
    {
    s.prev_addr[ i ] = 0L;
    s.prev_mode[ i ] = WORD;
    }
  }


  sync();

  Read_Super_Block( &s );

  Read_Bit_Maps( &s );

  s.address = 0L;



  /*  Recover mode basically performs an 'x' and an 'X'  */

  if ( recover )
    {
    ino_t inode = Find_Deleted_Entry( &s, argv[1] );
    off_t size;

    if ( inode == 0 )
      {
      unlink( s.file_name );
      Error( "Recover aborted" );
      }

    s.address = ( (long) s.first_data - s.inode_blocks ) * K
		      + (long) (inode - 1) * s.inode_size;

    Read_Block( &s, s.buffer );


    /*  Have found the lost i-node, now extract the blocks.  */

    if ( (size = Recover_Blocks( &s )) == -1L )
      {
      unlink( s.file_name );
      Error( "Recover aborted" );
      }

    Reset_Term();

    printf( "Recovered %ld bytes, written to file %s\n", size, s.file_name );

    exit( 0 );
    }


  /*  Enter the main loop, first time redraw the screen  */
  {
  int rc = REDRAW;


  do
    {
    if ( rc == REDRAW )
      {
      Read_Block( &s, s.buffer );
      Draw_Screen( &s );
      s.last_addr = s.address;
      Draw_Pointers( &s );
      }

    else if ( rc == REDRAW_POINTERS )
      {
      s.offset = (unsigned) (s.address & ~ K_MASK);
      Draw_Pointers( &s );
      }

    else if ( rc == ERROR )
      {
      Erase_Prompt();
      putchar( BELL );
      }
    } while ( (rc = Process( &s, Arrow_Esc(Get_Char()) )) != EOF );
  }


  /*  If there is an open output file that was never written to  */
  /*  then remove its directory entry. This occurs when no 'w' 	 */
  /*  or 'W' command occurred between a 'c' command and exiting	 */
  /*  the program.						 */

  if ( s.file_name[0] != '\0'  &&  ! s.file_written )
    unlink( s.file_name );


  Reset_Term();	   /*  Restore terminal characteristics  */

  exit( 0 );
  }



/****************************************************************/
/*								*/
/*	Get_Base( base )					*/
/*								*/
/*		Get a new base value.				*/
/*		Returns REDRAW or ERROR.			*/
/*								*/
/****************************************************************/



int Get_Base( base )
  int *base;
  {
	switch ( Get_Char() )
	  {
	  case 'h' :	*base = 16;
			break;

	  case 'd' :	*base = 10;
			break;

	  case 'o' :	*base = 8;
			break;

	  case 'b' :	*base = 2;
			break;

	  default  :	return( ERROR );
	  }

		return( REDRAW );
  }



/****************************************************************/
/*								*/
/*	Process( state, input_char )				*/
/*								*/
/*		Determine the function requested by the 	*/
/*		input character. Returns OK, REDRAW,		*/
/*		REDRAW_POINTERS,  ERROR or EOF.			*/
/*								*/
/****************************************************************/


int Process( s, c )
  de_state  *s;
  int  c;

  {
  switch ( c )
    {
    case 'b' :				/*  Back up one block	*/
    case ESC_PGUP :

		if ( s->address == 0 )
		  return( ERROR );

		s->address = (s->address - K) & K_MASK;

		return( REDRAW );


    case 'B' :				/*  Back up to home	*/
    case ESC_HOME :

		if ( s->address == 0 )
		  return( OK );

		Push( s );

		s->address = 0L;

		return( REDRAW );


    case 'c' :				/*  Change file name	*/

		{
		int rc = Get_Filename( s );

		return( rc == OK ? REDRAW : rc );
		}


    case 'd' :				/*  Down		*/
    case ESC_DOWN :

		{
		s->last_addr = s->address;

		switch ( s->mode )
		  {
		  case WORD :	s->address += 2;

				if ( (s->address & PAGE_MASK) == 0 )
				  return( REDRAW );

				return( REDRAW_POINTERS );

		  case BLOCK :	s->address += 64;

				if ( (s->last_addr & K_MASK) !=
				     (s->address   & K_MASK) )
				  return( REDRAW );

				return( REDRAW_POINTERS );

		  case MAP :	s->address += 256;

				return( REDRAW );

		  default :	Error( "Internal fault (mode)" );
		  }
		}


    case 'f' :				/*  Forward one block	*/
    case ' ' :
    case ESC_PGDN :

		if ( s->block == s->device_size - 1 )
		  return( ERROR );

		s->address = (s->address + K) & K_MASK;

		return( REDRAW );


    case 'F' :				/*  Forward to end	*/
    case ESC_END :

		{
		off_t  last_block = ( (long) s->device_size - 1 ) * K;

		if ( s->address == last_block )
		  return( OK );

		Push( s );

		s->address = last_block;

		return( REDRAW );
		}


    case 'g' :				/*  Goto block		*/

		{
		unsigned long block;

		if ( Get_Count( "Block?", &block ) )
		  {
		  if ( block >= s->zones )
		    {
		    Warning( "Block number too large" );
		    return( REDRAW );
		    }

		  Push( s );

		  s->address = (off_t) block * K;

		  return( REDRAW );
		  }
		else
		  return( ERROR );
		}


    case 'G' :				/*  Goto block indirect	*/

		{
		unsigned block = *( (word_t *) &s->buffer[ s->offset ] );

		if ( s->mode != WORD )
		  {
		  Warning( "Must be in visual mode \"word\"" );
		  return( REDRAW );
		  }

		if ( block >= s->zones )
		  {
		  Warning( "Block number too large" );
		  return( REDRAW );
		  }

		Push( s );

		s->mode = BLOCK;
		s->address = (long) block * K;

		return( REDRAW );
		}


    case 'h' :				/*  Help		*/
    case '?' :

		Draw_Help_Screen( s );

		Wait_For_Key();

		return( REDRAW );


    case 'i' :				/*  Goto i-node		*/

		{
		unsigned long inode;

		if ( Get_Count( "I-node?", &inode ) )
		  {
		  if ( inode < 1  || inode > s->inodes )
		    {
		    Warning( "Illegal i-node number" );
		    return( REDRAW );
		    }

		  Push( s );

		  s->mode = WORD;
		  s->address = (off_t) (s->first_data - s->inode_blocks) * K
				  + (off_t) (inode - 1) * s->inode_size;

		  return( REDRAW );
		  }
		else
		  return( ERROR );
		}


    case 'I' :				/*  Filename to i-node	*/

		{
		ino_t inode;
		char *filename;

		Draw_Prompt( "File name?" );

		filename = Get_Line();

		if ( filename == NULL  ||  filename[0] == '\0' )
		  return( ERROR );

		inode = Find_Inode( s, filename );

		if ( inode )
		  {
		  Push( s );

		  s->mode = WORD;
		  s->address = ( (long) s->first_data - s->inode_blocks ) * K
				  + (long) (inode - 1) * s->inode_size;
		  }

		return( REDRAW );
		}


    case 'l' :				/*  Left		*/
    case ESC_LEFT :

		{
		s->last_addr = s->address;

		switch ( s->mode )
		  {
		  case WORD :	s->address = s->address - 32;

				return( REDRAW );

		  case BLOCK :	s->address -= 1;

				if ( (s->last_addr & K_MASK) !=
				     (s->address   & K_MASK) )
				  return( REDRAW );

				return( REDRAW_POINTERS );

		  case MAP :	s->address -= 4;

				if ( (s->last_addr & ~ MAP_MASK) !=
				     (s->address   & ~ MAP_MASK) )
				  return( REDRAW );

				return( REDRAW_POINTERS );

		  default :	Error( "Internal fault (mode)" );
		  }
		}


    case 'm' :				/*  Invoke a Minix shell */

		Reset_Term();

		Exec_Shell();

		Set_Term();

		return( REDRAW );


    case 'n' :				/*  Search for next	*/

		{
		off_t addr;

		if ( s->search_string[0] == '\0' )
		  {
		  Warning( "No search string defined" );
		  return( REDRAW );
		  }

		Draw_Prompt( "Searching..." );

		if ( (addr = Search( s, s->search_string )) == -1L )
		  {
		  Warning( "Search string not found" );

		  Wait_For_Key();

		  return( REDRAW );
		  }

		Push( s );
		s->address = addr;

		return( REDRAW );
		}


    case 'o' :				/*  Set output base	*/

		Draw_Prompt( "Output base?" );

		return( Get_Base( &s->output_base ) );


    case 'p' :				/*  Previous address	*/

		{
		int  i;

		s->address = s->prev_addr[ 0 ];
		s->mode    = s->prev_mode[ 0 ];

  		for ( i = 0;  i < MAX_PREV - 1;  ++i )
		  {
    		  s->prev_addr[ i ] = s->prev_addr[ i + 1 ];
		  s->prev_mode[ i ] = s->prev_mode[ i + 1 ];
		  }

		return( REDRAW );
		}


    case 'q' :				/*  Quit		 */
    case EOF :
    case CTRL_D :

		return( EOF );


    case 'r' :				/*  Right		*/
    case ESC_RIGHT :

		{
		s->last_addr = s->address;

		switch ( s->mode )
		  {
		  case WORD :	s->address += 32;

				return( REDRAW );

		  case BLOCK :	s->address += 1;

				if ( (s->last_addr & K_MASK) !=
				     (s->address   & K_MASK) )
				  return( REDRAW );

				return( REDRAW_POINTERS );

		  case MAP :	s->address += 4;

				if ( (s->last_addr & ~ MAP_MASK) !=
				     (s->address   & ~ MAP_MASK) )
				  return( REDRAW );

				return( REDRAW_POINTERS );

		  default :	Error( "Internal fault (mode)" );
		  }
		}

    case 's' :				/*  Store word		*/

		{
		unsigned long word;

		if ( s->mode != WORD )
		  {
		  Warning( "Must be in visual mode \"word\"" );
		  return( REDRAW );
		  }

		if ( s->device_mode == O_RDONLY )
		  {
		  Warning( "Use -w option to open device for writing" );
		  return( REDRAW );
		  }

		if ( Get_Count( "Store word?", &word ) )
		  {
		  if ( word != (word_t) word )
		    {
		      Warning( "Word is more than 16 bits" );
		      return( REDRAW );
		    }
		  Write_Word( s, (word_t) word );

		  return( REDRAW );
		  }
		else
		  return( ERROR );
		}


    case 'u' :				/*  Up			*/
    case ESC_UP :

		{
		s->last_addr = s->address;

		switch ( s->mode )
		  {
		  case WORD :	s->address -= 2;

				if ( (s->last_addr & PAGE_MASK) == 0 )
				  return( REDRAW );

				return( REDRAW_POINTERS );

		  case BLOCK :	s->address -= 64;

				if ( (s->last_addr & K_MASK) !=
				     (s->address   & K_MASK) )
				  return( REDRAW );

				return( REDRAW_POINTERS );

		  case MAP :	s->address -= 256;

				return( REDRAW );

		  default :	Error( "Internal fault (mode)" );
		  }
		}


    case 'v' :				/*  Visual mode		*/

		Draw_Prompt( "Visual mode?" );

		switch ( Get_Char() )
		  {
		  case 'w' :	s->mode = WORD;
				break;

		  case 'b' :	s->mode = BLOCK;
				break;

		  case 'm' :	{
				/* Assume user knows if map mode is possible
				char *tty = ttyname( 0 );

				if ( tty == NULL  ||
				    strcmp( tty, "/dev/tty0" ) != 0 )
				  Warning( "Must be at console" );
				else
				*/
				  s->mode = MAP;

				break;
				}

		  default  :	return( ERROR );
		  }

		return( REDRAW );


    case 'w' :				/*  Write ASCII block	*/

		if ( s->file_name[0] == '\0' )
		  {
		  int  rc = Get_Filename( s );

		  if ( rc != OK )
		    return( rc );
		  }

		/*  We have a successfully opened file  */

		/*  Eliminate non-ASCII characters	*/
		{
		int i;
		char buf[ K ];
		char *from = s->buffer;
		char *to = buf;

		for ( i = 0;  i < K;  ++i, ++from )
		  {
		  *to = *from & 0x7f;

		  if ( *to != '\0'  &&  *to != '\177' )
		    ++to;
		  }

		if ( fwrite( buf, 1, (int)(to - buf), s->file_f ) != to - buf )
		  Warning( "Problem writing out buffer" );

		s->file_written = 1;

		return( REDRAW );
		}


    case 'W' :				/*  Write block exactly	*/

		if ( s->file_name[0] == '\0' )
		  {
		  int  rc = Get_Filename( s );

		  if ( rc != OK )
		    return( rc );
		  }

		/*  We have a successfully opened file  */

		if ( fwrite( s->buffer, 1, K, s->file_f ) != K )
		  Warning( "Problem writing out buffer" );

		s->file_written = 1;

		return( REDRAW );


    case 'x' :				/*  eXtract lost entry	*/

		{
		ino_t inode;
		char *filename;

		Draw_Prompt( "Lost file name?" );

		filename = Get_Line();

		if ( filename == NULL  ||  filename[0] == '\0' )
		  return( ERROR );

		inode = Find_Deleted_Entry( s, filename );

		if ( inode )
		  {
		  Push( s );

		  s->mode = WORD;
		  s->address = ( (long) s->first_data - s->inode_blocks ) * K
				  + (long) (inode - 1) * s->inode_size;
		  }

		return( REDRAW );
		}


    case 'X' :				/*  eXtract lost blocks	*/

		{
		int  rc;

		if ( s->mode != WORD )
		  {
		  Warning( "Must be in visual mode \"word\"" );
		  return( REDRAW );
		  }


		/*  Force a new output file name.  */

		if ( (rc = Get_Filename( s )) != OK )
		  return( rc );


		Draw_Strings( s );

		Erase_Prompt();
		Draw_Prompt( "Recovering..." );

		if ( Recover_Blocks( s ) == -1L )
		  unlink( s->file_name );

		/*  Force closure of output file.  */

		fclose( s->file_f );
		s->file_name[ 0 ] = '\0';

		return( REDRAW );
		}


    case '/' :				/*  Search		*/
    case ESC_PLUS :

		{
		off_t addr;
		char *string;

		Draw_Prompt( "Search string?" );

		string = Get_Line();

		if ( string == NULL )
		  return( ERROR );

		if ( string[0] != '\0' )
		  {
		  strcpy( s->search_string, string );
		  Draw_Strings( s );
		  }

		else if ( s->search_string[0] == '\0' )
		  {
		  Warning( "No search string defined" );
		  return( REDRAW );
		  }

		Erase_Prompt();
		Draw_Prompt( "Searching..." );

		if ( (addr = Search( s, s->search_string )) == -1L )
		  {
		  Warning( "Search string not found" );

		  Wait_For_Key();

		  return( REDRAW );
		  }

		Push( s );

		s->mode = BLOCK;
		s->address = addr;

		return( REDRAW );
		}


    default:
		return( ERROR );
    }
  }






/****************************************************************/
/*								*/
/*	Push( state )						*/
/*								*/
/*		Push current address and mode, used by the	*/
/*		commands B, F, g, G, i, I, n, x and /.  This	*/
/*		information is popped by the 'p' command.	*/
/*								*/
/****************************************************************/


void Push( s )
  de_state *s;

  {
  int  i;

  for ( i = MAX_PREV - 1;  i > 0;  --i )
    {
    s->prev_addr[ i ] = s->prev_addr[ i - 1 ];
    s->prev_mode[ i ] = s->prev_mode[ i - 1 ];
    }

  s->prev_addr[ 0 ] = s->address;
  s->prev_mode[ 0 ] = s->mode;
  }






/****************************************************************/
/*								*/
/*	Get_Filename( state )					*/
/*								*/
/*		Read and check a filename.			*/
/*								*/
/****************************************************************/


int Get_Filename( s )
  de_state *s;

  {
  char *filename;
  char *name;
  FILE *f;

  Draw_Prompt( "File name?" );

  filename = Get_Line();

  if ( filename == NULL  ||  filename[0] == '\0' )
    return( ERROR );


  for ( name = filename;  *name != '\0';  ++name )
    if ( ! isgraph( *name ) )
      {
      Warning( "File name contains non-graphic characters" );
      return( REDRAW );
      }


  if ( access( filename, F_OK ) == 0 )
    {
    Warning( "Will not overwrite file %s", filename );
    return( REDRAW );
    }

  if ( (f = fopen( filename, "w" )) == NULL )
    {
    Warning( "Can not open file %s", filename );
    return( REDRAW );
    }

  /*  If there is already an open output file then  */
  /*  close it. If it was never written to then	    */
  /*  remove its directory entry.		    */

  if ( s->file_name[0] != '\0' )
    {
    if ( ! s->file_written )
      unlink( s->file_name );

    fclose( s->file_f );
    }

  strcpy( s->file_name, filename );
  s->file_f = f;
  s->file_written = 0;

  return( OK );
  }






/****************************************************************/
/*								*/
/*	Get_Count()						*/
/*								*/
/*		Read and check a number. Returns non-zero	*/
/*		if successful.					*/
/*								*/
/****************************************************************/


int Get_Count( units, result )
  char *units;
  unsigned long *result;

  {
  char *number;

  Draw_Prompt( units );

  number = Get_Line();

  if ( number == NULL  ||  number[0] == '\0' )
    return( 0 );

  errno = 0;
  *result = strtoul( number, (char **) NULL, 0 );
  return( errno == 0 );
  }






/****************************************************************/
/*								*/
/*	In_Use( bit, map )					*/
/*								*/
/*		Is the bit set in the map?			*/
/*								*/
/****************************************************************/


int In_Use( bit, map )
  bit_t bit;
  bitchunk_t *map;

  {
  return( map[ (int) (bit / (CHAR_BIT * sizeof (bitchunk_t))) ] &
	  (1 << ((unsigned) bit % (CHAR_BIT * sizeof (bitchunk_t)))) );
  }






/****************************************************************/
/*								*/
/*	Find_Inode( state, filename )				*/
/*								*/
/*		Find the i-node for the given file name.	*/
/*								*/
/****************************************************************/


ino_t Find_Inode( s, filename )
  de_state *s;
  char *filename;

  {
  struct stat device_stat;
  struct stat file_stat;
  ino_t inode;


  if ( fstat( s->device_d, &device_stat ) == -1 )
    Error( "Can not fstat(2) file system device" );

#ifdef S_IFLNK
  if ( lstat( filename, &file_stat ) == -1 )
#else
  if ( stat( filename, &file_stat ) == -1 )
#endif
    {
    Warning( "Can not find file %s", filename );
    return( 0 );
    }

  if ( device_stat.st_rdev != file_stat.st_dev )
    {
    Warning( "File is not on device %s", s->device_name );
    return( 0 );
    }


  inode = file_stat.st_ino;

  if ( inode < 1  || inode > s->inodes )
    {
    Warning( "Illegal i-node number" );
    return( 0 );
    }

  return( inode );
  }






/****************************************************************/
/*								*/
/*	Exec_Shell()						*/
/*								*/
/*		Fork off a sub-process to exec() the shell.	*/
/*								*/
/****************************************************************/


void Exec_Shell()

  {
  int pid = fork();

  if ( pid == -1 )
    return;


  if ( pid == 0 )
    {
    /*  The child process  */

    extern char **environ;
    char *shell  =  getenv( "SHELL" );

    if ( shell == NULL )
      shell = "/bin/sh";

    execle( shell, shell, (char *) 0, environ );

    perror( shell );
    exit( 127 );
    }


  /*  The parent process: ignore signals, wait for sub-process	*/

  signal( SIGINT,  SIG_IGN );
  signal( SIGQUIT, SIG_IGN );

  {
  int  status;
  int  w;

  while ( (w=wait(&status)) != pid  &&  w != -1 );
  }

  signal( SIGINT,  Sigint );
  signal( SIGQUIT, Sigint );

  return;
  }






/****************************************************************/
/*								*/
/*	Sigint()						*/
/*								*/
/*		Terminate the program on an interrupt (^C)	*/
/*		or quit (^\) signal.				*/
/*								*/
/****************************************************************/


void Sigint(n)
int n;
  {
  Reset_Term();		/*  Restore terminal characteristics	*/

  putchar( '\n' );

  exit( 1 );
  }






/****************************************************************/
/*								*/
/*	Error( text, ... )					*/
/*								*/
/*		Print an error message on stderr.		*/
/*								*/
/****************************************************************/


#if __STDC__
void Error( const char *text, ... )
#else
void Error( text )
  char *text;
#endif  

  {
  va_list argp;

  Reset_Term();

  fprintf( stderr, "\nde: " );
  va_start( argp, text );
  vfprintf( stderr, text, argp );
  va_end( argp );
  if ( errno != 0 )
    fprintf( stderr, ": %s", strerror( errno ) );
  fprintf( stderr, "\n" );

  exit( 1 );
  }
