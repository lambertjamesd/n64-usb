#ifndef __USB_TRANSFER_H__
#define __USB_TRANSFER_H__

#include <stdint.h>
#include <stdbool.h>

// PORTB
//     0 USB-D0 - input
//     1 USB-D1 - input
//     2 USB-D2 - input
//     3 USB-D3 - input

//     4 USB-D4 - input
//     5 
//     6 disconnected
//     7 disconnected

// PORTC
//     0 USB-D5 - input
//     1 USB-D6 - input
//     2 USB-D7 - input
//     3 

//     4 SDA - managed by wire library
//     5 SDL - managed by wire library

// PORTD
//     0 RSX - input
//     1 TSX - output
//     2 N64 - input
//     3 USB-INT - input
#define USB_INT     (1 << 3)
//     4 USB-RD - output pull low only
#define USB_RD      (1 << 4)
//     5 USB-WR - output pull low only
#define USB_WR      (1 << 5)
//     6 USB-A0 - output pull low only
#define USB_A0      (1 << 6)
//     7 

#define GET_IC_VER    0x01
#define RESET_ALL     0x05
#define CHECK_EXIST   0x06
#define SET_RETRY     0x0B
#define SET_USB_ADDR  0x13
#define SET_USB_MODE  0x15
#define TEST_CONNECT  0x16
#define GET_STATUS    0x22
#define UNLOCK_USB    0x23
#define RD_USB_DATA0  0x27
#define RD_USB_DATA   0x28
#define WR_USB_DATA7  0x2B
#define SET_ADDRESS   0x45
#define GET_DESCR     0x46
#define SET_CONFIG    0x49
#define ISSUE_TKN_X   0x4E
#define ISSUE_TOKEN   0x4F


// possible values for GET_STATUS
#define USB_INT_SUCCESS     0x14
#define USB_INT_CONNECT     0x15
#define USB_INT_DISCONNECT  0x16
#define USB_INT_BUF_OVER    0x17

#define DEF_USB_PID_SETUP   0xD
#define DEF_USB_PID_OUT     0x1
#define DEF_USB_PID_IN      0x9

#define GET_DESCRIPTOR      0x06

// HID Control transfer types
#define GET_REPORT          0x01
#define GET_IDLE            0x02
#define GET_PROTOCOL        0x03
#define SET_REPORT          0x09
#define SET_IDLE            0x0A 
#define SET_PROTOCOL        0x0B 

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

enum USBMode {
  USBModeIdle = 0x05,
  USBModeReset = 0x07,
  USBModeActive = 0x06,
};

void usbWriteByte(uint8_t byte, bool isData);
uint8_t usbReadByte();
uint8_t usbReadBuffer(uint8_t* buffer);
bool issueTokenRead(uint8_t endpoint, uint8_t packetType, bool oddParity);
uint8_t waitForInterrupt();
bool readControlTransfer(uint8_t endpoint, uint8_t bmRequestType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, uint16_t wLength, void* data, PacketHandler packetHandler);
bool writeControlTransfer(uint8_t endpoint, uint8_t bmRequestType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, uint16_t wLength, char* toSend);
void setUSBMode(uint8_t mode);
void setRetry(bool shouldRetry);

void printHex(uint8_t);
void debugPrintBuffer(uint8_t* data, uint8_t bytes);

#endif