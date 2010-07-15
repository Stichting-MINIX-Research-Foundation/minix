/* Few u64 utils implemented in C
 * Author: Gautam BT
 */
#include <minix/u64.h>

u64_t rrotate64(u64_t x, unsigned short b)
{
	u64_t r, t;

	b %= 64;

	if(b == 32) {
		r.lo = x.hi;
		r.hi = x.lo;
		return r;
	}else if(b < 32) {
		r.lo = (x.lo >> b) | (x.hi << (32 - b));		
		r.hi = (x.hi >> b) | (x.lo << (32 - b));		
		return r;
	}else {
		/* Rotate by 32 bits first then rotate by remaining */
		t.lo = x.hi;
		t.hi = x.lo;
		b = b - 32;
		r.lo = (t.lo >> b) | (t.hi << (32 - b));		
		r.hi = (t.hi >> b) | (t.lo << (32 - b));		
		return r;
	}
}

u64_t rshift64(u64_t x, unsigned short b)
{
	u64_t r;

	if(b >= 64)
		return make64(0,0);

	if(b >= 32) {
		r.hi = 0;
		r.lo = x.hi >> (b - 32);
	}else {
		r.lo = (x.lo >> b) | (x.hi << (32 - b));		
		r.hi = (x.hi >> b);		
	}
	return r;
}

u64_t xor64(u64_t a, u64_t b)
{
	u64_t r;
	r.hi = a.hi ^ b.hi;
	r.lo = a.lo ^ b.lo;

	return r;
}

u64_t and64(u64_t a, u64_t b)
{
	u64_t r;
	r.hi = a.hi & b.hi;
	r.lo = a.lo & b.lo;

	return r;
}

u64_t not64(u64_t a)
{
	u64_t r;

	r.hi = ~a.hi;
	r.lo = ~a.lo;

	return r;
}


