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

struct termios oldtio;
LinkLayer connParams;
int fd;

volatile int STOP = FALSE;
int alarmTriggered = FALSE;
int alarmCount = 0;
int timeout = 0;

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
    fd = open(conParam.serialPort, O_RDWR | O_NOCTTY);
    if (fd < 0){
        perror(conParam.serialPort);
        return -1;
    }

    struct termios newtio;

    // Save current port settings
    if (tcgetattr(fd, &oldtio) == -1){
        perror("tcgetattr");
        return -1;
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
    if (tcsetattr(fd, TCSANOW, &newtio) == -1){
        perror("tcsetattr");
        return -1;
    }

    printf("New termios structure set\n");
    return fd;
}

int sendSFrame(unsigned char A, unsigned char C){
    unsigned char buffer[5] = {FLAG, A, C, A ^ C, FLAG};
    return write(fd, buffer, 5);
}

void readSFrame(STATE *state, unsigned char A, unsigned char C){
    switch (*state){
        case START:
            if(byte == FLAG) *state = FLAG_RCV;
            break;
        case FLAG_RCV:
            if(byte == A) *state = A_RCV;
            else if (byte != FLAG) *state = START;
            break;
        case A_RCV:
            if(byte == C) *state = C_RCV;
            else if(byte == FLAG) *state = FLAG_RCV;
            else state = START;
            break;
        case C_RCV:
            if(byte == (A ^ C)) *state = BCC1_RCV;
            else if (byte == FLAG) *state = FLAG_RCV;
            else state = START;
            break;
        case BCC1_RCV:
            if(byte == FLAG) STOP = TRUE;
            else state = START;
        default:
            break;
    }
}

void alarmHandler(int signal){
    alarmTriggered = TRUE;
    alarmCount++;
}

int testConnection_Tx(STATE *state, int retransmissions, int timeout){
    (void) signal(SIGALRM, alarmHandler);
    int retry = retransmissions;
    while(retry != 0 && STOP == FALSE){
        printf("Sending SET command\n");
        sendSFrame(AT, SET);
        alarm(timeout);
        alarmTriggered = FALSE;

        printf("Receiving UA command\n");
        while(alarmTriggered == FALSE && STOP == FALSE){
            if(read(fd, &byte, 1) > 0){
                readSFrame(&*state, AR, UA);
            }
        }
        retry--;
    }
    return STOP == FALSE ? -1 : 0;
}

int testConnection_Rx(STATE *state){
    printf("Receiving SET command\n");
    while(STOP == FALSE){
        if (read(fd, &byte, 1) > 0){
            readSFrame(&*state, AT, SET);
        }
    }

    printf("Sending UA command\n");
    return sendSFrame(AR, UA);
}

void updateFrame(unsigned char *frame, unsigned int *frameSize, int *currIndex, unsigned char flag){
    frame = (unsigned char *)realloc(frame, *++frameSize);
    for(int i = *currIndex; i < *frameSize; i++){
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

void closeConnection_Tx(STATE *state, int retransmissions, int timeout){
    (void) signal(SIGALRM, alarmHandler);
    int retry = retransmissions;

    while(retry != 0 && STOP == FALSE){
        printf("Sending DISC command\n");
        sendSFrame(AT, DISC);
        alarm(timeout);
        alarmTriggered = FALSE;

        printf("Receiving DISC command\n");
        while(alarmTriggered == FALSE && STOP == FALSE){
            if(read(fd, &byte, 1) > 0){
                readSFrame(&*state, AR, DISC);
            }
        }
        retry--;
    }

    printf("Sending UA command\n");
    sendSFrame(AT, UA);
}

void closeConnection_Rx(STATE *state, int retransmissions, int timeout){
    (void) signal(SIGALRM, alarmHandler);
    int retry = retransmissions;

    printf("Receiving DISC command\n");
    readSFrame(&*state, AT, DISC);

    while(retry != 0 && STOP == FALSE){
        printf("Sending DISC command\n");
        sendSFrame(AR, DISC);
        alarm(timeout);
        alarmTriggered = FALSE;

        printf("Receiving UA command\n");
        while(alarmTriggered == FALSE && STOP == FALSE){
            if(read(fd, &byte, 1) > 0){
                readSFrame(&*state, AT, UA);
            }
        }
        retry--;
    }
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

    STOP = FALSE;
    if(connParams.role == LlTx){
        if(testConnection_Tx(&state, connParams.nRetransmissions, connParams.timeout) < 0)
            return -1;
    } else{
        if(testConnection_Rx(&state) < 0)
            return -1;
    }

    return fd;
}


////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize){
    unsigned int frameSize = bufSize + 6; // buf contains data 
    unsigned char *frame = (unsigned char *) malloc(frameSize);

    frame[0] = FLAG;
    frame[1] = AT;
    frame[2] = CI_0;
    frame[3] = frame[1] ^ frame[2]; //BCC1

    memcpy(frame+4, buf, bufSize);

    unsigned char bcc2 = 0;

    for(int i = 0; i < bufSize; i++){
        bcc2 = bcc2 ^ buf[i];
    }

    int currIndex = 4;

    for(int i = 0; i < bufSize; i++){
        if(buf[i] == FLAG || buf[i] == ESC_B1)
            updateFrame(frame, &frameSize, &currIndex, buf[i]);
        currIndex++;
    }

    frame[frameSize-2] = bcc2;
    frame[frameSize-1] = FLAG;

    // verify if transmission was successful

    return 0;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet){
    // TODO

    return 0;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics){
    STATE state = START;

    STOP = FALSE;
    if(connParams.role == LlTx)
        closeConnection_Tx(&state, connParams.nRetransmissions, connParams.timeout);
    else
        closeConnection_Rx(&state, connParams.nRetransmissions, connParams.timeout);

    // Restore the old port settings
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1){
        perror("tcsetattr");
        return -1;
    }

    close(fd);
    return 0;
}
