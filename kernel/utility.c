/* This file contains a collection of miscellaneous procedures:
 *   panic	    abort MINIX due to a fatal error
 *   safe_lock	    lock the kernel, use in combination with safe_unlock
 *   safe_unlock    unlock the kernel, but prevent breaking nested locks
 *   alloc_bit      bit map manipulation
 *   free_bit       bit map manipulation
 */

#include "kernel.h"
#include "assert.h"
#include <unistd.h>
#include <minix/com.h>


PRIVATE int relock_count = 0;

/*===========================================================================*
 *                                   safe_lock                               *
 *===========================================================================*/
PUBLIC void safe_lock(c,v)
int c;
char *v;
{
  if(!(read_cpu_flags() & X86_FLAG_I)) {
  	relock_count++;
  } else {
  	intr_disable();
  }
}

/*===========================================================================*
 *                                   safe_unlock                             *
 *===========================================================================*/
PUBLIC void safe_unlock(void)
{
  if(! relock_count) {
  	intr_enable();
  } else {
  	relock_count--;
  }
}

/*===========================================================================*
 *                                   panic                                   *
 *===========================================================================*/
PUBLIC void panic(s,n)
_CONST char *s;
int n;
{
/* The system has run aground of a fatal kernel error. Terminate execution. */
  static int panicking = 0;
  if (panicking ++)		/* prevent recursive panics */
  	return;

  if (s != NULL) {
	kprintf("\nKernel panic: %s", karg(s));
	if (n != NO_NUM) kprintf(" %d", n);
	kprintf("\n",NO_NUM);
  }
  prepare_shutdown(RBT_PANIC);
}




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
    return(-1);    
}

