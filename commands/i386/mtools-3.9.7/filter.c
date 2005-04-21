#include "sysincludes.h"
#include "msdos.h"
#include "mtools.h"

typedef struct Filter_t {
	Class_t *Class;
	int refs;
	Stream_t *Next;
	Stream_t *Buffer;

	int dospos;
	int unixpos;
	int mode;
	int rw;
	int lastchar;
} Filter_t;

#define F_READ 1
#define F_WRITE 2

/* read filter filters out messy dos' bizarre end of lines and final 0x1a's */

static int read_filter(Stream_t *Stream, char *buf, mt_off_t iwhere, size_t len)
{
	DeclareThis(Filter_t);
	int i,j,ret;

	off_t where = truncBytes32(iwhere);

	if ( where != This->unixpos ){
		fprintf(stderr,"Bad offset\n");
		exit(1);
	}
	if (This->rw == F_WRITE){
		fprintf(stderr,"Change of transfer direction!\n");
		exit(1);
	}
	This->rw = F_READ;
	
	ret = READS(This->Next, buf, (mt_off_t) This->dospos, len);
	if ( ret < 0 )
		return ret;

	j = 0;
	for (i=0; i< ret; i++){
		if ( buf[i] == '\r' )
			continue;
		if (buf[i] == 0x1a)
			break;
		This->lastchar = buf[j++] = buf[i];	
	}

	This->dospos += i;
	This->unixpos += j;
	return j;
}

static int write_filter(Stream_t *Stream, char *buf, mt_off_t iwhere, 
						size_t len)
{
	DeclareThis(Filter_t);
	int i,j,ret;
	char buffer[1025];

	off_t where = truncBytes32(iwhere);

	if(This->unixpos == -1)
		return -1;

	if (where != This->unixpos ){
		fprintf(stderr,"Bad offset\n");
		exit(1);
	}
	
	if (This->rw == F_READ){
		fprintf(stderr,"Change of transfer direction!\n");
		exit(1);
	}
	This->rw = F_WRITE;

	j=i=0;
	while(i < 1024 && j < len){
		if (buf[j] == '\n' ){
			buffer[i++] = '\r';
			buffer[i++] = '\n';
			j++;
			continue;
		}
		buffer[i++] = buf[j++];
	}
	This->unixpos += j;

	ret = force_write(This->Next, buffer, (mt_off_t) This->dospos, i);
	if(ret >0 )
		This->dospos += ret;
	if ( ret != i ){
		/* no space on target file ? */
		This->unixpos = -1;
		return -1;
	}
	return j;
}

static int free_filter(Stream_t *Stream)
{
	DeclareThis(Filter_t);       
	char buffer=0x1a;

	/* write end of file */
	if (This->rw == F_WRITE)
		return force_write(This->Next, &buffer, (mt_off_t) This->dospos, 1);
	else
		return 0;
}

static Class_t FilterClass = { 
	read_filter,
	write_filter,
	0, /* flush */
	free_filter,
	0, /* set geometry */
	get_data_pass_through,
	0
};

Stream_t *open_filter(Stream_t *Next)
{
	Filter_t *This;

	This = New(Filter_t);
	if (!This)
		return NULL;
	This->Class = &FilterClass;
	This->dospos = This->unixpos = This->rw = 0;
	This->Next = Next;
	This->refs = 1;
	This->Buffer = 0;

	return (Stream_t *) This;
}
