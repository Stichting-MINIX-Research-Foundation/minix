/*
random.h

Public interface to the random number generator 
*/

/* Internal random sources */
#define RND_TIMING		0
#define RANDOM_SOURCES_INTERNAL	1
#define TOTAL_SOURCES	(RANDOM_SOURCES+RANDOM_SOURCES_INTERNAL)

_PROTOTYPE( void random_init, (void)					);
_PROTOTYPE( int random_isseeded, (void)					);
_PROTOTYPE( void random_update, (int source, rand_t *buf, int count)	);
_PROTOTYPE( void random_getbytes, (void *buf, size_t size)		);
_PROTOTYPE( void random_putbytes, (void *buf, size_t size)		);
