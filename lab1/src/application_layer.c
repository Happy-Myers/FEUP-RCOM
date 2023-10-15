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
    strncpy(connectionParams.serialPort, serialPort, sizeof(connectionParams.serialPort) - 1);
    connectionParams.serialPort[sizeof(connectionParams.serialPort) - 1] = '\0'; // Ensure null-termination.

    connectionParams.baudRate = baudRate;
    connectionParams.nRetransmissions = nTries;
    connectionParams.timeout = timeout;

    if(strcmp(role, "tx") == 0) connectionParams.role = LlTx;
    else connectionParams.role = LlRx;

    return connectionParams;
}


void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename)
{
    LinkLayer connectionParams = buildConnectionParams(serialPort, role, baudRate,
                                                        nTries, timeout);

    // Rx faz OPEN
    // Tx faz OPEN e envia Trama de Supervisão (S) com SET
    // Rx responde à Trama de Supervisão (S) com UA
    if(llopen(connectionParams) < 0){
        printf("erro no llopen()\n");
        exit(-1);
    }
    // open file (?)
    // llopen
    // retorna (in)sucesso

    // llwrite (controlo)
    // while (llwrite [imagem])
    // llwrite (controlo)


    // llread (controlo)
    // while (llread[imagem])
    // llread(controlo)


    // llclose
}
