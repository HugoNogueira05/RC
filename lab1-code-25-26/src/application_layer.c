// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int getFileSize(FILE *file);
int sendCP(unsigned char** message , long fileSize , int open);
int sendDP(unsigned char** message , long fileSize, unsigned char **fileContent);



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
    if(llopen(connectionParameters) == -1){
        perror("llopen\n");
        exit(1);
    } // works

    if (strcmp(role,"tx") == 0){
        FILE* file = fopen(filename , "r");
        int fileSize = getFileSize(file);
        unsigned char *message = malloc(3+sizeof(fileSize));
        int size= sendCP(&message, fileSize, 1); //send control packet with start information
        if(size >0){
            llwrite(message, size);
            printf("Wrote control packet\n");
        }
        unsigned char* buf = malloc(MAX_PAYLOAD_SIZE) ;

        int read;
        if (!buf){
            perror("buf\n");
            exit(1);
        }

        unsigned char *message2 = malloc((MAX_PAYLOAD_SIZE*2)+3);
        if (!message2) {
            perror("malloc buf failed");
            exit(1);
        }

        while((read = fread(buf, sizeof(unsigned char) , MAX_PAYLOAD_SIZE -200, file))){


            printf("read %d elements\n", read);
            int size = sendDP(&message2, (long)read , &buf);
            llwrite(message2, size);
            printf("wrote succesfully\n");
        }
        size = sendCP(&message, fileSize, 0);
        if(size > 0){
            llwrite(message, size);
            printf("Wrote control packet to disconnect\n");
            llclose();
        }
    }
    else if(strcmp(role,"rx") == 0){
        printf("Receiver Logic\n");
        FILE* file = fopen(filename , "w");
        unsigned char message [MAX_PAYLOAD_SIZE *2];
        enum readState {START, FILE, END};
        enum readState readState = START;
        unsigned char* packet = malloc(1200);
        unsigned char* startPacket;
        while (readState != END){
            int size = llread(packet);
            printf("got %d bytes\n", size);
            printf("readstate is %d\n", readState);
            if(size>0){
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
                        else{
                            printf("got data\n");
                            printf("size is %d\n", size);
                            if (!file){
                                perror("file\n");
                                exit(1);
                            }
                            printf("writing to file\n");
                            int written = fwrite(packet+3, sizeof(unsigned char),size-4 , file);
                            fflush(file);
                            printf("Wrote %d bytes to penguin\n", written);
                        }
                        break;

                }
            }
        
        }
        printf("opened file to write\n");

    }
}


int sendCP(unsigned char** message , long fileSize , int open){
    int index = 0;
    if(open == 1){
        (*message)[index++] = 1;
    }
    else{
        (*message)[index++] = 3;
    }
    (*message)[index++] = 0;
    (*message)[index++] = sizeof(fileSize);

    memcpy(&(*message)[index], &fileSize, sizeof(fileSize));
    index += sizeof(fileSize);

    return index;
}

int sendDP(unsigned char** message , long fileSize, unsigned char **fileContent){
    int index = 0;
    (*message)[index++] = 2;


    memcpy(&(*message)[index], &fileSize, 2);

    index+=2;
    memcpy(&(*message)[index], *fileContent, fileSize);
    index += fileSize;


    return index;
}




int getFileSize(FILE *file) {
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    return (int)size;
}