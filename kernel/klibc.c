/* This file contains simplified versions of the standard libary functions for
 * use with the kernel. This way the kernel sources remain separate from user
 * sources and can easily be verified. Note that the functionality provided
 * can be slightly different.  
 *				                March 2005, Jorrit N. Herder.
 * Entrypoints into this file:
 *     katoi:		convert string to integer
 *     kmemcpy:		copy n bytes from pointer p1 to pointer p2
 *     kmemset:		set n bytes to c starting at pointer p
 *     kprintf:		printf for the kernel (see working below) 
 *     kstrcmp:		lexicographical comparison of two strings
 *     kstrlen:		get number of non-null characters in string
 *     kstrncpy:	copy string and pad or copy up to n chars 
 *
 * This file contains the routines that take care of kernel messages, i.e.,
 * diagnostic output within the kernel. Kernel messages are not directly
 * displayed on the console, because this must be done by the PRINT driver. 
 * Instead, the kernel accumulates characters in a buffer and notifies the
 * PRINT driver when a new message is ready. 
 */

#include "kernel.h"

#include <minix/com.h>

#define isdigit(c)	((unsigned) ((c) - '0') <  (unsigned) 10)
#define END_OF_KMESS 	-1
FORWARD _PROTOTYPE(void kputc, (int c));


/*=========================================================================*
 *				katoi					   *
 *=========================================================================*/
PUBLIC int katoi(register const char *s)
{
  int value = 0;				/* default value */
  int sign = 1;					/* assume positive */

  while(*s == ' ') s++;				/* skip spaces */
  if (*s == '-') { sign = -1; s++; }		/* detect sign */
  while(isdigit(*s))				/* get integer */
      value = value*10 + (*s++) -'0';

  return(sign * value); 			/* return result */
}


/*=========================================================================*
 *				kmemcpy					   *
 *=========================================================================*/
PUBLIC void *kmemcpy(void *s1, const void *s2, register size_t n)
{
  register char *p1 = s1;
  register const char *p2 = s2;

  while (n-- > 0) 
      *p1++ = *p2++;
  return s1;
}


/*=========================================================================*
 *				kmemset		 			   *
 *=========================================================================*/
PUBLIC void *kmemset(void *s, register int c, register size_t n)
{
  register char *s1 = s;
  if (n++>0) {			/* optimized for speed */
      while (--n > 0) 
          *s1++ = c;
  }
  return s;
}


/*===========================================================================*
 *				kprintf					     *
 *===========================================================================*/
PUBLIC void kprintf(fmt, arg)
const char *fmt;		/* format string to be printed */
karg_t arg;			/* argument for format string */
{
  int c;					/* next character in fmt */
  unsigned long u;				/* hold number argument */
  int base;					/* base of number arg */
  int negative = 0;				/* print minus sign */
  static char x2c[] = "0123456789ABCDEF";	/* nr conversion table */
  char ascii[8 * sizeof(long) / 3 + 2];		/* string for ascii number */
  char *s = NULL;				/* string to be printed */

  while((c=*fmt++) != 0) {

      if (c == '%') {				/* expect format '%key' */
          switch(c = *fmt++) {			/* determine what to do */

          /* Known keys are %d, %u, %x, %s, and %%. This is easily extended 
           * with number types like %b and %o by providing a different base.
           * Number type keys don't set a string to 's', but use the general
           * conversion after the switch statement.
           */ 
          case 'd':				/* output decimal */
              u = arg < 0 ? -arg : arg;
              if (arg < 0) negative = 1;
              base = 10;
              break;
          case 'u':				/* output unsigned long */
              u = (unsigned long) arg;
              base = 10;
              break;
          case 'x':				/* output hexadecimal */
              u = (unsigned long) arg;
              base = 0x10;
              break;
          case 's': 				/* output string */
              if ((s=(char *)arg) == NULL)
                  s = "(null)";
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
  message m;
  if (c != END_OF_KMESS) {
      kmess.km_buf[kmess.km_next] = c;	/* put normal char in buffer */
      if (kmess.km_size < KMESS_BUF_SIZE)
          kmess.km_size += 1;		
      kmess.km_next = (kmess.km_next + 1) % KMESS_BUF_SIZE;
  } else {
      m.NOTIFY_TYPE = NEW_KMESS;
      lock_notify(PRINTF_PROC, &m);
  }
}


/*=========================================================================*
 *				kstrlen		 			   *
 *=========================================================================*/
PUBLIC size_t kstrlen(const char *org)
{
  register const char *s = org;
  while (*s++)
	/* EMPTY */ ;
  return --s - org;
}


/*=========================================================================*
 *				kstrcmp		 			   *
 *=========================================================================*/
int kstrcmp(register const char *s1, register const char *s2) 
{
  while (*s1 == *s2++) {
      if (*s1++ == '\0') return 0;
  }
  if (*s1 == '\0') return -1;
  if (*--s2 == '\0') return 1;
  return (unsigned char) *s1 - (unsigned char) *s2;
}


/*=========================================================================*
 *				kstrncmp		 		   *
 *=========================================================================*/
PUBLIC int kstrncmp(register const char *s1, register const char *s2, register size_t n)
{
  while (n > 0  &&  *s1 == *s2++) {
      if (*s1++ == '\0') return 0;
      n--;
  } 
  if (n > 0) {
      if (*s1 == '\0') return -1;
      if (*--s2 == '\0') return 1;
      return (unsigned char) *s1 - (unsigned char) *s2;
  }
  return 0;
}


/*=========================================================================*
 *				kstrncpy				   *
 *=========================================================================*/
PUBLIC char *kstrncpy(char *ret, register const char *s2, register size_t n)
{
  register char *s1 = ret;
  while((n-- > 0) && (*s1++ = *s2++)) 	/* copy up to n chars */
      /* EMPTY */ ;
  while(n-- > 0)   			/* possibly pad target */
      *s1++ = '\0';
  return ret;
}


