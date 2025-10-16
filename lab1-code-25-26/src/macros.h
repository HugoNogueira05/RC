#ifndef   MACROS_H
#define   MACROS_H

// basic constants
#define  FALSE   0
#define  TRUE    1

// frame characters
#define FLAG 0X7E
#define SENDER_ADDRESS 0X03
#define RECEIVER_ACK_ADDRESS 0X03
#define RECEIVER_ADDRESS 0x01
#define SENDER_ACK_ADDRESS 0X01
#define CONTROL_SET 0X03
#define CONTROL_UA 0X07

// serial port config
#define _POSIX_SOURCE 1 // POSIX compliant source
#define BAUDRATE 38400
#define BUF_SIZE 256



#endif