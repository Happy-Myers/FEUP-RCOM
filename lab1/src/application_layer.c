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


int receiverTasks(){
    return 0;
}



void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename)
{
    LinkLayer connectionParams = buildConnectionParams(serialPort, role, baudRate,
                                                        nTries, timeout);
    printf("\n---- OPEN PROTOCOL ----\n");
    int fd = llopen(connectionParams);


    if(fd < 0){
        printf("erro no llopen()\n");
        if(llclose(fd) < 0){
            printf("erro no llclose()\n");
        }
        exit(-1);
    }

    if(connectionParams.role == LlTx){
        printf("\n---- WRITE PROTOCOL ----\n");
        trasmitterTasks(); // write
    }
    else{
        printf("\n---- READ PROTOCOL ----\n");
        receiverTasks(); // read
    }

    // llclose
    printf("\n---- CLOSE PROTOCOL ----\n");
    if(llclose(fd) < 0){
        printf("erro no llclose()\n");
        exit(-1);
    }
}
