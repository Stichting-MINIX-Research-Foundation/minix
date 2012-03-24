/*   
 *  recwave.c
 *
 *  Record sound files in wave format. Only MicroSoft PCM is supported. 
 *
 *  Michel R. Prevenier.
 */

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <minix/sound.h>

int main(int argc, char **argv);
void usage(void);
void write_wave_header(void);
void terminate(int s);


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

/******** End of wave format definitions *********/

/* Default recording values */
unsigned int sign = 0; 
unsigned int bits = 8; 
unsigned int stereo = 0; 
unsigned int rate = 22050; 

int old_stdin;
struct termios old_tty, new_tty;
int audio, file;

void usage()
{
  fprintf(stderr, "Usage: recwav [-b -s -r] file_name\n");
  exit(-1);
}

void terminate(s)
int s;
{
  /* Restore terminal parameters */
  tcsetattr(0, TCSANOW, &old_tty);
  (void) fcntl(0,F_SETFL,old_stdin);
  close(audio);
  close(file);
  exit(0);		
}

void write_wave_header()
{
  /* RIFF fields */
  r_fields.RIFF_id = RIFF_ID;
  r_fields.WAVE_id1 = WAVE_ID1;
  r_fields.WAVE_id2 = WAVE_ID2;
  r_fields.data_ptr = 16;
  r_fields.RIFF_len = 20 + r_fields.data_ptr + data_len;

  /* MicroSoft PCM specific fields */
  s_fields.BitsPerSample = bits;

  /* Common fields */
  c_fields.FormatTag = MS_PCM_FORMAT;
  c_fields.Channels = stereo + 1;
  c_fields.SamplesPerSec = rate;
  c_fields.AvgBytesPerSec =  c_fields.Channels * rate * (bits / 8);
  c_fields.BlockAlign = c_fields.Channels * (bits / 8);

  /* Data chunk */
  data_id = DATA_ID;

  /* Write wave-file header */
  lseek(file, 0L, SEEK_SET);
  write(file, &r_fields, 20);
  write(file, &c_fields, 14);
  write(file, &s_fields, 2);
  write(file, &data_id, sizeof(data_id)); 
  write(file, &data_len, sizeof(data_len)); 
}


int main(argc, argv)
int argc;
char **argv;
{
  unsigned int fragment_size;
  char *buffer, *file_name;
  char c;
  int i;

  /* Read parameters */
  if (argc < 2) usage();

  i = 1;
  while (argv[i][0] == '-' && i < argc)
  {
    if (strncmp(argv[i], "-b", 2) == 0)
      bits = atoi(argv[i] + 2);
    else if (strncmp(argv[i], "-s", 2) == 0) 
      stereo = atoi(argv[i] + 2);
    else if (strncmp(argv[i], "-r", 2) == 0) 
      rate = (unsigned int) atol(argv[i] + 2);
    else usage();
    i++;
  }
  if (i == argc) usage();

  file_name = argv[i];

  /* Some sanity checks */
  if ((bits != 8 && bits != 16) || 
      (rate < 4000 || rate > 44100) ||
      (stereo != 0 && stereo != 1))
  {
    fprintf(stderr, "Invalid parameters\n");
    exit(-1);
  }

  /* Open DSP */
  if ((audio = open("/dev/rec", O_RDWR)) < 0) 
  {
    fprintf(stderr, "Cannot open /dev/rec\n");
    exit(-1);
  }

  /* Get maximum fragment size and try to allocate a buffer */
  ioctl(audio, DSPIOMAX, &fragment_size);
  if ((buffer = malloc(fragment_size)) == (char *) 0)
  {
    fprintf(stderr, "Cannot allocate buffer\n");
    exit(-1);
  } 

  /* Set sample parameters */
  ioctl(audio, DSPIOSIZE, &fragment_size); 
  ioctl(audio, DSPIOSTEREO, &stereo); 
  ioctl(audio, DSPIORATE, &rate);
  ioctl(audio, DSPIOBITS, &bits); 
  sign = (bits == 16 ? 1 : 0);
  ioctl(audio, DSPIOSIGN, &sign); 

  /* Create sample file */
  if ((file = creat(file_name, 511)) < 0) 
  {
    fprintf(stderr, "Cannot create %s\n", argv[1]);
    exit(-1);
  } 
  /* Skip wave header */
  lseek(file, (long)(sizeof(r_fields) + 
                     sizeof(c_fields) + 
                     sizeof(s_fields) + 
                     sizeof(data_id) + 
                     sizeof(data_len)), SEEK_SET); 

  printf("\nBits per sample   : %u\n", bits);
  printf("Stereo            : %s\n", (stereo == 1 ? "yes" : "no"));
  printf("Samples per second: %u\n", rate);

  /* Set terminal parameters and remember the old ones */
  tcgetattr(0, &old_tty);
  new_tty = old_tty;
  new_tty.c_lflag &= ~(ICANON|ECHO);
  old_stdin = fcntl(0, F_GETFL);

  /* Catch break signal to be able to restore terminal parameters in case
   * of a user interrupt
   */
  signal(SIGINT, terminate);

  /* Go to non-blocking mode */
  tcsetattr(0, TCSANOW, &new_tty);
  (void) fcntl(0, F_SETFL, old_stdin | O_NONBLOCK);

  printf("\nPress spacebar to start sampling...\n");
  while(!(read(0, &c, 1) == 1 && c == ' '));

  printf("Sampling, press spacebar to stop...\n");
  while(!(read(0, &c, 1) == 1 && c == ' '))
  {
    /* Read sample fragment and write to sample file */
    read(audio, buffer, fragment_size);
    write(file, buffer, fragment_size);
    data_len+= fragment_size;
  }
  printf("%ld bytes sampled. \n\n", data_len);
 
  /* Construct the wave header in front of the raw sample data */
  write_wave_header();
  
  /* Restore terminal parameters and exit */
  terminate(1);
}
