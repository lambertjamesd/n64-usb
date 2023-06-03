
#include "descriptor_parser.h"
#include "usb_transfer.h"
#include "usb_hid.h"

struct HidInfo gHid;

void setup() {
  pinMode(2, INPUT); // N64

  pinMode(13, OUTPUT);

  uint8_t version = usbUnit();

  Serial.begin(9600);

  Serial.write("GET_IC_VER: 0x");
  printHex(version);
  Serial.write("\n");

  // needs to work wouth TIMSK0
  TIMSK0 = 0;
}


void loop() {
  checkUsbInterupts(&gHid);

  char mouseData[8];
  usbPollMouse(&gHid, mouseData);
}
