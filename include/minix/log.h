#ifndef __LOG_H__
#define __LOG_H__
/*
 * Simple logging functions
 */

#include <stdarg.h>

/*
 * LEVEL_NONE  do not log anything.
 * LEVEL_WARN  Information that needs to be known.
 * LEVEL_INFO  Basic information like startup messages and occasional events.
 * LEVEL_DEBUG debug statements about things happening that are less expected.
 * LEVEL_TRACE Way to much information for anybody.
 */

#define LEVEL_NONE 0
#define LEVEL_WARN 1
#define LEVEL_INFO 2
#define LEVEL_DEBUG 3
#define LEVEL_TRACE 4

static const char *level_string[5] = {
	"none",
	"warn",
	"info",
	"debug",
	"trace"
};

/*
 * struct to be initialized by the user of the logging system.
 *
 * name: The name attribute is used in logging statements do differentiate
 * drivers
 *
 * log_level The level attribute describes the requested logging level. a level
 * of 1 will only print warnings while a level of 4 will print all the trace
 * information.
 *
 * log_func The logging function to use to log, log.h provides default_log
 * to display information on the kernel output buffer. As a bonus if the
 * requested log level is debug or trace the method , file and line number will
 * be printed to the steam.
 */
struct log
{
	const char *name;
	int log_level;

	/* the logging function itself */
	void (*log_func) (struct log * driver,
	    int level,
	    const char *file,
	    const char *function, int line, const char *fmt, ...);

};

#define __log(driver,log_level, fmt, args...) \
		((driver)->log_func(driver,log_level, \
				__FILE__, __FUNCTION__, __LINE__,\
				fmt, ## args))

/* Log a warning */
#define log_warn(driver, fmt, args...) \
		__log(driver, LEVEL_WARN, fmt, ## args)

/* Log an information message  */
#define log_info(driver, fmt, args...) \
		__log(driver, LEVEL_INFO, fmt, ## args)

/* log debugging output  */
#define log_debug(driver, fmt, args...) \
		__log(driver, LEVEL_DEBUG, fmt, ## args)

/* log trace output  */
#define log_trace(driver, fmt, args...) \
		__log(driver, LEVEL_TRACE, fmt, ## args)

#endif /* __LOG_H__ */

static void
default_log(struct log *driver,
    int level,
    const char *file, const char *function, int line, const char *fmt, ...)
{
	va_list args;

	if (level > driver->log_level) {
		return;
	}
	/* If the wanted level is debug also display line/method information */
	if (driver->log_level >= LEVEL_DEBUG) {
		fprintf(stderr, "%s(%s):%s+%d(%s):", driver->name,
		    level_string[level], file, line, function);
	} else {
		fprintf(stderr, "%s(%s)", driver->name, level_string[level]);
	}

	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
}

#ifdef hacks
static void
hexdump(unsigned char *d, unsigned int size)
{
	int s;
	for (s = 0; s < size; s += 4) {
		fprintf(stdout, "0x%04x 0x%02X%02X%02X%02X %c%c%c%c\n", s,
		    (unsigned int) d[s], (unsigned int) d[s + 1],
		    (unsigned int) d[s + 2], (unsigned int) d[s + 3], d[s],
		    d[s + 1], d[s + 2], d[s + 3]);
	}
}
#endif
