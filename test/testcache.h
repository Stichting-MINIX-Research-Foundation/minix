
/* Common definitions and declarations for the testcache code
 * and the testcache clients.
 */

#include <sys/types.h>
#include <machine/param.h>
#include <machine/vmparam.h>

#define MAXBLOCKS 1500000

#define MAXBLOCKSIZE (4*PAGE_SIZE)

int dowriteblock(int b, int blocksize, u32_t seed, char *block);
int readblock(int b, int blocksize, u32_t seed, char *block);
void testend(void);
int dotest(int blocksize, int nblocks, int iterations);
void cachequiet(int quiet);
void get_fd_offset(int b, int blocksize, u64_t *file_offset, int *fd);
void makefiles(int n);

#define OK_BLOCK_GONE	-999

/* for file-oriented tests:
 *
 * we want to flexibly split tests over multiple files
 * - for big working sets we might run over the 2GB MFS file limit
 * - we might want to test the FS being able to handle lots of
 *   files / unusual metadata situations
 */
#define MBPERFILE 2000
#define MB (1024*1024)
#define MAXFILES ((u64_t) MAXBLOCKS * MAXBLOCKSIZE / MB / MBPERFILE + 1)

extern int fds[MAXFILES], bigflag;

