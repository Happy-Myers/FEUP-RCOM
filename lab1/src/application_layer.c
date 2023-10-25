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
#include <math.h>
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

unsigned char * constructControlPacket(int type, const char* filename, unsigned long V1, unsigned long *packetSize){
    int L1 = ceil(log2(V1)/8);
    int L2 = strlen(filename);

    *packetSize = 1 + 2 + L1 + 2 + L2;

    unsigned char* controlPacket = (unsigned char *)malloc(*packetSize);
    int index = 0;

    controlPacket[index++] = type;
    controlPacket[index++] = 0;
    controlPacket[index++] = L1;

    for(unsigned char i = 0; i < L1; i++){
        controlPacket[L1 - i + 2] = V1 & 0xFF;
        V1 >>= 8;
        index++;
    }

    controlPacket[index++] = 1;
    controlPacket[index++] = L2;
    memcpy(controlPacket + index, filename, L2);

    return controlPacket;
}


int trasmitterTasks(const char *filename){
    FILE* penguin = fopen(filename, "rb");
    if(penguin == NULL){
        printf("file not found\n");
        return -1;
    }

    //construct control packet

    int fPos = ftell(penguin);

    fseek(penguin, 0, SEEK_END);
    int fileSize = ftell(penguin) - fPos;
    fseek(penguin, fPos, SEEK_SET);
    unsigned long cPacketSize;

    unsigned char* cPacket = constructControlPacket(2, filename, fileSize, &cPacketSize);

    if(llwrite(cPacket, cPacketSize) == -1){
        printf("[ERROR - Couldnt Send Control Packet START] \n");
        return -1;
    }

    free(cPacket);

    unsigned char *fileContent = (unsigned char*) malloc(sizeof(unsigned char) * fileSize);
    fread(fileContent, sizeof(unsigned char), fileSize, penguin);

    int totalSent = 0;

    //send data packets

    while(totalSent < fileSize){
        int remainingBytes = fileSize - totalSent;
        int dataSize = remainingBytes> (MAX_PAYLOAD_SIZE-3) ? (MAX_PAYLOAD_SIZE-3) : remainingBytes;
        int L1 = dataSize & 0xFF;
        int L2 = (dataSize >> 8) & 0xFF;
        unsigned char *data = (unsigned char*) malloc(dataSize+3);
        data[0] = 1;
        data[1] = L2;
        data[2] = L1;
        memcpy(data+3, fileContent, dataSize);
        if(llwrite(data, dataSize+3) == -1){
            printf("[ERROR - Couldnt Send Data Packet]\n");
            return -1;
        }
        totalSent += dataSize;
        fileContent += dataSize;
        free(data);
    }

    unsigned char *cPacketEnd = constructControlPacket(3, filename, fileSize, &cPacketSize);
    if(llwrite(cPacketEnd, cPacketSize) == -1){
        printf("[ERROR - Couldnt Send Control Packet END] \n");
        return -1;
    }

    return 0;
}

int parseCPacket(unsigned char* packet, int size, unsigned long int *fileSize, unsigned char **name){
    unsigned char dataLengthB = 0, *fileSizeAux = NULL;

    for(int i = 1; i < size; i+= dataLengthB + 1){
        switch(packet[i]){

            case 0: // File Size
                dataLengthB = packet[++i];
                fileSizeAux = (unsigned char*)malloc(dataLengthB);
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

    int tries = 3;
    while(packetSize < 0 && tries > 0){
        packetSize = llread(packet);
        if(packet[0] != CTRL_START) packetSize = -1;
        else printf("  -Receiving Control Field [START]\n");
        tries --;
    }

    unsigned long int fileSize = 0, fileSizeEnd = 0;
    unsigned char *name = NULL, *nameEnd = NULL;
    if(parseCPacket(packet, packetSize, &fileSize, &name) < 0) return -1;
    stats.fileSize = fileSize;
    unsigned char *buf;
    FILE* newFile = fopen("penguin-received.gif", "ab+");

    while (packetSize > 0 && packet[0] != CTRL_END) {

        while ((packetSize = llread(packet)) < 0);
        if(packetSize == 0){
            packetSize = -1;
            break;
        }
        if(packet[0] == CTRL_DATA){
            printf("    -Receiving Data\n");
            packetSize = (packet[1] << 8) + packet[2];
            buf = (unsigned char*) malloc (packetSize);
            memcpy(buf, packet + 3, packetSize);
            fwrite(buf, sizeof(unsigned char), packetSize, newFile);
            free(buf);

        } else if(packet[0] == CTRL_END){
            printf("  -Receiving Control Field [END]\n");
            if(parseCPacket(packet, packetSize, &fileSizeEnd, &nameEnd) < 0) return -1;
            if(fileSize != fileSizeEnd)
                printf("[ERROR - START AND END CONTROL FRAMES DO NOT MATCH]\n");

        } else{
            printf("[ERROR - DATA PACKET DOESNT MATCH]\n");
            return -1;
        }
    }

    fclose(newFile);
    return packetSize;
}


void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename){
    LinkLayer connectionParams = buildConnectionParams(serialPort, role, baudRate, nTries, timeout);

    int fd;

    printf("\n---- OPEN PROTOCOL ----\n");
    if((fd = llopen(connectionParams)) < 0){
        printf("[ERROR - llopen()]\n");
        exit(-1);
    }

    else{
        if(connectionParams.role == LlTx){
            printf("\n---- WRITE PROTOCOL ----\n");
            if(trasmitterTasks(filename) < 0) printf("[ERROR WHILE WRITING - CLOSING]\n");
        }
        else{
            printf("\n---- READ PROTOCOL ----\n");
            if(receiverTasks() < 0) printf("[ERROR WHILE READING - CLOSING]\n");
        }
    }

    // llclose
    printf("\n---- CLOSE PROTOCOL ----\n");
    if(llclose(fd) < 0){
        printf("[ERROR - llclose()]\n");
        exit(-1);
    }

}
