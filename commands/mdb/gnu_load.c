/* 
 * gnu_load for mdb.c
 */

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <gnu/a.out.h>

_PROTOTYPE( unsigned int gnu_load, (char *filename, struct nlist **start) );
_PROTOTYPE( void do_error, (char *message) );

unsigned int gnu_load( filename, start)
char *filename;
struct nlist **start;
{
        struct exec header;
	unsigned int nsym, string_size;
        char *names;
        struct nlist *p;
	int fd;

	if ( (fd = open( filename, 0)) < 0 ||
	     read( fd, (char *) &header, sizeof header ) != sizeof header )
	{
                do_error( "gnu_load" );
                if ( fd >= 0) close( fd );
		return 0;
        }       

        if ( lseek( fd, N_STROFF( header ), 0 ) != N_STROFF( header ) )
        {
                do_error( "gnu_load - reading header" );
                close( fd );
		return 0;
        }

        if ( read( fd, (char *) &string_size, sizeof string_size ) < 0 )
        {
                do_error( "gnu_load - reading header" );
                close( fd );
		return 0;
        }
        
        if ( (int) header.a_syms < 0 || 
		(unsigned) header.a_syms != header.a_syms ||
             (*start = (struct nlist *) malloc( (unsigned) header.a_syms + 
                        string_size ))
                                == (struct nlist *) NULL &&
             header.a_syms != 0 )
        {
                close( fd );
                return 0;
        }

        lseek( fd, N_SYMOFF( header ), 0 );

        if ( read( fd, (char *) *start, (int) header.a_syms + string_size ) < 0 )
        {
                do_error( "gnu_load - reading symbols" );
                close( fd );
                return 0;
        }
        close( fd );

        nsym = (unsigned int) header.a_syms / sizeof (struct nlist);
        names = (char *) *start + header.a_syms;
	
        for ( p = *start; p < *start + nsym; p++) 
                if(p->n_un.n_strx)
                        p->n_un.n_name = names + p->n_un.n_strx;

	return nsym;
}
