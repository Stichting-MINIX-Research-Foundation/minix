
/* Common definitions and declarations for the testcache code
 * and the testcache clients.
 */

#include <sys/types.h>

#define MAXBLOCKS 1500000

#define MAXBLOCKSIZE (4*PAGE_SIZE)

int dowriteblock(int b, int blocksize, u32_t seed, char *block);
int readblock(int b, int blocksize, u32_t seed, char *block);
void testend(void);
int dotest(int blocksize, int nblocks, int iterations);
void cachequiet(int quiet);

#define OK_BLOCK_GONE	-999
