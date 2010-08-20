
#include <minix/u64.h>
#include <minix/minlib.h>

/* Utility function to work directly with u64_t
 * By Antonio Mancina
 */
void read_tsc_64(t)
u64_t* t;
{
    u32_t lo, hi;
    read_tsc (&hi, &lo);
    *t = make64 (lo, hi);
}

