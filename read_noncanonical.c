// Example of how to read from the serial port in non-canonical mode
//
// Modified by: Eduardo Nuno Almeida [enalmeida@fe.up.pt]


#include "helpers.h"


volatile int STOP = FALSE;

// ---------------------------------------------------
// MAIN
// ---------------------------------------------------
int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("Incorrect program usage\n"
               "Usage: %s <SerialPort>\n"
               "Example: %s /dev/ttyS0\n",
               argv[0],
               argv[0]);
        exit(1);
    }

    // Open serial port device for reading and writing, and not as controlling tty
    // because we don't want to get killed if linenoise sends CTRL-C.
    //
    // NOTE: See the implementation of the serial port library in "serial_port/".
    const char *serialPort = argv[1];

    if (openSerialPort(serialPort, BAUDRATE) < 0)
    {
        perror("openSerialPort");
        exit(-1);
    }

    printf("Serial port %s opened\n", serialPort);

    // Read from serial port until the 'z' char is received.

    // NOTE: This while() cycle is a simple example showing how to read from the serial port.
    // It must be changed in order to respect the specifications of the protocol indicated in the Lab guide.

    // TODO: Save the received bytes in a buffer array and print it at the end of the program.
    int nBytesBuf = 0;

    while (STOP == FALSE)
    {
        // Read one byte from serial port.
        // NOTE: You must check how many bytes were actually read by reading the return value.
        // In this example, we assume that the byte is always read, which may not be true.
        unsigned char byte;
        int bytes = readByteSerialPort(&byte);
        nBytesBuf += bytes;

        printf("Byte received: %x\n", byte);

        if (byte == FLAG)
        {
            printf("Received flag char. Responding.\n");
            unsigned char response[5] = {FLAG, 0x03, 0x00, 0x03, FLAG};
            writeBytesSerialPort(response, 5);
            STOP = TRUE;
        }
    }

    printf("Total bytes received: %d\n", nBytesBuf);

    // Close serial port
    if (closeSerialPort() < 0)
    {
        perror("closeSerialPort");
        exit(-1);
    }

    printf("Serial port %s closed\n", serialPort);

    return 0;
}

// ---------------------------------------------------
// SERIAL PORT LIBRARY IMPLEMENTATION
// ---------------------------------------------------



