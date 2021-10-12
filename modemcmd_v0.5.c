#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <stdbool.h>
#include <errno.h>
#include <getopt.h>
#include <time.h>

#define DEFAULT_DEV         "/dev/ttyUSB2"
#define DEFAULT_BAUD        115200
#define DEFAULT_TIMEOUT     10.00
#define DEFAULT_CMD         "AT"
#define SILENT_MODE         false
#define RO_MODE         false
#define CR "\r"
#define NL "\n"

FILE *modem;
char  buf[1024];
char *dev               = DEFAULT_DEV;
double timeout          = DEFAULT_TIMEOUT;
char * cmd              = DEFAULT_CMD;
bool silent_mode        = SILENT_MODE;
bool ro_mode            = RO_MODE;
unsigned int baud            = DEFAULT_BAUD;

static void help_message()
{
    fprintf(stdout,"+-----------------------------------------------------------------------------+\n");
    fprintf(stdout,"|                          ModemCMD : Release v0.5                            |\n");
    fprintf(stdout,"+-----------------------------------------------------------------------------+\n\n");
    fprintf(stdout,"Usage: modemcmd -[hdbctrs] \n\n");
    fprintf(stdout,"    -h --help     Shows this message\n");
    fprintf(stdout,"    -d --device   Specifies modem device [ Default: %s ]\n",        DEFAULT_DEV);
    fprintf(stdout,"    -b --baud     Specifies baud rate    [ Default: %u ]\n",        DEFAULT_BAUD);
    fprintf(stdout,"    -c --command  Specifies AT command   [ Default: %s ]\n",        DEFAULT_CMD);
    fprintf(stdout,"    -t --timeout  Sets response timeout  [ Default: %.2fs ]\n",     DEFAULT_TIMEOUT);
    fprintf(stdout,"    -r --read     Reads the serial without sending any command\n" );
    fprintf(stdout,"    -s --silent   Disables any output including errors\n\n");
    fprintf(stdout,"+-----------------------------------------------------------------------------+\n\n");
    exit (EXIT_SUCCESS);
}

static void parse_options(int argc, char *argv[])
{
  int c;
  while (1)
    {
      static struct option long_options[] =
        {
          {"help",      no_argument,       0, 'h'},
          {"device",    required_argument, 0, 'd'},
          {"baud",      required_argument, 0, 'b'},
          {"command",   required_argument, 0, 'c'},
          {"timeout",   required_argument, 0, 't'},
          {"read",      no_argument,       0, 'r'},
          {"silent",    no_argument,       0, 's'},
          {0, 0, 0, 0}
        };

      int option_index = 0;

      c = getopt_long (argc, argv, "hsr:d:b:c:t",
                       long_options, &option_index);

      if (c == -1)
        break;

      switch (c)
        {
        case 'h':
          help_message();
          break;

        case 'd':
          dev = optarg;
          break;

        case 'b':
          baud = atoi(optarg);
          break;

        case 'c':
          cmd = optarg;
          break;

        case 't':
          timeout = atof(optarg);
          break;

        case 's':
          silent_mode = true;
          break;

        case 'r':
          ro_mode = true;
          break;

        default:
          exit (EXIT_FAILURE);
        }
    }
}

int main(int argc,char** argv)
{
        parse_options(argc,argv);

        struct termios tio;
        struct termios stdio;
        struct termios old_stdio;
        int modem;

        unsigned char c='D';
        tcgetattr(STDOUT_FILENO,&old_stdio);

        memset(&stdio,0,sizeof(stdio));
        stdio.c_iflag=0;
        stdio.c_oflag=0;
        stdio.c_cflag=0;
        stdio.c_lflag=0;
        stdio.c_cc[VMIN]=1;
        stdio.c_cc[VTIME]=0;
        tcsetattr(STDOUT_FILENO,TCSANOW,&stdio);
        tcsetattr(STDOUT_FILENO,TCSAFLUSH,&stdio);
        fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);       // make the reads non-blocking

        memset(&tio,0,sizeof(tio));
        tio.c_iflag=0;
        tio.c_oflag=0;
        tio.c_cflag=CS8|CREAD|CLOCAL;           // 8n1, see termios.h for more information
        tio.c_lflag=0;
        tio.c_cc[VMIN]=1;
        tio.c_cc[VTIME]=5;

        modem=open(dev, O_RDWR | O_NONBLOCK);
        if (modem < 0)
        {
                if (!silent_mode) { fprintf(stderr, "\nUnable to open device %s: %s\r\n\n",dev, strerror(errno));}
                tcsetattr(STDOUT_FILENO,TCSANOW,&old_stdio);
                exit (EXIT_FAILURE);
        }
        cfsetospeed(&tio,baud);            // 115200 baud
        cfsetispeed(&tio,baud);            // 115200 baud

        tcsetattr(modem,TCSANOW,&tio);

        tcflush(modem, TCIOFLUSH);

        if (!ro_mode){
                snprintf ( buf, sizeof(buf), "%s", cmd);
                write(modem,buf,sizeof(buf));
                write(modem,CR,1);
        }
        usleep(250000);

        int rsp = 0;
        clock_t start = time(0);
        do {
                if ((double)(time(0) - start) >= timeout){
                        if (!silent_mode) fprintf(stderr, "\nNO response from modem. Timeout reached.\r\n\n");
                        break;
                } else {
                        if (read(modem,&c,1)>0) {
                                if (!silent_mode) write(STDOUT_FILENO,&c,1);
                                rsp++;
                        } else {
                                if (rsp == 0){
                                        usleep(10000);
                                } else {
                                        break;
                                }

                        }
                }
        } while (true);

        usleep(250000);
        tcflush(modem, TCIOFLUSH);
        close(modem);
        tcsetattr(STDOUT_FILENO,TCSANOW,&old_stdio);
        return EXIT_SUCCESS;
}
