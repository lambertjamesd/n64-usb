
#include "descriptor_parser.h"
#include "usb_transfer.h"

struct HidInfo gHid;

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

  if (!setupConnectedUSBDevice(&gHid)) {
    gHid.bootMouseEndpoint = 0;
    return false;
  }

  return true;
}

void handleDisconnect() {
  gHid.bootMouseEndpoint = 0;
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

  pinMode(13, OUTPUT);

  Serial.begin(9600);

  usbWriteByte(RESET_ALL, false);
  delay(40);

  usbWriteByte(GET_IC_VER, false);
  Serial.write("GET_IC_VER: 0x");
  printHex(usbReadByte());
  Serial.write("\n");

  setUSBMode(USBModeIdle);  

  // turn retries off for polling
  setRetry(false);

  // needs to work wouth TIMSK0
  TIMSK0 = 0;
}

bool gOddPollParity = false;
int gFailCount = 0;

void loop() {
  if (!(PIND & USB_INT)) {
    // turn retries back on for important stuff
    setRetry(true);

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

    // turn retries off for polling
    setRetry(false);
  }

  char mouseData[8];

  if (gHid.bootMouseEndpoint != 0) {
    if(issueTokenRead(gHid.bootMouseEndpoint & 0x0F, DEF_USB_PID_IN, gOddPollParity)) {
      if (usbReadBuffer(mouseData) > 8) {
        Serial.write("Overflow!\n");
      }

      Serial.print(gFailCount);
      Serial.write('\n');

      gFailCount = 0;

      // debugPrintBuffer(mouseData, 3);
    } else {
      gFailCount = gFailCount + 1;
    }

    gOddPollParity = !gOddPollParity;
  }
}
