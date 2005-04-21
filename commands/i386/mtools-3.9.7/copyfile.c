#include "sysincludes.h"
#include "msdos.h"
#include "mtools.h"
#include "file.h"
#include "llong.h"

/*
 * Copy the data from source to target
 */

int copyfile(Stream_t *Source, Stream_t *Target)
{
	char buffer[8*16384];
	int pos;
	int ret, retw;
	size_t len;
	mt_size_t mt_len;

	if (!Source){
		fprintf(stderr,"Couldn't open source file\n");
		return -1;
	}

	if (!Target){
		fprintf(stderr,"Couldn't open target file\n");
		return -1;
	}

	pos = 0;
	GET_DATA(Source, 0, &mt_len, 0, 0);
	if (mt_len & ~max_off_t_31) {
		fprintf(stderr, "File too big\n");
		return -1;
	}
	len = truncBytes32(mt_len);
	while(1){
		ret = READS(Source, buffer, (mt_off_t) pos, 8*16384);
		if (ret < 0 ){
			perror("file read");
			return -1;
		}
		if(!ret)
			break;
		if(got_signal)
			return -1;
		if (ret == 0)
			break;
		if ((retw = force_write(Target, buffer, (mt_off_t) pos, ret)) != ret){
			if(retw < 0 )
				perror("write in copy");
			else
				fprintf(stderr,
					"Short write %d instead of %d\n", retw,
					ret);
			if(errno == ENOSPC)
				got_signal = 1;
			return ret;
		}
		pos += ret;
	}
	return pos;
}
