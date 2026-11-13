/* console.h */

#include <minix/chardriver.h>
#include <minix/timers.h>

struct cons_sw;
typedef void(*init_t) (struct tty *tp);
typedef int(*loadfont_t) (endpoint_t endpt, cp_grant_id_t grant);
typedef void(*stop_t) (void);
typedef void(*select_console_t) (int cons_line);
typedef void(*disable_console_t) (void);
typedef void(*reenable_console_t) (void);

typedef struct cons_sw {
	init_t sw_init;
	loadfont_t sw_loadfont;
	stop_t sw_stop;
	select_console_t sw_select_console;
	disable_console_t sw_disable_console;
	reenable_console_t sw_reenable_console;
} cons_sw_t;

struct consdev {
	void	(*cn_probe)	/* probe hardware and fill in consdev info */
		   (struct consdev *);
	void	(*cn_init)	/* turn on as console */
		   (struct consdev *);
	int	(*cn_getc)	/* kernel getchar interface */
		   (dev_t);
	void	(*cn_putc)	/* kernel putchar interface */
		   (dev_t, int);
	void	(*cn_pollc)	/* turn on and off polling */
		   (dev_t, int);
	void	(*cn_bell)	/* ring bell */
		   (dev_t, u_int, u_int, u_int);
	void	(*cn_halt)	/* stop device */
		   (dev_t);
	void	(*cn_flush)	/* flush output */
		   (dev_t);
	dev_t	cn_dev;		/* major/minor of device */
	int	cn_pri;		/* pecking order; the higher the better */
};

/* values for cn_pri - reflect our policy for console selection */
#define	CN_DEAD		0	/* device doesn't exist */
#define CN_NULL		1	/* noop console */
#define CN_NORMAL	2	/* device exists but is nothing special */
#define CN_INTERNAL	3	/* "internal" bit-mapped display */
#define CN_REMOTE	4	/* serial interface with remote bit set */

