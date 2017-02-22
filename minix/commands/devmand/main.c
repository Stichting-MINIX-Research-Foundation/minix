#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <errno.h>
#include <string.h>
#include <lib.h>
#include <sys/stat.h>
#include <dirent.h>
#include <assert.h>
#include <signal.h>
#include <minix/dmap.h>
#include <minix/paths.h>
#include "usb_driver.h"
#include "proto.h"

#define DEVMAN_TYPE_NAME "dev_type"
#define PATH_LEN 256
#define INVAL_MAJOR -1
#define MAX_CONFIG_DIRS 4

static void main_loop();
static void handle_event();
static void cleanup();
static void parse_config();
static void display_usage();
static enum dev_type determine_type(char *path);
static int get_major();
static void create_pid_file();
static void put_major(int major);
static struct devmand_usb_driver* match_usb_driver(struct usb_device_id *id);
static struct devmand_driver_instance *find_instance(int dev_id);

#define dbg(fmt, ... ) \
	if (args.verbose) \
	printf("%8s:%4d: %13s()| "fmt"\n", __FILE__, __LINE__, __func__,  ##__VA_ARGS__ )

static LIST_HEAD(usb_driver_head, devmand_usb_driver) drivers =
    LIST_HEAD_INITIALIZER(drivers);
static LIST_HEAD(usb_driver_inst_head, devmand_driver_instance) instances =
    LIST_HEAD_INITIALIZER(instances);


static int _run = 1;
struct global_args {
	char *path;
	char *config_dirs[MAX_CONFIG_DIRS];
	int config_dir_count ;
	int major_offset;
	int verbose;
	int check_config;
};

enum dev_type {
	DEV_TYPE_USB_DEVICE,
	DEV_TYPE_USB_INTF,
	DEV_TYPE_UNKOWN
};

extern FILE *yyin;

static struct global_args args = {
	.path = NULL,
	.config_dirs = {NULL,NULL,NULL,NULL},
	.config_dir_count = 0,
	.major_offset = USB_BASE_MAJOR,
	.verbose = 0,
	.check_config = 0};

static struct option options[] =
{
	{"dir"   ,    required_argument, NULL, 'd'},
	{"path",      required_argument, NULL, 'p'},
	{"verbose",    required_argument, NULL, 'v'},
	{"check-config", no_argument,       NULL, 'x'},
	{0,0,0,0} /* terminating entry */
};

static char major_bitmap[16]; /* can store up to 128 major number states */


/*===========================================================================*
 *             run_upscript                                                  *
 *===========================================================================*/
int run_upscript(struct devmand_driver_instance *inst)
{
	char cmdl[1024];
	cmdl[0] = 0;
	int ret;

	assert(inst->drv->upscript);
	assert(inst->label);

	snprintf(cmdl, 1024, "%s up %s %d %d",
	    inst->drv->upscript, inst->label, inst->major, inst->dev_id);
	dbg("Running Upscript:  \"%s\"", cmdl);
	ret = system(cmdl);
	if (ret != 0) {
		return EINVAL;
	}
	return 0;
}

/*===========================================================================*
 *             run_cleanscript                                               *
 *===========================================================================*/
int run_cleanscript(struct devmand_usb_driver *drv)
{
	char cmdl[1024];
	cmdl[0] = 0;
	int ret;

	assert(drv->upscript);
	assert(drv->devprefix);

	snprintf(cmdl, 1024, "%s clean %s ",
		drv->upscript, drv->devprefix);
	dbg("Running Upscript:  \"%s\"", cmdl);
	ret = system(cmdl);

	if (ret != 0) {
		return EINVAL;
	}

	return 0;
}


/*===========================================================================*
 *             run_downscript                                                *
 *===========================================================================*/
int run_downscript(struct devmand_driver_instance *inst)
{
	char cmdl[1024];
	cmdl[0] = 0;
	int ret;

	assert(inst->drv->downscript);
	assert(inst->label);

	snprintf(cmdl, 1024, "%s down %s %d",
	    inst->drv->downscript, inst->label, inst->major);

	dbg("Running Upscript:  \"%s\"", cmdl);

	ret = system(cmdl);

	if (ret != 0) {
		return EINVAL;
	}

	return 0;
}


/*===========================================================================*
 *             stop_driver                                                   *
 *===========================================================================*/
int stop_driver(struct devmand_driver_instance *inst)
{
	char cmdl[1024];
	cmdl[0] = 0;
	int ret;

	assert(inst->label);

	snprintf(cmdl, 1024, "%s down %s %d",
	    _PATH_MINIX_SERVICE, inst->label, inst->dev_id);
	dbg("executing minix-service: \"%s\"", cmdl);
	ret = system(cmdl);
	if (ret != 0)
	{
		return EINVAL;
	}
	printf("Stopped driver %s with label %s for device %d.\n",
		inst->drv->binary, inst->label, inst->dev_id);

	return 0;
}


/*===========================================================================*
 *             start_driver                                                  *
 *===========================================================================*/
int start_driver(struct devmand_driver_instance *inst)
{
	char cmdl[1024];
	cmdl[0] = 0;
	int ret;

	/* generate label */
	ret = snprintf(inst->label, 32,  "%s%d", inst->drv->devprefix,
		inst->dev_id);
	if (ret < 0 || ret > DEVMAND_DRIVER_LABEL_LEN) {
		dbg("label too long");
		return ENOMEM;
	}

	assert(inst->drv->binary);
	assert(inst->label);

	snprintf(cmdl, 1024, "%s up %s  -major %d -devid %d -label %s",
	    _PATH_MINIX_SERVICE, inst->drv->binary, inst->major, inst->dev_id,
		inst->label);
	dbg("executing minix-service: \"%s\"", cmdl);

	ret = system(cmdl);

	if (ret != 0) {
		return EINVAL;
	}

	printf("Started driver %s with label %s for device %d.\n",
		inst->drv->binary, inst->label, inst->dev_id);

	return 0;
}

/*===========================================================================*
 *             find_instance                                                 *
 *===========================================================================*/
static struct devmand_driver_instance *
find_instance(int dev_id)
{
	struct devmand_driver_instance *inst;

	LIST_FOREACH(inst, &instances, list) {
		if (inst->dev_id == dev_id) {
			return inst;
		}
	}
	return NULL;
}

/*===========================================================================*
 *              match_usb_driver                                             *
 *===========================================================================*/
static int
match_usb_id(struct devmand_usb_match_id *mid, struct usb_device_id *id)
{
	int res = 1;
	unsigned long match = mid->match_flags;
	struct usb_device_id *_id = &mid->match_id;

	if (match & USB_MATCH_ID_VENDOR)
		if (id->idVendor != _id->idVendor) res = 0;
	if (match & USB_MATCH_ID_PRODUCT)
		if (id->idProduct != _id->idProduct) res = 0;
	if (match & USB_MATCH_BCD_DEVICE)
		if (id->bcdDevice != _id->bcdDevice) res = 0;
	if (match & USB_MATCH_DEVICE_PROTOCOL)
		if (id->bDeviceProtocol != _id->bDeviceProtocol) res = 0;
	if (match & USB_MATCH_DEVICE_SUBCLASS)
		if (id->bDeviceSubClass != _id->bDeviceSubClass) res = 0;
	if (match & USB_MATCH_DEVICE_PROTOCOL)
		if (id->bDeviceProtocol != _id->bDeviceProtocol) res = 0;
	if (match & USB_MATCH_INTERFACE_CLASS)
		if (id->bInterfaceClass != _id->bInterfaceClass) res = 0;
	if (match & USB_MATCH_INTERFACE_SUBCLASS)
		if (id->bInterfaceSubClass != _id->bInterfaceSubClass) res = 0;
	if (match & USB_MATCH_INTERFACE_PROTOCOL)
		if (id->bInterfaceProtocol != _id->bInterfaceProtocol) res = 0;

	if (match == 0UL) {
		res = 0;
	}

	return res;
}

/*===========================================================================*
 *              match_usb_driver                                             *
 *===========================================================================*/
static struct devmand_usb_driver*
match_usb_driver(struct usb_device_id *id)
{
	struct devmand_usb_driver *driver;
	struct devmand_usb_match_id *mid;

	LIST_FOREACH(driver, &drivers, list) {
		LIST_FOREACH(mid, &driver->ids, list) {
			if (match_usb_id(mid, id)) {
				return driver;
			}
		}
	}
	return NULL;
}

/*===========================================================================*
 *              add_usb_match_id                                             *
 *===========================================================================*/
struct devmand_usb_driver * add_usb_driver(char *name)
{
	struct devmand_usb_driver *udrv = (struct devmand_usb_driver*)
	    malloc(sizeof(struct devmand_usb_driver));

	LIST_INSERT_HEAD(&drivers, udrv, list);
	LIST_INIT(&udrv->ids);

	udrv->name = name;
	return udrv;
}

/*===========================================================================*
 *              add_usb_match_id                                             *
 *===========================================================================*/
struct devmand_usb_match_id *
add_usb_match_id
(struct devmand_usb_driver *drv)
{
	struct devmand_usb_match_id *id = (struct devmand_usb_match_id*)
	    malloc(sizeof(struct devmand_usb_match_id));

	memset(id, 0, sizeof(struct devmand_usb_match_id));

	LIST_INSERT_HEAD(&drv->ids, id, list);

	return id;
}


/*===========================================================================*
 *           parse_config                                                    *
 *===========================================================================*/
static void parse_config()
{
	int i, status, error;
	struct stat stats;
	char * dirname;

	DIR * dir;
	struct dirent entry;
	struct dirent *result;
	char config_file[PATH_MAX];

	dbg("Parsing configuration directories... ");
	/* Next parse the configuration directories */
	for(i=0; i < args.config_dir_count; i++){
		dirname = args.config_dirs[i];
		dbg("Parsing config dir %s ", dirname);
		status = stat(dirname,&stats);
		if (status == -1){
			error = errno;
			dbg("Failed to read directory '%s':%s (skipping) \n", 
			    dirname,strerror(error));
			continue;
		}
		if (!S_ISDIR(stats.st_mode)){
			dbg("Parse configuration skipping %s "
			    "(not a directory) \n",dirname);
			continue;
		}
		dir = opendir(dirname);
		if (dir == NULL){
			error = errno;
			dbg("Parse configuration failed to read dir '%s'"
			    "(skipping) :%s\n",dirname, strerror(error));
			continue;
		}
		while( (status = readdir_r(dir,&entry,&result)) == 0 ){
			if (result == NULL){ /* last entry */ 
				closedir(dir);
				break;
			}

			/* concatenate dir and file name to open it */
			snprintf(config_file,PATH_MAX, "%s/%s",
				 dirname,entry.d_name);
			status = stat(config_file, &stats);
			if (status == -1){ 
				error = errno;
				dbg("Parse configuration Failed to stat file "
				    "'%s': %s (skipping)\n", config_file,
				    strerror(error));
			}
			if (S_ISREG(stats.st_mode)){
				dbg("Parsing file %s",config_file);
				yyin = fopen(config_file, "r");

				if (yyin == NULL) {
					dbg("Can not open config file:" 
				 	       " %d.\n", errno);
				}
				yyparse();
				dbg("Done.");
				fclose(yyin);
			}
		}
	}
	dbg("Parsing configuration directories done... ");

}

/*===========================================================================*
 *           cleanup                                                        *
 *===========================================================================*/
static void cleanup() {
	struct devmand_driver_instance *inst;
	/* destroy fifo */
	dbg("cleaning up... ");
	/* quit all running drivers */
	LIST_FOREACH(inst, &instances, list) {
		dbg("stopping driver %s", inst->label);
		if(inst->drv->downscript) {
			run_downscript (inst);
		}
		stop_driver(inst);
	}
	unlink("/var/run/devmand.pid");
}

static void sig_int(int sig) {
	dbg("devman: Received SIGINT... cleaning up.");
	_run = 0;
}

/*===========================================================================*
 *           create_pid_file                                                 *
 *===========================================================================*/
void create_pid_file()
{
	FILE *fd;

	fd = fopen("/var/run/devmand.pid", "r");
	if(fd) {
		fprintf(stderr, "devmand: /var/run/devmand.pid exists... "
		                "another devmand running?\n");
		fclose(fd);
		exit(1);
	} else {
		fd = fopen("/var/run/devmand.pid","w");
		fprintf(fd, "%d", getpid());
		fclose(fd);
	}
}

/*===========================================================================*
 *           main                                                            *
 *===========================================================================*/
int main(int argc, char *argv[])
{
	int opt, optindex;
	struct devmand_usb_driver *driver;


	/* get command line arguments */
	while ((opt = getopt_long(argc, argv, "d:p:vxh?", options, &optindex))
			!= -1) {
		switch (opt) {
			case 'd':/* config directory */
				if (args.config_dir_count >= MAX_CONFIG_DIRS){
				 	fprintf(stderr,"Parse arguments: Maximum" 
					        " of %i configuration directories"
						" reached skipping directory '%s'\n" 
						, MAX_CONFIG_DIRS, optarg);
				 	break;
				}
				args.config_dirs[args.config_dir_count] = optarg;
				args.config_dir_count++;
				break;
			case 'p': /* sysfs path */
				args.path = optarg;
				break;
			case 'v': /* verbose */
				args.verbose = 1;
				break;
			case 'x': /* check config */
				args.check_config = 1;
				break;
			case 'h': /* help */
			case '?': /* help */
			default:
				display_usage(argv[0]);
				return 0;
		}
	}


	/* is path set? */
	if (args.path == NULL) {
		args.path = "/sys/";
	}

	/* is the configuration directory set? */
	if (args.config_dir_count == 0) {
		dbg("Using default configuration directory");
		args.config_dirs[0] = "/etc/devmand";
		args.config_dir_count = 1;
	}

	/* If we only check the configuration run and exit imediately */
	if (args.check_config == 1){
		fprintf(stdout, "Only parsing configuration\n");
		parse_config();
		exit(0);
	}

	create_pid_file();

	parse_config();
	LIST_FOREACH(driver, &drivers, list) {
		if (driver->upscript) {
			run_cleanscript(driver);
		}
	}

	signal(SIGINT, sig_int);

	main_loop();

	cleanup();

	return 0;
}

/*===========================================================================*
 *           determine_type                                                  *
 *===========================================================================*/
static enum dev_type determine_type (char *path)
{
	FILE * fd;
	char *mypath;
	char buf[256];
	int res;

	mypath = (char *) calloc(1, strlen(path)+strlen(DEVMAN_TYPE_NAME)+1);

	if (mypath == NULL) {
		fprintf(stderr, "ERROR: out of mem\n");
		cleanup();
		exit(1);
	}

	strcat(mypath, path);
	strcat(mypath, DEVMAN_TYPE_NAME);

	fd = fopen(mypath, "r");
	free(mypath);

	if (fd == NULL) {
		fprintf(stderr, "WARN: could not open %s\n", mypath);
		return DEV_TYPE_UNKOWN;
	}

	res = fscanf(fd , "%255s\n", buf);
	fclose(fd);

	if (res != 1) {
		fprintf(stderr, "WARN: could not parse %s\n", mypath);
		return DEV_TYPE_UNKOWN;
	}

	if (strcmp(buf, "USB_DEV") == 0) {
		return DEV_TYPE_USB_DEVICE;
	} else if (strcmp(buf, "USB_INTF") == 0) {
		return DEV_TYPE_USB_INTF;
	}

	return  DEV_TYPE_UNKOWN;
}

/*===========================================================================*
 *           read_hex_uint                                                   *
 *===========================================================================*/
static int read_hex_uint(char *base_path, char *name, unsigned int* val )
{
	char my_path[PATH_LEN];
	FILE *fd;
	memset(my_path,0,PATH_LEN);
	int ret = 0;

	strcat(my_path, base_path);
	strcat(my_path, name);

	fd = fopen(my_path, "r");

	if (fd == NULL) {
		fprintf(stderr, "WARN: could not open %s\n", my_path);
		return EEXIST;
	} else	if (fscanf(fd, "0x%x\n", val ) != 1) {
		fprintf(stderr, "WARN: could not parse %s\n", my_path);
		ret = EINVAL;
	}
	fclose(fd);

	return ret;
}

/*===========================================================================*
 *               get_major                                                   *
 *===========================================================================*/
static int get_major() {
	int i, ret = args.major_offset;

	for (i=0; i < 16; i++) {
		int j;
		for (j = 0; j < 8; j++ ) {
			if ((major_bitmap[i] & (1 << j))) {
				major_bitmap[i] &= !(1 << j);
				return ret;
			}
			ret++;
		}
	}
	return INVAL_MAJOR;
}

/*===========================================================================*
 *               put_major                                                   *
 *===========================================================================*/
static void put_major(int major) {
	int i;
	major -= args.major_offset;
	assert(major >= 0);

	for (i=0; i < 16; i++) {
		int j;
		for (j = 0; j < 8; j++ ) {
			if (major==0) {
				assert(!(major_bitmap[i] & (1 <<j)));
				major_bitmap[i] |= (1 << j);
				return;
			}
			major--;
		}
	}
}

/*===========================================================================*
 *          generate_usb_device_id                                           *
 *===========================================================================*/
static struct usb_device_id *
generate_usb_device_id(char * path, int is_interface)
{
	struct usb_device_id *ret;
	int res;
	unsigned int val;

	ret = (struct usb_device_id *)
	    calloc(1,sizeof (struct usb_device_id));

	if (is_interface) {

		res = read_hex_uint(path, "../idVendor", &val);
		if (res) goto err;
		ret->idVendor = val;

		res = read_hex_uint(path, "../idProduct", &val);
		if (res) goto err;
		ret->idProduct = val;
#if 0
		res = read_hex_uint(path, "../bcdDevice", &val);
		if (res) goto err;
		ret->bcdDevice = val;
#endif
		res = read_hex_uint(path, "../bDeviceClass", &val);
		if (res) goto err;
		ret->bDeviceClass = val;

		res = read_hex_uint(path, "../bDeviceSubClass", &val);
		if (res) goto err;
		ret->bDeviceSubClass = val;

		res = read_hex_uint(path, "../bDeviceProtocol", &val);
		if (res) goto err;
		ret->bDeviceProtocol = val;

		res = read_hex_uint(path, "/bInterfaceClass", &val);
		if (res) goto err;
		ret->bInterfaceClass = val;

		res = read_hex_uint(path, "/bInterfaceSubClass", &val);
		if (res) goto err;
		ret->bInterfaceSubClass = val;

		res = read_hex_uint(path, "/bInterfaceProtocol", &val);
		if (res) goto err;
		ret->bInterfaceProtocol = val;
	}

	return ret;

err:
	free(ret);
	return NULL;
}

/*===========================================================================*
 *            usb_intf_add_even                                              *
 *===========================================================================*/
static void usb_intf_add_event(char *path, int dev_id)
{
	struct usb_device_id *id;
	struct devmand_usb_driver *drv;
	struct devmand_driver_instance *drv_inst;
	int major, ret;

	/* generate usb_match_id */
	id = generate_usb_device_id(path,TRUE);
	if (id == NULL) {
		fprintf(stderr, "WARN: could not create usb_device id...\n"
		                "      ommiting event\n");
		free(id);
		return;
	}

	/* find suitable driver */
	drv = match_usb_driver(id);
	free (id);

	if (drv == NULL) {
		dbg("INFO: could not find a suitable driver for %s", path);
		return;
	}

	/* create instance */
	drv_inst = (struct devmand_driver_instance *)
	    calloc(1,sizeof(struct devmand_driver_instance));

	if (drv_inst == NULL) {
		fprintf(stderr, "ERROR: out of memory");
		return; /* maybe better quit here. */
	}


	/* allocate inode number, if device files needed */
	major = get_major();
	if (major == INVAL_MAJOR) {
		fprintf(stderr, "WARN: ran out of major numbers\n"
		                "      cannot start driver %s for %s\n",
							   drv->name, path);
		return;
	}

	drv_inst->major  = major;
	drv_inst->drv    = drv;
	drv_inst->dev_id = dev_id;


	/* start driver (invoke minix-service) */
	start_driver(drv_inst);

	/*
	 * run the up action
	 *
	 * An up action can be any executable. Before running it devmand
	 * will set certain environment variables so the script can configure
	 * the device (or generate device files, etc). See up_action() for that.
	 */
	if (drv->upscript) {
		ret = run_upscript(drv_inst);
		if (ret) {
			stop_driver(drv_inst);
			fprintf(stderr, "devmand: warning, could not run up_action\n");
			free(drv_inst);
			return;
		}
	}

	LIST_INSERT_HEAD(&instances,drv_inst,list);
}

/*===========================================================================*
 *            usb_intf_remove_event                                          *
 *===========================================================================*/
static void usb_intf_remove_event(char *path, int dev_id)
{
	struct devmand_driver_instance *inst;
	struct devmand_usb_driver *drv;
	int ret;

	/* find the driver instance */
	inst = find_instance(dev_id);

	if (inst == NULL) {
		dbg("No driver running for id: %d", dev_id);
		return;
	}
	drv = inst->drv;

	/* run the down script */
	if (drv->downscript) {
		ret = run_downscript(inst);
		if (ret) {
			fprintf(stderr, "WARN: error running up_action");
		}
	}

	/* stop the driver */
	stop_driver(inst);

	/* free major */
	put_major(inst->major);

	/* free instance */
	LIST_REMOVE(inst,list);
	free(inst);
}

/*===========================================================================*
 *           handle_event                                                    *
 *===========================================================================*/
static void handle_event(char *event)
{
	enum dev_type type;
	char path[PATH_LEN];
	char tmp_path[PATH_LEN];
	int dev_id, res;

	path[0]=0;

	if (strncmp("ADD ", event, 4) == 0) {

		/* read data from event */
		res = sscanf(event, "ADD %s 0x%x", tmp_path, &dev_id);

		if (res != 2) {
			fprintf(stderr, "WARN: could not parse event: %s", event);
			fprintf(stderr, "WARN: omitting event: %s", event);
		}

		strcpy(path, args.path);
		strcat(path, tmp_path);

		/* what kind of device is added? */
		type = determine_type(path);

		switch (type) {
			case DEV_TYPE_USB_DEVICE:
				dbg("USB device added: ommited....");
				/* ommit usb devices for now */
				break;
			case DEV_TYPE_USB_INTF:
				dbg("USB interface added: (%s, devid: = %d)",path, dev_id);
				usb_intf_add_event(path, dev_id);
				return;
			default:
				dbg("default");
				fprintf(stderr, "WARN: ommiting event\n");
		}
	} else if (strncmp("REMOVE ", event, 7) == 0) {

		/* read data from event */
		res = sscanf(event,"REMOVE %s 0x%x", tmp_path, &dev_id);

		if (res != 2) {
			fprintf(stderr, "WARN: could not parse event: %s", event);
			fprintf(stderr, "WARN: omitting event: %s", event);
		}

		usb_intf_remove_event(path, dev_id);

#if 0
		strcpy(path, args.path);
		strcat(path, tmp_path);

		/* what kind of device is added? */
		type = determine_type(path);

		switch (type) {
			case DEV_TYPE_USB_DEVICE:
				/* ommit usb devices for now */
				break;
			case DEV_TYPE_USB_INTF:
				usb_intf_remove_event(path, dev_id);
				return;
			default:
				fprintf(stderr, "WARN: ommiting event\n");
		}
#endif

	}
}

/*===========================================================================*
 *           main_loop                                                       *
 *===========================================================================*/
static void main_loop()
{
	char ev_path[128];
	char buf[256];
	int len;
	FILE* fd;
	len = strlen(args.path);

	/* init major numbers */

	memset(&major_bitmap, 0xff, 16);

	if (len > 128 - 7 /*len of "events" */) {
		fprintf(stderr, "pathname to long\n");
		cleanup();
		exit(1);
	}

	strcpy(ev_path, args.path);
	strcat(ev_path, "events");


	while (_run) {

		char *res;

		fd = fopen(ev_path, "r");
		if (fd == NULL) {
			/*
			 * ENFILE is a temporary failure, often caused by
			 * running the test set.  Don't die from that..
			 */
			if (errno == ENFILE) {
				usleep(50000);
				continue;
			}

			fprintf(stderr,"devmand error: could not open event "
				"file %s bailing out\n", ev_path);
			cleanup();
			exit(1);
		}

		res = fgets(buf, 256, fd);
		fclose(fd);

		if (res == NULL) {
			usleep(50000);
			continue;
		}
		dbg("handle_event:  %s", buf);
		handle_event(buf);
	}
}

/*===========================================================================*
 *           display_usage                                                   *
 *===========================================================================*/
static void display_usage(const char *name)
{
	printf("Usage: %s [{-p|--pathname} PATH_TO_SYS}"
	       " [{-d|--config-dir} CONFIG_DIR] [-v|--verbose]" 
	       " [[x||--check-config]\n", name);
}

