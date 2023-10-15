#ifndef _UTILS_H_
#define _UTILS_H_

#define BUF_SIZE 256


#define FLAG    0x7E    // Synchronisation: start or end of frame
#define AR      0x03    // Address field in frames that are commands sent by the Transmitter or replies sent by the Receiver
#define AT      0x01    // Address field in frames that are commands sent by the Receiver or replies sent by the Transmitter

#define ESC_B1  0x7D    // First Escape Byte for Byte Stuffing (0x7E == 0x7D 0x5E)
#define ESC_B2  0x5E    // First/Second Escape Byte for Byte Stuffing (0x7E == 0x7D 0x5E or 0x7D == 0x7D 0x5D)
#define ESC_B3  0x5D    // Second Escape Byte for Byte Stuffing (0x7D == 0x7D 0x5D)

/* Tramas S e U */
#define SET     0x03    // SET frame: sent by the transmitter to initiate a connection
#define UA      0x07    // UA frame: confirmation to the reception of a valid supervision frame
#define RR0     0x05    // RR0 frame: indication sent by the Receiver that it is ready to receive an information frame number 0
#define RR1     0x85    // RR1 frame: indication sent by the Receiver that it is ready to receive an information frame number 1
#define REJ0    0x01    // REJ0 frame: indication sent by the Receiver that it rejects an information frame number 0 (detected an error)
#define REJ1    0x81    // REJ1 frame: indication sent by the Receiver that it rejects an information frame number 1 (detected an error)
#define DISC    0x0B    // DISC frame to indicate the termination of a connection

/* Tramas I */
#define CI_1    0x00    // Information frame number 0
#define CI_2    0x40    // Information frame number 1

#endif // _UTILS_H
