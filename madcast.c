#define INCL_WIN
#define INCL_GPI
#define INCL_DOS

#include <os2.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <time.h>

#include <netdb.h>              // NOTE: use headers from OS/2 Warp 4.0 MPTN
#include <netinet/in.h>         // if want to support TCP/IP stacks <4.1
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>

#define MADCAST_VERSION "MadCast 0.0.1"
#define MPG_MD_MONO 3
#define DEFAULT_BITRATE 128
#define BUFFERSIZE 4000

typedef struct mp3_headerSt
{
  int lay;
  int version;
  int error_protection;
  int bitrate_index;
  int sampling_frequency;
  int padding;
  int extension;
  int mode;
  int mode_ext;
  int copyright;
  int original;
  int emphasis;
  int stereo;
} mp3_header_t;

unsigned int bitrates[3][3][15] =
{
  {
    {0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448},
    {0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384},
    {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320}
  },
  {
    {0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256},
    {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160},
    {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160}
  },
  {
    {0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256},
    {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160},
    {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160}
  }
};

unsigned int s_freq[3][4] =
{
    {44100, 48000, 32000, 0},
    {22050, 24000, 16000, 0},
    {11025, 8000, 8000, 0}
};

unsigned int slotsf[3] = {384, 1152, 1152};

char *mode_names[5] = {"stereo", "j-stereo", "dual-ch", "single-ch",
                        "multi-ch"};
char *layer_names[3] = {"I", "II", "III"};
char *version_names[3] = {"MPEG-1", "MPEG-2 LSF", "MPEG-2.5"};
char *version_nums[3] = {"1", "2", "2.5"};

mp3_header_t mh;
int mp3_bitrate = 128;
int wait_msec = 250;

/* Return the number of slots for main data of current frame, */

int data_slots(mp3_header_t *pmh)
{
  int nSlots, sf = s_freq[pmh->version][pmh->sampling_frequency];

  if (sf == 0) return 1;

  nSlots = ((slotsf[pmh->lay - 1]/8) * bitrates[pmh->version][pmh->lay - 1][pmh->bitrate_index] * 1000)
          / sf;

  if (pmh->padding) nSlots++;

// Header
//  nSlots -= 4;

// CRC-16
//  if (pmh->error_protection) nSlots -= 2;

// Stereo ?
//  if (pmh->stereo == 1) nSlots -= 17;
//  else nSlots -= 32;

return nSlots; // slot = 8 bit
}

char *get_bitrate (unsigned char *block, int blocksize, int *datasize)
{
  unsigned char *buffer;
  unsigned int temp, slots;

  *datasize = -1; // next block by default

  if (blocksize < 0)
    {
      return NULL;
    }

  buffer = block - 1;

  do
    {
      buffer++;
      temp = ((buffer[0] << 4) & 0xFF0) | ((buffer[1] >> 4) & 0xE);
    } while ((temp != 0xFFE) && ((size_t)(buffer - block) < blocksize));

  if (temp != 0xFFE)
    {
      return NULL;

    }
  else
    {
      switch ((buffer[1] >> 3 & 0x3))
        {
        case 3:
          mh.version = 0;
          break;
        case 2:
          mh.version = 1;
          break;
        case 0:
          mh.version = 2;
          break;
        default:

          return NULL;
          break;
        }

      mh.lay = 4 - ((buffer[1] >> 1) & 0x3);
      if (mh.lay < 1) return NULL;
      mh.error_protection = !(buffer[1] & 0x1);
      mh.bitrate_index = (buffer[2] >> 4) & 0x0F;
      mh.sampling_frequency = (buffer[2] >> 2) & 0x3;
      mh.padding = (buffer[2] >> 1) & 0x01;
      mh.extension = buffer[2] & 0x01;
      mh.mode = (buffer[3] >> 6) & 0x3;
      mh.mode_ext = (buffer[3] >> 4) & 0x03;
      mh.copyright = (buffer[3] >> 3) & 0x01;
      mh.original = (buffer[3] >> 2) & 0x1;
      mh.emphasis = (buffer[3]) & 0x3;
      mh.stereo = (mh.mode == MPG_MD_MONO) ? 1 : 2;
      mp3_bitrate = bitrates[mh.version][mh.lay - 1][mh.bitrate_index];

      slots = data_slots(&mh);
      *datasize = slots;

      return buffer;

      //return bitrates[mh.version][mh.lay - 1][mh.bitrate_index];
    }

return NULL;
}

int server_socket = -1;

int connect_server (char *hostname, int port, char *pwd)
{
  char buf[250];
  struct hostent *hp;
  struct sockaddr_in name;
  int len, s, keep_alive;
  int snd_buf;

  if ((hp = gethostbyname (hostname)) == NULL)
    {
      printf ("unknown host: %s.\n", hostname);
      return -2;
    }

  printf ("Creating socket...\n");

  /* Create socket */
  if ((s = socket (AF_INET, SOCK_STREAM, 6)) < 0)
    {
      printf ("Could not create socket, exiting.\n");
      return -3;
    }

  /* Create server adress */
  memset (&name, 0, sizeof (struct sockaddr_in));
  name.sin_family = AF_INET;
  name.sin_port = ((port >> 8) & 255) | ((port & 255) << 8);
  memcpy (&name.sin_addr, hp->h_addr_list[0], hp->h_length);
  len = sizeof (struct sockaddr_in);

  printf ("Connecting to server %s on port %d by socket %d\n", hostname, port, s);

  /* Connect to the server */
  if (connect (s, (struct sockaddr *) &name, len) < 0)
    {
      printf ("Could not connect to %s.\n", hostname);
      return -4;
    }

  keep_alive = 1;
  setsockopt (s, SOL_SOCKET, SO_KEEPALIVE, (void*)&keep_alive, sizeof(keep_alive));
  snd_buf = 100000;
  setsockopt (s, SOL_SOCKET, SO_SNDBUF, (void*)&snd_buf, sizeof(snd_buf));

  /* Login on server, then play all arguments as files */
  printf ("Logging in...\n");

  sprintf (buf, "%s\n", pwd);
  send (s, buf, strlen (buf), 0);
  if (recv (s, buf, 100, 0) < 0)
    {
      printf ("Error in read, exiting\n");
      return -5;
    }

  if (buf[0] != 'O' && buf[0] != 'o')
    {
      printf ("Server says your password is invalid, damn.\n");
      return -6;
    }

  sprintf (buf, "icy-name:%s\n", "MadCast");
  send (s, buf, strlen (buf), 0);
  sprintf (buf, "icy-genre:%s\n", "MadCast");
  send (s, buf, strlen (buf), 0);
  sprintf (buf, "icy-url:%s\n", "http://localhost:8000");
  send (s, buf, strlen (buf), 0);
  sprintf (buf, "icy-pub:%d\n", 1);
  send (s, buf, strlen (buf), 0);
  sprintf (buf, "icy-br:%d\n\n", 128000);
  send (s, buf, strlen (buf), 0);

return s;
}

#define MAXLINES 4096

int playlist_size = 0;
char *playlist[MAXLINES];

int load_playlist (char *filename)
{
int i, j;
FILE *f;
char buf[250], *t;

f = fopen (filename, "rt");

if (!f) return -2;

while (fgets (buf, 250, f) == (char*)(&buf [0]))
  {
    t = (char*)strchr (buf, '\n'); if (t) *t = '\0';
    t = (char*)strchr (buf, '\r'); if (t) *t = '\0';

    playlist[playlist_size++] = (char*)strdup (buf);
    if (playlist_size >= MAXLINES) break;
  }

fclose (f);

return playlist_size;
}

int random_play = 0;
char playbuffer[BUFFERSIZE];

int play_song (int song_index)
{
FILE *f;
char rotater[] = "-/|\\-\\|/";
char *bigbuffer, *p, *t;
int size, i, pos, b, wait, frameindex, bitrate = DEFAULT_BITRATE;
static double dwait = 0.0;

if ((song_index < 0) || (song_index >= playlist_size)) return -1;

printf ("\r%-78s\r", " ");
printf ("%d - Playing file: %s\n", song_index + 1, playlist[song_index]);
fflush (stdout);
f = fopen (playlist[song_index], "rb");
if (!f)
  {
    printf ("Error opening!\n");
    fflush (stdout);
    return -2;
  }
fseek (f, 0L, SEEK_END);
size = ftell (f);
fseek (f, 0L, SEEK_SET);

// main loop

i = 0;
bigbuffer = malloc (size);
fread (bigbuffer, size, 1, f); // read mp3 in memory
fclose (f);
t = p = bigbuffer;
frameindex = 0;
//dwait = 0.0;

while (p && (p < (bigbuffer + size)))
  {
     p = get_bitrate (p, size - (p - bigbuffer), &i);
     if (i < 0) break;
     if (p)
       {
         if (mp3_bitrate == 0 )
           {
              printf ("bad mpeg header!\n");
              p ++; // try to get next
              continue;
           }

         dwait += ((double)i * 8000.0) / ((double)mp3_bitrate * 1000.0);
         p += i;
         frameindex ++;

         if (dwait >= (double) wait_msec)
           {
              char *mpegver [] = {"1.0","2.0","2.5","???"};
              unsigned int up1, up2; // uptimer probes from OS/2

              DosQuerySysInfo (14, 14, &up1, sizeof (up1));

              printf ("\r%-78s\r", " ");
              pos = (p - bigbuffer);
              printf ("MPEG %s Layer %d (%d kbit/s), Position %d (%d %%), Blocksize %d",
                      mpegver [mh.version], mh.lay, mp3_bitrate, pos, (pos * 100) / size, (p - t));

              if ((b = send (server_socket, t, (p - t), 0)) != (p - t))
                {
                  if (b < 0)
                  {
                     printf ("Server kicked us out, damn.\n");
                     server_socket = -1;
                     break;
                  }
                }

              DosQuerySysInfo (14, 14, &up2, sizeof (up2));

              t = p;
              DosSleep (wait_msec);
              dwait -= (double)wait_msec;
              if (up2 > up1) dwait -= (double)(up2 - up1); // can send () really affect this ?

              if (kbhit ())
                 if (toupper (getch ()) == 'N') break; // skip song
           }
       }
  }

free (bigbuffer);

// very bad old code
#if 0
for (pos = 0; pos < size; )
  {
    int b, x, blocksize = min (BUFFERSIZE, size - pos);
    ULONG probe1, probe2;
    int wait;

    DosQuerySysInfo (14, 14, &probe1, sizeof (ULONG));
    fread (playbuffer, blocksize, 1, f);
    x = ftell (f) - pos;
    b = get_bitrate (playbuffer, x - 4);
    if (b > 0) bitrate = b; // use default
    i ++;
    if (i >= strlen (rotater)) i = 0;

    b = (bitrate * 125); // hard - coded

    b -= x;

    if ((b + pos + x) > size) b = size - (pos + x);

    if (b > 0)
      {
         memcpy (bigbuffer, playbuffer, x);
         fread (&bigbuffer[x], b, 1, f);
      } else
      {
         memcpy (bigbuffer, playbuffer, x);
         b = 0;
      }

    x += b;
    pos += x;

    if ((b = send (server_socket, bigbuffer, x, 0)) != x)
      {
        if (b < 0)
          {
             printf ("Server kicked us out, damn.\n");
             server_socket = -1;
             break;
          }
      }

    DosQuerySysInfo (14, 14, &probe2, sizeof (ULONG));
    wait = 1000;

//    wait = x/((bitrate+7)/8);
    if (probe2 > probe1) wait -= (probe2 - probe1);
//    wait -= wait % 50; // wrap for OS/2 needs...
    printf ("%-78s\r", " ");
//    avg_wait += (double)wait;
//    frames += 1.0;
//    if ((int)(avg_wait/frames) < wait) wait = (int)(avg_wait/frames);
    printf ("[%c] %d bytes read, blocksize = %d [%d %% done] current bitrate is %d\r", rotater[i], pos, x, pos*100/size, bitrate, wait);

    DosSleep (wait);

    if (kbhit ())
       if (tolower (getch ()) == 's') break; // skip song
  }

free (bigbuffer);
fclose (f);
#endif

return 0;
}

int mad_shuffle (char *pl[], int n)
{
int i,j,o;

time((time_t*)&i);
srand(i);

for (o = 0; o < n*n; o ++)
  {
     i = rand () % n;
     j = rand () % n;

     if (i != j)
       {
          char *a = pl[i];
          pl[i] = pl[j];
          pl[j] = a;
       }
  }
  
//  for (j = 0; j < i; j ++)
//    if (rand()%3 || ((j > i/2) && (i < n/2)))
//      {
//        char *a = pl[i];
//        pl[i] = pl[j];
//        pl[j] = a;
//      }

return 0;
}

int main (int argc, char *argv[])
{
int i, arg, loop = 0, port = 8001;
char *p, hostname [200];

DosSetPriority(PRTYS_THREAD, 3, 31, 0); // set to high to avoid lags
setbuf (stdout, NULL); // our Watcom 'feature' to un-buffer all console output

puts(MADCAST_VERSION " an 'encoder' for IceCast internet mpeg streaming server");


if (argc < 4)
  {
     printf ("usage: %s <host[:port]> <password> <playlist> [-r] [-l] [-b msec]\n", argv[0]);
     puts ("host[:port] = icecast host and port to connect\n" \
           "password = icecast server password\n" \
           "playlist = playlist file\n" \
           "-r = shuffle playlist\n" \
           "-l = loop playlist\n" \
           "-b msec = buffer size in milliseconds (250 by default)");

     return -1;
  }

strcpy (hostname, argv[1]);
p = strchr (hostname, ':');
if (p)
  {
     *p = '\0'; p ++;
     sscanf (p, "%d", &port);
  }
i = load_playlist (argv[3]);

if (i > 0)
  {
    printf ("Playlist %s loaded, %d tracks\n", argv[3], i);

    if (argc > 3)
      for (arg = 3; arg < argc; arg ++)
        {
           if (argv[arg][0] != '-') continue;

           if (toupper (argv[arg][1]) == 'R')
             {
                mad_shuffle (playlist, i);
                mad_shuffle (playlist, i/2);
                mad_shuffle (&playlist[i/2], i/2);
                mad_shuffle (playlist, i);
             }

           if (toupper (argv[arg][1]) == 'L')
             loop = 1;

           if (toupper (argv[arg][1]) == 'B')
             {
                sscanf (argv[arg+1], "%d", &wait_msec);

                if (wait_msec < 100) wait_msec = 100; // wrap to lowest value
                if (wait_msec > 10000) wait_msec = 10000; // 10 sec is enough :)
             }

         }

    server_socket = connect_server (hostname, port, argv[2]);

    if (server_socket < 0) return -10;

    while (loop)
      {
         for (i = 0; i < playlist_size; i ++)
           {
              play_song (i);

              if (server_socket < 0)
                 server_socket = connect_server (hostname, port, argv[2]);
           }

         if (server_socket < 0) return -11; // kicked ;-(
      }

    soclose (server_socket);
  }

return 0;
}