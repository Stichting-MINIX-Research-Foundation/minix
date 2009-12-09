#define GLUE_HELPER(x, y) x ## _ ## y
#define GLUE(x, y) GLUE_HELPER(x, y)
#define TOSTRING(x) #x

static const char *GLUE(make_string, TYPE_FUNC)(TYPE value, int base)
{
	static char buffer[66];
	char *s; /* allows 64-bit base 2 value with minus and null */
	TYPEU valuetemp;
	
	/* build number string in proper base, work backwards, starting with null */
	s = buffer + sizeof(buffer);
	*--s = 0;

	/* fill in the digits */
	valuetemp = (value < 0) ? -value : value;
	do
	{
		*--s = "0123456789abcdefghijklmnopqrstuvwxyz"[valuetemp % base];
		valuetemp /= base;
	} while (valuetemp);

	/* add sign if needed */
	if (value < 0)
		*--s = '-';

	return s;
}

static void GLUE(e, TYPE_FUNC)(int n, const char *s, TYPE result, int base)
{
	/* watch out: don't overwrite the static buffer in make_string */
	printf("Subtest %s, error %d, errno=%d, s=\"%s\", base=%d, ", TOSTRING(TYPE_FUNC), n, errno, s, base);
	printf("result=%s\n", GLUE(make_string, TYPE_FUNC)(result, base));
	if (errct++ > MAX_ERROR) 
	{
		printf("Too many errors; test aborted\n");
		exit(1);
	}
}

static void GLUE(test_string, TYPE_FUNC)(const char *s, TYPE value, int base)
{
	char *end;
	TYPE result;

	/* must convert the entire string, resulting in the requested value */
	result = TYPE_FUNC(s, &end, base);
	if (result != value) GLUE(e, TYPE_FUNC)(1, s, result, base);
	if (*end) GLUE(e, TYPE_FUNC)(2, s, result, base);
}

static void GLUE(test_value_with_base, TYPE_FUNC)(TYPE value, int base)
{
	const char *s;

	/* convert to string, then convert back */
	s = GLUE(make_string, TYPE_FUNC)(value, base);
	GLUE(test_string, TYPE_FUNC)(s, value, base);
}

static void GLUE(test_value, TYPE_FUNC)(TYPE value)
{
	int base;

	/* let's get all our bases covered */
	for (base = 2; base <= 36; base++)
		GLUE(test_value_with_base, TYPE_FUNC)(value, base);
}

static void GLUE(test, TYPE_FUNC)(void)
{
	int base, i;
	TYPE value, valuenext;

	/* check 0x0000.... and 0xffff.... */
	value = 0;
	for (i = 0; i < 0x10000; i++)
	{
		/* test current value */
		GLUE(test_value, TYPE_FUNC)(value);
		GLUE(test_value, TYPE_FUNC)(-value);
		value++;
	}

	/* check 0x8000.... and 0x7fff.... */
	value = 0;
	value = ((~value) << 1) >> 1;
	for (i = 0; i < 0x10000; i++)
	{
		/* test current value */
		GLUE(test_value, TYPE_FUNC)(value);
		GLUE(test_value, TYPE_FUNC)(-value);
		value++;
	}

	/* check powers of possible bases */
	for (base = 2; base <= 36; base++)
	{
		value = 1;
		while (1)
		{
			/* test current value with offsets */
			for (i = -36; i <= 36; i++)
			{
				GLUE(test_value, TYPE_FUNC)(value + i);
				GLUE(test_value, TYPE_FUNC)(-value + i);
			}

			/* stop after overflow */
			valuenext = value * base;
			if (valuenext <= value)
				break;

			value = valuenext;
		}
	}

	/* automatic base */
	GLUE(test_string, TYPE_FUNC)("10",  10,  0);
	GLUE(test_string, TYPE_FUNC)("010", 010, 0);
	GLUE(test_string, TYPE_FUNC)("010", 010, 8);
	GLUE(test_string, TYPE_FUNC)("0x10", 0x10, 0);
	GLUE(test_string, TYPE_FUNC)("0X10", 0X10, 0);
	GLUE(test_string, TYPE_FUNC)("0x10", 0x10, 16);
	GLUE(test_string, TYPE_FUNC)("0X10", 0X10, 16);

	/* ignore plus sign, leading spaces and zeroes */
	GLUE(test_string, TYPE_FUNC)("10", 10, 10);
	GLUE(test_string, TYPE_FUNC)("010", 10, 10);
	GLUE(test_string, TYPE_FUNC)("0010", 10, 10);
	GLUE(test_string, TYPE_FUNC)(" 10", 10, 10);
	GLUE(test_string, TYPE_FUNC)(" 010", 10, 10);
	GLUE(test_string, TYPE_FUNC)(" 0010", 10, 10);
	GLUE(test_string, TYPE_FUNC)("\t10", 10, 10);
	GLUE(test_string, TYPE_FUNC)("\t010", 10, 10);
	GLUE(test_string, TYPE_FUNC)("\t0010", 10, 10);
	GLUE(test_string, TYPE_FUNC)(" \t10", 10, 10);
	GLUE(test_string, TYPE_FUNC)(" \t010", 10, 10);
	GLUE(test_string, TYPE_FUNC)(" \t0010", 10, 10);
	GLUE(test_string, TYPE_FUNC)("+10", 10, 10);
	GLUE(test_string, TYPE_FUNC)("+010", 10, 10);
	GLUE(test_string, TYPE_FUNC)("+0010", 10, 10);
	GLUE(test_string, TYPE_FUNC)(" +10", 10, 10);
	GLUE(test_string, TYPE_FUNC)(" +010", 10, 10);
	GLUE(test_string, TYPE_FUNC)(" +0010", 10, 10);
	GLUE(test_string, TYPE_FUNC)("\t+10", 10, 10);
	GLUE(test_string, TYPE_FUNC)("\t+010", 10, 10);
	GLUE(test_string, TYPE_FUNC)("\t+0010", 10, 10);
	GLUE(test_string, TYPE_FUNC)(" \t+10", 10, 10);
	GLUE(test_string, TYPE_FUNC)(" \t+010", 10, 10);
	GLUE(test_string, TYPE_FUNC)(" \t+0010", 10, 10);
}

#undef GLUE_HELPER
#undef GLUE
#undef TOSTRING
