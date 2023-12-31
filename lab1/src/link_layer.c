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

LinkLayer connParams;

struct termios oldtio;
int fd;
unsigned char byte;

volatile int STOP = FALSE;
int alarmTriggered = FALSE;

int frameNumTx = 0;
int frameNumRx = 1;

int totalPackets = 0;
int packetsReceived = 0;
int packetsRejected = 0;

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

unsigned char readCFrame(){
    STATE state = START;
    STOP = FALSE;
    unsigned char c = 0;
    while(STOP == FALSE && alarmTriggered == FALSE){
        if(read(fd, &byte, 1) > 0){
            switch(state){
                case START:
                    if(byte == FLAG) state = FLAG_RCV;
                    break;
                case FLAG_RCV:
                    if(byte == AR) state = A_RCV;
                    else if(byte != FLAG) state = START;
                    break;
                case A_RCV:
                    if(byte == RR0 || byte == RR1 || byte == REJ0 || byte == REJ1){
                        c = byte;
                        state = C_RCV;
                    }
                    else if(byte == FLAG) state = FLAG_RCV;
                    else state = START;
                    break;
                case C_RCV:
                    if(byte == (AR ^ c)) state = BCC1_RCV;
                    else if(byte == FLAG) state = FLAG_RCV;
                    else state = START;
                    break;
                case BCC1_RCV:
                    if(byte == FLAG) STOP = TRUE;
                    else state = START;
                    break;
                default:
                    break;
            }
        }
    }
    return c;
}

void alarmHandler(int signal){
    alarmTriggered = TRUE;
}

int testConnection_Tx(STATE *state, int retransmissions, int timeout){
    (void) signal(SIGALRM, alarmHandler);
    int retry = retransmissions;
    while(retry != 0 && STOP == FALSE){
        printf("   -Sending SET command\n");
        sendSFrame(AT, SET);
        alarm(timeout);
        alarmTriggered = FALSE;

        printf("   -Receiving UA command\n");
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
    printf("   -Receiving SET command\n");
    while(STOP == FALSE){
        if (read(fd, &byte, 1) > 0){
            readSFrame(&*state, AT, SET);
        }
    }

    printf("   -Sending UA command\n");
    return sendSFrame(AR, UA);
}

void closeConnection_Tx(STATE *state, int retransmissions, int timeout){
    (void) signal(SIGALRM, alarmHandler);
    int retry = retransmissions;

    while(retry != 0 && STOP == FALSE){
        printf("   -Sending DISC command\n");
        sendSFrame(AT, DISC);
        alarm(timeout);
        alarmTriggered = FALSE;

        printf("   -Receiving DISC command\n");
        while(alarmTriggered == FALSE && STOP == FALSE){
            if(read(fd, &byte, 1) > 0){
                readSFrame(&*state, AR, DISC);
            }
        }
        retry--;
    }

    printf("   -Sending UA command\n");
    sendSFrame(AT, UA);
}

void closeConnection_Rx(STATE *state, int retransmissions, int timeout){
    (void) signal(SIGALRM, alarmHandler);
    int retry = retransmissions;

    printf("   -Receiving DISC command\n");
    readSFrame(&*state, AT, DISC);

    while(retry != 0 && STOP == FALSE){
        printf("   -Sending DISC command\n");
        sendSFrame(AR, DISC);
        alarm(timeout);
        alarmTriggered = FALSE;

        printf("   -Receiving UA command\n");
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
int llopen(LinkLayer connectionParameters){
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
    (void) signal(SIGALRM, alarmHandler);
    unsigned char C = frameNumTx == 0 ? CI_0 : CI_1;
    unsigned char bcc1 = AT ^ C; //BCC1
    
    unsigned char bcc2 = 0;
    int newBuffSize = 0;

    for(int i = 0; i < bufSize; i++){
        bcc2 = bcc2 ^ buf[i];
        if(buf[i] == FLAG || buf[i] == ESC_B1)
            newBuffSize += 2;
        else 
            newBuffSize++;
    }
    if(bcc2 == FLAG || bcc2 == ESC_B1) newBuffSize++;
    unsigned int frameSize = newBuffSize + 6; // buf contains data 
    unsigned char *frame = (unsigned char *) malloc(frameSize);

    frame[0] = FLAG;
    frame[1] = AT;
    frame[2] = C;
    frame[3] = bcc1;

    int pos = 4;

    for(int i = 0; i < bufSize; i++){
        if(buf[i] == FLAG){
            frame[pos++] = ESC_B1;
            frame[pos++] = ESC_B2;
        }
        else if(buf[i] == ESC_B1){
            frame[pos++] = ESC_B1;
            frame[pos++] = ESC_B3;
        }
        else{
            frame[pos++] = buf[i];
        }
    }
 
    if(bcc2 == FLAG){
        frame[frameSize-3] = ESC_B1;
        frame[frameSize-2] = ESC_B2;
    }
    else if(bcc2 == ESC_B1){
        frame[frameSize-3] = ESC_B1;
        frame[frameSize-2] = ESC_B3;
    }
    else{
        frame[frameSize-2] = bcc2;
    }
    frame[frameSize-1] = FLAG;

    // verify if transmission was successful
    int retransmission = connParams.nRetransmissions;
    int accepted = FALSE;

    while(retransmission > 0 && !accepted){
        alarm(connParams.timeout);
        alarmTriggered = FALSE;

        while(alarmTriggered == FALSE && !accepted){
            printf("    -Sending Data [%d Bytes]\n", bufSize);
            write(fd, frame, frameSize);
            unsigned char response = readCFrame();

            if(response == RR0 || response == RR1){
                packetsReceived++;
                accepted = TRUE;
                frameNumTx = (frameNumTx + 1) % 2;
            }
            else packetsRejected++;
        }
        retransmission--;
    }

    totalPackets++;
    free(frame);
    if(accepted == TRUE)
        return 0;

    return -1;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet){
    STATE state = START;
    unsigned char c = 0;
    int index = 0;
    unsigned char aux = 0;  // neutral element of the XOR operation
    STOP = FALSE;

    while(STOP == FALSE){
        if (read(fd, &byte, 1) > 0){
            switch(state){
                case START:
                    if(byte == FLAG) state = FLAG_RCV;
                    break;
                case FLAG_RCV:
                    if(byte == AT) state = A_RCV;
                    else if(byte != FLAG) state = START;
                    break;
                case A_RCV:
                    if(byte == (frameNumTx == 0 ? CI_0 : CI_1)){
                        state = C_RCV;
                        c = byte;
                    }
                    else if(byte == FLAG) state = FLAG_RCV;
                    else if(byte == DISC) return 0;
                    else state = START;
                    break;
                case C_RCV:
                    if(byte == (AT ^ c)) state = READING;
                    else if (byte == FLAG) state = FLAG_RCV;
                    else state = START;
                    break;
                case READING:
                    if(byte == ESC_B1) state = BYTE_STUFF;
                    else if(byte == FLAG){
                        packet[--index] = 0;
                        if(aux == 0){
                            STOP = TRUE;
                            sendSFrame(AR, frameNumRx == 1 ? RR1 : RR0);
                            frameNumRx = frameNumRx == 0 ? 1 : 0;
                            frameNumTx = frameNumTx == 0 ? 1 : 0;
                            packetsReceived++;
                            totalPackets++;
                            return index;
                        }
                        else{
                            printf("[Error - Rejected Package]\n");
                            sendSFrame(AR, (frameNumRx == 0 ? REJ0 : REJ1));
                            packetsRejected++;
                            totalPackets++;
                            return -1;
                        }
                    }
                    else{
                        packet[index++] = byte;
                        aux ^= byte;
                    }
                    break;
                case BYTE_STUFF:
                    state = READING;
                    if(byte == ESC_B2){
                        packet[index++] = FLAG;
                    }
                    else if(byte == ESC_B3){
                        packet[index++] = ESC_B1;
                    }
                    else{
                        printf("[Error - Rejected Package - BYTE STUFF ERROR]\n");
                        sendSFrame(AR, (frameNumRx == 0 ? REJ0 : REJ1));
                        return -1;
                    }
                    aux ^= packet[index-1];
                    break;
                default:
                    break;
            }
        }
    }
    return -1;
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
    
    if(showStatistics){
        printf("\n---- STATISTICS ----\n");
        printf("Total Packets Sent/Received: %d\n", totalPackets);
        printf("Total Accepted Packets: %d/%d: %.2f%%\n", packetsReceived, totalPackets, (packetsReceived/totalPackets)*100.0);
        printf("Total Accepted Packets: %d/%d: %.2f%%\n", packetsRejected, totalPackets, (packetsRejected/totalPackets)*100.0);
    }

    close(fd);
    return 0;
}
