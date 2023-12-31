// Write to serial port in non-canonical mode
//
// Modified by: Eduardo Nuno Almeida [enalmeida@fe.up.pt]

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

// Baudrate settings are defined in <asm/termbits.h>, which is
// included by <termios.h>
#define BAUDRATE B38400
#define _POSIX_SOURCE 1 // POSIX compliant source

#define FALSE 0
#define TRUE 1

#define BUF_SIZE 256

enum STATE {
    ST,
    FI_rcv,
    A_rcv,
    C_rcv,
    BCC_rcv
};

volatile int STOP = FALSE;

int alarmEnabled = FALSE;
int alarmCount= 0;
void alarmHandler(int signal){
    alarmEnabled = FALSE;
    alarmCount++;

    printf("Alarm #%d\n", alarmCount);
}

int main(int argc, char *argv[])
{
    // Program usage: Uses either COM1 or COM2
    const char *serialPortName = argv[1];

    if (argc < 2)
    {
        printf("Incorrect program usage\n"
               "Usage: %s <SerialPort>\n"
               "Example: %s /dev/ttyS1\n",
               argv[0],
               argv[0]);
        exit(1);
    }

    // Open serial port device for reading and writing, and not as controlling tty
    // because we don't want to get killed if linenoise sends CTRL-C.
    int fd = open(serialPortName, O_RDWR | O_NOCTTY);

    if (fd < 0)
    {
        perror(serialPortName);
        exit(-1);
    }

    struct termios oldtio;
    struct termios newtio;

    // Save current port settings
    if (tcgetattr(fd, &oldtio) == -1)
    {
        perror("tcgetattr");
        exit(-1);
    }

    // Clear struct for new port settings
    memset(&newtio, 0, sizeof(newtio));

    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    // Set input mode (non-canonical, no echo,...)
    newtio.c_lflag = 0;
    newtio.c_cc[VTIME] = 0; // Inter-character timer unused
    newtio.c_cc[VMIN] = 0;  // Blocking read until 5 chars received

    // VTIME e VMIN should be changed in order to protect with a
    // timeout the reception of the following character(s)

    // Now clean the line and activate the settings for the port
    // tcflush() discards data written to the object referred to
    // by fd but not transmitted, or data received but not read,
    // depending on the value of queue_selector:
    //   TCIFLUSH - flushes data received but not read.
    tcflush(fd, TCIOFLUSH);

    // Set new port settings
    if (tcsetattr(fd, TCSANOW, &newtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    printf("New termios structure set\n");

    // Create string to send
    unsigned char buf[BUF_SIZE] = {0};

    unsigned char flag = 0x7E;
    unsigned char A = 0x03;
    unsigned char C = 0X03;
    unsigned char BCC1 = A^C;

    buf[0] = flag;
    buf[1] = A;
    buf[2] = C;
    buf[3] = BCC1;
    buf[4] = flag;

    (void)signal(SIGALRM, alarmHandler);


    // In non-canonical mode, '\n' does not end the writing.
    // Test this condition by placing a '\n' in the middle of the buffer.
    // The whole buffer must be sent even with the '\n'.

    while(alarmCount < 4 && !STOP){
        if(alarmEnabled == FALSE){
            alarm(3);
            alarmEnabled = TRUE;
            int bytes = write(fd, buf, 5);
            printf("%d bytes written\n", bytes);
            printf("waiting to read...\n");
        }


        enum STATE state = ST;

        while (STOP == FALSE && (read(fd, buf, 1) > 0)){
        buf[1] = '\0'; // Set end of string to '\0', so we can printf

        switch(state){
            case ST:
                if(buf[0] == 0x7E) state = FI_rcv;
                break;
            case FI_rcv:
                if(buf[0] == 0x01) state = A_rcv;
                else if(buf[0] != 0x7E) state = ST;
                break;
            case A_rcv:
                if(buf[0] == 0x07) state = C_rcv;
                else if(buf[0] == 0x7E) state = FI_rcv;
                else state = ST;
                break;
            case C_rcv:
                if(buf[0] == (0x01 ^ 0x07)) state = BCC_rcv;
                else if(buf[0] == 0x7E) state = FI_rcv;
                else state = ST;
                break;
            case BCC_rcv:
                if(buf[0] == 0x7E) {
                    STOP = TRUE;
                    alarm(0);
                    alarmEnabled = FALSE;
                }
                else state = ST;
                break;
        }

		printf("var = 0x%02X\n", buf[0]);
    }       
    }

    printf("ending program\n");

    // Restore the old port settings
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd);

    return 0;
}
