/*
 * Get everything SOFTFLOAT_FOR_GCC normally would rename out of the users
 * namespace. Much of this isn't used but to avoid dissecting softloat.c
 * all of it is pulled in even for just the _Qp* case
 */

#if defined(SOFTFLOATAARCH64_FOR_GCC) && !defined(SOFTFLOAT_FOR_GCC)
#define float_exception_flags   _softfloat_float_exception_flags
#define float_rounding_mode     _softfloat_float_rounding_mode
#define float_raise             _softfloat_float_raise

#define float32_eq              _softfloat_float32_eq
#define float32_le              _softfloat_float32_le
#define float32_lt              _softfloat_float32_lt
#define float64_eq              _softfloat_float64_eq
#define float64_le              _softfloat_float64_le
#define float64_lt              _softfloat_float64_lt

#define float32_add			_softfloat_float32_add
#define float64_add			_softfloat_float64_add
#define float32_sub			_softfloat_float32_sub
#define float64_sub			_softfloat_float64_sub
#define float32_mul			_softfloat_float32_mul
#define float64_mul			_softfloat_float64_mul
#define float32_div			_softfloat_float32_div
#define float64_div			_softfloat_float64_div
#define int32_to_float32		_softfloat_int32_to_float32
#define int32_to_float64		_softfloat_int32_to_float64
#define int64_to_float32		_softfloat_int64_to_float32
#define int64_to_float64		_softfloat_int64_to_float64
#define float32_to_int32_round_to_zero	_softfloat_float32_to_int32_round_to_zero
#define float64_to_int32_round_to_zero	_softfloat_float64_to_int32_round_to_zero
#define float32_to_int64_round_to_zero	_softfloat_float32_to_int64_round_to_zero
#define float64_to_int64_round_to_zero	_softfloat_float64_to_int64_round_to_zero
#define float32_to_uint32_round_to_zero	_softfloat_float32_to_uint32_round_to_zero
#define float64_to_uint32_round_to_zero	_softfloat_float64_to_uint32_round_to_zero
#define float32_to_float64		_softfloat_float32_to_float64
#define float64_to_float32		_softfloat_float64_to_float32
#define float32_is_signaling_nan	_softfloat_float32_is_signaling_nan
#define float64_is_signaling_nan	_softfloat_float64_is_signaling_nan

#endif /* SOFTFLOATAARCH64_FOR_GCC and !SOFTFLOAT_FOR_GCC */

/*
 * The following will always end up in the namespace if FLOAT128 is
 * defined and SOFTFLOAT_FOR_GCC isn't. So rename them out of the user's
 * namespace.
 */

#ifdef SOFTFLOATAARCH64_FOR_GCC
#define	float128_add			_softfloat_float128_add
#define	float128_div			_softfloat_float128_div
#define	float128_eq			_softfloat_float128_eq
#define	float128_eq_signaling		_softfloat_float128_eq_signaling
#define	float128_is_nan			_softfloat_float128_is_nan
#define	float128_is_signaling_nan	_softfloat_float128_is_signaling_nan
#define	float128_le			_softfloat_float128_le
#define	float128_le_quiet		_softfloat_float128_le_quiet
#define	float128_lt			_softfloat_float128_lt
#define	float128_lt_quiet		_softfloat_float128_lt_quiet
#define	float128_mul			_softfloat_float128_mul
#define	float128_rem			_softfloat_float128_rem
#define	float128_round_to_int		_softfloat_float128_round_to_int
#define	float128_sqrt			_softfloat_float128_sqrt
#define	float128_sub			_softfloat_float128_sub
#define	float128_to_float32		_softfloat_float128_to_float32
#define	float128_to_float64		_softfloat_float128_to_float64
#define	float128_to_int32		_softfloat_float128_to_int32
#define	float128_to_int32_round_to_zero	_softfloat_float128_to_int32_round_to_zero
#define	float128_to_int64		_softfloat_float128_to_int64
#define	float128_to_int64_round_to_zero	_softfloat_float128_to_int64_round_to_zero
#define	float128_to_uint64_round_to_zero	_softfloat_float128_to_uint64_round_to_zero
#define	float32_to_float128		_softfloat_float32_to_float128
#define	float64_to_float128		_softfloat_float64_to_float128
#define	int32_to_float128		_softfloat_int32_to_float128
#define	int64_to_float128		_softfloat_int64_to_float128

/*
 * If this isn't defined go ahead and set it now since this is now past
 * anywhere define's are happening and this will conditionally compile out
 * a lot of extraneous code in softfloat.c
 */

#ifndef SOFTFLOAT_FOR_GCC
#define SOFTFLOAT_FOR_GCC
#endif

#endif /* SOFTFLOATAARCH64_FOR_GCC */
