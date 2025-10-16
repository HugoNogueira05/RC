#include "helpers.h"

// int fd = -1;           // File descriptor for open serial port
// struct termios oldtio; // Serial port settings to restore on closing

enum State{ START, FLAG_RCV, A_RCV, C_RCV, BCC_OK, STOP, TIMEOUT };
enum State UAstate = START;

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
int alarmSetup() {
    struct sigaction act = {0};
    act.sa_handler = &UAalarmHandler;
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
    while (UAstate != STOP) {
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
            if (byte == FLAG) UAstate = STOP;
            else UAstate = START;
            break;
        case STOP:
            alarm(0); // disable alarm
            return 0;
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
    if (alarmSetup() < 0) return -1;
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


int expectSupervisionFrame(){
    enum State state = START;
    unsigned char byte;
    while(state != STOP){
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
                if(byte == SENDER_ADDRESS) state = A_RCV;
                else if(byte != FLAG) state = START;
                break;
            case A_RCV:
                if(byte == CONTROL_SET) state = C_RCV;
                else if(byte == FLAG) state = FLAG_RCV;
                else state = START;
                break;
            case C_RCV:
                if(byte == (SENDER_ADDRESS ^ CONTROL_SET)) state = BCC_OK;
                else if(byte == FLAG) state = FLAG_RCV;
                else state = START;
                break;
            case BCC_OK:
                if(byte == FLAG) state = STOP;
                else state = START;
                break;
            default:
                break;
        }
    }
    return 0;
}