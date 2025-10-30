#include "helpers.h"

// int fd = -1;           // File descriptor for open serial port
// struct termios oldtio; // Serial port settings to restore on closing

enum State{ START, FLAG_RCV, A_RCV, C_RCV, BCC_OK, TIMEOUT };
enum State UAstate = START;

enum State frameState = START;



// int openSerialPort(const char *serialPort, int baudRate)
// {
//     // Open with O_NONBLOCK to avoid hanging when CLOCAL
//     // is not yet set on the serial port (changed later)
//     int oflags = O_RDWR | O_NOCTTY | O_NONBLOCK;
//     fd = open(serialPort, oflags);
//     if (fd < 0)
//     {
//         perror(serialPort);
//         return -1;
//     }

//     // Save current port settings
//     if (tcgetattr(fd, &oldtio) == -1)
//     {
//         perror("tcgetattr");
//         return -1;
//     }

//     // Convert baud rate to appropriate flag

//     // Baudrate settings are defined in <asm/termbits.h>, which is included by <termios.h>
// #define CASE_BAUDRATE(baudrate) 
//     case baudrate:              
//         br = B##baudrate;       
//         break;

//     tcflag_t br;
//     switch (baudRate)
//     {
//         CASE_BAUDRATE(1200);
//         CASE_BAUDRATE(1800);
//         CASE_BAUDRATE(2400);
//         CASE_BAUDRATE(4800);
//         CASE_BAUDRATE(9600);
//         CASE_BAUDRATE(19200);
//         CASE_BAUDRATE(38400);
//         CASE_BAUDRATE(57600);
//         CASE_BAUDRATE(115200);
//     default:
//         fprintf(stderr, "Unsupported baud rate (must be one of 1200, 1800, 2400, 4800, 9600, 19200, 38400, 57600, 115200)\n");
//         return -1;
//     }
// #undef CASE_BAUDRATE

//     // New port settings
//     struct termios newtio;
//     memset(&newtio, 0, sizeof(newtio));

//     newtio.c_cflag = br | CS8 | CLOCAL | CREAD;
//     newtio.c_iflag = IGNPAR;
//     newtio.c_oflag = 0;

//     // Set input mode (non-canonical, no echo,...)
//     newtio.c_lflag = 0;
//     newtio.c_cc[VTIME] = 0; // Block reading
//     newtio.c_cc[VMIN] = 1;  // Byte by byte

//     tcflush(fd, TCIOFLUSH);

//     // Set new port settings
//     if (tcsetattr(fd, TCSANOW, &newtio) == -1)
//     {
//         perror("tcsetattr");
//         close(fd);
//         return -1;
//     }

//     // Clear O_NONBLOCK flag to ensure blocking reads
//     oflags ^= O_NONBLOCK;
//     if (fcntl(fd, F_SETFL, oflags) == -1)
//     {
//         perror("fcntl");
//         close(fd);
//         return -1;
//     }

//     return fd;
// }


// int closeSerialPort()
// {
//     // Restore the old port settings
//     if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
//     {
//         perror("tcsetattr");
//         return -1;
//     }

//     return close(fd);
// }


// int readByteSerialPort(unsigned char *byte)
// {
//     return read(fd, byte, 1);
// }


// int writeBytesSerialPort(const unsigned char *bytes, int nBytes)
// {
//     return write(fd, bytes, nBytes);
// }


int sendSupervisionFrame()
{
    unsigned char message[5] = {FLAG, SENDER_ADDRESS, CONTROL_SET, SENDER_ADDRESS^CONTROL_SET, FLAG};
    int bytes = writeBytesSerialPort(message, 5);
    if (bytes <= 0)
    {
        perror("failed to write supervision frame");
        return -1;
    }
    return bytes;
}

void UAalarmHandler(int signal)
{   
    // just interrupt the read
    UAstate = TIMEOUT;
    return;
}
int UAalarmSetup() {
    struct sigaction act = {0};
    act.sa_handler = &UAalarmHandler;
    if (sigaction(SIGALRM, &act, NULL) == -1)
    {
        perror("sigaction");
        return -1;
    }
    return 0;
}

void frameAlarmHandler(){
    frameState = TIMEOUT;
    return;
}

int frameAlarmSetup(){
    struct sigaction act = {0};
    act.sa_handler = &frameAlarmHandler;
    if (sigaction(SIGALRM, &act, NULL) == -1)
    {
        perror("sigaction");
        return -1;
    }
    return 0;
}

int expectUA(int timeout) {
    alarm(timeout);
    UAstate = START;
    unsigned char byte;
    /*
    Create a state machine to verify the UA frame, if it misses we retransmit the SET frame
    */
    while (UAstate != TIMEOUT) {
        if (readByteSerialPort(&byte) < 0) {
            perror("readByteSerialPort");
            return -1;
        }
        printf("Byte read: %02X\n", byte);
        switch (UAstate)
        {
        case START:
            if (byte == FLAG) {
            UAstate = FLAG_RCV;
            printf("FLAG received\n");
            }
            break;
        case FLAG_RCV:
            if (byte == RECEIVER_ACK_ADDRESS) UAstate = A_RCV;
            else if (byte != FLAG) UAstate = START;
            break;
        case A_RCV:
            if (byte == CONTROL_UA) UAstate = C_RCV;
            else if (byte == FLAG) UAstate = FLAG_RCV;
            else UAstate = START;
            break;
        case C_RCV:
            if (byte == (RECEIVER_ACK_ADDRESS ^ CONTROL_UA)) UAstate = BCC_OK;
            else if (byte == FLAG) UAstate = FLAG_RCV;
            else UAstate = START;
            break;
        case BCC_OK:
            if (byte == FLAG) return 0;
            else UAstate = START;
            break;
        case TIMEOUT:
            alarm(0); // disable alarm
            return -1; // timeout
            break;
        default:
            break;
        }
    }
    return 0;
}

int initiateSenderProtocol(int timeout, int maxRetries) {
    if (UAalarmSetup() < 0) return -1;
    int tries = 0;
    while (tries < maxRetries) {
        if (sendSupervisionFrame() < 0) return -1;
        if (expectUA(timeout) == 0) {
            printf("UA frame received successfully\n");
            return 0;
        }
        tries++;
        printf("Timeout, retrying... (%d/%d)\n", tries, maxRetries);
    }
    printf("Failed to receive UA frame after %d attempts\n", maxRetries);
    return -1;
}


int expectSupervisionFrame(int timeout, int maxTries){
    unsigned char byte;
    frameAlarmSetup();
    alarm(timeout);
    int tries = 0;
    printf("waiting supervision frame");
    while(frameState != TIMEOUT || tries < maxTries){
        if(readByteSerialPort(&byte) < 0){
            perror("readByteSerialPort");
        }
        printf("Byte read: %02X\n", byte);
        switch(frameState){
            case START:
                if(byte == FLAG){ 
                    frameState = FLAG_RCV;
                    printf("got flag");
                }
                break;
            case FLAG_RCV:
                if(byte == SENDER_ADDRESS) frameState = A_RCV;
                else if(byte != FLAG) frameState = START;
                break;
            case A_RCV:
                if(byte == CONTROL_SET) frameState = C_RCV;
                else if(byte == FLAG) frameState = FLAG_RCV;
                else frameState = START;
                break;
            case C_RCV:
                if(byte == (SENDER_ADDRESS ^ CONTROL_SET)) {frameState = BCC_OK;printf("got bcc , framestate %d" , frameState);}
                else if(byte == FLAG) frameState = FLAG_RCV;
                else frameState = START;
                break;
            case BCC_OK:
                if(byte == FLAG) {
                    alarm(0);
                    return 0;}
                else frameState = START;
                break;
            case TIMEOUT:
                tries++;
                alarm(timeout);
                printf("Timed");
                frameState = START;
                break;
        }
    }
    return 0;
}


//Generates an information frame and returns its size
int generateInformationFrame(const unsigned char *data, bool frameNumber, unsigned int size, unsigned char* message){ //framenumber is a bool because it is only between zero and 1 and we can save some bits
    unsigned char newData [MAX_PAYLOAD_SIZE*2];
    int dataSize = bytestuff(data , newData, size);
    message[0] = FLAG;
    message[1] = SENDER_ADDRESS;
    if(frameNumber == 0){
        message[2] = INFO_FRAME_0;
    } else {
        message[2] = INFO_FRAME_1;
    }
    message[3] = message[1] ^ message[2];
    memcpy(&message[4], newData, dataSize);
    message[4 + dataSize] = calculateBCC2(newData, dataSize);
    message[5 + dataSize] = FLAG;
    return 6+dataSize;
}

unsigned int bytestuff(const unsigned char* data, unsigned char* stuffedData, unsigned int size){
    int j = 0;
    for(int i = 0; i < size; i++){
        if(data[i] == FLAG){ // Swap 0x7E with 0x7D 0x5E
            stuffedData[j++] = 0x7D;
            stuffedData[j++] = 0x5E;
        } else if(data[i] == 0x7D){ //Swap 0x7D with 0x7D 0x5D
            stuffedData[j++] = 0x7D;
            stuffedData[j++] = 0x5D;
        } else {
            stuffedData[j++] = data[i];
        }
    }
    stuffedData[j] = '\0';
    return j;
}

int calculateBCC2(const unsigned char* data, int dataSize){
    unsigned char bcc2 = 0x00;
    for(int i = 0; i < dataSize; i++){
        bcc2 ^= data[i];
    }
    return bcc2;
}

bool waitWriteResponse(bool frameNum){
    enum responseState {START, FLAG_RCV, A_RCV, CONTROL_RCV, BCC1_OK , END};
    enum responseState state = START;
    unsigned char byte;
    bool rejected;
    unsigned int messageCounter = 0;
    while (state != END && messageCounter < 7) //The message size is supposed to be 5 we give 2 of buffer
    {
        readByteSerialPort(&byte);
        printf("%02x\n", byte);
        messageCounter++;
        switch (state)
        {
        case START:
            if (byte == FLAG) state = FLAG_RCV;
            break;
        case FLAG_RCV:
            if (byte == RECEIVER_ACK_ADDRESS) state = A_RCV;
            else if(byte == SENDER_ACK_ADDRESS) state = A_RCV;
            else if (byte != FLAG) state = START;
            break;
        case A_RCV:
            if (byte == FLAG) state = FLAG_RCV;
            else if (byte == RR0 + !frameNum) {
                state = CONTROL_RCV;
                rejected = false;
            } 
            else if (byte == REJ0 +!frameNum){
                state = CONTROL_RCV;
                rejected = true;
            }
            else{
                state = START;
            }
            break;
        case CONTROL_RCV:
            if (byte == FLAG) state = FLAG_RCV;
            else if (byte == (SENDER_ACK_ADDRESS ^ (rejected ? REJ0+!frameNum : RR0+!frameNum))) state = BCC1_OK;
            else state = START;
            break;
        case BCC1_OK:
            if (byte == FLAG) state = END;
            else state = START;
            break;
        case END:
            break;
        }
    }
        if (rejected || messageCounter >= 7){
            return 0;
        }
        return 1;
}


////////////////////////////////////////////////
// BYTEDESTUFF
////////////////////////////////////////////////
unsigned int bytedestuff(unsigned char *data, unsigned int dataSize, unsigned char *newData) {
    unsigned int j = 0;
    for (unsigned int i = 0; i < dataSize; i++) {
        if (j >= 1200) { // Prevent buffer overflow
            fprintf(stderr, "Error: Destuffed data exceeds packet buffer size\n");
            return 0; // Indicate an error
        }
        if (data[i] == 0x7D) {
            i++; // move to the next byte safely
            if (i >= dataSize) {
                fprintf(stderr, "Error in destuffing: unexpected end\n");
                return 0; // Indicate an error
            }
            if (data[i] == 0x5E) newData[j++] = 0x7E;
            else if (data[i] == 0x5D) newData[j++] = 0x7D;
            else {
                fprintf(stderr, "Error in destuffing: invalid escape\n");
                return 0; // Indicate an error
            }
        } else {
            newData[j++] = data[i];
        }
    }
    return j;
}
int sendDisconnect(unsigned char ADD){

    unsigned char disc[5] = {FLAG, ADD, CONTROL_DISC, (unsigned char)(ADD^CONTROL_DISC), FLAG};
    int bytes = writeBytesSerialPort(disc, 5);
    if (bytes <= 0)
    {
        perror("failed to write supervision frame");
        return -1;
    }
    return bytes;
}

int expectDISC(){
    enum State state = START;
    unsigned char byte;
    unsigned char receivedADD = 0;

    while(state != TIMEOUT){
        if(readByteSerialPort(&byte) < 0){
            perror("readByteSerialPort");
            return -1;
        }
        printf("Byte read: %02X\n", byte);
        switch(state){
            case START:
                if(byte == FLAG) state = FLAG_RCV;
                break;
            case FLAG_RCV:
                if(byte == SENDER_ADDRESS || byte == RECEIVER_ADDRESS){ 
                    receivedADD = byte;
                    state = A_RCV;
                }
                else if (byte != FLAG) state = START;
                break;
            case A_RCV:
                if(byte == CONTROL_DISC) state = C_RCV;
                else if(byte == FLAG) state = FLAG_RCV;
                else state = START;
                break;
            case C_RCV:
                if(byte == (receivedADD ^ CONTROL_DISC)) state = BCC_OK;
                else if(byte == FLAG) state = FLAG_RCV;
                else state = START;
                break;
            case BCC_OK:
                if(byte == FLAG){
                    if (receivedADD == SENDER_ADDRESS){
                        if (sendDisconnect(RECEIVER_ADDRESS) < 0){
                            perror("sendDisconnect (RX response)");
                            return -1;
                        }
                        return 0;
                    }
                }
                else state = START;
                break;
            default:
                break;
        }
    }
    return 0;
}


int sendRej(bool frameNumber){
    unsigned char message[5];
       message[0] = FLAG; 
       message[1] = SENDER_ACK_ADDRESS; 
       message[2] = REJ0+frameNumber; 
       message[3] = SENDER_ACK_ADDRESS^(REJ0+frameNumber);
       message[4] = FLAG;

    int bytes = writeBytesSerialPort(message, 5);
    if (bytes <= 0)
    {
        perror("failed to write rejection frame");
        return -1;
    }
    printf("Reject frame %d\n" , frameNumber);
    return bytes;
};

int sendRR(bool frameNumber){
    unsigned char message[5];
       message[0] = FLAG; 
       message[1] = SENDER_ACK_ADDRESS; 
       message[2] = RR0+frameNumber; 
       message[3] = SENDER_ACK_ADDRESS^(RR0+frameNumber);
       message[4] = FLAG;

    int bytes = writeBytesSerialPort(message, 5);
    if (bytes <= 0)
    {
        perror("failed to write rr frame");
        return -1;
    }
    printf("Accepted frame %d\n", frameNumber);
    return bytes;
};
