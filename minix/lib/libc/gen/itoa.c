#include <lib.h>
/* Integer to ASCII for signed decimal integers. */

static char	qbuf[12];

char *itoa(int n);

char *itoa(int n)
{
	register int	r;
	register int	k;
	int				flag = 0;
	long long		nb = n;
	int				next = 0;

	if (nb < 0)
	{
		qbuf[next++] = '-';
		nb = -nb;
	}
	if (nb == 0)
	{
		qbuf[next++] = '0';
	}
	else
	{
		k = 1000000000;
		while (k > 0)
		{
			r = nb / k;
			if (flag || r > 0)
			{
				qbuf[next++] = '0' + r;
				flag = 1;
			}
			nb -= r * k;
			k = k / 10;
		}
	}
	qbuf[next] = 0;
	return(qbuf);
}
