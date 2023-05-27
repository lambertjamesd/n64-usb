
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
#define SET_USB_MODE  0x15
#define TEST_CONNECT  0x16
#define GET_STATUS    0x22

// possible values for GET_STATUS
#define USB_INT_SUCCESS     0x14
#define USB_INT_CONNECT     0x15
#define USB_INT_DISCONNECT  0x16
#define USB_INT_BUF_OVER    0x17

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

void setUSBMode(uint8_t mode) {
  usbWriteByte(SET_USB_MODE, false);
  usbWriteByte(mode, true);
}

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

bool handleConnect() {
  setUSBMode(USBModeReset);
  delay(40);
  setUSBMode(USBModeActive);

  uint8_t resetResult = waitForInterrupt();
  Serial.write("setUSBMode(USBModeActive): 0x");
  printHex(resetResult);
  Serial.write("\n");

  return resetResult == USB_INT_SUCCESS;
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
