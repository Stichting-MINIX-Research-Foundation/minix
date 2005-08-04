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
#if	GNU_SUPPORT
#include <gnu/a.out.h>
#endif
#include "proto.h"

#if	GNU_SUPPORT
_PROTOTYPE( PUBLIC unsigned int gnu_load, (char *filename, struct nlist **start) );
#endif

struct symtab_s
{
	struct nlist *start;
	struct nlist *end;
	unsigned int nsym;
};

PRIVATE struct symtab_s symtab;

FORWARD _PROTOTYPE( void gnu_sort , (struct nlist *array , struct nlist *top ));
FORWARD _PROTOTYPE( int gnu_symeq , (char *t , struct nlist *sp ));
FORWARD _PROTOTYPE( int gnu_symprefix , (char *t , struct nlist *sp ));
FORWARD _PROTOTYPE( struct nlist *gnu_sname, (char *name, int is_text, int allflag) );
FORWARD _PROTOTYPE( struct nlist *gnu_sval, (off_t value, int where) );
FORWARD _PROTOTYPE( void gnu_sym, (struct nlist *sp, off_t off) );

PUBLIC void gnu_init( filename )
char *filename;
{
	register struct symtab_s *tp;

	tp = &symtab;
		
	tp->nsym = gnu_load(filename, &tp->start);
	tp->end = tp->start + tp->nsym;

	/* sort on value only, name search not used much and storage a problem */
	Printf("Sorting %d GNU symbols ....", tp->nsym );
	gnu_sort( tp->start, tp->end );
	Printf("\n");
}



PUBLIC long gnu_symbolvalue( name, is_text )
char *name;
int is_text;
{
  register struct nlist *sp;
  sp = gnu_sname(name,is_text,0);
  if (sp != NULL)  
	return sp->n_value;
  else
	return 0L;
}


PRIVATE struct nlist *gnu_sname( name, is_text, allflag )
char *name;
int is_text;
int allflag;
{
	char *s;
	unsigned char sclass;
	int schar;
	char *send;
	register struct nlist *sp;
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
				switch( sp->n_type & N_TYPE )
				{
					case N_ABS: schar = 'a'; break;
					case N_TEXT: schar = 't'; break;
					case N_DATA: schar = 'd'; break;
					case N_BSS: schar = 'b'; break;
					default: schar = '?'; break;
				}
				if ( (sp->n_type & N_EXT) && schar != '?' )
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
			sclass = sp->n_type & N_TYPE;
			if ( (is_text && sclass == N_TEXT ||
			      !is_text && (sclass == N_DATA || sclass == N_BSS)) &&
					 gnu_symeq( name, sp ) )
				return sp;
		}
	}
	return NULL;
}

PRIVATE struct nlist *gnu_sval( value, where )
off_t value;
int where;
{
	int left;
	int middle;
	int right;
	unsigned char sclass;
	register struct nlist *sp;
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
			if ( !(sp->n_type & N_EXT) ) continue; 
			sclass = sp->n_type & N_TYPE;
			if ( (where == CSEG && sclass == N_TEXT ||
						where != CSEG && (sclass == N_DATA || sclass == N_BSS)) )
			return sp;
		}
	return NULL;
}


PRIVATE void gnu_sym( sp, off )
struct nlist *sp;
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

PRIVATE void gnu_sort( array, top )
struct nlist *array;
struct nlist *top;
{
	int gap;
	int i;
	int j;
	register struct nlist *left;
	register struct nlist *right;
	struct nlist swaptemp;
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

PUBLIC void gnu_symbolic( value, separator )
off_t value;
int separator;
{
	register struct nlist *sp;
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


PRIVATE int gnu_symeq( t, sp )
register char *t;
struct nlist *sp;
{
	return strncmp( t, sp->n_un.n_name, strlen(t) ) == 0;
}

PRIVATE int gnu_symprefix( t, sp )
register char *t;
struct nlist *sp;
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

PUBLIC void gnu_listsym( tchar )
char tchar;
{
	register struct symtab_s *tp;
	register struct nlist *sp;
	char *s;
	char *send;
	char schar;

	outbyte('\n');	
	tp = &symtab;
    	for ( sp = tp->start; sp < tp->end; ++sp )
	{
	     switch( sp->n_type & N_TYPE )
	     {
			case N_ABS:	schar = 'a'; break;
			case N_TEXT:	schar = 't'; break;
			case N_DATA:	schar = 'd'; break;
			case N_BSS:	schar = 'b'; break;
			default: 	schar = '?'; break;
	     }

	     if ( (sp->n_type & N_EXT) && schar != '?' )
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

PUBLIC int gnu_text_symbol(value)
off_t value;
{
struct nlist *sp;

    if ((sp = gnu_sval(value, CSEG)) != NULL && sp->n_value == value)
    {
	gnu_sym(sp, value);
	return TRUE;
    }
    else
	return FALSE;
}

PUBLIC int gnu_finds_data(off,data_seg)
off_t off;
int data_seg;
{
struct nlist *sp;

	if ((sp = gnu_sval(off, data_seg)) != NULL)
   	{
	    gnu_sym(sp, off);
	    return TRUE;
    	}
    	else 
	    return FALSE;
}

PUBLIC int gnu_finds_pc(pc)
off_t pc;
{
struct nlist *sp;

	if ((sp = gnu_sval(pc, CSEG)) != NULL)
    	{
	    gnu_sym(sp, pc);
	    return TRUE;
        }
	else
	    return FALSE;
}


#endif /* EXTRA_SYMBOLS */
