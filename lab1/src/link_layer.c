// Link layer protocol implementation

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#include "link_layer.h"
#include "utils.h"
#include "read_noncanonical.h"

enum STATE {
    START,
    FLAG_RCV,
    A_RCV,
    C_RCV,
    BCC1_RCV,
    BCC2_RCV
};

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source

int set_fd(LinkLayer conParam){

    // Open serial port device for reading and writing and not as controlling tty
    // because we don't want to get killed if linenoise sends CTRL-C.
    int fd = open(conParam.serialPort, O_RDWR | O_NOCTTY);
    if (fd < 0)
    {
        perror(conParam.serialPort);
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

    newtio.c_cflag = conParam.baudRate | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    // Set input mode (non-canonical, no echo,...)
    newtio.c_lflag = 0;
    newtio.c_cc[VTIME] = 0; // Inter-character timer unused
    newtio.c_cc[VMIN] = 5;  // Blocking read until 5 chars received

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
    return fd;
}


int openChannel(int fd){
    
    unsigned char buf[BUF_SIZE + 1] = {0}; // +1: Save space for the final '\0' char
    volatile int STOP = FALSE;
    enum STATE state = START;
    
    while(STOP == FALSE && (read(fd, buf, 1) > 0)){
        buf[1] = '\0';
        switch (state){
        case START:
            if(buf[0] == FLAG) state = FLAG_RCV;
            break;
        case FLAG_RCV:
            if(buf[0] == AT) state = A_RCV;
            else if (buf[0] != FLAG) state = START;
            break;
        case A_RCV:
            if(buf[0] == SET) state = C_RCV;
            else state = START;
            break;
        case C_RCV:
            if(buf[0] == (AT ^ SET)) state = BCC1_RCV;
            else state = START;
            break;
        case BCC1_RCV:
            if(buf[0] == FLAG) STOP = TRUE;
            else state = START;
        default:
            break;
        }
    }

    memset(buf, 0, BUF_SIZE);
    buf[0] = FLAG;
	buf[1] = AR;
	buf[2] = UA;
	buf[3] = AR ^ UA;
	buf[4] = FLAG;

    if(write(fd, buf, 5) > 0) return 0;
    else return -1;
}


////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters)
{
    int fd = -1;

    if(connectionParameters.role == LlRx){
        if((fd = set_fd(connectionParameters)) > 0)
            if(openChannel(fd) < 0) return -1;
    }
    else{
        // write TODO
    }
    return fd;
}


////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize)
{
    // TODO

    return 0;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{
    // TODO

    return 0;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics)
{
    // TODO

    return 1;
}
