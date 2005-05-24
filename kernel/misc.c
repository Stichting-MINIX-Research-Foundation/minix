/* This file contains a collection of miscellaneous procedures:
 *   panic	    abort MINIX due to a fatal error
 *   bad_assertion  for debugging
 *   bad_compare    for debugging
 *   alloc_bit      bit map manipulation
 *   free_bit       bit map manipulation
 *   print_bitmap   bit map manipulation
 */

#include "kernel.h"
#include "assert.h"
#include <unistd.h>
#include <minix/com.h>


/*===========================================================================*
 *                                   panic                                   *
 *===========================================================================*/
PUBLIC void panic(s,n)
_CONST char *s;
int n;
{
/* The system has run aground of a fatal error.  Terminate execution.
 * If the panic originated in MM or FS, the string will be empty and the
 * file system already syncked.  If the panic originates in the kernel, we are
 * kind of stuck.
 */
  static int panicking = 0;
  if (panicking ++)		/* prevent recursive panics */
  	return;

  if (s != NULL) {
	kprintf("\nKernel panic: %s", karg(s));
	if (n != NO_NUM) kprintf(" %d", n);
	kprintf("\n",NO_ARG);
  }
  prepare_shutdown(RBT_PANIC);
}

#if ENABLE_K_DEBUGGING
/*===========================================================================*
 *			   print_bitmap					     * 
 *===========================================================================*/
PUBLIC void print_bitmap(bitmap, nr_bits)
bitchunk_t *bitmap; 
bit_t nr_bits;
{
    bit_t bit_i;
    
    for (bit_i=0; bit_i < nr_bits; bit_i++) {

        kprintf("%d", GET_BIT(bitmap, bit_i) > 0 );
        if (! ((bit_i+1) % 8) )   kprintf(" ", NO_ARG);
        if (! ((bit_i+1) % 64) )   kprintf("\n", NO_ARG);
    }
    kprintf("\n", NO_ARG);
}
#endif /* ENABLE_K_DEBUGGING */

/*===========================================================================*
 *			   	free_bit				     * 
 *===========================================================================*/
PUBLIC void free_bit(bit_nr, bitmap, nr_bits) 
bit_t bit_nr;
bitchunk_t *bitmap;
bit_t nr_bits;
{
  bitchunk_t *chunk;
  if (bit_nr >= nr_bits) {
  	kprintf("Warning, free_bit: %d illegal index\n", bit_nr);
  	return;
  }
  chunk = &bitmap[(bit_nr/BITCHUNK_BITS)];
  *chunk &= ~(1 << (bit_nr % BITCHUNK_BITS));
}

/*===========================================================================*
 *			   	alloc_bit				     * 
 *===========================================================================*/
PUBLIC int alloc_bit(bitmap, nr_bits) 
bitchunk_t *bitmap;
bit_t nr_bits;
{
    bitchunk_t *chunk;
    int nr_chunks;
    int bit_nr;
    int i;
    
    /* Iterate over the words in block. */
    nr_chunks = BITMAP_CHUNKS(nr_bits);
    for (chunk = &bitmap[0]; chunk < &bitmap[nr_chunks]; chunk++) {

        /* Does this chunk contain a free bit? */
        if (*chunk == (bitchunk_t) ~0) continue;
        
        /* Get bit number from the start of the bit map. */
        for (i = 0; (*chunk & (1 << i)) != 0; ++i) {}
        bit_nr = (chunk - &bitmap[0]) * BITCHUNK_BITS + i;
        
        /* Don't allocate bits beyond the end of the map. */
        if (bit_nr >= nr_bits) break;

        *chunk |= 1 << bit_nr % BITCHUNK_BITS;
        return(bit_nr);        
        
    }
    kprintf("Warning, all %d bits in map busy\n", nr_bits);
    return(-1);    
}




#if !NDEBUG
/*=========================================================================*
 *				bad_assertion				   *
 *=========================================================================*/
PUBLIC void bad_assertion(file, line, what)
char *file;
int line;
char *what;
{
  kprintf("panic at %s", karg(file));
  kprintf(" (line %d): ", line);
  kprintf("assertion \"%s\" failed.\n", karg(what));
  panic(NULL, NO_NUM);
}

/*=========================================================================*
 *				bad_compare				   *
 *=========================================================================*/
PUBLIC void bad_compare(file, line, lhs, what, rhs)
char *file;
int line;
int lhs;
char *what;
int rhs;
{
  kprintf("panic at %s", karg(file));
  kprintf(" (line %d): ", line);
  kprintf("compare (%d)", lhs);
  kprintf(" %s ", karg(what));
  kprintf("(%d) failed.\n", rhs);
  panic(NULL, NO_NUM);
}
#endif /* !NDEBUG */
