
#include "descriptor_parser.h"

#include <string.h>
#include <stddef.h>
#include "debug_print.h"

#include <Arduino.h>

enum ConfigParserState {
    ConfigParserStateFindingInterfaceClass,
    ConfigParserStateFindingInterfaceSubClass,
    ConfigParserStateFindingInterfaceProtocol,

    ConfigParserStateFindingEndpoint,

    ConfigParserStateDone,
};

struct ConfigParser {
    uint8_t state;
    uint8_t descSize;
    uint8_t descType;
    uint8_t descOffset;

    struct HidInfo* info;
};

void configParserInit(struct ConfigParser* parser, struct HidInfo* info) {
    parser->state = ConfigParserStateFindingInterfaceClass;
    parser->descSize = 0;
    parser->descType = 0;
    // size of header
    parser->descOffset = 2;
    parser->info = info;

    info->bootMouseEndpoint = 0;
}

void configParserStep(struct ConfigParser* parser, uint8_t next) {
    if (parser->descSize == 0) {
        parser->descSize = next;
        return;
    }

    if (parser->descType == 0) {
        parser->descType = next;
        return;
    }

    if (parser->state != ConfigParserStateDone) {
        if (parser->descType == DESC_TYPE_CONFIGURATION &&
            parser->descOffset == offsetof(struct ConfigurationDescriptor, bConfigurationValue)) {
            parser->info->bootMouseConfiguration = next;
        }

        if (parser->descType == DESC_TYPE_INTERFACE && 
            parser->descOffset == offsetof(struct InterfaceDescriptor, bInterfaceNumber)) {
            parser->info->bootMouseInterface = next;
        }
    }

    switch (parser->state) {
        case ConfigParserStateFindingInterfaceClass:
            if (parser->descType == DESC_TYPE_INTERFACE && 
                parser->descOffset == offsetof(struct InterfaceDescriptor, bInterfaceClass) &&
                next == DEVICE_CLASS_HID) {
                parser->state = ConfigParserStateFindingInterfaceSubClass;
            }
            break;
        case ConfigParserStateFindingInterfaceSubClass:
            if (parser->descType == DESC_TYPE_INTERFACE &&
                parser->descOffset == offsetof(struct InterfaceDescriptor, bInterfaceSubClass)) {
                if (next == HID_SUBCLASS_BOOT) {
                    parser->state = ConfigParserStateFindingInterfaceProtocol;
                } else {
                    parser->state = ConfigParserStateFindingInterfaceClass;
                }
            }
            break;
        case ConfigParserStateFindingInterfaceProtocol:
            if (parser->descType == DESC_TYPE_INTERFACE &&
                parser->descOffset == offsetof(struct InterfaceDescriptor, bInterfaceProtocol)) {
                if (next == HID_PROTOCOL_MOUSE) {
                    parser->state = ConfigParserStateFindingEndpoint;
                } else {
                    parser->state = ConfigParserStateFindingInterfaceClass;
                }
            }
            break;
        case ConfigParserStateFindingEndpoint:
            if (parser->descType == DESC_TYPE_ENDPOINT &&
                parser->descOffset == offsetof(struct EndpointDescriptor, bEndpointAddress)) {
                parser->info->bootMouseEndpoint = next;
                parser->state = ConfigParserStateDone;
            }
    }

    parser->descOffset++;

    if (parser->descOffset == parser->descSize) {
        parser->descSize = 0;
        parser->descType = 0;
        parser->descOffset = 2;
    }
}

void configParserPacketHandler(void* data, char* packetData, uint8_t packetSize, uint8_t offset) {
    struct ConfigParser* parser = (struct ConfigParser*)data;
    for (size_t i = 0; i < packetSize; ++i) {
        configParserStep(parser, (uint8_t)packetData[i]);
    }
}

void packetDirectCopy(void* data, char* packetData, uint8_t packetSize, uint8_t offset) {
    memcpy((char*)data + offset, packetData, packetSize);
}

bool getHIDInfo(struct HidInfo* result) {
    char readBuffer[18]; 
    
    if (!readControlTransfer(
        0, 
        REQUEST_TYPE_STANDARD | REQUEST_RECIPIENT_DEVICE,
        GET_DESCRIPTOR,
        PACK_WORD_BYTES(DESC_TYPE_DEVICE, 0x00),
        0x00,
        sizeof(struct DeviceDescriptor),
        readBuffer,
        packetDirectCopy
    )) {
        return false;
    }

    uint8_t bDeviceClass = ((struct DeviceDescriptor*)readBuffer)->bDeviceClass;

    if (bDeviceClass != DEVICE_CLASS_DEVICE && bDeviceClass != DEVICE_CLASS_HID) {
        // unsupported device
        return false;
    }

    uint8_t bNumConfigurations = 1;//((struct DeviceDescriptor*)readBuffer)->bNumConfigurations;

    for (uint8_t configurationIndex = 0; configurationIndex < bNumConfigurations; ++configurationIndex) {
        if (!readControlTransfer(
            0, 
            REQUEST_TYPE_STANDARD | REQUEST_RECIPIENT_DEVICE,
            GET_DESCRIPTOR,
            PACK_WORD_BYTES(DESC_TYPE_CONFIGURATION, configurationIndex),
            0x00,
            sizeof(struct ConfigurationDescriptor),
            readBuffer,
            packetDirectCopy
        )) {
            return false;
        }

        uint16_t wTotalLength = ((struct ConfigurationDescriptor*)readBuffer)->wTotalLength;

        struct ConfigParser parser;
        configParserInit(&parser, result);

        if (!readControlTransfer(
            0, 
            REQUEST_TYPE_STANDARD | REQUEST_RECIPIENT_DEVICE,
            GET_DESCRIPTOR,
            PACK_WORD_BYTES(DESC_TYPE_CONFIGURATION, configurationIndex),
            0x00,
            wTotalLength,
            &parser,
            configParserPacketHandler
        )) {
            return false;
        }

        if (result->bootMouseEndpoint != 0) {
            return true;
        }
    }

    return false;
} 