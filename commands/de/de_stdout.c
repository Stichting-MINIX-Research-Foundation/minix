/****************************************************************/
/*								*/
/*	de_stdout.c						*/
/*								*/
/*		Displaying information from the "Disk editor".	*/
/*								*/
/****************************************************************/
/*  origination         1989-Jan-15        Terrence W. Holm	*/
/****************************************************************/


#include <minix/config.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include <grp.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <termcap.h>
#include <time.h>
#include <unistd.h>

#include <minix/const.h>
#include <minix/type.h>
#include "mfs/const.h"
#include "mfs/type.h"
#include "mfs/inode.h"
#include <minix/fslib.h>

#include "de.h"

#ifndef major
#define major(x) ( (x>>8) & 0377)
#define minor(x) (x & 0377)
#endif

/****************************************************************/
/*   		Code for handling termcap			*/
/****************************************************************/


#define  TC_BUFFER  1024	/* Size of termcap(3) buffer	*/
#define  TC_STRINGS  200	/* Enough room for cm,cl,so,se	*/


static  char  *Tmove;		/* (cm) - Format for tgoto	*/
static  char  *Tclr_all;	/* (cl) - Clear screen  	*/
static  char  *Treverse;	/* (so) - Start reverse mode 	*/
static  char  *Tnormal;		/* (se) - End reverse mode	*/

char   Kup    = 0;		/* (ku) - Up arrow key		*/
char   Kdown  = 0;		/* (kd) - Down arrow key	*/
char   Kleft  = 0;		/* (kl) - Left arrow key	*/
char   Kright = 0;		/* (kr) - Right arrow key	*/

_PROTOTYPE(void Goto , (int column , int line ));
_PROTOTYPE(void Block_Type , (de_state *s ));
_PROTOTYPE(void Draw_Words , (de_state *s ));
_PROTOTYPE(void Draw_Info , (de_state *s ));
_PROTOTYPE(void Draw_Block , (char *block ));
_PROTOTYPE(void Draw_Map , (char *block , int max_bits ));
_PROTOTYPE(void Draw_Offset , (de_state *s ));
_PROTOTYPE(void Word_Pointers , (off_t old_addr , off_t new_addr ));
_PROTOTYPE(void Block_Pointers , (off_t old_addr , off_t new_addr ));
_PROTOTYPE(void Map_Pointers , (off_t old_addr , off_t new_addr ));
_PROTOTYPE(void Print_Number , (Word_t number , int output_base ));
_PROTOTYPE(void Draw_Zone_Numbers , (de_state *s , struct inode *inode ,
						int zindex , int zrow ));



/****************************************************************/
/*								*/
/*	Init_Termcap()						*/
/*								*/
/*		Initializes the external variables for the	*/
/*		current terminal.				*/
/*								*/
/****************************************************************/


int Init_Termcap()

  {
  char  *term;
  char   buffer[ TC_BUFFER ];
  static char strings[ TC_STRINGS ];
  char  *s = &strings[0];
  char  *Kcode;


  term = getenv( "TERM" );

  if ( term == NULL )
    return( 0 );

  if ( tgetent( buffer, term ) != 1 )
    return( 0 );


  if ( (Tmove = tgetstr( "cm", &s )) == NULL )
    return( 0 );

  if ( (Tclr_all = tgetstr( "cl", &s )) == NULL )
    return( 0 );

  if ( (Treverse = tgetstr( "so", &s )) == NULL )
    {
    Treverse = Tnormal = s;
    *s = '\0';
    ++s;
    }
  else if ( (Tnormal = tgetstr( "se", &s )) == NULL )
    return( 0 );


  /*  See if there are single character arrow key codes  */

  if ( (Kcode = tgetstr( "ku", &s )) != NULL  &&  strlen( Kcode ) == 1 )
    Kup = Kcode[0];

  if ( (Kcode = tgetstr( "kd", &s )) != NULL  &&  strlen( Kcode ) == 1 )
    Kdown = Kcode[0];

  if ( (Kcode = tgetstr( "kl", &s )) != NULL  &&  strlen( Kcode ) == 1 )
    Kleft = Kcode[0];

  if ( (Kcode = tgetstr( "kr", &s )) != NULL  &&  strlen( Kcode ) == 1 )
    Kright = Kcode[0];


  return( 1 );
  }






/****************************************************************/
/*								*/
/*	Goto( column, line )					*/
/*								*/
/*		Use the termcap string to move the cursor.	*/
/*								*/
/****************************************************************/


void Goto( column, line )
  int  column;
  int  line;

  {
  fputs( tgoto( Tmove, column, line ), stdout );
  }






/****************************************************************/
/*   		       Output routines				*/
/****************************************************************/




/****************************************************************/
/*								*/
/*	Draw_Help_Screen()					*/
/*								*/
/****************************************************************/


void Draw_Help_Screen( s )
  de_state *s;

  {
  int down;
  int right;

  switch ( s->mode )
    {
    case WORD  :   down = 2;    right = 32;  break;
    case BLOCK :   down = 64;   right = 1;   break;
    case MAP   :   down = 256;  right = 4;   break;
    }

  printf( "%s                             ", Tclr_all );
  printf( "%sDE  COMMANDS%s\r\n\n\n", Treverse, Tnormal );


  printf( "   PGUP   b   Back one block              h   Help\r\n" );
  printf( "   PGDN   f   Forward one block           q   Quit\r\n" );
  printf( "   HOME   B   Goto first block            m   Minix shell\r\n" );
  printf( "   END    F   Goto last block\r\n" );
  printf( "                                          v   Visual mode (w b m)\r\n" );
  printf( "          g   Goto specified block        o   Output base (h d o b)\r\n" );
  printf( "          G   Goto block indirectly\r\n" );
  printf( "          i   Goto i-node                 c   Change file name\r\n" );
  printf( "          I   Filename to i-node          w   Write ASCII block\r\n" );
  printf( "                                          W   Write block exactly\r\n" );
  printf( "          /   Search\r\n" );
  printf( "          n   Next occurrence             x   Extract lost entry\r\n" );
  printf( "          p   Previous address            X   Extract lost blocks\r\n" );
  printf( "                                          s   Store word\r\n" );
  printf( "   UP     u   Move back %d bytes\r\n", down );
  printf( "   DOWN   d   Move forward %d bytes\r\n", down );
  printf( "   LEFT   l   Move back %d byte%s\r\n", right,
					right == 1 ? "" : "s" );
  printf( "   RIGHT  r   Move forward %d byte%s\r\n\n\n", right,
					right == 1 ? "" : "s" );
  }






/****************************************************************/
/*								*/
/*	Wait_For_Key()						*/
/*								*/
/*		The user must press a key to continue.		*/
/*								*/
/****************************************************************/


void Wait_For_Key()

  {
  Draw_Prompt( "Press a key to continue..." );

  Get_Char();
  }






/****************************************************************/
/*								*/
/*	Draw_Prompt( string )					*/
/*								*/
/*		Write a message in the "prompt" area.		*/
/*								*/
/****************************************************************/


void Draw_Prompt( string )
  char  *string;

  {
  Goto( PROMPT_COLUMN, PROMPT_LINE );

  printf( "%s%s%s ", Treverse, string, Tnormal );
  }






/****************************************************************/
/*								*/
/*	Erase_Prompt()						*/
/*								*/
/*		Erase the message in the "prompt" area.		*/
/*								*/
/****************************************************************/


void Erase_Prompt()

  {
  Goto( PROMPT_COLUMN, PROMPT_LINE );

  printf( "%77c", ' ' );

  Goto( PROMPT_COLUMN, PROMPT_LINE );
  }






/****************************************************************/
/*								*/
/*	Draw_Screen( state )					*/
/*								*/
/*		Redraw everything, except pointers.		*/
/*								*/
/****************************************************************/


void Draw_Screen( s )
  de_state *s;

  {
  fputs( Tclr_all, stdout );

  Draw_Strings( s );
  Block_Type( s );

  switch ( s->mode )
    {
    case WORD :   Draw_Words( s );
		  Draw_Info( s );
		  break;

    case BLOCK :  Draw_Block( s->buffer );
		  break;

    case MAP :	  {
		  int max_bits = 2 * K;

		  /*  Don't display the bits after the end  */
		  /*  of the i-node or zone bit maps.	    */

		  if ( s->block == 2 + s->inode_maps - 1 )
		    max_bits = (int)
			       (s->inodes_in_map
				- CHAR_BIT * K * (ino_t) (s->inode_maps - 1)
				- CHAR_BIT * (ino_t) (s->offset & ~ MAP_MASK));

		  else if ( s->block == 2 + s->inode_maps + s->zone_maps - 1 )
		    max_bits = (int)
			       (s->zones_in_map
			        - CHAR_BIT * K * (zone_t) (s->zone_maps - 1)
				- CHAR_BIT * (zone_t) (s->offset & ~ MAP_MASK));

		  if ( max_bits < 0 )
		      max_bits = 0;

		  Draw_Map( &s->buffer[ s->offset & ~ MAP_MASK ], max_bits );
		  break;
		  }
    }
  }






/****************************************************************/
/*								*/
/*	Draw_Strings( state )					*/
/*								*/
/*		The first status line contains the device name,	*/
/*		the current write file name (if one is open)	*/
/*		and the current search string (if one has	*/
/*		been defined).					*/
/*								*/
/*		Long strings are truncated.			*/
/*								*/
/****************************************************************/


void Draw_Strings( s )
  de_state *s;

  {
  int len;
  int i;

  Goto( STATUS_COLUMN, STATUS_LINE );

  printf( "Device %s= %-14.14s  ",
	     s->device_mode == O_RDONLY ? "" : "(w) ", s->device_name );

  switch ( s->magic )
    {
    case SUPER_MAGIC :	printf( "V1 file system  ");
			break;
    case SUPER_REV :	printf( "V1-bytes-swapped file system (?)  ");
			break;
    case SUPER_V2 :	printf( "V2 file system  ");
			break;
    case SUPER_V2_REV :	printf( "V2-bytes-swapped file system (?)  ");
			break;
    case SUPER_V3 :	printf( "V3 file system  ");
			break;
    default :		printf( "not a Minix file system  ");
			break;
    }

  len = strlen( s->file_name );

  if ( len == 0 )
    printf( "%29s", " " );
  else if ( len <= 20 )
    printf( "File = %-20s  ", s->file_name );
  else
    printf( "File = ...%17.17s  ", s->file_name + len - 17 );


  len = strlen( s->search_string );

  if ( len == 0 )
    printf( "%20s", " " );
  else
    {
    printf( "Search = " );

    if ( len <= 11 )
      {
      for ( i = 0;  i < len;  ++i )
        Print_Ascii( s->search_string[ i ] );

      for ( ;  i < 11;  ++i )
	putchar( ' ' );
      }
    else
      {
      for ( i = 0;  i < 8;  ++i )
        Print_Ascii( s->search_string[ i ] );

      printf( "..." );
      }
    }
  }






/****************************************************************/
/*								*/
/*	Block_Type( state )					*/
/*								*/
/*		Display the current block type.			*/
/*								*/
/****************************************************************/


void Block_Type( s )
  de_state *s;

  {
  Goto( STATUS_COLUMN, STATUS_LINE + 1 );

  printf( "Block  = %5u of %-5u  ", s->block, s->zones );

  if ( !s->is_fs )
    return;

  if ( s->block == BOOT_BLOCK )
    printf( "Boot block" );

  else if ( s->block == 1 )
    printf( "Super block" );

  else if ( s->block < 2 + s->inode_maps )
    printf( "I-node bit map" );

  else if ( s->block < 2 + s->inode_maps + s->zone_maps )
    printf( "Zone bit map" );

  else if ( s->block < s->first_data )
    printf( "I-nodes" );

  else
    printf( "Data block  (%sin use)",
	In_Use( (bit_t) (s->block - (s->first_data - 1)), s->zone_map )
	? "" : "not " );
  }






/****************************************************************/
/*								*/
/*	Draw_Words( state )					*/
/*								*/
/*		Draw a page in word format.			*/
/*								*/
/****************************************************************/


void Draw_Words( s )
  de_state *s;

  {
  int line;
  int addr = s->offset & ~ PAGE_MASK;


  for ( line = 0;  line < 16;  ++line, addr += 2 )
    {
    Goto( BLOCK_COLUMN, BLOCK_LINE + line );

    printf( "%5d  ", addr );

    Print_Number( *( (word_t *) &s->buffer[ addr ] ), s->output_base );
    }

  Goto( BLOCK_COLUMN + 64, BLOCK_LINE  );
  printf( "(base %d)", s->output_base );
  }






/****************************************************************/
/*								*/
/*	Draw_Info( state )					*/
/*								*/
/*		Add information to a page drawn in word format.	*/
/*		The routine recognizes the super block, inodes,	*/
/*		executables and "ar" archives. If the current	*/
/*		page is not one of these, then ASCII characters	*/
/*		are printed from the data words.		*/
/*								*/
/****************************************************************/


char *super_block_info[] =  {	"number of inodes",
				"V1 number of zones",
				"inode bit map blocks",
				"zone bit map blocks",
				"first data zone",
				"blocks per zone shift & flags",
				"maximum file size",
				"",
				"magic number",
				"fsck magic number",
				"V2 number of zones"  };


void Draw_Info( s )
  de_state *s;

  {
  int i;
  int page = s->offset >> PAGE_SHIFT;
  dev_t dev;


  if ( s->is_fs  &&  s->block == 1  &&  page == 0 )
      for ( i = 0;  i < 11;  ++i )
 	{
	Goto( INFO_COLUMN, INFO_LINE + i );
	printf( "%s", super_block_info[ i ] );
	}

  else if ( s->is_fs  &&  s->block >= s->first_data - s->inode_blocks  &&
	    s->block < s->first_data )
      {
      struct inode core_inode;
      d1_inode *dip1;
      d2_inode *dip2;
      struct inode *inode = &core_inode;
      int special = 0;
      int m;
      struct passwd *user;
      struct group *grp;

      dip1 = (d1_inode *) &s->buffer[ s->offset & ~ PAGE_MASK ];
      dip2 = (d2_inode *) &s->buffer[ s->offset & ~ PAGE_MASK
						& ~ (V2_INODE_SIZE-1) ];
      conv_inode( inode, dip1, dip2, READING, s->magic );

      user = getpwuid( inode->i_uid );
      grp  = getgrgid( inode->i_gid );

      if ( s->magic != SUPER_MAGIC  &&  page & 1 )
	{
	Draw_Zone_Numbers( s, inode, 2, 0 );
	return;
	}

      Goto( INFO_COLUMN, INFO_LINE  );

      switch( inode->i_mode & S_IFMT )
    	{
    	case S_IFDIR :  printf( "directory  " );
		    	break;

    	case S_IFCHR :  printf( "character  " );
		    	special = 1;
		    	break;

    	case S_IFBLK :  printf( "block  " );
		   	special = 1;
		    	break;

    	case S_IFREG :  printf( "regular  " );
		    	break;
#ifdef S_IFIFO
    	case S_IFIFO :  printf( "fifo  " );
		    	break;
#endif
#ifdef S_IFLNK
    	case S_IFLNK :  printf( "symlink  " );
		    	break;
#endif
#ifdef S_IFSOCK
    	case S_IFSOCK:  printf( "socket  " );
		    	break;
#endif
    	default      :  printf( "unknown  " );
    	}

	for ( m = 11;  m >= 0;  --m )
	  putchar( (inode->i_mode & (1<<m)) ? "xwrxwrxwrtgu"[m] : '-' );

	if ( s->magic == SUPER_MAGIC )
	  {
	  /* V1 file system */
	  Goto( INFO_COLUMN, INFO_LINE + 1 );
	  printf( "user %s", user ? user->pw_name : "" );

	  Goto( INFO_COLUMN, INFO_LINE + 2 );
	  printf( "file size %lu", inode->i_size );

	  Goto( INFO_COLUMN, INFO_LINE + 4 );
	  printf( "m_time %s", ctime( &inode->i_mtime ) );

	  Goto( INFO_COLUMN, INFO_LINE + 6 );
	  printf( "links %d, group %s",
		  inode->i_nlinks, grp ? grp->gr_name : "" );

	  Draw_Zone_Numbers( s, inode, 0, 7 );
	  }
	else
	  {
	  /* V2 file system, even page. */
	  Goto( INFO_COLUMN, INFO_LINE + 1 );
	  printf( "links %d ", inode->i_nlinks);

	  Goto( INFO_COLUMN, INFO_LINE + 2 );
	  printf( "user %s", user ? user->pw_name : "" );

	  Goto( INFO_COLUMN, INFO_LINE + 3 );
	  printf( "group %s", grp ? grp->gr_name : "" );

	  Goto( INFO_COLUMN, INFO_LINE + 4 );
	  printf( "file size %lu", inode->i_size );

	  Goto( INFO_COLUMN, INFO_LINE + 6 );
	  printf( "a_time %s", ctime( &inode->i_atime ) );

	  Goto( INFO_COLUMN, INFO_LINE + 8 );
	  printf( "m_time %s", ctime( &inode->i_mtime ) );

	  Goto( INFO_COLUMN, INFO_LINE + 10 );
	  printf( "c_time %s", ctime( &inode->i_ctime ) );

	  Draw_Zone_Numbers( s, inode, 0, 12 );
	}

      if ( special )
	{
	Goto( INFO_COLUMN, INFO_LINE + 7 );
	dev = (dev_t) inode->i_zone[0];
	printf( "major %d, minor %d", major(dev), minor(dev) );
	}
      }

  else  /*  Print ASCII characters for each byte in page  */
      {
      char *p = &s->buffer[ s->offset & ~ PAGE_MASK ];

      for ( i = 0;  i < 16;  ++i )
        {
        Goto( INFO_COLUMN, INFO_LINE + i );
        Print_Ascii( *p++ );
        Print_Ascii( *p++ );
        }

      if ( s->block >= s->first_data  &&  page == 0 )
	{
	unsigned magic  = ((s->buffer[1] & 0xff) << 8) | (s->buffer[0] & 0xff);
	unsigned second = ((s->buffer[3] & 0xff) << 8) | (s->buffer[2] & 0xff);

	/*  Is this block the start of an executable file?  */

	if ( magic == (unsigned) A_OUT )
	  {
          Goto( INFO_COLUMN, INFO_LINE );
	  printf( "executable" );

          Goto( INFO_COLUMN, INFO_LINE + 1 );

	  if ( second == (unsigned) SPLIT )
	    printf( "separate I & D" );
	  else
	    printf( "combined I & D" );
	  }
	}
      }
  }






/****************************************************************/
/*								*/
/*	Draw_Block( block )					*/
/*								*/
/*		Redraw a 1k block in character format.		*/
/*								*/
/****************************************************************/


void Draw_Block( block )
  char *block;

  {
  int line;
  int column;
  int reverse = 0;
  int msb_flag = 0;


  for ( line = 0;  line < 16;  ++line )
    {
    Goto( BLOCK_COLUMN, BLOCK_LINE + line );

    for ( column = 0;  column < 64;  ++column )
      {
      char c = *block++;

      if ( c & 0x80 )
	{
	msb_flag = 1;
	c &= 0x7f;
	}

      if ( c >= ' '  &&  c < DEL )
	{
	if ( reverse )
	  { fputs( Tnormal, stdout ); reverse = 0; }

        putchar( c );
	}
      else
	{
	if ( ! reverse )
	  { fputs( Treverse, stdout ); reverse = 1; }

	putchar( c == DEL ? '?' : '@' + c );
	}
      }  /*  end for ( column )  */
    }  /*  end for ( line )  */

  if ( reverse )
    { fputs( Tnormal, stdout ); reverse = 0; }

  if ( msb_flag )
    {
    Goto( BLOCK_COLUMN + 68, BLOCK_LINE + 6 );
    fputs( "(MSB)", stdout );
    }
  }






/****************************************************************/
/*								*/
/*	Draw_Map( block, max_bits )				*/
/*								*/
/*		Redraw a block in a bit map format.		*/
/*		Display min( max_bits, 2048 ) bits.		*/
/*								*/
/*		The 256 bytes in "block" are displayed from	*/
/*		top to bottom and left to right. Bit 0 of	*/
/*		a byte is towards the top of the screen.	*/
/*								*/
/*		Special graphic codes are used to generate	*/
/*		two "bits" per character position. So a 16	*/
/*		line by 64 column display is 32 "bits" by	*/
/*		64 "bits". Or 4 bytes by 64 bytes.		*/
/*								*/
/****************************************************************/


void Draw_Map( block, max_bits )
  char *block;
  int   max_bits;

  {
  int line;
  int column;
  int bit_count = 0;

  for ( line = 0;  line < 16;  ++line )
    {
    char *p = &block[ (line & 0xC) >> 2 ];
    int shift = (line & 0x3) << 1;

    Goto( BLOCK_COLUMN, BLOCK_LINE + line );

    for ( column = 0;  column < 64;  ++column, p += 4 )
      {
      char c = (*p >> shift) & 0x3;
      int current_bit = ((p - block) << 3) + shift;

      /*  Don't display bits past "max_bits"  */

      if ( current_bit >= max_bits )
	break;

      /*  If "max_bits" occurs in between the two bits  */
      /*  I am trying to display as one character, then	*/
      /*  zero off the high-order bit.			*/

      if ( current_bit + 1 == max_bits )
	c &= 1;

      switch ( c )
	{
	case 0 :  putchar( BOX_CLR );
		  break;

	case 1 :  putchar( BOX_TOP );
		  ++bit_count;
		  break;

	case 2 :  putchar( BOX_BOT );
		  ++bit_count;
		  break;

	case 3 :  putchar( BOX_ALL );
		  bit_count += 2;
		  break;
	}
      }  /*  end for ( column )  */
    }  /*  end for ( line )  */


  Goto( BLOCK_COLUMN + 68, BLOCK_LINE + 6 );
  printf( "(%d)", bit_count );
  }






/****************************************************************/
/*								*/
/*	Draw_Pointers( state )					*/
/*								*/
/*		Redraw the pointers and the offset field.	*/
/*		The rest of the screen stays intact.		*/
/*								*/
/****************************************************************/


void Draw_Pointers( s )
  de_state *s;

  {
  Draw_Offset( s );

  switch ( s->mode )
    {
    case WORD :   Word_Pointers( s->last_addr, s->address );
		  break;

    case BLOCK :  Block_Pointers( s->last_addr, s->address );
		  break;

    case MAP :	  Map_Pointers( s->last_addr, s->address );
		  break;
    }

  Goto( PROMPT_COLUMN, PROMPT_LINE );
  }






/****************************************************************/
/*								*/
/*	Draw_Offset( state )					*/
/*								*/
/*		Display the offset in the current buffer	*/
/*		and the relative position if within a map	*/
/*		or i-node block.				*/
/*								*/
/****************************************************************/


void Draw_Offset( s )
  de_state *s;

  {
  Goto( STATUS_COLUMN, STATUS_LINE + 2 );

  printf( "Offset = %5d           ", s->offset );


  if ( s->block < 2 )
    return;

  if ( s->block < 2 + s->inode_maps )
    {
    long bit = (s->address - 2 * K) * 8;

    if ( bit < s->inodes_in_map )
	printf( "I-node %ld of %d     ", bit, s->inodes );
    else
	printf( "(padding)                " );
    }

  else if ( s->block < 2 + s->inode_maps + s->zone_maps )
    {
    long bit = (s->address - (2 + s->inode_maps) * K) * 8;

    if ( bit < s->zones_in_map )
	printf( "Block %ld of %u     ", bit + s->first_data - 1, s->zones );
    else
	printf( "(padding)                " );
    }

  else if ( s->block < s->first_data )
    {
    bit_t node = (s->address - (2 + s->inode_maps + s->zone_maps) * K) /
		s->inode_size + 1;

    if ( node <= s->inodes )
	printf( "I-node %lu of %lu  (%sin use)       ",
		(unsigned long) node, (unsigned long) s->inodes,
		In_Use( node, s->inode_map ) ? "" : "not " );
    else
	printf( "(padding)                             " );
    }
  }






/****************************************************************/
/*								*/
/*	Word_Pointers( old_addr, new_addr )			*/
/*								*/
/*	Block_Pointers( old_addr, new_addr )			*/
/*								*/
/*	Map_Pointers( old_addr, new_addr )			*/
/*								*/
/*		Redraw the index pointers for a each type	*/
/*		of display. The pointer at "old_addr" is	*/
/*		erased and a new pointer is positioned		*/
/*		for "new_addr". This makes the screen		*/
/*		update faster and more pleasant for the user.	*/
/*								*/
/****************************************************************/


void Word_Pointers( old_addr, new_addr )
  off_t old_addr;
  off_t new_addr;

  {
  int from = ( (int) old_addr & PAGE_MASK ) >> 1;
  int to   = ( (int) new_addr & PAGE_MASK ) >> 1;

  Goto( BLOCK_COLUMN - 2, BLOCK_LINE + from );
  putchar( ' ' );

  Goto( BLOCK_COLUMN - 2, BLOCK_LINE + to );
  putchar( '>' );
  }




void Block_Pointers( old_addr, new_addr )
  off_t old_addr;
  off_t new_addr;

  {
  int from = (int) old_addr & ~K_MASK;
  int to   = (int) new_addr & ~K_MASK;

  Goto( BLOCK_COLUMN - 2, BLOCK_LINE + from / 64 );
  putchar( ' ' );

  Goto( BLOCK_COLUMN - 2, BLOCK_LINE + to / 64 );
  putchar( '>' );

  Goto( BLOCK_COLUMN + from % 64, BLOCK_LINE + 17 );
  putchar( ' ' );

  Goto( BLOCK_COLUMN + to % 64, BLOCK_LINE + 17 );
  putchar( '^' );
  }




void Map_Pointers( old_addr, new_addr )
  off_t old_addr;
  off_t new_addr;

  {
  int from = ( (int) old_addr & MAP_MASK ) >> 2;
  int to   = ( (int) new_addr & MAP_MASK ) >> 2;

  Goto( BLOCK_COLUMN + from, BLOCK_LINE + 17 );
  putchar( ' ' );

  Goto( BLOCK_COLUMN + to, BLOCK_LINE + 17 );
  putchar( '^' );
  }






/****************************************************************/
/*								*/
/*	Print_Number( number, output_base )			*/
/*								*/
/*		Output "number" in the output base.		*/
/*								*/
/****************************************************************/


void Print_Number( number, output_base )
  word_t number;
  int output_base;

  {
  switch ( output_base )
    {
    case 16 :	printf( "%5x", number );
		break;

    case 10 :	printf( "%7u", number );
		break;

    case 8 :	printf( "%7o", number );
		break;

    case 2 :	{
      		unsigned int mask;
      		char pad = ' ';

      		for ( mask = 0x8000;  mask > 1;  mask >>= 1 )
		  putchar( (mask & number) ? (pad = '0', '1') : pad );

      		putchar( (0x01 & number) ? '1' : '0' );

		break;
      		}

    default :	Error( "Internal fault (output_base)" );
    }
  }






/****************************************************************/
/*								*/
/*	Print_Ascii( char )					*/
/*								*/
/*		Display a character in reverse mode if it	*/
/*		is not a normal printable ASCII character.	*/
/*								*/
/****************************************************************/


void Print_Ascii( c )
  char c;

  {
  c &= 0x7f;

  if ( c < ' ' )
    printf( "%s%c%s", Treverse, '@' + c, Tnormal );
  else if ( c == DEL )
    printf( "%s?%s", Treverse, Tnormal );
  else
    putchar( c );
  }






/****************************************************************/
/*								*/
/*	Warning( text, arg1, arg2 )				*/
/*								*/
/*		Display a message for 2 seconds.		*/
/*								*/
/****************************************************************/


#if __STDC__
void Warning( const char *text, ... )
#else
void Warning( text )
  char *text;
#endif  

  {
  va_list argp;
  
  printf( "%c%s", BELL, Tclr_all );

  Goto( WARNING_COLUMN, WARNING_LINE );

  printf( "%s Warning: ", Treverse );
  va_start( argp, text );
  vprintf( text, argp );
  va_end( argp );
  printf( " %s", Tnormal );

  fflush(stdout);		/* why does everyone forget this? */

  sleep( 2 );
  }


void Draw_Zone_Numbers( s, inode, zindex, zrow )
  de_state *s;
  struct inode *inode;
  int zindex;
  int zrow;

  {
  static char *plurals[] = { "", "double ", "triple " };
  zone_t zone;

  for ( ; zrow < 16;
	++zindex, zrow += s->zone_num_size / sizeof (word_t) )
    {
    Goto( INFO_COLUMN, INFO_LINE + zrow );
    if ( zindex < s->ndzones )
      printf( "zone %d", zindex );
    else
      printf( "%sindirect", plurals[ zindex - s->ndzones ] );
    if ( s->magic != SUPER_MAGIC )
      {
      zone = inode->i_zone[ zindex ];
      if ( zone != (word_t) zone )
	{
	Goto( INFO_COLUMN + 16, INFO_LINE + zrow );
	printf("%ld", (long) zone );
	}
      }
    }
  }
