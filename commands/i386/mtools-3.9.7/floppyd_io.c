/*
 * IO to the floppyd daemon running on the local X-Server Host
 *
 * written by:
 *
 * Peter Schlaile
 *
 * udbz@rz.uni-karlsruhe.de
 *
 */

#include "sysincludes.h"
#include "stream.h"
#include "mtools.h"
#include "msdos.h"
#include "scsi.h"
#include "partition.h"
#include "floppyd_io.h"

#ifdef USE_FLOPPYD

/* ######################################################################## */


typedef unsigned char Byte;
typedef unsigned long Dword;

char* AuthErrors[] = {
	"Auth success!",
	"Auth failed: Packet oversized!",
	"Auth failed: X-Cookie doesn't match!",
	"Auth failed: Wrong transmission protocol version!",
	"Auth failed: Device locked!"
};


typedef struct RemoteFile_t {
	Class_t *Class;
	int refs;
	Stream_t *Next;
	Stream_t *Buffer;
	int fd;
	mt_off_t offset;
	mt_off_t lastwhere;
	mt_off_t size;
} RemoteFile_t;


#ifndef HAVE_HTONS
unsigned short myhtons(unsigned short parm) 
{
	Byte val[2];
	
	val[0] = (parm >> 8) & 0xff;
	val[1] = parm        & 0xff;

	return *((unsigned short*) (val));
}
#endif

Dword byte2dword(Byte* val) 
{
	Dword l;
	l = (val[0] << 24) + (val[1] << 16) + (val[2] << 8) + val[3];

	return l;
}	

void dword2byte(Dword parm, Byte* rval) 
{
	rval[0] = (parm >> 24) & 0xff;
	rval[1] = (parm >> 16) & 0xff;
	rval[2] = (parm >> 8)  & 0xff;
	rval[3] = parm         & 0xff;
}

Dword read_dword(int handle) 
{
	Byte val[4];
	
	read(handle, val, 4);

	return byte2dword(val);
}

void write_dword(int handle, Dword parm) 
{
	Byte val[4];

	dword2byte(parm, val);

	write(handle, val, 4);
}


/* ######################################################################## */

int authenticate_to_floppyd(int sock, char *display)
{
	off_t filelen;
	Byte buf[16];
	char *command[] = { "xauth", "xauth", "extract", "-", 0, 0 };
	char *xcookie;
	Dword errcode;

	command[4] = display;

	filelen=strlen(display);
	filelen += 100;

	xcookie = (char *) safe_malloc(filelen+4);
	filelen = safePopenOut(command, xcookie+4, filelen);
	if(filelen < 1)
		return AUTH_AUTHFAILED;

	dword2byte(4,buf);
	dword2byte(FLOPPYD_PROTOCOL_VERSION,buf+4);
	write(sock, buf, 8);

	if (read_dword(sock) != 4) {
		return AUTH_WRONGVERSION;
	}

	errcode = read_dword(sock);

	if (errcode != AUTH_SUCCESS) {
		return errcode;
	}

	dword2byte(filelen, xcookie);
	write(sock, xcookie, filelen+4);

	if (read_dword(sock) != 4) {
		return AUTH_PACKETOVERSIZE;
	}

	errcode = read_dword(sock);
	
	return errcode;
}


static int floppyd_reader(int fd, char* buffer, int len) 
{
	Dword errcode;
	Dword gotlen;
	int l;
	int start;
	Byte buf[16];

	dword2byte(1, buf);
	buf[4] = OP_READ;
	dword2byte(4, buf+5);
	dword2byte(len, buf+9);
	write(fd, buf, 13);

	if (read_dword(fd) != 8) {
		errno = EIO;
		return -1;
	}

	gotlen = read_dword(fd);
	errcode = read_dword(fd);

	if (gotlen != -1) {
		if (read_dword(fd) != gotlen) {
			errno = EIO;
			return -1;
		}
		for (start = 0, l = 0; start < gotlen; start += l) {
			l = read(fd, buffer+start, gotlen-start);
			if (l == 0) {
				errno = EIO;
				return -1;
			}
		}
	} else {
		errno = errcode;
	}
	return gotlen;
}

static int floppyd_writer(int fd, char* buffer, int len) 
{
	Dword errcode;
	Dword gotlen;
	Byte buf[16];

	dword2byte(1, buf);
	buf[4] = OP_WRITE;
	dword2byte(len, buf+5);

	write(fd, buf, 9);
        write(fd, buffer, len);
	
	if (read_dword(fd) != 8) {
		errno = EIO;
		return -1;
	}

	gotlen = read_dword(fd);
	errcode = read_dword(fd);

	errno = errcode;
	
	return gotlen;
}

static int floppyd_lseek(int fd, mt_off_t offset, int whence) 
{
	Dword errcode;
	Dword gotlen;
	Byte buf[32];
	
	dword2byte(1, buf);
	buf[4] = OP_SEEK;
	
	dword2byte(8, buf+5);
	dword2byte(truncBytes32(offset), buf+9);
	dword2byte(whence, buf+13);
	
	write(fd, buf, 17);
       
	if (read_dword(fd) != 8) {
		errno = EIO;
		return -1;
	}

	gotlen = read_dword(fd);
	errcode = read_dword(fd);

	errno = errcode;
	
	return gotlen;
}

/* ######################################################################## */

typedef int (*iofn) (int, char *, int);

static int floppyd_io(Stream_t *Stream, char *buf, mt_off_t where, int len,
		   iofn io)
{
	DeclareThis(RemoteFile_t);
	int ret;

	where += This->offset;

	if (where != This->lastwhere ){
		if(floppyd_lseek( This->fd, where, SEEK_SET) < 0 ){
			perror("floppyd_lseek");
			This->lastwhere = (mt_off_t) -1;
			return -1;
		}
	}
	ret = io(This->fd, buf, len);
	if ( ret == -1 ){
		perror("floppyd_io");
		This->lastwhere = (mt_off_t) -1;
		return -1;
	}
	This->lastwhere = where + ret;
	return ret;
}

static int floppyd_read(Stream_t *Stream, char *buf, mt_off_t where, size_t len)
{	
	return floppyd_io(Stream, buf, where, len, (iofn) floppyd_reader);
}

static int floppyd_write(Stream_t *Stream, char *buf, mt_off_t where, size_t len)
{
	return floppyd_io(Stream, buf, where, len, (iofn) floppyd_writer);
}

static int floppyd_flush(Stream_t *Stream)
{
#if 0
	Byte buf[16];

	DeclareThis(RemoteFile_t);

	dword2byte(1, buf);
	buf[4] = OP_FLUSH;

	write(This->fd, buf, 5);

	if (read_dword(This->fd) != 8) {
		errno = EIO;
		return -1;
	}

	read_dword(This->fd);
	read_dword(This->fd);
#endif
	return 0;
}

static int floppyd_free(Stream_t *Stream)
{
	Byte buf[16];

	DeclareThis(RemoteFile_t);

	if (This->fd > 2) {
		dword2byte(1, buf);
		buf[4] = OP_CLOSE;
		write(This->fd, buf, 5);
		return close(This->fd);
	} else {
		return 0;
	}
}

static int floppyd_geom(Stream_t *Stream, struct device *dev, 
		     struct device *orig_dev,
		     int media, struct bootsector *boot)
{
	size_t tot_sectors;
	int sect_per_track;
	DeclareThis(RemoteFile_t);

	dev->ssize = 2; /* allow for init_geom to change it */
	dev->use_2m = 0x80; /* disable 2m mode to begin */

	if(media == 0xf0 || media >= 0x100){		
		dev->heads = WORD(nheads);
		dev->sectors = WORD(nsect);
		tot_sectors = DWORD(bigsect);
		SET_INT(tot_sectors, WORD(psect));
		sect_per_track = dev->heads * dev->sectors;
		tot_sectors += sect_per_track - 1; /* round size up */
		dev->tracks = tot_sectors / sect_per_track;

	} else if (media >= 0xf8){
		media &= 3;
		dev->heads = old_dos[media].heads;
		dev->tracks = old_dos[media].tracks;
		dev->sectors = old_dos[media].sectors;
		dev->ssize = 0x80;
		dev->use_2m = ~1;
	} else {
		fprintf(stderr,"Unknown media type\n");
		exit(1);
	}

	This->size = (mt_off_t) 512 * dev->sectors * dev->tracks * dev->heads;

	return 0;
}


static int floppyd_data(Stream_t *Stream, time_t *date, mt_size_t *size,
		     int *type, int *address)
{
	DeclareThis(RemoteFile_t);

	if(date)
		/* unknown, and irrelevant anyways */
		*date = 0;
	if(size)
		/* the size derived from the geometry */
		*size = (mt_size_t) This->size;
	if(type)
		*type = 0; /* not a directory */
	if(address)
		*address = 0;
	return 0;
}

/* ######################################################################## */

static Class_t FloppydFileClass = {
	floppyd_read, 
	floppyd_write,
	floppyd_flush,
	floppyd_free,
	floppyd_geom,
	floppyd_data
};

/* ######################################################################## */

int get_host_and_port(const char* name, char** hostname, char **display,
					  short* port)
{
	char* newname = strdup(name);
	char* p;
	char* p2;

	p = newname;
	while (*p != '/' && *p) p++;
	p2 = p;
	if (*p) p++;
	*p2 = 0;
	
	*port = atoi(p);
	if (*port == 0) {
		*port = FLOPPYD_DEFAULT_PORT;	
	}

	*display = strdup(newname);

	p = newname;
	while (*p != ':' && *p) p++;
	p2 = p;
	if (*p) p++;
	*p2 = 0;

	*port += atoi(p);  /* add display number to the port */

	if (!*newname || strcmp(newname, "unix") == 0) {
		free(newname);
		newname = strdup("localhost");
	}

	*hostname = newname;
	return 1;
}

/*
 *  * Return the IP address of the specified host.
 *  */
static IPaddr_t getipaddress(char *ipaddr)
{
	
	struct hostent  *host;
	IPaddr_t        ip;

	if (((ip = inet_addr(ipaddr)) == INADDR_NONE) &&
	    (strcmp(ipaddr, "255.255.255.255") != 0)) {
		
		if ((host = gethostbyname(ipaddr)) != NULL) {
			memcpy(&ip, host->h_addr, sizeof(ip));
		}
		
		endhostent();
	}
	
#ifdef DEBUG
	fprintf(stderr, "IP lookup %s -> 0x%08lx\n", ipaddr, ip);
#endif
	  
	return (ip);
}

/*
 *  * Connect to the floppyd server.
 *  */
static int connect_to_server(IPaddr_t ip, short port)
{
	
	struct sockaddr_in      addr;
	int                     sock;
	
	/*
	 * Allocate a socket.
	 */
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		return (-1);
	}
	
	/*
	 * Set the address to connect to.
	 */
	
	addr.sin_family = AF_INET;
#ifndef HAVE_HTONS
	addr.sin_port = myhtons(port);
#else	
	addr.sin_port = htons(port);
#endif	
	addr.sin_addr.s_addr = ip;
	
        /*
	 * Connect our socket to the above address.
	 */
	if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		return (-1);
	}

        /*
	 * Set the keepalive socket option to on.
	 */
	{
		int             on = 1;
		setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, 
			   (char *)&on, sizeof(on));
	}
	
	return (sock);
}

static int ConnectToFloppyd(const char* name);

Stream_t *FloppydOpen(struct device *dev, struct device *dev2,
					  char *name, int mode, char *errmsg,
					  int mode2, int locked)
{
	RemoteFile_t *This;

	if (!dev ||  !(dev->misc_flags & FLOPPYD_FLAG))
		return NULL;
	
	This = New(RemoteFile_t);
	if (!This){
		printOom();
		return NULL;
	}
	This->Class = &FloppydFileClass;
	This->Next = 0;
	This->offset = 0;
	This->lastwhere = 0;
	This->refs = 1;
	This->Buffer = 0;

	This->fd = ConnectToFloppyd(name);
	if (This->fd == -1) {
		Free(This);
		return NULL;
	}
	return (Stream_t *) This;
}

static int ConnectToFloppyd(const char* name) 
{
	char* hostname;
	char* display;
	short port;
	int rval = get_host_and_port(name, &hostname, &display, &port);
	int sock;
	int reply;
	
	if (!rval) return -1;

	sock = connect_to_server(getipaddress(hostname), port);

	if (sock == -1) {
		fprintf(stderr,
			"Can't connect to floppyd server on %s, port %i!\n",
			hostname, port);
		return -1;
	}
	
	reply = authenticate_to_floppyd(sock, display);

	if (reply != 0) {
		fprintf(stderr, 
			"Permission denied, authentication failed!\n"
			"%s\n", AuthErrors[reply]);
		return -1;
	}
	
	free(hostname);
	free(display);

	return sock;
}
#endif
