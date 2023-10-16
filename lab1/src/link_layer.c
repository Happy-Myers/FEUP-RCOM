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
        if((fd = read_noncanonical(connectionParameters)) > 0)
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
