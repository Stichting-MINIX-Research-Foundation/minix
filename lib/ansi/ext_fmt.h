struct mantissa {
	unsigned long h_32;
	unsigned long l_32;
};

struct EXTEND {
	short	sign;
	short	exp;
	struct mantissa mantissa;
#define m1 mantissa.h_32
#define m2 mantissa.l_32
};
	
