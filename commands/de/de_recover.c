/****************************************************************/
/*								*/
/*	de_recover.c						*/
/*								*/
/*		File restoration routines.			*/
/*								*/
/****************************************************************/
/*  origination         1989-Jan-21        Terrence W. Holm	*/
/*  handle "holes"	1989-Jan-28	   Terrence W. Holm	*/
/****************************************************************/


#include <minix/config.h>
#include <sys/types.h>
#include <sys/dir.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include <pwd.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>

#include <minix/const.h>
#include <minix/type.h>
#include "mfs/const.h"
#include "mfs/type.h"
#include "mfs/inode.h"
#include <minix/fslib.h>

#include "de.h"

_PROTOTYPE(int Indirect, (de_state *s, zone_t block, off_t *size, int dblind));
_PROTOTYPE(int Data_Block, (de_state *s, zone_t block, off_t *file_size ));
_PROTOTYPE(int Free_Block, (de_state *s, zone_t block ));




/****************************************************************/
/*								*/
/*	Path_Dir_File( path_name, dir_name, file_name )		*/
/*								*/
/*		Split "path_name" into a directory name and	*/
/*		a file name.					*/
/*								*/
/*		Zero is returned on error conditions.		*/
/*								*/
/****************************************************************/


int Path_Dir_File( path_name, dir_name, file_name )
  char  *path_name;
  char **dir_name;
  char **file_name;

  {
  char *p;
  static char directory[ MAX_STRING + 1 ];
  static char filename[ MAX_STRING + 1 ];


  if ( (p = strrchr( path_name, '/' )) == NULL )
    {
    strcpy( directory, "." );
    strcpy( filename, path_name );
    }
  else
    {
    *directory = '\0';
    strncat( directory, path_name, p - path_name );
    strcpy( filename, p + 1 );
    }

  if ( *directory == '\0' )
    strcpy( directory, "/" );

  if ( *filename == '\0' )
    {
    Warning( "A file name must follow the directory name" );
    return( 0 );
    }

  *dir_name  = directory;
  *file_name = filename;

  return( 1 );
  }






/****************************************************************/
/*								*/
/*	File_Device( file_name )				*/
/*								*/
/*		Return the name of the file system device	*/
/*		containing the file "file_name".		*/
/*								*/
/*		This is used if the "-r" option was specified.	*/
/*		In this case we have only been given a file	*/
/*		name, and must determine which file system	*/
/*		device to open.					*/
/*								*/
/*		NULL is returned on error conditions.		*/
/*								*/
/****************************************************************/



char *File_Device( file_name )
  char *file_name;

  {
  struct stat file_stat;
  struct stat device_stat;
  int dev_d;
  struct direct entry;
  static char device_name[ NAME_MAX + 1 ];


  if ( access( file_name, R_OK ) != 0 )
    {
    Warning( "Can not find %s", file_name );
    return( NULL );
    }


  if ( stat( file_name, &file_stat ) == -1 )
    {
    Warning( "Can not stat(2) %s", file_name );
    return( NULL );
    }


  /*  Open /dev for reading  */

  if ( (dev_d = open( DEV, O_RDONLY )) == -1 )
    {
    Warning( "Can not read %s", DEV );
    return( NULL );
    }


  while ( read( dev_d, (char *) &entry, sizeof(struct direct) )
				     == sizeof(struct direct) )
    {
    if ( entry.d_ino == 0 )
      continue;

    strcpy( device_name, DEV );
    strcat( device_name, "/" );
    strncat( device_name, entry.d_name, NAME_MAX );

    if ( stat( device_name, &device_stat ) == -1 )
      continue;

    if ( (device_stat.st_mode & S_IFMT) != S_IFBLK )
      continue;

    if ( file_stat.st_dev == device_stat.st_rdev )
      {
      close( dev_d );
      return( device_name );
      }
    }

  close( dev_d );

  Warning( "The device containing file %s is not in %s", file_name, DEV );

  return( NULL );
  }






/****************************************************************/
/*								*/
/*	Find_Deleted_Entry( state, path_name )			*/
/*								*/
/*		Split "path_name" into a directory name and	*/
/*		a file name. Then search the directory for	*/
/*		an entry that would match the deleted file	*/
/*		name. (Deleted entries have a zero i-node	*/
/*		number, but the original i-node number is 	*/
/*		placed at the end of the file name.)		*/
/*								*/
/*		If successful an i-node number is returned,	*/
/*		else zero is returned.				*/
/*								*/
/****************************************************************/


ino_t Find_Deleted_Entry( s, path_name )
  de_state *s;
  char *path_name;

  {
  char *dir_name;
  char *file_name;


  /*  Check if the file exists  */

  if ( access( path_name, F_OK ) == 0 )
    {
    Warning( "File has not been deleted" );
    return( 0 );
    }


  /*  Split the path name into a directory and a file name  */

  if ( ! Path_Dir_File( path_name, &dir_name, &file_name ) )
    return( 0 );


  /*  Check to make sure the user has read permission on  */
  /*  the directory.					  */

  if ( access( dir_name, R_OK ) != 0 )
    {
    Warning( "Can not find %s", dir_name );
    return( 0 );
    }


  /*  Make sure "dir_name" is really a directory. */
  {
  struct stat dir_stat;

  if ( stat( dir_name, &dir_stat ) == -1   ||
		 (dir_stat.st_mode & S_IFMT) != S_IFDIR )
    {
    Warning( "Can not find directory %s", dir_name );
    return( 0 );
    }
  }


  /*  Make sure the directory is on the current  */
  /*  file system device.                        */

  if ( Find_Inode( s, dir_name ) == 0 )
    return( 0 );


  /*  Open the directory and search for the lost file name.  */
  {
  int   dir_d;
  int   count;
  struct direct entry;

  if ( (dir_d = open( dir_name, O_RDONLY )) == -1 )
    {
    Warning( "Can not read directory %s", dir_name );
    return( 0 );
    }

  while ( (count = read( dir_d, (char *) &entry, sizeof(struct direct) ))
					      == sizeof(struct direct) )
    {
    if ( entry.d_ino == 0  &&
	strncmp( file_name, entry.d_name, NAME_MAX - sizeof(ino_t) ) == 0 )
      {
      ino_t inode = *( (ino_t *) &entry.d_name[ NAME_MAX - sizeof(ino_t) ] );

      close( dir_d );

      if ( inode < 1  || inode > s->inodes )
    	{
    	Warning( "Illegal i-node number" );
    	return( 0 );
    	}

      return( inode );
      }
    }

  close( dir_d );

  if ( count == 0 )
    Warning( "Can not find a deleted entry for %s", file_name );
  else
    Warning( "Problem reading directory %s", dir_name );

  return( 0 );
  }
  }






/****************************************************************/
/*								*/
/*	Recover_Blocks( state )					*/
/*								*/
/*		Try to recover all the blocks for the i-node	*/
/*		currently pointed to by "s->address". The	*/
/*		i-node and all of the blocks must be marked	*/
/*		as FREE in the bit maps. The owner of the	*/
/*		i-node must match the current real user name.	*/
/*								*/
/*		"Holes" in the original file are maintained.	*/
/*		This allows moving sparse files from one device	*/
/*		to another.					*/
/*								*/
/*		On any error -1L is returned, otherwise the	*/
/*		size of the recovered file is returned.		*/
/*								*/
/*								*/
/*		NOTE: Once a user has read access to a device,	*/
/*		there is a security hole, as we lose the	*/
/*		normal file system protection. For convenience,	*/
/*		de(1) is sometimes set-uid root, this allows	*/
/*		anyone to use the "-r" option. When recovering,	*/
/*		Recover_Blocks() can only superficially check	*/
/*		the validity of a request.			*/
/*								*/
/****************************************************************/


off_t Recover_Blocks( s )
  de_state *s;

  {
  struct inode core_inode;
  d1_inode *dip1;
  d2_inode *dip2;
  struct inode *inode = &core_inode;
  bit_t node = (s->address - (s->first_data - s->inode_blocks) * K) /
		s->inode_size + 1;

  dip1 = (d1_inode *) &s->buffer[ s->offset & ~ PAGE_MASK ];
  dip2 = (d2_inode *) &s->buffer[ s->offset & ~ PAGE_MASK
					    & ~ (V2_INODE_SIZE-1) ];
  conv_inode( inode, dip1, dip2, READING, s->magic );

  if ( s->block < s->first_data - s->inode_blocks  ||
	    s->block >= s->first_data )
    {
    Warning( "Not in an inode block" );
    return( -1L );
    }


  /*  Is this a valid, but free i-node?  */

  if ( node > s->inodes )
    {
    Warning( "Not an inode" );
    return( -1L );
    }

  if ( In_Use(node, s->inode_map) )
    {
    Warning( "I-node is in use" );
    return( -1L );
    }


  /*  Only recover files that belonged to the real user.  */

  {
  uid_t real_uid = getuid();
  struct passwd *user = getpwuid( real_uid );

  if ( real_uid != SU_UID  &&  real_uid != inode->i_uid )
    {
    Warning( "I-node did not belong to user %s", user ? user->pw_name : "" );
    return( -1L );
    }
  }


  /*  Recover all the blocks of the file.  */

  {
  off_t file_size = inode->i_size;
  int i;


  /*  Up to s->ndzones pointers are stored in the i-node.  */

  for ( i = 0;  i < s->ndzones;  ++i )
    {
    if ( file_size == 0 )
	return( inode->i_size );

    if ( ! Data_Block( s, inode->i_zone[ i ], &file_size ) )
      return( -1L );
    }

  if ( file_size == 0 )
    return( inode->i_size );


  /*  An indirect block can contain up to inode->i_indirects more blk ptrs.  */

  if ( ! Indirect( s, inode->i_zone[ s->ndzones ], &file_size, 0 ) )
    return( -1L );

  if ( file_size == 0 )
    return( inode->i_size );


  /*  A double indirect block can contain up to inode->i_indirects blk ptrs. */

  if ( ! Indirect( s, inode->i_zone[ s->ndzones+1 ], &file_size, 1 ) )
    return( -1L );

  if ( file_size == 0 )
    return( inode->i_size );

  Error( "Internal fault (file_size != 0)" );
  }

  /* NOTREACHED */
  return( -1L );
  }






/*  Indirect( state, block, &file_size, double )
 *
 *  Recover all the blocks pointed to by the indirect block
 *  "block",  up to "file_size" bytes. If "double" is true,
 *  then "block" is a double-indirect block pointing to
 *  V*_INDIRECTS indirect blocks.
 *
 *  If a "hole" is encountered, then just seek ahead in the
 *  output file.
 */


int Indirect( s, block, file_size, dblind )
  de_state *s;
  zone_t   block;
  off_t    *file_size;
  int       dblind;

  {
  union
    {
    zone1_t ind1[ V1_INDIRECTS ];
    zone_t  ind2[ V2_INDIRECTS(_MAX_BLOCK_SIZE) ];
    } indirect;
  int  i;
  zone_t zone;

  /*  Check for a "hole".  */

  if ( block == NO_ZONE )
    {
    off_t skip = (off_t) s->nr_indirects * K;

    if ( *file_size < skip  ||  dblind )
      {
      Warning( "File has a hole at the end" );
      return( 0 );
      }

    if ( fseek( s->file_f, skip, SEEK_CUR ) == -1 )
      {
      Warning( "Problem seeking %s", s->file_name );
      return( 0 );
      }

    *file_size -= skip;
    return( 1 );
    }


  /*  Not a "hole". Recover indirect block, if not in use.  */

  if ( ! Free_Block( s, block ) )
    return( 0 );


  Read_Disk( s, (long) block << K_SHIFT, (char *) &indirect );

  for ( i = 0;  i < s->nr_indirects;  ++i )
    {
    if ( *file_size == 0 )
	return( 1 );

    zone = (s->v1 ? indirect.ind1[ i ] : indirect.ind2[ i ]);
    if ( dblind )
      {
      if ( ! Indirect( s, zone, file_size, 0 ) )
	return( 0 );
      }
    else
      {
      if ( ! Data_Block( s, zone, file_size ) )
        return( 0 );
      }
    }

  return( 1 );
  }






/*  Data_Block( state, block, &file_size )
 *
 *  If "block" is free then write  Min(file_size, k)
 *  bytes from it onto the current output file.
 *
 *  If "block" is zero, this means that a 1k "hole"
 *  is in the file. The recovered file maintains
 *  the reduced size by not allocating the block.
 *
 *  The file size is decremented accordingly.
 */


int Data_Block( s, block, file_size )
  de_state *s;
  zone_t   block;
  off_t    *file_size;

  {
  char buffer[ K ];
  off_t block_size = *file_size > K ? K : *file_size;


  /*  Check for a "hole".  */

  if ( block == NO_ZONE )
    {
    if ( block_size < K )
      {
      Warning( "File has a hole at the end" );
      return( 0 );
      }

    if ( fseek( s->file_f, block_size, SEEK_CUR ) == -1 )
      {
      Warning( "Problem seeking %s", s->file_name );
      return( 0 );
      }

    *file_size -= block_size;
    return( 1 );
    }


  /*  Block is not a "hole". Copy it to output file, if not in use.  */

  if ( ! Free_Block( s, block ) )
    return( 0 );

  Read_Disk( s, (long) block << K_SHIFT, buffer );


  if ( fwrite( buffer, 1, (size_t) block_size, s->file_f )
       != (size_t) block_size )
    {
    Warning( "Problem writing %s", s->file_name );
    return( 0 );
    }

  *file_size -= block_size;
  return( 1 );
  }






/*  Free_Block( state, block )
 *
 *  Make sure "block" is a valid data block number, and it
 *  has not been allocated to another file.
 */


int Free_Block( s, block )
  de_state *s;
  zone_t  block;

  {
  if ( block < s->first_data  ||  block >= s->zones )
    {
    Warning( "Illegal block number" );
    return( 0 );
    }

  if ( In_Use( (bit_t) (block - (s->first_data - 1)), s->zone_map ) )
    {
    Warning( "Encountered an \"in use\" data block" );
    return( 0 );
    }

  return( 1 );
  }

