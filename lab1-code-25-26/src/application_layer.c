// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"
#include <stdio.h>
#include <string.h>

int getFileSize(FILE *file);
int sendCP(unsigned char** message , long fileSize , int open);


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
        int fileSize = getFileSize(file);
        unsigned char message[3 + sizeof(fileSize)];
        int size= sendCP(&message, fileSize, 1);
        if(size >0){
            llwrite(message, size);
            printf("Wrote control packet\n");
        }
        printf("opened file\n");
        unsigned char buf [MAX_PAYLOAD_SIZE];
        int read;
        while((read = fread(buf, sizeof(unsigned char) , MAX_PAYLOAD_SIZE , file))){
            printf("read %d elements\n", read);
            llwrite(buf, MAX_PAYLOAD_SIZE);
        }
    }

    else if(strcmp(role,"rx") == 0){
        FILE* file = fopen(filename , "w+");
        unsigned char message [MAX_PAYLOAD_SIZE *2];
        enum readState {START, FILE, END};
        enum readState readState = START;
        unsigned char* packet;
        unsigned char* startPacket;
        while (readState != END){
            llread(packet);
            switch (readState){
                case START:
                    if(packet[0] == 1){
                        readState = FILE;
                        startPacket = packet+1;
                        printf("switched readState\n");
                    }
                    break;
                case FILE:
                    if(packet[0] == 3 && memcmp(packet+1 , startPacket , 2+packet[1])){
                        readState = END;
                    }

            }
        }

        llread(message);
        printf("opened file to write\n");

    }
}


int sendCP(unsigned char** message , long fileSize , int open){
    int index = 0;
    if(open == 1){
        message[index++] = 1;
    }
    else{
        message[index++] = 3;
    }
    message[index++] = 0;
    message[index++] = sizeof(fileSize);

    memcpy(&message[index], &fileSize, sizeof(fileSize));
    index += sizeof(fileSize);

    return index;
}

int getFileSize(FILE *file) {
    int size;
    long currentPos = ftell(file);  
    fseek(file, 0, SEEK_END);       
    size = ftell(file);            
    fseek(file, currentPos, SEEK_SET);
    return size;
}