// Application layer protocol implementation

#include "application_layer.h"

void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename)
{
    LinkLayer connectionParams = buildConnectionParams(serialPort, role, baudRate,
                                                        nTries, timeout);

    if(llopen(connectionParams) < 0){
        printf("erro no llopen()\n");
        exit();
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

LinkLayer buildConnectionParams(const char *serialPort, const char *role, int baudRate,
                               int nTries, int timeout) {
    LinkLayer connectionParams;

    // Copy serialPort into the LinkLayer struct.
    strncpy(connectionParams.serialPort, serialPort, sizeof(connectionParams.serialPort) - 1);
    connectionParams.serialPort[sizeof(connectionParams.serialPort) - 1] = '\0'; // Ensure null-termination.

    connectionParams.baudRate = baudRate;
    connectionParams.nRetransmissions = nTries;
    connectionParams.timeout = timeout;

    if(role == "tx"){
        connectionParams.role = LinkLayerRole.LlTx;
    }
    else{
        connectionParams.role = LinkLayerRole.LlRx;
    }

    return connectionParams;
}