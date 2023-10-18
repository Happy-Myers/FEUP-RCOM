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

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source

volatile int STOP = FALSE;
int alarmTriggered = FALSE;
int alarmCount = 0;
int timeout = 0;
LinkLayer connParams;

unsigned char byte;

typedef enum {
    START,
    FLAG_RCV,
    A_RCV,
    C_RCV,
    BCC1_RCV,
    BCC2_RCV
} STATE;

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
    return fd;
}

int sendSFrame(int fd, unsigned char A, unsigned char C){
    unsigned char buffer[5] = {FLAG, A, C, A ^ C, FLAG};
    return write(fd, buffer, 5);
}

void readSFrame(STATE state, unsigned char A, unsigned char C){
    switch (state){
        case START:
            if(byte == FLAG) state = FLAG_RCV;
            break;
        case FLAG_RCV:
            if(byte == AT) state = A_RCV;
            else if (byte != FLAG) state = START;
            break;
        case A_RCV:
            if(byte == SET) state = C_RCV;
            else if(byte == FLAG) state = FLAG_RCV;
            else state = START;
            break;
        case C_RCV:
            if(byte == (AT ^ SET)) state = BCC1_RCV;
            else if (byte == FLAG) state = FLAG_RCV;
            else state = START;
            break;
        case BCC1_RCV:
            if(byte == FLAG) STOP = TRUE;
            else state = START;
        default:
            break;
    }
    printf("var = 0x%02X\n", byte);
}

void alarmHandler(int signal){
    alarmTriggered = TRUE;
    alarmCount++;
}

int testConnection_Rx(int fd, STATE state){
    
    while(STOP == FALSE){
        if (read(fd, &byte, 1) > 0){
            readSFrame(state, AT, SET);
        }
    }

    return sendSFrame(fd, AR, UA);
}

int testConnection_Tx(int fd, STATE state, int retransmissions, int timeout){
    (void) signal(SIGALRM, alarmHandler);
    while(retransmissions != 0 && STOP == FALSE){
        sendSFrame(fd, AT, SET);
        alarm(timeout);
        alarmTriggered = FALSE;

        while(alarmTriggered == FALSE && STOP == FALSE){
            if(read(fd, &byte, 1) > 0){
                readSFrame(state, AR, UA);
            }
        }
        retransmissions--;
    }
    return STOP == FALSE ? -1 : 0;
}


////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters)
{
    connParams = connectionParameters;
    STATE state = START;    
    int fd = set_fd(connParams);
    if(fd < 0) return -1;

    switch(connParams.role){
        case LlTx: //write
            if(testConnection_Tx(fd, state, connParams.nRetransmissions, connParams.timeout) == -1) return -1;
            break;
        case LlRx: //read
            testConnection_Rx(fd, state);
            break;
        default: 
            return -1;
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
    // STATE state = START;
    // int fd = showStatistics;
    // TODO
    switch(connParams.role){
        case LlTx: //write
            // sendSFrame(fd, AT, DISC);
            // readSFrame(state, AR, DISC);
            // sendSFrame(fd, AT, UA);
            break;
        case LlRx: //read
            // readSFrame(state, AT, DISC);
            // sendSFrame(fd, AR, DISC);
            // readSFrame(state, AT, UA);
            break;
        default: 
            return -1;
    }
    return 1;
}
