#ifndef __USB_HID_H__
#define __USB_HID_H__

#include "descriptor_parser.h"

uint8_t usbUnit();
void checkUsbInterupts(struct HidInfo* hidInfo);
bool usbPollMouse(struct HidInfo* hidInfo, char* mouseData);

#endif