// Link layer protocol implementation

#include "link_layer.h"
#include "serial_port.h"
#include "helpers.h"

volatile bool frameNumber = FALSE;
LinkLayer globalLinklayer;


// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source

////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters)
{
    globalLinklayer = connectionParameters;
    if (connectionParameters.role == LlTx){
        // Transmitter
        if (openSerialPort(connectionParameters.serialPort, connectionParameters.baudRate) < 0)
        {
            perror("openSerialPort");
            return -1;
        }

        printf("Serial port %s opened\n", connectionParameters.serialPort);


        if(initiateSenderProtocol(connectionParameters.timeout , connectionParameters.nRetransmissions) == 0){
            printf("Connection established\n");
        }
        else{
            printf("Connection failed\n");
        }
    } 
    else if (connectionParameters.role == LlRx){
        // Receiver

        volatile bool STOP = FALSE;
        if (openSerialPort(connectionParameters.serialPort, connectionParameters.baudRate) < 0)
        {
            perror("openSerialPort");
            return -1;
        }

        printf("Serial port %s opened\n", connectionParameters.serialPort);


        while (STOP == FALSE)
        {
            // unsigned char byte;
            // readByteSerialPort(&byte);
            // printf("Byte received: %x\n", byte);

        if (expectSupervisionFrame(connectionParameters.timeout , connectionParameters.nRetransmissions) == 0)
        {
            printf("Received flag char. Responding.\n");
            unsigned char response[5] = {FLAG, 0x03, 0x07, 0x04, FLAG};
            writeBytesSerialPort(response, 5);
            STOP = TRUE;
        }
    }
    } 
    printf("returning\n");
    return 0;
}

////////////////////////////////////////////////
// LLWRITE
// return number of bytes written or negative in case of error
// buf - data to be sent  
// bufSize - size of data to be sent
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize)
{
    unsigned char message[BUF_SIZE]; // we assume that BUF_SIZE is at least double the maxsize of bufSize in case of worst case bytestuffing
    unsigned int newSize = generateInformationFrame(buf, frameNumber, bufSize, message);
    frameNumber = !frameNumber;
    int sentBytes = writeBytesSerialPort(message, newSize);
    if (sentBytes < 0){
        return -1;
    }
    else{
        printf("wrote %d bytes to serial port\n", sentBytes);
    }
    if(waitWriteResponse(frameNumber) == 0){ // 0 if rej , 1 if rr 
        return llwrite(buf, bufSize); // This locks us in an infinite loop until we successfully send the message, maybe switch this to have maxtries?
    }
    return sentBytes;
}

////////////////////////////////////////////////
// LLREAD
// Packet : Array of bytes read
// return: array lenght or negative in case of error
////////////////////////////////////////////////
int llread(unsigned char *packet)
{   
    enum readState { START, FLAG_RCV, A_RCV, C_RCV , BCC1_OK, DATA_RCV, BCC2_OK, STOP_READ };
    enum readState readState = START;
    unsigned int packetIter = 0;
    unsigned char dataBuffer[MAX_PAYLOAD_SIZE];
    int dataBufferIter = 0; 
    unsigned char byte;
    while (readState != STOP_READ && packetIter < MAX_PAYLOAD_SIZE){
        readByteSerialPort(&byte);
        printf("got %02x\n" , byte);
        switch (readState){
            case START:
                if(byte == FLAG) readState = FLAG_RCV;
                break;
            case FLAG_RCV:
                printf("read flag\n");
                if(byte == SENDER_ADDRESS) readState = A_RCV;
                else if(byte != FLAG) readState = START;
                break;
            case A_RCV: // Como saber se o number frame é o correto?
                if (byte == INFO_FRAME_0 || byte == INFO_FRAME_1) readState = C_RCV;
                else if(byte == FLAG) readState = FLAG_RCV;
                else readState = START;
                break;
            case C_RCV:
                if(byte == (SENDER_ADDRESS ^INFO_FRAME_0) || byte == (SENDER_ADDRESS ^INFO_FRAME_1)) readState = BCC1_OK;
                else if(byte == FLAG) readState = FLAG_RCV;
                else readState = START;
                break;
            case BCC1_OK:
                if(byte == FLAG) readState = FLAG_RCV;
                else {
                    dataBuffer[dataBufferIter] = byte;
                    dataBufferIter++;
                    readState = DATA_RCV;
                }
                break;
            case DATA_RCV:
                if(byte == calculateBCC2(dataBuffer, dataBufferIter)) readState = BCC2_OK;
                else if (byte == FLAG) readState = FLAG_RCV;
                else {
                    dataBuffer[dataBufferIter] = byte;
                    dataBufferIter++;
                }
                break;
            case BCC2_OK:
                if(byte == FLAG) readState = STOP_READ;
                else readState = START;
                break;
            case STOP_READ:
                break;
        }
        packetIter++;
    }
    if (readState != STOP_READ){ //stopped because buffer ran out which means failed
        return -1; // send rej message here or in application layer?
    }
    unsigned char* finalMessage = malloc(dataBufferIter); 
    unsigned int finalMessageSize = bytedestuff(dataBuffer, dataBufferIter , finalMessage);
    memcpy(packet, finalMessage, finalMessageSize);
    return finalMessageSize;
}

////////////////////////////////////////////////
// LLCLOSE        // error handling
////////////////////////////////////////////////
int llclose()
{
    // TODO: Implement this function

    if (globalLinklayer.role == LlRx){
        if (expectDISC() < 0){
            perror("expectDISC");
        }
        if (expectUA(globalLinklayer.timeout) < 0){
            perror("expectUA");
        }

        if (closeSerialPort()<0){
            perror("closeSerialPort\n");
            exit(-1);
        }
    }

    if (globalLinklayer.role == LlTx){
        if (sendDisconnect(SENDER_ADDRESS)<0)
            perror("sendDisconnect");
        if (expectDISC()<0)
            perror("expectDISC");
        else {
            unsigned char ua[5] = {FLAG, SENDER_ADDRESS, CONTROL_UA, (unsigned char)(SENDER_ADDRESS^CONTROL_UA), FLAG};
            if (writeBytesSerialPort(ua, 5) < 0)
                perror("writeBytesSerialPort (UA)");
        }

        if (closeSerialPort() < 0){
            perror("closeSerialPort");
            exit(-1);
        }

        printf("Port %s closed\n", globalLinklayer.serialPort);
    }

    return 0;
}
