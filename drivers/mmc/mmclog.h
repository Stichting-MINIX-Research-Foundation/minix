/* 
 * Simple logging functions for the MMC layer
 */

#define LEVEL_NONE 0
#define LEVEL_WARN 1
#define LEVEL_INFO 2
#define LEVEL_DEBUG 3

struct mmclog {
	int log_level;
	const char *name;

	/* the logging function itself */
	void (*log)(struct mmclog *driver, int level,const char *file, const char *function, int line, const char * fmt, ...);

};

#define __mmc_log(driver,log_level, fmt, args...) \
		((driver)->log(driver,log_level,  __FILE__, __FUNCTION__, __LINE__,fmt, ## args))

/* Log a warning */
#define mmc_log_warn(driver, fmt, args...) \
		__mmc_log(driver, LEVEL_INFO, fmt, ## args)

/* Log an information message  */
#define mmc_log_info(driver, fmt, args...) \
		__mmc_log(driver, LEVEL_DEBUG, fmt, ## args)

/* log debugging output  */
#define mmc_log_debug(driver, fmt, args...) \
		__mmc_log(driver, LEVEL_INFO, fmt, ## args)


void mmc_log (struct mmclog *driver, int level, const char *file, const char *function , int line, const char * fmt, ...)
{
	va_list args;

	if (level > driver->log_level){
		return;
	}
	/* If the wanted level is debug also display line/method information */
	if (driver->log_level >= LEVEL_DEBUG){
		fprintf(stderr,"%s(%d):%s+%d(%s):", driver->name,level,file,line,function);
	} else {
		fprintf(stderr,"%s(%d)", driver->name,level);
	}

	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
}
