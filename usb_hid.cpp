
#include <Arduino.h>

#include "usb_hid.h"
#include "usb_transfer.h"
#include "descriptor_parser.h"
#include "debug_print.h"

void delayNoTimer(int ms) {
  while (ms) {
    delayMicroseconds(1000);
    --ms;
  }
}

uint8_t usbUnit() {
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

  usbWriteByte(RESET_ALL, false);
  delayNoTimer(40);

  usbWriteByte(GET_IC_VER, false);
  uint8_t version = usbReadByte();

  setUSBMode(USBModeIdle);

  // turn retries off for polling
  setRetry(false);

  return version;
}

uint8_t gNextAddress = 1;

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

bool handleConnect(struct HidInfo* hidInfo) {
  setUSBMode(USBModeReset);
  delayNoTimer(40);
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

  if (!setupConnectedUSBDevice(hidInfo)) {
    hidInfo->bootMouseEndpoint = 0;
    return false;
  }

  return true;
}

void handleDisconnect(struct HidInfo* hidInfo) {
  hidInfo->bootMouseEndpoint = 0;
  setUSBMode(USBModeIdle);
}

void checkUsbInterupts(struct HidInfo* hidInfo) {
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
        handleConnect(hidInfo);
        break;
      case USB_INT_DISCONNECT:
        handleDisconnect(hidInfo);
        break;
    }

    // turn retries off for polling
    setRetry(false);
  }
}

bool gOddPollParity = false;

bool usbPollMouse(struct HidInfo* hidInfo, char* mouseData) {
  if (hidInfo->bootMouseEndpoint == 0) {
    return false;
  }

  if(!issueTokenRead(hidInfo->bootMouseEndpoint & 0x0F, DEF_USB_PID_IN, gOddPollParity)) {
    return false;
  }

  if (usbReadBuffer(mouseData) > 8) {
    Serial.write("Overflow!\n");
  }
  
  gOddPollParity = !gOddPollParity;

  return true;
}