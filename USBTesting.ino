
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
#define TEST_CONNECT  0x16
#define GET_STATUS    0x22

void usbWriteByte(uint8_t byte, bool isData) {
  delayMicroseconds(2);
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

  PORTC &= 0xF8;
  PORTB = 0;

  delayMicroseconds(2);

  // trigger write
  DDRD |= USB_WR;
  asm volatile ("nop");
  asm volatile ("nop");
  asm volatile ("nop");
  asm volatile ("nop");
  DDRD &= ~USB_WR;
  asm volatile ("nop");
  asm volatile ("nop");
  asm volatile ("nop");
  asm volatile ("nop");

  // reconfigure inputs
  PORTB = 0;
  PORTC &= 0xFB;

  DDRB = 0x00;
  DDRC &= 0xF8;
}

uint8_t usbReadByte() {
  delayMicroseconds(2);
  // configure the data line to be input pins and configure USB_A0 to read
  DDRD |= USB_A0;

  DDRB = 0x00;
  DDRC &= 0xF8;

  // trigger read
  DDRD |= USB_RD;
  
  delayMicroseconds(2);

  // read data
  uint8_t result = (PINB & 0x1F) | ((PINC & 0x07) << 5);

  // return read signal
  DDRD &= ~USB_RD;

  return result;
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

  usbWriteByte(RESET_ALL, false);
  delay(40);
  usbWriteByte(GET_IC_VER, false);
  Serial.write("GET_IC_VER: 0x");
  printHex(usbReadByte());
  Serial.write("\n");
}

uint8_t testConnectId = 0;

void loop() {
  if (!(PIND & USB_INT)) {
    usbWriteByte(GET_STATUS, false);
    Serial.write("GET_STATUS: ");
    Serial.print(usbReadByte());
    Serial.write("\n");
  }

  usbWriteByte(CHECK_EXIST, false);
  usbWriteByte(testConnectId, true);
  Serial.write("CHECK_EXIST: 0b");
  printBinary(testConnectId);
  Serial.write(" -> 0b");
  printBinary(usbReadByte());
  Serial.write("\n");

  ++testConnectId;

  // usbWriteByte(GET_IC_VER, false);
  // Serial.write("GET_IC_VER: ");
  // Serial.print(usbReadByte());
  // Serial.write("\n");

  delay(500);
}
