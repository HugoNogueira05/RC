// Link layer protocol implementation

#include "link_layer.h"
#include "serial_port.h"
#include "helpers.h"

volatile bool frameNumber = FALSE;
int maxTries ;
int timeout;
int counter =1;
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
            return -1;
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
    maxTries = connectionParameters.nRetransmissions;
    timeout = connectionParameters.timeout;
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
    unsigned char message[1000]; // we assume that BUF_SIZE is at least double the maxsize of bufSize in case of worst case bytestuffing
    unsigned int newSize = generateInformationFrame(buf, frameNumber, bufSize, message);
    frameNumber = !frameNumber;
    int sentBytes = writeBytesSerialPort(message, newSize);
    if (sentBytes < 0){
        return -1;
    }
    else{
        printf("wrote %d bytes to serial port\n", sentBytes);
    }
    if(waitWriteResponse(!frameNumber) == 0){ // 0 if rej , 1 if rr 
        if(counter > maxTries){
            printf("Exceeded maximum number of retransmissions\n");
            return -1;
        }
        counter++;

        return llwrite(buf, bufSize);
    }
    counter = 1;
    printf("Got response\n");
    return sentBytes;
}

////////////////////////////////////////////////
// LLREAD
// Packet : Array of bytes read
// return: array lenght or negative in case of error
////////////////////////////////////////////////
int llread(unsigned char *packet)
{   
    enum readState { START, FLAG_RCV, A_RCV, C_RCV , BCC1_OK, DATA_RCV, STOP_READ };
    enum readState readState = START;
    unsigned int packetIter = 0;
    unsigned char dataBuffer[MAX_PAYLOAD_SIZE];
    int dataBufferIter = 0; 
    unsigned char byte;
    while (readState != STOP_READ && packetIter < MAX_PAYLOAD_SIZE+15){
        readByteSerialPort(&byte);
        // printf("got %02x\n" , byte);
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
                printf("got address\n");
                if (byte == INFO_FRAME_0 || byte == INFO_FRAME_1) readState = C_RCV;
                else if(byte == FLAG) readState = FLAG_RCV;
                else readState = START;
                break;
            case C_RCV:
                printf("got control\n");
                if(byte == (SENDER_ADDRESS ^INFO_FRAME_0) || byte == (SENDER_ADDRESS ^INFO_FRAME_1)) readState = BCC1_OK;
                else if(byte == FLAG) readState = FLAG_RCV;
                else readState = START;
                break;
            case BCC1_OK:
                printf("bcc1 is ok\n");
                if(byte == FLAG) readState = FLAG_RCV;
                else {
                    dataBuffer[dataBufferIter] = byte;
                    dataBufferIter++;
                    readState = DATA_RCV;
                }
                break;
            case DATA_RCV:
            // printf("getting data %02x \n", byte);
                if (byte == FLAG){ 
                    // if (calculateBCC2(dataBuffer , dataBufferIter-1) == dataBuffer[dataBufferIter-1]){
                    //     readState = STOP_READ;
                    // }
                    // else{
                    //     printf("bcc2 is wrong, rejecting\n");
                    //     printf("BCC2 check: XORing %d bytes\n", dataBufferIter - 1);
                    //     for (int i = 0; i < dataBufferIter - 1; i++) printf("%02X ", dataBuffer[i]);
                    //     printf("\nExpected BCC2: %02X\n", dataBuffer[dataBufferIter - 1]);
                    //     printf("Calculated BCC2: %02X\n", calculateBCC2(dataBuffer, dataBufferIter - 1));
                    //     readState = START;
                    //     dataBufferIter = 0;
                    //     memset(dataBuffer, 0, sizeof(dataBuffer));
                    // }
                    readState = STOP_READ;
                }
                else {
                    dataBuffer[dataBufferIter] = byte;
                    dataBufferIter++;
                }
                break;
            // case BCC2_OK:
            //     printf ("got %02x\n",byte );
            //     printf("got data and bcc2 is ok\n");
            //     if(byte == FLAG) {
            //         readState = STOP_READ;
            //         printf("got final falg\n");
            //         }
            //     else readState = START;
            //     break;
            case STOP_READ:
                printf("stopping\n");
                break;
        }
        packetIter++;
    }
    if (readState != STOP_READ) { // stopped because buffer ran out which means failed
        sendRej(!frameNumber);
        return 0; // send rej message here or in application layer?
    } else {

        // Ensure the destuffed data fits within the packet buffer
        if (dataBufferIter - 4 > MAX_PAYLOAD_SIZE) {
            fprintf(stderr, "Error: Destuffed data exceeds MAX_PAYLOAD_SIZE\n");
            sendRej(!frameNumber);
            return -1; // Indicate an error
        }

        unsigned int finalMessageSize = bytedestuff(dataBuffer , dataBufferIter , packet);
        if (finalMessageSize > 1200) { // Check against packet buffer size
            fprintf(stderr, "Error: finalMessageSize exceeds packet buffer size\n");
            sendRej(!frameNumber);
            return -1; // Indicate an error
        }

        printf("destuffed\n");
        printf("finalMessageSize=%d\n", finalMessageSize);
        dataBufferIter = 0;
        sendRR(frameNumber);
        frameNumber = !frameNumber;
        return (int)finalMessageSize;
    }
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
        printf("got disc\n");
        if (expectUA(globalLinklayer.timeout) < 0){
            perror("expectUA");
        }
        printf("got ua\n");
        if (closeSerialPort()<0){
            perror("closeSerialPort\n");
            exit(-1);
        }
        printf("closed serial\n");
    }

    if (globalLinklayer.role == LlTx){
        if (sendDisconnect(SENDER_ADDRESS)<0)
            perror("sendDisconnect");
        printf("sent disc\n");
        if (expectDISC()<0)
            perror("expectDISC");
        else {
            unsigned char ua[5] = {FLAG, SENDER_ADDRESS, CONTROL_UA, (unsigned char)(SENDER_ADDRESS^CONTROL_UA), FLAG};
            if (writeBytesSerialPort(ua, 5) < 0)
                perror("writeBytesSerialPort (UA)");
            printf("wrote ua\n");\
        }

        if (closeSerialPort() < 0){
            perror("closeSerialPort");
            exit(-1);
        }

        printf("Port %s closed\n", globalLinklayer.serialPort);
    }

    return 0;
}


