#include "minix_utils.h"

u64_t lshift64(u64_t x, unsigned short b)
{
	u64_t r;

	if(b >= 32) {
		r.lo = 0;
		r.hi = x.lo << (b - 32);
	}else {
		r.lo = x.lo << b;
		r.hi = (x.lo >> (32 - b)) | (x.hi << b);
	}
	return r;
}
