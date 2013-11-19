/* Includes. */
#include <minix/drivers.h>
#include <minix/chardriver.h>
#include <minix/type.h>
#include <minix/const.h>
#include <minix/com.h>
#include <sys/types.h>
#include <minix/ipc.h>

/* Constants and types. */

#define LOG_SIZE	(50*1024) 

struct logdevice {
	char log_buffer[LOG_SIZE];
	int	log_size,	/* no. of bytes in log buffer */
		log_read,	/* read mark */
		log_write;	/* write mark */
	endpoint_t log_source;
	cdev_id_t log_id;
	int log_iosize,
		log_status;
	cp_grant_id_t log_grant;
	int	log_selected, log_select_proc;
};

/* Function prototypes. */
void do_new_kmess(void);
void log_append(char *buf, int len);

