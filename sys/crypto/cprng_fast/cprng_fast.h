#ifndef	_SYS_CPRNG_FAST_H_
#define	_SYS_CPRNG_FAST_H_

size_t		cprng_fast(void *, size_t);
uint32_t	cprng_fast32(void);
uint64_t	cprng_fast64(void);
void		cprng_fast_init(void);

#endif	/* _SYS_CPRNG_FAST_H_ */
