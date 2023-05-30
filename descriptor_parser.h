#ifndef __DESCRIPTOR_PARSER___
#define __DESCRIPTOR_PARSER___

#include <stdint.h>
#include <stdbool.h>

#include "./usb_transfer.h"

#define DEVICE_CLASS_DEVICE         0x00
#define DEVICE_CLASS_HID            0x03

#define HID_SUBCLASS_BOOT           0x01

#define HID_PROTOCOL_KEYBOARD       0x01
#define HID_PROTOCOL_MOUSE          0x02

#define DESC_TYPE_DEVICE            0x01
#define DESC_TYPE_CONFIGURATION     0x02
#define DESC_TYPE_INTERFACE         0x03
#define DESC_TYPE_ENDPOINT          0x05

#define DESC_TYPE_HID               0x21
#define DESC_TYPE_REPORT            0x22
#define DESC_TYPE_PHYSICAL          0x23

struct DeviceDescriptor {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t bcdUSB;
    uint8_t bDeviceClass;
    uint8_t bDeviceSubClass;
    uint8_t bDeviceProtocol;
    uint8_t bMaxPacketSize0;
    uint16_t idVendor;
    uint16_t idProduct;
    uint16_t bcdDevice;
    uint8_t iManufacturer;
    uint8_t iProduct;
    uint8_t iSerialNumber;
    uint8_t bNumConfigurations;
};

struct ConfigurationDescriptor {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t wTotalLength;
    uint8_t bNumInterfacs;
    uint8_t bConfigurationValue;
    uint8_t iConfiguration;
    uint8_t bmAttributes;
    uint8_t bMaxPower;
};

struct InterfaceDescriptor {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bInterfaceNumber;
    uint8_t bAlternateSetting;
    uint8_t bNumEndpoints;
    uint8_t bInterfaceClass;
    uint8_t bInterfaceSubClass;
    uint8_t bInterfaceProtocol;
    uint8_t iInterface;
};

struct EndpointDescriptor {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bEndpointAddress;
    uint8_t bmAttributes;
    uint16_t wMaxPacketSize;
    uint8_t bInterval;
};

struct HidDescriptorElement {
    uint8_t bDescriptorType;
    uint16_t wDescriptorLength;
};

struct HidDescriptor {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t bcdHID;
    uint8_t bCountryCode;
    uint8_t bNumDescriptors;
    struct HidDescriptorElement descriptors[];
};

struct HidInfo {
    uint8_t bootMouseConfiguration;
    uint8_t bootMouseInterface;
    uint8_t bootMouseEndpoint;
};

bool getHIDInfo(struct HidInfo* result);

#endif