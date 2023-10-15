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
    
    while(STOP == FALSE && (read(fd, buf, 1) > 0)){
        STOP = TRUE;
    }

    return 0;
}




////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters)
{

    // TODO
    // openChannel();
    if(connectionParameters.role == LlRx){
        int fd = read_noncanonical(connectionParameters);
        openChannel(fd);
    }
    return 1;
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
