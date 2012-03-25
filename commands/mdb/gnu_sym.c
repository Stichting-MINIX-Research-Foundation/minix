/* 
 * gnu_sym.c for mdb 
 * copied and modified from sym.c
 * Support GNU Exec symbol tables
 */

#include "mdb.h"

#ifdef	EXTRA_SYMBOLS

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#ifdef __NBSD_LIBC
#include <compat/a.out.h>
#else
#include <minix/a.out.h>
#endif
#include "proto.h"

#define NN_UNDF 0
#define NN_ABS 2
#define NN_TEXT 4
#define NN_DATA 6
#define NN_BSS 8
#define NN_FN 15
#define NN_EXT 1
#define NN_TYPE 036

struct newnlist {
	union {
		char *n_name;
		struct newnlist *n_next;
		long n_strx;
	} n_un;
	unsigned char n_type;
	char n_other;
	short n_desc;
	unsigned long n_value;
};

struct symtab_s
{
	struct newnlist *start;
	struct newnlist *end;
	unsigned int nsym;
};

static struct symtab_s symtab;

static void gnu_sort(struct newnlist *array , struct newnlist *top );
static int gnu_symeq(char *t , struct newnlist *sp );
static int gnu_symprefix(char *t , struct newnlist *sp );
static struct newnlist *gnu_sname(char *name, int is_text, int
	allflag);
static struct newnlist *gnu_sval(off_t value, int where);
static void gnu_sym(struct newnlist *sp, off_t off);


void gnu_init( filename )
char *filename;
{
        struct exec header;
	unsigned int string_size;
        char *names;
        struct newnlist *p;
	int fd;
	register struct symtab_s *tp;

	tp = &symtab;
	if ( (fd = open( filename, O_RDONLY)) < 0 ||
	     read( fd, (char *) &header, sizeof header ) != sizeof header )
	{
                do_error( "gnu_load - reading header" );
                if ( fd >= 0) close( fd );
		return;
        }

	if ( (string_size = lseek( fd, 0, SEEK_END ) ) == -1 )
	{
		do_error( "gnu_load - determining file size" );
		close( fd );
		return;
	}

	string_size -= A_SYMPOS( header );

        if ( (int) header.a_syms < 0 || 
		(unsigned) header.a_syms != header.a_syms ||
             (tp->start = (struct newnlist *) malloc( string_size ))
                                == (struct newnlist *) NULL &&
             header.a_syms != 0 )
        {
		do_error( "gnu_load - allocating memory" );
                close( fd );
                return;
        }

        if ( lseek( fd, A_SYMPOS( header ), SEEK_SET ) != A_SYMPOS( header ) )
        {
                do_error( "gnu_load - reading header" );
                close( fd );
		return;
        }

        if ( read( fd, (char *) tp->start, string_size ) < 0 )
        {
                do_error( "gnu_load - reading symbols" );
                close( fd );
                return;
        }
        close( fd );

        tp->nsym = (unsigned int) header.a_syms / sizeof (struct newnlist);
	tp->end = tp->start + tp->nsym;

        names = (char *) tp->start + header.a_syms;
	
        for ( p = tp->start; p < tp->end; p++) 
                if(p->n_un.n_strx)
                        p->n_un.n_name = names + p->n_un.n_strx;
                else
                        p->n_un.n_name = "";

	/* sort on value only, name search not used much and storage a problem */
	Printf("Sorting %d GNU symbols ....", tp->nsym );
	gnu_sort( tp->start, tp->end );
	Printf("\n");
}


long gnu_symbolvalue( name, is_text )
char *name;
int is_text;
{
  register struct newnlist *sp;
  sp = gnu_sname(name,is_text,0);
  if (sp != NULL)  
	return sp->n_value;
  else
	return 0L;
}


static struct newnlist *gnu_sname( name, is_text, allflag )
char *name;
int is_text;
int allflag;
{
	char *s;
	unsigned char sclass;
	int schar;
	char *send;
	register struct newnlist *sp;
	register struct symtab_s *tp;

	tp = &symtab;

	if ( allflag )
	{
		/* find and print all matching symbols */
		for ( sp = tp->start; sp < tp->end; ++sp )
		{
			if ( gnu_symprefix( name, sp ) )
			{
				sp = sp;
				for ( s = sp->n_un.n_name, send = s + strlen(s);
				      *s != 0 && s < send; ++s )
					outbyte( *s );
				for ( ; s <= send; ++s )
					outspace();
				switch( sp->n_type & NN_TYPE )
				{
					case NN_ABS: schar = 'a'; break;
					case NN_TEXT: schar = 't'; break;
					case NN_DATA: schar = 'd'; break;
					case NN_BSS: schar = 'b'; break;
					default: schar = '?'; break;
				}
				if ( (sp->n_type & NN_EXT) && schar != '?' )
					schar += 'A' - 'a';
				outbyte( schar );
				outspace();
				outh32( sp->n_value );
				outbyte('\n');
			}
		}
	}
	else
	{
		/* find symbol by dumb linear search */
		for ( sp = tp->start; sp < tp->end; ++sp )
		{
			sclass = sp->n_type & NN_TYPE;
			if ( (is_text && sclass == NN_TEXT ||
			      !is_text && (sclass == NN_DATA ||
			                   sclass == NN_BSS)) &&
					 gnu_symeq( name, sp ) )
				return sp;
		}
	}
	return NULL;
}

static struct newnlist *gnu_sval( value, where )
off_t value;
int where;
{
	int left;
	int middle;
	int right;
	unsigned char sclass;
	register struct newnlist *sp;
	register struct symtab_s *tp;

	tp = &symtab;

	/* find last symbol with value <= desired one by binary search */
	for ( left = 0, right = tp->nsym - 1; left <= right; )
	{
		middle = (left + right) / 2;
		sp = tp->start + middle;
		if ( value < sp->n_value )
			right = middle - 1;
		else
			left = middle + 1;
	}
	if ( right >= 0 )
		/* otherwise tp->start + right may wrap around to > tp->start !! */
		for ( sp = tp->start + right; sp >= tp->start; --sp )
		{
			if ( !(sp->n_type & NN_EXT) ) continue; 
			sclass = sp->n_type & NN_TYPE;
			if ( (where == CSEG && sclass == NN_TEXT) ||
						(where != CSEG &&
				(sclass == NN_DATA || sclass == NN_BSS)) )
			return sp;
		}
	return NULL;
}


static void gnu_sym( sp, off )
struct newnlist *sp;
off_t off;
{
	register char *s;
	char *send;

	for ( s = sp->n_un.n_name, send = s + strlen(s); *s != 0 && s < send; ++s )
		outbyte( *s );
	if ( (off -= sp->n_value) != 0 )
	{
		outbyte( '+' );
		printhex(off);
	}
}

/* shell sort symbols on value */

static void gnu_sort( array, top )
struct newnlist *array;
struct newnlist *top;
{
	int gap;
	int i;
	int j;
	register struct newnlist *left;
	register struct newnlist *right;
	struct newnlist swaptemp;
	int size;

	size = top - array;
	/* choose gaps according to Knuth V3 p95 */
	for ( gap = 1, i = 4; (j = 3 * i + 1) < size; gap = i, i = j )
		;
	do
	{
		for ( j = gap; j < size; ++j )
			for ( i = j - gap; i >= 0; i -= gap )
			{
				left = array + i; 
				right = array + (i + gap);
				if ( (off_t) left->n_value <=
				     right->n_value )
					break;
				swaptemp = *left;
				*left = *right;
				*right = swaptemp;
			}
	}
	while ( (gap /= 3) != 0 );
}

void gnu_symbolic( value, separator )
off_t value;
int separator;
{
	register struct newnlist *sp;
	long off;

	if (value < st_addr || value > end_addr) {
		outstr("0x");
		printhex(value);
		outbyte(separator);
		return;
	}

	if ( (sp = gnu_sval( value, CSEG )) != NULL )
	{
		gnu_sym( sp, value );
	}
	else if ( (sp = gnu_sval( value, DSEG )) != NULL )
	{
		gnu_sym( sp, value );
	}
	else
	{
		outstr("_start");
		off = value - st_addr; 
		if ( off != 0 )  
		{
		outbyte( '+' );
		printhex(off);
		}
	}
	outbyte( separator );
}


static int gnu_symeq( t, sp )
register char *t;
struct newnlist *sp;
{
	return strncmp( t, sp->n_un.n_name, strlen(t) ) == 0;
}

static int gnu_symprefix( t, sp )
register char *t;
struct newnlist *sp;
{
	register char *s;
	char *send;

	for ( ; *t == '_'; ++t )
		;
	for ( s = sp->n_un.n_name, send = s + strlen(s);
	      s < send && *s == '_'; ++s )
		;
	return strncmp( s, t, send - s ) == 0;
}



/* list all symbols - test for selection criteria */

void gnu_listsym( tchar )
char tchar;
{
	register struct symtab_s *tp;
	register struct newnlist *sp;
	char *s;
	char *send;
	char schar;

	outbyte('\n');	
	tp = &symtab;
    	for ( sp = tp->start; sp < tp->end; ++sp )
	{
	     switch( sp->n_type & NN_TYPE )
	     {
			case NN_ABS:	schar = 'a'; break;
			case NN_TEXT:	schar = 't'; break;
			case NN_DATA:	schar = 'd'; break;
			case NN_BSS:	schar = 'b'; break;
			default: 	schar = '?'; break;
	     }

	     if ( (sp->n_type & NN_EXT) && schar != '?' )
		schar += 'A' - 'a';

	     /* check for selection */	
	     if ( tchar != '*' && schar != tchar)
		continue; 	

	     /* print symbol type and value */	
	     outh32( sp->n_value );
	     outspace();
	     outbyte( schar );
	     outbyte( '\t' );	
	     for ( s = sp->n_un.n_name, send = s + strlen(s);
		 *s != 0 && s < send; ++s ) outbyte( *s );
	     outbyte('\n');	
	}
}

int gnu_text_symbol(value)
off_t value;
{
    struct newnlist *sp;

    if ((sp = gnu_sval(value, CSEG)) != NULL && sp->n_value == value)
    {
	gnu_sym(sp, value);
	return TRUE;
    }
    else
	return FALSE;
}

int gnu_finds_data(off,data_seg)
off_t off;
int data_seg;
{
	struct newnlist *sp;

	if ((sp = gnu_sval(off, data_seg)) != NULL)
   	{
	    gnu_sym(sp, off);
	    return TRUE;
    	}
    	else 
	    return FALSE;
}

int gnu_finds_pc(pc)
off_t pc;
{
	struct newnlist *sp;

	if ((sp = gnu_sval(pc, CSEG)) != NULL)
    	{
	    gnu_sym(sp, pc);
	    return TRUE;
        }
	else
	    return FALSE;
}


#endif /* EXTRA_SYMBOLS */
