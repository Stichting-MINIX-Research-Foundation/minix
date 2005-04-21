/*
 * Buffer read/write module
 */

#include "sysincludes.h"
#include "msdos.h"
#include "mtools.h"
#include "buffer.h"

typedef struct Buffer_t {
	Class_t *Class;
	int refs;
	Stream_t *Next;
	Stream_t *Buffer;
	
	size_t size;     	/* size of read/write buffer */
	int dirty;	       	/* is the buffer dirty? */

	int sectorSize;		/* sector size: all operations happen
				 * in multiples of this */
	int cylinderSize;	/* cylinder size: preferred alignemnt,
				 * but for efficiency, less data may be read */
	int ever_dirty;	       	/* was the buffer ever dirty? */
	int dirty_pos;
	int dirty_end;
	mt_off_t current;		/* first sector in buffer */
	size_t cur_size;		/* the current size */
	char *buf;		/* disk read/write buffer */
} Buffer_t;

/*
 * Flush a dirty buffer to disk.  Resets Buffer->dirty to zero.
 * All errors are fatal.
 */

static int _buf_flush(Buffer_t *Buffer)
{
	int ret;

	if (!Buffer->Next || !Buffer->dirty)
		return 0;
	if(Buffer->current < 0L) {
		fprintf(stderr,"Should not happen\n");
		return -1;
	}
#ifdef DEBUG
	fprintf(stderr, "write %08x -- %02x %08x %08x\n",
		Buffer,
		(unsigned char) Buffer->buf[0],
		Buffer->current + Buffer->dirty_pos,
		Buffer->dirty_end - Buffer->dirty_pos);
#endif

	ret = force_write(Buffer->Next, 
			  Buffer->buf + Buffer->dirty_pos,
			  Buffer->current + Buffer->dirty_pos,
			  Buffer->dirty_end - Buffer->dirty_pos);
	if(ret != Buffer->dirty_end - Buffer->dirty_pos) {
		if(ret < 0)
			perror("buffer_flush: write");
		else
			fprintf(stderr,"buffer_flush: short write\n");
		return -1;
	}
	Buffer->dirty = 0;
	Buffer->dirty_end = 0;
	Buffer->dirty_pos = 0;
	return 0;
}

static int invalidate_buffer(Buffer_t *Buffer, mt_off_t start)
{
	/*fprintf(stderr, "invalidate %x\n", Buffer);*/
	if(Buffer->sectorSize == 32) {
		fprintf(stderr, "refreshing directory\n");
	}

	if(_buf_flush(Buffer) < 0)
		return -1;

	/* start reading at the beginning of start's sector
	 * don't start reading too early, or we might not even reach
	 * start */
	Buffer->current = ROUND_DOWN(start, Buffer->sectorSize);
	Buffer->cur_size = 0;
	return 0;
}

#undef OFFSET
#define OFFSET (start - This->current)

typedef enum position_t {
	OUTSIDE,
	APPEND,
	INSIDE,
	ERROR 
} position_t;

static position_t isInBuffer(Buffer_t *This, mt_off_t start, size_t *len)
{
	if(start >= This->current &&
	   start < This->current + This->cur_size) {
		maximize(*len, This->cur_size - OFFSET);
		return INSIDE;
	} else if(start == This->current + This->cur_size &&
		  This->cur_size < This->size &&
		  *len >= This->sectorSize) {
		/* append to the buffer for this, three conditions have to
		 * be met:
		 *  1. The start falls exactly at the end of the currently
		 *     loaded data
		 *  2. There is still space
		 *  3. We append at least one sector
		 */
		maximize(*len, This->size - This->cur_size);
		*len = ROUND_DOWN(*len, This->sectorSize);
		return APPEND;
	} else {
		if(invalidate_buffer(This, start) < 0)
			return ERROR;
		maximize(*len, This->cylinderSize - OFFSET);
		maximize(*len, This->cylinderSize - This->current % This->cylinderSize);
		return OUTSIDE;
	}
}

static int buf_read(Stream_t *Stream, char *buf, mt_off_t start, size_t len)
{
	size_t length;
	int offset;
	char *disk_ptr;
	int ret;
	DeclareThis(Buffer_t);	

	if(!len)
		return 0;	

	/*fprintf(stderr, "buf read %x   %x %x\n", Stream, start, len);*/
	switch(isInBuffer(This, start, &len)) {
		case OUTSIDE:
		case APPEND:
			/* always load until the end of the cylinder */
			length = This->cylinderSize -
				(This->current + This->cur_size) % This->cylinderSize;
			maximize(length, This->size - This->cur_size);

			/* read it! */
			ret=READS(This->Next,
				  This->buf + This->cur_size,
				  This->current + This->cur_size,
				  length);
			if ( ret < 0 )
				return ret;
			This->cur_size += ret;
			if (This->current+This->cur_size < start) {
				fprintf(stderr, "Short buffer fill\n");
				exit(1);
			}														  
			break;
		case INSIDE:
			/* nothing to do */
			break;
		case ERROR:
			return -1;
	}

	offset = OFFSET;
	disk_ptr = This->buf + offset;
	maximize(len, This->cur_size - offset);
	memcpy(buf, disk_ptr, len);
	return len;
}

static int buf_write(Stream_t *Stream, char *buf, mt_off_t start, size_t len)
{
	char *disk_ptr;
	DeclareThis(Buffer_t);	
	int offset, ret;

	if(!len)
		return 0;

	This->ever_dirty = 1;

#ifdef DEBUG
	fprintf(stderr, "buf write %x   %02x %08x %08x -- %08x %08x -- %08x\n", 
		Stream, (unsigned char) This->buf[0],
		start, len, This->current, This->cur_size, This->size);
	fprintf(stderr, "%d %d %d %x %x\n", 
		start == This->current + This->cur_size,
		This->cur_size < This->size,
		len >= This->sectorSize, len, This->sectorSize);
#endif
	switch(isInBuffer(This, start, &len)) {
		case OUTSIDE:
#ifdef DEBUG
			fprintf(stderr, "outside\n");
#endif
			if(start % This->cylinderSize || 
			   len < This->sectorSize) {
				size_t readSize;

				readSize = This->cylinderSize - 
					This->current % This->cylinderSize;

				ret=READS(This->Next, This->buf, This->current, readSize);
				/* read it! */
				if ( ret < 0 )
					return ret;
				This->cur_size = ret;
				/* for dosemu. Autoextend size */
				if(!This->cur_size) {
					memset(This->buf,0,readSize);
					This->cur_size = readSize;
				}
				offset = OFFSET;
				break;
			}
			/* FALL THROUGH */
		case APPEND:
#ifdef DEBUG
			fprintf(stderr, "append\n");
#endif
			len = ROUND_DOWN(len, This->sectorSize);
			offset = OFFSET;
			maximize(len, This->size - offset);
			This->cur_size += len;
			if(This->Next->Class->pre_allocate)
				PRE_ALLOCATE(This->Next,
							 This->current + This->cur_size);
			break;
		case INSIDE:
			/* nothing to do */
#ifdef DEBUG
			fprintf(stderr, "inside\n");
#endif
			offset = OFFSET;
			maximize(len, This->cur_size - offset);
			break;
		case ERROR:
			return -1;
		default:
#ifdef DEBUG
			fprintf(stderr, "Should not happen\n");
#endif
			exit(1);
	}

	disk_ptr = This->buf + offset;

	/* extend if we write beyond end */
	if(offset + len > This->cur_size) {
		len -= (offset + len) % This->sectorSize;
		This->cur_size = len + offset;
	}

	memcpy(disk_ptr, buf, len);
	if(!This->dirty || offset < This->dirty_pos)
		This->dirty_pos = ROUND_DOWN(offset, This->sectorSize);
	if(!This->dirty || offset + len > This->dirty_end)
		This->dirty_end = ROUND_UP(offset + len, This->sectorSize);
	
	if(This->dirty_end > This->cur_size) {
		fprintf(stderr, 
			"Internal error, dirty end too big %x %x %x %d %x\n",
			This->dirty_end, (unsigned int) This->cur_size, (unsigned int) len, 
				(int) offset, (int) This->sectorSize);
		fprintf(stderr, "offset + len + grain - 1 = %x\n",
				(int) (offset + len + This->sectorSize - 1));
		fprintf(stderr, "ROUNDOWN(offset + len + grain - 1) = %x\n",
				(int)ROUND_DOWN(offset + len + This->sectorSize - 1,
								This->sectorSize));
		fprintf(stderr, "This->dirty = %d\n", This->dirty);
		exit(1);
	}

	This->dirty = 1;
	return len;
}

static int buf_flush(Stream_t *Stream)
{
	int ret;
	DeclareThis(Buffer_t);

	if (!This->ever_dirty)
		return 0;
	ret = _buf_flush(This);
	if(ret == 0)
		This->ever_dirty = 0;
	return ret;
}


static int buf_free(Stream_t *Stream)
{
	DeclareThis(Buffer_t);

	if(This->buf)
		free(This->buf);
	This->buf = 0;
	return 0;
}

static Class_t BufferClass = {
	buf_read,
	buf_write,
	buf_flush,
	buf_free,
	0, /* set_geom */
	get_data_pass_through, /* get_data */
	0, /* pre-allocate */
};

Stream_t *buf_init(Stream_t *Next, int size, 
		   int cylinderSize, 
		   int sectorSize)
{
	Buffer_t *Buffer;
	Stream_t *Stream;


	if(size % cylinderSize != 0) {
		fprintf(stderr, "size not multiple of cylinder size\n");
		exit(1);
	}
	if(cylinderSize % sectorSize != 0) {
		fprintf(stderr, "cylinder size not multiple of sector size\n");
		exit(1);
	}

	if(Next->Buffer){
		Next->refs--;
		Next->Buffer->refs++;
		return Next->Buffer;
	}

	Stream = (Stream_t *) malloc (sizeof(Buffer_t));
	if(!Stream)
		return 0;
	Buffer = (Buffer_t *) Stream;
	Buffer->buf = malloc(size);
	if ( !Buffer->buf){
		Free(Stream);
		return 0;
	}
	Buffer->size = size;
	Buffer->dirty = 0;
	Buffer->cylinderSize = cylinderSize;
	Buffer->sectorSize = sectorSize;

	Buffer->ever_dirty = 0;
	Buffer->dirty_pos = 0;
	Buffer->dirty_end = 0;
	Buffer->current = 0;
	Buffer->cur_size = 0; /* buffer currently empty */

	Buffer->Next = Next;
	Buffer->Class = &BufferClass;
	Buffer->refs = 1;
	Buffer->Buffer = 0;
	Buffer->Next->Buffer = (Stream_t *) Buffer;
	return Stream;
}

