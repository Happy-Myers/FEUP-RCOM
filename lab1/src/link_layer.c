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
int fd;

unsigned char byte;

typedef enum {
    START,
    FLAG_RCV,
    A_RCV,
    C_RCV,
    BCC1_RCV,
    BCC2_RCV
} STATE;

void set_fd(LinkLayer conParam){

    // Open serial port device for reading and writing and not as controlling tty
    // because we don't want to get killed if linenoise sends CTRL-C.
    fd = open(conParam.serialPort, O_RDWR | O_NOCTTY);
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

int sendSFrame(unsigned char A, unsigned char C){
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

int testConnection_Rx(STATE state){
    
    while(STOP == FALSE){
        if (read(fd, &byte, 1) > 0){
            readSFrame(state, AT, SET);
        }
    }

    return sendSFrame(AR, UA);
}

int testConnection_Tx(STATE state, int retransmissions, int timeout){
    (void) signal(SIGALRM, alarmHandler);
    while(retransmissions != 0 && STOP == FALSE){
        sendSFrame(AT, SET);
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
    set_fd(connParams);
    if(fd < 0) return -1;

    switch(connParams.role){
        case LlTx: //write
            if(testConnection_Tx(state, connParams.nRetransmissions, connParams.timeout) == -1) return -1;
            break;
        case LlRx: //read
            testConnection_Rx(state);
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
    int frameSize = bufSize + 6; // buf contains data 
    unsigned char *frame = (unsigned char *) malloc(frameSize);

    frame[0] = FLAG;
    frame[1] = AT;
    frame[2] = CI_1;
    frame[3] = frame[1] ^ frame[2]; //BCC1

    memcpy(frame+4, buf, bufSize);

    unsigned char bcc2 = 0;

    for(int i = 0; i < bufSize; i++){
        bcc2 = bcc2 ^ buf[i];
    }

    int currIndex = 4;

    for(int i = 0; i < bufSize; i++){
        if(buf[i] == FLAG || buf[i] == ESC_B1)
            updateFrame(&frame, &frameSize, &currIndex, buf[i]);
        currIndex++;
    }

    frame[frameSize-2] = bcc2;
    frame[frameSize-1] = FLAG;

    // verify if transmission was successful

    return 0;
}

void updateFrame(unsigned char *frame, unsigned int *frameSize, int *currIndex, unsigned char flag){
    frame = (unsigned char *)realloc(frame, *++frameSize);
    for(int i = currIndex; i < *frameSize; i++){
        frame[i+1] = frame[i];
    }

    switch(flag){
        case FLAG:
            frame[*currIndex++] = ESC_B1;
            frame[*currIndex] = ESC_B2;
            break;
        case ESC_B1:
            frame[*++currIndex] = ESC_B3;
            break;
        default: 
            break;
    }
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
