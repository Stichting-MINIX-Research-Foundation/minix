#include <lib.h>

#include <stdlib.h>
#include <string.h>
#include <minix/profile.h>
#include <minix/syslib.h>
#include <minix/type.h>
#include <minix/minlib.h>
#include <minix/sysutil.h>

/*===========================================================================*
 *                              get_randomness                               *
 *===========================================================================*/
void get_randomness(rand, source)
struct k_randomness *rand;
int source;
{
/* Use architecture-dependent high-resolution clock for
 * raw entropy gathering.
 */
  int r_next;
  unsigned long tsc_high, tsc_low;
 
  source %= RANDOM_SOURCES;
  r_next= rand->bin[source].r_next;  
  read_tsc((u32_t *) &tsc_high, (u32_t *) &tsc_low);
  rand->bin[source].r_buf[r_next] = tsc_low;  
  if (rand->bin[source].r_size < RANDOM_ELEMENTS) {  
        rand->bin[source].r_size ++;  
  }
  rand->bin[source].r_next = (r_next + 1 ) % RANDOM_ELEMENTS;  
} 
  

