/*
 * misc - data and miscellaneous routines
 */
/* $Header$ */

#include	<ctype.h>
#include	<time.h>
#include	<stdlib.h>
#include	<string.h>

#include	"loc_time.h"

#define	RULE_LEN	120
#define	TZ_LEN		10

/* Make sure that the strings do not end up in ROM.
 * These strings probably contain the wrong value, and we cannot obtain the
 * right value from the system. TZ is the only help.
 */

long	_timezone = 0;
long	_dst_off = 60 * 60;
int	_daylight = 0;

long	timezone = 0;
int	daylight = 0;


static struct dsttype {
	char ds_type;		/* Unknown, Julian, Zero-based or M */
	int ds_date[3];		/* months, weeks, days */
	long ds_sec;		/* usually 02:00:00 */
};

const char *_days[] = {
			"Sunday", "Monday", "Tuesday", "Wednesday",
			"Thursday", "Friday", "Saturday"
		};

const char *_months[] = {
			"January", "February", "March",
			"April", "May", "June",
			"July", "August", "September",
			"October", "November", "December"
		};

const int _ytab[2][12] = {
		{ 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },
		{ 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 }
	};

