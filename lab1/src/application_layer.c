// Application layer protocol implementation

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
#include "application_layer.h"


LinkLayer buildConnectionParams(const char *serialPort, const char *role, int baudRate,
                               int nTries, int timeout) {
    LinkLayer connectionParams;

    // Copy serialPort into the LinkLayer struct.
    strcpy(connectionParams.serialPort, serialPort);

    connectionParams.baudRate = baudRate;
    connectionParams.nRetransmissions = nTries;
    connectionParams.timeout = timeout;

    connectionParams.role = strcmp(role, "tx") ? LlRx : LlTx; 

    return connectionParams;
}


int trasmitterTasks(){
    return 0;
}

int parseCPacket(unsigned char* packet, int size, unsigned long int *fileSize, unsigned char **name){
    unsigned char dataLengthB = 0, fileSizeAux[MAX_PAYLOAD_SIZE];

    for(int i = 1; i < size; i+= dataLengthB + 1){
        switch(packet[i]){
            case 0: // File Size
                dataLengthB = packet[++i];
                memcpy(fileSizeAux, packet+i+1, dataLengthB);
                for(unsigned int j = 0; j < dataLengthB; j++)
                    *fileSize = (*fileSize << 8) + fileSizeAux[j];
                break;
            case 1: // File Name
                dataLengthB = packet[++i];
                *name = (unsigned char*) malloc (dataLengthB);
                memcpy(*name, packet+i+1, dataLengthB);
                break;
            default:
                return -1;
        }
    }
    return 0;
}

int receiverTasks(){
    unsigned char *packet = (unsigned char *) malloc (MAX_PAYLOAD_SIZE);
    int packetSize = -1;

    while((packetSize = llread(packet)) <= 0){
        if(packet[0] != CTRL_START) packetSize = 0;
        else printf("  -Receiving Control Field [START]\n");
    }

    unsigned long int fileSize = 0, fileSizeEnd = 0;
    unsigned char *name = (unsigned char*) malloc(MAX_PAYLOAD_SIZE);
    unsigned char *nameEnd = (unsigned char*) malloc(MAX_PAYLOAD_SIZE);
    if(parseCPacket(packet, packetSize, &fileSize, &name) < 0) return -1;

    unsigned char *buf;
    FILE* newFile = fopen((char *) name, "ab+");

    while (packetSize > 0 && packet[0] != CTRL_END) {    
        while ((packetSize = llread(packet)) <= 0);
        if(packet[0] == CTRL_DATA){
            printf("    -Receiving Data\n");
            packetSize = (packet[1] << 8) + packet[2];
            buf = (unsigned char*) malloc (packetSize);
            memcpy(buf, packet + 3, packetSize);
            fwrite(buf, sizeof(unsigned char), packetSize, newFile);
            free(buf);
        }
        else if(packet[0] == CTRL_END){
            printf("  -Receiving Control Field [END]\n");
            parseCPacket(packet, packetSize, &fileSizeEnd, &nameEnd);
            if(strcmp((char *)name, (char *)nameEnd) != 0 && fileSize != fileSizeEnd)
                printf("[ERROR - START AND END CONTROL FRAMES DO NOT MATCH]\n"); 
        }
        else{
            printf("[ERROR - DATA PACKET DOESNT MATCH]\n");
            packetSize = -1;
        }
    }
    free(name);
    free(nameEnd);

    fclose(newFile);
    return packetSize;
}



void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename){
    LinkLayer connectionParams = buildConnectionParams(serialPort, role, baudRate, nTries, timeout);
    int fd;

    printf("\n---- OPEN PROTOCOL ----\n");
    if((fd = llopen(connectionParams)) < 0) printf("erro no llopen()\n");
    
    else{
        if(connectionParams.role == LlTx){
            printf("\n---- WRITE PROTOCOL ----\n");
            if(trasmitterTasks() < 0) printf("[ERROR WHILE WRITING - CLOSING]");
        }
        else{
            printf("\n---- READ PROTOCOL ----\n");
            if(receiverTasks() < 0) printf("[ERROR WHILE READING - CLOSING]");
        }
    }

    // llclose
    printf("\n---- CLOSE PROTOCOL ----\n");
    if(llclose(fd) < 0){
        printf("erro no llclose()\n");
        exit(-1);
    }
}
