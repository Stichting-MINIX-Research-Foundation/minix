/*
random.h

Public interface to the random number generator 
*/

/* Internal random sources */
#define RND_TIMING		0
#define RANDOM_SOURCES_INTERNAL	1
#define TOTAL_SOURCES	(RANDOM_SOURCES+RANDOM_SOURCES_INTERNAL)

void random_init(void);
int random_isseeded(void);
void random_update(int source, rand_t *buf, int count);
void random_getbytes(void *buf, size_t size);
void random_putbytes(void *buf, size_t size);
