/*   
 *  playwave.c
 *
 *  Play sound files in wave format. Only MicroSoft PCM is supported. 
 *
 *  Michel R. Prevenier.
 */

#include <sys/types.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <minix/sound.h>

int main(int argc, char **argv);
void usage(void);

/******* Wave format definitions *********/

#define RIFF_ID		0x46464952
#define WAVE_ID1	0x45564157
#define WAVE_ID2	0x20746D66
#define DATA_ID		0x61746164
#define MS_PCM_FORMAT	0x0001
	
#define WORD	short  
#define DWORD   unsigned long

struct RIFF_fields
{
  DWORD RIFF_id;
  DWORD RIFF_len;
  DWORD WAVE_id1;
  DWORD WAVE_id2;
  DWORD data_ptr;
} r_fields; 

struct common_fields
{
  WORD  FormatTag;
  WORD  Channels;
  DWORD SamplesPerSec;
  DWORD AvgBytesPerSec;
  WORD  BlockAlign;
} c_fields;

struct specific_fields
{
  WORD BitsPerSample;
} s_fields;

DWORD data_id;
DWORD data_len;

/******** End of wave definitions *********/


void usage()
{
  fprintf(stderr, "Usage: playwav [-i] file\n");
  exit(-1);
}

int open_audio(unsigned int *fragment_size, unsigned int channels,
	unsigned int samples_per_sec, unsigned int bits)
{
  unsigned int sign;
  int audio;

  /* Open DSP */
  if ((audio = open("/dev/audio", O_RDWR)) < 0)
  {
    printf("Cannot open /dev/audio: %s\n", strerror(errno));
    exit(-1);
  }

  ioctl(audio, DSPIOMAX, fragment_size); /* Get maximum fragment size. */

  /* Set DSP parameters (should check return values..) */
  ioctl(audio, DSPIOSIZE, fragment_size);	/* Use max. fragment size. */
  ioctl(audio, DSPIOSTEREO, &channels);
  ioctl(audio, DSPIORATE, &samples_per_sec);
  ioctl(audio, DSPIOBITS, &bits);
  sign = (bits == 16 ? 1 : 0);
  ioctl(audio, DSPIOSIGN, &sign);
  return audio;
}

int main ( int argc, char *argv[] )
{
  int i, r, audio, file;
  char *buffer, *file_name = NULL;
  unsigned int fragment_size, fragment_size2;
  long data_pos;  
  int showinfo = 0;

  /* Check Parameters */
  if (argc > 2)
  {
    if (strncmp(argv[1], "-i", 2) == 0)
    {
      showinfo = 1;
      file_name = argv[2];
    }
    else
      usage();
  }
  else file_name = argv[1];

  /* Open wav file */
  if((file = open(file_name, O_RDONLY)) < 0)
  {
    printf("Cannot open %s\n", file_name);
    exit(-1);
  }

  /* Check for valid wave format */
  read(file, &r_fields, 20);
  if(r_fields.RIFF_id != RIFF_ID)
  {
      printf("%s not in RIFF format\n", file_name);
      exit(1);
  }
  if(r_fields.WAVE_id1 != WAVE_ID1 || r_fields.WAVE_id2 != WAVE_ID2)
  {
      printf("%s not in WAVE format\n", file_name);
      exit(1);
  }

  /* Store data_chunk position */
  data_pos = lseek(file, 0L, 1) + r_fields.data_ptr;

  /* Read the common and specific fields */
  read(file, &c_fields, 14);
  read(file, &s_fields, 2);

  /* Check for valid wave format, we can only play MicroSoft PCM */
  if(c_fields.FormatTag != MS_PCM_FORMAT)
  {
    printf("%s not in MicroSoft PCM format\n", file_name);
    exit(1);
  }

  /* Open audio device and set DSP parameters */
  audio = open_audio(&fragment_size, c_fields.Channels - 1,
	c_fields.SamplesPerSec, s_fields.BitsPerSample);

  if ((buffer = malloc(fragment_size)) == (char *)0)
  {
    fprintf(stderr, "Cannot allocate buffer\n");
    exit(-1);
  }

  /* Goto data chunk */
  lseek(file, data_pos, SEEK_SET);

  /* Check for valid data chunk */
  read(file, &data_id, sizeof(data_id));
  if(data_id != DATA_ID)
  {
    printf("Invalid data chunk\n");
    exit(1);
  }

  /* Get length of data */
  read(file, &data_len, sizeof(data_len));

  if (showinfo)
  {
    printf("\nBits per sample   : %d \n", s_fields.BitsPerSample);
    printf("Stereo            : %s \n", (c_fields.Channels == 1 ? "yes" : "no"));
    printf("Samples per second: %ld \n", c_fields.SamplesPerSec); 
    printf("Average bytes/sec : %ld \n", c_fields.AvgBytesPerSec);
    printf("Block alignment   : %d \n", c_fields.BlockAlign);
    printf("Datalength (bytes): %ld \n\n", data_len);
  }
    
  /* Play data */
  while(data_len > 0)
  {
    if (data_len > fragment_size) 
    {
      /* Read next fragment */
      read(file, buffer, fragment_size); 
      data_len-= fragment_size;
    }
    else 
    { 
      /* Read until end of file and fill rest of buffer with silence,
       * in PCM this means: fill buffer with last played value
       */
      read(file, buffer, data_len); 
      for (i = data_len; i< fragment_size; i++) 
        buffer[i] = buffer[(int)data_len-1];
      data_len = 0;
    }

    /* Copy data to DSP */
    r= write(audio, buffer,  fragment_size);
    if (r != fragment_size)
    {
	if (r < 0)
	{
		fprintf(stderr, "playwave: write to audio device failed: %s\n",
			strerror(errno));

		/* If we get EIO, the driver might have restarted. Reopen the
		 * audio device.
		 */
		if (errno == EIO) {
			close(audio);
			audio = open_audio(&fragment_size2,
				c_fields.Channels - 1, c_fields.SamplesPerSec,
				s_fields.BitsPerSample);
			if (fragment_size2 != fragment_size) {
			    fprintf(stderr, "Fragment size has changed\n");
			    exit(1);
			}
		}
	}
	else
	{
		fprintf(stderr, "playwave: partial write %d instead of %d\n",
			r, fragment_size);
	}
    }
  }
}
