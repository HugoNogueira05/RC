// Link layer protocol implementation

#include "link_layer.h"
#include "serial_port.h"
#include "helpers.h"

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source

////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters)
{
    if (connectionParameters.role == LlTx){
        // Transmitter
        if (openSerialPort(connectionParameters.serialPort, connectionParameters.baudRate) < 0)
        {
            perror("openSerialPort");
            exit(-1);
        }

        printf("Serial port %s opened\n", connectionParameters.serialPort);


        initiateSenderProtocol(connectionParameters.timeout , connectionParameters.nRetransmissions);
        printf("Connection established\n");
    } 
    else if (connectionParameters.role == LlRx){
        // Receiver

        volatile int STOP = FALSE;
        if (openSerialPort(connectionParameters.serialPort, connectionParameters.baudRate) < 0)
        {
            perror("openSerialPort");
            exit(-1);
        }

        printf("Serial port %s opened\n", connectionParameters.serialPort);


        while (STOP == FALSE)
        {
            unsigned char byte;
            readByteSerialPort(&byte);
            printf("Byte received: %x\n", byte);

        if (expectSupervisionFrame() == 0)
        {
            printf("Received flag char. Responding.\n");
            unsigned char response[5] = {FLAG, 0x03, 0x07, 0x04, FLAG};
            writeBytesSerialPort(response, 5);
            STOP = TRUE;
        }
    }
    } 

    return 0;
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize)
{
    // TODO: Implement this function

    return 0;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{
    // TODO: Implement this function

    return 0;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose()
{
    // TODO: Implement this function

    return 0;
}
