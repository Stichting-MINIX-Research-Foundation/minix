/*
  (c) copyright 1989 by the Vrije Universiteit, Amsterdam, The Netherlands.
  See the copyright notice in the ACK home directory, in the file "Copyright".
*/

/* extended precision arithmetic for the strtod() and cvt() routines */

/* This may require some more work when long doubles get bigger than 8
   bytes. In this case, these routines may become obsolete. ???
*/

#include	"ext_fmt.h"
#include	<float.h>
#include	<errno.h>
#include	<ctype.h>

static int b64_add(struct mantissa *e1, struct mantissa *e2);
static void b64_sft(struct mantissa *e1, int n);

static void
mul_ext(const struct EXTEND *e1, const struct EXTEND *e2, struct EXTEND *e3)
{
	/*	Multiply the extended numbers e1 and e2, and put the
		result in e3.
	*/
	register int	i,j;		/* loop control	*/
	unsigned short	mp[4];
	unsigned short	mc[4];
	unsigned short	result[8];	/* result */

	register unsigned short *pres;

	/* first save the sign (XOR)			*/
	e3->sign = e1->sign ^ e2->sign;

	/* compute new exponent */
	e3->exp = e1->exp + e2->exp + 1;

	/* check for overflow/underflow	??? */

	/* 128 bit multiply of mantissas	*/

	/* assign unknown long formats		*/
	/* to known unsigned word formats	*/
	mp[0] = e1->m1 >> 16;
	mp[1] = (unsigned short) e1->m1;
	mp[2] = e1->m2 >> 16;
	mp[3] = (unsigned short) e1->m2;
	mc[0] = e2->m1 >> 16;
	mc[1] = (unsigned short) e2->m1;
	mc[2] = e2->m2 >> 16;
	mc[3] = (unsigned short) e2->m2;
	for (i = 8; i--;) {
		result[i] = 0;
	}
	/*
	 *	fill registers with their components
	 */
	for(i=4, pres = &result[4];i--;pres--) if (mp[i]) {
		unsigned short k = 0;
		unsigned long mpi = mp[i];
		for(j=4;j--;) {
			unsigned long tmp = (unsigned long)pres[j] + k;
			if (mc[j]) tmp += mpi * mc[j];
			pres[j] = tmp;
			k = tmp >> 16;
		}
		pres[-1] = k;
	}

	if (! (result[0] & 0x8000)) {
		e3->exp--;
		for (i = 0; i <= 3; i++) {
			result[i] <<= 1;
			if (result[i+1]&0x8000) result[i] |= 1;
		}
		result[4] <<= 1;
	}	
	/*
	 *	combine the registers to a total
	 */
	e3->m1 = ((unsigned long)(result[0]) << 16) + result[1];
	e3->m2 = ((unsigned long)(result[2]) << 16) + result[3];
	if (result[4] & 0x8000) {
		if (++e3->m2 == 0) {
			if (++e3->m1 == 0) {
				e3->m1 = 0x80000000;
				e3->exp++;
			}
		}
	}
}

static void
add_ext(struct EXTEND *e1, struct EXTEND *e2, struct EXTEND *e3)
{
	/*	Add two extended numbers e1 and e2, and put the result
		in e3
	*/
	struct EXTEND ce2;
	int diff;

	if ((e2->m1 | e2->m2) == 0L) {
		*e3 = *e1;
		return;
	}
	if ((e1->m1 | e1->m2) == 0L) {
		*e3 = *e2;
		return;
	}
	ce2 = *e2;
	*e3 = *e1;
	e1 = &ce2;

	/* adjust mantissas to equal power */
	diff = e3->exp - e1->exp;
	if (diff < 0) {
		diff = -diff;
		e3->exp += diff;
		b64_sft(&(e3->mantissa), diff);
	}
	else if (diff > 0) {
		e1->exp += diff;
		b64_sft(&(e1->mantissa), diff);
	}
	if (e1->sign != e3->sign) {
		/* e3 + e1 = e3 - (-e1) */
		if (e1->m1 > e3->m1 ||
                    (e1->m1 == e3->m1 && e1->m2 > e3->m2)) {
                	/*      abs(e1) > abs(e3) */
                	if (e3->m2 > e1->m2) {
                        	e1->m1 -= 1;    /* carry in */
                	}
                	e1->m1 -= e3->m1;
                	e1->m2 -= e3->m2;
                	*e3 = *e1;
        	}
        	else {
                	if (e1->m2 > e3->m2)
                        	e3->m1 -= 1;    /* carry in */
                	e3->m1 -= e1->m1;
                	e3->m2 -= e1->m2;
        	}
	}
	else {
		if (b64_add(&e3->mantissa,&e1->mantissa)) {/* addition carry */
			b64_sft(&e3->mantissa,1);/* shift mantissa one bit RIGHT */
			e3->m1 |= 0x80000000L;	/* set max bit	*/
			e3->exp++;		/* increase the exponent */
		}
	}
	if ((e3->m2 | e3->m1) != 0L) {
		/* normalize */
		if (e3->m1 == 0L) {
			e3->m1 = e3->m2; e3->m2 = 0L; e3->exp -= 32;
		}
		if (!(e3->m1 & 0x80000000)) {
			unsigned long l = 0x40000000;
			int cnt = -1;

			while (! (l & e3->m1)) {
				l >>= 1; cnt--;
			}
			e3->exp += cnt;
			b64_sft(&(e3->mantissa), cnt);
		}
	}
}

static int
cmp_ext(struct EXTEND *e1, struct EXTEND *e2)
{
        struct EXTEND tmp; 
         
        e2->sign = ! e2->sign; 
        add_ext(e1, e2, &tmp);  
        e2->sign = ! e2->sign;
        if (tmp.m1 == 0 && tmp.m2 == 0) return 0; 
        if (tmp.sign) return -1;
        return 1;
}

static void
b64_sft(struct mantissa *e1, int n)
{
	if (n > 0) {
		if (n > 63) {
			e1->l_32 = 0;
			e1->h_32 = 0;
			return;
		}
		if (n >= 32) {
			e1->l_32 = e1->h_32;
			e1->h_32 = 0;
			n -= 32;
		}
		if (n > 0) {
			e1->l_32 >>= n;
			if (e1->h_32 != 0) {
				e1->l_32 |= (e1->h_32 << (32 - n));
				e1->h_32 >>= n;
			}
		}
		return;
	}
	n = -n;
	if (n > 0) {
		if (n > 63) {
			e1->l_32 = 0;
			e1->h_32 = 0;
			return;
		}
		if (n >= 32) {
			e1->h_32 = e1->l_32;
			e1->l_32 = 0;
			n -= 32;
		}
		if (n > 0) {
			e1->h_32 <<= n;
			if (e1->l_32 != 0) {
				e1->h_32 |= (e1->l_32 >> (32 - n));
				e1->l_32 <<= n;
			}
		}
	}
}

static int
b64_add(struct mantissa *e1, struct mantissa *e2)
		/*
		 * pointers to 64 bit 'registers'
		 */
{
	register int	overflow;
	int		carry;

			/* add higher pair of 32 bits */
	overflow = ((unsigned long) 0xFFFFFFFF - e1->h_32 < e2->h_32);
	e1->h_32 += e2->h_32;

			/* add lower pair of 32 bits */
	carry = ((unsigned long) 0xFFFFFFFF - e1->l_32 < e2->l_32);
	e1->l_32 += e2->l_32;
	if ((carry) && (++e1->h_32 == 0))
		return(1);		/* had a 64 bit overflow */
	else
		return(overflow);	/* return status from higher add */
}

/* The following tables can be computed with the following bc(1)
   program:

obase=16
scale=0
define t(x){
	auto a, b, c
	a=2;b=1;c=2^32;n=1
	while(a<x) {
		b=a;n+=n;a*=a
	}
	n/=2
	a=b
	while(b<x) {
		a=b;b*=c;n+=32
	}
	n-=32
	b=a
	while(a<x) {
		b=a;a+=a;n+=1
	}
	n-=1
	x*=16^16
	b=x%a
	x/=a
	if(a<=(2*b)) x+=1
	obase=10
	n
	obase=16
	return(x)
}
for (i=1;i<28;i++) {
	t(10^i)
}
0
for (i=1;i<20;i++) {
	t(10^(28*i))
}
0
define r(x){
	auto a, b, c
	a=2;b=1;c=2^32;n=1
	while(a<x) {
		b=a;n+=n;a*=a
	}
	n/=2
	a=b
	while(b<x) {
		a=b;b*=c;n+=32
	}
	n-=32
	b=a
	while(a<x) {
		b=a;a+=a;n+=1
	}
	a=b
	a*=16^16
	b=a%x
	a/=x
	if(x<=(2*b)) a+=1
	obase=10
	-n
	obase=16
	return(a)
}
for (i=1;i<28;i++) {
	r(10^i)
}
0
for (i=1;i<20;i++) {
	r(10^(28*i))
}
0

*/
static struct EXTEND ten_powers[] = {	/* representation of 10 ** i */
	{ 0,	0,	0x80000000,	0 },
	{ 0,	3,	0xA0000000,	0 },
	{ 0,	6,	0xC8000000,	0 },
	{ 0,	9,	0xFA000000,	0 },
	{ 0,	13,	0x9C400000,	0 },
	{ 0,	16,	0xC3500000,	0 },
	{ 0,	19,	0xF4240000,	0 },
	{ 0,	23,	0x98968000,	0 },
	{ 0,	26,	0xBEBC2000,	0 },
	{ 0,	29,	0xEE6B2800,	0 },
	{ 0,	33,	0x9502F900,	0 },
	{ 0,	36,	0xBA43B740,	0 },
	{ 0,	39,	0xE8D4A510,	0 },
	{ 0,	43,	0x9184E72A,	0 },
	{ 0,	46,	0xB5E620F4,	0x80000000 },
	{ 0,	49,	0xE35FA931,	0xA0000000 },
	{ 0,	53,	0x8E1BC9BF,	0x04000000 },
	{ 0,	56,	0xB1A2BC2E,	0xC5000000 },
	{ 0,	59,	0xDE0B6B3A,	0x76400000 },
	{ 0,	63,	0x8AC72304,	0x89E80000 },
	{ 0,	66,	0xAD78EBC5,	0xAC620000 },
	{ 0,	69,	0xD8D726B7,	0x177A8000 },
	{ 0,	73,	0x87867832,	0x6EAC9000 },
	{ 0,	76,	0xA968163F,	0x0A57B400 },
	{ 0,	79,	0xD3C21BCE,	0xCCEDA100 },
	{ 0,	83,	0x84595161,	0x401484A0 },
	{ 0,	86,	0xA56FA5B9,	0x9019A5C8 },
	{ 0,	89,	0xCECB8F27,	0xF4200F3A }
};
static struct EXTEND big_ten_powers[] = {  /* representation of 10 ** (28*i) */
	{ 0,	0,	0x80000000,	0 },
	{ 0,	93,	0x813F3978,	0xF8940984 },
	{ 0,	186,	0x82818F12,	0x81ED44A0 },
	{ 0,	279,	0x83C7088E,	0x1AAB65DB },
	{ 0,	372,	0x850FADC0,	0x9923329E },
	{ 0,	465,	0x865B8692,	0x5B9BC5C2 },
	{ 0,	558,	0x87AA9AFF,	0x79042287 },
	{ 0,	651,	0x88FCF317,	0xF22241E2 },
	{ 0,	744,	0x8A5296FF,	0xE33CC930 },
	{ 0,	837,	0x8BAB8EEF,	0xB6409C1A },
	{ 0,	930,	0x8D07E334,	0x55637EB3 },
	{ 0,	1023,	0x8E679C2F,	0x5E44FF8F },
	{ 0,	1116,	0x8FCAC257,	0x558EE4E6 },
	{ 0,	1209,	0x91315E37,	0xDB165AA9 },
	{ 0,	1302,	0x929B7871,	0xDE7F22B9 },
	{ 0,	1395,	0x940919BB,	0xD4620B6D },
	{ 0,	1488,	0x957A4AE1,	0xEBF7F3D4 },
	{ 0,	1581,	0x96EF14C6,	0x454AA840 },
	{ 0,	1674,	0x98678061,	0x27ECE4F5 },
	{ 0,	1767,	0x99E396C1,	0x3A3ACFF2 }
};

static struct EXTEND r_ten_powers[] = { /* representation of 10 ** -i */
	{ 0,	0,	0x80000000,	0 },
	{ 0,	-4,	0xCCCCCCCC,	0xCCCCCCCD },
	{ 0,	-7,	0xA3D70A3D,	0x70A3D70A },
	{ 0,	-10,	0x83126E97,	0x8D4FDF3B },
	{ 0,	-14,	0xD1B71758,	0xE219652C },
	{ 0,	-17,	0xA7C5AC47,	0x1B478423 },
	{ 0,	-20,	0x8637BD05,	0xAF6C69B6 },
	{ 0,	-24,	0xD6BF94D5,	0xE57A42BC },
	{ 0,	-27,	0xABCC7711,	0x8461CEFD },
	{ 0,	-30,	0x89705F41,	0x36B4A597 },
	{ 0,	-34,	0xDBE6FECE,	0xBDEDD5BF },
	{ 0,	-37,	0xAFEBFF0B,	0xCB24AAFF },
	{ 0,	-40,	0x8CBCCC09,	0x6F5088CC },
	{ 0,	-44,	0xE12E1342,	0x4BB40E13 },
	{ 0,	-47,	0xB424DC35,	0x095CD80F },
	{ 0,	-50,	0x901D7CF7,	0x3AB0ACD9 },
	{ 0,	-54,	0xE69594BE,	0xC44DE15B },
	{ 0,	-57,	0xB877AA32,	0x36A4B449 },
	{ 0,	-60,	0x9392EE8E,	0x921D5D07 },
	{ 0,	-64,	0xEC1E4A7D,	0xB69561A5 },
	{ 0,	-67,	0xBCE50864,	0x92111AEB },
	{ 0,	-70,	0x971DA050,	0x74DA7BEF },
	{ 0,	-74,	0xF1C90080,	0xBAF72CB1 },
	{ 0,	-77,	0xC16D9A00,	0x95928A27 },
	{ 0,	-80,	0x9ABE14CD,	0x44753B53 },
	{ 0,	-84,	0xF79687AE,	0xD3EEC551 },
	{ 0,	-87,	0xC6120625,	0x76589DDB },
	{ 0,	-90,	0x9E74D1B7,	0x91E07E48 }
};

static struct EXTEND r_big_ten_powers[] = { /* representation of 10 ** -(28*i) */
	{ 0,	0,	0x80000000,	0 },
	{ 0,	-94,	0xFD87B5F2,	0x8300CA0E },
	{ 0,	-187,	0xFB158592,	0xBE068D2F },
	{ 0,	-280,	0xF8A95FCF,	0x88747D94 },
	{ 0,	-373,	0xF64335BC,	0xF065D37D },
	{ 0,	-466,	0xF3E2F893,	0xDEC3F126 },
	{ 0,	-559,	0xF18899B1,	0xBC3F8CA2 },
	{ 0,	-652,	0xEF340A98,	0x172AACE5 },
	{ 0,	-745,	0xECE53CEC,	0x4A314EBE },
	{ 0,	-838,	0xEA9C2277,	0x23EE8BCB },
	{ 0,	-931,	0xE858AD24,	0x8F5C22CA },
	{ 0,	-1024,	0xE61ACF03,	0x3D1A45DF },
	{ 0,	-1117,	0xE3E27A44,	0x4D8D98B8 },
	{ 0,	-1210,	0xE1AFA13A,	0xFBD14D6E },
	{ 0,	-1303,	0xDF82365C,	0x497B5454 },
	{ 0,	-1396,	0xDD5A2C3E,	0xAB3097CC },
	{ 0,	-1489,	0xDB377599,	0xB6074245 },
	{ 0,	-1582,	0xD91A0545,	0xCDB51186 },
	{ 0,	-1675,	0xD701CE3B,	0xD387BF48 },
	{ 0,	-1768,	0xD4EEC394,	0xD6258BF8 }
};

#define	TP	(int)(sizeof(ten_powers)/sizeof(ten_powers[0]))
#define BTP	(int)(sizeof(big_ten_powers)/sizeof(big_ten_powers[0]))
#define MAX_EXP	(TP * BTP - 1)

static void
add_exponent(struct EXTEND *e, int exp)
{
	int neg = exp < 0;
	int divsz, modsz;
	struct EXTEND x;

	if (neg) exp = -exp;
	divsz = exp / TP;
	modsz = exp % TP;
	if (neg) {
		mul_ext(e, &r_ten_powers[modsz], &x);
		mul_ext(&x, &r_big_ten_powers[divsz], e);
	}
	else {
		mul_ext(e, &ten_powers[modsz], &x);
		mul_ext(&x, &big_ten_powers[divsz], e);
	}
}

void _str_ext_cvt(const char *s, char **ss, struct EXTEND *e)
{
	/*	Like strtod, but for extended precision */
	register int	c;
	int		dotseen = 0;
	int		digitseen = 0;
	int		exp = 0;

	if (ss) *ss = (char *)s;
	while (isspace(*s)) s++;

	e->sign = 0;
	e->exp = 0;
	e->m1 = e->m2 = 0;

	c = *s;
	switch(c) {
	case '-':
		e->sign = 1;
	case '+':
		s++;
	}
	while (c = *s++, isdigit(c) || (c == '.' && ! dotseen++)) {
		if (c == '.') continue;
		digitseen = 1;
		if (e->m1 <= (unsigned long)(0xFFFFFFFF)/10) {
			struct mantissa	a1;

			a1 = e->mantissa;
			b64_sft(&(e->mantissa), -3);
			b64_sft(&a1, -1);
			b64_add(&(e->mantissa), &a1);
			a1.h_32 = 0;
			a1.l_32 = c - '0';
			b64_add(&(e->mantissa), &a1);
		}
		else exp++;
		if (dotseen) exp--;
	}
	if (! digitseen) return;

	if (ss) *ss = (char *)s - 1;

	if (c == 'E' || c == 'e') {
		int	exp1 = 0;
		int	sign = 1;
		int	exp_overflow = 0;

		switch(*s) {
		case '-':
			sign = -1;
		case '+':
			s++;
		}
		if (c = *s, isdigit(c)) {
			do {
				int tmp;

				exp1 = 10 * exp1 + (c - '0');
				if ((tmp = sign * exp1 + exp) > MAX_EXP ||
				     tmp < -MAX_EXP) {
					exp_overflow = 1;
				}
			} while (c = *++s, isdigit(c));
			if (ss) *ss = (char *)s;
		}
		exp += sign * exp1;
		if (exp_overflow) {
			exp = sign * MAX_EXP;
			if (e->m1 != 0 || e->m2 != 0) errno = ERANGE;
		}
	}
	if (e->m1 == 0 && e->m2 == 0) return;
	e->exp = 63;
	while (! (e->m1 & 0x80000000)) {
		b64_sft(&(e->mantissa),-1);
		e->exp--;
	}
	add_exponent(e, exp);
}

#include	<math.h>


static void
ten_mult(struct EXTEND *e)
{
	struct EXTEND e1 = *e;

	e1.exp++;
	e->exp += 3;
	add_ext(e, &e1, e);
}

#define NDIGITS 128
#define NSIGNIFICANT 19

char *
_ext_str_cvt(struct EXTEND *e, int ndigit, int *decpt, int *sign, int ecvtflag)
{
	/*	Like cvt(), but for extended precision */

	static char buf[NDIGITS+1];
	struct EXTEND m;
	register char *p = buf;
	register char *pe;
	int findex = 0;

	if (ndigit < 0) ndigit = 0;
	if (ndigit > NDIGITS) ndigit = NDIGITS;
	pe = &buf[ndigit];
	buf[0] = '\0';

	*sign = 0;
	if (e->sign) {
		*sign = 1;
		e->sign = 0;
	}

	*decpt = 0;
	if (e->m1 != 0) {
		register struct EXTEND *pp = &big_ten_powers[1];

		while(cmp_ext(e,pp) >= 0) {
			pp++;
			findex = pp - big_ten_powers;
			if (findex >= BTP) break;
		}
		pp--;
		findex = pp - big_ten_powers;
		mul_ext(e,&r_big_ten_powers[findex],e);
		*decpt += findex * TP;
		pp = &ten_powers[1];
		while(pp < &ten_powers[TP] && cmp_ext(e, pp) >= 0) pp++;
		pp--;
		findex = pp - ten_powers;
		*decpt += findex;

		if (cmp_ext(e, &ten_powers[0]) < 0) {
			pp = &r_big_ten_powers[1];
			while(cmp_ext(e,pp) < 0) pp++;
			pp--;
			findex = pp - r_big_ten_powers;
			mul_ext(e, &big_ten_powers[findex], e);
			*decpt -= findex * TP;
			/* here, value >= 10 ** -28 */
			ten_mult(e);
			(*decpt)--;
			pp = &r_ten_powers[0];
			while(cmp_ext(e, pp) < 0) pp++;
			findex = pp - r_ten_powers;
			mul_ext(e, &ten_powers[findex], e);
			*decpt -= findex;
			findex = 0;
		}
		(*decpt)++;	/* because now value in [1.0, 10.0) */
	}
	if (! ecvtflag) {
		/* for fcvt() we need ndigit digits behind the dot */
		pe += *decpt;
		if (pe > &buf[NDIGITS]) pe = &buf[NDIGITS];
	}
	m.exp = -62;
	m.sign = 0;
	m.m1 = 0xA0000000;
	m.m2 = 0;
	while (p <= pe) {
		struct EXTEND oneminm;

		if (p - pe > NSIGNIFICANT) {
			findex = 0;
			e->m1 = 0;
		}
		if (findex) {
			struct EXTEND tc, oldtc;
			int count = 0;

			oldtc.exp = 0;
			oldtc.sign = 0;
			oldtc.m1 = 0;
			oldtc.m2 = 0;
			tc = ten_powers[findex];
			while (cmp_ext(e, &tc) >= 0) {
				oldtc = tc;
				add_ext(&tc, &ten_powers[findex], &tc);
				count++;
			}
			*p++ = count + '0';
			oldtc.sign = 1;
			add_ext(e, &oldtc, e);
			findex--;
			continue;
		}
		if (e->m1) {
			m.sign = 1;
			add_ext(&ten_powers[0], &m, &oneminm);
			m.sign = 0;
			if (e->exp >= 0) {
				struct EXTEND x;

				x.m2 = 0; x.exp = e->exp;
				x.sign = 1;
				x.m1 = e->m1>>(31-e->exp);
				*p++ = (x.m1) + '0';
				x.m1 = x.m1 << (31-e->exp);
				add_ext(e, &x, e);
			}
			else *p++ = '0';
			/* Check that remainder is still significant */
			if (cmp_ext(&m, e) > 0 || cmp_ext(e, &oneminm) > 0) {
				if (e->m1 && e->exp >= -1) *(p-1) += 1;
				e->m1 = 0;
				continue;
			}
			ten_mult(&m);
			ten_mult(e);
		}
		else *p++ = '0';
	}
	if (pe >= buf) {
		p = pe;
		*p += 5;	/* round of at the end */
		while (*p > '9') {
			*p = '0';
			if (p > buf) ++*--p;
			else {
				*p = '1';
				++*decpt;
				if (! ecvtflag) {
					/* maybe add another digit at the end,
					   because the point was shifted right
					*/
					if (pe > buf) *pe = '0';
					pe++;
				}
			}
		}
		*pe = '\0';
	}
	return buf;
}

void _dbl_ext_cvt(double value, struct EXTEND *e)
{
	/*	Convert double to extended
	*/
	int exponent;

	value = frexp(value, &exponent);
	e->sign = value < 0.0;
	if (e->sign) value = -value;
	e->exp = exponent - 1;
	value *= 4294967296.0;
	e->m1 = value;
	value -= e->m1;
	value *= 4294967296.0;
	e->m2 = value;
}

static struct EXTEND max_d;

double
_ext_dbl_cvt(struct EXTEND *e)
{
	/*	Convert extended to double
	*/
	double f;
	int sign = e->sign;

	e->sign = 0;
	if (e->m1 == 0 && e->m2 == 0) {
		return 0.0;
	}
	if (max_d.exp == 0) {
		_dbl_ext_cvt(DBL_MAX, &max_d);
	}
	if (cmp_ext(&max_d, e) < 0) {
		f = HUGE_VAL;
		errno = ERANGE;
	}
	else	f = ldexp((double)e->m1*4294967296.0 + (double)e->m2, e->exp-63);
	if (sign) f = -f;
	if (f == 0.0 && (e->m1 != 0 || e->m2 != 0)) {
		errno = ERANGE;
	}
	return f;
}
