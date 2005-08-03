/*
random.h

Public interface to the random number generator 
*/

_PROTOTYPE( void random_init, (void)					);
_PROTOTYPE( int random_isseeded, (void)					);
_PROTOTYPE( void random_update, (int source, unsigned short *buf, 
							int count)	);
_PROTOTYPE( void random_getbytes, (void *buf, size_t size)		);
_PROTOTYPE( void random_putbytes, (void *buf, size_t size)		);
