
// PORTB
//     0 USB-D0 - input
//     1 USB-D1 - input
//     2 USB-D2 - input
//     3 USB-D3 - input

//     4 USB-D4 - input
//     5 USB-D5 - input
//     6 disconnected
//     7 disconnected

// PORTC
//     0 USB-A0 - output pull low only
#define USB_A0    (1 << 0)
//     1 B0 - input pull up
//     2 B1 - input pull up
//     3 B2 - input pull up

//     4 SDA - managed by wire library
//     5 SDL - managed by wire library

// PORTD
//     0 RSX - input
//     1 TSX - output
//     2 N64 - input
//     3 USB-INT - input

//     4 USB-RD - output pull low only
#define USB_RD      (1 << 4)
//     5 USB-WR - output pull low only
#define USB_WR      (1 << 5)
//     6 USB-D6 - input
//     7 USB-D7 - input

#define GET_IC_VER    0x01

void usbWriteByte(uint8_t byte, bool isData) {
  if (isData) {
    // send data
    DDRC |= USB_A0;
  } else {
    // send command
    PORTC &= ~USB_A0;
  }

  // configure the output pins 
  DDRB = ~byte;
  DDRD = (~byte & 0xC0) | (PORTD & 0x3F);

  // trigger write
  DDRD |= USB_WR;
  DDRD &= ~USB_WR;
}

uint8_t usbReadByte(bool isData) {
  if (isData) {
    // read data
    DDRC |= USB_A0;
  } else {
    // read interrupt
    PORTC &= ~USB_A0;
  }

  // configure the data line to be input pins
  DDRB = 0;
  DDRD &= 0x3F;

  // trigger read
  DDRD |= USB_RD;

  // read data
  uint8_t result = (PINB & 0x3F) | (PIND & 0xC0);
  // uint8_t result = PIND;

  // return write signal
  DDRD &= ~USB_RD;

  return result;
}

void setup() {
  DDRB = 0x00;
  PORTB = 0x00;

  // disable weak pull up
  DDRC = 0x00;
  PORTC = 0x0E;

  // configure data port
  DDRD = 0x02;
  PORTD = 0x00l;

  Serial.begin(9600);

  usbWriteByte(GET_IC_VER, false);
  Serial.write("Usb version: ");
  Serial.print(usbReadByte(true));
  Serial.write("\n");
}

void loop() {
  // put your main code here, to run repeatedly:

}
