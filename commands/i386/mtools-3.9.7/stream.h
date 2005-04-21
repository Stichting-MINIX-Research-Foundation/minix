#ifndef MTOOLS_STREAM_H
#define MTOOLS_STREAM_H

typedef struct Stream_t {
	struct Class_t *Class;
	int refs;
	struct Stream_t *Next;
	struct Stream_t *Buffer;
} Stream_t;

#include "mtools.h"
#include "msdos.h"

#include "llong.h"

typedef struct Class_t {
	int (*read)(Stream_t *, char *, mt_off_t, size_t);
	int (*write)(Stream_t *, char *, mt_off_t, size_t);
	int (*flush)(Stream_t *);
	int (*freeFunc)(Stream_t *);
	int (*set_geom)(Stream_t *, device_t *, device_t *, int media,
					struct bootsector *);
	int (*get_data)(Stream_t *, time_t *, mt_size_t *, int *, int *);
	int (*pre_allocate)(Stream_t *, mt_size_t);
} Class_t;

#define READS(stream, buf, address, size) \
(stream)->Class->read( (stream), (char *) (buf), (address), (size) )

#define WRITES(stream, buf, address, size) \
(stream)->Class->write( (stream), (char *) (buf), (address), (size) )

#define SET_GEOM(stream, dev, orig_dev, media, boot) \
(stream)->Class->set_geom( (stream), (dev), (orig_dev), (media), (boot) )

#define GET_DATA(stream, date, size, type, address) \
(stream)->Class->get_data( (stream), (date), (size), (type), (address) )

#define PRE_ALLOCATE(stream, size) \
(stream)->Class->pre_allocate((stream), (size))

int flush_stream(Stream_t *Stream);
Stream_t *copy_stream(Stream_t *Stream);
int free_stream(Stream_t **Stream);

#define FLUSH(stream) \
flush_stream( (stream) )

#define FREE(stream) \
free_stream( (stream) )

#define COPY(stream) \
copy_stream( (stream) )


#define DeclareThis(x) x *This = (x *) Stream

int force_write(Stream_t *Stream, char *buf, mt_off_t start, size_t len);
int force_read(Stream_t *Stream, char *buf, mt_off_t start, size_t len);

extern struct Stream_t *default_drive;

int get_data_pass_through(Stream_t *Stream, time_t *date, mt_size_t *size,
						  int *type, int *address);

int read_pass_through(Stream_t *Stream, char *buf, mt_off_t start, size_t len);
int write_pass_through(Stream_t *Stream, char *buf, mt_off_t start, size_t len);


#endif

