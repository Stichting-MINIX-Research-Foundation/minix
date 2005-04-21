#include "sysincludes.h"
#include "msdos.h"
#include "mtools.h"
#include "file.h"

/*
 * Read the clusters given the beginning FAT entry.  Returns 0 on success.
 */

int file_read(FILE *fp, Stream_t *Source, int textmode, int stripmode)
{
	char buffer[16384];
	int pos;
	int ret;

	if (!Source){
		fprintf(stderr,"Couldn't open source file\n");
		return -1;
	}
	
	pos = 0;
	while(1){
		ret = Source->Class->read(Source, buffer, (mt_off_t) pos, 16384);
		if (ret < 0 ){
			perror("file read");
			return -1;
		}
		if ( ret == 0)
			break;
		if(!fwrite(buffer, 1, ret, fp)){
			perror("write");
			return -1;
		}
		pos += ret;
	}
	return 0;
}
