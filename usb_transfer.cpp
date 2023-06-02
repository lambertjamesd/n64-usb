#include "usb_transfer.h"

#include <Arduino.h>
#include "descriptor_parser.h"

char gHexCharacter[] = {
  '0', '1', '2', '3',
  '4', '5', '6', '7',
  '8', '9', 'A', 'B',
  'C', 'D', 'E', 'F',
};

void printHex(uint8_t value) {
  Serial.write(gHexCharacter[value >> 4]);
  Serial.write(gHexCharacter[value & 0xF]);
}

void printBinary(uint8_t value) {
  for (uint8_t i = 0; i < 8; ++i) {
    if (value & 0x80) {
      Serial.write('1');
    } else {
      Serial.write('0');
    }

    value <<= 1;
  }
}

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

uint8_t gNextAddress = 1;
uint8_t gReadBuffer[64];

void debugPrintBuffer(uint8_t* data, uint8_t bytes) {
  for (uint8_t i = 0; i < bytes; ++i) {
    printHex(data[i]);

    if ((i & 0x7) == 0x7) {
      Serial.write('\n');
    } else {
      Serial.write(' ');
    }
  }

  if ((bytes & 0x7) != 0) {
    Serial.write('\n');
  }
}

uint8_t findNextUSBAddress() {
  uint8_t result = gNextAddress;
  ++gNextAddress;
  return result;
}

bool setupConnectedUSBDevice(struct HidInfo* hidInfo) {
#if DEBUG
  Serial.print("Setting target address to 0\n");
#endif
  usbWriteByte(SET_USB_ADDR, false);
  usbWriteByte(0x00, true);

  uint8_t address = findNextUSBAddress();
#if DEBUG
  Serial.print("Configuring target to have address ");
  Serial.print(address);
  Serial.print("\n");
#endif
  usbWriteByte(SET_ADDRESS, false);
  usbWriteByte(address, true);

  if (waitForInterrupt() != USB_INT_SUCCESS) {
#if DEBUG
    Serial.print("Failed to configure device address\n");
#endif
    return false;
  }

#if DEBUG
  Serial.print("Configured device address\n");
  Serial.print("Getting boot mouse\n");
#endif

  usbWriteByte(SET_USB_ADDR, false);
  usbWriteByte(address, true);

  if (!getHIDInfo(hidInfo)) {
    Serial.print("Could not find boot mouse ");
    return false;
  }

#if DEBUG
  Serial.print("Found boot mouse at ");
  printHex(hidInfo->bootMouseConfiguration);
  Serial.print(", ");
  printHex(hidInfo->bootMouseInterface);
  Serial.print(", ");
  printHex(hidInfo->bootMouseEndpoint);
  Serial.print("\n");
#endif

  if (!writeControlTransfer(0, REQUEST_TYPE_STANDARD | REQUEST_RECIPIENT_DEVICE, SET_CONFIGURATION, hidInfo->bootMouseConfiguration, 0, 0, NULL)) {
    Serial.print("Could not set configuration\n");
    return false;
  }

  if (!writeControlTransfer(0, REQUEST_TYPE_CLASS | REQUEST_RECIPIENT_INTERFACE, SET_PROTOCOL, SET_PROTOCOL_BOOT, hidInfo->bootMouseInterface, 0, NULL)) {
    Serial.print("Could not set protocol\n");
    return false;
  }

  return true;
}