/*
libc/ieee_float/ieee_float.h

Created:	Oct 14, 1993 by Philip Homburg <philip@cs.vu.nl>

Define structures and macros for manipulating IEEE floats
*/

#ifndef IEEE_FLOAT_H
#define IEEE_FLOAT_H

#define isnan __IsNan

struct f64
{
	u32_t low_word;
	u32_t high_word;
};

#define F64_SIGN_SHIFT	31
#define F64_SIGN_MASK	1

#define F64_EXP_SHIFT	20
#define F64_EXP_MASK	0x7ff
#define F64_EXP_BIAS	1023
#define F64_EXP_MAX	2047

#define F64_MANT_SHIFT	0
#define F64_MANT_MASK	0xfffff

#define F64_GET_SIGN(fp)	(((fp)->high_word >> F64_SIGN_SHIFT) & \
					F64_SIGN_MASK)
#define F64_GET_EXP(fp)		(((fp)->high_word >> F64_EXP_SHIFT) & \
					F64_EXP_MASK)
#define F64_SET_EXP(fp, val)	((fp)->high_word= ((fp)->high_word &	\
				~(F64_EXP_MASK << F64_EXP_SHIFT)) | 	\
				(((val) & F64_EXP_MASK) << F64_EXP_SHIFT))

#define F64_GET_MANT_LOW(fp)		((fp)->low_word)
#define F64_SET_MANT_LOW(fp, val)	((fp)->low_word= (val))
#define F64_GET_MANT_HIGH(fp)	(((fp)->high_word >> F64_MANT_SHIFT) & \
					F64_MANT_MASK)
#define F64_SET_MANT_HIGH(fp, val)	((fp)->high_word= ((fp)->high_word & \
				~(F64_MANT_MASK << F64_MANT_SHIFT)) |	\
				(((val) & F64_MANT_MASK) << F64_MANT_SHIFT))

#endif /* IEEE_FLOAT_H */

/*
 * $PchId: ieee_float.h,v 1.3 1996/02/22 21:01:39 philip Exp $
 */
