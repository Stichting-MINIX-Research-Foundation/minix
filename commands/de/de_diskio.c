/****************************************************************/
/*								*/
/*	de_diskio.c						*/
/*								*/
/*		Reading and writing to a file system device.	*/
/*								*/
/****************************************************************/
/*  origination         1989-Jan-15        Terrence W. Holm	*/
/****************************************************************/


#include <minix/config.h>
#include <sys/types.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <dirent.h>

#include <minix/const.h>
#include <minix/type.h>
#include "mfs/const.h"
#include "mfs/type.h"
#include "mfs/super.h"
#include "mfs/inode.h"
#include <minix/fslib.h>

#include "de.h"




/****************************************************************/
/*								*/
/*	Read_Disk( state, block_addr, buffer )			*/
/*								*/
/*		Reads a 1k block at "block_addr" into "buffer".	*/
/*								*/
/****************************************************************/


void Read_Disk( s, block_addr, buffer )
  de_state *s;
  off_t  block_addr;
  char  *buffer;

  {
  if ( lseek( s->device_d, block_addr, SEEK_SET ) == -1 )
    Error( "Error seeking %s", s->device_name );

  if ( read( s->device_d, buffer, s->block_size ) != s->block_size )
    Error( "Error reading %s", s->device_name );
  }






/****************************************************************/
/*								*/
/*	Read_Block( state, buffer )				*/
/*								*/
/*		Reads a 1k block from "state->address" into	*/
/*		"buffer". Checks "address", and updates		*/
/*		"block" and "offset".				*/
/*								*/
/****************************************************************/


void Read_Block( s, buffer )
  de_state *s;
  char *buffer;

  {
  off_t end_addr;
  off_t block_addr;
  end_addr = (long) s->device_size * s->block_size - 1;

  if ( s->address < 0 )
    s->address = 0L;

  if ( s->address > end_addr )
    s->address = end_addr;

  /*  The address must be rounded off for  */
  /*  certain visual display modes.        */

  if ( s->mode == WORD )
    s->address &= ~1L;
  else if ( s->mode == MAP )
    s->address &= ~3L;


  block_addr = s->address & K_MASK;

  s->block  = (zone_t) (block_addr >> K_SHIFT);
  s->offset = (unsigned) (s->address - block_addr);

  Read_Disk( s, block_addr, buffer );
  }






/****************************************************************/
/*								*/
/*	Read_Super_Block( state )				*/
/*								*/
/*		Read and check the super block.			*/
/*								*/
/****************************************************************/


void Read_Super_Block( s )
  de_state *s;

  {
  struct super_block *super = (struct super_block *) s->buffer;
  unsigned inodes_per_block;
  off_t size;

  s->block_size = K;
  Read_Disk( s, (long) SUPER_BLOCK_BYTES, s->buffer );

  s->magic = super->s_magic;
  if ( s->magic == SUPER_MAGIC )
    {
    s->is_fs = TRUE;
    s->v1 = TRUE;
    s->inode_size = V1_INODE_SIZE;
    inodes_per_block = V1_INODES_PER_BLOCK;
    s->nr_indirects = V1_INDIRECTS;
    s->zone_num_size = V1_ZONE_NUM_SIZE;
    s->zones = super->s_nzones;
    s->ndzones = V1_NR_DZONES;
    s->block_size = _STATIC_BLOCK_SIZE;
    }
  else if ( s->magic == SUPER_V2 || s->magic == SUPER_V3)
    {
    if(s->magic == SUPER_V3)
    	s->block_size = super->s_block_size;
    else
    	s->block_size = _STATIC_BLOCK_SIZE;
    s->is_fs = TRUE;
    s->v1 = FALSE;
    s->inode_size = V2_INODE_SIZE;
    inodes_per_block = V2_INODES_PER_BLOCK(s->block_size);
    s->nr_indirects = V2_INDIRECTS(s->block_size);
    s->zone_num_size = V2_ZONE_NUM_SIZE;
    s->zones = super->s_zones;
    s->ndzones = V2_NR_DZONES;
    }
  else  
    {
    if ( super->s_magic == SUPER_REV )
      Warning( "V1-bytes-swapped file system (?)" );
    else if ( super->s_magic == SUPER_V2_REV )
      Warning( "V2-bytes-swapped file system (?)" );
    else  
      Warning( "Not a Minix file system" );
    Warning( "The file system features will not be available" );  
    s->zones = 100000L;
    return;
    }

  s->inodes = super->s_ninodes;
  s->inode_maps = bitmapsize( (bit_t) s->inodes + 1 , s->block_size);
  if ( s->inode_maps != super->s_imap_blocks )
    {
    if ( s->inode_maps > super->s_imap_blocks )
      Error( "Corrupted inode map count or inode count in super block" );
    else  
      Warning( "Count of inode map blocks in super block suspiciously high" );
    s->inode_maps = super->s_imap_blocks;
    }

  s->zone_maps = bitmapsize( (bit_t) s->zones , s->block_size);
  if ( s->zone_maps != super->s_zmap_blocks )
    {
    if ( s->zone_maps > super->s_zmap_blocks )
      Error( "Corrupted zone map count or zone count in super block" );
    else
      Warning( "Count of zone map blocks in super block suspiciously high" );
    s->zone_maps = super->s_zmap_blocks;
    }

  s->inode_blocks = (s->inodes + inodes_per_block - 1) / inodes_per_block;
  s->first_data   = 2 + s->inode_maps + s->zone_maps + s->inode_blocks;
  if ( super->s_firstdatazone_old != 0 &&
  	s->first_data != super->s_firstdatazone_old )
  {
    if ( s->first_data > super->s_firstdatazone_old )
      Error( "Corrupted first data zone offset or inode count in super block" );
    else
      Warning( "First data zone in super block suspiciously high" );
    s->first_data = super->s_firstdatazone_old;
  }  

  s->inodes_in_map = s->inodes + 1;
  s->zones_in_map  = s->zones + 1 - s->first_data;

  /*
  if ( s->zones != s->device_size )
    Warning( "Zone count does not equal device size" );
  */

  s->device_size = s->zones;

  if ( super->s_log_zone_size != 0 )
    Error( "Can not handle multiple blocks per zone" );
}






/****************************************************************/
/*								*/
/*	Read_Bit_Maps( state )					*/
/*								*/
/*		Read in the i-node and zone bit maps from the	*/
/*		specified file system device.			*/
/*								*/
/****************************************************************/


void Read_Bit_Maps( s )
  de_state *s;

  {
  int i;

  if ( s->inode_maps > I_MAP_SLOTS  ||  s->zone_maps > Z_MAP_SLOTS )
    {
    Warning( "Super block specifies too many bit map blocks" );
    return;
    }

  for ( i = 0;  i < s->inode_maps;  ++i )
    {
    Read_Disk( s, (long) (2 + i) * K,
	       (char *) &s->inode_map[ i * K / sizeof (bitchunk_t ) ] );
    }

  for ( i = 0;  i < s->zone_maps;  ++i )
    {
    Read_Disk( s, (long) (2 + s->inode_maps + i) * K,
	       (char *) &s->zone_map[ i * K / sizeof (bitchunk_t ) ] );
    }
  }






/****************************************************************/
/*								*/
/*	Search( state, string )					*/
/*								*/
/*		Search from the current address for the ASCII	*/
/*		"string" on the device.				*/
/*								*/
/****************************************************************/


off_t Search( s, string )
  de_state *s;
  char *string;

  {
  off_t address   = s->address + 1;
  off_t last_addr = address;
  char  buffer[ SEARCH_BUFFER ];
  int   offset;
  int   tail_length = strlen( string ) - 1;
  int   count = SEARCH_BUFFER;
  int   last_offset;


  for (  ;  count == SEARCH_BUFFER;  address += SEARCH_BUFFER - tail_length )
    {
    if ( lseek( s->device_d, address, SEEK_SET ) == -1 )
      Error( "Error seeking %s", s->device_name );

    if ( (count = read( s->device_d, buffer, SEARCH_BUFFER)) == -1 )
      Error( "Error reading %s", s->device_name );


    if ( address - last_addr >= 500L * K )
      {
      putchar( '.' );
      fflush( stdout );

      last_addr += 500L * K;
      }


    last_offset = count - tail_length;

    for ( offset = 0;  offset < last_offset;  ++offset )
      {
      register char c = buffer[ offset ];

      if ( c == *string )
	{
	char *tail_buffer = &buffer[ offset + 1 ];
	char *tail_string = string + 1;

	do
	  {
	  if ( *tail_string == '\0' )
	    return( address + offset );
	  }
          while ( *tail_buffer++ == *tail_string++ );
        }
      }  /*  end for ( offset )  */
    }  /*  end for ( address )  */

  return( -1L );
  }






/****************************************************************/
/*								*/
/*	Write_Word( state, word )				*/
/*								*/
/*		Write a word at address.			*/
/*								*/
/****************************************************************/


void Write_Word( s, word )
  de_state *s;
  word_t word;

  {
  if ( s->address & 01 )
    Error( "Internal fault (unaligned address)" );

  if ( lseek( s->device_d, s->address, SEEK_SET ) == -1 )
    Error( "Error seeking %s", s->device_name );

  if ( write( s->device_d, (char *) &word, sizeof word ) != sizeof word )
    Error( "Error writing %s", s->device_name );
  }
