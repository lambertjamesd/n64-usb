#ifndef __USB_TRANSFER_H__
#define __USB_TRANSFER_H__

#include <stdint.h>
#include <stdbool.h>

typedef void (*PacketHandler)(void* data, char* packetData, uint8_t packetSize, uint8_t offset);

#define REQUEST_DIRECTION_H2D    0x00
#define REQUEST_DIRECTION_D2H    0x80

#define REQUEST_TYPE_STANDARD    0x00
#define REQUEST_TYPE_CLASS       0x20
#define REQUEST_TYPE_VENDOR      0x40

#define REQUEST_RECIPIENT_DEVICE    0x00
#define REQUEST_RECIPIENT_INTERFACE 0x01
#define REQUEST_RECIPIENT_ENDPOINT  0x02
#define REQUEST_RECIPIENT_OTHER     0x03

// device request types
#define GET_DESCRIPTOR      0x06
#define SET_CONFIGURATION   0x09

// interface request types
#define SET_PROTOCOL        0x0B

#define SET_PROTOCOL_BOOT   0x00
#define SET_PROTOCOL_REPORT 0x01

#define PACK_WORD_BYTES(a, b)   ((uint16_t)(b) | ((uint16_t)(a) << 8))

bool readControlTransfer(uint8_t endpoint, uint8_t bmRequestType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, uint16_t wLength, void* data, PacketHandler packetHandler);

#endif