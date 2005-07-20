/* This file contains a collection of miscellaneous procedures:
 *     panic:	        abort MINIX due to a fatal error
 *     kprintf:		diagnostic output for the kernel 
 *
 * Changes:
 *                      simple printing to circular buffer  (Jorrit N. Herder)
 * 
 * This file contains the routines that take care of kernel messages, i.e.,
 * diagnostic output within the kernel. Kernel messages are not directly
 * displayed on the console, because this must be done by the PRINT driver. 
 * Instead, the kernel accumulates characters in a buffer and notifies the
 * output driver when a new message is ready. 
 */

#include "kernel.h"
#include <unistd.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <signal.h>
#include <minix/com.h>

#define END_OF_KMESS 	-1
FORWARD _PROTOTYPE(void kputc, (int c));


/*===========================================================================*
 *                                   panic                                   *
 *===========================================================================*/
PUBLIC void panic(mess,nr)
_CONST char *mess;
int nr;
{
/* The system has run aground of a fatal kernel error. Terminate execution. */
  static int panicking = 0;
  if (panicking ++) return;		/* prevent recursive panics */

  if (mess != NULL) {
	kprintf("\nKernel panic: %s", mess);
	if (nr != NO_NUM) kprintf(" %d", nr);
	kprintf("\n",NO_NUM);
  }
  prepare_shutdown(RBT_PANIC);
}


/*===========================================================================*
 *				kprintf					     *
 *===========================================================================*/
PUBLIC void kprintf(const char *fmt, ...) 	/* format to be printed */
{
  int c;					/* next character in fmt */
  unsigned long u;				/* hold number argument */
  int base;					/* base of number arg */
  int negative = 0;				/* print minus sign */
  static char x2c[] = "0123456789ABCDEF";	/* nr conversion table */
  char ascii[8 * sizeof(long) / 3 + 2];		/* string for ascii number */
  char *s = NULL;				/* string to be printed */
  va_list argp;					/* optional arguments */
  
  va_start(argp, fmt);				/* init variable arguments */

  while((c=*fmt++) != 0) {

      if (c == '%') {				/* expect format '%key' */
          switch(c = *fmt++) {			/* determine what to do */

          /* Known keys are %d, %u, %x, %s, and %%. This is easily extended 
           * with number types like %b and %o by providing a different base.
           * Number type keys don't set a string to 's', but use the general
           * conversion after the switch statement.
           */ 
          case 'd':				/* output decimal */
              u = va_arg(argp, int);
              if (u < 0) { negative = 1; u = -u; }
              base = 10;
              break;
          case 'u':				/* output unsigned long */
              u = va_arg(argp, unsigned long);
              base = 10;
              break;
          case 'x':				/* output hexadecimal */
              u = va_arg(argp, unsigned long);
              base = 0x10;
              break;
          case 's': 				/* output string */
              s = va_arg(argp, char *);
              if (s == NULL) s = "(null)";
              break;
          case '%':				/* output percent */
              s = "%";				 
              break;			

          /* Unrecognized key. */
          default:				/* echo back %key */
              s = "%?";				
              s[1] = c;				/* set unknown key */
          }

          /* Assume a number if no string is set. Convert to ascii. */
          if (s == NULL) {
              s = ascii + sizeof(ascii)-1;
              *s = 0;			
              do {  *--s = x2c[(u % base)]; }	/* work backwards */
              while ((u /= base) > 0); 
          }

          /* This is where the actual output for format "%key" is done. */
          if (negative) kputc('-');  		/* print sign if negative */
          while(*s != 0) { kputc(*s++); }	/* print string/ number */
      }
      else {
          kputc(c);				/* print and continue */
      }
  }
  kputc(END_OF_KMESS);				/* terminate output */
  va_end(argp);					/* end variable arguments */
}


/*===========================================================================*
 *			            kputc				     *
 *===========================================================================*/
PRIVATE void kputc(c)
int c;					/* character to append */
{
/* Accumulate a single character for a kernel message. Send a notification
 * the to PRINTF_PROC driver if an END_OF_KMESS is encountered. 
 */
  if (c != END_OF_KMESS) {
      kmess.km_buf[kmess.km_next] = c;	/* put normal char in buffer */
      if (kmess.km_size < KMESS_BUF_SIZE)
          kmess.km_size += 1;		
      kmess.km_next = (kmess.km_next + 1) % KMESS_BUF_SIZE;
  } else {
      send_sig(PRINTF_PROC, SIGKMESS);
  }
}



#if TEMP_CODE

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

#endif
