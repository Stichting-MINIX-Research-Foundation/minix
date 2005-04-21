/*   
 *  mixer
 *
 *  Michel R. Prevenier.
 */

#include <sys/types.h>
#include <errno.h>
#include <curses.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <minix/sound.h>

#define CURS_CTRL	'\033'
#define ESCAPE		27
#define UP   		'A'
#define DOWN		'B'
#define LEFT		'D'
#define RIGHT		'C'
#define SPACE		' '


_PROTOTYPE ( int main, (int arg, char **argv));
_PROTOTYPE ( void usage, (void));
_PROTOTYPE ( void non_interactive, (void));
_PROTOTYPE ( void setup_screen, (void));
_PROTOTYPE ( int read_settings, (void));
_PROTOTYPE ( int write_settings, (void));
_PROTOTYPE ( void rdwr_levels, (int flag));
_PROTOTYPE ( void rdwr_inputs, (int flag));
_PROTOTYPE ( void rdwr_outputs, (int flag));
_PROTOTYPE ( void create_slider, (int x, int y, enum Device device));
_PROTOTYPE ( void show_inputs, (int x, int y));
_PROTOTYPE ( void show_outputs, (int x, int y));
_PROTOTYPE ( char *d_name, (enum Device device, char *name));
_PROTOTYPE ( void user_interface, (void));
_PROTOTYPE ( void terminate, (int s));

WINDOW *main_win;
int old_stdin;
int fd;
char name[9];
char *file_name;
struct volume_level levels[9];
struct inout_ctrl inputs_left[9];
struct inout_ctrl inputs_right[9];
struct inout_ctrl outputs[9];


void usage()
{
  fprintf(stderr, "Usage: mixer [-r]\n");
  exit(-1);
}


void terminate(s)
int s;
{
  /* Restore terminal parameters and exit */

  (void) fcntl(0,F_SETFL,old_stdin);
  move(23, 0);			
  refresh();			
  resetty();
  endwin();			
  exit(1);		
}


int write_settings()
{
  /* Write the current mixer settings to $HOME/.mixer */

  int fd;

  if ((fd = creat(file_name, 0x124)) > 0)
  {
    write(fd, levels, sizeof(levels));
    write(fd, inputs_left, sizeof(inputs_left));
    write(fd, inputs_right, sizeof(inputs_right));
    write(fd, outputs, sizeof(outputs));
    close(fd);
    return 1;
  }

  return 0;
}


int read_settings()
{
  /* Restore mixer settings saved in $HOME/.mixer */

  int fd;

  if ((fd = open(file_name, O_RDONLY)) > 0)
  {
    read(fd, levels, sizeof(levels));
    read(fd, inputs_left, sizeof(inputs_left));
    read(fd, inputs_right, sizeof(inputs_right));
    read(fd, outputs, sizeof(outputs));
    close(fd);
    rdwr_levels(1);
    rdwr_outputs(1);
    rdwr_inputs(1);
    return 1;
  }
  return 0;
}


void rdwr_levels(flag)
int flag;             /* 0 = read, 1 = write */
{
  /* Get or set mixer settings */

  int i;
  int cmd;
 
  cmd = (flag == 0 ? MIXIOGETVOLUME : MIXIOSETVOLUME);  
  
  for(i = Master; i <= Bass; i++)
    (void) (ioctl(fd, cmd, &levels[i])); 
}    


void rdwr_inputs(flag)
int flag;              /* 0 = read, 1 = write */
{
  /* Get or set input settings */

  int i;
  int cmd_left, cmd_right;
  
  cmd_left = (flag == 0 ? MIXIOGETINPUTLEFT : MIXIOSETINPUTLEFT);  
  cmd_right = (flag == 0 ? MIXIOGETINPUTRIGHT : MIXIOSETINPUTRIGHT);  

  for(i = Fm; i <= Mic; i++)
  {
    (void) (ioctl(fd, cmd_left, &inputs_left[i])); 
    (void) (ioctl(fd, cmd_right, &inputs_right[i])); 
  }
}


void rdwr_outputs(flag)
int flag;               /* 0 = read, 1 = write */
{
  /* Get or set output settings */

  int i;
  int cmd; 

  cmd = (flag == 0 ? MIXIOGETOUTPUT : MIXIOSETOUTPUT);  

  for(i = Cd; i <= Mic; i++)
    (void) (ioctl(fd, cmd, &outputs[i])); 
}


int main(argc, argv)
int argc;
char **argv;

{
  int i;
  char *home_ptr;
  int fd2;

  /* Open mixer */
  if ((fd = open("/dev/mixer",O_RDONLY)) < 0)
  {
    fprintf(stderr, "Cannot open /dev/mixer\n");
    exit(-1);
  }

  /* Get user's home directory and construct the $HOME/.mixer
   * file name 
   */
  home_ptr = getenv("HOME");
  file_name = malloc(strlen(home_ptr)+strlen("mixer.ini\0"));
  if (file_name == (char *)0) 
  {
    fprintf(stderr, "Not enough memory\n");
    exit(-1);
  }
  strncpy(file_name, home_ptr, strlen(home_ptr));
  strncpy(file_name+strlen(home_ptr), "/.mixer\0", 9);

  /* Fill in the device numbers */    
  for(i = Master; i <= Bass; i++) 
  {
    levels[i].device = i;
    inputs_left[i].device = i;
    inputs_right[i].device = i;
    outputs[i].device = i;
  }

  /* Get arguments */
  if (argc > 1)
  {
    if (strncmp(argv[1], "-r", 2) == 0)
    {
      if (read_settings())
      {
        printf("Mixer settings restored\n");
        exit(0);
      }
      else
      {
        fprintf(stderr, "Could not restore mixer settings\n");
        exit(-1);
      }
    } 
    else usage();
  }

  /* Initialize windows. */
  (void) initscr();
  signal(SIGINT, terminate);
  old_stdin = fcntl(0,F_GETFL);
  cbreak();
  noecho();
  main_win = newwin(23,80,0,0);
  scrollok(main_win, FALSE);

  /* Read all current mixer settings */
  rdwr_levels(0);
  rdwr_inputs(0);
  rdwr_outputs(0);

  /* Set up the user screen and handle user input */
  setup_screen();
  user_interface();
}


void user_interface()
{
  /* This is the user interface. */
 
  char c;
  int x,y;
  int right;
  int input_scr, input_pos;
  int output_scr, output_pos;
  int max_level;
  enum Device device;
  int fd2;
 
  device = Master;
  right = 0;
  input_scr = 0;
  output_scr = 0;
  input_pos = 0;
  output_pos = 0;

  while(1)
  {
    if (input_scr)
    {
      y = device + 9;
      x = 51 + input_pos + (device == Mic ? 2 : 0);
    }
    else if (output_scr)
    {
      y = device + 15;
      x = 53 + output_pos + (device == Mic ? 4 : 0);
    }
    else
    {
      y = (device != Speaker ? 2 : 1) + 
          (device - (device < Treble ? 0 : Treble)) * 3 + 
          (right == 0 ? 0 : 1);
      if (!right)
        x = 9 + levels[device].left / (device < Speaker ? 2 : 1 ) +
              (device > Speaker ? 39 : 0);
      else
        x = 9 + levels[device].right / (device < Speaker ? 2 : 1) +
              (device > Speaker ? 39 : 0);
    }
    
    wmove(main_win,y,x);
    wrefresh(main_win);
    c = wgetch(main_win);

    switch(c)
    {
      case CURS_CTRL:
      {
        (void) wgetch(main_win);
        c = wgetch(main_win);
        
        switch(c)
        { 
          case DOWN: 
          {
           if (output_scr) 
           {
             if (device < Mic)
             {
                device++;
                if (device == Mic) output_pos = 0;
             }
           }
           else if (right || input_scr)
           {
             if (!input_scr)
             {
               if (device < Bass) 
               {
                 device++;         
                 right = 0;
               }
               else
               {  
                 input_scr = 1;
                 input_pos = 0;
                 device = Fm;
               }
             }
             else
             {
               if (device < Mic) 
               {
                 device++;
                 if (device == Mic && input_pos > 8) input_pos = 8;
               }
               else 
               { 
                 device = Cd; 
                 output_scr = 1; 
                 input_scr = 0; 
                 output_pos = 0; 
               }
             }
           }
           else 
           {
             if (device != Mic && device != Speaker)  right = 1;
             else { device++; right = 0; }
           }
          };break;
          case UP: 
          {
           if (output_scr) 
           {
             if (device > Cd) device--;
             else 
             {
               device = Mic; 
               output_scr = 0;
               input_scr = 1;
             }
           }
           else if (!right || input_scr)
           {
             if (input_scr)
             {
               if (device > Fm) device--;
               else 
               {
                 input_scr = 0;
                 device = Bass;
                 right = 1;
               }
             }
             else 
             {
               if (device > Master) 
               {
                 device--;         
                 if (device != Mic && device != Speaker) right = 1;
               }
             }
           }
           else 
             right = 0;
          };break;
          case RIGHT: 
          {
            if (output_scr) 
            {
              if (output_pos < 8 && device != Mic) output_pos = 8;
            }
            else if (!input_scr)
            {
              if (device < Speaker) max_level = 31;
              else if (device > Speaker) max_level = 15;
              else max_level = 4;

              if (!right) 
              {
                if (levels[device].left < max_level) levels[device].left+=
                  (device < Speaker ? 2 : 1);
              }
              else
              {
                if (levels[device].right < max_level) levels[device].right+=
                  (device < Speaker ? 2 : 1);
              }
              ioctl(fd, MIXIOSETVOLUME, &levels[device]); 
              ioctl(fd, MIXIOGETVOLUME, &levels[device]); 
              create_slider(1 + (device < Treble ? 0 : 39), 
                            (device - (device < Treble ? 0 : Treble))*3 +
                            (device != Speaker ? 2 : 1), device);
            }
            else
            {
              if ((device != Mic && input_pos < 12) ||
                  (device == Mic && input_pos < 8))
                input_pos += (4 + (device == Mic ? 4 : 0));
            }
          };break;
          case LEFT: 
          {
            if (output_scr)
            {
              if (output_pos > 0) output_pos = 0;
            }
            else if (!input_scr)
            {
              if (!right)
              {
                if (levels[device].left > 0) levels[device].left-=
                  (device < Speaker ? 2 : 1);
              }
              else
              {
                if (levels[device].right > 0) levels[device].right-=
                  (device < Speaker ? 2 : 1);
              }
              ioctl(fd, MIXIOSETVOLUME, &levels[device]); 
              ioctl(fd, MIXIOGETVOLUME, &levels[device]); 
              create_slider(1 + (device < Treble ? 0 : 39), 
                            (device - (device < Treble ? 0 : Treble))*3 +
                            (device != Speaker ? 2 : 1), device);
            }
            else
            {
              if (input_pos > 0) 
                input_pos -= (4 + (device == Mic ? 4 : 0));
            } 
          };break;
        } 
      };break;
    case SPACE:
      {
        if (output_scr)
        {
          switch(output_pos)
          {
            case 0:
            case 4:
            {
              outputs[device].left = 
               (outputs[device].left == ON ? OFF : ON);
              ioctl(fd, MIXIOSETOUTPUT, &outputs[device]);
            };break;
            case 8:
            {
              outputs[device].right = 
               (outputs[device].right == ON ? OFF : ON);
              ioctl(fd, MIXIOSETOUTPUT, &outputs[device]);
            };break;
          }
          ioctl(fd, MIXIOGETOUTPUT, &outputs[device]);
          show_outputs(41,16);
        }
        else if (input_scr)
        { 
          switch(input_pos)
          {
            case 0:
            {
              inputs_left[device].left = 
               (inputs_left[device].left == ON ? OFF : ON);
              ioctl(fd, MIXIOSETINPUTLEFT, &inputs_left[device]);
            };break;
            case 4:
            {
              inputs_left[device].right = 
               (inputs_left[device].right == ON ? OFF : ON);
              ioctl(fd, MIXIOSETINPUTLEFT, &inputs_left[device]);
            };break;
            case 8:
            {
              inputs_right[device].left = 
               (inputs_right[device].left == ON ? OFF : ON);
              ioctl(fd, MIXIOSETINPUTRIGHT, &inputs_right[device]);
            };break;
            case 12:
            {
              inputs_right[device].right = 
               (inputs_right[device].right == ON ? OFF : ON);
              ioctl(fd, MIXIOSETINPUTRIGHT, &inputs_right[device]);
            };break;
          }
          ioctl(fd, MIXIOGETINPUTLEFT, &inputs_left[device]);
          ioctl(fd, MIXIOGETINPUTRIGHT, &inputs_right[device]);
          show_inputs(41,8);
        }
      };break;
      case 's':
      {
        if (write_settings())  
          mvwprintw(main_win,22,28, "mixer settings saved");
        else
          mvwprintw(main_win,22,28, "error: file not saved");
        wrefresh(main_win);
        sleep(1);
        mvwprintw(main_win,22,28, "                           ");
      };break;
      case 'r':
      {
        if (read_settings())
          mvwprintw(main_win,22,28, "mixer settings restored");
        else
          mvwprintw(main_win,22,28, "error: could not open");
        wrefresh(main_win);
        sleep(1);
        setup_screen();
      };break;
      case 'e': terminate(1);
    }
  } 
}


char *d_name(device, name)
enum Device device;
char *name;
{
  /* Convert the device number to a name */

  switch (device)
  {
    case Master:	strncpy(name, "Master  \0", 9);break;
    case Dac:		strncpy(name, "Dac     \0", 9);break;
    case Fm:		strncpy(name, "Fm      \0", 9);break;
    case Cd:		strncpy(name, "CD      \0", 9);break;
    case Line:		strncpy(name, "Line    \0", 9);break;
    case Mic:		strncpy(name, "Mic     \0", 9);break;
    case Speaker:	strncpy(name, "Speaker \0", 9);break;
    case Treble:	strncpy(name, "Treble  \0", 9);break;
    case Bass:		strncpy(name, "Bass    \0", 9);break;
  }
  return name;
}


void create_slider(x, y, device)
int x;
int y;
enum Device device;
{
  /* Create a slider on the screen */

  int left;
  int right;
  int i;

  mvwprintw(main_win,y,x, "%s", d_name(device, name));

  left = levels[device].left / (device < Speaker ? 2 : 1); 
  right = levels[device].right / (device < Speaker ? 2 : 1); 

  for (i = 0; i < 16; i++)
  {
    if (device != Speaker || i < 4)
      mvwprintw(main_win,y,x+i+8, (i == left ? "*" : "-"));
    if (device < Mic || device > Speaker) 
      mvwprintw(main_win,y+1,x+i+8, (i == right ? "*" : "-"));
  }

  if (device < Mic || device > Speaker)
  {
    mvwprintw(main_win,y,x+i+10, "left");
    mvwprintw(main_win,y+1,x+i+10, "right");
  }
  wrefresh(main_win);
} 
  
void show_inputs(x,y)
int x;
int y;
{
  /* Show the input settings */

  int i;

  mvwprintw(main_win,y,x,  "             Rec-In  ");
  mvwprintw(main_win,y+1,x,"          left    right");
  mvwprintw(main_win,y+2,x,"          l   r   l   r");
  for (i = Fm; i <= Line; i++)
  {
    mvwprintw(main_win,y+i+1,x,  "%s  %d   %d   %d   %d", 
      d_name(i, (char *)name),
      (inputs_left[i].left == ON ? 1 : 0),
      (inputs_left[i].right == ON ? 1 : 0),
      (inputs_right[i].left == ON ? 1 : 0),
      (inputs_right[i].right == ON ? 1 : 0));
  }
  mvwprintw(main_win,y+i+1,x,  "%s    %d       %d", 
    d_name(Mic, (char *)name),
    (inputs_left[Mic].left == ON ? 1 : 0),
    (inputs_right[Mic].left == ON ? 1 : 0));
  wrefresh(main_win);
}

void show_outputs(x,y)
int x;
int y;
{
  /* Show the output settings */

  int i;

  mvwprintw(main_win,y,x,      "            Mix-Out  ");
  mvwprintw(main_win,y+1,x,    "          left    right");
  for (i = Cd; i <= Line; i++)
  {
    mvwprintw(main_win,y+i-1,x,"%s    %d       %d", 
      d_name(i, (char *)name),
      (outputs[i].left == ON ? 1 : 0),
      (outputs[i].right == ON ? 1 : 0));
  }
  mvwprintw(main_win,y+i-1,x,"%s        %d", 
    d_name(Mic, (char *)name),
    (outputs[Mic].left == ON ? 1 : 0));

  wrefresh(main_win);
}


void setup_screen()
{
  int i;

  wclear(main_win);
  mvwprintw(main_win,0,23,"------- Mixer Controls -------");
  wrefresh(main_win);
 
  for(i = 0; i <= Speaker; i++)
   create_slider(1, i*3+(i <= Mic ? 2 : 1), i);

   create_slider(40, 2, Treble);
   create_slider(40, 5, Bass);
 
   show_inputs(41,8);
   show_outputs(41,16);
}
