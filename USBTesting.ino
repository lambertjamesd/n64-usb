
#include "descriptor_parser.h"
#include "usb_transfer.h"

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
#define SET_USB_ADDR  0x13
#define SET_USB_MODE  0x15
#define TEST_CONNECT  0x16
#define GET_STATUS    0x22
#define RD_USB_DATA0  0x27
#define RD_USB_DATA   0x28
#define WR_USB_DATA7  0x2B
#define SET_ADDRESS   0x45
#define GET_DESCR     0x46
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

#define DEBUG     1

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
  asm volatile ("nop");
  asm volatile ("nop");
  asm volatile ("nop");
  asm volatile ("nop");

  asm volatile ("nop");
  asm volatile ("nop");
  asm volatile ("nop");
  asm volatile ("nop");

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

enum USBMode {
  USBModeIdle = 0x05,
  USBModeReset = 0x07,
  USBModeActive = 0x06,
};

uint8_t usbReadBuffer(uint8_t* buffer) {
  usbWriteByte(RD_USB_DATA, false);
  uint8_t result = usbReadByte();

  for (uint8_t i = 0; i < result; ++i) {
    buffer[i] = usbReadByte();
  }

  return result;
}

#define MAX_WAIT_TIME 100

uint8_t waitForInterrupt() {
  unsigned long startTime = millis();
  while ((PIND & USB_INT)) {
    if (millis() - startTime > MAX_WAIT_TIME) {
      return 0;
    }
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

#define READ_PACKET_SIZE    8

bool readControlTransfer(uint8_t endpoint, uint8_t bmRequestType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, uint16_t wLength, void* data, PacketHandler packetHandler) {
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

  while (wLength > 0) {
    if (!issueToken(endpoint, DEF_USB_PID_IN, oddParity)) {
      return false;
    }
    
    oddParity = !oddParity;

    usbWriteByte(RD_USB_DATA, false);

    uint8_t packetSize = usbReadByte();
    uint8_t curr = 0;

    wLength -= packetSize;

    while (packetSize > 0) {
      buffer[curr] = usbReadByte();
      ++curr;
      --packetSize;

      if (curr == READ_PACKET_SIZE) {
        packetHandler(data, buffer, curr);
        curr = 0;
      }
    }
    
    if (curr) {
      packetHandler(data, buffer, curr);
    }
  }

  oddParity = !oddParity;

  usbWriteByte(WR_USB_DATA7, false);
  usbWriteByte(0, true);

  return issueToken(endpoint, DEF_USB_PID_OUT, oddParity);
}

void setUSBMode(uint8_t mode) {
  usbWriteByte(SET_USB_MODE, false);
  usbWriteByte(mode, true);
}

uint8_t gNextAddress = 1;
uint8_t gReadBuffer[64];

void debugPrintBuffer(uint8_t bytes) {
  for (uint8_t i = 0; i < bytes; ++i) {
    printHex(gReadBuffer[i]);

    if ((i & 0x7) == 0x7) {
      Serial.write('\n');
    } else {
      Serial.write(' ');
    }
  }

  Serial.write('\n');
}

uint8_t findNextUSBAddress() {
  uint8_t result = gNextAddress;
  ++gNextAddress;
  return result;
}

bool setupConnectedUSBDevice() {
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
  Serial.print("Getting device descriptor\n");
#endif

  usbWriteByte(SET_USB_ADDR, false);
  usbWriteByte(address, true);

  usbWriteByte(GET_DESCR, false);
  usbWriteByte(1, true);

  uint8_t getDescrResult = waitForInterrupt();

  if (getDescrResult != USB_INT_SUCCESS) {
#if DEBUG
    Serial.print("Failed to get device descriptor with error 0x");
    printHex(getDescrResult);
    Serial.print("\n");
#endif
    return false;
  }

  uint8_t descriptorSize = usbReadBuffer(gReadBuffer);

#if DEBUG
  Serial.print("Got descriptor of size ");
  Serial.print(descriptorSize);
  Serial.print("\n");
  debugPrintBuffer(descriptorSize);

  Serial.print("Getting configuration descriptor\n");
#endif

  usbWriteByte(GET_DESCR, false);
  usbWriteByte(2, true);

  getDescrResult = waitForInterrupt();

  if (getDescrResult != USB_INT_SUCCESS) {
#if DEBUG
    Serial.print("Failed to get configuration descriptor with error 0x");
    printHex(getDescrResult);
    Serial.print("\n");
#endif
    return false;
  }

  descriptorSize = usbReadBuffer(gReadBuffer);

#if DEBUG
  Serial.print("Got descriptor of size ");
  Serial.print(descriptorSize);
  Serial.print("\n");
  debugPrintBuffer(descriptorSize);
#endif

  return true;
}

bool handleConnect() {
  setUSBMode(USBModeReset);
  delay(40);
  setUSBMode(USBModeActive);

  uint8_t resetResult = waitForInterrupt();

#if DEBUG
  Serial.write("setUSBMode(USBModeActive): 0x");
  printHex(resetResult);
  Serial.write("\n");
#endif

  if (resetResult != USB_INT_CONNECT) {
#if DEBUG
  Serial.write("Failed to setup USB mode\n");
#endif
    return false;
  }

  return setupConnectedUSBDevice();
}

void handleDisconnect() {
  setUSBMode(USBModeIdle);
}

void setup() {
  pinMode(2, INPUT); // N64
  pinMode(3, INPUT); // USB-INT

  pinMode(4, INPUT); // USB-RD
  pinMode(5, INPUT); // USB-WR
  pinMode(6, INPUT); // USB-A0

  pinMode(8, INPUT); // USB-D0
  pinMode(9, INPUT); // USB-D1
  pinMode(10, INPUT); // USB-D2
  pinMode(11, INPUT); // USB-D3
  pinMode(12, INPUT); // USB-D4

  pinMode(A0, INPUT); // USB-D5
  pinMode(A1, INPUT); // USB-D6
  pinMode(A2, INPUT); // USB-D7

  Serial.begin(9600);

  usbWriteByte(GET_IC_VER, false);
  Serial.write("GET_IC_VER: 0x");
  printHex(usbReadByte());
  Serial.write("\n");

  setUSBMode(USBModeIdle);
}

uint8_t testConnectId = 0;

void loop() {
  if (!(PIND & USB_INT)) {
    usbWriteByte(GET_STATUS, false);
    uint8_t interrupt = usbReadByte();

    Serial.write("GET_STATUS: 0x");
    printHex(interrupt);
    Serial.write("\n");

    switch (interrupt & 0x1F) {
      case USB_INT_CONNECT:
        handleConnect();
        break;
      case USB_INT_DISCONNECT:
        handleDisconnect();
        break;
    }
  }

  usbWriteByte(CHECK_EXIST, false);
  usbWriteByte(testConnectId, true);
  uint8_t result = usbReadByte();

  if (result != (uint8_t)~testConnectId) {
    Serial.write("CHECK_EXIST: 0x");
    printHex(testConnectId);
    Serial.write(" -> 0x");
    printHex(result);
    Serial.write("\n");

    delay(1000);
  }

  ++testConnectId;

  delay(10);
}
