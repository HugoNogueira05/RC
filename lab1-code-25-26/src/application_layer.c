// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"
#include <stdio.h>
#include <string.h>

void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename)
{
    // TODO: Implement this function
    LinkLayer connectionParameters;
    connectionParameters.baudRate = baudRate;
    connectionParameters.nRetransmissions = nTries;
    connectionParameters.timeout = timeout;
    strncpy(connectionParameters.serialPort, serialPort, 50);
    if (strcmp(role, "tx") == 0)
        connectionParameters.role = LlTx;
    else if (strcmp(role, "rx") == 0)
        connectionParameters.role = LlRx;
    llopen(connectionParameters); // works
    if (strcmp(role,"tx") == 0){
        FILE* file = fopen(filename , "r");
        
        fread()
    }
    if (strcmp(role,"rx") == 0){
        FILE* file = fopen(filename , "w+");

    }
}
