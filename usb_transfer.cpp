#include "usb_transfer.h"

#include <Arduino.h>

#include "debug_print.h"

void usbWriteByte(uint8_t byte, bool isData) {
  // needed to space commands out
  asm volatile ("nop");
  asm volatile ("nop");
  asm volatile ("nop");
  asm volatile ("nop");

  asm volatile ("nop");
  asm volatile ("nop");
  asm volatile ("nop");
  asm volatile ("nop");

  if (isData) {
    // send data
    DDRD |= USB_A0;
  } else {
    // send command
    DDRD &= ~USB_A0;
  }

  // configure the output pins 
  DDRC = ((~byte & 0xE0) >> 5) | (DDRC & 0xF8);
  // DDRC |= 0x07;
  DDRB = (~byte) & 0x1F;

  // trigger write
  DDRD |= USB_WR;
  DDRD &= ~USB_WR;
  // needed to allow USB chip to finish reading
  asm volatile ("nop");
  asm volatile ("nop");
  asm volatile ("nop");
  asm volatile ("nop");

  asm volatile ("nop");
  asm volatile ("nop");
  asm volatile ("nop");
  asm volatile ("nop");

  DDRB = 0x00;
  DDRC &= 0xF8;
}

uint8_t usbReadByte() {
  // needed to space commands out
  delayMicroseconds(3);

  // configure the data line to be input pins and configure USB_A0 to read
  DDRD |= USB_A0;

  DDRB = 0x00;
  DDRC &= 0xF8;

  // trigger read
  DDRD |= USB_RD;
  // needed to let inputs stabilize
  asm volatile ("nop");
  asm volatile ("nop");
  asm volatile ("nop");
  asm volatile ("nop");

  // read data
  uint8_t result = (PINB & 0x1F) | ((PINC & 0x07) << 5);

  // turn off read signal
  DDRD &= ~USB_RD;

  return result;
}

uint8_t usbReadBuffer(uint8_t* buffer) {
  usbWriteByte(RD_USB_DATA, false);
  uint8_t result = usbReadByte();

  for (uint8_t i = 0; i < result; ++i) {
    buffer[i] = usbReadByte();
  }

  return result;
}

#define MAX_ATTEMPTS 10000

uint8_t waitForInterrupt() {
  uint16_t attempt = 0;
  while ((PIND & USB_INT)) {
    if (attempt == MAX_ATTEMPTS) {
      return 0;
    }

    ++attempt;
  }

  usbWriteByte(GET_STATUS, false);
  return usbReadByte();
}

bool issueToken(uint8_t endpoint, uint8_t packetType, bool oddParity) {
  usbWriteByte(ISSUE_TKN_X, false);
  usbWriteByte(oddParity ? 0x80 : 0x00, true);
  usbWriteByte((endpoint << 4) | packetType, true);
  return waitForInterrupt() == USB_INT_SUCCESS;
}

bool issueTokenRead(uint8_t endpoint, uint8_t packetType, bool oddParity) {
  usbWriteByte(ISSUE_TKN_X, false);
  usbWriteByte(oddParity ? 0x40 : 0x00, true);
  usbWriteByte((endpoint << 4) | packetType, true);

  return waitForInterrupt() == USB_INT_SUCCESS;
}

#define READ_PACKET_SIZE    8

bool readControlTransfer(uint8_t endpoint, uint8_t bmRequestType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, uint16_t wLength, void* data, PacketHandler packetHandler) {
  bmRequestType |= REQUEST_DIRECTION_D2H;

  usbWriteByte(WR_USB_DATA7, false);
  // number of bytes coming
  usbWriteByte(8, true);
  usbWriteByte(bmRequestType, true);
  usbWriteByte(bRequest, true);
  usbWriteByte(((uint8_t*)&wValue)[0], true);
  usbWriteByte(((uint8_t*)&wValue)[1], true);
  usbWriteByte(((uint8_t*)&wIndex)[0], true);
  usbWriteByte(((uint8_t*)&wIndex)[1], true);
  usbWriteByte(((uint8_t*)&wLength)[0], true);
  usbWriteByte(((uint8_t*)&wLength)[1], true);

  bool oddParity = true;
  char buffer[READ_PACKET_SIZE];

  if (!issueToken(endpoint, DEF_USB_PID_SETUP, false)) {
    return false;
  }

  uint8_t offset = 0;

  while (wLength > 0) {
    if (!issueToken(endpoint, DEF_USB_PID_IN, oddParity)) {
#if DEBUG
  Serial.print("Failed to get input data\n");
#endif
      return false;
    }
    
    oddParity = !oddParity;

    usbWriteByte(RD_USB_DATA0, false);

    uint8_t packetSize = usbReadByte();
    uint8_t curr = 0;

    wLength -= packetSize;

    while (packetSize > 0) {
      buffer[curr] = usbReadByte();
      ++curr;
      --packetSize;

      if (curr == READ_PACKET_SIZE) {
        packetHandler(data, buffer, curr, offset);
        offset += curr;
        curr = 0;
      }
    }

    usbWriteByte(UNLOCK_USB, false);
    
    if (curr) {
      packetHandler(data, buffer, curr, offset);
    }
  }

  usbWriteByte(WR_USB_DATA7, false);
  usbWriteByte(0, true);

  return issueToken(endpoint, DEF_USB_PID_OUT, oddParity);
}

#define MAX_WRITE_PACKET_SIZE   8

bool writeControlTransfer(uint8_t endpoint, uint8_t bmRequestType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, uint16_t wLength, char* toSend) {
  usbWriteByte(WR_USB_DATA7, false);
  // number of bytes coming
  usbWriteByte(8, true);
  usbWriteByte(bmRequestType, true);
  usbWriteByte(bRequest, true);
  usbWriteByte(((uint8_t*)&wValue)[0], true);
  usbWriteByte(((uint8_t*)&wValue)[1], true);
  usbWriteByte(((uint8_t*)&wIndex)[0], true);
  usbWriteByte(((uint8_t*)&wIndex)[1], true);
  usbWriteByte(((uint8_t*)&wLength)[0], true);
  usbWriteByte(((uint8_t*)&wLength)[1], true);

  bool oddParity = true;
  char buffer[READ_PACKET_SIZE];

  if (!issueToken(endpoint, DEF_USB_PID_SETUP, false)) {
    return false;
  }

  uint8_t offset = 0;

  while (wLength > 0) {
    uint8_t chunkSize = wLength;

    if (chunkSize > MAX_WRITE_PACKET_SIZE) {
      chunkSize = MAX_WRITE_PACKET_SIZE;
    }

    usbWriteByte(WR_USB_DATA7, false);
    // number of bytes coming
    usbWriteByte(chunkSize, true);

    for (uint8_t i = 0; i < chunkSize; ++i) {
      usbWriteByte(toSend[offset], true);
      ++offset;
    }

    usbWriteByte(WR_USB_DATA7, false);
    usbWriteByte(0, true);

    if (!issueToken(endpoint, DEF_USB_PID_OUT, oddParity)) {
#if DEBUG
  Serial.print("Failed to get input data\n");
#endif
      return false;
    }
    
    oddParity = !oddParity;
  }

  return issueToken(endpoint, DEF_USB_PID_IN, oddParity);
}

void setUSBMode(uint8_t mode) {
  usbWriteByte(SET_USB_MODE, false);
  usbWriteByte(mode, true);
}

void setRetry(bool shouldRetry) {
  usbWriteByte(SET_RETRY, false);
  usbWriteByte(0x25, true);
  usbWriteByte(shouldRetry ? 0x85 : 0x00, true);

}