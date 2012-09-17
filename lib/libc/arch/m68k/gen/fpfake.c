#include <ieeefp.h>

fp_except
fpgetmask(void)
{
	return 0;
}

fp_rnd
fpgetround(void)
{
	return 0;
}

fp_except
fpgetsticky(void)
{
	return 0;
}

/* ARGSUSED */
fp_except
fpsetmask(fp_except mask)
{
	return 0;
}

/* ARGSUSED */
fp_rnd
fpsetround(fp_rnd rnd_dir)
{
	return 0;
}

/* ARGSUSED */
fp_except
fpsetsticky(fp_except sticky)
{
	return 0;
}
