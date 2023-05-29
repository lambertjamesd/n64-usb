#ifndef __USB_TRANSFER_H__
#define __USB_TRANSFER_H__

#include <stdint.h>

typedef void (*PacketHandler)(void* data, char* packetData, uint8_t packetSize);

#endif